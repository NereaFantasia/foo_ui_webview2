// test_window_utils.cpp - WindowUtils inline utility functions
//
// P0-b / R3: 本文件原先在 namespace reimpl 内**重新实现**被测 helper，
// 生产代码漂移时测试仍会通过（等于无防护）。现已改为包含真实生产头文件
// WindowUtilsCore.h 并测试真实符号 WindowUtils::*。
//
// 之所以能直接包含：GetUserBackdropEffectString() 所需的 fb2k SDK 依赖
// （core/PreferencesPage.h）已留在 WindowUtils.h，WindowUtilsCore.h 零 SDK 依赖。
#include "pch.h"
#include "../src/utils/WindowUtilsCore.h"

using json = nlohmann::json;

// ============================================
// ToLower
// ============================================

TEST(ToLower, EmptyString) {
    EXPECT_EQ(WindowUtils::ToLower(""), "");
}

TEST(ToLower, AllUppercase) {
    EXPECT_EQ(WindowUtils::ToLower("HELLO WORLD"), "hello world");
}

TEST(ToLower, MixedCase) {
    EXPECT_EQ(WindowUtils::ToLower("HeLLo"), "hello");
}

TEST(ToLower, AlreadyLower) {
    EXPECT_EQ(WindowUtils::ToLower("already lower"), "already lower");
}

TEST(ToLower, WithDigitsAndSymbols) {
    EXPECT_EQ(WindowUtils::ToLower("ABC-123_XYZ"), "abc-123_xyz");
}

// ============================================
// TryGetBool
// ============================================

TEST(TryGetBool, ValidBoolTrue) {
    json obj = {{"key", true}};
    bool out = false;
    EXPECT_TRUE(WindowUtils::TryGetBool(obj, "key", out));
    EXPECT_TRUE(out);
}

TEST(TryGetBool, ValidBoolFalse) {
    json obj = {{"key", false}};
    bool out = true;
    EXPECT_TRUE(WindowUtils::TryGetBool(obj, "key", out));
    EXPECT_FALSE(out);
}

TEST(TryGetBool, MissingKey) {
    json obj = {{"other", true}};
    bool out = false;
    EXPECT_FALSE(WindowUtils::TryGetBool(obj, "key", out));
}

TEST(TryGetBool, NonBoolValue) {
    json obj = {{"key", 42}};
    bool out = false;
    EXPECT_FALSE(WindowUtils::TryGetBool(obj, "key", out));
}

TEST(TryGetBool, StringValueNotBool) {
    json obj = {{"key", "true"}};
    bool out = false;
    EXPECT_FALSE(WindowUtils::TryGetBool(obj, "key", out));
}

TEST(TryGetBool, NonObjectInput) {
    json arr = json::array({1, 2, 3});
    bool out = false;
    EXPECT_FALSE(WindowUtils::TryGetBool(arr, "key", out));
}

// 失败路径不得改写 out —— PopupWindow / WindowChromeResolver 依赖
// "取不到就保留调用方默认值" 的语义。
TEST(TryGetBool, FailureLeavesOutUnchanged) {
    json obj = {{"key", "not-a-bool"}};
    bool out = true;
    EXPECT_FALSE(WindowUtils::TryGetBool(obj, "key", out));
    EXPECT_TRUE(out);
}

// ============================================
// IsPluginManagedBackdropEffect
// ============================================

TEST(IsPluginManagedBackdropEffect, Mica) {
    EXPECT_TRUE(WindowUtils::IsPluginManagedBackdropEffect("mica"));
}

TEST(IsPluginManagedBackdropEffect, MicaAlt) {
    EXPECT_TRUE(WindowUtils::IsPluginManagedBackdropEffect("mica-alt"));
}

TEST(IsPluginManagedBackdropEffect, Acrylic) {
    EXPECT_TRUE(WindowUtils::IsPluginManagedBackdropEffect("acrylic"));
}

TEST(IsPluginManagedBackdropEffect, None) {
    EXPECT_FALSE(WindowUtils::IsPluginManagedBackdropEffect("none"));
}

TEST(IsPluginManagedBackdropEffect, Empty) {
    EXPECT_FALSE(WindowUtils::IsPluginManagedBackdropEffect(""));
}

TEST(IsPluginManagedBackdropEffect, Unknown) {
    EXPECT_FALSE(WindowUtils::IsPluginManagedBackdropEffect("blur"));
}
