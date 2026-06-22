#include "pch.h"
#include "window/TaskbarTrayContracts.h"

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

// rating/slider keep the menu open on value change; nowplaying and ordinary
// items select and close like a normal click.
TEST(TaskbarTrayContractsTest, OnlyValueControlsKeepMenuOpen) {
    EXPECT_TRUE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("rating"));
    EXPECT_TRUE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("slider"));
    EXPECT_FALSE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("nowplaying"));
    EXPECT_FALSE(taskbar_tray_contracts::RichTrayItemKeepsMenuOpen("normal"));
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
