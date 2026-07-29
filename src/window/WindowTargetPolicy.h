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
// 语义与 `WindowTargetResolver.cpp` 当前实现严格对齐（P0 不改变任何行为）：
//   - ResolveForMutation：显式 id 优先 → caller → **失败**（禁止回退 main）
//   - ResolveForObservation：显式 id 优先 → caller → **回退 main**
//
// 注意：P0 阶段本文件只是新增的可测 seam，`WindowTargetResolver` 接入它
// 属 P1' 范围。Q7/Q8 的结论可能改变 observation 是否保留 main 回退。
// ============================================

#include <string>

#include "api/ApiConstants.h"

namespace window_target_policy {

// 调用意图。决定「无显式 id 且 caller 不可解析」时是失败还是回退主窗口。
enum class TargetIntent {
    // 会改写窗口状态的 API。找不到 target 必须失败。
    Mutation,
    // 只读取窗口状态的 API。当前实现允许回退主窗口。
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
};

// 策略选出的解析路径。
enum class TargetRoute {
    // 按显式 windowId 查找（对应 WindowTargetResolver::ResolveById）
    ById,
    // 按 caller HWND 查找（对应 WindowTargetResolver::ResolveByCallerHwnd）
    ByCallerHwnd,
    // 回退主窗口（仅 Observation 意图可达）
    FallbackMain,
    // 无可用路径，必须返回错误
    Fail,
};

struct TargetDecision {
    TargetRoute route = TargetRoute::Fail;
    // route == ById 时要查找的 id
    std::string windowId;
    // route == Fail 时应返回的错误常量。非 Fail 时为空。
    // 取自 ApiError::WINDOW_NOT_FOUND —— 直接引用常量而非复制字面量，
    // 使常量改动不会在本文件产生静默漂移。
    std::string error;
    // 当 ById 路径失败时，Observation 是否允许继续回退到主窗口。
    //
    // 当前实现：**不允许**。ResolveForObservation 在显式 id 分支直接
    // `return ResolveById(wid)`，不再回退。即显式 id 写错时 observation
    // 也会失败，而不是静默返回主窗口数据。这一点两种意图行为相同。
    bool explicitIdMayFallBack = false;
};

// 解析失败时使用的错误常量，直接取自 ApiError 以避免字面量重复。
inline constexpr const char* kErrorWindowNotFound = ApiError::WINDOW_NOT_FOUND;

// 依意图与输入分类选出解析路径。
//
// 决策表（与 WindowTargetResolver 当前实现逐条对应）：
//
//   显式 id | caller | Mutation      | Observation
//   --------|--------|---------------|----------------
//   有      | 任意   | ById          | ById
//   无      | 有     | ByCallerHwnd  | ByCallerHwnd
//   无      | 无     | Fail          | FallbackMain
//
// 「caller 查找失败后是否回退 main」不属本函数职责：Mutation 直接返回
// ByCallerHwnd 的失败结果，Observation 在 ByCallerHwnd 失败后才回退。
// 该差异由 ObservationFallsBackAfterCallerMiss() 表达。
TargetDecision SelectTarget(const TargetRequest& request, TargetIntent intent);

// caller 路径查找失败后，该意图是否允许回退主窗口。
// Mutation: false（禁止静默改错窗口）；Observation: true（向后兼容）。
bool AllowsFallbackAfterCallerMiss(TargetIntent intent);

}  // namespace window_target_policy
