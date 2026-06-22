// TrayIcon.cpp - system tray icon management
#include "pch.h"
#include "window/TrayIcon.h"
#include "window/TaskbarTrayContracts.h"
#include "utils/IconLoader.h"
#include "api/BridgeCore.h"
#include <shellapi.h>
#include <foobar2000/SDK/playback_control.h>
#include <foobar2000/SDK/core_api.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "window/MenuOverlayHost.h"
#include <foobar2000/SDK/album_art.h>
#include "utils/Base64.h"
#include "utils/ImageUtils.h"

TrayIcon& TrayIcon::GetInstance() {
    static TrayIcon instance;
    return instance;
}

namespace {
constexpr wchar_t kTrayMessageWindowClass[] = L"FooUiWebView2TrayMessageWindow";
}

namespace {

// Minimal magic-byte image MIME sniff (covers the formats embedded album art uses).
const char* DetectImageMime(const uint8_t* d, size_t n) {
    if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF) return "image/jpeg";
    if (n >= 4 && d[0] == 0x89 && d[1] == 0x50 && d[2] == 0x4E && d[3] == 0x47) return "image/png";
    if (n >= 4 && d[0] == 0x47 && d[1] == 0x49 && d[2] == 0x46) return "image/gif";
    if (n >= 12 && d[0] == 0x52 && d[1] == 0x49 && d[2] == 0x46 && d[3] == 0x46 &&
        d[8] == 0x57 && d[9] == 0x45 && d[10] == 0x42 && d[11] == 0x50) return "image/webp";
    if (n >= 2 && d[0] == 0x42 && d[1] == 0x4D) return "image/bmp";
    return "image/jpeg";
}

// Build a small data: URL for the current track's FRONT cover, or "" on miss.
// CACHE-ONLY: reads the now-playing in-memory art cache (no disk I/O), so it never
// blocks the right-click popup; on a cache miss it returns "" (no cover) rather
// than falling back to a synchronous extractor read. WIC-downscales to a <=64px
// thumbnail and bounds the embedded payload (~256KB).
std::string BuildCurrentCoverDataUrl() {
    try {
        album_art_data::ptr data = now_playing_album_art_notify_manager::get()->current();
        if (!data.is_valid() || data->get_size() == 0) {
            return std::string();
        }
        const uint8_t* src = static_cast<const uint8_t*>(data->data());
        const size_t srcLen = data->get_size();

        int outW = 0, outH = 0;
        const char* outMime = "image/jpeg";
        std::vector<uint8_t> resized = ResizeImageWIC(src, srcLen, 64, outW, outH, outMime);

        const uint8_t* bytes;
        size_t size;
        std::string mime;
        if (!resized.empty()) {            // downscaled JPEG thumbnail
            bytes = resized.data();
            size  = resized.size();
            mime  = outMime;
        } else {                           // already <=64px or resize unavailable: use original
            bytes = src;
            size  = srcLen;
            mime  = DetectImageMime(src, srcLen);
        }

        constexpr size_t kCoverByteCap = 256 * 1024;  // bound embedded payload (~256KB)
        if (size == 0 || size > kCoverByteCap) {
            return std::string();
        }
        return "data:" + mime + ";base64," + utils::Base64Encode(bytes, size);
    } catch (...) {
        return std::string();
    }
}

// Render the current track's %title% / [%artist%] via the playback control so
// dynamic stream titles and the filename fallback both work. Returns false when
// nothing is playing.
bool FormatCurrentTitleArtist(std::string& title, std::string& artist) {
    try {
        static titleformat_object::ptr s_title;
        static titleformat_object::ptr s_artist;
        if (!s_title.is_valid())  static_api_ptr_t<titleformat_compiler>()->compile_safe(s_title, "%title%");
        if (!s_artist.is_valid()) static_api_ptr_t<titleformat_compiler>()->compile_safe(s_artist, "[%artist%]");
        auto pc = playback_control::get();
        pfc::string8 t, a;
        if (!pc->playback_format_title(nullptr, t, s_title, nullptr, playback_control::display_level_titles)) {
            return false;   // nothing playing
        }
        pc->playback_format_title(nullptr, a, s_artist, nullptr, playback_control::display_level_titles);
        title = t.c_str();
        artist = a.c_str();
        return true;
    } catch (...) {
        return false;
    }
}

// Now-playing smart fallback (generic; any frontend). For each nowplaying item,
// fill any field the frontend left empty from the current track (frontend-first).
// cover is webview-only; the native degrade fills text (title/artist) only.
void AutoFillNowPlaying(std::vector<TrayMenuItem>& items, bool autoOn, bool includeCover) {
    if (!autoOn) return;
    for (auto& it : items) {
        if (it.type == "nowplaying") {
            const bool fillTitle = taskbar_tray_contracts::TrayShouldAutoFillField(autoOn, !it.title.empty());
            const bool fillSub   = taskbar_tray_contracts::TrayShouldAutoFillField(autoOn, !it.subtitle.empty());
            if (fillTitle || fillSub) {
                std::string title, artist;
                if (FormatCurrentTitleArtist(title, artist)) {
                    if (fillTitle) it.title = title;
                    if (fillSub)   it.subtitle = artist;
                }
            }
            if (includeCover &&
                taskbar_tray_contracts::TrayShouldAutoFillField(autoOn, !it.cover.empty())) {
                std::string url = BuildCurrentCoverDataUrl();
                if (!url.empty()) it.cover = url;
            }
        }
        if (!it.submenu.empty()) AutoFillNowPlaying(it.submenu, autoOn, includeCover);
    }
}

}  // namespace

bool TrayIcon::RegisterMessageWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TrayIcon::MessageWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kTrayMessageWindowClass;

    if (RegisterClassExW(&wc)) return true;
    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool TrayIcon::EnsureMessageWindow(HWND ownerHwnd) {
    if (!taskbar_tray_contracts::ShouldUseDedicatedTrayMessageWindow()) return false;
    m_ownerHwnd = ownerHwnd;
    if (m_hwnd && IsWindow(m_hwnd)) return true;
    if (!RegisterMessageWindowClass()) return false;

    // IMPORTANT: a real (but hidden) top-level window, NOT an HWND_MESSAGE
    // message-only window. A message-only window can receive the tray callback
    // but can NEVER become the foreground window. TrackPopupMenu requires its
    // owner to be foreground (via SetForegroundWindow) so the menu auto-dismisses
    // when the user clicks elsewhere; with a message-only window that call had to
    // fall back to the fb2k main window, which is exactly why a tray right-click
    // pulled the main window to the foreground (tray bug 5). A hidden WS_POPUP
    // top-level window (WS_EX_TOOLWINDOW keeps it off the taskbar and Alt-Tab,
    // never ShowWindow'd so it stays invisible) CAN be foregrounded, so it owns
    // the popup menu without disturbing the main window.
    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kTrayMessageWindowClass, L"",
        WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
        GetModuleHandleW(nullptr), this);
    return m_hwnd != nullptr;
}

LRESULT CALLBACK TrayIcon::MessageWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TrayIcon* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<TrayIcon*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self && msg == TrayIcon::kCallbackMessage) {
        return self->HandleTrayCallback(wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool TrayIcon::Create(HWND hwnd, HICON hIcon, const char* tooltip) {
    if (m_created) return true;
    if (!EnsureMessageWindow(hwnd)) return false;
    m_callbackMsg = kCallbackMessage;
    m_cachedIcon = hIcon;
    m_cachedTooltip = tooltip ? tooltip : "";

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    // Bind the icon identity (hWnd + uID) to our dedicated hidden top-level window,
    // NOT the foobar2000 main window. The owner `hwnd` arg is only used as the
    // popup-menu owner (m_ownerHwnd, set in EnsureMessageWindow). Registering on
    // m_hwnd is what makes this an INDEPENDENT tray entry: tray callbacks reach
    // MessageWindowProc -> HandleTrayCallback (so our click/menu/event handling
    // runs instead of fb2k's native main-window behaviour), and it keeps the
    // identity consistent with the NIM_MODIFY/NIM_DELETE calls below which all
    // use m_hwnd. Using `hwnd` here was the root cause of tray bugs 1/2/3:
    // the icon was owned by the fb2k main window (native menu/click) while
    // SetIcon/SetTooltip/ShowBalloon targeted a non-existent (m_hwnd,1) icon.
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = m_callbackMsg;
    nid.hIcon = hIcon;
    if (tooltip) wcsncpy_s(nid.szTip, _countof(nid.szTip), Utf8ToWide(tooltip).c_str(), _TRUNCATE);

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        m_ownerHwnd = nullptr;
        return false;
    }
    m_created = true;
    return true;
}

void TrayIcon::RecreateAfterShellRestart() {
    // No-op when we never created an icon, or have been explicitly destroyed.
    if (!m_hwnd) return;
    // The previous NIM_ADD was wiped by explorer.exe restart -- force a
    // fresh registration even though m_created is still true.
    m_created = false;

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = m_callbackMsg;
    nid.hIcon = m_cachedIcon;
    if (!m_cachedTooltip.empty()) {
        wcsncpy_s(nid.szTip, _countof(nid.szTip),
            Utf8ToWide(m_cachedTooltip).c_str(), _TRUNCATE);
    }
    if (Shell_NotifyIconW(NIM_ADD, &nid)) m_created = true;
}

void TrayIcon::Destroy() {
    if (!m_created) {
        if (m_hwnd) DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        m_ownerHwnd = nullptr;
        return;
    }
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    m_created = false;
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
    m_hwnd = nullptr;
    m_ownerHwnd = nullptr;
    m_cachedIcon = nullptr;
    m_cachedTooltip.clear();

    if (!m_menuIdMap.empty()) {
        m_menuIdMap.clear();
    }
}

bool TrayIcon::SetIcon(HICON hIcon) {
    if (!m_created) return false;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON;
    nid.hIcon = hIcon;
    bool ok = Shell_NotifyIconW(NIM_MODIFY, &nid) != FALSE;
    if (ok) m_cachedIcon = hIcon;
    return ok;
}

bool TrayIcon::SetTooltip(const char* tooltip) {
    if (!m_created) return false;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_TIP;
    wcsncpy_s(nid.szTip, _countof(nid.szTip), Utf8ToWide(tooltip).c_str(), _TRUNCATE);
    bool ok = Shell_NotifyIconW(NIM_MODIFY, &nid) != FALSE;
    if (ok) m_cachedTooltip = tooltip ? tooltip : "";
    return ok;
}

bool TrayIcon::ShowBalloon(const char* title, const char* message, DWORD iconType) {
    if (!m_created) return false;
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = iconType;
    wcsncpy_s(nid.szInfoTitle, _countof(nid.szInfoTitle), Utf8ToWide(title).c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, _countof(nid.szInfo), Utf8ToWide(message).c_str(), _TRUNCATE);
    return Shell_NotifyIconW(NIM_MODIFY, &nid) != FALSE;
}

void TrayIcon::SetContextMenu(const std::vector<TrayMenuItem>& items) {
    // Full replace into customPosition zone (default Top).
    int zone = static_cast<int>(m_menuConfig.customPosition);
    if (zone < 0 || zone > 2) zone = 0;
    m_zones[zone] = items;
}

void TrayIcon::AppendMenuItems(const std::vector<TrayMenuItem>& items, TrayMenuPosition position) {
    int zone = static_cast<int>(position);
    if (zone < 0 || zone > 2) zone = 0;
    auto& z = m_zones[zone];
    z.insert(z.end(), items.begin(), items.end());
}

int TrayIcon::RemoveMenuItems(const std::vector<std::string>& ids) {
    if (ids.empty()) return 0;
    int removed = 0;
    auto matchId = [&](const TrayMenuItem& it) {
        return std::find(ids.begin(), ids.end(), it.id) != ids.end();
    };
    for (auto& z : m_zones) {
        auto before = z.size();
        z.erase(std::remove_if(z.begin(), z.end(), matchId), z.end());
        removed += static_cast<int>(before - z.size());
    }
    return removed;
}

void TrayIcon::ClearMenuItems(TrayMenuPosition position) {
    int zone = static_cast<int>(position);
    if (zone < 0 || zone > 2) zone = 0;
    m_zones[zone].clear();
}

void TrayIcon::ClearAllMenuItems() {
    for (auto& z : m_zones) z.clear();
}

std::vector<TrayMenuItem> TrayIcon::GetMenuItems() const {
    std::vector<TrayMenuItem> out;
    for (const auto& z : m_zones) {
        out.insert(out.end(), z.begin(), z.end());
    }
    return out;
}

static bool TraySetItemStateRecursive(std::vector<TrayMenuItem>& items,
                                      const std::string& id,
                                      std::optional<bool> checked,
                                      std::optional<bool> enabled) {
    for (auto& it : items) {
        if (it.id == id) {
            if (checked.has_value()) it.checked = *checked;
            if (enabled.has_value()) it.enabled = *enabled;
            return true;
        }
        if (!it.submenu.empty() &&
            TraySetItemStateRecursive(it.submenu, id, checked, enabled)) {
            return true;
        }
    }
    return false;
}

bool TrayIcon::SetMenuItemState(const std::string& id,
                                std::optional<bool> checked,
                                std::optional<bool> enabled) {
    if (id.empty()) return false;
    for (auto& zone : m_zones) {
        if (TraySetItemStateRecursive(zone, id, checked, enabled)) return true;
    }
    return false;
}

std::vector<TrayMenuItem> TrayIcon::BuildPlaybackDefaults() const {
    // Built-in playback controls when TrayMenuConfig.showPlaybackControls=true.
    // ids prefixed with "_pb_" so dispatch can short-circuit straight to
    // playback_control without leaving the C++ side (mirrors taskbar defaults).
    std::vector<TrayMenuItem> items;
    auto add = [&](const char* id, const char* label) {
        TrayMenuItem m;
        m.id = id;
        m.label = label;
        m.type = "normal";
        m.enabled = true;
        m.visible = true;
        items.push_back(std::move(m));
    };
    add("_pb_playPause", "Play / Pause");
    add("_pb_prev",      "Previous Track");
    add("_pb_next",      "Next Track");
    add("_pb_stop",      "Stop");
    return items;
}

std::vector<TrayMenuItem> TrayIcon::BuildSystemDefaults() const {
    std::vector<TrayMenuItem> items;
    TrayMenuItem exitItem;
    exitItem.id = "_sys_exit";
    exitItem.label = "Exit foobar2000";
    exitItem.type = "normal";
    items.push_back(std::move(exitItem));
    return items;
}

HMENU TrayIcon::BuildMenu(const std::vector<TrayMenuItem>& items, int& cmdId) {
    HMENU hMenu = CreatePopupMenu();
    for (const auto& item : items) {
        if (!item.visible) continue;
        if (item.type == "separator") {
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        } else if (item.type == "nowplaying") {
            // Native degrade: a disabled header line (TrackPopupMenu can't host
            // album art). Prefer the title; fall back to label.
            std::string text = !item.title.empty() ? item.title : item.label;
            if (!item.subtitle.empty()) text += "  -  " + item.subtitle;
            if (text.empty()) text = " ";
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, Utf8ToWide(text).c_str());
        } else if (item.type == "rating") {
            // Native degrade: a "1..N stars" submenu; each pick reports a value.
            HMENU hSub = CreatePopupMenu();
            const int levels = taskbar_tray_contracts::RatingNativeLevelCount();
            int curRating = item.value;
            if (curRating < 0) curRating = 0;
            if (curRating > levels) curRating = levels;
            for (int k = 1; k <= levels; ++k) {
                UINT flags = MF_STRING;
                if (k <= curRating) flags |= MF_CHECKED;
                int id = cmdId++;
                m_menuValueMap[id] = { item.id, k };
                std::wstring lbl = std::to_wstring(k) + L" \u2605";
                AppendMenuW(hSub, flags, id, lbl.c_str());
            }
            int clearId = cmdId++;
            m_menuValueMap[clearId] = { item.id, 0 };
            AppendMenuW(hSub, MF_STRING, clearId, L"Clear");
            UINT pflags = MF_POPUP;
            if (!item.enabled) pflags |= MF_GRAYED;
            std::string plabel = !item.label.empty() ? item.label : "Rating";
            AppendMenuW(hMenu, pflags, reinterpret_cast<UINT_PTR>(hSub),
                Utf8ToWide(plabel).c_str());
        } else if (item.type == "slider") {
            // Native degrade: a quantised "level" submenu (endpoints inclusive);
            // each pick reports a value.
            HMENU hSub = CreatePopupMenu();
            int mn = item.minValue, mx = item.maxValue;
            if (mx < mn) std::swap(mn, mx);
            const int stops = taskbar_tray_contracts::SliderNativeStopCount();
            const int span = stops > 1 ? stops - 1 : 1;
            for (int s = 0; s < stops; ++s) {
                int val = mn + (mx - mn) * s / span;
                UINT flags = MF_STRING;
                if (val == item.value) flags |= MF_CHECKED;
                int id = cmdId++;
                m_menuValueMap[id] = { item.id, val };
                AppendMenuW(hSub, flags, id, std::to_wstring(val).c_str());
            }
            UINT pflags = MF_POPUP;
            if (!item.enabled) pflags |= MF_GRAYED;
            std::string plabel = !item.label.empty() ? item.label : "Volume";
            AppendMenuW(hMenu, pflags, reinterpret_cast<UINT_PTR>(hSub),
                Utf8ToWide(plabel).c_str());
        } else if (item.type == "submenu" && !item.submenu.empty()) {
            HMENU hSub = BuildMenu(item.submenu, cmdId);
            AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSub),
                Utf8ToWide(item.label).c_str());
        } else {
            // Plain item path. The "segmented" rich kind has no TrackPopupMenu
            // equivalent, so it intentionally lands here too and degrades to a
            // plain text item (clicking it reports {id} with no segment value).
            // A full native degrade (e.g. a submenu of segments) is a non-goal;
            // segmented is webview-only. This keeps the native backend crash-free.
            UINT flags = MF_STRING;
            if (!item.enabled) flags |= MF_GRAYED;
            if (item.checked) flags |= MF_CHECKED;
            int id = cmdId++;
            m_menuIdMap[id] = item.id;
            AppendMenuW(hMenu, flags, id, Utf8ToWide(item.label).c_str());
        }
    }
    return hMenu;
}

namespace {
// TrayMenuItem -> menu.* item JSON 转换器（webview 分支用）。与原生 BuildMenu 语义对齐：
// 过滤 visible=false；submenu 仅 type=="submenu" && 非空；渲染器靠 checked 打勾、
// enabled===false 置灰、id 用于 select、label 显示。富类型（nowplaying/rating/slider）
// 透传 type 及其字段（cover/title/subtitle/value/min/max），并放行 icon/cover —— 仅
// webview 分支渲染；native 分支由 BuildMenu 降级处理。富类型判定走 TaskbarTrayContracts
// 的共享契约 taskbar_tray_contracts::IsRichTrayItemType（与 native 降级 / 契约测试同源）。
void CollectRealIds(const std::vector<TrayMenuItem>& items, std::unordered_set<std::string>& out) {
    for (const auto& it : items) {
        if (!it.id.empty()) out.insert(it.id);
        if (!it.submenu.empty()) CollectRealIds(it.submenu, out);
    }
}

json TrayItemToMenuJson(const TrayMenuItem& m,
                        const std::unordered_set<std::string>& realIds,
                        std::unordered_map<std::string, std::string>& syntheticToEmpty,
                        int& synthSeq) {
    if (m.type == "separator") {
        return json{ {"type", "separator"} };
    }
    json j;
    j["label"] = m.label;
    j["enabled"] = m.enabled;
    if (m.checked) j["checked"] = true;

    // Rich types carry their kind + payload so the self-drawn renderer can
    // branch. icon/cover are passed through here (webview backend only).
    if (taskbar_tray_contracts::IsRichTrayItemType(m.type)) j["type"] = m.type;
    if (!m.icon.empty()) j["icon"] = m.icon;
    // Inline monochrome SVG icon passthrough (webview backend only; native
    // ignores it). Emitted only when both viewBox and content are present.
    if (taskbar_tray_contracts::TrayItemKindRendersIcon(m.type) &&
        taskbar_tray_contracts::TrayItemHasRenderableIconSvg(m.iconSvgViewBox, m.iconSvgContent))
        j["iconSvg"] = { {"viewBox", m.iconSvgViewBox}, {"content", m.iconSvgContent} };
    if (m.type == "nowplaying") {
        if (!m.cover.empty()) j["cover"] = m.cover;
        if (!m.title.empty()) j["title"] = m.title;
        if (!m.subtitle.empty()) j["subtitle"] = m.subtitle;
    } else if (m.type == "rating") {
        j["value"] = m.value;
    } else if (m.type == "slider") {
        j["value"] = m.value;
        j["min"] = m.minValue;
        j["max"] = m.maxValue;
    } else if (m.type == "segmented") {
        // segmented：发选中段索引 value + 各段（label / 可渲染 iconSvg / enabled）。
        // 渲染器据 it.segments 渲控件；点击段经 menu.__valueChanged 走现有 value 通道
        // → tray:menuItemClicked{id,value} 且不关菜单（与 rating/slider 同源）。
        j["value"] = m.value;
        json segs = json::array();
        for (const auto& s : m.segments) {
            json sj;
            if (!s.label.empty()) sj["label"] = s.label;
            if (taskbar_tray_contracts::TrayItemHasRenderableIconSvg(s.iconSvgViewBox, s.iconSvgContent))
                sj["iconSvg"] = { {"viewBox", s.iconSvgViewBox}, {"content", s.iconSvgContent} };
            sj["enabled"] = s.enabled;
            segs.push_back(std::move(sj));
        }
        j["segments"] = segs;
    }

    if (m.type == "submenu" && !m.submenu.empty()) {
        json sub = json::array();
        for (const auto& s : m.submenu) {
            if (!s.visible) continue;
            sub.push_back(TrayItemToMenuJson(s, realIds, syntheticToEmpty, synthSeq));
        }
        j["submenu"] = sub;
        if (!m.id.empty()) j["id"] = m.id;
        return j;
    }

    // leaf：必须带 id 才能被渲染器判为可导航。无 id 时合成不碰撞占位 id，并精确映射回
    // 空 id（与 native m_menuItemCb("") 等价）。禁止用前缀判断还原——仅凭映射命中。
    if (!m.id.empty()) {
        j["id"] = m.id;
    } else {
        std::string synth;
        do {
            synth = "__trayidx_" + std::to_string(synthSeq++);
        } while (realIds.contains(synth) || syntheticToEmpty.contains(synth));
        syntheticToEmpty[synth] = std::string();
        j["id"] = synth;
    }
    return j;
}

json TrayItemsToMenuJson(const std::vector<TrayMenuItem>& items,
                         std::unordered_map<std::string, std::string>& syntheticToEmpty) {
    std::unordered_set<std::string> realIds;
    CollectRealIds(items, realIds);
    int synthSeq = 0;
    json arr = json::array();
    for (const auto& it : items) {
        if (!it.visible) continue;
        arr.push_back(TrayItemToMenuJson(it, realIds, syntheticToEmpty, synthSeq));
    }
    return arr;
}
}  // namespace

std::vector<TrayMenuItem> TrayIcon::ComposeMenu() const {
    // Compose: [top] [playback (defaults + user)] [bottom (user + system defaults)].
    // Separators inserted only between non-empty zones. Shared by native + webview.
    std::vector<TrayMenuItem> composed;
    auto appendZone = [&](const std::vector<TrayMenuItem>& items, bool addLeadingSeparator) {
        if (items.empty()) return;
        if (addLeadingSeparator && !composed.empty()) {
            TrayMenuItem sep; sep.type = "separator";
            composed.push_back(sep);
        }
        composed.insert(composed.end(), items.begin(), items.end());
    };

    appendZone(m_zones[(int)TrayMenuPosition::Top], false);

    std::vector<TrayMenuItem> playback = m_zones[(int)TrayMenuPosition::Playback];
    if (m_menuConfig.showPlaybackControls) {
        auto defaults = BuildPlaybackDefaults();
        playback.insert(playback.begin(), defaults.begin(), defaults.end());
    }
    appendZone(playback, true);

    std::vector<TrayMenuItem> bottom = m_zones[(int)TrayMenuPosition::Bottom];
    if (m_menuConfig.showSystemItems) {
        auto defaults = BuildSystemDefaults();
        bottom.insert(bottom.end(), defaults.begin(), defaults.end());
    }
    appendZone(bottom, true);

    return composed;
}

void TrayIcon::RouteSelectedId(const std::string& id) {
    // Built-in id routing for default playback / system items. Shared by the
    // native (sync, after TrackPopupMenu) and webview (async, via select sink) paths.
    if (id.starts_with("_pb_")) {
        try {
            auto pc = playback_control::get();
            if      (id == "_pb_playPause") pc->play_or_pause();
            else if (id == "_pb_prev")      pc->previous();
            else if (id == "_pb_next")      pc->next();
            else if (id == "_pb_stop")      pc->stop();
        } catch (...) {}
        return;
    }
    if (id == "_sys_exit") {
        try {
            HWND mainWnd = core_api::get_main_window();
            if (mainWnd) PostMessage(mainWnd, WM_CLOSE, 0, 0);
        } catch (...) {}
        return;
    }

    if (m_menuItemCb) m_menuItemCb(id);
}

void TrayIcon::RouteValueChanged(const std::string& id, int value) {
    // Rich-item (rating / slider) value change. Shared by the native (sync,
    // submenu pick) and webview (async, value sink) paths. The menu stays open
    // on the webview side; the native popup has already closed by this point.
    if (m_menuItemValueCb) m_menuItemValueCb(id, value);
}

void TrayIcon::ShowContextMenu() {
    if (!m_hwnd) return;

    POINT pt;
    GetCursorPos(&pt);

    // Fire tray:beforeContextMenu as an asynchronous notification. The bridge
    // dispatch does NOT block on JS, so frontend mutations in the handler only
    // take effect on the NEXT right-click.
    if (m_beforeMenuCb) {
        try { m_beforeMenuCb(pt.x, pt.y); } catch (...) {}
    }

    std::vector<TrayMenuItem> composed = ComposeMenu();
    if (composed.empty()) return;

    // Now-playing smart fallback (generic): when config.autoNowPlaying is on, fill
    // any nowplaying field the frontend left empty (frontend-first, backend-fallback).
    // cover auto-fill is webview-only; the native degrade fills text (title/artist) only.
    AutoFillNowPlaying(composed, m_menuConfig.autoNowPlaying,
                       m_menuConfig.render == TrayMenuRender::WebView);

    // render:'webview': drive the self-drawn overlay instead of
    // TrackPopupMenu. Selection routes back through tray:menuItemClicked via the
    // owner-mode select sink; id-less leaves use a per-show synthetic-id map that
    // restores the empty id (native parity). No menu:select / menu:dismiss leak.
    if (m_menuConfig.render == TrayMenuRender::WebView) {
        PFC_ASSERT(core_api::is_main_thread());
        std::unordered_map<std::string, std::string> syntheticToEmpty;
        json items = TrayItemsToMenuJson(composed, syntheticToEmpty);
        // 用 shared_ptr 承载本次 show 的 synthetic 映射：lambda 捕获为 nothrow，
        // 避免每次 std::function 拷贝都复制整张表（亦消解 bugprone-exception-escape）。
        auto synthMap = std::make_shared<const std::unordered_map<std::string, std::string>>(
            std::move(syntheticToEmpty));
        MenuOverlayHost::GetInstance().Show(
            items, pt.x, pt.y,
            [this, synthMap](const std::string& id) {
                try {
                    auto it = synthMap->find(id);
                    RouteSelectedId(it != synthMap->end() ? it->second : id);
                } catch (...) {}
            },
            nullptr,
            // 字段顺序须与 MenuShowOptions 一致：windowModel, css, cssReplace, backdrop, backdropDarkMode, closeAnimationMs。
            MenuShowOptions{ MenuWindowModel::ContentSized, m_menuConfig.css, m_menuConfig.cssReplace, m_menuConfig.backdrop, m_menuConfig.backdropDarkMode, m_menuConfig.closeAnimationMs },   // tray 用内容尺寸窗，浮于任务栏之上；透传前端样式接管 + DWM 背景 + 退场动画时长
            // value sink: rich items (rating/slider) report value WITHOUT closing.
            // rating/slider always carry a real id, so the synthetic remap is a
            // no-op for them, but we resolve through it for consistency.
            [this, synthMap](const std::string& id, int value) {
                try {
                    auto it = synthMap->find(id);
                    RouteValueChanged(it != synthMap->end() ? it->second : id, value);
                } catch (...) {}
            });
        return;
    }

    // render:'native' (default): existing Win32 TrackPopupMenu path. Own the menu
    // with our hidden top-level window (m_hwnd), NOT the main window, so the
    // mandatory SetForegroundWindow does not pull fb2k to the foreground (tray
    // bug 5). The trailing PostMessage(WM_NULL) is the documented MSDN workaround.
    m_menuIdMap.clear();
    m_menuValueMap.clear();
    int cmdId = 1;
    HMENU hMenu = BuildMenu(composed, cmdId);

    SetForegroundWindow(m_hwnd);
    int result = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
        pt.x, pt.y, 0, m_hwnd, nullptr);
    PostMessageW(m_hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);

    if (result <= 0) return;
    // Value-bearing picks (degraded rating/slider submenus) route first.
    auto vit = m_menuValueMap.find(result);
    if (vit != m_menuValueMap.end()) {
        RouteValueChanged(vit->second.first, vit->second.second);
        return;
    }
    auto it = m_menuIdMap.find(result);
    if (it == m_menuIdMap.end()) return;
    RouteSelectedId(it->second);
}

LRESULT TrayIcon::HandleTrayCallback(WPARAM /*wParam*/, LPARAM lParam) {
    UINT event = LOWORD(lParam);
    int x = 0, y = 0;
    POINT pt;
    GetCursorPos(&pt);
    x = pt.x; y = pt.y;

    switch (event) {
        case WM_LBUTTONUP:
            if (m_clickCb) m_clickCb(0, x, y);
            break;
        case WM_RBUTTONUP:
            ShowContextMenu();
            break;
        case WM_LBUTTONDBLCLK:
            if (m_doubleClickCb) m_doubleClickCb(x, y);
            break;
    }
    return 0;
}
