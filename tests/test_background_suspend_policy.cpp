// test_background_suspend_policy.cpp - BackgroundSuspendPolicy projection, deep-suspend
// and resume contracts
#include "pch.h"
#include <Windows.h>
#include "window/BackgroundSuspendPolicy.h"

TEST(BackgroundSuspendPolicyTest, CoveredWindowKeepsSurfaceForTaskbarPreview) {
    constexpr auto projection = background_suspend_policy::Project(
        background_suspend_policy::kCovered);

    EXPECT_FALSE(projection.hideSurface);
    EXPECT_TRUE(projection.useLowMemory);
}

TEST(BackgroundSuspendPolicyTest, SessionLockHidesSurface) {
    constexpr auto projection = background_suspend_policy::Project(
        background_suspend_policy::kLocked);

    EXPECT_TRUE(projection.hideSurface);
    EXPECT_FALSE(projection.useLowMemory);
    EXPECT_TRUE(projection.deepSuspend);
}

TEST(BackgroundSuspendPolicyTest, UnlockWhileCoveredRestoresSurfaceButStaysLow) {
    constexpr auto combined = background_suspend_policy::Project(
        background_suspend_policy::kLocked | background_suspend_policy::kCovered);
    constexpr auto coveredOnly = background_suspend_policy::Project(
        background_suspend_policy::kCovered);

    EXPECT_TRUE(combined.hideSurface);
    EXPECT_FALSE(coveredOnly.hideSurface);
    EXPECT_TRUE(coveredOnly.useLowMemory);
}

TEST(BackgroundSuspendPolicyTest, ClearingLastReasonRestoresNormalProjection) {
    constexpr auto projection = background_suspend_policy::Project(0);

    EXPECT_FALSE(projection.hideSurface);
    EXPECT_FALSE(projection.useLowMemory);
}

TEST(BackgroundSuspendPolicyTest, AutomationKeepAliveVetoesSessionLockHide) {
    constexpr auto projection = background_suspend_policy::Project(
        background_suspend_policy::kLocked, /*automationKeepAlive=*/true);

    EXPECT_FALSE(projection.hideSurface);
    EXPECT_TRUE(projection.useLowMemory);
}

TEST(BackgroundSuspendPolicyTest, ExplicitNoKeepAliveMatchesLegacyProjection) {
    constexpr auto explicitOff = background_suspend_policy::Project(
        background_suspend_policy::kLocked, /*automationKeepAlive=*/false);
    constexpr auto legacy = background_suspend_policy::Project(
        background_suspend_policy::kLocked);

    EXPECT_EQ(explicitOff.hideSurface, legacy.hideSurface);
    EXPECT_EQ(explicitOff.useLowMemory, legacy.useLowMemory);
    EXPECT_TRUE(legacy.hideSurface);
}

TEST(BackgroundSuspendPolicyTest, MayHidePageTruthTable) {
    static_assert(background_suspend_policy::MayHidePage(false),
                  "no keep-alive must allow hiding");
    static_assert(!background_suspend_policy::MayHidePage(true),
                  "keep-alive must veto hiding");
    EXPECT_TRUE(background_suspend_policy::MayHidePage(false));
    EXPECT_FALSE(background_suspend_policy::MayHidePage(true));
}

TEST(BackgroundSuspendPolicyTest, CoveredNeverHidesRegardlessOfKeepAlive) {
    constexpr auto keepAliveOn = background_suspend_policy::Project(
        background_suspend_policy::kCovered, /*automationKeepAlive=*/true);
    constexpr auto keepAliveOff = background_suspend_policy::Project(
        background_suspend_policy::kCovered, /*automationKeepAlive=*/false);

    EXPECT_FALSE(keepAliveOn.hideSurface);
    EXPECT_FALSE(keepAliveOff.hideSurface);
    EXPECT_TRUE(keepAliveOn.useLowMemory);
    EXPECT_TRUE(keepAliveOff.useLowMemory);
}

// ── P2b deep-suspend projection truth table (DESIGN §6.3) ───────────────────
// New reasons kMinimized/kTrayHidden route the former direct-call paths through
// the projection; deepSuspend gates the TrySuspend upgrade and must stay equal
// to hideSurface (put_IsVisible(FALSE) is its API precondition).

TEST(BackgroundSuspendPolicyTest, MinimizedHidesSurfaceAndDeepSuspends) {
    constexpr auto projection = background_suspend_policy::Project(
        background_suspend_policy::kMinimized);

    EXPECT_TRUE(projection.hideSurface);
    EXPECT_FALSE(projection.useLowMemory);
    EXPECT_TRUE(projection.deepSuspend);
}

TEST(BackgroundSuspendPolicyTest, TrayHiddenHidesSurfaceAndDeepSuspends) {
    constexpr auto projection = background_suspend_policy::Project(
        background_suspend_policy::kTrayHidden);

    EXPECT_TRUE(projection.hideSurface);
    EXPECT_FALSE(projection.useLowMemory);
    EXPECT_TRUE(projection.deepSuspend);
}

TEST(BackgroundSuspendPolicyTest, SessionLockUpgradesToDeepSuspend) {
    constexpr auto projection = background_suspend_policy::Project(
        background_suspend_policy::kLocked);

    EXPECT_TRUE(projection.hideSurface);
    EXPECT_TRUE(projection.deepSuspend);
}

// Covered-only projection is explicitly unchanged by P2b: the surface stays
// (DWM taskbar preview) and the memory tool remains the manual Low target.
TEST(BackgroundSuspendPolicyTest, CoveredOnlyNeverDeepSuspends) {
    constexpr auto projection = background_suspend_policy::Project(
        background_suspend_policy::kCovered);

    EXPECT_FALSE(projection.hideSurface);
    EXPECT_TRUE(projection.useLowMemory);
    EXPECT_FALSE(projection.deepSuspend);
}

TEST(BackgroundSuspendPolicyTest, MinimizedWhileCoveredStillDeepSuspends) {
    constexpr auto projection = background_suspend_policy::Project(
        background_suspend_policy::kMinimized | background_suspend_policy::kCovered);

    EXPECT_TRUE(projection.hideSurface);
    EXPECT_TRUE(projection.deepSuspend);
}

// "covered(Low) → minimized(suspend) → restore while still covered": after the
// minimized reason clears, the covered-only projection must ask for Low again
// (Resume auto-restored Normal; the unified restore path re-projects).
TEST(BackgroundSuspendPolicyTest, RestoreWhileStillCoveredReprojectsLowNotDeep) {
    constexpr auto whileMinimized = background_suspend_policy::Project(
        background_suspend_policy::kMinimized | background_suspend_policy::kCovered);
    constexpr auto afterRestore = background_suspend_policy::Project(
        background_suspend_policy::kCovered);

    EXPECT_TRUE(whileMinimized.deepSuspend);
    EXPECT_FALSE(whileMinimized.useLowMemory);
    EXPECT_FALSE(afterRestore.deepSuspend);
    EXPECT_FALSE(afterRestore.hideSurface);
    EXPECT_TRUE(afterRestore.useLowMemory);
}

TEST(BackgroundSuspendPolicyTest, KeepAliveVetoesDeepSuspendForEveryReasonCombo) {
    constexpr unsigned kAllReasons =
        background_suspend_policy::kLocked | background_suspend_policy::kCovered |
        background_suspend_policy::kMinimized | background_suspend_policy::kTrayHidden;

    for (unsigned reasons = 0; reasons <= kAllReasons; ++reasons) {
        const auto projection = background_suspend_policy::Project(
            reasons, /*automationKeepAlive=*/true);
        EXPECT_FALSE(projection.hideSurface) << "reasons=" << reasons;
        EXPECT_FALSE(projection.deepSuspend) << "reasons=" << reasons;
        EXPECT_EQ(projection.useLowMemory, reasons != 0) << "reasons=" << reasons;
    }
}

// deepSuspend == hideSurface invariant across the full reasons × keep-alive
// space: TrySuspend is only legal on a hidden page, and the keep-alive veto is
// folded into hideSurface, so the two must never diverge.
TEST(BackgroundSuspendPolicyTest, DeepSuspendAlwaysEqualsHideSurface) {
    constexpr unsigned kAllReasons =
        background_suspend_policy::kLocked | background_suspend_policy::kCovered |
        background_suspend_policy::kMinimized | background_suspend_policy::kTrayHidden;

    for (unsigned reasons = 0; reasons <= kAllReasons; ++reasons) {
        for (const bool keepAlive : {false, true}) {
            const auto projection = background_suspend_policy::Project(reasons, keepAlive);
            EXPECT_EQ(projection.deepSuspend, projection.hideSurface)
                << "reasons=" << reasons << " keepAlive=" << keepAlive;
        }
    }
}

TEST(BackgroundSuspendPolicyTest, ClearingAllReasonsClearsDeepSuspend) {
    constexpr auto projection = background_suspend_policy::Project(0);

    EXPECT_FALSE(projection.hideSurface);
    EXPECT_FALSE(projection.useLowMemory);
    EXPECT_FALSE(projection.deepSuspend);
}

TEST(BackgroundSuspendPolicyTest, SuccessfulResumeCommitsVisibleState) {
    constexpr auto result = background_suspend_policy::ApplyVisibilityResult(
        /*pageHidden=*/true, /*desiredHidden=*/false,
        /*commandSucceeded=*/true);

    EXPECT_FALSE(result.pageHidden);
    EXPECT_FALSE(result.retryResume);
}

TEST(BackgroundSuspendPolicyTest, FailedResumeRetainsHiddenStateForRetry) {
    constexpr auto result = background_suspend_policy::ApplyVisibilityResult(
        /*pageHidden=*/true, /*desiredHidden=*/false,
        /*commandSucceeded=*/false);

    EXPECT_TRUE(result.pageHidden);
    EXPECT_TRUE(result.retryResume);
}

TEST(BackgroundSuspendPolicyTest, FailedHideDoesNotInventAppliedHiddenState) {
    constexpr auto result = background_suspend_policy::ApplyVisibilityResult(
        /*pageHidden=*/false, /*desiredHidden=*/true,
        /*commandSucceeded=*/false);

    EXPECT_FALSE(result.pageHidden);
    EXPECT_FALSE(result.retryResume);
}

TEST(BackgroundSuspendPolicyTest, HealthyPageCompletesRestoreWithoutReload) {
    constexpr auto action = background_suspend_policy::DecidePageHealthRecovery(
        /*healthy=*/true, /*reloadAttempts=*/4, /*reloadLimit=*/4);

    EXPECT_EQ(action, background_suspend_policy::PageHealthRecoveryAction::Accept);
}

TEST(BackgroundSuspendPolicyTest, UnhealthyPageReloadsBeforeLimit) {
    constexpr auto action = background_suspend_policy::DecidePageHealthRecovery(
        /*healthy=*/false, /*reloadAttempts=*/3, /*reloadLimit=*/4);

    EXPECT_EQ(action, background_suspend_policy::PageHealthRecoveryAction::Reload);
}

TEST(BackgroundSuspendPolicyTest, UnhealthyPageRebuildsAtLimit) {
    constexpr auto action = background_suspend_policy::DecidePageHealthRecovery(
        /*healthy=*/false, /*reloadAttempts=*/4, /*reloadLimit=*/4);

    EXPECT_EQ(action, background_suspend_policy::PageHealthRecoveryAction::Rebuild);
}
