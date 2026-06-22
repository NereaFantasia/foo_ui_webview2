// test_base64.cpp — Base64 编码测试
#include "pch.h"
#include "../src/utils/Base64.h"

TEST(Base64, Empty) {
    std::string result = utils::Base64Encode(nullptr, 0);
    EXPECT_TRUE(result.empty());
}

TEST(Base64, SingleByte) {
    uint8_t data[] = { 'A' };
    std::string result = utils::Base64Encode(data, 1);
    EXPECT_EQ(result, "QQ==");
}

TEST(Base64, TwoBytes) {
    uint8_t data[] = { 'A', 'B' };
    std::string result = utils::Base64Encode(data, 2);
    EXPECT_EQ(result, "QUI=");
}

TEST(Base64, ThreeBytes) {
    uint8_t data[] = { 'A', 'B', 'C' };
    std::string result = utils::Base64Encode(data, 3);
    EXPECT_EQ(result, "QUJD");
}

TEST(Base64, HelloWorld) {
    const char* text = "Hello, World!";
    std::string result = utils::Base64Encode(
        reinterpret_cast<const uint8_t*>(text), strlen(text));
    EXPECT_EQ(result, "SGVsbG8sIFdvcmxkIQ==");
}

TEST(Base64, BinaryData) {
    uint8_t data[] = { 0x00, 0xFF, 0x80, 0x7F, 0x01, 0xFE };
    std::string result = utils::Base64Encode(data, 6);
    // Manually verified: 00 FF 80 7F 01 FE -> AP+Af wH+
    EXPECT_EQ(result, "AP+AfwH+");
}
