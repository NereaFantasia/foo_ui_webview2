#pragma once

#include <Windows.h>
#include <algorithm>
#include <optional>
#include <string>

namespace window_caller {

struct Identity {
    std::string windowId = "main";
    std::string mode = "standalone";
    bool panelMode = false;
    HWND panelHwnd = nullptr;

    std::string CurrentWindowId() const {
        return windowId.empty() ? "main" : windowId;
    }

    std::string ModeWindowId() const {
        return windowId.empty() ? "panel" : windowId;
    }
};

// lookup 只做精确 HWND 匹配；先检查调用方自身，再逐级查找真实父窗口。
template <typename Lookup>
Identity Resolve(HWND callerHwnd, Lookup&& lookup, bool fallbackPanelMode) {
    for (HWND hwnd = callerHwnd; hwnd && ::IsWindow(hwnd); hwnd = ::GetAncestor(hwnd, GA_PARENT)) {
        if (auto identity = lookup(hwnd)) return *identity;
    }

    Identity fallback;
    fallback.mode = fallbackPanelMode ? "panel" : "standalone";
    fallback.panelMode = fallbackPanelMode;
    return fallback;
}

template <typename Manager, typename Context>
Identity Resolve(HWND callerHwnd, Manager& manager, Context& context, bool fallbackPanelMode) {
    auto* mainWindow = manager.GetMainWindow();
    const auto instances = context.GetAllInstances();
    const auto windowIds = manager.GetAllWindowIds();

    return Resolve(callerHwnd, [&](HWND hwnd) -> std::optional<Identity> {
        if (mainWindow && mainWindow->GetHwnd() == hwnd) return Identity{};
        for (const auto& id : windowIds) {
            if (id == "main") continue;
            auto* popup = manager.GetPopup(id);
            if (popup && popup->GetHwnd() == hwnd) return Identity{id, "standalone", false, nullptr};
        }

        // GetPanelByHwnd 自带顶层回退，只对已经精确命中的登记项调用。
        if (std::find(instances.begin(), instances.end(), hwnd) == instances.end()) return std::nullopt;
        auto* panel = context.GetPanelByHwnd(hwnd);
        if (!panel) return std::nullopt;

        Identity identity;
        identity.windowId = context.GetWindowIdByHwnd(hwnd);
        identity.panelHwnd = hwnd;
        identity.panelMode = panel->IsPanelMode();
        using PanelMode = decltype(panel->GetMode());
        switch (panel->GetMode()) {
            case PanelMode::Standalone: identity.mode = "standalone"; break;
            case PanelMode::DuiPanel:   identity.mode = "dui"; break;
            case PanelMode::CuiPanel:   identity.mode = "cui"; break;
            default: identity.mode = "unknown"; break;
        }
        return identity;
    }, fallbackPanelMode);
}

} // namespace window_caller
