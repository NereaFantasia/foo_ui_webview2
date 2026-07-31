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

    // 面板模式（DUI/CUI）: 显式失败。
    //
    // 此处**不能**尝试反查再 ResolveById：面板实例的 windowId 由
    // WindowManager::GeneratePanelId() 产出（"panel_N"），既不等于 "main"
    // 也不会进入 popups_，所以 ResolveById 对它结构上不可能成功。旧实现
    // 在这里调 ResolveById 是一条永远走不通的死路径（Q8），且掩盖了真正
    // 的原因：面板不实现 WindowShellBase，本就不是合法 target。
    if (IsPanelCallerHwnd(callerHwnd)) {
        result.error = ApiError::PANEL_CALLER_UNSUPPORTED;
        result.panelCaller = true;
        return result;
    }

    result.error = ApiError::WINDOW_NOT_FOUND;
    return result;
}

bool WindowTargetResolver::IsPanelCallerHwnd(HWND callerHwnd) {
    if (!callerHwnd) return false;

    auto& ctx = WebViewContext::GetInstance();
    for (auto instanceHwnd : ctx.GetAllInstances()) {
        // callerHwnd 已被 ExtractCallerHwnd 提升到 GA_ROOT。面板实例本身是
        // 宿主框架内的子窗口，其 GA_ROOT 是 fb2k 主框架而非实例自身，因此
        // 两种匹配方向都要检查。
        //
        // 注意：面板下 GA_ROOT 究竟返回哪个窗口属于 R10 的未验证项，需在
        // fb2k 运行时加载 DUI/CUI 面板后实测确认；此处同时匹配两种方向，
        // 无论实测结果如何都能命中。
        const bool matches = (instanceHwnd == callerHwnd) ||
                             (::GetAncestor(instanceHwnd, GA_ROOT) == callerHwnd);
        if (!matches) continue;

        // panel 指针只在 DUI/CUI 注册路径上非空（WebViewCuiPanel /
        // WebViewDuiElement 传 this）；独立窗口与 popup 走不带 panel 的
        // 重载，故此判定是结构性的，不依赖 windowId 的字符串形状。
        if (ctx.GetPanelByHwnd(instanceHwnd) != nullptr) {
            return true;
        }
    }
    return false;
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
        return ResolveByCallerHwnd(callerHwnd);
    }

    // 3. 不回退主窗口（Q7）。
    //
    // 旧实现在此回退 main，理由是「向后兼容」。但对**非主窗口**的调用方
    // （面板、或 caller 已销毁），回退会返回一个看似合法、实际属于另一个
    // 窗口的几何/状态值——调用方无从分辨。这正是本项目要消灭的静默错值
    // 形态，比返回错误更有害。
    //
    // 因此 observation 与 mutation 在「无显式 id 且 caller 不可解析」时
    // 行为一致：显式失败。
    WindowTargetResult result;
    result.error = ApiError::WINDOW_NOT_FOUND;
    return result;
}
