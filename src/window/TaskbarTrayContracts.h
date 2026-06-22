#pragma once

#include <cstddef>
#include <string>

namespace taskbar_tray_contracts {

inline bool ShouldUseDedicatedTrayMessageWindow() {
    return true;
}

inline bool CanInstallUserThumbnailButtons(bool usingDefaultButtons,
                                           bool buttonsAdded,
                                           std::size_t buttonCount) {
    // The only hard limit is Windows' 7-slot thumbnail toolbar cap. Within that
    // limit a user setThumbnailButtons call is always serviceable:
    //   - usingDefaultButtons : replace the implicit default toolbar (first call)
    //   - buttonsAdded        : update the existing toolbar via ThumbBarUpdateButtons
    //   - neither             : fresh ThumbBarAddButtons
    // The previous `return !buttonsAdded` wrongly rejected every *repeat* call
    // once a custom toolbar had been installed (buttonsAdded && !usingDefault),
    // so the frontend could only set thumbnail buttons once. SetThumbnailButtons
    // already branches on m_buttonsAdded to pick Add vs Update, so this gate just
    // needs to enforce the slot cap.
    (void)usingDefaultButtons;
    (void)buttonsAdded;
    return buttonCount <= 7;
}

inline std::size_t ThumbnailToolbarSlotCount() {
    return 7;
}

// ── Rich tray menu items (webview self-drawn branch only) ────────────────
// The webview overlay renders rich controls (now-playing card / inline star
// rating / volume slider); the native TrackPopupMenu branch degrades them
// (see TrayIcon::BuildMenu). These predicates keep both branches and the
// contract tests in sync on what "rich" means.

inline bool IsRichTrayItemType(const std::string& type) {
    return type == "nowplaying" || type == "rating" || type == "slider"
        || type == "segmented";
}

// Inline menu icon (webview self-drawn branch only): an item carries a
// renderable monochrome SVG icon only when BOTH the viewBox and the SVG inner
// markup are present. The native TrackPopupMenu branch ignores icons entirely.
// Keeps the TrayItemToMenuJson passthrough and the contract test in sync.
inline bool TrayItemHasRenderableIconSvg(const std::string& viewBox,
                                         const std::string& content) {
    return !viewBox.empty() && !content.empty();
}

// Inline icons attach only to ordinary menu entries: not separators and not the
// rich kinds (nowplaying/rating/slider), which have their own layouts. Used by the
// webview JSON passthrough (TrayItemToMenuJson) and mirrored by the renderer's
// per-layer icon-column scan, so both gate the icon column the same way.
inline bool TrayItemKindRendersIcon(const std::string& type) {
    return type != "separator" && !IsRichTrayItemType(type);
}

// Now-playing smart fallback (generic; any frontend, local or streaming): the
// backend auto-fills a nowplaying field (cover / title / subtitle) only when
// config.autoNowPlaying is on AND the frontend left that field empty
// (frontend-first, backend-fallback). Keeps AutoFillNowPlaying and the test in
// sync.
inline bool TrayShouldAutoFillField(bool autoNowPlaying, bool frontendValuePresent) {
    return autoNowPlaying && !frontendValuePresent;
}

// Rich value controls (rating / slider) report value changes WITHOUT closing
// the menu; everything else (normal items and the now-playing card) selects
// and closes like an ordinary click.
inline bool RichTrayItemKeepsMenuOpen(const std::string& type) {
    return type == "rating" || type == "slider";
}

// Native degrade: a rating item becomes a "★1..N" submenu.
inline int RatingNativeLevelCount() {
    return 5;
}

// Native degrade: a slider item becomes a stepped submenu (inclusive of both
// endpoints), so SliderNativeStopCount stops span min..max.
inline int SliderNativeStopCount() {
    return 5;
}

// tray:menuItemClicked carries `value` for rich controls. Integral values
// (rating 0-5, rounded slider volume) broadcast as integers; non-integral
// values keep their fractional part. Returns true when `value` is integral.
inline bool TrayMenuValueIsIntegral(double value) {
    return value == static_cast<double>(static_cast<long long>(value));
}

}
