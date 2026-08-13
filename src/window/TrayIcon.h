#pragma once
#include "pch.h"
#include "window/MenuActionContract.h"
#include "window/MenuResourceLimits.h"
#include "utils/I18n.h"
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
    // True when the caller explicitly supplied `checked` (including false), set
    // type to "checkbox", or later called setMenuItemState with checked. Preserves
    // checkable identity so checked:false is not lost as "not a checkbox".
    bool checkable = false;
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
    // Slider-only axis. Exact "vertical" is vertical; missing /
    // "horizontal" / unknown → horizontal. Non-slider types ignore this field.
    // Native backend ignores orientation and keeps the stepped-submenu degrade.
    std::string orientation;

    // Caller-declared native playback action (public API field).
    // One of "play-pause" | "previous" | "next" | "stop", or empty. Unlike the
    // internal origin/builtinAction below, this IS parsed from setContextMenu
    // JSON and round-tripped by getMenuItems. Effective-zone composition stamps
    // a normal leaf that carries a valid value with Origin::BuiltinPlayback so
    // selection executes natively (reliable while the main page is suspended)
    // instead of firing tray:menuItemClicked. The API layer rejects invalid
    // values fail-loud, so a stored item only ever holds a valid token or "".
    std::string playbackAction;

    // segmented: the inline single-select options (webview backend only).
    std::vector<TraySegment> segments;

    std::vector<TrayMenuItem> submenu;

    // Internal routing identity (NOT a public API field): never parsed from
    // setContextMenu JSON and never serialized by getMenuItems / any event. A
    // frontend item starts as Origin::User; effective-menu composition may
    // promote only the exact tray compatibility ids `_sys_show` / `_sys_exit`.
    // Playback ids supplied by a frontend remain user actions. Routing never
    // uses a prefix.
    menu_action::Origin origin = menu_action::Origin::User;
    menu_action::Builtin builtinAction = menu_action::Builtin::None;
};

// Single action authority for the native command map and the WebView token
// transport. Effective-zone composition may stamp the exact `_sys_show` /
// `_sys_exit` compatibility items; malformed metadata always falls back to a
// user action.
inline menu_action::ResolvedAction ResolveTrayMenuItemAction(const TrayMenuItem& item) {
    return menu_action::NormalizeResolvedAction({
        item.id, item.origin, item.builtinAction
    });
}

inline menu_action::InternalActionFields TrayMenuItemActionFields(const TrayMenuItem& item) {
    return menu_action::MakeInternalActionFields(ResolveTrayMenuItemAction(item));
}

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

// WebView tray DOM structure mode.
// Flat  = default; root keeps direct-child .fb-item / separator DOM (legacy selectors).
// Zones = opt-in; emits .fb-zone[data-zone] wrappers for top/playback/bottom.
// Native backends ignore this field and always flatten from effective zones.
enum class TrayMenuLayoutMode {
    Flat = 0,
    Zones = 1,
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
    // css is injected into the overlay's
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

    // Opt-in WebView DOM layout mode. Default Flat preserves `#menu > .fb-item`.
    // Zones is WebView-only; native ignores it. Unknown JSON values coerce to Flat.
    TrayMenuLayoutMode layoutMode = TrayMenuLayoutMode::Flat;
};

// Compose the three tray zones (top / playback / bottom, each already carrying
// any injected built-in items) into one flat list, inserting a separator only
// between zones that have at least one VISIBLE item. A zone whose items are all
// hidden contributes neither a separator nor content, so no orphan separator
// survives: visible filtering precedes the separator decision (DESIGN 7.7 / 8.2).
// Both backends drop !visible items downstream (native BuildMenu skips them,
// webview TrayItemsToMenuJson filters them), so the separator gate must match to
// avoid a leading / trailing / doubled divider. Header-only (inline) so the
// separator contract is unit-testable without linking the Win32 TrayIcon.cpp.
inline std::vector<TrayMenuItem> ComposeTrayZones(const std::vector<TrayMenuItem>& top,
                                                  const std::vector<TrayMenuItem>& playback,
                                                  const std::vector<TrayMenuItem>& bottom) {
    std::vector<TrayMenuItem> composed;
    auto hasVisible = [](const std::vector<TrayMenuItem>& items) {
        for (const auto& it : items) if (it.visible) return true;
        return false;
    };
    auto appendZone = [&](const std::vector<TrayMenuItem>& items, bool addLeadingSeparator) {
        if (!hasVisible(items)) return;
        if (addLeadingSeparator && !composed.empty()) {
            TrayMenuItem sep; sep.type = "separator";
            composed.push_back(sep);
        }
        composed.insert(composed.end(), items.begin(), items.end());
    };
    appendZone(top, false);
    appendZone(playback, true);
    appendZone(bottom, true);
    return composed;
}

// Single effective-zone authority:
//   top
//   playback = builtin playback + user playback
//   bottom   = user bottom + builtin system
// Native / WebView flat / WebView zones all consume this structure.
struct EffectiveTrayZones {
    std::vector<TrayMenuItem> top;
    std::vector<TrayMenuItem> playback;
    std::vector<TrayMenuItem> bottom;

    // After BuildEffectiveTrayZones normalization, zones contain only visible
    // nodes (hidden descendants stripped; empty submenus degraded to leaves).
    bool HasAnyVisible() const {
        return !top.empty() || !playback.empty() || !bottom.empty();
    }
};

// Recursively drop !visible nodes before any backend consumes the tree
// A visible submenu parent whose children are all hidden is
// degraded to a normal leaf so native does not open an empty popup and the
// WebView does not keep an empty submenu array that would still look like a
// parent row.
inline std::vector<TrayMenuItem> NormalizeVisibleMenuItems(std::vector<TrayMenuItem> items) {
    std::vector<TrayMenuItem> out;
    out.reserve(items.size());
    for (auto& it : items) {
        if (!it.visible) continue;
        if (!it.submenu.empty()) {
            it.submenu = NormalizeVisibleMenuItems(std::move(it.submenu));
        }
        if (it.type == "submenu" && it.submenu.empty()) {
            it.type = "normal";
        }
        out.push_back(std::move(it));
    }
    return out;
}

// Single recursive slider normalization shared by storage / WebView / native /
// token index / getMenuItems:
//   - max < min → swap
//   - max == min → constant slider (display only; no value change)
//   - value clamped into the normalized range
//   - orientation: only exact "vertical" kept; else cleared (horizontal default)
//   - non-slider types drop orientation
//   - recurses into submenu
inline void NormalizeSliderMenuItems(std::vector<TrayMenuItem>& items) {
    for (auto& it : items) {
        if (it.type == "slider") {
            if (it.maxValue < it.minValue) {
                std::swap(it.minValue, it.maxValue);
            }
            if (it.value < it.minValue) it.value = it.minValue;
            if (it.value > it.maxValue) it.value = it.maxValue;
            if (it.orientation != "vertical") {
                // Keep explicit "horizontal"; unknown / empty → empty (horizontal).
                if (it.orientation != "horizontal") it.orientation.clear();
            }
        } else {
            it.orientation.clear();
        }
        if (!it.submenu.empty()) {
            NormalizeSliderMenuItems(it.submenu);
        }
    }
}

// Exact "vertical" only; everything else (including empty / horizontal / unknown)
// is horizontal. Non-slider callers should not rely on this.
inline bool TraySliderIsVertical(const TrayMenuItem& item) {
    return item.type == "slider" && item.orientation == "vertical";
}

// Constant slider: normalized max == min. Display the value; never emit changes.
inline bool TraySliderIsConstant(const TrayMenuItem& item) {
    return item.type == "slider" && item.maxValue == item.minValue;
}

// Built-in playback controls injected when TrayMenuConfig.showPlaybackControls
// is on. Each carries Origin::BuiltinPlayback + its Builtin action so routing is
// by trusted origin, not by the "_pb_" id prefix. The ids keep the historical
// "_pb_*" spelling for backward compatibility only — a user item with the same
// id is still Origin::User and never runs a built-in. Header-only so the origin
// stamping is unit-testable.
inline std::vector<TrayMenuItem> BuildPlaybackDefaultItems() {
    std::vector<TrayMenuItem> items;
    auto add = [&](const char* id, const char* label, menu_action::Builtin action) {
        TrayMenuItem m;
        m.id = id;
        m.label = label;
        m.type = "normal";
        m.enabled = true;
        m.visible = true;
        m.origin = menu_action::Origin::BuiltinPlayback;
        m.builtinAction = action;
        items.push_back(std::move(m));
    };
    // Ids stay stable across locales; only the display label follows the fb2k UI language.
    add("_pb_playPause", TRU("Play / Pause", "播放 / 暂停"), menu_action::Builtin::PlayPause);
    add("_pb_prev",      TRU("Previous Track", "上一首"),    menu_action::Builtin::Previous);
    add("_pb_next",      TRU("Next Track", "下一首"),        menu_action::Builtin::Next);
    add("_pb_stop",      TRU("Stop", "停止"),                menu_action::Builtin::Stop);
    return items;
}

// Built-in system items injected when TrayMenuConfig.showSystemItems is on.
// Origin::BuiltinSystem; the exact "_sys_show" / "_sys_exit" spellings are also
// tray-only public compatibility commands handled by promotion above.
//
// "_sys_show" (show main window) must be native for the same reason Exit is:
// hiding to tray applies put_IsVisible(FALSE) + TrySuspend to the main page, so
// a tray:menuItemClicked handler cannot run to call window.focus itself. A
// frontend-event route would be dead in exactly the state the item exists for.
// It is injected before Exit (show/restore first, quit last), matching the
// conventional Windows tray menu order.
inline std::vector<TrayMenuItem> BuildSystemDefaultItems() {
    std::vector<TrayMenuItem> items;
    auto add = [&](const char* id, const std::string& label, menu_action::Builtin action) {
        TrayMenuItem m;
        m.id = id;
        m.label = label;
        m.type = "normal";
        m.enabled = true;
        m.visible = true;
        m.origin = menu_action::Origin::BuiltinSystem;
        m.builtinAction = action;
        items.push_back(std::move(m));
    };
    // Ids stay stable across locales; only the display label follows the fb2k UI language.
    add("_sys_show", TRU("Show Main Window", "显示主窗口"),
        menu_action::Builtin::ShowMainWindow);
    add("_sys_exit", TRU("Exit foobar2000", "退出 foobar2000"),
        menu_action::Builtin::Exit);
    return items;
}

// Apply the exact tray reserved-id compatibility contract recursively. Public
// JSON cannot provide an origin or action; this function derives both from the
// fixed `_sys_show` / `_sys_exit` allowlist. Similar ids and all caller-supplied
// `_pb_*` ids remain Origin::User.
inline void PromoteReservedBuiltinItems(std::vector<TrayMenuItem>& items) {
    for (auto& item : items) {
        if (auto reserved = menu_action::ReservedBuiltinForPublicId(item.id)) {
            item.origin = reserved->origin;
            item.builtinAction = reserved->builtin;
        }
        if (!item.submenu.empty()) {
            PromoteReservedBuiltinItems(item.submenu);
        }
    }
}

// Stamp caller-declared native playback actions recursively. A
// normal leaf whose public `playbackAction` maps to a playback Builtin gains
// Origin::BuiltinPlayback so selection routes to ExecutePlayback natively,
// reliable while the main page is suspended, and never fires
// tray:menuItemClicked. The caller keeps full control of label / icon / id.
// Only normal leaves qualify: separators, submenus and rich controls
// (rating / slider / segmented / nowplaying) ignore the field. The API layer
// already rejected invalid tokens fail-loud, so a non-empty value here is a
// valid playback action; PlaybackActionFromString still gates defensively.
// Runs after reserved-id promotion and before default injection so the stamp
// is present when the token table indexes the internal action fields.
inline void PromoteDeclaredPlaybackItems(std::vector<TrayMenuItem>& items) {
    for (auto& item : items) {
        if (!item.submenu.empty()) {
            PromoteDeclaredPlaybackItems(item.submenu);
            continue;  // a submenu parent is not itself a selectable leaf
        }
        if (item.playbackAction.empty()) continue;
        if (item.type != "normal") continue;
        if (auto action = menu_action::PlaybackActionFromString(item.playbackAction)) {
            item.origin = menu_action::Origin::BuiltinPlayback;
            item.builtinAction = *action;
        }
    }
}

inline bool TrayMenuContainsPublicId(const std::vector<TrayMenuItem>& items,
                                     const std::string& publicId) {
    for (const auto& item : items) {
        if (item.id == publicId) return true;
        if (!item.submenu.empty() && TrayMenuContainsPublicId(item.submenu, publicId)) {
            return true;
        }
    }
    return false;
}

inline bool EffectiveTrayZonesContainPublicId(const EffectiveTrayZones& zones,
                                              const std::string& publicId) {
    return TrayMenuContainsPublicId(zones.top, publicId)
        || TrayMenuContainsPublicId(zones.playback, publicId)
        || TrayMenuContainsPublicId(zones.bottom, publicId);
}

inline EffectiveTrayZones BuildEffectiveTrayZones(
    const std::vector<TrayMenuItem>& top,
    const std::vector<TrayMenuItem>& playback,
    const std::vector<TrayMenuItem>& bottom,
    const TrayMenuConfig& config) {
    EffectiveTrayZones z;
    // Visibility normalization precedes reserved-id promotion and built-in
    // injection. A hidden user item therefore does not suppress a visible
    // configured default, while a visible exact reserved item becomes the one
    // authoritative presentation of that action.
    z.top = NormalizeVisibleMenuItems(top);
    z.playback = NormalizeVisibleMenuItems(playback);
    z.bottom = NormalizeVisibleMenuItems(bottom);
    PromoteReservedBuiltinItems(z.top);
    PromoteReservedBuiltinItems(z.playback);
    PromoteReservedBuiltinItems(z.bottom);

    // Caller-declared playbackAction is stamped after reserved-id promotion and
    // before default injection, so a declared leaf routes natively without
    // colliding with the exact `_sys_show` / `_sys_exit` compatibility contract.
    PromoteDeclaredPlaybackItems(z.top);
    PromoteDeclaredPlaybackItems(z.playback);
    PromoteDeclaredPlaybackItems(z.bottom);

    if (config.showPlaybackControls) {
        auto defaults = BuildPlaybackDefaultItems();
        z.playback.insert(z.playback.begin(),
                          std::make_move_iterator(defaults.begin()),
                          std::make_move_iterator(defaults.end()));
    }
    if (config.showSystemItems) {
        auto defaults = BuildSystemDefaultItems();
        for (auto& item : defaults) {
            if (!EffectiveTrayZonesContainPublicId(z, item.id)) {
                z.bottom.push_back(std::move(item));
            }
        }
    }
    return z;
}

inline std::vector<TrayMenuItem> FlattenEffectiveZones(const EffectiveTrayZones& z) {
    return ComposeTrayZones(z.top, z.playback, z.bottom);
}

// Flat list with per-node zone stamps for WebView flat DOM / tests.
// Inter-zone separators inherit the *last emitted* zone id (not a hardcoded
// neighbour name), so top→bottom with an empty playback zone tags the
// separator as "top".
struct ZoneTaggedTrayItem {
    TrayMenuItem item;
    std::string zone;
};

inline std::vector<ZoneTaggedTrayItem> FlattenEffectiveZonesTagged(const EffectiveTrayZones& z) {
    std::vector<ZoneTaggedTrayItem> out;
    const char* lastEmittedZone = nullptr;
    auto appendZone = [&](const char* zoneId, const std::vector<TrayMenuItem>& items) {
        if (items.empty()) return;
        if (lastEmittedZone != nullptr) {
            TrayMenuItem sep;
            sep.type = "separator";
            sep.visible = true;
            out.push_back(ZoneTaggedTrayItem{ std::move(sep), lastEmittedZone });
        }
        for (const auto& it : items) {
            out.push_back(ZoneTaggedTrayItem{ it, zoneId });
        }
        lastEmittedZone = zoneId;
    };
    appendZone("top", z.top);
    appendZone("playback", z.playback);
    appendZone("bottom", z.bottom);
    return out;
}

// Compose the three storage zones plus built-in injection (same口径 as
// TrayIcon::ComposeMenu) so resource preflight sees the menu that would open.
inline std::vector<TrayMenuItem> ComposeTrayMenuSnapshot(
    const std::vector<TrayMenuItem>& top,
    const std::vector<TrayMenuItem>& playback,
    const std::vector<TrayMenuItem>& bottom,
    const TrayMenuConfig& config) {
    return FlattenEffectiveZones(BuildEffectiveTrayZones(top, playback, bottom, config));
}

// Discard any single SVG content > 32 KiB (item icon + segmented option icons,
// recursive into submenu). Oversized singles do not count toward the 256 KiB
// total and do not fail the API (DESIGN 8.5).
inline void StripOversizedSvgInTree(std::vector<TrayMenuItem>& items) {
    for (auto& m : items) {
        menu_limits::StripOneSvgIfOversized(m.iconSvgViewBox, m.iconSvgContent);
        for (auto& s : m.segments) {
            menu_limits::StripOneSvgIfOversized(s.iconSvgViewBox, s.iconSvgContent);
        }
        if (!m.submenu.empty()) StripOversizedSvgInTree(m.submenu);
    }
}

namespace detail_tray_limits {

inline void AddSvg(long long& total, const std::string& content, menu_limits::CheckResult& breach) {
    if (!breach.ok) return;
    const long long n = static_cast<long long>(content.size());
    if (n <= 0) return;
    total += n;
    if (total > menu_limits::kMaxTotalSvgBytes) {
        breach = menu_limits::CheckResult::Breach("svgTotal", menu_limits::kMaxTotalSvgBytes, total);
    }
}

inline void TallyTrayItem(const TrayMenuItem& item, long long& itemCount, long long& svgTotal,
                          menu_limits::CheckResult& breach) {
    if (!breach.ok) return;
    ++itemCount;
    if (itemCount > menu_limits::kMaxMenuItems) {
        breach = menu_limits::CheckResult::Breach("items", menu_limits::kMaxMenuItems, itemCount);
        return;
    }
    if (item.type == "segmented") {
        const long long segCount = static_cast<long long>(item.segments.size());
        if (segCount > menu_limits::kMaxSegmentOptions) {
            breach = menu_limits::CheckResult::Breach(
                "segments", menu_limits::kMaxSegmentOptions, segCount);
            return;
        }
    }
    AddSvg(svgTotal, item.iconSvgContent, breach);
    if (!breach.ok) return;
    for (const auto& s : item.segments) {
        AddSvg(svgTotal, s.iconSvgContent, breach);
        if (!breach.ok) return;
    }
    for (const auto& sub : item.submenu) {
        TallyTrayItem(sub, itemCount, svgTotal, breach);
        if (!breach.ok) return;
    }
}

}  // namespace detail_tray_limits

// Validate a composed tray menu tree + css AFTER StripOversizedSvgInTree.
// Tray ContentSized expand depth (root+1) is NOT a fail here — deeper nodes are
// kept but non-expandable at runtime (DESIGN 8.5).
inline menu_limits::CheckResult ValidateTrayMenuResources(
    const std::vector<TrayMenuItem>& composed, const std::string& css) {
    menu_limits::CheckResult breach = menu_limits::ValidateCssBytes(css);
    if (!breach.ok) return breach;
    long long itemCount = 0;
    long long svgTotal = 0;
    for (const auto& it : composed) {
        detail_tray_limits::TallyTrayItem(it, itemCount, svgTotal, breach);
        if (!breach.ok) return breach;
    }
    return breach;
}

// Mutable three-zone + config storage used by tray preflight. Header-only so
// GoogleTest can drive the same transactional writer TrayIcon/TrayApi use
// without linking TrayIcon.cpp (Win32 / foobar2000).
struct TrayMenuStorage {
    std::vector<TrayMenuItem> zones[3];
    TrayMenuConfig config;
};

// Strip oversized single SVGs, validate the post-change composed snapshot
// (other zones + built-in injection + css), and only then mutate storage.
// On breach the previous zones/config are left untouched (DESIGN 8.5).
inline menu_limits::CheckResult TryReplaceContextMenuZone(
    TrayMenuStorage& storage,
    std::vector<TrayMenuItem> items,
    const std::optional<TrayMenuConfig>& newConfig) {
    TrayMenuConfig conf = newConfig.has_value() ? *newConfig : storage.config;
    StripOversizedSvgInTree(items);
    // Shared recursive slider normalization before storage / backends share
    // the same min/max/value/orientation.
    NormalizeSliderMenuItems(items);

    std::vector<TrayMenuItem> preview[3] = {
        storage.zones[0], storage.zones[1], storage.zones[2]
    };
    int target = static_cast<int>(conf.customPosition);
    if (target < 0 || target > 2) target = 0;
    preview[target] = items;

    auto composed = ComposeTrayMenuSnapshot(preview[0], preview[1], preview[2], conf);
    auto breach = ValidateTrayMenuResources(composed, conf.css);
    if (!breach.ok) return breach;

    storage.config = conf;
    storage.zones[target] = std::move(items);
    return menu_limits::CheckResult::Ok();
}

inline menu_limits::CheckResult TryAppendMenuItemsToStorage(
    TrayMenuStorage& storage,
    std::vector<TrayMenuItem> items,
    TrayMenuPosition position) {
    StripOversizedSvgInTree(items);
    NormalizeSliderMenuItems(items);

    int zone = static_cast<int>(position);
    if (zone < 0 || zone > 2) zone = 0;

    std::vector<TrayMenuItem> preview[3] = {
        storage.zones[0], storage.zones[1], storage.zones[2]
    };
    preview[zone].insert(preview[zone].end(), items.begin(), items.end());

    auto composed = ComposeTrayMenuSnapshot(preview[0], preview[1], preview[2], storage.config);
    auto breach = ValidateTrayMenuResources(composed, storage.config.css);
    if (!breach.ok) return breach;

    storage.zones[zone].insert(storage.zones[zone].end(), items.begin(), items.end());
    return menu_limits::CheckResult::Ok();
}

inline std::vector<TrayMenuItem> FlatZoneItems(const TrayMenuStorage& storage) {
    std::vector<TrayMenuItem> out;
    for (const auto& z : storage.zones) {
        out.insert(out.end(), z.begin(), z.end());
    }
    return out;
}

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
    const std::vector<TrayMenuItem>& GetZoneItems(TrayMenuPosition position) const;

    // Transactional writers used by tray.setContextMenu / tray.appendMenuItems:
    // strip oversized single SVGs, validate the post-change composed snapshot
    // (other zones + built-in injection + css), and only then mutate storage.
    // On breach the previous zones/config are left untouched (DESIGN 8.5).
    menu_limits::CheckResult TrySetContextMenu(std::vector<TrayMenuItem> items,
                                               const std::optional<TrayMenuConfig>& config);
    menu_limits::CheckResult TryAppendMenuItems(std::vector<TrayMenuItem> items,
                                                TrayMenuPosition position);

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
    // Native-backend command id -> resolved action (public id + trusted origin +
    // built-in). Routing keys off the stored origin, never the id prefix.
    std::map<int, menu_action::ResolvedAction> m_menuIdMap;
    // Native-backend command id -> (richItemId, value) for the degraded
    // rating/slider submenus. Resolved before m_menuIdMap so a value-bearing
    // pick routes through RouteValueChanged instead of RouteResolvedAction.
    std::map<int, std::pair<std::string, int>> m_menuValueMap;

    ClickCallback m_clickCb;
    DoubleClickCallback m_doubleClickCb;
    MenuItemCallback m_menuItemCb;
    MenuItemValueCallback m_menuItemValueCb;
    BeforeMenuCallback m_beforeMenuCb;

    HMENU BuildMenu(const std::vector<TrayMenuItem>& items, int& cmdId);
    // Thin wrappers over the free BuildPlaybackDefaultItems / BuildSystemDefaultItems
    // (TrayIcon.h, header-only + unit-testable). Kept as members so ComposeMenu
    // reads naturally.
    std::vector<TrayMenuItem> BuildPlaybackDefaults() const;
    std::vector<TrayMenuItem> BuildSystemDefaults() const;
    std::vector<TrayMenuItem> ComposeMenu() const;   // 三区组装（native + webview 共用）
    // Route a resolved selection by its trusted origin (never the id prefix):
    // built-in playback/system origins run their fixed command; Origin::User
    // reaches the frontend callback (-> tray:menuItemClicked). Shared sink for
    // native (sync) and, once tokenised, the webview (async) paths.
    void RouteResolvedAction(const menu_action::ResolvedAction& action);
    void RouteValueChanged(const std::string& id, int value);  // 富控件值变更路由（两路共用）
    void ShowContextMenu();
};
