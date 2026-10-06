// test_dnd_drag_over_throttle.cpp - which DragOver calls reach the WebView.
//
// OLE can call DragOver a thousand times a second; passing each one on floods
// the WebView2 processes. The throttle must still pass on anything that can
// change the page's answer at once.
#include "pch.h"
#include "../src/webview/dnd/DragOverThrottle.h"

using fb2k_dnd::DragOverForwardState;
using fb2k_dnd::kDragOverIdleIntervalMs;
using fb2k_dnd::kDragOverMoveIntervalMs;
using fb2k_dnd::RecordForwardedDragOver;
using fb2k_dnd::ShouldForwardDragOver;

namespace {

constexpr POINTL kOrigin{100, 200};
constexpr POINTL kMoved{101, 200};

DragOverForwardState ForwardedAt(int64_t nowMs, DWORD keyState = MK_LBUTTON) {
    DragOverForwardState state;
    RecordForwardedDragOver(state, kOrigin, keyState, nowMs, DROPEFFECT_COPY);
    return state;
}

}  // namespace

TEST(DragOverThrottle, FirstCallIsForwarded) {
    EXPECT_TRUE(ShouldForwardDragOver(DragOverForwardState{}, kOrigin, MK_LBUTTON, 0));
}

TEST(DragOverThrottle, RestingCursorIsNotForwardedWithinTheIdleInterval) {
    const auto state = ForwardedAt(1000);
    EXPECT_FALSE(ShouldForwardDragOver(state, kOrigin, MK_LBUTTON, 1000));
    EXPECT_FALSE(ShouldForwardDragOver(state, kOrigin, MK_LBUTTON, 1000 + kDragOverIdleIntervalMs - 1));
}

TEST(DragOverThrottle, RestingCursorIsForwardedAfterTheIdleInterval) {
    const auto state = ForwardedAt(1000);
    EXPECT_TRUE(ShouldForwardDragOver(state, kOrigin, MK_LBUTTON, 1000 + kDragOverIdleIntervalMs));
}

TEST(DragOverThrottle, MovingCursorIsForwardedAtMostOncePerMoveInterval) {
    const auto state = ForwardedAt(1000);
    EXPECT_FALSE(ShouldForwardDragOver(state, kMoved, MK_LBUTTON, 1000 + kDragOverMoveIntervalMs - 1));
    EXPECT_TRUE(ShouldForwardDragOver(state, kMoved, MK_LBUTTON, 1000 + kDragOverMoveIntervalMs));
}

TEST(DragOverThrottle, KeyStateChangeIsForwardedAtOnce) {
    // Pressing Ctrl or Shift, or a mouse button, can change what the page
    // accepts, so it must not wait for the next interval.
    const auto state = ForwardedAt(1000);
    EXPECT_TRUE(ShouldForwardDragOver(state, kOrigin, MK_LBUTTON | MK_CONTROL, 1000));
}

TEST(DragOverThrottle, ClockGoingBackwardsIsForwarded) {
    const auto state = ForwardedAt(1000);
    EXPECT_TRUE(ShouldForwardDragOver(state, kOrigin, MK_LBUTTON, 999));
}

TEST(DragOverThrottle, RecordKeepsTheWebViewAnswer) {
    DragOverForwardState state;
    RecordForwardedDragOver(state, kMoved, MK_RBUTTON, 42, DROPEFFECT_NONE);
    EXPECT_TRUE(state.hasForwarded);
    EXPECT_EQ(state.lastForwardMs, 42);
    EXPECT_EQ(state.lastPoint.x, kMoved.x);
    EXPECT_EQ(state.lastKeyState, static_cast<DWORD>(MK_RBUTTON));
    EXPECT_EQ(state.lastAnswer, static_cast<DWORD>(DROPEFFECT_NONE));
}

TEST(DragOverThrottle, AThousandCallsASecondBecomeAboutSixty) {
    // The rate measured during a drag from Explorer, with the cursor moving by
    // one pixel on every call.
    DragOverForwardState state;
    int forwarded = 0;
    for (int ms = 0; ms < 1000; ++ms) {
        const POINTL point{ms, 0};
        if (ShouldForwardDragOver(state, point, MK_LBUTTON, ms)) {
            RecordForwardedDragOver(state, point, MK_LBUTTON, ms, DROPEFFECT_COPY);
            ++forwarded;
        }
    }
    EXPECT_LE(forwarded, 1000 / kDragOverMoveIntervalMs + 1);
    EXPECT_GE(forwarded, 1000 / kDragOverMoveIntervalMs - 1);
}
