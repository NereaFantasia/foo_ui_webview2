#include "pch.h"
#include "window/WindowTargetResolver.h"
#include "window/WindowShellBase.h"
#include "window/WindowManager.h"
#include "window/MainWindow.h"
#include "window/PopupWindow.h"
#include "core/WebViewContext.h"
#include "api/ApiConstants.h"

HWND WindowTargetResolver::ExtractCallerHwnd(const json& params) {
    if (params.contains("_callerHwnd") && params["_callerHwnd"].is_number_integer()) {
        auto hwnd = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
        if (hwnd && IsWindow(hwnd)) {
            // WebViewPanel.hwnd_ 可能是子窗口，获取顶级窗口
            HWND topLevel = ::GetAncestor(hwnd, GA_ROOT);
            return topLevel ? topLevel : hwnd;
        }
    }
    return nullptr;
}

WindowTargetResult WindowTargetResolver::ResolveById(const std::string& windowId) {
    WindowTargetResult result;
    auto& wm = WindowManager::GetInstance();

    if (windowId == "main") {
        auto* mainWin = wm.GetMainWindow();
        if (mainWin && mainWin->GetHwnd()) {
            result.shell = static_cast<WindowShellBase*>(mainWin);
            result.windowId = "main";
        } else {
            result.error = ApiError::WINDOW_NOT_FOUND;
        }
        return result;
    }

    auto* popup = wm.GetPopup(windowId);
    if (popup && popup->GetHwnd()) {
        result.shell = static_cast<WindowShellBase*>(popup);
        result.windowId = windowId;
    } else {
        result.error = ApiError::WINDOW_NOT_FOUND;
    }
    return result;
}

WindowTargetResult WindowTargetResolver::ResolveByCallerHwnd(HWND callerHwnd) {
    WindowTargetResult result;
    if (!callerHwnd) {
        result.error = ApiError::WINDOW_NOT_FOUND;
        return result;
    }

    auto& wm = WindowManager::GetInstance();

    // 检查主窗口
    auto* mainWin = wm.GetMainWindow();
    if (mainWin && mainWin->GetHwnd() == callerHwnd) {
        result.shell = static_cast<WindowShellBase*>(mainWin);
        result.windowId = "main";
        return result;
    }

    // 检查弹出窗口
    for (const auto& id : wm.GetAllWindowIds()) {
        if (id == "main") continue;
        auto* popup = wm.GetPopup(id);
        if (popup && popup->GetHwnd() == callerHwnd) {
            result.shell = static_cast<WindowShellBase*>(popup);
            result.windowId = id;
            return result;
        }
    }

    // 面板模式: 通过 WebViewContext 反查 windowId
    auto& ctx = WebViewContext::GetInstance();
    for (auto instanceHwnd : ctx.GetAllInstances()) {
        if (instanceHwnd == callerHwnd ||
            ::GetAncestor(instanceHwnd, GA_ROOT) == callerHwnd) {
            std::string wid = ctx.GetWindowIdByHwnd(instanceHwnd);
            if (!wid.empty()) {
                // 解析失败时继续扫描其他匹配实例
                auto resolved = ResolveById(wid);
                if (resolved.Success()) {
                    return resolved;
                }
            }
        }
    }

    result.error = ApiError::WINDOW_NOT_FOUND;
    return result;
}

WindowTargetResult WindowTargetResolver::ResolveForMutation(const json& params) {
    // 1. 显式 windowId 优先
    if (params.contains("windowId") && params["windowId"].is_string()) {
        std::string wid = params["windowId"].get<std::string>();
        if (!wid.empty()) {
            return ResolveById(wid);
        }
    }

    // 2. _callerHwnd
    HWND callerHwnd = ExtractCallerHwnd(params);
    if (callerHwnd) {
        return ResolveByCallerHwnd(callerHwnd);
    }

    // 3. 对 mutating shell API: 禁止静默回退到 main
    WindowTargetResult result;
    result.error = ApiError::WINDOW_NOT_FOUND;
    return result;
}

WindowTargetResult WindowTargetResolver::ResolveForObservation(const json& params) {
    // 1. 显式 windowId
    if (params.contains("windowId") && params["windowId"].is_string()) {
        std::string wid = params["windowId"].get<std::string>();
        if (!wid.empty()) {
            return ResolveById(wid);
        }
    }

    // 2. _callerHwnd
    HWND callerHwnd = ExtractCallerHwnd(params);
    if (callerHwnd) {
        auto result = ResolveByCallerHwnd(callerHwnd);
        if (result.Success()) return result;
    }

    // 3. Observation API: 允许 fallback 到 main（向后兼容）
    auto& wm = WindowManager::GetInstance();
    auto* mainWin = wm.GetMainWindow();
    if (mainWin && mainWin->GetHwnd()) {
        WindowTargetResult result;
        result.shell = static_cast<WindowShellBase*>(mainWin);
        result.windowId = "main";
        return result;
    }

    WindowTargetResult result;
    result.error = ApiError::WINDOW_NOT_FOUND;
    return result;
}
