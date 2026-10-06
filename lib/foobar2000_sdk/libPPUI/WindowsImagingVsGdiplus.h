#pragma once
#include "WindowsImaging.h"
#include "gdiplus_helpers.h"

namespace PP::WIC {
	static Gdiplus::PixelFormat gdiplusPixelFormat(const WICPixelFormatGUID& fmt) {
		if (fmt == GUID_WICPixelFormat32bppBGR) {
			return PixelFormat32bppRGB;
		}
		if (fmt == GUID_WICPixelFormat24bppBGR) {
			return PixelFormat24bppRGB;
		}
		if (fmt == GUID_WICPixelFormat32bppBGRA) {
			return PixelFormat32bppARGB;
		}

		// stick with formats that work properly with Gdiplus, downconvert or upconvert the rest
		// don't bother with palettes, they're too rare to care
		return PixelFormatUndefined;
	}

	static std::unique_ptr<Gdiplus::Bitmap> toGdiplus(WICContext& ctx, CComPtr<IWICBitmapSource> source) {
		WICPixelFormatGUID pixelFormat;
		EH << source->GetPixelFormat(&pixelFormat);
		auto pixelFormat_gdiplus = gdiplusPixelFormat(pixelFormat);
		if (pixelFormat_gdiplus == PixelFormatUndefined) {
			pixelFormat = ctx.HasAlpha(source) ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat32bppBGR;
			source = ctx.ChangePixelFormat(source, pixelFormat);
			pixelFormat_gdiplus = gdiplusPixelFormat(pixelFormat);
		}

		UINT width = 0;
		UINT height = 0;

		EH << source->GetSize(&width, &height);
		using namespace Gdiplus;
		GdiplusErrorHandler EH2;
		auto ret = std::make_unique<Gdiplus::Bitmap>(width, height, pixelFormat_gdiplus);
		EH2 << ret->GetLastStatus();


		Rect rc(0, 0, width, height);
		Gdiplus::BitmapData bitmapData = {};
		EH2 << ret->LockBits(&rc, ImageLockModeWrite, pixelFormat_gdiplus, &bitmapData);
		if (bitmapData.Stride <= 0 || bitmapData.Scan0 == nullptr) {
			// Go no further
			// Above is NOT supposed to create bottom-up bitmaps with negative stride, only top-down
			ret->UnlockBits(&bitmapData);
			throw std::runtime_error("Gdiplus bitmap invalid state");
		}
		{
			WICRect wrc = { 0, 0, (INT)width, (INT)height };
			UINT uStride = (UINT)bitmapData.Stride;
			EH << source->CopyPixels(&wrc, uStride, uStride * bitmapData.Height, (BYTE*)bitmapData.Scan0);
		}
		EH2 << ret->UnlockBits(&bitmapData);
		return ret;
	}


}