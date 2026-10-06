#pragma once

// ============================================
// WindowUtilsCore.h - WindowUtils 中零 foobar2000 SDK 依赖的部分
//
// WindowUtils.h 因 GetUserBackdropEffectString() 需要 core/PreferencesPage.h
// （fb2k SDK），测试项目无法包含它。本头文件只保留不依赖 SDK 的符号，使测试
// 可以直接包含并测试**真实生产符号**，而不必在测试里另写一份实现（那样生产
// 代码漂移时测试仍会通过）。
//
// WindowUtils.h include 本文件并保持同一 namespace，消费者继续 include
// WindowUtils.h 即可。
// ============================================

#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

namespace WindowUtils {

using json = nlohmann::json;

// Shared string-to-lowercase (eliminates duplication across PopupWindow / MainWindow / WindowChromeResolver)
inline std::string ToLower(std::string v) {
    std::transform(v.begin(), v.end(), v.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return v;
}

// Shared JSON bool safe extraction (eliminates duplication across PopupWindow / MainWindow / WindowChromeResolver)
inline bool TryGetBool(const json& obj, const char* key, bool& out) {
    if (!obj.is_object() || !obj.contains(key) || !obj[key].is_boolean()) return false;
    out = obj[key].get<bool>();
    return true;
}

inline bool IsPluginManagedBackdropEffect(std::string_view effect) {
    return effect == "mica" || effect == "mica-alt" || effect == "acrylic";
}

} // namespace WindowUtils
