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

    // 2. caller HWND。
    if (request.hasCallerHwnd) {
        decision.route = TargetRoute::ByCallerHwnd;
        return decision;
    }

    // 3. 两者皆缺：mutation 失败，observation 回退主窗口。
    if (intent == TargetIntent::Observation) {
        decision.route = TargetRoute::FallbackMain;
        return decision;
    }

    decision.route = TargetRoute::Fail;
    decision.error = kErrorWindowNotFound;
    return decision;
}

bool AllowsFallbackAfterCallerMiss(TargetIntent intent) {
    return intent == TargetIntent::Observation;
}

}  // namespace window_target_policy
