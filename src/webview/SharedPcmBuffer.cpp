#include "pch.h"
#include "webview/SharedPcmBuffer.h"

bool SharedPcmBuffer::IsSupported(ICoreWebView2Environment* environment, ICoreWebView2* webview) {
    if (!environment || !webview) return false;
    wil::com_ptr<ICoreWebView2Environment12> environment12;
    wil::com_ptr<ICoreWebView2_17> webview17;
    return SUCCEEDED(environment->QueryInterface(IID_PPV_ARGS(&environment12))) && environment12 &&
           SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&webview17))) && webview17;
}

std::unique_ptr<SharedPcmBuffer> SharedPcmBuffer::Create(ICoreWebView2Environment* environment, std::uint64_t bytes,
                                                         HRESULT* hr) {
    auto fail = [hr](HRESULT code) {
        if (hr) *hr = code;
        return std::unique_ptr<SharedPcmBuffer>();
    };
    if (!environment) return fail(E_POINTER);

    wil::com_ptr<ICoreWebView2Environment12> environment12;
    HRESULT result = environment->QueryInterface(IID_PPV_ARGS(&environment12));
    if (FAILED(result) || !environment12) return fail(FAILED(result) ? result : E_NOINTERFACE);

    wil::com_ptr<ICoreWebView2SharedBuffer> buffer;
    result = environment12->CreateSharedBuffer(bytes, &buffer);
    if (FAILED(result) || !buffer) return fail(FAILED(result) ? result : E_FAIL);

    BYTE* data = nullptr;
    UINT64 size = 0;
    result = buffer->get_Buffer(&data);
    if (SUCCEEDED(result)) result = buffer->get_Size(&size);
    if (FAILED(result) || !data || size < bytes) {
        buffer->Close();
        return fail(FAILED(result) ? result : E_FAIL);
    }

    if (hr) *hr = S_OK;
    return std::unique_ptr<SharedPcmBuffer>(new SharedPcmBuffer(std::move(buffer), data, size));
}

SharedPcmBuffer::~SharedPcmBuffer() {
    Close();
}

HRESULT SharedPcmBuffer::PostReadOnly(ICoreWebView2* webview, const std::wstring& additionalDataJson) {
    if (!buffer_ || !data_) return RO_E_CLOSED;
    if (!webview) return E_POINTER;
    wil::com_ptr<ICoreWebView2_17> webview17;
    const HRESULT result = webview->QueryInterface(IID_PPV_ARGS(&webview17));
    if (FAILED(result) || !webview17) return FAILED(result) ? result : E_NOINTERFACE;
    return webview17->PostSharedBufferToScript(buffer_.get(), COREWEBVIEW2_SHARED_BUFFER_ACCESS_READ_ONLY,
                                               additionalDataJson.empty() ? nullptr : additionalDataJson.c_str());
}

void SharedPcmBuffer::Close() {
    if (buffer_ && data_) buffer_->Close();
    data_ = nullptr;
}

void SharedPcmBuffer::Detach() {
    // detach() 交出裸指针而不 Release；这里故意不接，引用随进程结束一起丢掉。
    (void)buffer_.detach();
    data_ = nullptr;
}
