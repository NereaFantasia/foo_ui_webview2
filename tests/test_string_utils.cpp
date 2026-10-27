// test_string_utils.cpp — JSON-safe UTF-8 normalization tests
#include "pch.h"
#include "../src/utils/StringUtils.h"

namespace string_utils_test {

constexpr std::string_view kReplacement = "\xEF\xBF\xBD";

void ExpectJsonSerializable(std::string_view input) {
    const std::string sanitized = StringUtils::SafeUtf8(input);
    EXPECT_NO_THROW({
        const nlohmann::json value = sanitized;
        const std::string serialized = value.dump();
        EXPECT_FALSE(serialized.empty());
    });
}

}  // namespace string_utils_test

TEST(StringUtils, IsValidUtf8AcceptsEmptyStringsAndNullPointer) {
    EXPECT_TRUE(StringUtils::IsValidUtf8(std::string_view{}));
    EXPECT_TRUE(StringUtils::IsValidUtf8(""));
    EXPECT_TRUE(StringUtils::IsValidUtf8(static_cast<const char*>(nullptr)));
}

TEST(StringUtils, IsValidUtf8AcceptsAsciiChineseAndEmoji) {
    EXPECT_TRUE(StringUtils::IsValidUtf8("Title 123"));
    EXPECT_TRUE(StringUtils::IsValidUtf8("播放 \xF0\x9F\x8E\xB5"));
    EXPECT_TRUE(StringUtils::IsValidUtf8(std::string_view("播放 \xF0\x9F\x8E\xB5")));
}

TEST(StringUtils, IsValidUtf8AcceptsUnicodeScalarBoundaries) {
    const std::string_view inputs[] = {
        "\x7F", "\xC2\x80", "\xDF\xBF", "\xE0\xA0\x80", "\xED\x9F\xBF",
        "\xEE\x80\x80", "\xEF\xBF\xBF", "\xF0\x90\x80\x80", "\xF4\x8F\xBF\xBF"
    };
    for (const auto input : inputs) {
        EXPECT_TRUE(StringUtils::IsValidUtf8(input));
        EXPECT_EQ(StringUtils::SafeUtf8(input), input);
    }
}

TEST(StringUtils, IsValidUtf8RejectsStrayContinuationAndInvalidLeadingBytes) {
    const std::string_view inputs[] = {
        "\x80", "\xB9", "\xBF", "Title \xB9", "\xFE", "\xFF", "\xF5\x80\x80\x80"
    };
    for (const auto input : inputs) EXPECT_FALSE(StringUtils::IsValidUtf8(input));
    EXPECT_FALSE(StringUtils::IsValidUtf8("\xB9"));
    EXPECT_EQ(StringUtils::SafeUtf8("\xB9"), string_utils_test::kReplacement);
}

TEST(StringUtils, IsValidUtf8RejectsTruncatedAndInterruptedSequences) {
    const std::string_view inputs[] = {
        "\xC2", "\xE6\x92", "\xF0\x9F\x8E", "\xC2Z", "\xE6\x92Z", "\xF0\x9F\x8EZ"
    };
    for (const auto input : inputs) EXPECT_FALSE(StringUtils::IsValidUtf8(input));
}

TEST(StringUtils, IsValidUtf8RejectsOverlongEncodings) {
    EXPECT_FALSE(StringUtils::IsValidUtf8("\xC0\x80"));
    EXPECT_FALSE(StringUtils::IsValidUtf8("\xC1\xBF"));
    EXPECT_FALSE(StringUtils::IsValidUtf8("\xE0\x80\xAF"));
    EXPECT_FALSE(StringUtils::IsValidUtf8("\xF0\x80\x80\xAF"));
}

TEST(StringUtils, IsValidUtf8RejectsSurrogatesAndOutOfRangeCodePoints) {
    EXPECT_FALSE(StringUtils::IsValidUtf8("\xED\xA0\x80"));
    EXPECT_FALSE(StringUtils::IsValidUtf8("\xED\xBF\xBF"));
    EXPECT_FALSE(StringUtils::IsValidUtf8("\xF4\x90\x80\x80"));
}

TEST(StringUtils, IsValidUtf8ChecksBytesAfterEmbeddedNullInStringView) {
    const std::string valid("A\0B", 3);
    const std::string invalid("A\0\xB9", 3);
    EXPECT_TRUE(StringUtils::IsValidUtf8(std::string_view(valid)));
    EXPECT_FALSE(StringUtils::IsValidUtf8(std::string_view(invalid)));
    EXPECT_TRUE(StringUtils::IsValidUtf8(invalid.c_str()));
}

TEST(StringUtils, SafeUtf8PreservesValidChineseAndEmoji) {
    const std::string input = "播放 \xF0\x9F\x8E\xB5";
    EXPECT_EQ(StringUtils::SafeUtf8(input), input);
    string_utils_test::ExpectJsonSerializable(input);
}

TEST(StringUtils, SafeUtf8ReplacesTruncatedSequences) {
    EXPECT_EQ(StringUtils::SafeUtf8(std::string_view("\xC2", 1)),
              std::string(string_utils_test::kReplacement));
    EXPECT_EQ(StringUtils::SafeUtf8(std::string_view("\xE6\x92", 2)),
              std::string(string_utils_test::kReplacement) +
                  std::string(string_utils_test::kReplacement));
    EXPECT_EQ(StringUtils::SafeUtf8(std::string_view("\xF0\x9F\x8E", 3)),
              std::string(string_utils_test::kReplacement) +
                  std::string(string_utils_test::kReplacement) +
                  std::string(string_utils_test::kReplacement));
}

TEST(StringUtils, SafeUtf8RejectsOverlongEncodings) {
    string_utils_test::ExpectJsonSerializable(std::string_view("\xC0\xAF", 2));
    string_utils_test::ExpectJsonSerializable(std::string_view("\xE0\x80\xAF", 3));
    string_utils_test::ExpectJsonSerializable(std::string_view("\xF0\x80\x80\xAF", 4));
    EXPECT_NE(StringUtils::SafeUtf8(std::string_view("\xC0\xAF", 2)),
              std::string("\xC0\xAF", 2));
}

TEST(StringUtils, SafeUtf8RejectsSurrogatesAndOutOfRangeCodePoints) {
    const std::string surrogate = StringUtils::SafeUtf8(std::string_view("\xED\xA0\x80", 3));
    const std::string outOfRange = StringUtils::SafeUtf8(std::string_view("\xF4\x90\x80\x80", 4));

    EXPECT_NE(surrogate, std::string("\xED\xA0\x80", 3));
    EXPECT_NE(outOfRange, std::string("\xF4\x90\x80\x80", 4));
    EXPECT_NO_THROW(nlohmann::json(surrogate).dump());
    EXPECT_NO_THROW(nlohmann::json(outOfRange).dump());
}

TEST(StringUtils, SafeUtf8HandlesEmbeddedNullWithStringView) {
    const std::string input("A\0B", 3);
    const std::string output = StringUtils::SafeUtf8(std::string_view(input.data(), input.size()));
    EXPECT_EQ(output, input);
    EXPECT_NO_THROW(nlohmann::json(output).dump());
}

TEST(StringUtils, SafeUtf8MakesOriginalFailureSerializable) {
    const std::string incompleteMenuLabel("Menu \xE6", 6);
    const std::string sanitized = StringUtils::SafeUtf8(incompleteMenuLabel);

    EXPECT_EQ(sanitized,
              std::string("Menu ") + std::string(string_utils_test::kReplacement));
    nlohmann::json menu;
    menu["label"] = sanitized;
    EXPECT_NO_THROW(menu.dump());
}
