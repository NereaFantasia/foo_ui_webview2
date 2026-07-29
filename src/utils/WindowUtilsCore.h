#pragma once

// ============================================
// WindowUtilsCore.h - WindowUtils 中零 foobar2000 SDK 依赖的部分
//
// 拆分动机（P0-b / R3）：WindowUtils.h 因 GetUserBackdropEffectString() 需要
// core/PreferencesPage.h（fb2k SDK），导致整个头文件无法被测试项目包含。
// 既有 tests/test_window_utils.cpp 因此在 namespace reimpl 内**重新实现**了
// 这些 helper——生产代码漂移时测试仍会通过，等于没有防护。
//
// 本头文件只保留不依赖 SDK 的符号，使测试可以直接包含并测试**真实生产符号**。
// WindowUtils.h 继续 include 本文件并保持同一 namespace，故所有既有消费者
// （MainWindow.cpp / PopupWindow.cpp / WindowChromeResolver.cpp）无需改动。
//
// 本次拆分不改变任何函数体，属纯搬迁。
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
