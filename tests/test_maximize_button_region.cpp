// test_maximize_button_region.cpp - when the main window answers the page-drawn
// maximize button as HTMAXBUTTON.
//
// Answering it takes the mouse away from the page, so every condition that
// cannot hand the input back, or that has nothing to maximize, must leave the
// button as ordinary page content.
#include "pch.h"
#include "../src/window/MaximizeButtonRegion.h"

using maximize_button_region::Conditions;
using maximize_button_region::Contains;
using maximize_button_region::Rect;
using maximize_button_region::ShouldAnswerMaximizeButton;
using maximize_button_region::ToPhysical;

namespace {

Conditions AllMet() {
    Conditions c;
    c.snapLayoutsSupported = true;
    c.frameless = true;
    c.resizable = true;
    c.fullscreen = false;
    c.webViewForwarding = true;
    return c;
}

}  // namespace

// --- Scaling ---

TEST(MaximizeButtonRegion, ScalesByDevicePixelRatio) {
    const auto rect = ToPhysical(1000, 0, 46, 32, 1.5);
    ASSERT_TRUE(rect.has_value());
    EXPECT_EQ(rect->x, 1500);
    EXPECT_EQ(rect->y, 0);
    EXPECT_EQ(rect->width, 69);
    EXPECT_EQ(rect->height, 48);
}

TEST(MaximizeButtonRegion, EmptyRectangleMeansNoButton) {
    EXPECT_FALSE(ToPhysical(10, 10, 0, 32, 1.0).has_value());
    EXPECT_FALSE(ToPhysical(10, 10, 46, -1, 1.0).has_value());
}

TEST(MaximizeButtonRegion, ScaleThatShrinksBelowOnePixelMeansNoButton) {
    EXPECT_FALSE(ToPhysical(0, 0, 1, 1, 0.5).has_value());
}

// --- Containment ---

TEST(MaximizeButtonRegion, ContainsIsHalfOpen) {
    Rect rect;
    rect.x = 100;
    rect.y = 0;
    rect.width = 46;
    rect.height = 32;
    EXPECT_TRUE(Contains(rect, 100, 0));
    EXPECT_TRUE(Contains(rect, 145, 31));
    EXPECT_FALSE(Contains(rect, 146, 10));
    EXPECT_FALSE(Contains(rect, 120, 32));
    EXPECT_FALSE(Contains(rect, 99, 10));
}

// --- Conditions ---

TEST(MaximizeButtonRegion, AnsweredWhenEveryConditionHolds) {
    EXPECT_TRUE(ShouldAnswerMaximizeButton(AllMet()));
}

TEST(MaximizeButtonRegion, NotAnsweredBeforeWindows11) {
    Conditions c = AllMet();
    c.snapLayoutsSupported = false;
    EXPECT_FALSE(ShouldAnswerMaximizeButton(c));
}

TEST(MaximizeButtonRegion, NotAnsweredWithASystemTitleBar) {
    Conditions c = AllMet();
    c.frameless = false;
    EXPECT_FALSE(ShouldAnswerMaximizeButton(c));
}

TEST(MaximizeButtonRegion, NotAnsweredForAFixedSizeWindow) {
    Conditions c = AllMet();
    c.resizable = false;
    EXPECT_FALSE(ShouldAnswerMaximizeButton(c));
}

TEST(MaximizeButtonRegion, NotAnsweredInFullScreen) {
    Conditions c = AllMet();
    c.fullscreen = true;
    EXPECT_FALSE(ShouldAnswerMaximizeButton(c));
}

TEST(MaximizeButtonRegion, NotAnsweredWhenInputCannotReachThePage) {
    Conditions c = AllMet();
    c.webViewForwarding = false;
    EXPECT_FALSE(ShouldAnswerMaximizeButton(c));
}
