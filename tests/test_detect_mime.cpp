// test_detect_mime.cpp - MIME type magic byte detection
// Inline reimplementation of ArtworkApi.cpp::DetectMimeType (static function)
#include "pch.h"

// ============================================
// Reimplementation for testing (original is static in ArtworkApi.cpp)
// ============================================
namespace reimpl {

static const char* DetectMimeType(const uint8_t* data, size_t len) {
    if (len < 2) return "application/octet-stream";

    const unsigned char* bytes = data;

    // BMP: 42 4D
    if (bytes[0] == 0x42 && bytes[1] == 0x4D) {
        return "image/bmp";
    }

    if (len < 4) return "application/octet-stream";

    // JPEG: FF D8 FF
    if (bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
        return "image/jpeg";
    }

    // PNG: 89 50 4E 47
    if (bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47) {
        return "image/png";
    }

    // GIF: 47 49 46 38
    if (bytes[0] == 0x47 && bytes[1] == 0x49 && bytes[2] == 0x46 && bytes[3] == 0x38) {
        return "image/gif";
    }

    // WebP: 52 49 46 46 ... 57 45 42 50
    if (len >= 12 && bytes[0] == 0x52 && bytes[1] == 0x49 && bytes[2] == 0x46 && bytes[3] == 0x46 &&
        bytes[8] == 0x57 && bytes[9] == 0x45 && bytes[10] == 0x42 && bytes[11] == 0x50) {
        return "image/webp";
    }

    return "application/octet-stream";
}

} // namespace reimpl

// ============================================
// Tests
// ============================================

TEST(DetectMimeType, JPEG) {
    uint8_t data[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00};
    EXPECT_STREQ(reimpl::DetectMimeType(data, sizeof(data)), "image/jpeg");
}

TEST(DetectMimeType, PNG) {
    uint8_t data[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A};
    EXPECT_STREQ(reimpl::DetectMimeType(data, sizeof(data)), "image/png");
}

TEST(DetectMimeType, GIF) {
    uint8_t data[] = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};
    EXPECT_STREQ(reimpl::DetectMimeType(data, sizeof(data)), "image/gif");
}

TEST(DetectMimeType, BMP) {
    uint8_t data[] = {0x42, 0x4D, 0x00, 0x00};
    EXPECT_STREQ(reimpl::DetectMimeType(data, sizeof(data)), "image/bmp");
}

TEST(DetectMimeType, WebP) {
    uint8_t data[] = {0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50};
    EXPECT_STREQ(reimpl::DetectMimeType(data, sizeof(data)), "image/webp");
}

TEST(DetectMimeType, TooShort_OneByte) {
    uint8_t data[] = {0xFF};
    EXPECT_STREQ(reimpl::DetectMimeType(data, 1), "application/octet-stream");
}

TEST(DetectMimeType, TooShort_ThreeBytes) {
    uint8_t data[] = {0x89, 0x50, 0x4E};  // PNG header truncated
    EXPECT_STREQ(reimpl::DetectMimeType(data, 3), "application/octet-stream");
}

TEST(DetectMimeType, Empty) {
    EXPECT_STREQ(reimpl::DetectMimeType(nullptr, 0), "application/octet-stream");
}

TEST(DetectMimeType, UnknownBytes) {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_STREQ(reimpl::DetectMimeType(data, sizeof(data)), "application/octet-stream");
}
