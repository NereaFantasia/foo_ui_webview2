#pragma once

// ============================================
// WindowTargetPolicy.h - 窗口目标解析的纯决策逻辑
//
// 目的（P0-b 可测性重构）：`WindowTargetResolver` 的**输入分类逻辑**
// （显式 windowId / caller / 两者皆缺 → 走哪条分支、失败时用哪个错误常量）
// 与真实 HWND 查找是两件事。前者是纯逻辑，可单测；后者需要活动窗口，
// 只能列入手工验证清单。
//
// 本头文件把前者剥离出来，不依赖 HWND / WindowManager / json，
// 使测试可以链接并测试真实生产符号。
//
// 语义与 `WindowTargetResolver.cpp` 严格对齐（P1' 已落实 Q7/Q8）：
//   - 显式 id 优先，两种意图相同，且 ById 失败不回退
//   - caller 是面板实例 → 显式失败（面板不实现 WindowShellBase）
//   - 两者皆缺 → 两种意图**一致失败**，均不回退主窗口
//
// 历史：P0 阶段 observation 在「两者皆缺」时回退主窗口。Q7-1 取消了该回退，
// 理由是回退会向非主窗口的调用方返回属于另一个窗口的值（静默错值形态）。
// ============================================

#include <string>

#include "api/ApiConstants.h"

namespace window_target_policy {

// 调用意图。
//
// Q7-1 之后两种意图的解析行为**完全一致**（都不回退主窗口）。保留区分是为了
// 让调用点显式表达意图，并为将来可能的意图相关策略留出位置——而不是因为
// 二者当前行为不同。
enum class TargetIntent {
    // 会改写窗口状态的 API。
    Mutation,
    // 只读取窗口状态的 API。
    Observation,
};

// 解析请求的输入分类。只描述「调用方提供了什么」，不含任何 HWND 查找结果。
struct TargetRequest {
    // params 是否含**非空字符串**的 windowId。
    // 注意：resolver 对空字符串 windowId 的处理等同于「未提供」。
    bool hasExplicitWindowId = false;
    // 显式 windowId 的值（hasExplicitWindowId 为 false 时无意义）。
    std::string explicitWindowId;
    // params 是否含可用的 _callerHwnd（已通过 IsWindow 校验并做过
    // GetAncestor(GA_ROOT) 提升）。本策略层不关心该 HWND 的具体值。
    bool hasCallerHwnd = false;
    // caller 是否为 DUI/CUI 面板实例。
    // 由 WindowTargetResolver::IsPanelCallerHwnd 判定后传入——那是需要
    // WebViewContext 的查找，不属本纯逻辑层职责。
    bool callerIsPanel = false;
};

// 策略选出的解析路径。
enum class TargetRoute {
    // 按显式 windowId 查找（对应 WindowTargetResolver::ResolveById）
    ById,
    // 按 caller HWND 查找（对应 WindowTargetResolver::ResolveByCallerHwnd）
    ByCallerHwnd,
    // 无可用路径，必须返回错误
    Fail,
    // caller 是面板实例：失败，且错误原因比 Fail 更确切（Q7-2）。
    // 独立成一路而非复用 Fail，是为了让响应能带上 panelMode 标志，
    // 与 WindowApi.cpp 既有的 PanelModeUnsupported() 形状一致。
    FailPanelCaller,
};

struct TargetDecision {
    TargetRoute route = TargetRoute::Fail;
    // route == ById 时要查找的 id
    std::string windowId;
    // route 为 Fail / FailPanelCaller 时应返回的错误常量，其余为空。
    // 直接引用 ApiError 常量而非复制字面量，使常量改动不会在本文件产生
    // 静默漂移。
    std::string error;
    // 当 ById 路径失败时，是否允许继续回退到主窗口。
    //
    // **不允许**：ResolveForObservation 在显式 id 分支直接
    // `return ResolveById(wid)`，不再回退。即显式 id 写错时 observation
    // 也会失败，而不是静默返回主窗口数据。这一点两种意图行为相同。
    bool explicitIdMayFallBack = false;
};

// 解析失败时使用的错误常量，直接取自 ApiError 以避免字面量重复。
inline constexpr const char* kErrorWindowNotFound = ApiError::WINDOW_NOT_FOUND;
inline constexpr const char* kErrorPanelCallerUnsupported =
    ApiError::PANEL_CALLER_UNSUPPORTED;

// 依意图与输入分类选出解析路径。
//
// 决策表（与 WindowTargetResolver 逐条对应；两种意图现已完全一致）：
//
//   显式 id | panel caller | caller | 结果
//   --------|--------------|--------|------------------
//   有      | 任意         | 任意   | ById
//   无      | 是           | 任意   | FailPanelCaller
//   无      | 否           | 有     | ByCallerHwnd
//   无      | 否           | 无     | Fail
//
// panel 判定先于 caller 判定：面板的 caller HWND 是可解析出实例的，
// 若先走 ByCallerHwnd 就会退化成泛化的 WINDOW_NOT_FOUND，丢失确切原因。
//
// 「caller 查找失败后不回退主窗口」不再需要单独的谓词来表达：TargetRoute
// 里已经没有 FallbackMain 这一路，故该性质由类型系统保证，而非靠约定。
TargetDecision SelectTarget(const TargetRequest& request, TargetIntent intent);

}  // namespace window_target_policy
