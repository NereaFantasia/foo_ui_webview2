#pragma once

#include "pch.h"
#include "window/WindowTargetPolicy.h"

class WindowShellBase;

// ============================================
// WindowTargetResult - target 解析结果
// ============================================
struct WindowTargetResult {
    WindowShellBase* shell = nullptr;
    std::string windowId;
    std::string error;
    // 调用方是 DUI/CUI 面板实例。面板不实现 WindowShellBase，永远不可能成为
    // target，故此标志只在失败结果上出现，handler 据此以 PANEL_MODE_UNSUPPORTED
    // 失败（api::PanelModeUnsupported）。
    bool panelCaller = false;

    bool Success() const { return shell != nullptr; }
};

// ============================================
// WindowTargetResolver - 统一 target 解析
//
// 替代 WindowApi.cpp 中散落的 GetCallerHwnd /
// FindMainByCallerHwnd / FindPopupByCallerHwnd 模式。
//
// 分支决策不在本类内实现：`ResolveWithIntent` 先把 windowId 与调用方句柄归类为
// `window_target_policy::TargetRequest`（这一步需要 Win32 查找），再由
// `window_target_policy::SelectTarget` 选路。这样决策表只有一份、且被
// `tests/test_window_target_policy.cpp` 固定住——避免出现「策略层与真实
// 解析各写一份分支」的漂移。
//
// 两种意图当前语义一致（Q7-1 取消了 observation 的主窗口回退），保留双入口
// 是为了在调用点表达意图，并为将来可能的意图相关策略留出位置。
//
// 面板调用方（DUI/CUI）一律显式失败：面板不实现 WindowShellBase。
// ============================================
class WindowTargetResolver {
public:
    // windowId 是解析好的参数（空串与省略等价），callerHwnd 是 CallerContext::callerHwnd
    // （桥接层注入的原始句柄，未提升到顶级窗口）。
    // 对 mutating shell API：找不到 target 必须失败，禁止静默回退 main。
    static WindowTargetResult ResolveForMutation(
        const std::optional<std::string>& windowId, HWND callerHwnd);
    // 对 observation API：同样禁止回退 main（见 .cpp 内说明）。
    static WindowTargetResult ResolveForObservation(
        const std::optional<std::string>& windowId, HWND callerHwnd);

    // 依给定意图解析。Mutation/Observation 各入口都委托到此，
    // 决策交给 window_target_policy::SelectTarget。
    static WindowTargetResult ResolveWithIntent(
        const std::optional<std::string>& windowId, HWND rawCallerHwnd,
        window_target_policy::TargetIntent intent);

    // 通过显式 windowId 解析
    static WindowTargetResult ResolveById(const std::string& windowId);

    // 通过 caller HWND 解析
    static WindowTargetResult ResolveByCallerHwnd(HWND callerHwnd);

    // 把调用方句柄提升到顶级窗口（面板的句柄可能是子窗口）；空或已失效的句柄返回
    // nullptr，不做 fallback。
    static HWND TopLevelCallerHwnd(HWND callerHwnd);

    // caller HWND 是否属于某个 DUI/CUI 面板实例。
    // 面板实例的 windowId 由 WindowManager::GeneratePanelId() 产出（"panel_N"），
    // 既不是 "main" 也不在 popups_ 中，故永远无法解析为 shell。
    static bool IsPanelCallerHwnd(HWND callerHwnd);
};
