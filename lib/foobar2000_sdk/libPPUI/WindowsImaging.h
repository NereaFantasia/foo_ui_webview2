#pragma once
#include <optional>
#include <wincodec.h>
#include "CPropVariant.h"

#define PP_WIC_DEBUG_PRETEND_CODECS_MISSING 0

CSize AdjustSizeToFit(CSize sizeFit, CSize sizeFitIn); // gdiplus helpers method

namespace PP::WIC {
	class EH_ {
	public:
		void operator<<(HRESULT hr) {
			if (FAILED(hr)) {
				throw exception_com(hr);
			}
		}
	};
	static EH_ EH;

	static CComPtr<IWICImagingFactory> getWICFactory() {
		// Create WIC factory
		CComPtr<IWICImagingFactory> ret;
		EH << CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&ret.p)
		);
		return ret;
	}

	static CComPtr<IWICPalette> makeWICPalette() {
		CComPtr<IWICPalette> ret;
		EH << getWICFactory()->CreatePalette(&ret.p);
		return ret;
	}

	static CComPtr<IWICImagingFactory> WICFactory() {
		return getWICFactory(); // no ptr caching
	}

	// Returns palette if available, null otherwise
	static CComPtr<IWICPalette> getPalette(CComPtr<IWICBitmapSource> source) {
		auto palette = makeWICPalette();
		source->CopyPalette(palette.p); // disregard failure, leave null ptr
		return palette;
	}

	static bool bitmapPaletteHasAlpha(CComPtr<IWICBitmapSource> source) {
		BOOL rv = FALSE;
		auto palette = getPalette(source);
		if (palette) {
			EH << palette->HasAlpha(&rv);
		}
		return !!rv;
	}
	static SIZE bitmapSize(IWICBitmapSource * bmp) {
		UINT w = 0, h = 0;
		EH << bmp->GetSize(&w, &h);
		return { (LONG)w, (LONG)h };
	}
	class WICContext {
	public:
		WICContext(CComPtr< IWICImagingFactory> f = WICFactory()) : m_factory(std::move(f)) {}

		CComPtr<IWICBitmapDecoder> CreateDecoder(const void* data, size_t size) {
#if PP_WIC_DEBUG_PRETEND_CODECS_MISSING
			throw exception_com(E_NOTIMPL);
#endif
			CComPtr<IStream> stream;
			stream.p = SHCreateMemStream((const BYTE*)data, pfc::downcast_guarded<UINT>(size));
			if (stream.p == nullptr) throw std::bad_alloc();
			CComPtr<IWICBitmapDecoder> ret;
			EH << m_factory->CreateDecoderFromStream(stream.p, nullptr, WICDecodeMetadataCacheOnDemand, &ret.p);
			return ret;
		}

		CComPtr<IWICBitmapSource> Scale(CComPtr<IWICBitmapSource> const& arg, UINT x, UINT y, WICBitmapInterpolationMode q = WICBitmapInterpolationModeHighQualityCubic) {
			CComPtr<IWICBitmapScaler> scaler;
			EH << m_factory->CreateBitmapScaler(&scaler);
			EH << scaler->Initialize(arg, x, y, q);
			return scaler.p;
		}

		CComPtr<IWICBitmapSource> Scale(CComPtr<IWICBitmapSource> const& arg, const SIZE & s, WICBitmapInterpolationMode q = WICBitmapInterpolationModeHighQualityCubic) {
			return Scale(arg, s.cx, s.cy, q);
		}

		CComPtr<IWICBitmapSource> FlipRotate(CComPtr<IWICBitmapSource> const& source, WICBitmapTransformOptions mode) {
			// PROBLEM: if told to rotate IWICBitmapSource that reads JPEG or so without caching, O(n^2) behavior occurs
			// We can't just check if passed object is an IWICBitmap, because non-caching IWICBitmap created from slow source will do the same
			// Always copy source pixels first for good measure
			CComPtr<IWICBitmap> bitmap = CreateBitmapFromSource(source);
			CComPtr<IWICBitmapFlipRotator> rotator;
			EH << m_factory->CreateBitmapFlipRotator(&rotator);
			EH << rotator->Initialize(bitmap, mode);
			return rotator.p;
		}

		static bool OrientationFlipsXY(UINT o) {
			return o >= 5 && o <= 8;
		}

		static WICBitmapTransformOptions TransformForEXIFOrientation(UINT o) {
			switch (o) {
			case 2: return WICBitmapTransformFlipHorizontal;
			case 3: return WICBitmapTransformRotate180;
			case 4: return WICBitmapTransformFlipVertical;
			case 5: return (WICBitmapTransformOptions)((unsigned)WICBitmapTransformFlipHorizontal | (unsigned)WICBitmapTransformRotate270);
			case 6: return WICBitmapTransformRotate90;
			case 7: return (WICBitmapTransformOptions)((unsigned)WICBitmapTransformFlipHorizontal | (unsigned)WICBitmapTransformRotate90);
			case 8: return WICBitmapTransformRotate270;
			default: return WICBitmapTransformRotate0;
			}
		}
		static unsigned GetEXIFOrientation(IWICBitmapFrameDecode* frame) {
			unsigned ret = 1;
			try {
				CPropVariant value;

				CComPtr<IWICMetadataQueryReader> pQueryReader;
				EH << frame->GetMetadataQueryReader(&pQueryReader);

				// EXIF orientation tag
				auto hr = pQueryReader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &value);
				ret = (SUCCEEDED(hr) && value.vt == VT_UI2) ? value.uiVal : 1;
			} catch (...) {}
			return ret;
		}
		void FixOrientation(CComPtr<IWICBitmapSource>& ret, CComPtr<IWICBitmapFrameDecode> const& frame) {
			if (!ret) ret = frame.p;
			auto r = TransformForEXIFOrientation(GetEXIFOrientation(frame));
			if (r != WICBitmapTransformRotate0) {
				ret = FlipRotate(ret, r);
			}
		}
		void FixColorProfile(CComPtr<IWICBitmapSource>& ret, CComPtr<IWICBitmapFrameDecode> const& frame) {
			if (!ret) ret = frame.p;
			CComPtr<IWICColorContext> pColorContext, pDestColorContext;
			EH << m_factory->CreateColorContext(&pColorContext);
			UINT actualCount = 0;
			EH << frame->GetColorContexts(1, &pColorContext.p, &actualCount);
			if (actualCount != 1) return;
			m_factory->CreateColorContext(&pDestColorContext);
			EH << pDestColorContext->InitializeFromExifColorSpace(1);
			CComPtr<IWICColorTransform> pColorTransform;
			EH << m_factory->CreateColorTransformer(&pColorTransform);
			GUID pixelFormat = {};
			EH << frame->GetPixelFormat(&pixelFormat);
			EH << pColorTransform->Initialize(ret, pColorContext, pDestColorContext, pixelFormat);
			ret = pColorTransform.p;
		}

		CComPtr<IWICComponentInfo> ComponentInfo(const GUID& clsid) {
			CComPtr<IWICComponentInfo> componentInfo;
			EH << m_factory->CreateComponentInfo(clsid, &componentInfo);
			return componentInfo;
		}
		CComPtr<IWICPixelFormatInfo> PixelFormatInfo(const GUID& pixelFormat) {
			CComPtr<IWICPixelFormatInfo> ret; ret = ComponentInfo(pixelFormat);
			PFC_ASSERT(ret);
			return ret;
		}
		CComPtr<IWICBitmap> CreateBitmapFromSource(IWICBitmapSource* source, WICBitmapCreateCacheOption option = WICBitmapCacheOnLoad) {
			CComPtr<IWICBitmap> ret;
			EH << m_factory->CreateBitmapFromSource(source, option, &ret);
			return ret;
		}
		CComPtr<IWICBitmap> PerformSourceTransform(CComPtr<IWICBitmapSourceTransform> const& xform, UINT width, UINT height, WICBitmapTransformOptions transform = WICBitmapTransformRotate0) {
			GUID pixelFormat = GUID_WICPixelFormat32bppRGBA;
			EH << xform->GetClosestPixelFormat(&pixelFormat);
			auto pfi = PixelFormatInfo(pixelFormat);
			UINT bits = 0;
			EH << pfi->GetBitsPerPixel(&bits);
			UINT stride = width * ((bits + 7) / 8);
			UINT bufferSize = stride * height;
			std::vector<BYTE> buffer(bufferSize);
			EH << xform->CopyPixels(nullptr, width, height, &pixelFormat, transform, stride, bufferSize, buffer.data());
			CComPtr<IWICBitmap> ret;
			EH << m_factory->CreateBitmapFromMemory(width, height, pixelFormat, stride, bufferSize, buffer.data(), &ret);
			return ret;
		}

		struct bitmapSourceArg_t {
			SIZE wantSize = {};
			bool wantSizeLoose = false;
			bool noRotate = false;
			unsigned* retOrientation = nullptr;
			SIZE* retOriginalSize = nullptr;
		};

		CComPtr<IWICBitmapSource> CreateBitmapSource(CComPtr<IWICBitmapFrameDecode> const & frame, const bitmapSourceArg_t& arg) {
			CComPtr<IWICBitmapSource> ret = frame.p;

			const auto orientation = GetEXIFOrientation(frame);
			if (arg.retOrientation) *arg.retOrientation = orientation;

			const auto origSize = bitmapSize(frame);
			if (origSize.cx < 1 || origSize.cy < 1) throw std::runtime_error("nonsensical bitmap size");
			if (arg.retOriginalSize) *arg.retOriginalSize = origSize;

			auto wantSize = arg.wantSize;
			if (OrientationFlipsXY(orientation)) std::swap(wantSize.cx, wantSize.cy);
			if (wantSize.cx > 0 && wantSize.cy > 0) {
				CComPtr<IWICBitmapSourceTransform> xform; xform = frame;
				if (xform) {
					// NOTE
					// API supports rotation while decoding, but per documentation it only works for JPEG-XR
					// For lack of real world usage cases, we don't bother with it here
					double ratio = 1;
					double ratioX = (double)wantSize.cx / origSize.cx, ratioY = (double)wantSize.cy / origSize.cy;
					ratio = ratioX > ratioY ? ratioY : ratioX;
					if (ratio < 1) {
						UINT w = (UINT)pfc::rint32(origSize.cx * ratio), h = (UINT)pfc::rint32(origSize.cy * ratio);
						if (SUCCEEDED(xform->GetClosestSize(&w, &h)) && w < (UINT)origSize.cx && h < (UINT)origSize.cy) {
							try {
								ret = PerformSourceTransform(xform, w, h);
							} catch (...) {}
						}
					}
				} else if (arg.wantSizeLoose) {
					double ratio = 1;
					double ratioX = (double)wantSize.cx / origSize.cx, ratioY = (double)wantSize.cy / origSize.cy;
					ratio = ratioX > ratioY ? ratioY : ratioX;
					if (ratio <= 0.5) {
						ratio = 1.0 / floor(1.0 / ratio);
						ret = Scale(ret, {pfc::rint32(origSize.cx * ratio), pfc::rint32(origSize.cy * ratio)});
					}
				}

				if (!arg.wantSizeLoose) {
					ret = Scale(ret, AdjustSizeToFit(origSize, wantSize));
				}
			}

			if ( ! arg.noRotate ) {
				auto transform = TransformForEXIFOrientation(orientation);
				constexpr auto noTransform = WICBitmapTransformRotate0;
				if (transform != noTransform) {
					ret = FlipRotate(ret, transform);
				}
			}

			try { FixColorProfile(ret, frame); } catch (...) {}

			return ret;
		}
		CComPtr<IWICBitmapSource> CreateBitmapSource(CComPtr<IWICBitmapDecoder> const& decoder, bitmapSourceArg_t const& arg) {
			CComPtr<IWICBitmapFrameDecode> frame;
			EH << decoder->GetFrame(0, &frame.p);
			return CreateBitmapSource(frame, arg);
		}

		bool HasAlpha(CComPtr<IWICBitmapSource> const& source, CComPtr<IWICPixelFormatInfo> const & pfInfo_ = nullptr) {
			if (bitmapPaletteHasAlpha(source)) return true;

			CComPtr<IWICPixelFormatInfo> pfInfo = pfInfo_;
			if (!pfInfo) {
				WICPixelFormatGUID pf = {};
				EH << source->GetPixelFormat(&pf);
				pfInfo = this->PixelFormatInfo(pf);
			}
			{
				CComPtr<IWICPixelFormatInfo2> info2;
				info2 = pfInfo;
				if (info2) {
					BOOL v = FALSE;
					if (SUCCEEDED(info2->SupportsTransparency(&v))) return !!v;
				}
			}

			return false;
		}

		CComPtr<IWICBitmapSource> ChangePixelFormat(CComPtr<IWICBitmapSource> source, WICPixelFormatGUID const& toFormat) {
			CComPtr<IWICFormatConverter> converter;
			EH << m_factory->CreateFormatConverter(&converter.p);
			EH << converter->Initialize(source.p, toFormat, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
			CComPtr<IWICBitmapSource> ret;
			EH << converter->QueryInterface(IID_PPV_ARGS(&ret.p));
			return ret;
		}


		const CComPtr<IWICImagingFactory> m_factory = WICFactory();
	};
}

