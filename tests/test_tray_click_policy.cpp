#include "pch.h"
#include "window/TrayClickPolicy.h"

using tray_click::Action;
using tray_click::Input;
using tray_click::Policy;

static Policy NativeTray() {
    Policy policy;
    policy.SetAction(Action::ToggleMainWindow);
    policy.Created();
    return policy;
}

TEST(TrayClickPolicy, SingleClickWaitsForSystemDoubleClickInterval) {
    auto p = NativeTray();
    EXPECT_EQ(p.Handle(Input::Down, 10, 500), 0u);
    EXPECT_EQ(p.Handle(Input::Up, 20, 500), 0u);
    const auto timer = p.Pending();
    EXPECT_NE(timer.id, 0u);
    EXPECT_EQ(p.Expire(timer, 519), 0u);
    EXPECT_EQ(p.Expire(timer, 520), 1u);
    EXPECT_EQ(p.Expire(timer, 700), 0u);
}

TEST(TrayClickPolicy, DoubleClickAndItsFinalUpToggleOnce) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Handle(Input::Up, 20, 500);
    const auto old = p.Pending();
    EXPECT_EQ(p.Handle(Input::DoubleClick, 50, 500), 1u);
    EXPECT_EQ(p.Handle(Input::Up, 60, 500), 0u);
    EXPECT_EQ(p.Expire(old, 800), 0u);
    EXPECT_EQ(p.Pending().id, 0u);
}

TEST(TrayClickPolicy, IndependentClicksDoNotOverwritePendingToggle) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Handle(Input::Up, 20, 500);
    const auto old = p.Pending();
    p.Handle(Input::Down, 40, 500);
    EXPECT_EQ(p.Handle(Input::Up, 50, 500), 1u);
    const auto current = p.Pending();
    EXPECT_NE(old.id, current.id);
    EXPECT_EQ(p.Expire(old, 900), 0u);
    EXPECT_EQ(p.Expire(current, 550), 1u);
}

TEST(TrayClickPolicy, RepeatedSetterRejectsOldTimerAndResidualDoubleClickUp) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Handle(Input::Up, 20, 500);
    const auto old = p.Pending();
    EXPECT_EQ(p.Handle(Input::DoubleClick, 50, 500), 1u);
    p.SetAction(Action::ToggleMainWindow);
    p.SetAction(Action::ToggleMainWindow);
    EXPECT_EQ(p.Handle(Input::Up, 60, 500), 0u);
    EXPECT_EQ(p.Pending().id, 0u);
    p.Handle(Input::Down, 70, 500);
    p.Handle(Input::Up, 80, 500);
    EXPECT_EQ(p.Expire(old, 900), 0u);
    EXPECT_EQ(p.Expire(p.Pending(), 900), 1u);
}

TEST(TrayClickPolicy, RebuildWhileHeldRequiresAFreshCompleteClick) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Rebuilding();
    p.Created();
    EXPECT_EQ(p.GetAction(), Action::ToggleMainWindow);
    EXPECT_EQ(p.Handle(Input::Up, 20, 500), 0u);
    EXPECT_EQ(p.Pending().id, 0u);
    p.Handle(Input::Down, 30, 500);
    p.Handle(Input::Up, 40, 500);
    EXPECT_EQ(p.Expire(p.Pending(), 600), 1u);
}

TEST(TrayClickPolicy, OrphanUpOrDoubleClickHasNoAction) {
    auto p = NativeTray();
    EXPECT_EQ(p.Handle(Input::Up, 10, 500), 0u);
    EXPECT_EQ(p.Handle(Input::DoubleClick, 20, 500), 0u);
    EXPECT_EQ(p.Pending().id, 0u);
}

TEST(TrayClickPolicy, ReleaseDuringFailedRebuildDoesNotConsumeNextFreshClick) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Rebuilding();
    p.Handle(Input::Up, 20, 500);
    p.Created();
    p.Handle(Input::Down, 30, 500);
    p.Handle(Input::Up, 40, 500);
    EXPECT_EQ(p.Expire(p.Pending(), 540), 1u);
}

TEST(TrayClickPolicy, DestroyResetsModeAndInvalidatesPriorTimer) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Handle(Input::Up, 20, 500);
    const auto old = p.Pending();
    p.Destroyed();
    p.Created();
    EXPECT_EQ(p.GetAction(), Action::Event);
    EXPECT_EQ(p.Expire(old, 900), 0u);
    p.SetAction(Action::ToggleMainWindow);
    p.Handle(Input::Down, 50, 500);
    p.Handle(Input::Up, 60, 500);
    EXPECT_EQ(p.Expire(old, 900), 0u);
    EXPECT_EQ(p.Expire(p.Pending(), 900), 1u);
}

TEST(TrayClickPolicy, EventModeTracksHeldButtonAcrossModeChange) {
    Policy p;
    p.Created();
    p.Handle(Input::Down, 10, 500);
    p.SetAction(Action::ToggleMainWindow);
    p.Handle(Input::Up, 20, 500);
    EXPECT_EQ(p.Pending().id, 0u);
    p.Handle(Input::Down, 30, 500);
    p.Handle(Input::Up, 40, 500);
    EXPECT_EQ(p.Expire(p.Pending(), 540), 1u);
}

TEST(TrayClickPolicy, RepeatedSetterCancelsPendingSingleClick) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Handle(Input::Up, 20, 500);
    const auto pending = p.Pending();
    ASSERT_NE(pending.id, 0u);
    p.SetAction(Action::ToggleMainWindow);
    EXPECT_EQ(p.Pending().id, 0u);
    EXPECT_EQ(p.Expire(pending, 900), 0u);
}

TEST(TrayClickPolicy, RebuildCancelsPendingSingleClickAndPreservesMode) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Handle(Input::Up, 20, 500);
    const auto pending = p.Pending();
    p.Rebuilding();
    p.Created();
    EXPECT_EQ(p.GetAction(), Action::ToggleMainWindow);
    EXPECT_EQ(p.Pending().id, 0u);
    EXPECT_EQ(p.Expire(pending, 900), 0u);
}

TEST(TrayClickPolicy, RepeatedCreatePreservesModeAndPendingClick) {
    auto p = NativeTray();
    p.Handle(Input::Down, 10, 500);
    p.Handle(Input::Up, 20, 500);
    const auto pending = p.Pending();
    p.Created();
    EXPECT_EQ(p.GetAction(), Action::ToggleMainWindow);
    EXPECT_EQ(p.Pending().id, pending.id);
    EXPECT_EQ(p.Expire(pending, 520), 1u);
}
