#include "pch.h"
#include "window/WindowTargetPolicy.h"

namespace window_target_policy {

TargetDecision SelectTarget(const TargetRequest& request, TargetIntent intent) {
    TargetDecision decision;

    // 1. 显式 windowId 优先。两种意图行为相同：直接走 ById，且失败不回退。
    if (request.hasExplicitWindowId && !request.explicitWindowId.empty()) {
        decision.route = TargetRoute::ById;
        decision.windowId = request.explicitWindowId;
        decision.explicitIdMayFallBack = false;
        return decision;
    }

    // 2. caller 是面板实例：显式失败（Q7-2）。
    //
    // 必须先于 ByCallerHwnd 判定：面板的 caller HWND 是「可解析出实例」的，
    // 但实例 windowId 形如 "panel_N"，既非 "main" 也不在 popups_ 中，
    // 故按 caller 查找必然失败。与其返回泛化的 WINDOW_NOT_FOUND，不如在
    // 此给出确切原因——面板不是窗口 shell，本就不是合法 target。
    if (request.callerIsPanel) {
        decision.route = TargetRoute::FailPanelCaller;
        decision.error = kErrorPanelCallerUnsupported;
        return decision;
    }

    // 3. caller HWND。
    if (request.hasCallerHwnd) {
        decision.route = TargetRoute::ByCallerHwnd;
        return decision;
    }

    // 4. 两者皆缺：两种意图**一致失败**（Q7-1）。
    //
    // 旧实现在此让 observation 回退主窗口。但对非主窗口的调用方，回退会
    // 返回一个看似合法、实际属于另一个窗口的值，调用方无从分辨——这比
    // 返回错误更有害，故取消回退。
    decision.route = TargetRoute::Fail;
    decision.error = kErrorWindowNotFound;
    return decision;
}

bool AllowsFallbackAfterCallerMiss(TargetIntent /*intent*/) {
    // Q7-1 之后两种意图都不回退主窗口。保留此函数是为了让「不回退」成为
    // 一条被测试固定住的显式契约，而不是靠调用点缺失回退代码来隐式表达。
    return false;
}

}  // namespace window_target_policy
