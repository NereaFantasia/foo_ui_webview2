#pragma once
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>
#include "core/PreferencesPage.h"
// 零 foobar2000 SDK 依赖的 helper（ToLower / TryGetBool /
// IsPluginManagedBackdropEffect）已拆到 WindowUtilsCore.h，位于同一
// namespace，故本头文件的既有消费者无需任何改动。
// 拆分动机见 WindowUtilsCore.h 头部说明（P0-b / R3：让测试链接真实生产符号）。
#include "utils/WindowUtilsCore.h"

namespace WindowUtils {

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

} // namespace WindowUtils
