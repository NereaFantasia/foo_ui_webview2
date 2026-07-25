#include "pch.h"
#include <Windows.h>
#include "window/BackgroundSuspendPolicy.h"
#include "window/TaskbarTrayContracts.h"
#include "window/TrayIcon.h"
#include "window/MenuTokenTable.h"
#include "window/MenuOverlayGeometry.h"
#include "window/MenuResourceLimits.h"

using json = nlohmann::json;

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

TEST(TaskbarTrayContractsTest, UserButtonsCanReplaceImplicitDefaultsBeforeCustomAdd) {
    EXPECT_TRUE(taskbar_tray_contracts::CanInstallUserThumbnailButtons(
        true, true, 3));
}

// Regression: a repeat setThumbnailButtons call after a custom toolbar was
// already installed (usingDefaultButtons=false, buttonsAdded=true) must remain
// serviceable (SetThumbnailButtons routes it through ThumbBarUpdateButtons).
// The previous `return !buttonsAdded` contract wrongly rejected this, so the
// frontend could only set thumbnail buttons once.
TEST(TaskbarTrayContractsTest, UserButtonsCanBeUpdatedAfterCustomInstall) {
    EXPECT_TRUE(taskbar_tray_contracts::CanInstallUserThumbnailButtons(
        false, true, 3));
}

// A fresh custom install before any add (neither default nor added yet) is also
// serviceable within the slot cap.
TEST(TaskbarTrayContractsTest, UserButtonsCanBeInstalledFresh) {
    EXPECT_TRUE(taskbar_tray_contracts::CanInstallUserThumbnailButtons(
        false, false, 3));
}

TEST(TaskbarTrayContractsTest, DefaultButtonsReserveAllThumbnailSlots) {
    EXPECT_EQ(taskbar_tray_contracts::ThumbnailToolbarSlotCount(), 7u);
}

TEST(TaskbarTrayContractsTest, UserButtonsRespectWindowsSevenButtonLimit) {
    EXPECT_FALSE(taskbar_tray_contracts::CanInstallUserThumbnailButtons(
        false, false, 8));
}

TEST(TaskbarTrayContractsTest, TrayShellHostUsesDedicatedMessageWindow) {
    EXPECT_TRUE(taskbar_tray_contracts::ShouldUseDedicatedTrayMessageWindow());
}

// Rich tray item kinds are recognised; plain kinds and separators are not.
TEST(TaskbarTrayContractsTest, RichTrayItemTypesAreRecognised) {
    EXPECT_TRUE(taskbar_tray_contracts::IsRichTrayItemType("nowplaying"));
    EXPECT_TRUE(taskbar_tray_contracts::IsRichTrayItemType("rating"));
    EXPECT_TRUE(taskbar_tray_contracts::IsRichTrayItemType("slider"));
    EXPECT_TRUE(taskbar_tray_contracts::IsRichTrayItemType("segmented"));
    EXPECT_FALSE(taskbar_tray_contracts::IsRichTrayItemType("normal"));
    EXPECT_FALSE(taskbar_tray_contracts::IsRichTrayItemType("checkbox"));
    EXPECT_FALSE(taskbar_tray_contracts::IsRichTrayItemType("submenu"));
    EXPECT_FALSE(taskbar_tray_contracts::IsRichTrayItemType("separator"));
}

// Inline SVG menu icons (webview branch) are renderable only when BOTH a
// viewBox and the SVG inner markup are present; the native branch ignores them.
TEST(TaskbarTrayContractsTest, InlineIconSvgNeedsBothViewBoxAndContent) {
    EXPECT_TRUE (taskbar_tray_contracts::TrayItemHasRenderableIconSvg("0 0 24 24", "<path d=\"M0 0h24v24H0z\"/>"));
    EXPECT_FALSE(taskbar_tray_contracts::TrayItemHasRenderableIconSvg("", "<path/>"));
    EXPECT_FALSE(taskbar_tray_contracts::TrayItemHasRenderableIconSvg("0 0 24 24", ""));
    EXPECT_FALSE(taskbar_tray_contracts::TrayItemHasRenderableIconSvg("", ""));
}

// Now-playing auto-fill is frontend-first / backend-fallback: the backend fills a
// field only when autoNowPlaying is on AND the frontend left it empty.
TEST(TaskbarTrayContractsTest, NowPlayingAutoFillIsFrontendFirstBackendFallback) {
    EXPECT_TRUE (taskbar_tray_contracts::TrayShouldAutoFillField(true,  false));  // auto on + empty -> fill
    EXPECT_FALSE(taskbar_tray_contracts::TrayShouldAutoFillField(true,  true));   // frontend value wins
    EXPECT_FALSE(taskbar_tray_contracts::TrayShouldAutoFillField(false, false));  // auto off -> never
    EXPECT_FALSE(taskbar_tray_contracts::TrayShouldAutoFillField(false, true));
}

// Inline icons attach to ordinary entries only (normal/submenu), never to
// separators or the rich kinds (which have their own layouts). Mirrors the
// renderer's icon-column gate.
TEST(TaskbarTrayContractsTest, OnlyOrdinaryItemKindsRenderInlineIcons) {
    EXPECT_TRUE (taskbar_tray_contracts::TrayItemKindRendersIcon("normal"));
    EXPECT_TRUE (taskbar_tray_contracts::TrayItemKindRendersIcon("submenu"));
    EXPECT_TRUE (taskbar_tray_contracts::TrayItemKindRendersIcon(""));  // empty defaults to an ordinary item
    EXPECT_FALSE(taskbar_tray_contracts::TrayItemKindRendersIcon("separator"));
    EXPECT_FALSE(taskbar_tray_contracts::TrayItemKindRendersIcon("nowplaying"));
    EXPECT_FALSE(taskbar_tray_contracts::TrayItemKindRendersIcon("rating"));
    EXPECT_FALSE(taskbar_tray_contracts::TrayItemKindRendersIcon("slider"));
    EXPECT_FALSE(taskbar_tray_contracts::TrayItemKindRendersIcon("segmented"));
}

// rating/slider/segmented are keep-open value controls: a value change reports
// {id,value} through the value channel WITHOUT closing the menu. nowplaying and
// ordinary items select and close like a normal click. segmented was reported
// through the value channel by the renderer all along, but this shared predicate
// used to omit it (contract drift, DESIGN 2.6 / 10.6): any consumer reading the
// predicate would wrongly treat a segmented pick as a closing click.
TEST(TaskbarTrayContractsTest, OnlyValueControlsKeepMenuOpen) {
    EXPECT_TRUE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("rating"));
    EXPECT_TRUE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("slider"));
    EXPECT_TRUE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("segmented"));
    EXPECT_FALSE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("nowplaying"));
    EXPECT_FALSE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("normal"));
    EXPECT_FALSE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("separator"));
}

// Native degrade fans rating into a 5-level submenu and slider into 5 stops.
TEST(TaskbarTrayContractsTest, NativeDegradeUsesFiveStepsForRichControls) {
    EXPECT_EQ(taskbar_tray_contracts::RatingNativeLevelCount(), 5);
    EXPECT_EQ(taskbar_tray_contracts::SliderNativeStopCount(), 5);
}

// tray:menuItemClicked value integrality probe (rating 0-5 and rounded slider
// volume are integral; fractional values are reported verbatim).
TEST(TaskbarTrayContractsTest, TrayMenuValueIntegralityProbe) {
    EXPECT_TRUE(taskbar_tray_contracts::TrayMenuValueIsIntegral(0.0));
    EXPECT_TRUE(taskbar_tray_contracts::TrayMenuValueIsIntegral(5.0));
    EXPECT_TRUE(taskbar_tray_contracts::TrayMenuValueIsIntegral(100.0));
    EXPECT_FALSE(taskbar_tray_contracts::TrayMenuValueIsIntegral(2.5));
}

namespace {
TrayMenuItem MkTrayItem(std::string id, bool visible = true) {
    TrayMenuItem m;
    m.id = std::move(id);
    m.type = "normal";
    m.visible = visible;
    return m;
}
int CountSeparators(const std::vector<TrayMenuItem>& v) {
    int n = 0;
    for (const auto& m : v) if (m.type == "separator") ++n;
    return n;
}
bool StartsOrEndsWithSeparator(const std::vector<TrayMenuItem>& v) {
    if (v.empty()) return false;
    return v.front().type == "separator" || v.back().type == "separator";
}
}  // namespace

// Three zones that all render content get separators only *between* them: no
// leading / trailing / doubled divider.
TEST(TaskbarTrayContractsTest, ComposeZonesSeparatesVisibleZones) {
    auto out = ComposeTrayZones({MkTrayItem("a")}, {MkTrayItem("b")}, {MkTrayItem("c")});
    EXPECT_EQ(CountSeparators(out), 2);
    EXPECT_FALSE(StartsOrEndsWithSeparator(out));
}

// A zone whose items are all hidden must not emit a separator (leading, middle
// or trailing). Regression: appendZone gated on emptiness, but the downstream
// visible filter drops the hidden items and left an orphan divider (DESIGN 7.7).
TEST(TaskbarTrayContractsTest, ComposeZonesHiddenZoneProducesNoSeparator) {
    // Hidden top zone -> no leading separator before playback.
    auto topHidden = ComposeTrayZones({MkTrayItem("a", false)}, {MkTrayItem("b")}, {MkTrayItem("c")});
    EXPECT_EQ(CountSeparators(topHidden), 1);
    EXPECT_FALSE(StartsOrEndsWithSeparator(topHidden));

    // Hidden middle zone -> only one separator (top|bottom), none around playback.
    auto midHidden = ComposeTrayZones({MkTrayItem("a")}, {MkTrayItem("b", false)}, {MkTrayItem("c")});
    EXPECT_EQ(CountSeparators(midHidden), 1);
    EXPECT_FALSE(StartsOrEndsWithSeparator(midHidden));

    // Hidden bottom zone -> no trailing separator.
    auto botHidden = ComposeTrayZones({MkTrayItem("a")}, {MkTrayItem("b")}, {MkTrayItem("c", false)});
    EXPECT_EQ(CountSeparators(botHidden), 1);
    EXPECT_FALSE(StartsOrEndsWithSeparator(botHidden));
}

// A zone mixing hidden + visible items still contributes its separator (it has
// visible content); the hidden items are dropped downstream, not here.
TEST(TaskbarTrayContractsTest, ComposeZonesMixedZoneKeepsSeparator) {
    auto out = ComposeTrayZones({MkTrayItem("a")}, {MkTrayItem("b0", false), MkTrayItem("b1")}, {});
    EXPECT_EQ(CountSeparators(out), 1);
    EXPECT_FALSE(StartsOrEndsWithSeparator(out));
}

// A single contributing zone (or none) yields no separators.
TEST(TaskbarTrayContractsTest, ComposeZonesSingleOrEmptyHasNoSeparator) {
    EXPECT_EQ(CountSeparators(ComposeTrayZones({MkTrayItem("a")}, {}, {})), 0);
    EXPECT_EQ(CountSeparators(ComposeTrayZones({}, {MkTrayItem("b")}, {})), 0);
    EXPECT_TRUE(ComposeTrayZones({}, {}, {}).empty());
    // All-hidden everywhere -> empty composition, no separators.
    EXPECT_TRUE(ComposeTrayZones({MkTrayItem("a", false)}, {MkTrayItem("b", false)}, {}).empty());
}

// 鈹€鈹€ Origin-based routing + opaque-token contract (DESIGN 7.2 / 8.3) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

// Built-in playback defaults carry a trusted origin + action, stamped by the
// builder, NOT derived from the "_pb_" id prefix.
TEST(TaskbarTrayContractsTest, PlaybackDefaultsCarryBuiltinOrigin) {
    auto items = BuildPlaybackDefaultItems();
    ASSERT_EQ(items.size(), 4u);
    for (const auto& m : items)
        EXPECT_EQ(m.origin, menu_action::Origin::BuiltinPlayback);
    EXPECT_EQ(items[0].builtinAction, menu_action::Builtin::PlayPause);
    EXPECT_EQ(items[1].builtinAction, menu_action::Builtin::Previous);
    EXPECT_EQ(items[2].builtinAction, menu_action::Builtin::Next);
    EXPECT_EQ(items[3].builtinAction, menu_action::Builtin::Stop);
}

TEST(TaskbarTrayContractsTest, SystemDefaultsCarryBuiltinExitOrigin) {
    auto items = BuildSystemDefaultItems();
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].origin, menu_action::Origin::BuiltinSystem);
    EXPECT_EQ(items[0].builtinAction, menu_action::Builtin::Exit);
}

// Exact historical ids map to fixed built-in actions. Prefix lookalikes and
// ordinary ids never gain a built-in route.
TEST(TaskbarTrayContractsTest, ReservedBuiltinAllowlistIsExact) {
    using namespace menu_action;
    auto exit = ReservedBuiltinForPublicId("_sys_exit");
    ASSERT_TRUE(exit.has_value());
    EXPECT_EQ(exit->origin, Origin::BuiltinSystem);
    EXPECT_EQ(exit->builtin, Builtin::Exit);
    EXPECT_EQ(DecideRoute(*exit), RouteDecision::ExecuteSystem);

    EXPECT_FALSE(ReservedBuiltinForPublicId("_sys_exit_backup").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_SYS_EXIT").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_sys_").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_pb_playPause").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_pb_prev").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_pb_next").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_pb_stop").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_pb_next_extra").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("ordinary").has_value());
}

TEST(TaskbarTrayContractsTest, EffectiveZonesPromoteOnlyExitAndKeepPlaybackUserItem) {
    TrayMenuItem explicitExit = MkTrayItem("_sys_exit");
    explicitExit.label = "退出 foobar2000";
    TrayMenuItem explicitNext = MkTrayItem("_pb_next");
    explicitNext.label = "Next (custom label)";
    TrayMenuItem lookalike = MkTrayItem("_sys_exit_backup");

    TrayMenuConfig config;
    config.showPlaybackControls = true;
    config.showSystemItems = true;
    auto zones = BuildEffectiveTrayZones(
        { explicitExit, lookalike }, { explicitNext }, {}, config);

    EXPECT_EQ(std::ranges::count_if(zones.top, [](const TrayMenuItem& item) {
        return item.id == "_sys_exit";
    }), 1);
    EXPECT_FALSE(TrayMenuContainsPublicId(zones.bottom, "_sys_exit"));
    EXPECT_EQ(zones.top[0].origin, menu_action::Origin::BuiltinSystem);
    EXPECT_EQ(zones.top[0].builtinAction, menu_action::Builtin::Exit);
    EXPECT_EQ(zones.top[0].label, "退出 foobar2000");

    EXPECT_EQ(std::ranges::count_if(zones.playback, [](const TrayMenuItem& item) {
        return item.id == "_pb_next";
    }), 2);
    auto userNext = std::ranges::find_if(zones.playback, [](const TrayMenuItem& item) {
        return item.id == "_pb_next" && item.origin == menu_action::Origin::User;
    });
    ASSERT_NE(userNext, zones.playback.end());
    EXPECT_EQ(userNext->builtinAction, menu_action::Builtin::None);
    EXPECT_EQ(userNext->label, "Next (custom label)");
    auto builtinNext = std::ranges::find_if(zones.playback, [](const TrayMenuItem& item) {
        return item.id == "_pb_next" && item.origin == menu_action::Origin::BuiltinPlayback;
    });
    ASSERT_NE(builtinNext, zones.playback.end());
    EXPECT_EQ(builtinNext->builtinAction, menu_action::Builtin::Next);

    ASSERT_EQ(zones.top.size(), 2u);
    EXPECT_EQ(zones.top[1].origin, menu_action::Origin::User);
    EXPECT_EQ(zones.top[1].builtinAction, menu_action::Builtin::None);
    EXPECT_EQ(menu_action::DecideRoute({ zones.top[1].id, zones.top[1].origin,
                                         zones.top[1].builtinAction }),
              menu_action::RouteDecision::FireUserCallback);
}

// DecideRoute keys off origin only (native + webview share it, so a selection
// resolves identically on both backends).
TEST(TaskbarTrayContractsTest, DecideRouteKeysOffOrigin) {
    using namespace menu_action;
    EXPECT_EQ(DecideRoute({ "x", Origin::User, Builtin::None }),
              RouteDecision::FireUserCallback);
    EXPECT_EQ(DecideRoute({ "_pb_next", Origin::BuiltinPlayback, Builtin::Next }),
              RouteDecision::ExecutePlayback);
    EXPECT_EQ(DecideRoute({ "_sys_exit", Origin::BuiltinSystem, Builtin::Exit }),
              RouteDecision::ExecuteSystem);
}

// Origin / Builtin string transport round-trips; unknown/absent -> user/none, so
// the overlay never trusts a forged _origin as a built-in.
TEST(TaskbarTrayContractsTest, OriginBuiltinStringTransport) {
    using namespace menu_action;
    EXPECT_EQ(OriginFromString(OriginToString(Origin::BuiltinPlayback)), Origin::BuiltinPlayback);
    EXPECT_EQ(OriginFromString(OriginToString(Origin::BuiltinSystem)), Origin::BuiltinSystem);
    EXPECT_EQ(OriginFromString(OriginToString(Origin::User)), Origin::User);
    EXPECT_EQ(OriginFromString("bogus"), Origin::User);
    EXPECT_EQ(BuiltinFromString(BuiltinToString(Builtin::Next)), Builtin::Next);
    EXPECT_EQ(BuiltinFromString("bogus"), Builtin::None);
}

// Select eligibility (token resolution): only an enabled, non-submenu, non-value
// row selects; disabled rows, submenu parents and value controls do not.
TEST(TaskbarTrayContractsTest, CanSelectLeafGate) {
    using menu_action::CanSelectLeaf;
    EXPECT_TRUE (CanSelectLeaf("normal", true, false));
    EXPECT_TRUE (CanSelectLeaf("nowplaying", true, false));
    EXPECT_FALSE(CanSelectLeaf("normal", false, false));  // disabled
    EXPECT_FALSE(CanSelectLeaf("normal", true, true));    // submenu parent
    EXPECT_FALSE(CanSelectLeaf("rating", true, false));   // value control
    EXPECT_FALSE(CanSelectLeaf("slider", true, false));
    EXPECT_FALSE(CanSelectLeaf("segmented", true, false));
}

// Value validation by kind: rating 0..5, slider [min,max], segmented an existing
// AND enabled index; wrong kind / out-of-range / disabled segment are rejected.
TEST(TaskbarTrayContractsTest, RichValueRangeGate) {
    using menu_action::RichValueInRange;
    std::vector<char> segs{1, 0, 1};   // index 1 disabled
    EXPECT_TRUE (RichValueInRange("rating", 0, 0, 0, {}));
    EXPECT_TRUE (RichValueInRange("rating", 5, 0, 0, {}));
    EXPECT_FALSE(RichValueInRange("rating", 6, 0, 0, {}));
    EXPECT_FALSE(RichValueInRange("rating", -1, 0, 0, {}));
    EXPECT_TRUE (RichValueInRange("slider", 50, 0, 100, {}));
    EXPECT_FALSE(RichValueInRange("slider", 101, 0, 100, {}));
    EXPECT_FALSE(RichValueInRange("slider", -1, 0, 100, {}));
    EXPECT_TRUE (RichValueInRange("segmented", 0, 0, 0, segs));
    EXPECT_TRUE (RichValueInRange("segmented", 2, 0, 0, segs));
    EXPECT_FALSE(RichValueInRange("segmented", 1, 0, 0, segs));  // disabled segment
    EXPECT_FALSE(RichValueInRange("segmented", 3, 0, 0, segs));  // out of range
    EXPECT_FALSE(RichValueInRange("normal", 1, 0, 100, {}));     // not a value control
}

// Internal IPC caller gate: only the overlay's own window is accepted; a null
// overlay (menu not shown) rejects everything, and a different window is rejected.
TEST(TaskbarTrayContractsTest, OverlayCallerGate) {
    int overlayWindow = 0;   // stand-in HWNDs (address identity is all that matters)
    int otherWindow = 0;
    EXPECT_TRUE (menu_action::IsSameOverlayWindow(&overlayWindow, &overlayWindow));
    EXPECT_FALSE(menu_action::IsSameOverlayWindow(&otherWindow, &overlayWindow));  // wrong caller
    EXPECT_FALSE(menu_action::IsSameOverlayWindow(&overlayWindow, nullptr));       // no overlay
    EXPECT_FALSE(menu_action::IsSameOverlayWindow(nullptr, nullptr));
}

// Internal IPC menuId gate: exact match against the current visible menu only;
// a mismatch, an empty current id, or a hidden menu is rejected.
TEST(TaskbarTrayContractsTest, MenuIdGate) {
    using menu_action::IsCurrentMenuId;
    EXPECT_TRUE (IsCurrentMenuId("menu-3", "menu-3", true));
    EXPECT_FALSE(IsCurrentMenuId("menu-2", "menu-3", true));   // wrong / stale menuId
    EXPECT_FALSE(IsCurrentMenuId("menu-3", "menu-3", false));  // not visible
    EXPECT_FALSE(IsCurrentMenuId("", "", true));               // no current menu
    EXPECT_FALSE(IsCurrentMenuId("menu-3", "", true));
}

// 鈹€鈹€ Opaque token state machine (production MenuTokenTable; DESIGN 8.3) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€

namespace {
// A mixed menu: user leaf, built-in playback item (routes by origin, not the
// "_pb_" prefix), a value control, a separator (no token) and a disabled leaf.
json MakeMenuItemsJson() {
    json items = json::array();
    { json it; it["id"] = "play";     it["label"] = "Play";     it["enabled"] = true;  items.push_back(it); }
    { json it; it["id"] = "_pb_next";  it["label"] = "Next";     it["_origin"] = "builtin-playback"; it["_builtinAction"] = "next"; items.push_back(it); }
    { json it; it["type"] = "slider";  it["id"] = "vol";  it["min"] = 0; it["max"] = 100; it["value"] = 50; items.push_back(it); }
    { json it; it["type"] = "separator"; items.push_back(it); }
    { json it; it["id"] = "dis";      it["label"] = "Disabled"; it["enabled"] = false; items.push_back(it); }
    return items;
}
// Deterministic sequential token generator ("t0", "t1", ...).
MenuTokenTable::TokenGen SeqGen(std::shared_ptr<int> n) {
    return [n]() -> std::optional<std::string> { return "t" + std::to_string((*n)++); };
}
}  // namespace

// The tray owner path explicitly opts into its own C++-stamped action fields.
// Rebuild assigns a token per actionable node (separators skipped), strips the
// internal routing fields, and resolves the complete trusted pair by origin.
TEST(TaskbarTrayContractsTest, TokenTableOwnerModeExplicitlyTrustsActionFields) {
    json items = MakeMenuItemsJson();
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, SeqGen(std::make_shared<int>(0)), true));
    EXPECT_EQ(table.Size(), 4u);   // play, _pb_next, slider, disabled (separator has no token)

    const std::string playTok = items[0]["_token"];
    auto sel = table.ResolveSelect(playTok);
    ASSERT_TRUE(sel.has_value());
    EXPECT_EQ(sel->publicId, "play");
    EXPECT_EQ(sel->origin, menu_action::Origin::User);

    const std::string nextTok = items[1]["_token"];
    auto builtin = table.ResolveSelect(nextTok);
    ASSERT_TRUE(builtin.has_value());
    EXPECT_EQ(builtin->origin, menu_action::Origin::BuiltinPlayback);
    EXPECT_EQ(builtin->builtin, menu_action::Builtin::Next);
    // internal routing fields never reach the renderer json
    EXPECT_FALSE(items[1].contains("_origin"));
    EXPECT_FALSE(items[1].contains("_builtinAction"));
    EXPECT_TRUE (items[1].contains("_token"));
}

TEST(TaskbarTrayContractsTest, TokenTableDoesNotPromoteUnstampedReservedId) {
    json items = json::array({
        {{"id", "_sys_exit"}, {"label", "Exit-looking user action"}, {"enabled", true}}
    });
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, SeqGen(std::make_shared<int>(0))));
    auto action = table.ResolveSelect(items[0]["_token"].get<std::string>());
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(action->origin, menu_action::Origin::User);
    EXPECT_EQ(action->builtin, menu_action::Builtin::None);
    EXPECT_EQ(menu_action::DecideRoute(*action), menu_action::RouteDecision::FireUserCallback);
}

// The exact 1.9.0 tray compatibility id must survive the complete effective
// composition -> native map / WebView token transport route. This does not
// merely test DecideRoute: the same production helpers that feed both backends
// carry the stamp, while lookalikes, caller `_pb_*`, and normal user items do
// not receive it.
TEST(TaskbarTrayContractsTest, ExplicitExitPreservesTrustedRouteAcrossBackends) {
    TrayMenuConfig config;
    config.showPlaybackControls = false;
    config.showSystemItems = false;

    TrayMenuItem exit = MkTrayItem("_sys_exit");
    exit.label = "退出 foobar2000";
    TrayMenuItem lookalike = MkTrayItem("_sys_exit_backup");
    TrayMenuItem caseVariant = MkTrayItem("_SYS_EXIT");
    TrayMenuItem playbackLike = MkTrayItem("_pb_next");
    TrayMenuItem ordinary = MkTrayItem("open-settings");
    auto zones = BuildEffectiveTrayZones(
        {exit, lookalike, caseVariant, playbackLike, ordinary}, {}, {}, config);
    ASSERT_EQ(zones.top.size(), 5u);

    const TrayMenuItem& composedExit = zones.top[0];
    EXPECT_EQ(composedExit.origin, menu_action::Origin::BuiltinSystem);
    EXPECT_EQ(composedExit.builtinAction, menu_action::Builtin::Exit);

    // Native BuildMenu stores this exact helper result in m_menuIdMap.
    const auto nativeExit = ResolveTrayMenuItemAction(composedExit);
    EXPECT_EQ(menu_action::DecideRoute(nativeExit),
              menu_action::RouteDecision::ExecuteSystem);

    // WebView TrayItemToMenuJson uses the same fields before MenuTokenTable
    // strips them from renderer-visible JSON and resolves the opaque token.
    const auto fields = TrayMenuItemActionFields(composedExit);
    ASSERT_TRUE(fields.IsStamped());
    EXPECT_EQ(*fields.origin, "builtin-system");
    EXPECT_EQ(*fields.builtin, "exit");
    json webItems = json::array({{
        {"id", composedExit.id}, {"label", composedExit.label},
        {"_origin", *fields.origin}, {"_builtinAction", *fields.builtin}
    }});
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(webItems, SeqGen(std::make_shared<int>(0)), true));
    EXPECT_FALSE(webItems[0].contains("_origin"));
    EXPECT_FALSE(webItems[0].contains("_builtinAction"));
    const auto webExit = table.ResolveSelect(webItems[0]["_token"].get<std::string>());
    ASSERT_TRUE(webExit.has_value());
    EXPECT_EQ(menu_action::DecideRoute(*webExit),
              menu_action::RouteDecision::ExecuteSystem);
    EXPECT_EQ(webExit->builtin, menu_action::Builtin::Exit);

    for (size_t i = 1; i < zones.top.size(); ++i) {
        const auto userAction = ResolveTrayMenuItemAction(zones.top[i]);
        EXPECT_EQ(userAction.origin, menu_action::Origin::User);
        EXPECT_EQ(userAction.builtin, menu_action::Builtin::None);
        EXPECT_EQ(menu_action::DecideRoute(userAction),
                  menu_action::RouteDecision::FireUserCallback);
        EXPECT_FALSE(TrayMenuItemActionFields(zones.top[i]).IsStamped());
    }
}

TEST(TaskbarTrayContractsTest, TokenTableDefaultsToUntrustedActionFields) {
    // Callers that omit the policy, including public menu.show, must fail closed:
    // even a complete valid pair remains a user action.
    json items = json::array({{
        {"id", "_sys_exit"}, {"label", "Forged exit"},
        {"_origin", "builtin-system"}, {"_builtinAction", "exit"}
    }});
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, SeqGen(std::make_shared<int>(0))));
    const auto action = table.ResolveSelect(items[0]["_token"].get<std::string>());
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(action->origin, menu_action::Origin::User);
    EXPECT_EQ(action->builtin, menu_action::Builtin::None);
    EXPECT_EQ(menu_action::DecideRoute(*action),
              menu_action::RouteDecision::FireUserCallback);
}

TEST(TaskbarTrayContractsTest, TokenTableZoneDefaultsToUntrustedActionFields) {
    json zones = json::array({json{
        {"id", "top"}, {"items", json::array({json{
            {"id", "_pb_next"}, {"label", "Forged next"},
            {"_origin", "builtin-playback"}, {"_builtinAction", "next"}
        }})}
    }});
    MenuTokenTable table;
    ASSERT_TRUE(table.RebuildZones(zones, SeqGen(std::make_shared<int>(0))));
    const auto action = table.ResolveSelect(
        zones[0]["items"][0]["_token"].get<std::string>());
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(action->origin, menu_action::Origin::User);
    EXPECT_EQ(action->builtin, menu_action::Builtin::None);
    EXPECT_EQ(menu_action::DecideRoute(*action),
              menu_action::RouteDecision::FireUserCallback);
}

TEST(TaskbarTrayContractsTest, TokenTableExplicitTrustRejectsPartialAndMismatchedPairs) {
    json items = json::array({
        {{"id", "origin-only"}, {"_origin", "builtin-system"}},
        {{"id", "builtin-only"}, {"_builtinAction", "exit"}},
        {{"id", "system-next"}, {"_origin", "builtin-system"}, {"_builtinAction", "next"}},
        {{"id", "playback-exit"}, {"_origin", "builtin-playback"}, {"_builtinAction", "exit"}},
        {{"id", "user-exit"}, {"_origin", "user"}, {"_builtinAction", "exit"}}
    });
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, SeqGen(std::make_shared<int>(0)), true));
    for (const auto& item : items) {
        const auto action = table.ResolveSelect(item["_token"].get<std::string>());
        ASSERT_TRUE(action.has_value());
        EXPECT_EQ(action->origin, menu_action::Origin::User);
        EXPECT_EQ(action->builtin, menu_action::Builtin::None);
        EXPECT_EQ(menu_action::DecideRoute(*action),
                  menu_action::RouteDecision::FireUserCallback);
    }
}

// Unknown token, a value control (not selectable) and a disabled item are all
// rejected by ResolveSelect; the value control resolves + range-checks via value.
TEST(TaskbarTrayContractsTest, TokenTableRejectsUnknownDisabledAndValueSelects) {
    json items = MakeMenuItemsJson();
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, SeqGen(std::make_shared<int>(0))));

    EXPECT_FALSE(table.ResolveSelect("nonexistent").has_value());  // unknown token

    const std::string volTok = items[2]["_token"];
    EXPECT_FALSE(table.ResolveSelect(volTok).has_value());          // value control not selectable
    auto val = table.ResolveValue(volTok, 55);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "vol");
    EXPECT_FALSE(table.ResolveValue(volTok, 101).has_value());      // slider out of range
    EXPECT_FALSE(table.ResolveValue(items[0]["_token"], 1).has_value());  // "play" is not a value control

    const std::string disTok = items[4]["_token"];
    EXPECT_FALSE(table.ResolveSelect(disTok).has_value());          // disabled item
}

// The first accepted select invalidates the whole set (single-use), so a rapid
// repeat during the exit animation resolves to nothing (expired token).
TEST(TaskbarTrayContractsTest, TokenTableClearInvalidatesWholeSet) {
    json items = MakeMenuItemsJson();
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, SeqGen(std::make_shared<int>(0))));
    const std::string playTok = items[0]["_token"];
    EXPECT_TRUE(table.ResolveSelect(playTok).has_value());
    table.Clear();                                   // first accepted select clears the set
    EXPECT_TRUE(table.Empty());
    EXPECT_FALSE(table.ResolveSelect(playTok).has_value());  // now expired
}

// Generator failure (GUID HRESULT failure surrogate) fails the build
// transactionally: no partial token set survives.
TEST(TaskbarTrayContractsTest, TokenTableFailsTransactionallyOnGeneratorFailure) {
    json items = MakeMenuItemsJson();
    MenuTokenTable table;
    EXPECT_FALSE(table.Rebuild(items, []() { return std::optional<std::string>(); }));       // always fails
    EXPECT_TRUE(table.Empty());
    json items2 = MakeMenuItemsJson();
    EXPECT_FALSE(table.Rebuild(items2, []() { return std::optional<std::string>(std::string()); }));  // empty token
    EXPECT_TRUE(table.Empty());
}

// An unresolvable collision (generator always yields the same token) exhausts the
// retry budget and fails transactionally, rather than overwriting an entry.
TEST(TaskbarTrayContractsTest, TokenTableFailsOnUnresolvableCollision) {
    json items = MakeMenuItemsJson();
    MenuTokenTable table;
    EXPECT_FALSE(table.Rebuild(items, []() { return std::optional<std::string>("dup"); }));
    EXPECT_TRUE(table.Empty());
}

// A transient collision is retried and recovered, producing distinct tokens.
TEST(TaskbarTrayContractsTest, TokenTableRetriesTransientCollision) {
    json items = json::array();
    { json it; it["id"] = "a"; items.push_back(it); }
    { json it; it["id"] = "b"; items.push_back(it); }
    MenuTokenTable table;
    std::vector<std::string> seq{"x", "x", "y"};   // 2nd node collides on "x", retries to "y"
    auto i = std::make_shared<size_t>(0);
    ASSERT_TRUE(table.Rebuild(items, [i, seq]() -> std::optional<std::string> {
        return (*i < seq.size()) ? std::optional<std::string>(seq[(*i)++]) : std::optional<std::string>("z");
    }));
    EXPECT_EQ(table.Size(), 2u);
    EXPECT_EQ(items[0]["_token"], "x");
    EXPECT_EQ(items[1]["_token"], "y");
    EXPECT_TRUE(table.ResolveSelect("x").has_value());
    EXPECT_TRUE(table.ResolveSelect("y").has_value());
}

TEST(TaskbarTrayContractsTest, MeasureReportValidationAndGateAreAtomic) {
    using namespace menu_overlay_geometry;
    EXPECT_TRUE(IsValidMeasureReport({{240, 310}, {320, 280}}, true));
    EXPECT_TRUE(IsValidMeasureReport({{240, 310}, {0, 0}}, false));
    EXPECT_FALSE(IsValidMeasureReport({{240, 310}, {0, 0}}, true));
    EXPECT_FALSE(IsValidMeasureReport({{0, 310}, {0, 0}}, false));
    EXPECT_FALSE(IsValidMeasureReport({{240, 310}, {200000, 280}}, true));

    MeasureGate gate;
    gate.Begin();
    EXPECT_FALSE(gate.TryConsume(false));
    EXPECT_TRUE(gate.IsAwaiting());
    EXPECT_TRUE(gate.TryConsume(true));
    EXPECT_FALSE(gate.IsAwaiting());
    EXPECT_FALSE(gate.TryConsume(true));
}

TEST(TaskbarTrayContractsTest, SubmenuSlotAllocationUsesActualMeasuredSize) {
    using namespace menu_overlay_geometry;
    auto exact = ComputePlacement({{240, 310}, {320, 280}}, true, 1000, 700);
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(exact->rootSlotW, 240);
    EXPECT_EQ(exact->subSlotW, 320);
    EXPECT_EQ(exact->viewportW, 560);
    EXPECT_EQ(exact->viewportH, 310);
    EXPECT_EQ(exact->rootTop, 0);

    auto wide = ComputePlacement({{300, 200}, {900, 180}}, true, 800, 600);
    ASSERT_TRUE(wide.has_value());
    EXPECT_EQ(wide->rootSlotW, 300);
    EXPECT_EQ(wide->subSlotW, 500);

    auto bothWide = ComputePlacement({{700, 200}, {600, 900}}, true, 800, 500);
    ASSERT_TRUE(bothWide.has_value());
    EXPECT_EQ(bothWide->rootSlotW, 400);
    EXPECT_EQ(bothWide->subSlotW, 400);
    EXPECT_EQ(bothWide->viewportH, 500);
    EXPECT_EQ(bothWide->rootTop, 300);

    auto plain = ComputePlacement({{240, 310}, {0, 0}}, false, 800, 500);
    ASSERT_TRUE(plain.has_value());
    EXPECT_EQ(plain->viewportW, 240);
    EXPECT_EQ(plain->subSlotW, 0);
}

TEST(TaskbarTrayContractsTest, ContentSizedWindowsUseTightRootAndSubmenuExtents) {
    using namespace menu_overlay_geometry;
    const auto placement = ComputePlacement({{240, 180}, {320, 420}}, true, 900, 500);
    ASSERT_TRUE(placement.has_value());
    EXPECT_EQ(placement->viewportW, 560);  // virtual canvas reserves one submenu slot
    EXPECT_EQ(placement->viewportH, 420);  // submenu can be taller than root
    EXPECT_EQ(placement->rootTop, 240);

    const Rect root = RootWindowExtent(*placement);
    EXPECT_EQ(root.x, 0);
    EXPECT_EQ(root.y, 240);
    EXPECT_EQ(root.w, 240);
    EXPECT_EQ(root.h, 180);
    EXPECT_NE(root.w, placement->viewportW);  // initial HWND never contains the reserved slot

    const auto submenu = SubmenuWindowExtent(*placement, Rect{240, 16, 320, 420});
    ASSERT_TRUE(submenu.has_value());
    EXPECT_EQ(submenu->x, 240);
    EXPECT_EQ(submenu->y, 0);
    EXPECT_EQ(submenu->w, 320);
    EXPECT_EQ(submenu->h, 420);

    const Rect virtualOrigin{1000, 300, 0, 0};
    const Rect rootScreen = TranslateRect(root, virtualOrigin.x, virtualOrigin.y);
    const Rect submenuScreen = TranslateRect(*submenu, virtualOrigin.x, virtualOrigin.y);
    EXPECT_EQ(rootScreen.x, 1000);
    EXPECT_EQ(rootScreen.y, 540);
    EXPECT_EQ(submenuScreen.x, 1240);
    EXPECT_EQ(submenuScreen.y, 300);
    // Closing restores exactly the same root bounds and screen anchor.
    EXPECT_EQ(TranslateRect(RootWindowExtent(*placement), virtualOrigin.x, virtualOrigin.y).x,
              rootScreen.x);
    EXPECT_EQ(TranslateRect(RootWindowExtent(*placement), virtualOrigin.x, virtualOrigin.y).y,
              rootScreen.y);
}

TEST(TaskbarTrayContractsTest, SubmenuStateSequenceRejectsLateHoverReports) {
    using namespace menu_overlay_geometry;
    EXPECT_TRUE(IsNewerSubmenuPanelSequence(0, 1));
    EXPECT_TRUE(IsNewerSubmenuPanelSequence(7, 8));
    EXPECT_FALSE(IsNewerSubmenuPanelSequence(7, 7));
    EXPECT_FALSE(IsNewerSubmenuPanelSequence(7, 6));
    EXPECT_FALSE(IsNewerSubmenuPanelSequence(7, 0));
}

TEST(TaskbarTrayContractsTest, RootClosePolicyHidesChildBeforeAnyRootFade) {
    using namespace menu_overlay_geometry;
    for (const auto reason : {"outside", "escape", "select", "blur", "replaced", "timeout", "measure_timeout"}) {
        const auto policy = ResolveRootClosePolicy(reason, true, 180);
        EXPECT_TRUE(policy.hideSubmenuImmediately) << reason;
        EXPECT_TRUE(policy.clearSubmenuStateImmediately) << reason;
        EXPECT_FALSE(policy.restoreRootFocus) << reason;
        EXPECT_FALSE(policy.notifyRootRenderer) << reason;
    }
    EXPECT_TRUE(ResolveRootClosePolicy("outside", true, 180).animateRoot);
    EXPECT_TRUE(ResolveRootClosePolicy("escape", true, 180).animateRoot);
    EXPECT_TRUE(ResolveRootClosePolicy("select", true, 180).animateRoot);
    EXPECT_TRUE(ResolveRootClosePolicy("blur", true, 180).animateRoot);
    EXPECT_FALSE(ResolveRootClosePolicy("replaced", true, 180).animateRoot);
    EXPECT_FALSE(ResolveRootClosePolicy("timeout", true, 180).animateRoot);
    EXPECT_FALSE(ResolveRootClosePolicy("outside", false, 180).animateRoot);
    EXPECT_FALSE(ResolveRootClosePolicy("outside", true, 0).animateRoot);
}

TEST(TaskbarTrayContractsTest, SubmenuGateRequiresNonZeroStateSequence) {
    using namespace menu_overlay_geometry;
    const Placement placement{560, 310, 240, 320, 0, 310};
    const auto missingSequence = EvaluateSubmenuPanelUpdate(
        true, placement, SubmenuPanelRequest{true, 240, 20, 300, 120, 0});
    EXPECT_FALSE(missingSequence.accepted);
    EXPECT_FALSE(missingSequence.submenuWindow.has_value());
}

TEST(TaskbarTrayContractsTest, SubmenuPanelGateRequiresContentPlacement) {
    using namespace menu_overlay_geometry;
    const Placement placement{560, 310, 240, 320, 0, 310};
    const SubmenuPanelRequest visible{true, 240, 20, 300, 120, 1};

    const auto fullscreen = EvaluateSubmenuPanelUpdate(false, placement, visible);
    EXPECT_FALSE(fullscreen.accepted);

    const auto unplaced = EvaluateSubmenuPanelUpdate(true, std::nullopt, visible);
    EXPECT_FALSE(unplaced.accepted);
}

TEST(TaskbarTrayContractsTest, SubmenuPanelGateRestoresRootOnlyForValidMenu) {
    using namespace menu_overlay_geometry;
    const Placement placement{560, 310, 240, 320, 0, 310};
    const auto update = EvaluateSubmenuPanelUpdate(
        true, placement, SubmenuPanelRequest{false, 0, 0, 0, 0, 1});

    ASSERT_TRUE(update.accepted);
    EXPECT_EQ(update.rootWindow.w, 240);
    EXPECT_FALSE(update.submenuWindow.has_value());
    EXPECT_FALSE(update.submenu.has_value());
}

TEST(TaskbarTrayContractsTest, SubmenuPanelGateRejectsRangeBeforeClamp) {
    using namespace menu_overlay_geometry;
    const Placement placement{560, 310, 240, 320, 0, 310};
    const auto invalid = EvaluateSubmenuPanelUpdate(
        true, placement,
        SubmenuPanelRequest{true, kMaxPanelCoordinatePx + 1, 0, 300, 120, 1});

    EXPECT_FALSE(invalid.accepted);
    EXPECT_FALSE(invalid.submenu.has_value());

    const auto overflowingEdge = EvaluateSubmenuPanelUpdate(
        true, placement,
        SubmenuPanelRequest{true, kMaxPanelCoordinatePx - 10, 0, 20, 120, 1});
    EXPECT_FALSE(overflowingEdge.accepted);
}

TEST(TaskbarTrayContractsTest, SubmenuPanelGateClampsOnlyValidatedInput) {
    using namespace menu_overlay_geometry;
    const Placement placement{560, 310, 240, 320, 0, 310};
    const auto update = EvaluateSubmenuPanelUpdate(
        true, placement, SubmenuPanelRequest{true, -800, -50, 9999, 9999, 1});

    ASSERT_TRUE(update.accepted);
    ASSERT_TRUE(update.submenu.has_value());
    ASSERT_TRUE(update.submenuWindow.has_value());
    EXPECT_EQ(update.submenu->x, 240);
    EXPECT_EQ(update.submenu->y, 0);
    EXPECT_EQ(update.submenu->w, 320);
    EXPECT_EQ(update.submenu->h, 310);
    EXPECT_EQ(update.submenuWindow->x, 240);
    EXPECT_EQ(update.submenuWindow->w, 320);
}

// 鈹€鈹€ Resource limits (DESIGN 8.5) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€

static TrayMenuItem MakePlainItem(const std::string& id) {
    TrayMenuItem m;
    m.id = id;
    m.label = id;
    m.type = "normal";
    return m;
}

static TrayMenuConfig LimitsTestConfig() {
    TrayMenuConfig c;
    c.showPlaybackControls = false;
    c.showSystemItems = false;
    c.css.clear();
    c.customPosition = TrayMenuPosition::Top;
    return c;
}

TEST(TaskbarTrayContractsTest, ResourceLimitsRejectTooManyItems) {
    std::vector<TrayMenuItem> items;
    items.reserve(static_cast<size_t>(menu_limits::kMaxMenuItems) + 1);
    for (long long i = 0; i < menu_limits::kMaxMenuItems + 1; ++i) {
        items.push_back(MakePlainItem("i" + std::to_string(i)));
    }
    auto r = ValidateTrayMenuResources(items, "");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.field, "items");
    EXPECT_EQ(r.limit, menu_limits::kMaxMenuItems);
    EXPECT_EQ(r.actual, menu_limits::kMaxMenuItems + 1);
}

TEST(TaskbarTrayContractsTest, ResourceLimitsRejectShowMenuDepth) {
    // Build depth 9: root -> 8 nested submenus (depth of deepest node = 9).
    json leaf = {{"id", "d9"}, {"label", "d9"}};
    json cur = leaf;
    for (int d = 8; d >= 1; --d) {
        json parent = {{"id", "d" + std::to_string(d)}, {"label", "d" + std::to_string(d)}};
        parent["submenu"] = json::array({cur});
        cur = parent;
    }
    json items = json::array({cur});
    auto r = menu_limits::ValidateShowMenuResources(items);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.field, "depth");
    EXPECT_EQ(r.limit, menu_limits::kMaxShowMenuDepth);
    EXPECT_GT(r.actual, menu_limits::kMaxShowMenuDepth);
}

TEST(TaskbarTrayContractsTest, ResourceLimitsRejectTooManySegments) {
    TrayMenuItem m = MakePlainItem("seg");
    m.type = "segmented";
    for (int i = 0; i < 65; ++i) {
        TraySegment s;
        s.label = "s" + std::to_string(i);
        m.segments.push_back(s);
    }
    auto r = ValidateTrayMenuResources({m}, "");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.field, "segments");
    EXPECT_EQ(r.limit, menu_limits::kMaxSegmentOptions);
    EXPECT_EQ(r.actual, 65);
}

TEST(TaskbarTrayContractsTest, ResourceLimitsRejectCssOverCap) {
    std::string css(static_cast<size_t>(menu_limits::kMaxCssBytes) + 1, 'x');
    auto r = ValidateTrayMenuResources({}, css);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.field, "css");
    EXPECT_EQ(r.limit, menu_limits::kMaxCssBytes);
    EXPECT_EQ(r.actual, menu_limits::kMaxCssBytes + 1);
}

TEST(TaskbarTrayContractsTest, ResourceLimitsStripOversizedSingleSvgWithoutFail) {
    TrayMenuItem m = MakePlainItem("icon");
    m.iconSvgViewBox = "0 0 24 24";
    m.iconSvgContent.assign(static_cast<size_t>(menu_limits::kMaxSingleSvgBytes) + 1, 'a');
    std::vector<TrayMenuItem> items{m};
    StripOversizedSvgInTree(items);
    EXPECT_TRUE(items[0].iconSvgContent.empty());
    EXPECT_TRUE(items[0].iconSvgViewBox.empty());
    auto r = ValidateTrayMenuResources(items, "");
    EXPECT_TRUE(r.ok);
}

TEST(TaskbarTrayContractsTest, ResourceLimitsRejectSvgTotalOverCap) {
    // 9 脳 30 KiB = 270 KiB > 256 KiB (each under the 32 KiB single cap).
    const size_t each = 30u * 1024u;
    std::vector<TrayMenuItem> items;
    for (int i = 0; i < 9; ++i) {
        TrayMenuItem m = MakePlainItem("s" + std::to_string(i));
        m.iconSvgViewBox = "0 0 24 24";
        m.iconSvgContent.assign(each, 'b');
        items.push_back(std::move(m));
    }
    auto r = ValidateTrayMenuResources(items, "");
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.field, "svgTotal");
    EXPECT_EQ(r.limit, menu_limits::kMaxTotalSvgBytes);
    EXPECT_GT(r.actual, menu_limits::kMaxTotalSvgBytes);
}

TEST(TaskbarTrayContractsTest, ResourceLimitsDetailsCarryFieldLimitActual) {
    auto r = menu_limits::CheckResult::Breach("items", 512, 600);
    auto d = menu_limits::DetailsJson(r);
    EXPECT_EQ(d["field"], "items");
    EXPECT_EQ(d["limit"], 512);
    EXPECT_EQ(d["actual"], 600);
}

// Production transactional writer: a failing preflight must leave the previous
// zone contents untouched (observed via FlatZoneItems on TrayMenuStorage 鈥?the
// same helper TrayIcon::TrySetContextMenu / TrayApi use).
TEST(TaskbarTrayContractsTest, ResourceLimitsTrySetContextMenuIsTransactional) {
    TrayMenuStorage storage;
    storage.config = LimitsTestConfig();
    TrayMenuItem keep = MakePlainItem("keep");
    ASSERT_TRUE(TryReplaceContextMenuZone(storage, {keep}, storage.config).ok);
    ASSERT_EQ(FlatZoneItems(storage).size(), 1u);
    EXPECT_EQ(FlatZoneItems(storage)[0].id, "keep");

    std::vector<TrayMenuItem> tooMany;
    for (long long i = 0; i < menu_limits::kMaxMenuItems + 1; ++i) {
        tooMany.push_back(MakePlainItem("x" + std::to_string(i)));
    }
    auto breach = TryReplaceContextMenuZone(storage, std::move(tooMany), storage.config);
    EXPECT_FALSE(breach.ok);
    EXPECT_EQ(breach.field, "items");
    auto after = FlatZoneItems(storage);
    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ(after[0].id, "keep");
}

TEST(TaskbarTrayContractsTest, ResourceLimitsTryAppendIsTransactional) {
    TrayMenuStorage storage;
    storage.config = LimitsTestConfig();
    ASSERT_TRUE(TryReplaceContextMenuZone(storage, {MakePlainItem("a")}, storage.config).ok);

    std::vector<TrayMenuItem> flood;
    for (long long i = 0; i < menu_limits::kMaxMenuItems; ++i) {
        flood.push_back(MakePlainItem("f" + std::to_string(i)));
    }
    auto breach = TryAppendMenuItemsToStorage(storage, std::move(flood), TrayMenuPosition::Top);
    EXPECT_FALSE(breach.ok);
    EXPECT_EQ(breach.field, "items");
    auto after = FlatZoneItems(storage);
    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ(after[0].id, "a");
}

TEST(TaskbarTrayContractsTest, ResourceLimitsShowMenuAcceptsDepthEight) {
    json cur = {{"id", "d8"}, {"label", "d8"}};
    for (int d = 7; d >= 1; --d) {
        json parent = {{"id", "d" + std::to_string(d)}, {"label", "d" + std::to_string(d)}};
        parent["submenu"] = json::array({cur});
        cur = parent;
    }
    json items = json::array({cur});
    EXPECT_TRUE(menu_limits::ValidateShowMenuResources(items).ok);
}

TEST(TaskbarTrayContractsTest, EffectiveZonesInjectBuiltinsAndFlatten) {
    TrayMenuConfig cfg;
    cfg.showPlaybackControls = true;
    cfg.showSystemItems = true;
    std::vector<TrayMenuItem> top, pb, bt;
    TrayMenuItem userTop; userTop.id = "u"; userTop.label = "User"; userTop.visible = true;
    top.push_back(userTop);
    auto eff = BuildEffectiveTrayZones(top, pb, bt, cfg);
    EXPECT_EQ(eff.top.size(), 1u);
    EXPECT_GE(eff.playback.size(), 4u);
    EXPECT_GE(eff.bottom.size(), 1u);
    EXPECT_TRUE(eff.HasAnyVisible());
    EXPECT_FALSE(FlattenEffectiveZones(eff).empty());
}

TEST(TaskbarTrayContractsTest, EffectiveZonesAllHiddenIsEmptyForShow) {
    TrayMenuConfig cfg;
    cfg.showPlaybackControls = false;
    cfg.showSystemItems = false;
    TrayMenuItem hidden; hidden.id = "h"; hidden.label = "H"; hidden.visible = false;
    auto eff = BuildEffectiveTrayZones({ hidden }, {}, {}, cfg);
    EXPECT_FALSE(eff.HasAnyVisible());
}

TEST(TaskbarTrayContractsTest, TokenTableRebuildZonesAssignsTokens) {
    MenuTokenTable table;
    json zones = json::array({
        json{ {"id", "top"}, {"items", json::array({
            json{{"id", "a"}, {"label", "A"}},
            json{{"type", "separator"}},
        })} },
        json{ {"id", "playback"}, {"items", json::array({
            json{{"id", "b"}, {"label", "B"}},
        })} },
    });
    int n = 0;
    auto gen = [&]() -> std::optional<std::string> {
        return std::string("tok-") + std::to_string(++n);
    };
    ASSERT_TRUE(table.RebuildZones(zones, gen));
    EXPECT_EQ(table.Size(), 2u);
    EXPECT_TRUE(zones[0]["items"][0].contains("_token"));
    EXPECT_TRUE(zones[1]["items"][0].contains("_token"));
    EXPECT_FALSE(zones[0]["items"][1].contains("_token"));
}

TEST(TaskbarTrayContractsTest, FlatSeparatorUsesLastEmittedZoneWhenMiddleEmpty) {
    TrayMenuConfig cfg;
    cfg.showPlaybackControls = false;
    cfg.showSystemItems = false;
    TrayMenuItem a; a.id = "a"; a.label = "A"; a.visible = true;
    TrayMenuItem c; c.id = "c"; c.label = "C"; c.visible = true;
    auto eff = BuildEffectiveTrayZones({ a }, {}, { c }, cfg);
    ASSERT_TRUE(eff.playback.empty());
    auto tagged = FlattenEffectiveZonesTagged(eff);
    ASSERT_EQ(tagged.size(), 3u);
    EXPECT_EQ(tagged[0].item.id, "a");
    EXPECT_EQ(tagged[0].zone, "top");
    EXPECT_EQ(tagged[1].item.type, "separator");
    EXPECT_EQ(tagged[1].zone, "top");  // not "playback"
    EXPECT_EQ(tagged[2].item.id, "c");
    EXPECT_EQ(tagged[2].zone, "bottom");
}

TEST(TaskbarTrayContractsTest, NormalizeVisibleDegradesEmptySubmenuAndDropsHidden) {
    TrayMenuConfig cfg;
    cfg.showPlaybackControls = false;
    cfg.showSystemItems = false;
    TrayMenuItem hiddenChild; hiddenChild.id = "hc"; hiddenChild.visible = false;
    TrayMenuItem parent; parent.id = "p"; parent.label = "Parent"; parent.type = "submenu";
    parent.visible = true;
    parent.submenu = { hiddenChild };
    TrayMenuItem leaf; leaf.id = "l"; leaf.visible = true;
    auto eff = BuildEffectiveTrayZones({ parent, leaf }, {}, {}, cfg);
    ASSERT_EQ(eff.top.size(), 2u);
    EXPECT_EQ(eff.top[0].id, "p");
    EXPECT_EQ(eff.top[0].type, "normal");
    EXPECT_TRUE(eff.top[0].submenu.empty());
    EXPECT_EQ(eff.top[1].id, "l");
    EXPECT_TRUE(eff.HasAnyVisible());
}

TEST(TaskbarTrayContractsTest, NormalizeVisibleAllHiddenSubtreeIsEmpty) {
    TrayMenuConfig cfg;
    cfg.showPlaybackControls = false;
    cfg.showSystemItems = false;
    TrayMenuItem hiddenRoot; hiddenRoot.id = "h"; hiddenRoot.visible = false;
    TrayMenuItem hiddenChild; hiddenChild.id = "hc"; hiddenChild.visible = false;
    TrayMenuItem parent; parent.id = "p"; parent.type = "submenu"; parent.visible = true;
    parent.submenu = { hiddenChild };
    // Parent degrades to leaf — still visible content.
    auto withParent = BuildEffectiveTrayZones({ parent }, {}, {}, cfg);
    EXPECT_TRUE(withParent.HasAnyVisible());
    EXPECT_EQ(withParent.top.size(), 1u);
    EXPECT_EQ(withParent.top[0].type, "normal");

    auto onlyHidden = BuildEffectiveTrayZones({ hiddenRoot }, {}, {}, cfg);
    EXPECT_FALSE(onlyHidden.HasAnyVisible());
    EXPECT_TRUE(onlyHidden.top.empty());
}

// ── Phase 3: slider orientation / range / constant / checkable ──────────────

TEST(TaskbarTrayContractsTest, NormalizeSliderSwapsRangeClampsValueAndOrientation) {
    TrayMenuItem s;
    s.type = "slider";
    s.minValue = 80;
    s.maxValue = 20;
    s.value = 999;
    s.orientation = "vertical";
    TrayMenuItem child = s;
    child.orientation = "sideways";  // unknown → horizontal/cleared
    child.value = -5;
    TrayMenuItem parent;
    parent.type = "submenu";
    parent.submenu = { child };
    TrayMenuItem nons;
    nons.type = "rating";
    nons.orientation = "vertical";  // non-slider ignores orientation
    std::vector<TrayMenuItem> items{ s, parent, nons };
    NormalizeSliderMenuItems(items);
    EXPECT_EQ(items[0].minValue, 20);
    EXPECT_EQ(items[0].maxValue, 80);
    EXPECT_EQ(items[0].value, 80);  // clamped
    EXPECT_EQ(items[0].orientation, "vertical");
    EXPECT_TRUE(TraySliderIsVertical(items[0]));
    EXPECT_FALSE(TraySliderIsConstant(items[0]));
    ASSERT_EQ(items[1].submenu.size(), 1u);
    EXPECT_EQ(items[1].submenu[0].minValue, 20);
    EXPECT_EQ(items[1].submenu[0].maxValue, 80);
    EXPECT_EQ(items[1].submenu[0].value, 20);  // clamped low
    EXPECT_TRUE(items[1].submenu[0].orientation.empty());
    EXPECT_TRUE(items[2].orientation.empty());
}

TEST(TaskbarTrayContractsTest, ConstantSliderRejectsValueChangeAndIsDetected) {
    using menu_action::RichValueInRange;
    TrayMenuItem s;
    s.type = "slider";
    s.minValue = 40;
    s.maxValue = 40;
    s.value = 7;
    std::vector<TrayMenuItem> items{ s };
    NormalizeSliderMenuItems(items);
    EXPECT_TRUE(TraySliderIsConstant(items[0]));
    EXPECT_EQ(items[0].value, 40);
    EXPECT_FALSE(RichValueInRange("slider", 40, items[0].minValue, items[0].maxValue, {}));
    EXPECT_FALSE(RichValueInRange("slider", 41, 40, 40, {}));
}

TEST(TaskbarTrayContractsTest, StorageNormalizesSliderOnReplaceAndAppend) {
    TrayMenuStorage storage;
    TrayMenuItem s;
    s.id = "vol";
    s.type = "slider";
    s.minValue = 10;
    s.maxValue = 0;
    s.value = 50;
    s.orientation = "vertical";
    auto ok = TryReplaceContextMenuZone(storage, { s }, std::nullopt);
    ASSERT_TRUE(ok.ok);
    ASSERT_EQ(storage.zones[0].size(), 1u);
    EXPECT_EQ(storage.zones[0][0].minValue, 0);
    EXPECT_EQ(storage.zones[0][0].maxValue, 10);
    EXPECT_EQ(storage.zones[0][0].value, 10);
    EXPECT_EQ(storage.zones[0][0].orientation, "vertical");

    TrayMenuItem s2;
    s2.id = "vol2";
    s2.type = "slider";
    s2.minValue = 5;
    s2.maxValue = 5;
    s2.value = 1;
    s2.orientation = "horizontal";
    ok = TryAppendMenuItemsToStorage(storage, { s2 }, TrayMenuPosition::Top);
    ASSERT_TRUE(ok.ok);
    ASSERT_EQ(storage.zones[0].size(), 2u);
    EXPECT_TRUE(TraySliderIsConstant(storage.zones[0][1]));
    EXPECT_EQ(storage.zones[0][1].value, 5);
    EXPECT_EQ(storage.zones[0][1].orientation, "horizontal");
}

TEST(TaskbarTrayContractsTest, CheckableIdentityPreservedIncludingFalse) {
    TrayMenuItem explicitFalse;
    explicitFalse.id = "c";
    explicitFalse.checked = false;
    explicitFalse.checkable = true;  // ParseMenuItem would set this on key presence
    EXPECT_TRUE(explicitFalse.checkable);
    EXPECT_FALSE(explicitFalse.checked);

    TrayMenuItem notCheckable;
    notCheckable.id = "n";
    EXPECT_FALSE(notCheckable.checkable);
    EXPECT_FALSE(notCheckable.checked);

    // setMenuItemState path: providing checked establishes checkable.
    TrayMenuStorage storage;
    TrayMenuItem leaf; leaf.id = "x"; leaf.label = "X";
    ASSERT_TRUE(TryReplaceContextMenuZone(storage, { leaf }, std::nullopt).ok);
    // Emulate SetMenuItemState recursive update used by TrayIcon.
    storage.zones[0][0].checked = false;
    storage.zones[0][0].checkable = true;
    EXPECT_TRUE(storage.zones[0][0].checkable);
    EXPECT_FALSE(storage.zones[0][0].checked);
}

TEST(TaskbarTrayContractsTest, TokenTableRejectsConstantSliderValue) {
    json items = json::array();
    { json it; it["type"] = "slider"; it["id"] = "cvol"; it["min"] = 7; it["max"] = 7; it["value"] = 7; items.push_back(it); }
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, SeqGen(std::make_shared<int>(0))));
    const std::string tok = items[0]["_token"];
    EXPECT_FALSE(table.ResolveValue(tok, 7).has_value());
    EXPECT_FALSE(table.ResolveValue(tok, 8).has_value());
}

// ── DESIGN §6.6: caller-declared native playback action (playbackAction) ────

// The public token maps to exactly the four playback built-ins; `exit` and any
// unknown token yield nullopt so the API layer can reject them fail-loud.
TEST(TaskbarTrayContractsTest, PlaybackActionFromStringMapsFourTokens) {
    using namespace menu_action;
    EXPECT_EQ(PlaybackActionFromString("play-pause"), std::optional<Builtin>(Builtin::PlayPause));
    EXPECT_EQ(PlaybackActionFromString("previous"),   std::optional<Builtin>(Builtin::Previous));
    EXPECT_EQ(PlaybackActionFromString("next"),       std::optional<Builtin>(Builtin::Next));
    EXPECT_EQ(PlaybackActionFromString("stop"),       std::optional<Builtin>(Builtin::Stop));

    EXPECT_FALSE(PlaybackActionFromString("exit").has_value());
    EXPECT_FALSE(PlaybackActionFromString("").has_value());
    EXPECT_FALSE(PlaybackActionFromString("Play-Pause").has_value());
    EXPECT_FALSE(PlaybackActionFromString("play_pause").has_value());
    EXPECT_FALSE(PlaybackActionFromString("_pb_next").has_value());
    EXPECT_FALSE(PlaybackActionFromString("nonsense").has_value());
}

// Round-trip inverse: only the four playback actions have a public token;
// None and Exit map to empty so getMenuItems never surfaces a synthetic token.
TEST(TaskbarTrayContractsTest, PlaybackActionToStringRoundTrips) {
    using namespace menu_action;
    EXPECT_STREQ(PlaybackActionToString(Builtin::PlayPause), "play-pause");
    EXPECT_STREQ(PlaybackActionToString(Builtin::Previous),  "previous");
    EXPECT_STREQ(PlaybackActionToString(Builtin::Next),      "next");
    EXPECT_STREQ(PlaybackActionToString(Builtin::Stop),      "stop");
    EXPECT_STREQ(PlaybackActionToString(Builtin::None),      "");
    EXPECT_STREQ(PlaybackActionToString(Builtin::Exit),      "");
    for (const char* tok : {"play-pause", "previous", "next", "stop"}) {
        auto b = PlaybackActionFromString(tok);
        ASSERT_TRUE(b.has_value());
        EXPECT_STREQ(PlaybackActionToString(*b), tok);
    }
}

// A user leaf that declares a valid playbackAction is stamped BuiltinPlayback
// during effective composition while keeping its caller-supplied label / icon /
// id, so it routes to ExecutePlayback (reliable while the main page is hidden).
TEST(TaskbarTrayContractsTest, DeclaredPlaybackActionStampsBuiltinAndKeepsAppearance) {
    TrayMenuItem custom = MkTrayItem("my-next-btn");
    custom.label = "跳到下一首";
    custom.icon = "data:image/png;base64,AAAA";
    custom.playbackAction = "next";

    TrayMenuConfig config;  // showPlaybackControls / showSystemItems default off
    auto zones = BuildEffectiveTrayZones({ custom }, {}, {}, config);

    ASSERT_EQ(zones.top.size(), 1u);
    const TrayMenuItem& stamped = zones.top[0];
    EXPECT_EQ(stamped.origin, menu_action::Origin::BuiltinPlayback);
    EXPECT_EQ(stamped.builtinAction, menu_action::Builtin::Next);
    // Appearance and identity are caller-owned and untouched by the stamp.
    EXPECT_EQ(stamped.id, "my-next-btn");
    EXPECT_EQ(stamped.label, "跳到下一首");
    EXPECT_EQ(stamped.icon, "data:image/png;base64,AAAA");
    EXPECT_EQ(stamped.playbackAction, "next");
    EXPECT_EQ(menu_action::DecideRoute(ResolveTrayMenuItemAction(stamped)),
              menu_action::RouteDecision::ExecutePlayback);
}

// All four tokens stamp their matching built-in action.
TEST(TaskbarTrayContractsTest, DeclaredPlaybackActionStampsEachOfFourActions) {
    struct Case { const char* token; menu_action::Builtin builtin; };
    const Case cases[] = {
        { "play-pause", menu_action::Builtin::PlayPause },
        { "previous",   menu_action::Builtin::Previous },
        { "next",       menu_action::Builtin::Next },
        { "stop",       menu_action::Builtin::Stop },
    };
    for (const auto& c : cases) {
        TrayMenuItem item = MkTrayItem("btn");
        item.playbackAction = c.token;
        auto zones = BuildEffectiveTrayZones({ item }, {}, {}, TrayMenuConfig{});
        ASSERT_EQ(zones.top.size(), 1u);
        EXPECT_EQ(zones.top[0].origin, menu_action::Origin::BuiltinPlayback);
        EXPECT_EQ(zones.top[0].builtinAction, c.builtin);
        EXPECT_EQ(menu_action::DecideRoute(ResolveTrayMenuItemAction(zones.top[0])),
                  menu_action::RouteDecision::ExecutePlayback);
    }
}

// Composition-layer defence: an invalid token that somehow reaches assembly
// (the API layer rejects it earlier fail-loud) must NOT gain a built-in route.
TEST(TaskbarTrayContractsTest, InvalidDeclaredPlaybackActionStaysUserAtComposition) {
    for (const char* bad : {"exit", "Next", "nonsense", "_pb_next"}) {
        TrayMenuItem item = MkTrayItem("btn");
        item.playbackAction = bad;
        auto zones = BuildEffectiveTrayZones({ item }, {}, {}, TrayMenuConfig{});
        ASSERT_EQ(zones.top.size(), 1u);
        EXPECT_EQ(zones.top[0].origin, menu_action::Origin::User);
        EXPECT_EQ(zones.top[0].builtinAction, menu_action::Builtin::None);
        EXPECT_EQ(menu_action::DecideRoute(ResolveTrayMenuItemAction(zones.top[0])),
                  menu_action::RouteDecision::FireUserCallback);
    }
}

// Only normal leaves are eligible. A rich control / separator that carries the
// field is never stamped, so it never runs a built-in playback command.
TEST(TaskbarTrayContractsTest, DeclaredPlaybackActionIgnoredOnNonNormalLeaves) {
    for (const char* type : {"separator", "rating", "slider", "segmented", "nowplaying", "checkbox"}) {
        TrayMenuItem item = MkTrayItem("rich");
        item.type = type;
        item.playbackAction = "next";
        auto zones = BuildEffectiveTrayZones({ item }, {}, {}, TrayMenuConfig{});
        ASSERT_EQ(zones.top.size(), 1u);
        EXPECT_EQ(zones.top[0].origin, menu_action::Origin::User)
            << "type=" << type;
        EXPECT_EQ(zones.top[0].builtinAction, menu_action::Builtin::None)
            << "type=" << type;
    }
}

// A submenu parent is not a selectable leaf; the field on it is ignored, and a
// declaring leaf INSIDE the submenu is still stamped (recursion works).
TEST(TaskbarTrayContractsTest, DeclaredPlaybackActionIgnoredOnSubmenuParentButAppliedToChild) {
    TrayMenuItem child = MkTrayItem("child-prev");
    child.playbackAction = "previous";
    TrayMenuItem parent = MkTrayItem("parent");
    parent.type = "submenu";
    parent.playbackAction = "next";  // must be ignored on the parent
    parent.submenu.push_back(child);

    auto zones = BuildEffectiveTrayZones({ parent }, {}, {}, TrayMenuConfig{});
    ASSERT_EQ(zones.top.size(), 1u);
    const TrayMenuItem& composedParent = zones.top[0];
    EXPECT_EQ(composedParent.origin, menu_action::Origin::User);
    EXPECT_EQ(composedParent.builtinAction, menu_action::Builtin::None);
    ASSERT_EQ(composedParent.submenu.size(), 1u);
    EXPECT_EQ(composedParent.submenu[0].origin, menu_action::Origin::BuiltinPlayback);
    EXPECT_EQ(composedParent.submenu[0].builtinAction, menu_action::Builtin::Previous);
}

// Event-emission gap coverage (previously only implicit): a plain user item
// routes to the frontend callback (tray:menuItemClicked), while a declared
// playback item routes to native execution and therefore fires no click event.
TEST(TaskbarTrayContractsTest, PlainUserItemFiresCallbackDeclaredPlaybackDoesNot) {
    TrayMenuItem plain = MkTrayItem("open-settings");
    TrayMenuItem declared = MkTrayItem("bg-play");
    declared.playbackAction = "play-pause";

    auto zones = BuildEffectiveTrayZones({ plain, declared }, {}, {}, TrayMenuConfig{});
    ASSERT_EQ(zones.top.size(), 2u);

    // Plain user item -> FireUserCallback (the only path to tray:menuItemClicked).
    const auto plainAction = ResolveTrayMenuItemAction(zones.top[0]);
    EXPECT_EQ(plainAction.origin, menu_action::Origin::User);
    EXPECT_EQ(menu_action::DecideRoute(plainAction),
              menu_action::RouteDecision::FireUserCallback);

    // Declared playback item -> ExecutePlayback, NOT FireUserCallback, so
    // RouteResolvedAction returns before touching m_menuItemCb (no click event).
    const auto declaredAction = ResolveTrayMenuItemAction(zones.top[1]);
    EXPECT_EQ(declaredAction.origin, menu_action::Origin::BuiltinPlayback);
    EXPECT_NE(menu_action::DecideRoute(declaredAction),
              menu_action::RouteDecision::FireUserCallback);
    EXPECT_EQ(menu_action::DecideRoute(declaredAction),
              menu_action::RouteDecision::ExecutePlayback);
}

// A forged internal action pair still fails closed even when playbackAction is
// absent: the public field is the only sanctioned promotion path, and it is
// stamped by the host, never trusted from renderer-supplied _origin/_builtinAction.
TEST(TaskbarTrayContractsTest, ForgedInternalFieldsIgnoredWhilePlaybackActionPathIsHostOnly) {
    // Renderer-forged internal pair without the public field: token table with
    // the default (untrusted) policy must keep it a user action.
    json items = json::array({{
        {"id", "forged"}, {"label", "Forged next"},
        {"_origin", "builtin-playback"}, {"_builtinAction", "next"}
    }});
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, SeqGen(std::make_shared<int>(0))));
    const auto action = table.ResolveSelect(items[0]["_token"].get<std::string>());
    ASSERT_TRUE(action.has_value());
    EXPECT_EQ(action->origin, menu_action::Origin::User);
    EXPECT_EQ(action->builtin, menu_action::Builtin::None);

    // The sanctioned path is the public field stamped by the host during
    // composition; it does not depend on any renderer-supplied internal field.
    TrayMenuItem declared = MkTrayItem("bg-next");
    declared.playbackAction = "next";
    auto zones = BuildEffectiveTrayZones({ declared }, {}, {}, TrayMenuConfig{});
    ASSERT_EQ(zones.top.size(), 1u);
    EXPECT_EQ(zones.top[0].origin, menu_action::Origin::BuiltinPlayback);
    EXPECT_EQ(zones.top[0].builtinAction, menu_action::Builtin::Next);
}

// Dual-backend consistency (mirrors ExplicitExitPreservesTrustedRouteAcrossBackends):
// a declared playback leaf carries the same stamp into the native command map
// and the WebView token transport, and the renderer never sees internal fields.
TEST(TaskbarTrayContractsTest, DeclaredPlaybackPreservesTrustedRouteAcrossBackends) {
    TrayMenuItem declared = MkTrayItem("bg-play");
    declared.label = "后台播放/暂停";
    declared.playbackAction = "play-pause";
    auto zones = BuildEffectiveTrayZones({ declared }, {}, {}, TrayMenuConfig{});
    ASSERT_EQ(zones.top.size(), 1u);
    const TrayMenuItem& composed = zones.top[0];

    // Native BuildMenu stores this exact helper result in m_menuIdMap.
    const auto nativeAction = ResolveTrayMenuItemAction(composed);
    EXPECT_EQ(menu_action::DecideRoute(nativeAction),
              menu_action::RouteDecision::ExecutePlayback);
    EXPECT_EQ(nativeAction.builtin, menu_action::Builtin::PlayPause);

    // WebView TrayItemToMenuJson emits the same internal fields; the token table
    // strips them from renderer-visible JSON and resolves the opaque token.
    const auto fields = TrayMenuItemActionFields(composed);
    ASSERT_TRUE(fields.IsStamped());
    EXPECT_EQ(*fields.origin, "builtin-playback");
    EXPECT_EQ(*fields.builtin, "play-pause");
    json webItems = json::array({{
        {"id", composed.id}, {"label", composed.label},
        {"_origin", *fields.origin}, {"_builtinAction", *fields.builtin}
    }});
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(webItems, SeqGen(std::make_shared<int>(0)), true));
    EXPECT_FALSE(webItems[0].contains("_origin"));
    EXPECT_FALSE(webItems[0].contains("_builtinAction"));
    const auto webAction = table.ResolveSelect(webItems[0]["_token"].get<std::string>());
    ASSERT_TRUE(webAction.has_value());
    EXPECT_EQ(menu_action::DecideRoute(*webAction),
              menu_action::RouteDecision::ExecutePlayback);
    EXPECT_EQ(webAction->builtin, menu_action::Builtin::PlayPause);
}
