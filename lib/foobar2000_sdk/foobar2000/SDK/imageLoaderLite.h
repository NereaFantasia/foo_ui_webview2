#pragma once

#include "commonObjects.h"

// 2025 addendum
// fb2k::imageLoaderLite was originally introduced to interop with bundled image libraries (namely libwebp) to handle non-natively-supported formats.
// It is deprecated and should not be used in new code.
// Use Windows Imaging Component & Direct2D instead, or NSImage on macOS.
// Only still useful part is getInfo(), which provides a lightweight way to query image description.

#ifdef _WIN32
namespace Gdiplus {
	class Image;
}
#endif
namespace fb2k {
#ifdef _WIN32
	typedef Gdiplus::Image * nativeImage_t;
#else
	typedef void * nativeImage_t;
#endif

	struct imageInfo_t {
		uint32_t width, height, bitDepth;
		bool haveAlpha;
		const char * formatName; // MAY BE NULL IF UNKNOWN
		const char * mime; // MAY BE NULL IF UNKNOWN
	};

	//! \since 1.6
	//! Interface to common image loader routines that turn a bytestream into a image that can be drawn in a window. \n
	//! Windows: Using imageLoaderLite methods initializes gdiplus if necessary, leaving gdiplus initialized for the rest of app lifetime. \n
	//! If your component supports running on foobar2000 older than 1.6, use tryGet() to gracefully fall back to your own image loader: \n
	//! auto api = fb2k::imageLoaderLite::tryGet(); if (api.is_valid()) { do stuff with api; } else { use fallbacks; }
	class imageLoaderLite : public service_base {
		FB2K_MAKE_SERVICE_COREAPI(imageLoaderLite);
	public:
		//! Throws excpetions on failure, returns valid image otherwise.\n
		//! Caller takes ownership of the returned object. \n
		//! @param outInfo Optional struct to receive information about the loaded image.
		virtual nativeImage_t load(const void * data, size_t bytes, imageInfo_t * outInfo = nullptr, abort_callback & aborter = fb2k::noAbort) = 0;

		//! Parses the image data just enough to hand over basic info about what's inside. \n
		//! Much faster than load(). \n
		//! Throws exceptions on failure. \n
		//! Supports all formats recognized by load().
		virtual imageInfo_t getInfo(const void * data, size_t bytes, abort_callback & aborter = fb2k::noAbort) = 0;

		//! Helper - made virtual so it can be possibly specialized in the future
		virtual nativeImage_t load(album_art_data_ptr data, imageInfo_t * outInfo = nullptr, abort_callback & aborter = fb2k::noAbort) {
			return load(data->get_ptr(), data->get_size(), outInfo, aborter);
		}
		//! Helper - made virtual so it can be possibly specialized in the future
		virtual imageInfo_t getInfo(album_art_data_ptr data, abort_callback & aborter = fb2k::noAbort) {
			return getInfo(data->get_ptr(), data->get_size(), aborter);
		}
	};
}

#define FB2K_GETOPENFILENAME_PICTUREFILES "Picture files|*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.webp"
#define FB2K_GETOPENFILENAME_PICTUREFILES_ALL FB2K_GETOPENFILENAME_PICTUREFILES "|All files|*.*"