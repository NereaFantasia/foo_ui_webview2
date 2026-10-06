// test_preferences_template_name.cpp - 模板起名校验的纯逻辑覆盖
//
// 直接链接生产头 prefs_template::*，不在测试内重写被测逻辑。
// 断言依据是 docs/preferences/DESIGN.md 第 6 节：名字为空、含非法字符、
// 与现有模板重名时在起名对话框内拒绝；重名按大小写不敏感判定。
#include "pch.h"
#include "../src/core/PreferencesTemplateName.h"

using namespace prefs_template;

namespace {

const std::vector<std::string> kExisting = {"default", "night", "Studio-Mix_2"};

}  // namespace

// ============================================
// Trim
// ============================================

TEST(PreferencesTemplateNameTrim, StripsLeadingAndTrailingAsciiWhitespace) {
    EXPECT_EQ(Trim("  night "), "night");
    EXPECT_EQ(Trim("\tnight\r\n"), "night");
    EXPECT_EQ(Trim("night"), "night");
}

TEST(PreferencesTemplateNameTrim, AllWhitespaceBecomesEmpty) {
    EXPECT_EQ(Trim(""), "");
    EXPECT_EQ(Trim("   "), "");
    EXPECT_EQ(Trim(" \t\n"), "");
}

TEST(PreferencesTemplateNameTrim, KeepsInteriorWhitespaceForTheCharsetCheck) {
    EXPECT_EQ(Trim(" my theme "), "my theme");
}

// ============================================
// 大小写不敏感比较
// ============================================

TEST(PreferencesTemplateNameCompare, EqualsIgnoreCaseFoldsAsciiOnly) {
    EXPECT_TRUE(EqualsIgnoreCase("Night", "night"));
    EXPECT_TRUE(EqualsIgnoreCase("STUDIO-MIX_2", "studio-mix_2"));
    EXPECT_FALSE(EqualsIgnoreCase("night", "nights"));
    EXPECT_FALSE(EqualsIgnoreCase("night", "day"));
    EXPECT_TRUE(EqualsIgnoreCase("", ""));
}

TEST(PreferencesTemplateNameCompare, ContainsIgnoreCaseMatchesAnyEntry) {
    EXPECT_TRUE(ContainsIgnoreCase(kExisting, "NIGHT"));
    EXPECT_TRUE(ContainsIgnoreCase(kExisting, "studio-mix_2"));
    EXPECT_FALSE(ContainsIgnoreCase(kExisting, "day"));
    EXPECT_FALSE(ContainsIgnoreCase({}, "default"));
}

// ============================================
// CheckName：空
// ============================================

TEST(PreferencesTemplateNameCheck, EmptyIsRejectedBeforeAnythingElse) {
    EXPECT_EQ(CheckName("", kExisting), NameError::Empty);
    // 空串即便“存在”于列表里也先报空，不报重名
    EXPECT_EQ(CheckName("", {""}), NameError::Empty);
}

// ============================================
// CheckName：非法字符
// ============================================

TEST(PreferencesTemplateNameCheck, InteriorSpaceIsInvalid) {
    EXPECT_EQ(CheckName("my theme", kExisting), NameError::InvalidChars);
}

TEST(PreferencesTemplateNameCheck, PathCharactersAreInvalid) {
    EXPECT_EQ(CheckName("a/b", kExisting), NameError::InvalidChars);
    EXPECT_EQ(CheckName("a\\b", kExisting), NameError::InvalidChars);
    EXPECT_EQ(CheckName("c:", kExisting), NameError::InvalidChars);
    EXPECT_EQ(CheckName("..", kExisting), NameError::InvalidChars);
    EXPECT_EQ(CheckName(".hidden", kExisting), NameError::InvalidChars);
    EXPECT_EQ(CheckName("theme.v2", kExisting), NameError::InvalidChars);
}

TEST(PreferencesTemplateNameCheck, PunctuationAndNonAsciiAreInvalid) {
    EXPECT_EQ(CheckName("theme!", kExisting), NameError::InvalidChars);
    EXPECT_EQ(CheckName("theme name", kExisting), NameError::InvalidChars);
    // UTF-8 编码的中文：字节值为负，转 unsigned char 后 isalnum 为假
    EXPECT_EQ(CheckName("\xE4\xB8\xBB\xE9\xA2\x98", kExisting), NameError::InvalidChars);
}

TEST(PreferencesTemplateNameCheck, InvalidCharsWinOverDuplicate) {
    // 列表里有一个含空格的脏名字时，同名候选仍先报非法字符
    EXPECT_EQ(CheckName("my theme", {"my theme"}), NameError::InvalidChars);
}

// ============================================
// CheckName：重名
// ============================================

TEST(PreferencesTemplateNameCheck, ExactDuplicateIsRejected) {
    EXPECT_EQ(CheckName("night", kExisting), NameError::Duplicate);
    EXPECT_EQ(CheckName("default", kExisting), NameError::Duplicate);
}

TEST(PreferencesTemplateNameCheck, DuplicateIsCaseInsensitive) {
    EXPECT_EQ(CheckName("Night", kExisting), NameError::Duplicate);
    EXPECT_EQ(CheckName("NIGHT", kExisting), NameError::Duplicate);
    EXPECT_EQ(CheckName("studio-mix_2", kExisting), NameError::Duplicate);
}

TEST(PreferencesTemplateNameCheck, RenamingToOwnNameCountsAsDuplicate) {
    // Rename 把旧名一并放进 existing：改回自己或只改大小写都不算有效的新名字
    const std::vector<std::string> withSelf = {"night"};
    EXPECT_EQ(CheckName("night", withSelf), NameError::Duplicate);
    EXPECT_EQ(CheckName("NIGHT", withSelf), NameError::Duplicate);
}

// ============================================
// CheckName：通过
// ============================================

TEST(PreferencesTemplateNameCheck, LettersDigitsHyphenUnderscoreAreAccepted) {
    EXPECT_EQ(CheckName("day", kExisting), NameError::None);
    EXPECT_EQ(CheckName("Theme_2026-09", kExisting), NameError::None);
    EXPECT_EQ(CheckName("42", kExisting), NameError::None);
    EXPECT_EQ(CheckName("_", kExisting), NameError::None);
    EXPECT_EQ(CheckName("-", kExisting), NameError::None);
}

TEST(PreferencesTemplateNameCheck, PrefixOfAnExistingNameIsNotADuplicate) {
    EXPECT_EQ(CheckName("nigh", kExisting), NameError::None);
    EXPECT_EQ(CheckName("nights", kExisting), NameError::None);
}

TEST(PreferencesTemplateNameCheck, EmptyExistingListAcceptsAnyValidName) {
    EXPECT_EQ(CheckName("default", {}), NameError::None);
}

TEST(PreferencesTemplateNameCheck, TrimThenCheckIsTheDialogPath) {
    // 对话框先 Trim 再 CheckName：首尾空白不会让一个合法名字变非法，也不会绕过重名
    EXPECT_EQ(CheckName(Trim("  day  "), kExisting), NameError::None);
    EXPECT_EQ(CheckName(Trim("  night  "), kExisting), NameError::Duplicate);
    EXPECT_EQ(CheckName(Trim("   "), kExisting), NameError::Empty);
}
