#include "pch.h"
#include "utils/ImageUtils.h"

// WIC for image resizing
#include <wincodec.h>
#include <wrl/client.h>
#include <limits>
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

std::vector<uint8_t> ResizeImageWIC(
    const uint8_t* srcData,
    size_t srcLen,
    int maxSize,
    int& outWidth,
    int& outHeight,
    const char*& outMimeType)
{
    std::vector<uint8_t> result;
    outWidth = 0;
    outHeight = 0;
    outMimeType = "image/jpeg";

    if (!srcData || srcLen == 0 || maxSize <= 0) {
        return result;
    }

    // 确保当前线程 COM 已初始化（WebView2 custom protocol 线程可能未初始化）
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // S_OK = 我们初始化了, S_FALSE = 已初始化, RPC_E_CHANGED_MODE = STA 已初始化（WIC 仍可用）
    struct ComGuard {
        bool should_uninit;
        ~ComGuard() { if (should_uninit) CoUninitialize(); }
    } comGuard{hrCom == S_OK};

    HRESULT hr;

    // Use a fresh factory per request so COM/WIC lifetime stays inside this call.
    ComPtr<IWICImagingFactory> factory;
    hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)
    );
    if (FAILED(hr) || !factory) {
        LOG("WIC: Factory not available");
        return result;
    }

    // Create stream from memory
    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) return result;

    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(srcData),
        static_cast<DWORD>(srcLen)
    );
    if (FAILED(hr)) return result;

    // Create decoder
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(
        stream.Get(),
        nullptr,
        WICDecodeMetadataCacheOnDemand,
        &decoder
    );
    if (FAILED(hr)) {
        LOG("WIC: Failed to create decoder, hr=", pfc::format_hex((uint32_t)hr));
        return result;
    }

    // Get first frame
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return result;

    // Get original dimensions
    UINT origWidth, origHeight;
    hr = frame->GetSize(&origWidth, &origHeight);
    if (FAILED(hr)) return result;

    // Calculate new dimensions (maintain aspect ratio)
    UINT newWidth, newHeight;
    if (origWidth <= (UINT)maxSize && origHeight <= (UINT)maxSize) {
        // No resize needed
        outWidth = (int)origWidth;
        outHeight = (int)origHeight;
        return result;  // empty = no resize needed
    }

    if (origWidth > origHeight) {
        newWidth = (UINT)maxSize;
        newHeight = (UINT)((float)origHeight / (float)origWidth * (float)maxSize);
    } else {
        newHeight = (UINT)maxSize;
        newWidth = (UINT)((float)origWidth / (float)origHeight * (float)maxSize);
    }

    if (newWidth < 1) newWidth = 1;
    if (newHeight < 1) newHeight = 1;

    outWidth = (int)newWidth;
    outHeight = (int)newHeight;

    // Create scaler
    ComPtr<IWICBitmapScaler> scaler;
    hr = factory->CreateBitmapScaler(&scaler);
    if (FAILED(hr)) return result;

    // 缩略图使用 Fant 插值（高质量且快），大图保持 Cubic 最高质量
    WICBitmapInterpolationMode interpolation =
        (maxSize <= 300)
            ? WICBitmapInterpolationModeFant
            : WICBitmapInterpolationModeHighQualityCubic;

    hr = scaler->Initialize(frame.Get(), newWidth, newHeight, interpolation);
    if (FAILED(hr)) return result;

    // Convert to BGR for JPEG encoding
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return result;

    hr = converter->Initialize(
        scaler.Get(),
        GUID_WICPixelFormat24bppBGR,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) return result;

    // Create memory stream for output
    ComPtr<IStream> memStream;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, &memStream);
    if (FAILED(hr)) return result;

    // Create JPEG encoder
    ComPtr<IWICBitmapEncoder> encoder;
    hr = factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder);
    if (FAILED(hr)) return result;

    hr = encoder->Initialize(memStream.Get(), WICBitmapEncoderNoCache);
    if (FAILED(hr)) return result;

    // Create frame for output
    ComPtr<IWICBitmapFrameEncode> outFrame;
    ComPtr<IPropertyBag2> props;
    hr = encoder->CreateNewFrame(&outFrame, &props);
    if (FAILED(hr)) return result;

    // Set JPEG quality (85%)
    PROPBAG2 option = {};
    option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
    VARIANT varValue;
    VariantInit(&varValue);
    varValue.vt = VT_R4;
    varValue.fltVal = 0.85f;
    props->Write(1, &option, &varValue);

    hr = outFrame->Initialize(props.Get());
    if (FAILED(hr)) return result;

    hr = outFrame->SetSize(newWidth, newHeight);
    if (FAILED(hr)) return result;

    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat24bppBGR;
    hr = outFrame->SetPixelFormat(&pixelFormat);
    if (FAILED(hr)) return result;

    if (!IsEqualGUID(pixelFormat, GUID_WICPixelFormat24bppBGR)) {
        LOG("WIC: Unexpected encoder pixel format after SetPixelFormat");
        return result;
    }

    // Materialize resized pixels before encoding so the JPEG encoder does not
    // walk the source frame / metadata tree through WriteSource().
    const size_t stride = static_cast<size_t>(newWidth) * 3;
    const size_t bufferSize = stride * static_cast<size_t>(newHeight);
    if (stride > std::numeric_limits<UINT>::max() ||
        bufferSize > std::numeric_limits<UINT>::max()) {
        LOG("WIC: Output buffer exceeds CopyPixels/WritePixels limits");
        return result;
    }

    std::vector<uint8_t> pixelBuffer(bufferSize);
    hr = converter->CopyPixels(
        nullptr,
        static_cast<UINT>(stride),
        static_cast<UINT>(bufferSize),
        pixelBuffer.data()
    );
    if (FAILED(hr)) return result;

    hr = outFrame->WritePixels(
        newHeight,
        static_cast<UINT>(stride),
        static_cast<UINT>(bufferSize),
        pixelBuffer.data()
    );
    if (FAILED(hr)) return result;

    hr = outFrame->Commit();
    if (FAILED(hr)) return result;

    hr = encoder->Commit();
    if (FAILED(hr)) return result;

    // Get output data
    STATSTG stat;
    hr = memStream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr)) return result;

    size_t outLen = (size_t)stat.cbSize.QuadPart;
    result.resize(outLen);

    LARGE_INTEGER zero = {};
    memStream->Seek(zero, STREAM_SEEK_SET, nullptr);

    ULONG bytesRead;
    hr = memStream->Read(result.data(), (ULONG)outLen, &bytesRead);
    if (FAILED(hr) || bytesRead != outLen) {
        result.clear();
        return result;
    }

    LOG("WIC: Resized ", origWidth, "x", origHeight, " -> ", newWidth, "x", newHeight, ", output: ", outLen, " bytes");
    return result;
}
