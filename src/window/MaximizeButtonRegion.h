// MaximizeButtonRegion.h - the page-drawn maximize button that the main window
// answers as HTMAXBUTTON, so Windows 11 offers Snap layouts when it is hovered.
#pragma once

#include <optional>

namespace maximize_button_region {

// Physical pixels, relative to the main window's client area.
struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Scales a rectangle the page gave in CSS pixels by scale, the page's
// devicePixelRatio (window DPI / 96 times the WebView zoom). The CSS values
// arrive already truncated to integers, as for the drag rectangles. nullopt when
// the scaled width or height is not positive, which the caller treats as "no
// button".
std::optional<Rect> ToPhysical(int x, int y, int width, int height, double scale);

// Half-open on the right and bottom edges, like the drag rectangles.
bool Contains(const Rect& rect, int clientX, int clientY);

// The window state that decides whether the rectangle is answered as the
// maximize button at all.
struct Conditions {
    bool snapLayoutsSupported = false;  // Windows 11 or later
    bool frameless = false;             // with a system title bar Windows draws its own button
    bool resizable = false;             // a window that cannot be resized has nothing to maximize
    bool fullscreen = false;            // full screen has no maximize or restore
    bool webViewForwarding = false;     // the WebView takes the mouse input the host forwards
};

// Whether a point inside the rectangle gets HTMAXBUTTON. Answering it takes the
// mouse away from the page, so the host must be able to hand that input back to
// the WebView; when any condition fails the button stays ordinary page content.
bool ShouldAnswerMaximizeButton(const Conditions& conditions);

// Windows 11 (build 22000) or later: the systems where hovering a maximize button
// offers Snap layouts. Earlier systems gain nothing from HTMAXBUTTON, so there
// the button is left to the page. Read once per process.
bool SnapLayoutsSupported() noexcept;

}  // namespace maximize_button_region
