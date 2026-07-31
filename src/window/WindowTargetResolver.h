#pragma once

#include "pch.h"

class WindowShellBase;

// ============================================
// WindowTargetResult - target 解析结果
// ============================================
struct WindowTargetResult {
    WindowShellBase* shell = nullptr;
    std::string windowId;
    std::string error;
    // 调用方是 DUI/CUI 面板实例。面板不实现 WindowShellBase，永远不可能成为
    // target，故此标志只在失败结果上出现，用于产出与 WindowApi.cpp 既有
    // PanelModeUnsupported() 一致的响应形状。
    bool panelCaller = false;

    bool Success() const { return shell != nullptr; }

    // 生成标准错误响应 JSON
    json ErrorResponse() const {
        if (panelCaller) {
            // 与 WindowApi.cpp 的 PanelModeUnsupported() 保持同一形状，
            // 使前端无需区分「宏在 handler 入口拦下」和「resolver 在
            // 解析阶段拦下」两种来源。
            return {
                {"success", false},
                {"supported", false},
                {"panelMode", true},
                {"error", error}
            };
        }
        return {{"success", false}, {"error", error}};
    }
};

// ============================================
// WindowTargetResolver - 统一 target 解析
//
// 替代 WindowApi.cpp 中散落的 GetCallerHwnd /
// FindMainByCallerHwnd / FindPopupByCallerHwnd 模式。
//
// 两种意图的差别只在「显式 id 与 caller 都不可用」时：
//   - Mutation:    失败（禁止静默改错窗口）
//   - Observation: 同样失败
// 即两者当前语义一致（见 ResolveForObservation 的说明），保留双入口是为了
// 在调用点表达意图，并为将来可能的意图相关策略留出位置。
//
// 面板调用方（DUI/CUI）一律显式失败：面板不实现 WindowShellBase。
// ============================================
class WindowTargetResolver {
public:
    // 对 mutating shell API: 找不到 target 必须失败，禁止静默回退 main
    static WindowTargetResult ResolveForMutation(const json& params);

    // 对 observation API: 同样禁止回退 main（见 .cpp 内说明）
    static WindowTargetResult ResolveForObservation(const json& params);

    // 通过显式 windowId 解析
    static WindowTargetResult ResolveById(const std::string& windowId);

    // 通过 caller HWND 解析
    static WindowTargetResult ResolveByCallerHwnd(HWND callerHwnd);

    // 从 params 提取 caller HWND（不做 fallback）
    static HWND ExtractCallerHwnd(const json& params);

    // caller HWND 是否属于某个 DUI/CUI 面板实例。
    // 面板实例的 windowId 由 WindowManager::GeneratePanelId() 产出（"panel_N"），
    // 既不是 "main" 也不在 popups_ 中，故永远无法解析为 shell。
    static bool IsPanelCallerHwnd(HWND callerHwnd);
};
