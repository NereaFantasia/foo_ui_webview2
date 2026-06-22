#pragma once
#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include "core/PreferencesPage.h"

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

inline std::string GetUserBackdropEffectString() {
    switch (webview_prefs::GetBackdropEffect()) {
        case webview_prefs::BackdropEffect::None:
            return "none";
        case webview_prefs::BackdropEffect::Acrylic:
            return "acrylic";
        case webview_prefs::BackdropEffect::MicaAlt:
        case webview_prefs::BackdropEffect::Tabbed:
            return "mica-alt";
        case webview_prefs::BackdropEffect::Mica:
        default:
            return "mica";
    }
}

inline bool IsPluginManagedBackdropEffect(std::string_view effect) {
    return effect == "mica" || effect == "mica-alt" || effect == "acrylic";
}

} // namespace WindowUtils
