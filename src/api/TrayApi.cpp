// TrayApi.cpp - tray.* API implementation
#include "pch.h"
#include "api/TrayApi.h"
#include "api/BridgeCore.h"
#include "window/TrayIcon.h"
#include "window/TaskbarTrayContracts.h"
#include "utils/IconLoader.h"
#include "core/UserInterface.h"
#include "core/WebViewContext.h"
#include "window/MainWindow.h"

static bool IsPanelMode() {
    auto* ui = WebViewUI::GetInstance();
    if (ui && ui->GetMainWindow() && ui->GetMainWindow()->GetHwnd()) return false;
    return WebViewContext::GetInstance().GetInstanceCount() > 0;
}
static json PanelModeResponse() { return {{"success", false}, {"panelMode", true}}; }

static HICON ResolveIconParam(const json& params, const char* key = "icon") {
    if (params.contains(key) && params[key].is_string()) {
        std::string b64 = params[key].get<std::string>();
        if (!b64.empty()) {
            HICON h = IconLoader::FromBase64(b64);
            if (h) return h;
        }
    }
    try {
        static_api_ptr_t<ui_control> fb_ui;
        return fb_ui->get_main_icon();
    } catch (...) { return nullptr; }
}

static HWND GetMainHwnd() {
    auto* uiInst = WebViewUI::GetInstance();
    if (uiInst && uiInst->GetMainWindow()) return uiInst->GetMainWindow()->GetHwnd();
    return core_api::get_main_window();
}

// ============================================================
// tray.create
// ============================================================
static json TrayCreate(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    HWND hwnd = GetMainHwnd();
    if (!hwnd) return {{"success", false}, {"error", "window not available"}};

    HICON hIcon = ResolveIconParam(params);
    std::string tooltip = params.value("tooltip", "foobar2000");
    bool ok = TrayIcon::GetInstance().Create(hwnd, hIcon, tooltip.c_str());
    return {{"success", ok}};
}

// ============================================================
// tray.destroy
// ============================================================
static json TrayDestroy(const json& /*params*/) {
    if (IsPanelMode()) return PanelModeResponse();
    TrayIcon::GetInstance().Destroy();
    return {{"success", true}};
}

// ============================================================
// tray.setIcon
// ============================================================
static json TraySetIcon(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    HICON hIcon = ResolveIconParam(params);
    bool ok = TrayIcon::GetInstance().SetIcon(hIcon);
    return {{"success", ok}};
}

// ============================================================
// tray.setTooltip
// ============================================================
static json TraySetTooltip(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    std::string tooltip = params.value("tooltip", "");
    bool ok = TrayIcon::GetInstance().SetTooltip(tooltip.c_str());
    return {{"success", ok}};
}

// ============================================================
// tray.showBalloon
// ============================================================
static json TrayShowBalloon(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    std::string title = params.value("title", "");
    std::string message = params.value("message", "");
    std::string iconStr = params.value("icon", "info");
    DWORD iconType = NIIF_INFO;
    if (iconStr == "warning") iconType = NIIF_WARNING;
    else if (iconStr == "error") iconType = NIIF_ERROR;
    bool ok = TrayIcon::GetInstance().ShowBalloon(title.c_str(), message.c_str(), iconType);
    return {{"success", ok}};
}

// ============================================================
// tray.setContextMenu
// ============================================================
static std::vector<TrayMenuItem> ParseMenuItemsVec(const json& arr);
static TrayMenuItem ParseMenuItem(const json& item) {
    TrayMenuItem m;
    m.id = item.value("id", "");
    m.label = item.value("label", "");
    m.type = item.value("type", "normal");
    m.enabled = item.value("enabled", true);
    m.visible = item.value("visible", true);
    m.checked = item.value("checked", false);
    m.icon = item.value("icon", "");
    // Inline monochrome SVG icon { viewBox, content } (webview backend only).
    if (item.contains("iconSvg") && item["iconSvg"].is_object()) {
        const auto& iv = item["iconSvg"];
        m.iconSvgViewBox = iv.value("viewBox", "");
        m.iconSvgContent = iv.value("content", "");
    }
    // Rich-item payload (rendered only by the webview backend; see TrayIcon).
    m.cover = item.value("cover", "");
    m.title = item.value("title", "");
    m.subtitle = item.value("subtitle", "");
    m.value = item.value("value", 0);
    m.minValue = item.value("min", 0);
    m.maxValue = item.value("max", 100);
    // Segmented rich item: inline single-select options (webview backend only).
    // Each segment carries an optional label / inline SVG icon / enabled flag.
    if (item.contains("segments") && item["segments"].is_array()) {
        for (const auto& seg : item["segments"]) {
            if (!seg.is_object()) continue;
            TraySegment s;
            s.label = seg.value("label", "");
            if (seg.contains("iconSvg") && seg["iconSvg"].is_object()) {
                const auto& iv = seg["iconSvg"];
                s.iconSvgViewBox = iv.value("viewBox", "");
                s.iconSvgContent = iv.value("content", "");
            }
            s.enabled = seg.value("enabled", true);
            m.segments.push_back(std::move(s));
        }
    }
    if (item.contains("submenu") && item["submenu"].is_array())
        m.submenu = ParseMenuItemsVec(item["submenu"]);
    return m;
}
static std::vector<TrayMenuItem> ParseMenuItemsVec(const json& arr) {
    std::vector<TrayMenuItem> items;
    for (const auto& it : arr) items.push_back(ParseMenuItem(it));
    return items;
}

// ============================================================
// position string <-> TrayMenuPosition enum
// ============================================================
static TrayMenuPosition ParsePosition(const std::string& s, TrayMenuPosition def = TrayMenuPosition::Top) {
    if (s == "top")      return TrayMenuPosition::Top;
    if (s == "playback") return TrayMenuPosition::Playback;
    if (s == "bottom")   return TrayMenuPosition::Bottom;
    return def;
}
static const char* PositionToString(TrayMenuPosition p) {
    switch (p) {
        case TrayMenuPosition::Top:      return "top";
        case TrayMenuPosition::Playback: return "playback";
        case TrayMenuPosition::Bottom:   return "bottom";
    }
    return "top";
}

static json TraySetContextMenu(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    if (!params.contains("items") || !params["items"].is_array())
        return {{"success", false}, {"error", "items array required"}};

    auto& tray = TrayIcon::GetInstance();

    // Apply optional config first, so SetContextMenu writes into the configured zone.
    if (params.contains("config") && params["config"].is_object()) {
        const auto& cfg = params["config"];
        TrayMenuConfig conf = tray.GetContextMenuConfig();
        if (cfg.contains("showPlaybackControls") && cfg["showPlaybackControls"].is_boolean())
            conf.showPlaybackControls = cfg["showPlaybackControls"].get<bool>();
        if (cfg.contains("showSystemItems") && cfg["showSystemItems"].is_boolean())
            conf.showSystemItems = cfg["showSystemItems"].get<bool>();
        if (cfg.contains("customPosition") && cfg["customPosition"].is_string())
            conf.customPosition = ParsePosition(cfg["customPosition"].get<std::string>());
        if (cfg.contains("render") && cfg["render"].is_string())
            conf.render = (cfg["render"].get<std::string>() == "webview")
                ? TrayMenuRender::WebView : TrayMenuRender::Native;
        if (cfg.contains("autoNowPlaying") && cfg["autoNowPlaying"].is_boolean())
            conf.autoNowPlaying = cfg["autoNowPlaying"].get<bool>();
        // Frontend style takeover (webview backend only, STYLING_TAKEOVER_DESIGN S-CSS).
        if (cfg.contains("css") && cfg["css"].is_string())
            conf.css = cfg["css"].get<std::string>();
        if (cfg.contains("cssReplace") && cfg["cssReplace"].is_boolean())
            conf.cssReplace = cfg["cssReplace"].get<bool>();
        // DWM system backdrop for the self-drawn (webview) tray menu. Illegal
        // values are ignored, keeping the "acrylic" default.
        if (cfg.contains("backdrop") && cfg["backdrop"].is_string()) {
            auto b = cfg["backdrop"].get<std::string>();
            if (b == "acrylic" || b == "mica" || b == "mica-alt" || b == "none") conf.backdrop = b;
        }
        if (cfg.contains("backdropDarkMode") && cfg["backdropDarkMode"].is_boolean())
            conf.backdropDarkMode = cfg["backdropDarkMode"].get<bool>();
        // Exit (fade-out) animation duration for the self-drawn (webview) tray
        // menu (opt-in; 0 = immediate hide). Clamped to 0..1000 ms.
        if (cfg.contains("closeAnimationMs") && cfg["closeAnimationMs"].is_number_integer()) {
            int v = cfg["closeAnimationMs"].get<int>();
            conf.closeAnimationMs = v < 0 ? 0 : (v > 1000 ? 1000 : v);
        }
        tray.SetContextMenuConfig(conf);
    }

    tray.SetContextMenu(ParseMenuItemsVec(params["items"]));
    return {{"success", true}};
}

// ============================================================
// Incremental menu management
// ============================================================
static json TrayAppendMenuItems(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    if (!params.contains("items") || !params["items"].is_array())
        return {{"success", false}, {"error", "items array required"}};
    TrayMenuPosition pos = TrayMenuPosition::Top;
    if (params.contains("position") && params["position"].is_string())
        pos = ParsePosition(params["position"].get<std::string>());
    TrayIcon::GetInstance().AppendMenuItems(ParseMenuItemsVec(params["items"]), pos);
    return {{"success", true}};
}

static json TrayRemoveMenuItems(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    if (!params.contains("ids") || !params["ids"].is_array())
        return {{"success", false}, {"error", "ids array required"}};
    std::vector<std::string> ids;
    for (const auto& v : params["ids"]) {
        if (v.is_string()) ids.push_back(v.get<std::string>());
    }
    int removed = TrayIcon::GetInstance().RemoveMenuItems(ids);
    return {{"success", true}, {"removed", removed}};
}

static json TrayClearMenuItems(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    if (params.contains("position") && params["position"].is_string()) {
        TrayMenuPosition pos = ParsePosition(params["position"].get<std::string>());
        TrayIcon::GetInstance().ClearMenuItems(pos);
    } else {
        TrayIcon::GetInstance().ClearAllMenuItems();
    }
    return {{"success", true}};
}

static json MenuItemToJson(const TrayMenuItem& m) {
    json out;
    out["id"] = m.id;
    out["label"] = m.label;
    out["type"] = m.type.empty() ? "normal" : m.type;
    out["enabled"] = m.enabled;
    out["visible"] = m.visible;
    out["checked"] = m.checked;
    if (!m.icon.empty()) out["icon"] = m.icon;
    if (taskbar_tray_contracts::TrayItemHasRenderableIconSvg(m.iconSvgViewBox, m.iconSvgContent))
        out["iconSvg"] = { {"viewBox", m.iconSvgViewBox}, {"content", m.iconSvgContent} };
    // Echo rich-item payload so getMenuItems round-trips what was set.
    if (!m.cover.empty()) out["cover"] = m.cover;
    if (!m.title.empty()) out["title"] = m.title;
    if (!m.subtitle.empty()) out["subtitle"] = m.subtitle;
    if (m.type == "rating" || m.type == "slider" || m.type == "segmented") out["value"] = m.value;
    if (m.type == "slider") { out["min"] = m.minValue; out["max"] = m.maxValue; }
    // Segmented: round-trip the inline single-select options so getMenuItems
    // echoes what was set (label / inline SVG icon / enabled per segment).
    if (m.type == "segmented" && !m.segments.empty()) {
        json segs = json::array();
        for (const auto& s : m.segments) {
            json sj;
            if (!s.label.empty()) sj["label"] = s.label;
            if (taskbar_tray_contracts::TrayItemHasRenderableIconSvg(s.iconSvgViewBox, s.iconSvgContent))
                sj["iconSvg"] = { {"viewBox", s.iconSvgViewBox}, {"content", s.iconSvgContent} };
            sj["enabled"] = s.enabled;
            segs.push_back(std::move(sj));
        }
        out["segments"] = segs;
    }
    if (!m.submenu.empty()) {
        json sub = json::array();
        for (const auto& s : m.submenu) sub.push_back(MenuItemToJson(s));
        out["submenu"] = sub;
    }
    return out;
}

static json TrayGetMenuItems(const json& /*params*/) {
    if (IsPanelMode()) return PanelModeResponse();
    auto items = TrayIcon::GetInstance().GetMenuItems();
    json arr = json::array();
    for (const auto& m : items) arr.push_back(MenuItemToJson(m));
    return {{"success", true}, {"items", arr}};
}

// ============================================================
// tray.setMenuItemState - mutate one item's checked/enabled in place
// ============================================================
// Granular alternative to re-sending the whole menu via setContextMenu (which
// is a full-zone replace). Native menus rebuild from stored data on each open,
// so the new state shows on the NEXT right-click.
static json TraySetMenuItemState(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    std::string id = params.value("id", "");
    if (id.empty()) return {{"success", false}, {"error", "id required"}};

    std::optional<bool> checked;
    std::optional<bool> enabled;
    if (params.contains("checked") && params["checked"].is_boolean())
        checked = params["checked"].get<bool>();
    if (params.contains("enabled") && params["enabled"].is_boolean())
        enabled = params["enabled"].get<bool>();
    if (!checked.has_value() && !enabled.has_value())
        return {{"success", false}, {"error", "at least one of checked/enabled required"}};

    bool found = TrayIcon::GetInstance().SetMenuItemState(id, checked, enabled);
    return {{"success", found}, {"found", found}};
}

// ============================================================
// tray.setMinimizeToTray / setCloseToTray / isVisible
// ============================================================
static json TraySetMinimizeToTray(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    TrayIcon::GetInstance().SetMinimizeToTray(params.value("enabled", false));
    return {{"success", true}};
}
static json TraySetCloseToTray(const json& params) {
    if (IsPanelMode()) return PanelModeResponse();
    TrayIcon::GetInstance().SetCloseToTray(params.value("enabled", false));
    return {{"success", true}};
}
static json TrayIsVisible(const json& /*params*/) {
    if (IsPanelMode()) return PanelModeResponse();
    return {{"success", true}, {"visible", TrayIcon::GetInstance().IsCreated()}};
}

// ============================================================
// Registration
// ============================================================
void RegisterTrayApi() {
    auto& bridge = BridgeCore::GetInstance();

    // Tray interaction events.
    // Broadcast to every window's bridge (like playback:* / window:* in the
    // callbacks). The tray icon is app-global with no originating window, so the
    // singleton BridgeCore::EmitEvent (which only reaches the last window to call
    // SetWebView, i.e. "main") would silently drop these for any popup/secondary
    // window whose frontend holds the handler.
    TrayIcon::GetInstance().SetClickCallback([](int button, int x, int y) {
        WebViewContext::GetInstance().BroadcastEvent("tray:click", {{"button", button}, {"x", x}, {"y", y}});
    });
    TrayIcon::GetInstance().SetDoubleClickCallback([](int x, int y) {
        WebViewContext::GetInstance().BroadcastEvent("tray:doubleClick", {{"x", x}, {"y", y}});
    });
    TrayIcon::GetInstance().SetMenuItemCallback([](const std::string& id) {
        WebViewContext::GetInstance().BroadcastEvent("tray:menuItemClicked", {{"id", id}});
    });
    // Rich items (rating / slider) report value changes without closing the
    // menu. Same event name as a normal click, with an extra `value` field;
    // frontends that ignore `value` keep their existing {id} behaviour.
    TrayIcon::GetInstance().SetMenuItemValueCallback([](const std::string& id, int value) {
        WebViewContext::GetInstance().BroadcastEvent("tray:menuItemClicked", {{"id", id}, {"value", value}});
    });
    // tray:beforeContextMenu is an asynchronous notification fired
    // immediately before the popup is built. Frontend mutations performed in
    // the handler only affect the NEXT right-click (the bridge dispatch is
    // async; see TrayIcon::ShowContextMenu comments).
    TrayIcon::GetInstance().SetBeforeMenuCallback([](int x, int y) {
        WebViewContext::GetInstance().BroadcastEvent("tray:beforeContextMenu", {{"x", x}, {"y", y}});
    });

    // Icon / balloon / lifecycle APIs
    bridge.RegisterApi("tray.create",            TrayCreate);
    bridge.RegisterApi("tray.destroy",           TrayDestroy);
    bridge.RegisterApi("tray.setIcon",           TraySetIcon);
    bridge.RegisterApi("tray.setTooltip",        TraySetTooltip);
    bridge.RegisterApi("tray.showBalloon",       TrayShowBalloon);
    bridge.RegisterApi("tray.setContextMenu",    TraySetContextMenu);
    bridge.RegisterApi("tray.setMinimizeToTray", TraySetMinimizeToTray);
    bridge.RegisterApi("tray.setCloseToTray",    TraySetCloseToTray);
    bridge.RegisterApi("tray.isVisible",         TrayIsVisible);

    // Incremental menu management APIs
    bridge.RegisterApi("tray.appendMenuItems", TrayAppendMenuItems);
    bridge.RegisterApi("tray.removeMenuItems", TrayRemoveMenuItems);
    bridge.RegisterApi("tray.clearMenuItems",  TrayClearMenuItems);
    bridge.RegisterApi("tray.getMenuItems",    TrayGetMenuItems);
    bridge.RegisterApi("tray.setMenuItemState", TraySetMenuItemState);
}
