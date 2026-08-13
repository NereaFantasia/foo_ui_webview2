// test_menu_overlay_anchor.cpp - L1 tests for the ContentSized anchor policy,
// the shared css byte cap and the value-control resolution the public
// menu:valueChanged event is built from.
//
// These drive the SAME headers the overlay host and the API layer include, so a
// regression in product code fails here instead of only on screen.
#include "pch.h"
#include "window/MenuOverlayGeometry.h"
#include "window/MenuResourceLimits.h"
#include "window/MenuTokenTable.h"
#include "window/TrayIcon.h"

using json = nlohmann::json;
using namespace menu_overlay_geometry;

namespace {

// Primary monitor and a secondary monitor placed to the left of it, which is
// the only way a negative-origin rectangle reaches the placement math.
constexpr Rect kPrimary{0, 0, 1920, 1080};
constexpr Rect kLeftMonitor{-1920, 0, 1920, 1080};

// The placement that used to be inlined in MenuOverlayHost::OnContentMeasured.
// Kept as an independent oracle: the tray path must stay bit-identical after
// the extraction, so any behavioural drift in "bottomUp" fails immediately.
Point LegacyBottomUpOrigin(Point anchor, int virtualW, int virtualH, const Rect& full) {
    const int right = full.x + full.w;
    int x = anchor.x;
    int y = anchor.y - virtualH;
    if (x + virtualW > right) x = right - virtualW;
    if (x < full.x) x = full.x;
    if (y < full.y) y = full.y;
    return Point{x, y};
}

}  // namespace

// ===========================================================================
// bottomUp - tray behaviour is frozen
// ===========================================================================

TEST(MenuOverlayAnchorTest, BottomUpMatchesLegacyTrayPlacementAcrossGrid) {
    for (const Rect& full : {kPrimary, kLeftMonitor}) {
        for (const int anchorX : {full.x, full.x + 5, full.x + 900, full.x + full.w - 8, full.x + full.w}) {
            for (const int anchorY : {full.y + 1, full.y + 240, full.y + full.h - 40, full.y + full.h}) {
                for (const int w : {1, 240, 560, 2400}) {
                    for (const int h : {1, 310, 1100}) {
                        const Point expected =
                            LegacyBottomUpOrigin(Point{anchorX, anchorY}, w, h, full);
                        const Point actual = ComputeContentAnchorOrigin(
                            Point{anchorX, anchorY}, w, h, full, AnchorPolicy::BottomUp,
                            /*dropAlignLeft=*/false);
                        EXPECT_EQ(actual.x, expected.x)
                            << "anchor=(" << anchorX << "," << anchorY << ") size=" << w << "x" << h;
                        EXPECT_EQ(actual.y, expected.y)
                            << "anchor=(" << anchorX << "," << anchorY << ") size=" << w << "x" << h;
                    }
                }
            }
        }
    }
}

TEST(MenuOverlayAnchorTest, BottomUpIgnoresDropAlignment) {
    // The tray anchor is an icon, not a cursor: the left-handed system setting
    // must not move a menu that is already pinned to the tray corner.
    const Point right = ComputeContentAnchorOrigin(Point{1000, 1040}, 240, 300, kPrimary,
                                                   AnchorPolicy::BottomUp, false);
    const Point left = ComputeContentAnchorOrigin(Point{1000, 1040}, 240, 300, kPrimary,
                                                  AnchorPolicy::BottomUp, true);
    EXPECT_EQ(right.x, 1000);
    EXPECT_EQ(right.y, 740);   // bottom edge sits on the anchor
    EXPECT_EQ(left.x, right.x);
    EXPECT_EQ(left.y, right.y);
}

// ===========================================================================
// cursor - standard context-menu semantics
// ===========================================================================

TEST(MenuOverlayAnchorTest, CursorDropsDownAndRightFromTheCursor) {
    const Point origin = ComputeContentAnchorOrigin(Point{400, 300}, 240, 300, kPrimary,
                                                    AnchorPolicy::Cursor, false);
    EXPECT_EQ(origin.x, 400);   // top-left corner is the cursor
    EXPECT_EQ(origin.y, 300);
}

TEST(MenuOverlayAnchorTest, CursorFlipsUpWhenBottomOverflows) {
    const Point origin = ComputeContentAnchorOrigin(Point{400, 1000}, 240, 300, kPrimary,
                                                    AnchorPolicy::Cursor, false);
    EXPECT_EQ(origin.x, 400);
    EXPECT_EQ(origin.y, 700);   // flipped: bottom edge lands on the cursor
}

TEST(MenuOverlayAnchorTest, CursorFlipsLeftWhenRightOverflows) {
    const Point origin = ComputeContentAnchorOrigin(Point{1800, 200}, 240, 300, kPrimary,
                                                    AnchorPolicy::Cursor, false);
    EXPECT_EQ(origin.x, 1560);  // flipped: right edge lands on the cursor
    EXPECT_EQ(origin.y, 200);
}

TEST(MenuOverlayAnchorTest, CursorFlipsBothAxesInTheBottomRightCorner) {
    const Point origin = ComputeContentAnchorOrigin(Point{1900, 1060}, 240, 300, kPrimary,
                                                    AnchorPolicy::Cursor, false);
    EXPECT_EQ(origin.x, 1660);
    EXPECT_EQ(origin.y, 760);
}

TEST(MenuOverlayAnchorTest, CursorLeftHandedPrefersLeftThenFlipsBack) {
    const Point leftward = ComputeContentAnchorOrigin(Point{1000, 200}, 240, 300, kPrimary,
                                                      AnchorPolicy::Cursor, true);
    EXPECT_EQ(leftward.x, 760);   // right edge on the cursor
    EXPECT_EQ(leftward.y, 200);

    // Not enough room on the left: fall back to the normal rightward drop
    // instead of clamping to the screen edge under the cursor.
    const Point flippedBack = ComputeContentAnchorOrigin(Point{100, 200}, 240, 300, kPrimary,
                                                         AnchorPolicy::Cursor, true);
    EXPECT_EQ(flippedBack.x, 100);
    EXPECT_EQ(flippedBack.y, 200);
}

TEST(MenuOverlayAnchorTest, CursorClampsWhenNeitherSideFits) {
    // Menu larger than the monitor in both axes: no flip can help, so the
    // top-left corner wins and the panel stays inside the monitor rectangle.
    const Rect tinyMonitor{0, 0, 800, 600};   // `small` is a Win32 macro (RpcNdr.h)
    const Point origin = ComputeContentAnchorOrigin(Point{700, 500}, 1000, 900, tinyMonitor,
                                                    AnchorPolicy::Cursor, false);
    EXPECT_EQ(origin.x, 0);
    EXPECT_EQ(origin.y, 0);
}

TEST(MenuOverlayAnchorTest, CursorStaysInsideANegativeOriginMonitor) {
    const Point origin = ComputeContentAnchorOrigin(Point{-100, 900}, 240, 300, kLeftMonitor,
                                                    AnchorPolicy::Cursor, false);
    EXPECT_EQ(origin.x, -340);  // right edge on the cursor, still on that monitor
    EXPECT_EQ(origin.y, 600);   // flipped up at the bottom edge

    const Point clamped = ComputeContentAnchorOrigin(Point{-1910, 100}, 240, 300, kLeftMonitor,
                                                     AnchorPolicy::Cursor, true);
    EXPECT_EQ(clamped.x, -1910);  // left flip would cross the monitor edge
    EXPECT_EQ(clamped.y, 100);
}

// ===========================================================================
// cursor + submenu slot - the root edge stays on the cursor when flipping
// ===========================================================================

namespace {

// root 240x300 + first-level submenu 200x200 on a 1920x1080 work area:
// rootSlotW=240, subSlotW=200, viewportW=440, rootTop=0, rootVisibleH=300.
Placement PlacementWithSubmenu() {
    MeasureReport report;
    report.root = Size{240, 300};
    report.submenu = Size{200, 200};
    const auto placement = ComputePlacement(report, /*hasFirstLevelSubmenu=*/true, 1920, 1080);
    EXPECT_TRUE(placement.has_value());
    return *placement;
}

}  // namespace

TEST(MenuOverlayAnchorTest, CursorFlipWithSubmenuKeepsRootRightEdgeOnCursor) {
    // 曾按整张画布宽(440)平移：根右缘离光标差一个子槽宽(200)。修复后根右缘=光标，
    // 子菜单槽换到根左侧。
    Placement placement = PlacementWithSubmenu();
    const auto anchored = ComputeAnchoredOrigin(Point{1800, 200}, placement, kPrimary,
                                                AnchorPolicy::Cursor, false);
    EXPECT_TRUE(anchored.subOnLeft);
    placement.subOnLeft = anchored.subOnLeft;
    const Rect root = RootPanelRect(placement);
    EXPECT_EQ(root.x, placement.subSlotW);                       // 根被推到子槽右侧
    EXPECT_EQ(anchored.origin.x + root.x + root.w, 1800);        // 根右缘 = 光标
    EXPECT_EQ(anchored.origin.y, 200);
}

TEST(MenuOverlayAnchorTest, CursorKeepsRootOnCursorWhenOnlySlotOverflows) {
    // 根自身放得下、只有子槽越界：根保持贴光标，子菜单换到左侧（原生语义）。
    Placement placement = PlacementWithSubmenu();
    const auto anchored = ComputeAnchoredOrigin(Point{1600, 200}, placement, kPrimary,
                                                AnchorPolicy::Cursor, false);
    EXPECT_TRUE(anchored.subOnLeft);
    placement.subOnLeft = anchored.subOnLeft;
    EXPECT_EQ(anchored.origin.x + RootPanelRect(placement).x, 1600);   // 根左缘 = 光标
}

TEST(MenuOverlayAnchorTest, CursorMidScreenWithSubmenuKeepsCurrentLayout) {
    Placement placement = PlacementWithSubmenu();
    const auto anchored = ComputeAnchoredOrigin(Point{400, 300}, placement, kPrimary,
                                                AnchorPolicy::Cursor, false);
    EXPECT_FALSE(anchored.subOnLeft);
    EXPECT_EQ(anchored.origin.x, 400);   // 现状形态：根在左、槽在右，根左缘=光标
    EXPECT_EQ(anchored.origin.y, 300);
}

TEST(MenuOverlayAnchorTest, ClampSubmenuRectHugsRootEdgeOnBothSides) {
    Placement placement = PlacementWithSubmenu();
    const Rect requested{placement.rootSlotW, 0, 150, 200};

    const auto rightSide = ClampSubmenuRect(placement, requested);
    ASSERT_TRUE(rightSide.has_value());
    EXPECT_EQ(rightSide->x, placement.rootSlotW);                // 面板左缘贴根右缘

    placement.subOnLeft = true;
    const auto leftSide = ClampSubmenuRect(placement, requested);
    ASSERT_TRUE(leftSide.has_value());
    EXPECT_EQ(leftSide->x + leftSide->w, placement.subSlotW);    // 面板右缘贴根左缘
    EXPECT_EQ(leftSide->x, placement.subSlotW - 150);
}

TEST(MenuOverlayAnchorTest, UnknownAnchorPolicyFallsBackToBottomUp) {
    EXPECT_EQ(ParseAnchorPolicy("cursor"), AnchorPolicy::Cursor);
    EXPECT_EQ(ParseAnchorPolicy("bottomUp"), AnchorPolicy::BottomUp);
    EXPECT_EQ(ParseAnchorPolicy(""), AnchorPolicy::BottomUp);
    EXPECT_EQ(ParseAnchorPolicy("Cursor"), AnchorPolicy::BottomUp);   // exact match only
    EXPECT_EQ(ParseAnchorPolicy("topDown"), AnchorPolicy::BottomUp);
}

// ===========================================================================
// css byte cap - one helper shared by tray config and menu.show
// ===========================================================================

TEST(MenuResourceCssCapTest, AcceptsExactLimitAndRejectsOneMoreByte) {
    const std::string atLimit(static_cast<size_t>(menu_limits::kMaxCssBytes), 'x');
    EXPECT_TRUE(menu_limits::ValidateCssBytes(atLimit).ok);
    EXPECT_TRUE(menu_limits::ValidateCssBytes("").ok);

    const std::string overLimit(static_cast<size_t>(menu_limits::kMaxCssBytes) + 1, 'x');
    const auto breach = menu_limits::ValidateCssBytes(overLimit);
    EXPECT_FALSE(breach.ok);
    EXPECT_EQ(breach.field, "css");
    EXPECT_EQ(breach.limit, menu_limits::kMaxCssBytes);
    EXPECT_EQ(breach.actual, menu_limits::kMaxCssBytes + 1);
}

TEST(MenuResourceCssCapTest, TrayAndShowPreflightReportTheSameBreach) {
    const std::string overLimit(static_cast<size_t>(menu_limits::kMaxCssBytes) + 1, 'x');
    const auto shared = menu_limits::ValidateCssBytes(overLimit);
    const auto tray = ValidateTrayMenuResources({}, overLimit);

    EXPECT_EQ(tray.ok, shared.ok);
    EXPECT_EQ(tray.field, shared.field);
    EXPECT_EQ(tray.limit, shared.limit);
    EXPECT_EQ(tray.actual, shared.actual);

    const auto details = menu_limits::DetailsJson(shared);
    EXPECT_EQ(details["field"], "css");
    EXPECT_EQ(details["limit"], menu_limits::kMaxCssBytes);
    EXPECT_EQ(details["actual"], menu_limits::kMaxCssBytes + 1);
}

// ===========================================================================
// menu:valueChanged payload source
// ===========================================================================

// The public event carries {menuId, itemId, value}. `itemId` is never the
// opaque token: it is exactly what MenuTokenTable resolves for an accepted
// value change, which is the same resolution the owner-mode (tray) sink gets.
// The broadcast itself lives in MenuOverlayHost.cpp, which needs Win32 +
// foobar2000 and is out of this project's link scope; the resolution contract
// below is the testable half.
TEST(MenuValueChangedContractTest, AcceptedValueResolvesToThePublicItemId) {
    json items = json::array();
    {
        json rating;
        rating["id"] = "rate";
        rating["type"] = "rating";
        items.push_back(rating);
    }
    {
        json disabled;
        disabled["id"] = "locked";
        disabled["type"] = "rating";
        disabled["enabled"] = false;
        items.push_back(disabled);
    }

    MenuTokenTable table;
    auto next = std::make_shared<int>(0);
    ASSERT_TRUE(table.Rebuild(items, [next]() -> std::optional<std::string> {
        return "t" + std::to_string((*next)++);
    }));

    const std::string ratingToken = items[0]["_token"].get<std::string>();
    const std::string lockedToken = items[1]["_token"].get<std::string>();

    const auto accepted = table.ResolveValue(ratingToken, 3);
    ASSERT_TRUE(accepted.has_value());
    EXPECT_EQ(*accepted, "rate");
    EXPECT_NE(*accepted, ratingToken);   // the token must never reach the event

    EXPECT_FALSE(table.ResolveValue(ratingToken, 9).has_value());     // out of range
    EXPECT_FALSE(table.ResolveValue(lockedToken, 3).has_value());     // disabled item
    EXPECT_FALSE(table.ResolveValue("unknown-token", 3).has_value());

    // A rejected value change emits nothing at all, in either mode.
    EXPECT_FALSE(table.ResolveSelect(ratingToken).has_value());       // value control, not a select
}

TEST(MenuValueChangedContractTest, IdLessItemStillReportsAnEmptyPublicId) {
    // menu.show items may omit `id`. The event then carries an empty itemId
    // rather than leaking the token, matching menu:select parity.
    json items = json::array();
    json slider;
    slider["type"] = "slider";
    slider["min"] = 0;
    slider["max"] = 100;
    items.push_back(slider);

    MenuTokenTable table;
    ASSERT_TRUE(table.Rebuild(items, []() -> std::optional<std::string> {
        return std::string("only-token");
    }));

    const auto resolved = table.ResolveValue("only-token", 42);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_TRUE(resolved->empty());
}
