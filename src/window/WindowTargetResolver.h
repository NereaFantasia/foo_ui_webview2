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

    bool Success() const { return shell != nullptr; }

    // 生成标准错误响应 JSON
    json ErrorResponse() const {
        return {{"success", false}, {"error", error}};
    }
};

// ============================================
// WindowTargetResolver - 统一 target 解析
//
// 替代 WindowApi.cpp 中散落的 GetCallerHwnd /
// FindMainByCallerHwnd / FindPopupByCallerHwnd 模式。
//
// 对 mutating shell API: 找不到 target 必须失败
// 对 observation API: 允许 fallback 到 main（向后兼容）
// ============================================
class WindowTargetResolver {
public:
    // 对 mutating shell API: 找不到 target 必须失败，禁止静默回退 main
    static WindowTargetResult ResolveForMutation(const json& params);

    // 对 observation API: 允许 fallback 到 main（向后兼容）
    static WindowTargetResult ResolveForObservation(const json& params);

    // 通过显式 windowId 解析
    static WindowTargetResult ResolveById(const std::string& windowId);

    // 通过 caller HWND 解析
    static WindowTargetResult ResolveByCallerHwnd(HWND callerHwnd);

    // 从 params 提取 caller HWND（不做 fallback）
    static HWND ExtractCallerHwnd(const json& params);
};
