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

    // 句柄非空不等于句柄有效：正在销毁的 shell 仍持有陈旧 HWND。若放它通过，
    // 下游按 96 DPI 回退换算后仍返回 success，等于把无效目标伪装成成功变更。
    // 故此处与 GetShellDpi 采用同一强度的校验。
    const auto isLive = [](HWND hwnd) { return hwnd && IsWindow(hwnd); };

    if (windowId == "main") {
        auto* mainWin = wm.GetMainWindow();
        if (mainWin && isLive(mainWin->GetHwnd())) {
            result.shell = static_cast<WindowShellBase*>(mainWin);
            result.windowId = "main";
        } else {
            result.error = ApiError::WINDOW_NOT_FOUND;
        }
        return result;
    }

    auto* popup = wm.GetPopup(windowId);
    if (popup && isLive(popup->GetHwnd())) {
        result.shell = static_cast<WindowShellBase*>(popup);
        result.windowId = windowId;
    } else {
        result.error = ApiError::WINDOW_NOT_FOUND;
    }
    return result;
}

WindowTargetResult WindowTargetResolver::ResolveByCallerHwnd(HWND callerHwnd) {
    WindowTargetResult result;
    // 本方法是 public，调用方未必经过 ExtractCallerHwnd 的 IsWindow 校验，
    // 故在入口独立校验一次：陈旧句柄不得解析成 target。
    if (!callerHwnd || !IsWindow(callerHwnd)) {
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

// 把 params 归类为纯策略层的输入。
//
// 归类需要 HWND 查找（IsWindow 校验、面板判定），而策略层不依赖 Win32，
// 故分类在此完成、决策交给 window_target_policy::SelectTarget。
WindowTargetResult WindowTargetResolver::ResolveWithIntent(
    const json& params, window_target_policy::TargetIntent intent) {
    using namespace window_target_policy;

    TargetRequest request;
    if (params.contains("windowId") && params["windowId"].is_string()) {
        std::string wid = params["windowId"].get<std::string>();
        if (!wid.empty()) {
            request.hasExplicitWindowId = true;
            request.explicitWindowId = std::move(wid);
        }
    }

    // 只有在没有显式 windowId 时才需要检查 caller —— 显式 id 无条件优先，
    // 此时省掉 caller 解析既避免无谓的实例遍历与加锁，也避免面板页面
    // 显式操作其他窗口（如 window.focus("main")）被自身面板身份拦下。
    //
    // caller 解析**只做一次**并缓存：面板判定必须复用 ResolveByCallerHwnd
    // 内部的 main → popup → panel 优先级，而不是在此另立一套。否则一旦
    // 某个 HWND 同时命中我们的 shell 与某个面板实例（拓扑上是否可能取决于
    // 宿主窗口层级，不应由本层假设），两处判定就会分歧。
    HWND callerHwnd = nullptr;
    std::optional<WindowTargetResult> callerResult;
    if (!request.hasExplicitWindowId) {
        callerHwnd = ExtractCallerHwnd(params);
        if (callerHwnd) {
            request.hasCallerHwnd = true;
            callerResult = ResolveByCallerHwnd(callerHwnd);
            // panelCaller 由 ResolveByCallerHwnd 在「既非 main 也非 popup、
            // 但属于某个 DUI/CUI 实例」时置位，正是策略层需要的分类。
            request.callerIsPanel = callerResult->panelCaller;
        }
    }

    const TargetDecision decision = SelectTarget(request, intent);

    switch (decision.route) {
        case TargetRoute::ById:
            return ResolveById(decision.windowId);

        case TargetRoute::ByCallerHwnd:
        case TargetRoute::FailPanelCaller:
            // 两者都用已缓存的解析结果：成功时它就是目标，面板时它已带上
            // panelCaller 标志与 PANEL_CALLER_UNSUPPORTED 错误。
            if (callerResult.has_value()) {
                return *callerResult;
            }
            [[fallthrough]];

        case TargetRoute::Fail:
        default: {
            WindowTargetResult result;
            result.error = decision.error.empty()
                ? ApiError::WINDOW_NOT_FOUND
                : decision.error;
            return result;
        }
    }
}

WindowTargetResult WindowTargetResolver::ResolveForMutation(const json& params) {
    // 决策表由 window_target_policy 持有并被单测固定，此处不再复制分支逻辑。
    // 对 mutating shell API：找不到 target 必须失败，禁止静默回退 main。
    return ResolveWithIntent(params, window_target_policy::TargetIntent::Mutation);
}

WindowTargetResult WindowTargetResolver::ResolveForObservation(const json& params) {
    // 与 Mutation 共用同一决策表。
    //
    // Q7-1 取消了 observation 的主窗口回退：回退会向非主窗口的调用方返回
    // 属于另一个窗口的几何/状态值，调用方无从分辨——这正是本项目要消灭的
    // 静默错值形态，比返回错误更有害。故两种意图行为一致。
    return ResolveWithIntent(params, window_target_policy::TargetIntent::Observation);
}
