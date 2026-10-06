// test_taskbar_tray_contracts.cpp - Taskbar buttons, tray item kinds, zone composition,
// origin routing, opaque menu tokens and overlay submenu gates
#include "pch.h"
#include <Windows.h>
#include "window/TaskbarTrayContracts.h"
#include "window/TrayIcon.h"
#include "window/MenuTokenTable.h"
#include "window/MenuOverlayGeometry.h"
#include "window/MenuResourceLimits.h"
#include "harness/TrayMenuTestSupport.h"

using json = nlohmann::json;

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

// Integrality probe for tray rich-control values. The host path reads the value
// as an int, so tray:menuItemClicked never reports a fraction; this only pins
// the predicate itself.
TEST(TaskbarTrayContractsTest, TrayMenuValueIntegralityProbe) {
    EXPECT_TRUE(taskbar_tray_contracts::TrayMenuValueIsIntegral(0.0));
    EXPECT_TRUE(taskbar_tray_contracts::TrayMenuValueIsIntegral(5.0));
    EXPECT_TRUE(taskbar_tray_contracts::TrayMenuValueIsIntegral(100.0));
    EXPECT_FALSE(taskbar_tray_contracts::TrayMenuValueIsIntegral(2.5));
}

namespace {
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

// System defaults inject show-main-window BEFORE exit (show/restore first, quit
// last), both stamped Origin::BuiltinSystem by the builder. `_sys_show` must be
// native for the same reason Exit is: hiding to tray deep-suspends the main page
// (put_IsVisible(FALSE) + TrySuspend), so a tray:menuItemClicked handler cannot
// run to call window.focus itself.
TEST(TaskbarTrayContractsTest, SystemDefaultsCarryBuiltinExitOrigin) {
    auto items = BuildSystemDefaultItems();
    ASSERT_EQ(items.size(), 2u);
    for (const auto& m : items) {
        EXPECT_EQ(m.origin, menu_action::Origin::BuiltinSystem);
        EXPECT_EQ(m.type, "normal");
        EXPECT_TRUE(m.enabled);
        EXPECT_TRUE(m.visible);
        // Built-in system items never declare the public playbackAction field.
        EXPECT_TRUE(m.playbackAction.empty());
    }
    EXPECT_EQ(items[0].id, "_sys_show");
    EXPECT_EQ(items[0].builtinAction, menu_action::Builtin::ShowMainWindow);
    EXPECT_EQ(items[1].id, "_sys_exit");
    EXPECT_EQ(items[1].builtinAction, menu_action::Builtin::Exit);

    // Both route natively, never to the frontend callback.
    for (const auto& m : items) {
        EXPECT_EQ(menu_action::DecideRoute({ m.id, m.origin, m.builtinAction }),
                  menu_action::RouteDecision::ExecuteSystem);
    }
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

    // A frontend that renders its own "show main window" row still gets the
    // native route, because the frontend-event route is structurally dead in
    // exactly the tray-hidden state such a row exists for.
    auto show = ReservedBuiltinForPublicId("_sys_show");
    ASSERT_TRUE(show.has_value());
    EXPECT_EQ(show->origin, Origin::BuiltinSystem);
    EXPECT_EQ(show->builtin, Builtin::ShowMainWindow);
    EXPECT_EQ(DecideRoute(*show), RouteDecision::ExecuteSystem);

    EXPECT_FALSE(ReservedBuiltinForPublicId("_sys_exit_backup").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_SYS_EXIT").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_sys_show_alt").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_SYS_SHOW").has_value());
    EXPECT_FALSE(ReservedBuiltinForPublicId("_sys_showMainWindow").has_value());
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

// showSystemItems injects `_sys_show` into bottom by default, and dedupes it
// against a frontend-supplied row of the same exact id (which promotion has
// already stamped native). A lookalike id stays a user action, so it is both
// kept as-is AND does not suppress the injection.
TEST(TaskbarTrayContractsTest, EffectiveZonesInjectAndDedupeShowMainWindow) {
    TrayMenuConfig config;
    config.showPlaybackControls = false;
    config.showSystemItems = true;

    // Default injection: both system items land in bottom, show before exit.
    auto injected = BuildEffectiveTrayZones({ MkTrayItem("u") }, {}, {}, config);
    ASSERT_EQ(injected.bottom.size(), 2u);
    EXPECT_EQ(injected.bottom[0].id, "_sys_show");
    EXPECT_EQ(injected.bottom[0].builtinAction, menu_action::Builtin::ShowMainWindow);
    EXPECT_EQ(injected.bottom[1].id, "_sys_exit");

    // Frontend supplies its own `_sys_show` row with a custom label: it is
    // promoted native (label preserved) and the duplicate is not injected.
    TrayMenuItem ownShow = MkTrayItem("_sys_show");
    ownShow.label = "打开播放器";
    auto deduped = BuildEffectiveTrayZones({ ownShow }, {}, {}, config);
    EXPECT_EQ(std::ranges::count_if(deduped.top, [](const TrayMenuItem& item) {
        return item.id == "_sys_show";
    }), 1);
    EXPECT_FALSE(TrayMenuContainsPublicId(deduped.bottom, "_sys_show"));
    EXPECT_EQ(deduped.top[0].origin, menu_action::Origin::BuiltinSystem);
    EXPECT_EQ(deduped.top[0].builtinAction, menu_action::Builtin::ShowMainWindow);
    EXPECT_EQ(deduped.top[0].label, "打开播放器");
    // Exit is still injected; only the exact duplicate is suppressed.
    EXPECT_TRUE(TrayMenuContainsPublicId(deduped.bottom, "_sys_exit"));

    // Lookalike stays a user action and does NOT suppress the injection.
    auto lookalike = BuildEffectiveTrayZones({ MkTrayItem("_sys_show_alt") }, {}, {}, config);
    EXPECT_EQ(lookalike.top[0].origin, menu_action::Origin::User);
    EXPECT_EQ(lookalike.top[0].builtinAction, menu_action::Builtin::None);
    EXPECT_TRUE(TrayMenuContainsPublicId(lookalike.bottom, "_sys_show"));
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
    EXPECT_EQ(DecideRoute({ "_sys_show", Origin::BuiltinSystem, Builtin::ShowMainWindow }),
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
    EXPECT_EQ(BuiltinFromString(BuiltinToString(Builtin::Exit)), Builtin::Exit);
    EXPECT_EQ(BuiltinFromString(BuiltinToString(Builtin::ShowMainWindow)),
              Builtin::ShowMainWindow);
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

// The injected `_sys_show` stamp must survive the same two transports as Exit.
// This is what keeps "show main window" working while the main page is
// deep-suspended: routing resolves to ExecuteSystem natively on both backends,
// so it never depends on a tray:menuItemClicked handler running.
TEST(TaskbarTrayContractsTest, ShowMainWindowPreservesTrustedRouteAcrossBackends) {
    TrayMenuConfig config;
    config.showPlaybackControls = false;
    config.showSystemItems = true;

    auto zones = BuildEffectiveTrayZones({}, {}, {}, config);
    auto composed = std::ranges::find_if(zones.bottom, [](const TrayMenuItem& item) {
        return item.id == "_sys_show";
    });
    ASSERT_NE(composed, zones.bottom.end());
    EXPECT_EQ(composed->origin, menu_action::Origin::BuiltinSystem);
    EXPECT_EQ(composed->builtinAction, menu_action::Builtin::ShowMainWindow);

    // Native BuildMenu stores this exact helper result in m_menuIdMap.
    EXPECT_EQ(menu_action::DecideRoute(ResolveTrayMenuItemAction(*composed)),
              menu_action::RouteDecision::ExecuteSystem);

    // WebView overlay: stamped fields -> opaque token -> resolved back.
    const auto fields = TrayMenuItemActionFields(*composed);
    ASSERT_TRUE(fields.IsStamped());
    EXPECT_EQ(*fields.origin, "builtin-system");
    EXPECT_EQ(*fields.builtin, "show-main-window");
    json webItems = json::array({{
        {"id", composed->id}, {"label", composed->label},
        {"_origin", *fields.origin}, {"_builtinAction", *fields.builtin}
    }});
    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(webItems, SeqGen(std::make_shared<int>(0)), true));
    EXPECT_FALSE(webItems[0].contains("_origin"));
    EXPECT_FALSE(webItems[0].contains("_builtinAction"));
    const auto webShow = table.ResolveSelect(webItems[0]["_token"].get<std::string>());
    ASSERT_TRUE(webShow.has_value());
    EXPECT_EQ(webShow->builtin, menu_action::Builtin::ShowMainWindow);
    EXPECT_EQ(menu_action::DecideRoute(*webShow),
              menu_action::RouteDecision::ExecuteSystem);
}

// A renderer that forges the show-main-window pair without owner mode must fail
// closed, exactly like the forged-exit case.
TEST(TaskbarTrayContractsTest, TokenTableDefaultsToUntrustedShowMainWindow) {
    json items = json::array({{
        {"id", "_sys_show"}, {"label", "Forged show"},
        {"_origin", "builtin-system"}, {"_builtinAction", "show-main-window"}
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
