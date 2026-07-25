// test_url_encode.cpp - Production artwork URL encoding
#include "pch.h"
#include "../src/api/ArtworkRequestParser.h"

// ============================================
// Tests
// ============================================

TEST(UrlEncode, EmptyString) {
    EXPECT_EQ(artwork_request::UrlEncode(""), "");
}

TEST(UrlEncode, AlphanumericPassthrough) {
    EXPECT_EQ(artwork_request::UrlEncode("abc123XYZ"), "abc123XYZ");
}

TEST(UrlEncode, SafeCharsPreserved) {
    EXPECT_EQ(artwork_request::UrlEncode("-_.~"), "-_.~");
}

TEST(UrlEncode, SpaceEncoded) {
    EXPECT_EQ(artwork_request::UrlEncode("hello world"), "hello%20world");
}

TEST(UrlEncode, PathSeparatorsEncoded) {
    EXPECT_EQ(artwork_request::UrlEncode("E:\\Music\\song.flac"), "E%3A%5CMusic%5Csong.flac");
}

TEST(UrlEncode, ForwardSlashEncoded) {
    EXPECT_EQ(artwork_request::UrlEncode("/path/to/file"), "%2Fpath%2Fto%2Ffile");
}

TEST(UrlEncode, SpecialCharsEncoded) {
    std::string input = "#?&=%";
    std::string encoded = artwork_request::UrlEncode(input);
    EXPECT_NE(encoded.find("%23"), std::string::npos);  // #
    EXPECT_NE(encoded.find("%3F"), std::string::npos);  // ?
    EXPECT_NE(encoded.find("%26"), std::string::npos);  // &
    EXPECT_NE(encoded.find("%3D"), std::string::npos);  // =
    EXPECT_NE(encoded.find("%25"), std::string::npos);  // %
}

TEST(UrlEncode, EncodesPlusAndUtf8Bytes) {
    EXPECT_EQ(artwork_request::UrlEncode("A+B"), "A%2BB");
    EXPECT_EQ(artwork_request::UrlEncode("\xE9\x9F\xB3\xE4\xB9\x90"), "%E9%9F%B3%E4%B9%90");
}
