#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <functional>
#include <optional>

// One segment of a "segmented" rich item (webview backend only): an inline
// single-select control. Each segment shows its inline SVG icon when present,
// otherwise its label; a disabled segment is greyed out and cannot be picked.
struct TraySegment {
    std::string label;
    std::string iconSvgViewBox;
    std::string iconSvgContent;
    bool enabled = true;
};

struct TrayMenuItem {
    std::string id;
    std::string label;
    // "normal" | "separator" | "checkbox" | "submenu" plus the webview-only rich
    // kinds "nowplaying" | "rating" | "slider" | "segmented" (degraded on the
    // native backend; segmented degrades to a plain text item).
    std::string type;
    bool enabled = true;
    bool visible = true;
    bool checked = false;
    std::string icon;   // base64 or empty

    // Inline monochrome SVG icon (webview backend only): viewBox + inner markup
    // (paths/shapes). Rendered as <svg viewBox=...>content</svg> with
    // fill:currentColor so it follows the menu text colour; the native
    // TrackPopupMenu backend ignores it (text-only).
    std::string iconSvgViewBox;
    std::string iconSvgContent;

    // Rich-item payload. Only the self-drawn (webview) backend renders these;
    // the native TrackPopupMenu backend degrades them (see TrayIcon::BuildMenu).
    std::string cover;       // nowplaying: album-art data URL or raw base64
    std::string title;       // nowplaying: primary line (track title)
    std::string subtitle;    // nowplaying: secondary line (artist / album)
    int value = 0;           // rating: 0-5 stars; slider: current value in [min,max];
                             // segmented: zero-based index of the selected segment
    int minValue = 0;        // slider: range minimum
    int maxValue = 100;      // slider: range maximum

    // segmented: the inline single-select options (webview backend only).
    std::vector<TraySegment> segments;

    std::vector<TrayMenuItem> submenu;
};

// Menu zones for customPosition.
enum class TrayMenuPosition {
    Top = 0,
    Playback = 1,
    Bottom = 2,
};

// Render backend for the tray context menu.
// Native  = Win32 TrackPopupMenu (default, backward compatible).
// WebView = self-drawn overlay via MenuOverlayHost (owner-mode routing).
enum class TrayMenuRender {
    Native = 0,
    WebView = 1,
};

struct TrayMenuConfig {
    bool showPlaybackControls = true;
    bool showSystemItems = true;
    TrayMenuPosition customPosition = TrayMenuPosition::Top;
    TrayMenuRender render = TrayMenuRender::Native;
    // When true, nowplaying items get any empty field (cover/title/subtitle)
    // auto-filled from the current track at right-click time (frontend-first,
    // backend-fallback). cover auto-fill is webview-only; the native degrade
    // fills text (title/artist) only.
    bool autoNowPlaying = false;

    // Frontend style takeover for the self-drawn (webview) tray menu only
    // (STYLING_TAKEOVER_DESIGN S-CSS). css is injected into the overlay's
    // <style id="fb-user">; cssReplace=true switches to replace mode (default
    // styles disabled, only the user CSS + protected structural layer remain),
    // false (default) keeps override/append on top of the built-in styles.
    // Ignored by the native TrackPopupMenu backend.
    std::string css;
    bool cssReplace = false;

    // DWM system backdrop for the self-drawn (webview) tray menu only. Shares the
    // main/popup window effect vocabulary (none/mica/mica-alt/acrylic -> DWMSBT
    // 1/2/4/3). Default "acrylic" = transient-surface material (correct for a
    // pop-up menu); applied per-show via PopupWindow::UpdateBackdropPolicy.
    // backdropDarkMode = backdrop dark tint (false follows a light theme).
    // Ignored by the native TrackPopupMenu backend.
    std::string backdrop = "acrylic";
    bool backdropDarkMode = true;

    // Exit (fade-out) animation for the self-drawn (webview) tray menu only
    // (opt-in). 0 (default) = hide immediately on close, no animation (zero
    // regression); >0 = wait this many milliseconds (clamped to 0..1000) for the
    // renderer to play the exit transition before hiding. Set it to ~your
    // `#menu.out` transition duration. Ignored by the native backend; the
    // replaced / timeout close paths always hide immediately regardless.
    int closeAnimationMs = 0;
};

class TrayIcon {
public:
    static constexpr UINT kCallbackMessage = WM_APP + 0x70;

    static TrayIcon& GetInstance();

    bool Create(HWND hwnd, HICON hIcon, const char* tooltip);
    void Destroy();
    bool IsCreated() const { return m_created; }

    /**
     * Re-register the icon after Explorer (the shell) has restarted.
     * Called by MainWindow when the TaskbarCreated message arrives.
     * No-op if the tray has not been created at least once before.
     */
    void RecreateAfterShellRestart();

    bool SetIcon(HICON hIcon);
    bool SetTooltip(const char* tooltip);

    bool ShowBalloon(const char* title, const char* message, DWORD iconType = NIIF_INFO);

    // Full menu setter. items go into customPosition zone.
    void SetContextMenu(const std::vector<TrayMenuItem>& items);
    void SetContextMenuConfig(const TrayMenuConfig& config) { m_menuConfig = config; }
    const TrayMenuConfig& GetContextMenuConfig() const { return m_menuConfig; }

    // Incremental menu management.
    void AppendMenuItems(const std::vector<TrayMenuItem>& items, TrayMenuPosition position);
    int  RemoveMenuItems(const std::vector<std::string>& ids);  // returns count removed
    void ClearMenuItems(TrayMenuPosition position);
    void ClearAllMenuItems();
    std::vector<TrayMenuItem> GetMenuItems() const;  // flat list, top->playback->bottom

    // Mutate a single item's checked/enabled state in place (searched across all
    // zones, recursive into submenus). Returns true if the id was found. Native
    // menus are rebuilt from m_zones on each right-click, so the change is
    // reflected on the NEXT open without the frontend re-sending the whole menu
    // via setContextMenu (which is a full-zone replace).
    bool SetMenuItemState(const std::string& id,
                          std::optional<bool> checked,
                          std::optional<bool> enabled);

    void SetMinimizeToTray(bool enabled) { m_minimizeToTray = enabled; }
    void SetCloseToTray(bool enabled) { m_closeToTray = enabled; }
    bool GetMinimizeToTray() const { return m_minimizeToTray; }
    bool GetCloseToTray() const { return m_closeToTray; }

    LRESULT HandleTrayCallback(WPARAM wParam, LPARAM lParam);

    using ClickCallback = std::function<void(int button, int x, int y)>;
    using DoubleClickCallback = std::function<void(int x, int y)>;
    using MenuItemCallback = std::function<void(const std::string& id)>;
    // Fired by rich items (rating / slider) whose value changed without closing
    // the menu. Routed to tray:menuItemClicked with an extra `value` field.
    using MenuItemValueCallback = std::function<void(const std::string& id, int value)>;
    using BeforeMenuCallback = std::function<void(int x, int y)>;
    void SetClickCallback(ClickCallback cb) { m_clickCb = std::move(cb); }
    void SetDoubleClickCallback(DoubleClickCallback cb) { m_doubleClickCb = std::move(cb); }
    void SetMenuItemCallback(MenuItemCallback cb) { m_menuItemCb = std::move(cb); }
    void SetMenuItemValueCallback(MenuItemValueCallback cb) { m_menuItemValueCb = std::move(cb); }
    void SetBeforeMenuCallback(BeforeMenuCallback cb) { m_beforeMenuCb = std::move(cb); }

    UINT GetCallbackMessage() const { return m_callbackMsg; }

private:
    TrayIcon() = default;
    ~TrayIcon() { Destroy(); }
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    static LRESULT CALLBACK MessageWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool EnsureMessageWindow(HWND ownerHwnd);
    bool RegisterMessageWindowClass();

    HWND m_ownerHwnd = nullptr;
    HWND m_hwnd = nullptr;
    bool m_created = false;
    bool m_minimizeToTray = false;
    bool m_closeToTray = false;
    UINT m_callbackMsg = 0;

    // Cached create parameters so RecreateAfterShellRestart can re-register
    // the icon after explorer.exe restart (the Shell drops every NIM_ADD
    // registration when it crashes; TaskbarCreated is broadcast on restart).
    HICON m_cachedIcon = nullptr;
    std::string m_cachedTooltip;

    // Three menu zones: top, playback, bottom.
    std::vector<TrayMenuItem> m_zones[3];
    TrayMenuConfig m_menuConfig;
    std::map<int, std::string> m_menuIdMap;
    // Native-backend command id -> (richItemId, value) for the degraded
    // rating/slider submenus. Resolved before m_menuIdMap so a value-bearing
    // pick routes through RouteValueChanged instead of RouteSelectedId.
    std::map<int, std::pair<std::string, int>> m_menuValueMap;

    ClickCallback m_clickCb;
    DoubleClickCallback m_doubleClickCb;
    MenuItemCallback m_menuItemCb;
    MenuItemValueCallback m_menuItemValueCb;
    BeforeMenuCallback m_beforeMenuCb;

    HMENU BuildMenu(const std::vector<TrayMenuItem>& items, int& cmdId);
    std::vector<TrayMenuItem> BuildPlaybackDefaults() const;
    std::vector<TrayMenuItem> BuildSystemDefaults() const;
    std::vector<TrayMenuItem> ComposeMenu() const;   // 三区组装（native + webview 共用）
    void RouteSelectedId(const std::string& id);     // 选中路由（_pb_/_sys_/回调，两路共用）
    void RouteValueChanged(const std::string& id, int value);  // 富控件值变更路由（两路共用）
    void ShowContextMenu();
};
