// test_window_utils.cpp - WindowUtils inline utility functions
// Reimplementation to avoid PreferencesPage.h SDK dependency
#include "pch.h"
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>

// ============================================
// Reimplementation for testing (originals in WindowUtils.h)
// Cannot include WindowUtils.h directly due to PreferencesPage.h SDK dep
// ============================================
namespace reimpl {

inline std::string ToLower(std::string v) {
    std::transform(v.begin(), v.end(), v.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return v;
}

inline bool TryGetBool(const nlohmann::json& obj, const char* key, bool& out) {
    if (!obj.is_object() || !obj.contains(key) || !obj[key].is_boolean()) return false;
    out = obj[key].get<bool>();
    return true;
}

inline bool IsPluginManagedBackdropEffect(std::string_view effect) {
    return effect == "mica" || effect == "mica-alt" || effect == "acrylic";
}

} // namespace reimpl

using json = nlohmann::json;

// ============================================
// ToLower
// ============================================

TEST(ToLower, EmptyString) {
    EXPECT_EQ(reimpl::ToLower(""), "");
}

TEST(ToLower, AllUppercase) {
    EXPECT_EQ(reimpl::ToLower("HELLO WORLD"), "hello world");
}

TEST(ToLower, MixedCase) {
    EXPECT_EQ(reimpl::ToLower("HeLLo"), "hello");
}

TEST(ToLower, AlreadyLower) {
    EXPECT_EQ(reimpl::ToLower("already lower"), "already lower");
}

TEST(ToLower, WithDigitsAndSymbols) {
    EXPECT_EQ(reimpl::ToLower("ABC-123_XYZ"), "abc-123_xyz");
}

// ============================================
// TryGetBool
// ============================================

TEST(TryGetBool, ValidBoolTrue) {
    json obj = {{"key", true}};
    bool out = false;
    EXPECT_TRUE(reimpl::TryGetBool(obj, "key", out));
    EXPECT_TRUE(out);
}

TEST(TryGetBool, ValidBoolFalse) {
    json obj = {{"key", false}};
    bool out = true;
    EXPECT_TRUE(reimpl::TryGetBool(obj, "key", out));
    EXPECT_FALSE(out);
}

TEST(TryGetBool, MissingKey) {
    json obj = {{"other", true}};
    bool out = false;
    EXPECT_FALSE(reimpl::TryGetBool(obj, "key", out));
}

TEST(TryGetBool, NonBoolValue) {
    json obj = {{"key", 42}};
    bool out = false;
    EXPECT_FALSE(reimpl::TryGetBool(obj, "key", out));
}

TEST(TryGetBool, StringValueNotBool) {
    json obj = {{"key", "true"}};
    bool out = false;
    EXPECT_FALSE(reimpl::TryGetBool(obj, "key", out));
}

TEST(TryGetBool, NonObjectInput) {
    json arr = json::array({1, 2, 3});
    bool out = false;
    EXPECT_FALSE(reimpl::TryGetBool(arr, "key", out));
}

// ============================================
// IsPluginManagedBackdropEffect
// ============================================

TEST(IsPluginManagedBackdropEffect, Mica) {
    EXPECT_TRUE(reimpl::IsPluginManagedBackdropEffect("mica"));
}

TEST(IsPluginManagedBackdropEffect, MicaAlt) {
    EXPECT_TRUE(reimpl::IsPluginManagedBackdropEffect("mica-alt"));
}

TEST(IsPluginManagedBackdropEffect, Acrylic) {
    EXPECT_TRUE(reimpl::IsPluginManagedBackdropEffect("acrylic"));
}

TEST(IsPluginManagedBackdropEffect, None) {
    EXPECT_FALSE(reimpl::IsPluginManagedBackdropEffect("none"));
}

TEST(IsPluginManagedBackdropEffect, Empty) {
    EXPECT_FALSE(reimpl::IsPluginManagedBackdropEffect(""));
}

TEST(IsPluginManagedBackdropEffect, Unknown) {
    EXPECT_FALSE(reimpl::IsPluginManagedBackdropEffect("blur"));
}
