// test_url_encode.cpp - URL encoding
// Inline reimplementation of ArtworkApi.cpp::UrlEncode (static function)
#include "pch.h"
#include <sstream>
#include <iomanip>

// ============================================
// Reimplementation for testing
// ============================================
namespace reimpl {

static std::string UrlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    escaped << std::uppercase;

    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << static_cast<char>(c);
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }

    return escaped.str();
}

} // namespace reimpl

// ============================================
// Tests
// ============================================

TEST(UrlEncode, EmptyString) {
    EXPECT_EQ(reimpl::UrlEncode(""), "");
}

TEST(UrlEncode, AlphanumericPassthrough) {
    EXPECT_EQ(reimpl::UrlEncode("abc123XYZ"), "abc123XYZ");
}

TEST(UrlEncode, SafeCharsPreserved) {
    EXPECT_EQ(reimpl::UrlEncode("-_.~"), "-_.~");
}

TEST(UrlEncode, SpaceEncoded) {
    EXPECT_EQ(reimpl::UrlEncode("hello world"), "hello%20world");
}

TEST(UrlEncode, PathSeparatorsEncoded) {
    EXPECT_EQ(reimpl::UrlEncode("E:\\Music\\song.flac"), "E%3A%5CMusic%5Csong.flac");
}

TEST(UrlEncode, ForwardSlashEncoded) {
    EXPECT_EQ(reimpl::UrlEncode("/path/to/file"), "%2Fpath%2Fto%2Ffile");
}

TEST(UrlEncode, SpecialCharsEncoded) {
    std::string input = "#?&=%";
    std::string encoded = reimpl::UrlEncode(input);
    EXPECT_NE(encoded.find("%23"), std::string::npos);  // #
    EXPECT_NE(encoded.find("%3F"), std::string::npos);  // ?
    EXPECT_NE(encoded.find("%26"), std::string::npos);  // &
    EXPECT_NE(encoded.find("%3D"), std::string::npos);  // =
    EXPECT_NE(encoded.find("%25"), std::string::npos);  // %
}
