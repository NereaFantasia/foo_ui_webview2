// test_tray_menu_contracts.cpp - Tray menu resource limits, zone storage and flattening,
// visibility normalization, sliders and declared playback actions
#include "pch.h"
#include <Windows.h>
#include "window/TaskbarTrayContracts.h"
#include "window/TrayIcon.h"
#include "window/MenuTokenTable.h"
#include "window/MenuOverlayGeometry.h"
#include "window/MenuResourceLimits.h"
#include "harness/TrayMenuTestSupport.h"

using json = nlohmann::json;

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
// zone contents untouched (observed via FlatZoneItems on TrayMenuStorage, the
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

static TrayZoneReplacement FullTrayMenu(const std::string& tag) {
    TrayZoneReplacement zones;
    zones[0] = std::vector<TrayMenuItem>{MakePlainItem(tag + "Top")};
    zones[1] = std::vector<TrayMenuItem>{MakePlainItem(tag + "Playback")};
    zones[2] = std::vector<TrayMenuItem>{MakePlainItem(tag + "Bottom")};
    return zones;
}

TEST(TaskbarTrayContractsTest, SetContextMenuReplacesOnlyTheCustomPositionZone) {
    TrayMenuStorage storage;
    storage.config = LimitsTestConfig();
    ASSERT_TRUE(TryReplaceTrayZones(storage, FullTrayMenu("old"), std::nullopt).ok);

    TrayMenuConfig conf = storage.config;
    conf.customPosition = TrayMenuPosition::Bottom;
    ASSERT_TRUE(TryReplaceContextMenuZone(storage, {MakePlainItem("new")}, conf).ok);

    auto flat = FlatZoneItems(storage);
    ASSERT_EQ(flat.size(), 3u);
    EXPECT_EQ(flat[0].id, "oldTop");
    EXPECT_EQ(flat[1].id, "oldPlayback");
    EXPECT_EQ(flat[2].id, "new");
    EXPECT_EQ(storage.config.customPosition, TrayMenuPosition::Bottom);
}

// tray.setMenuZones passes an empty vector for a zone the caller left out, so
// the zone is cleared; std::nullopt (setContextMenu's untouched zones) keeps it.
TEST(TaskbarTrayContractsTest, ReplaceTrayZonesClearsEmptyZonesAndKeepsNullopt) {
    TrayMenuStorage storage;
    storage.config = LimitsTestConfig();
    ASSERT_TRUE(TryReplaceTrayZones(storage, FullTrayMenu("old"), std::nullopt).ok);

    TrayZoneReplacement zones;
    zones[0] = std::vector<TrayMenuItem>{MakePlainItem("newTop")};
    zones[1] = std::vector<TrayMenuItem>{};
    ASSERT_TRUE(TryReplaceTrayZones(storage, std::move(zones), std::nullopt).ok);

    ASSERT_EQ(storage.zones[0].size(), 1u);
    EXPECT_EQ(storage.zones[0][0].id, "newTop");
    EXPECT_TRUE(storage.zones[1].empty());
    ASSERT_EQ(storage.zones[2].size(), 1u);
    EXPECT_EQ(storage.zones[2][0].id, "oldBottom");
}

TEST(TaskbarTrayContractsTest, ReplaceTrayZonesTwiceKeepsOnlyTheLaterMenu) {
    TrayMenuStorage storage;
    storage.config = LimitsTestConfig();
    ASSERT_TRUE(TryReplaceTrayZones(storage, FullTrayMenu("first"), std::nullopt).ok);
    ASSERT_TRUE(TryReplaceTrayZones(storage, FullTrayMenu("second"), std::nullopt).ok);

    auto flat = FlatZoneItems(storage);
    ASSERT_EQ(flat.size(), 3u);
    EXPECT_EQ(flat[0].id, "secondTop");
    EXPECT_EQ(flat[1].id, "secondPlayback");
    EXPECT_EQ(flat[2].id, "secondBottom");
}

// Each zone is within the item limit on its own; only the composed menu is
// over it, so the check has to run on all three zones before any is stored.
TEST(TaskbarTrayContractsTest, ResourceLimitsReplaceTrayZonesIsTransactional) {
    TrayMenuStorage storage;
    storage.config = LimitsTestConfig();
    ASSERT_TRUE(TryReplaceTrayZones(storage, FullTrayMenu("old"), std::nullopt).ok);

    TrayZoneReplacement zones;
    const long long perZone = menu_limits::kMaxMenuItems / 2 + 1;
    for (size_t z = 0; z < 2; ++z) {
        std::vector<TrayMenuItem> rows;
        for (long long i = 0; i < perZone; ++i) {
            rows.push_back(MakePlainItem("z" + std::to_string(z) + "_" + std::to_string(i)));
        }
        zones[z] = std::move(rows);
    }
    zones[2] = std::vector<TrayMenuItem>{};
    TrayMenuConfig conf = storage.config;
    conf.css = ".fb-menu{}";
    conf.customPosition = TrayMenuPosition::Playback;

    auto breach = TryReplaceTrayZones(storage, std::move(zones), conf);
    EXPECT_FALSE(breach.ok);
    EXPECT_EQ(breach.field, "items");

    auto flat = FlatZoneItems(storage);
    ASSERT_EQ(flat.size(), 3u);
    EXPECT_EQ(flat[0].id, "oldTop");
    EXPECT_EQ(flat[1].id, "oldPlayback");
    EXPECT_EQ(flat[2].id, "oldBottom");
    EXPECT_TRUE(storage.config.css.empty());
    EXPECT_EQ(storage.config.customPosition, TrayMenuPosition::Top);
}

// tray.removeMenuItems reaches rows inside submenus, and a listed row inside a listed submenu
// counts too.
TEST(TaskbarTrayContractsTest, RemoveMenuItemsByIdReachesSubmenus) {
    TrayMenuItem deep = MakePlainItem("deep");
    TrayMenuItem inner = MakePlainItem("inner");
    inner.type = "submenu";
    inner.submenu = {deep, MakePlainItem("innerKeep")};
    TrayMenuItem outer = MakePlainItem("outer");
    outer.type = "submenu";
    outer.submenu = {MakePlainItem("child"), inner};
    TrayMenuItem gone = MakePlainItem("gone");
    gone.type = "submenu";
    gone.submenu = {MakePlainItem("goneChild")};
    std::vector<TrayMenuItem> zone = {MakePlainItem("top"), outer, gone};

    EXPECT_EQ(RemoveTrayMenuItemsById(zone, {"child", "deep", "gone", "goneChild", "missing"}), 4);

    ASSERT_EQ(zone.size(), 2u);
    EXPECT_EQ(zone[0].id, "top");
    ASSERT_EQ(zone[1].id, "outer");
    ASSERT_EQ(zone[1].submenu.size(), 1u);
    EXPECT_EQ(zone[1].submenu[0].id, "inner");
    ASSERT_EQ(zone[1].submenu[0].submenu.size(), 1u);
    EXPECT_EQ(zone[1].submenu[0].submenu[0].id, "innerKeep");
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
    // The public field must never be able to declare a system route. Both
    // system built-ins stay exclusive to the exact reserved-id allowlist.
    EXPECT_FALSE(PlaybackActionFromString("show-main-window").has_value());
    EXPECT_FALSE(PlaybackActionFromString("show").has_value());
    EXPECT_FALSE(PlaybackActionFromString("").has_value());
    EXPECT_FALSE(PlaybackActionFromString("Play-Pause").has_value());
    EXPECT_FALSE(PlaybackActionFromString("play_pause").has_value());
    EXPECT_FALSE(PlaybackActionFromString("_pb_next").has_value());
    EXPECT_FALSE(PlaybackActionFromString("nonsense").has_value());
}

// Round-trip inverse: only the four playback actions have a public token; None
// and the system built-ins (Exit / ShowMainWindow) map to empty so getMenuItems
// never surfaces a synthetic token for a natively-routed system item.
TEST(TaskbarTrayContractsTest, PlaybackActionToStringRoundTrips) {
    using namespace menu_action;
    EXPECT_STREQ(PlaybackActionToString(Builtin::PlayPause), "play-pause");
    EXPECT_STREQ(PlaybackActionToString(Builtin::Previous),  "previous");
    EXPECT_STREQ(PlaybackActionToString(Builtin::Next),      "next");
    EXPECT_STREQ(PlaybackActionToString(Builtin::Stop),      "stop");
    EXPECT_STREQ(PlaybackActionToString(Builtin::None),      "");
    EXPECT_STREQ(PlaybackActionToString(Builtin::Exit),      "");
    EXPECT_STREQ(PlaybackActionToString(Builtin::ShowMainWindow), "");
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
