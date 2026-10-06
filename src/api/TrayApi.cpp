// TrayApi.cpp - tray.* API implementation
// 形状由 src/api/schema/tray.ts 声明，结构体与参数解析来自生成的 TraySchema.h。
#include "pch.h"
#include "api/TrayApi.h"
#include "api/BridgeCore.h"
#include "api/ErrorEnvelope.h"
#include "api/EventEmit.h"
#include "api/TypedApi.h"
#include "api/generated/TraySchema.h"
#include "window/TrayIcon.h"
#include "window/TaskbarTrayContracts.h"
#include "window/MenuResourceLimits.h"
#include "utils/IconLoader.h"
#include "ui/UserInterface.h"
#include "core/WebViewContext.h"
#include "window/MainWindow.h"

namespace {

namespace tray = api::tray;

bool IsPanelMode() {
    auto* ui = WebViewUI::GetInstance();
    if (ui && ui->GetMainWindow() && ui->GetMainWindow()->GetHwnd()) return false;
    return WebViewContext::GetInstance().GetInstanceCount() > 0;
}

// 空或解码失败的图标回退到 foobar2000 主图标。
HICON ResolveIcon(const std::optional<std::string>& b64) {
    if (b64 && !b64->empty()) {
        HICON h = IconLoader::FromBase64(*b64);
        if (h) return h;
    }
    try {
        static_api_ptr_t<ui_control> fb_ui;
        return fb_ui->get_main_icon();
    } catch (...) { return nullptr; }
}

HWND GetMainHwnd() {
    auto* uiInst = WebViewUI::GetInstance();
    if (uiInst && uiInst->GetMainWindow()) return uiInst->GetMainWindow()->GetHwnd();
    return core_api::get_main_window();
}

api::Failure NotCreated() {
    return api::Fail("tray icon is not created", ApiErrorCode::OPERATION_FAILED);
}

// ============================================================
// tray.create / destroy / setIcon / setTooltip / showBalloon
// ============================================================
api::Result<void> TrayCreate(const tray::CreateParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.create");
    HWND hwnd = GetMainHwnd();
    if (!hwnd) return api::Fail("window not available", ApiErrorCode::OPERATION_FAILED);

    HICON hIcon = ResolveIcon(p.icon);
    if (!TrayIcon::GetInstance().Create(hwnd, hIcon, p.tooltip.c_str())) {
        return api::Fail("tray icon could not be created", ApiErrorCode::OPERATION_FAILED);
    }
    return api::Ok();
}

api::Result<void> TrayDestroy(const tray::DestroyParams&) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.destroy");
    TrayIcon::GetInstance().Destroy();
    return api::Ok();
}

api::Result<void> TraySetIcon(const tray::SetIconParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.setIcon");
    if (!TrayIcon::GetInstance().SetIcon(ResolveIcon(p.icon))) return NotCreated();
    return api::Ok();
}

api::Result<void> TraySetTooltip(const tray::SetTooltipParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.setTooltip");
    if (!TrayIcon::GetInstance().SetTooltip(p.tooltip.c_str())) return NotCreated();
    return api::Ok();
}

api::Result<void> TrayShowBalloon(const tray::ShowBalloonParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.showBalloon");
    DWORD iconType = NIIF_INFO;
    if (p.icon == "warning") iconType = NIIF_WARNING;
    else if (p.icon == "error") iconType = NIIF_ERROR;
    if (!TrayIcon::GetInstance().ShowBalloon(p.title.c_str(), p.message.c_str(), iconType)) return NotCreated();
    return api::Ok();
}

// ============================================================
// Menu rows: declared shape <-> stored TrayMenuItem
// ============================================================

// Thrown by ToStored when a row carries a value the tray contract rejects
// fail-loud. Caught at each setContextMenu / appendMenuItems entry and converted
// to an INVALID_PARAMS envelope so no partial menu is stored.
struct TrayMenuParseError {
    std::string message;
    json details;
};

std::vector<TrayMenuItem> ToStoredRows(const std::vector<tray::TrayMenuItem>& rows);

TrayMenuItem ToStored(const tray::TrayMenuItem& item) {
    TrayMenuItem m;
    m.id = item.id.value_or("");
    m.label = item.label.value_or("");
    m.type = item.type;
    m.enabled = item.enabled;
    m.visible = item.visible;
    // Explicit `checked` presence (including false) marks the item checkable so
    // checked:false is not lost as "not a checkbox".
    m.checkable = item.checked.has_value();
    m.checked = item.checked.value_or(false);
    // type:"checkbox" is also an explicit checkable identity (legacy callers).
    if (m.type == "checkbox") m.checkable = true;
    m.icon = item.icon.value_or("");
    if (item.iconSvg) {
        m.iconSvgViewBox = item.iconSvg->viewBox;
        m.iconSvgContent = item.iconSvg->content;
    }
    m.cover = item.cover.value_or("");
    m.title = item.title.value_or("");
    m.subtitle = item.subtitle.value_or("");
    m.value = static_cast<int>(item.value.value_or(0));
    m.minValue = static_cast<int>(item.min.value_or(0));
    m.maxValue = static_cast<int>(item.max.value_or(100));
    if (item.segments) {
        for (const auto& seg : *item.segments) {
            TraySegment s;
            s.label = seg.label.value_or("");
            if (seg.iconSvg) {
                s.iconSvgViewBox = seg.iconSvg->viewBox;
                s.iconSvgContent = seg.iconSvg->content;
            }
            s.enabled = seg.enabled;
            m.segments.push_back(std::move(s));
        }
    }
    if (item.submenu) m.submenu = ToStoredRows(*item.submenu);
    // Slider normalization: range order, value clamp, orientation only kept for sliders.
    if (m.type == "slider") {
        if (m.maxValue < m.minValue) std::swap(m.minValue, m.maxValue);
        if (m.value < m.minValue) m.value = m.minValue;
        if (m.value > m.maxValue) m.value = m.maxValue;
        m.orientation = item.orientation.value_or("");
    }
    // Caller-declared native playback action. The token itself is checked by the
    // declared enum; its placement is a hard error rather than a silent
    // no-promote, because such an item would look like a working control yet
    // silently fail in the background.
    if (item.playbackAction && !item.playbackAction->empty()) {
        const std::string& pa = *item.playbackAction;
        if (m.type != "normal") {
            throw TrayMenuParseError{
                "playbackAction requires type 'normal'",
                { {"id", m.id}, {"playbackAction", pa}, {"type", m.type} }
            };
        }
        if (!m.submenu.empty()) {
            throw TrayMenuParseError{
                "playbackAction cannot be declared on a submenu item",
                { {"id", m.id}, {"playbackAction", pa} }
            };
        }
        m.playbackAction = pa;
    }
    return m;
}

std::vector<TrayMenuItem> ToStoredRows(const std::vector<tray::TrayMenuItem>& rows) {
    std::vector<TrayMenuItem> items;
    items.reserve(rows.size());
    for (const auto& r : rows) items.push_back(ToStored(r));
    return items;
}

std::vector<tray::TrayMenuItem> ToDeclaredRows(const std::vector<TrayMenuItem>& rows);

tray::TrayMenuItem ToDeclared(const TrayMenuItem& m) {
    tray::TrayMenuItem out;
    out.id = m.id;
    out.label = m.label;
    out.type = m.type.empty() ? "normal" : m.type;
    out.enabled = m.enabled;
    out.visible = m.visible;
    // Preserve checkable field existence: report checked when the item is
    // checkable (including checked:false), not only when true.
    if (m.checkable || m.checked || m.type == "checkbox") out.checked = m.checked;
    if (!m.icon.empty()) out.icon = m.icon;
    if (taskbar_tray_contracts::TrayItemHasRenderableIconSvg(m.iconSvgViewBox, m.iconSvgContent)) {
        out.iconSvg = tray::TrayIconSvg{m.iconSvgViewBox, m.iconSvgContent};
    }
    if (!m.cover.empty()) out.cover = m.cover;
    if (!m.title.empty()) out.title = m.title;
    if (!m.subtitle.empty()) out.subtitle = m.subtitle;
    if (m.type == "rating" || m.type == "slider" || m.type == "segmented") out.value = m.value;
    if (m.type == "slider") {
        out.min = m.minValue;
        out.max = m.maxValue;
        if (m.orientation == "vertical" || m.orientation == "horizontal") out.orientation = m.orientation;
    }
    if (m.type == "segmented" && !m.segments.empty()) {
        std::vector<tray::TraySegment> segs;
        for (const auto& s : m.segments) {
            tray::TraySegment sj;
            if (!s.label.empty()) sj.label = s.label;
            if (taskbar_tray_contracts::TrayItemHasRenderableIconSvg(s.iconSvgViewBox, s.iconSvgContent)) {
                sj.iconSvg = tray::TrayIconSvg{s.iconSvgViewBox, s.iconSvgContent};
            }
            sj.enabled = s.enabled;
            segs.push_back(std::move(sj));
        }
        out.segments = std::move(segs);
    }
    if (!m.submenu.empty()) out.submenu = ToDeclaredRows(m.submenu);
    if (!m.playbackAction.empty()) out.playbackAction = m.playbackAction;
    return out;
}

std::vector<tray::TrayMenuItem> ToDeclaredRows(const std::vector<TrayMenuItem>& rows) {
    std::vector<tray::TrayMenuItem> out;
    out.reserve(rows.size());
    for (const auto& m : rows) out.push_back(ToDeclared(m));
    return out;
}

// ============================================================
// position string <-> TrayMenuPosition enum
// ============================================================
TrayMenuPosition ParsePosition(const std::string& s) {
    if (s == "playback") return TrayMenuPosition::Playback;
    if (s == "bottom")   return TrayMenuPosition::Bottom;
    return TrayMenuPosition::Top;
}

api::Failure ResourceLimit(const menu_limits::CheckResult& breach) {
    return api::Fail("tray menu resource limit exceeded", ApiErrorCode::INVALID_PARAMS,
                     {{"details", menu_limits::DetailsJson(breach)}});
}

api::Failure ParseFailure(const TrayMenuParseError& e) {
    return api::Fail(e.message, ApiErrorCode::INVALID_PARAMS, {{"details", e.details}});
}

// The stored config with only the keys the caller gave overwritten. It is a
// would-be value: callers hand it to a transactional writer, which stores it
// only when the menu passes the resource preflight.
TrayMenuConfig MergeMenuConfig(TrayMenuConfig conf, const tray::TrayMenuConfig& cfg) {
    if (cfg.showPlaybackControls.has_value()) conf.showPlaybackControls = *cfg.showPlaybackControls;
    if (cfg.showSystemItems.has_value()) conf.showSystemItems = *cfg.showSystemItems;
    if (cfg.customPosition.has_value()) conf.customPosition = ParsePosition(*cfg.customPosition);
    if (cfg.render.has_value()) conf.render = (*cfg.render == "webview") ? TrayMenuRender::WebView : TrayMenuRender::Native;
    if (cfg.autoNowPlaying.has_value()) conf.autoNowPlaying = *cfg.autoNowPlaying;
    if (cfg.css.has_value()) conf.css = *cfg.css;
    if (cfg.cssReplace.has_value()) conf.cssReplace = *cfg.cssReplace;
    if (cfg.backdrop.has_value()) conf.backdrop = *cfg.backdrop;
    if (cfg.backdropDarkMode.has_value()) conf.backdropDarkMode = *cfg.backdropDarkMode;
    if (cfg.closeAnimationMs.has_value()) {
        conf.closeAnimationMs = static_cast<int>(std::clamp<std::int64_t>(*cfg.closeAnimationMs, 0, 1000));
    }
    if (cfg.layoutMode.has_value()) conf.layoutMode = (*cfg.layoutMode == "zones") ? TrayMenuLayoutMode::Zones : TrayMenuLayoutMode::Flat;
    return conf;
}

// ============================================================
// tray.setContextMenu
// ============================================================
api::Result<void> TraySetContextMenu(const tray::SetContextMenuParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.setContextMenu");

    auto& trayIcon = TrayIcon::GetInstance();

    std::optional<TrayMenuConfig> newConf;
    if (p.config) newConf = MergeMenuConfig(trayIcon.GetContextMenuConfig(), *p.config);

    std::vector<TrayMenuItem> parsedItems;
    try {
        parsedItems = ToStoredRows(p.items);
    } catch (const TrayMenuParseError& e) {
        return ParseFailure(e);
    }

    auto breach = trayIcon.TrySetContextMenu(std::move(parsedItems), newConf);
    if (!breach.ok) return ResourceLimit(breach);
    return api::Ok();
}

// ============================================================
// tray.setMenuZones
// ============================================================
// Every zone is parsed before anything is stored, and all three plus the config
// go through one TryReplaceMenuZones call. Bridge calls and the menu build both
// run on the main thread, so a right-click never sees a half-applied update.
api::Result<void> TraySetMenuZones(const tray::SetMenuZonesParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.setMenuZones");

    auto& trayIcon = TrayIcon::GetInstance();

    std::optional<TrayMenuConfig> newConf;
    if (p.config) newConf = MergeMenuConfig(trayIcon.GetContextMenuConfig(), *p.config);

    // Indexed by TrayMenuPosition. A zone left out becomes an empty vector,
    // which clears it, rather than std::nullopt, which would keep it.
    const std::array<const std::optional<std::vector<tray::TrayMenuItem>>*, 3> given{
        &p.top, &p.playback, &p.bottom};
    static constexpr std::array<const char*, 3> kZoneNames{"top", "playback", "bottom"};
    TrayZoneReplacement zones;
    for (size_t i = 0; i < given.size(); ++i) {
        try {
            zones[i] = *given[i] ? ToStoredRows(**given[i]) : std::vector<TrayMenuItem>{};
        } catch (TrayMenuParseError& e) {
            e.details["zone"] = kZoneNames[i];
            return ParseFailure(e);
        }
    }

    auto breach = trayIcon.TryReplaceMenuZones(std::move(zones), newConf);
    if (!breach.ok) return ResourceLimit(breach);
    return api::Ok();
}

// ============================================================
// Incremental menu management
// ============================================================
api::Result<void> TrayAppendMenuItems(const tray::AppendMenuItemsParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.appendMenuItems");
    std::vector<TrayMenuItem> parsedItems;
    try {
        parsedItems = ToStoredRows(p.items);
    } catch (const TrayMenuParseError& e) {
        return ParseFailure(e);
    }
    auto breach = TrayIcon::GetInstance().TryAppendMenuItems(std::move(parsedItems), ParsePosition(p.position));
    if (!breach.ok) return ResourceLimit(breach);
    return api::Ok();
}

api::Result<tray::RemoveMenuItemsResult> TrayRemoveMenuItems(const tray::RemoveMenuItemsParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.removeMenuItems");
    tray::RemoveMenuItemsResult result;
    result.removed = TrayIcon::GetInstance().RemoveMenuItems(p.ids);
    return result;
}

api::Result<void> TrayClearMenuItems(const tray::ClearMenuItemsParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.clearMenuItems");
    if (p.position) {
        TrayIcon::GetInstance().ClearMenuItems(ParsePosition(*p.position));
    } else {
        TrayIcon::GetInstance().ClearAllMenuItems();
    }
    return api::Ok();
}

api::Result<tray::GetMenuItemsResult> TrayGetMenuItems(const tray::GetMenuItemsParams&) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.getMenuItems");
    tray::GetMenuItemsResult result;
    result.items = ToDeclaredRows(TrayIcon::GetInstance().GetMenuItems());
    return result;
}

// ============================================================
// tray.setMenuItemState - mutate one item's checked/enabled in place
// ============================================================
// Granular alternative to re-sending the whole menu via setContextMenu (which
// is a full-zone replace). Native menus rebuild from stored data on each open,
// so the new state shows on the NEXT right-click.
api::Result<tray::SetMenuItemStateResult> TraySetMenuItemState(const tray::SetMenuItemStateParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.setMenuItemState");
    if (!p.checked && !p.enabled) {
        return api::Fail("at least one of checked/enabled required", ApiErrorCode::INVALID_PARAMS);
    }
    if (!TrayIcon::GetInstance().SetMenuItemState(p.id, p.checked, p.enabled)) {
        return api::Fail("menu item not found", ApiErrorCode::NOT_FOUND, {{"found", false}});
    }
    tray::SetMenuItemStateResult result;
    result.found = true;
    return result;
}

// ============================================================
// tray.setMinimizeToTray / setCloseToTray / isVisible
// ============================================================
api::Result<void> TraySetMinimizeToTray(const tray::SetMinimizeToTrayParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.setMinimizeToTray");
    TrayIcon::GetInstance().SetMinimizeToTray(p.enabled);
    return api::Ok();
}

api::Result<void> TraySetCloseToTray(const tray::SetCloseToTrayParams& p) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.setCloseToTray");
    TrayIcon::GetInstance().SetCloseToTray(p.enabled);
    return api::Ok();
}

api::Result<tray::IsVisibleResult> TrayIsVisible(const tray::IsVisibleParams&) {
    if (IsPanelMode()) return api::PanelModeUnsupported("tray.isVisible");
    tray::IsVisibleResult result;
    result.visible = TrayIcon::GetInstance().IsCreated();
    return result;
}

} // namespace

// ============================================================
// Registration
// ============================================================
void RegisterTrayApi() {
    // Tray interaction events.
    // Broadcast to every window's bridge (like playback:* / window:* in the
    // callbacks). The tray icon is app-global with no originating window, so the
    // singleton BridgeCore::EmitEvent (which only reaches the last window to call
    // SetWebView, i.e. "main") would silently drop these for any popup/secondary
    // window whose frontend holds the handler.
    TrayIcon::GetInstance().SetClickCallback([](int button, int x, int y) {
        tray::ClickPayload payload;
        payload.button = button;
        payload.x = x;
        payload.y = y;
        api::emit::Broadcast<tray::events::Click>(payload);
    });
    TrayIcon::GetInstance().SetDoubleClickCallback([](int x, int y) {
        tray::DoubleClickPayload payload;
        payload.x = x;
        payload.y = y;
        api::emit::Broadcast<tray::events::DoubleClick>(payload);
    });
    TrayIcon::GetInstance().SetMenuItemCallback([](const std::string& id) {
        tray::MenuItemClickedPayload payload;
        payload.id = id;
        api::emit::Broadcast<tray::events::MenuItemClicked>(payload);
    });
    // Rich items (rating / slider / segmented) report value changes under the
    // same event with an extra `value`; the webview-rendered menu stays open,
    // the native one has already closed. Frontends that ignore `value` keep
    // their existing {id} behaviour.
    TrayIcon::GetInstance().SetMenuItemValueCallback([](const std::string& id, int value) {
        tray::MenuItemClickedPayload payload;
        payload.id = id;
        payload.value = value;
        api::emit::Broadcast<tray::events::MenuItemClicked>(payload);
    });
    // tray:beforeContextMenu is an asynchronous notification fired
    // immediately before the popup is built. Frontend mutations performed in
    // the handler only affect the NEXT right-click (the bridge dispatch is
    // async; see TrayIcon::ShowContextMenu comments).
    TrayIcon::GetInstance().SetBeforeMenuCallback([](int x, int y) {
        tray::BeforeContextMenuPayload payload;
        payload.x = x;
        payload.y = y;
        api::emit::Broadcast<tray::events::BeforeContextMenu>(payload);
    });

    // Icon / balloon / lifecycle APIs
    api::RegisterApi("tray.create",            TrayCreate);
    api::RegisterApi("tray.destroy",           TrayDestroy);
    api::RegisterApi("tray.setIcon",           TraySetIcon);
    api::RegisterApi("tray.setTooltip",        TraySetTooltip);
    api::RegisterApi("tray.showBalloon",       TrayShowBalloon);
    api::RegisterApi("tray.setContextMenu",    TraySetContextMenu);
    api::RegisterApi("tray.setMenuZones",      TraySetMenuZones);
    api::RegisterApi("tray.setMinimizeToTray", TraySetMinimizeToTray);
    api::RegisterApi("tray.setCloseToTray",    TraySetCloseToTray);
    api::RegisterApi("tray.isVisible",         TrayIsVisible);

    // Incremental menu management APIs
    api::RegisterApi("tray.appendMenuItems", TrayAppendMenuItems);
    api::RegisterApi("tray.removeMenuItems", TrayRemoveMenuItems);
    api::RegisterApi("tray.clearMenuItems",  TrayClearMenuItems);
    api::RegisterApi("tray.getMenuItems",    TrayGetMenuItems);
    api::RegisterApi("tray.setMenuItemState", TraySetMenuItemState);
}
