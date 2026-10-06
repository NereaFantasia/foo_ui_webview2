// DragOverThrottle.cpp
#include "pch.h"
#include "webview/dnd/DragOverThrottle.h"

namespace fb2k_dnd {

bool ShouldForwardDragOver(const DragOverForwardState& state, POINTL point, DWORD keyState,
                           int64_t nowMs) {
    if (!state.hasForwarded || keyState != state.lastKeyState) {
        return true;
    }
    const int64_t elapsed = nowMs - state.lastForwardMs;
    // A clock that went backwards says nothing about the rate; forwarding is the
    // safe side.
    if (elapsed < 0 || elapsed >= kDragOverIdleIntervalMs) {
        return true;
    }
    const bool moved = point.x != state.lastPoint.x || point.y != state.lastPoint.y;
    return moved && elapsed >= kDragOverMoveIntervalMs;
}

void RecordForwardedDragOver(DragOverForwardState& state, POINTL point, DWORD keyState,
                             int64_t nowMs, DWORD answer) {
    state.hasForwarded = true;
    state.lastForwardMs = nowMs;
    state.lastPoint = point;
    state.lastKeyState = keyState;
    state.lastAnswer = answer;
}

}  // namespace fb2k_dnd
