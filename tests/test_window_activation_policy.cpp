#include "pch.h"
#include "window/WindowActivationPolicy.h"

using window_activation::Policy;

TEST(WindowActivationPolicy, ExplicitIntentInvalidatesQueuedUndoBeforeAnySinkCall) {
    Policy p;
    const auto popup = p.Register(10), main = p.Register(20);
    const auto interaction = p.Interact(popup, 100);
    const auto undo = p.CaptureUndo(interaction, main);
    EXPECT_TRUE(p.CanUndo(undo, main));
    const auto request = p.BeginExplicit(main);
    p.Enter(request);
    EXPECT_TRUE(p.IsExplicit(main));
    EXPECT_FALSE(p.IsExplicit(popup));
    unsigned sinkCalls = 0;
    if (p.CanUndo(undo, main)) ++sinkCalls;
    EXPECT_EQ(sinkCalls, 0u);
    p.Leave(request);
    EXPECT_FALSE(p.CanUndo(undo, main));
    EXPECT_FALSE(p.IsRecent(interaction, main, 101, 300));
}

TEST(WindowActivationPolicy, FreshInteractionAfterExplicitFocusStillProtects) {
    Policy p;
    const auto popup = p.Register(10), main = p.Register(20);
    const auto request = p.BeginExplicit(main);
    p.Enter(request);
    p.Leave(request);
    const auto fresh = p.Interact(popup, 200);
    EXPECT_TRUE(p.IsRecent(fresh, main, 250, 300));
    EXPECT_FALSE(p.IsRecent(fresh, main, 501, 300));
    EXPECT_TRUE(p.CanUndo(p.CaptureUndo(fresh, main), main));
}

TEST(WindowActivationPolicy, ReusedHandleCannotReviveOldSourceOrTarget) {
    Policy p;
    const auto popup = p.Register(10), main = p.Register(20);
    const auto undo = p.CaptureUndo(p.Interact(popup, 100), main);
    p.Forget(main);
    const auto replacement = p.Register(20);
    EXPECT_NE(main, replacement);
    EXPECT_FALSE(p.CanUndo(undo, replacement));
    p.Forget(popup);
    p.Register(10);
    EXPECT_FALSE(p.CanContinue(undo));
    p.Forget(main);
    EXPECT_TRUE(p.IsLive(replacement));
}

TEST(WindowActivationPolicy, DuplicateRegistrationDoesNotResetLiveInstance) {
    Policy p;
    const auto first = p.Register(10);
    EXPECT_NE(first.instance, 0u);
    EXPECT_EQ(first, p.Register(10));
    EXPECT_EQ(first, p.Find(10));
}

TEST(WindowActivationPolicy, ExplicitPopupCannotCreateSynchronousUndo) {
    Policy p;
    const auto popup = p.Register(10), main = p.Register(20);
    const auto interaction = p.Interact(popup, 100);
    const auto request = p.BeginExplicit(popup);
    p.Enter(request);
    EXPECT_TRUE(p.IsExplicit(popup));
    EXPECT_FALSE(p.IsExplicit(main));
    EXPECT_FALSE(p.CanUndo(p.CaptureUndo(interaction, popup), popup));
    p.Leave(request);
    const auto fresh = p.Interact(popup, 110);
    EXPECT_TRUE(p.CanUndo(p.CaptureUndo(fresh, popup), popup));
}

TEST(WindowActivationPolicy, ForegroundMismatchAndNewInteractionRejectOldUndo) {
    Policy p;
    const auto popup = p.Register(10), main = p.Register(20), other = p.Register(30);
    const auto undo = p.CaptureUndo(p.Interact(popup, 100), main);
    EXPECT_FALSE(p.CanUndo(undo, other));
    EXPECT_TRUE(p.CanUndo(undo, main));
    p.Interact(other, 101);
    EXPECT_FALSE(p.CanUndo(undo, main));
}

TEST(WindowActivationPolicy, ExplicitRequestDuringUndoStopsRemainingNativeSteps) {
    Policy p;
    const auto popup = p.Register(10), main = p.Register(20);
    const auto undo = p.CaptureUndo(p.Interact(popup, 100), main);
    unsigned sink = 0, zOrder = 0, restore = 0;
    if (p.CanUndo(undo, main)) {
        ++sink;
        p.BeginExplicit(main);
        if (p.CanContinue(undo)) ++zOrder;
        if (p.CanContinue(undo)) ++restore;
    }
    EXPECT_EQ(sink, 1u);
    EXPECT_EQ(zOrder, 0u);
    EXPECT_EQ(restore, 0u);
}

TEST(WindowActivationPolicy, MenuReturnIsCancelledByLaterExplicitIntent) {
    Policy p;
    const auto menu = p.Register(10), main = p.Register(20);
    const auto ticket = p.CaptureReturn(menu);
    EXPECT_TRUE(p.CanReturn(ticket));
    p.BeginExplicit(main);
    EXPECT_FALSE(p.CanReturn(ticket));
    const auto next = p.CaptureReturn(menu);
    EXPECT_TRUE(p.CanReturn(next));
    p.Forget(menu);
    p.Register(10);
    EXPECT_FALSE(p.CanReturn(next));
}

TEST(WindowActivationPolicy, OldMenuSessionCannotRunReadyOrMoveFocusAfterReplacement) {
    Policy p;
    const auto menu = p.Register(10), target = p.Register(20);
    const auto old = p.CaptureReturn(menu);
    const auto replacement = p.CaptureReturn(menu);
    EXPECT_FALSE(p.CanReturn(old));
    EXPECT_TRUE(p.CanReturn(replacement));
    unsigned foregroundCalls = 0, moveFocusCalls = 0;
    if (p.CanReturn(replacement)) {
        ++foregroundCalls;
        p.BeginExplicit(target);
        if (p.CanReturn(replacement)) ++moveFocusCalls;
    }
    EXPECT_EQ(foregroundCalls, 1u);
    EXPECT_EQ(moveFocusCalls, 0u);
}

TEST(WindowActivationPolicy, InvalidExplicitTargetDoesNotExpireAnExistingMenuReturn) {
    Policy p;
    const auto menu = p.Register(10);
    const auto ticket = p.CaptureReturn(menu);
    p.BeginExplicit({999, 1});
    EXPECT_TRUE(p.CanReturn(ticket));
}

TEST(WindowActivationPolicy, NestedScopeRestoresOuterTargetWithoutRestoringOldEpoch) {
    Policy p;
    const auto main = p.Register(20), popup = p.Register(10);
    const auto outer = p.BeginExplicit(main);
    window_activation::Scope first(p, outer);
    {
        window_activation::Scope second(p, p.BeginExplicit(popup));
        EXPECT_TRUE(p.IsExplicit(popup));
        EXPECT_FALSE(p.IsExplicit(main));
    }
    EXPECT_TRUE(p.IsExplicit(main));
    EXPECT_FALSE(p.IsCurrent(outer));
}

TEST(WindowActivationPolicy, ChangedForegroundStopsUndoEvenWithoutANewExplicitRequest) {
    Policy p;
    const auto popup = p.Register(10), main = p.Register(20), sink = p.Register(30), other = p.Register(40);
    const auto undo = p.CaptureUndo(p.Interact(popup, 100), main);
    EXPECT_TRUE(p.CanContinue(undo, main, sink));
    EXPECT_TRUE(p.CanContinue(undo, sink, sink));
    EXPECT_FALSE(p.CanContinue(undo, other, sink));
}

TEST(WindowActivationPolicy, ShutdownInvalidatesSurvivingHostAndDoesNotReuseInstanceNumbers) {
    Policy p;
    const auto host = p.Register(20);
    const auto request = p.BeginExplicit(host);
    p.Enter(request);
    p.Clear();
    EXPECT_FALSE(p.IsLive(host));
    EXPECT_FALSE(p.IsCurrent(request));
    EXPECT_FALSE(p.IsExplicit(host));
    EXPECT_NE(p.Register(20), host);
}

TEST(WindowActivationPolicy, QueuedExplicitCommandMustStillOwnCurrentIntentAndInstance) {
    Policy p;
    const auto main = p.Register(20), popup = p.Register(10);
    const auto old = p.BeginExplicit(main);
    EXPECT_TRUE(p.IsCurrent(old));
    p.BeginExplicit(popup);
    EXPECT_FALSE(p.IsCurrent(old));
    const auto current = p.BeginExplicit(main);
    p.Forget(main);
    p.Register(20);
    EXPECT_FALSE(p.IsCurrent(current));
}
