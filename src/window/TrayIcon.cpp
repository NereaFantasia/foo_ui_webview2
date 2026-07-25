// TrayIcon.cpp - system tray icon management
#include "pch.h"
#include "window/TrayIcon.h"
#include "window/TaskbarTrayContracts.h"
#include "utils/IconLoader.h"
#include "api/BridgeCore.h"
#include <shellapi.h>
#include <foobar2000/SDK/playback_control.h>
#include <foobar2000/SDK/core_api.h>
#include <foobar2000/SDK/menu_helpers.h>  // standard_commands::run_main / guid_main_exit
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "window/MenuOverlayHost.h"
#include <foobar2000/SDK/album_art.h>
#include "utils/Base64.h"
#include "utils/ImageUtils.h"
#include "utils/StringUtils.h"

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
        title = StringUtils::SafeUtf8(t.c_str());
        artist = StringUtils::SafeUtf8(a.c_str());
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

const std::vector<TrayMenuItem>& TrayIcon::GetZoneItems(TrayMenuPosition position) const {
    int zone = static_cast<int>(position);
    if (zone < 0 || zone > 2) zone = 0;
    return m_zones[zone];
}

menu_limits::CheckResult TrayIcon::TrySetContextMenu(
    std::vector<TrayMenuItem> items, const std::optional<TrayMenuConfig>& config) {
    TrayMenuStorage storage;
    storage.zones[0] = m_zones[0];
    storage.zones[1] = m_zones[1];
    storage.zones[2] = m_zones[2];
    storage.config = m_menuConfig;
    auto breach = TryReplaceContextMenuZone(storage, std::move(items), config);
    if (!breach.ok) return breach;
    m_menuConfig = storage.config;
    m_zones[0] = std::move(storage.zones[0]);
    m_zones[1] = std::move(storage.zones[1]);
    m_zones[2] = std::move(storage.zones[2]);
    return menu_limits::CheckResult::Ok();
}

menu_limits::CheckResult TrayIcon::TryAppendMenuItems(
    std::vector<TrayMenuItem> items, TrayMenuPosition position) {
    TrayMenuStorage storage;
    storage.zones[0] = m_zones[0];
    storage.zones[1] = m_zones[1];
    storage.zones[2] = m_zones[2];
    storage.config = m_menuConfig;
    auto breach = TryAppendMenuItemsToStorage(storage, std::move(items), position);
    if (!breach.ok) return breach;
    m_zones[0] = std::move(storage.zones[0]);
    m_zones[1] = std::move(storage.zones[1]);
    m_zones[2] = std::move(storage.zones[2]);
    return menu_limits::CheckResult::Ok();
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
            // Providing checked (true or false) establishes checkable identity
            // so getMenuItems / overlay keep menuitemcheckbox semantics.
            if (checked.has_value()) {
                it.checked = *checked;
                it.checkable = true;
            }
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
    return BuildPlaybackDefaultItems();
}

std::vector<TrayMenuItem> TrayIcon::BuildSystemDefaults() const {
    return BuildSystemDefaultItems();
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
            // each pick reports a value. Orientation is ignored.
            // Constant slider (min==max) emits a single greyed display value and
            // no selectable duplicates / no value map entries.
            HMENU hSub = CreatePopupMenu();
            int mn = item.minValue, mx = item.maxValue;
            if (mx < mn) std::swap(mn, mx);
            if (mx == mn) {
                // Constant: one disabled display row, no value map / no picks.
                AppendMenuW(hSub, MF_STRING | MF_GRAYED, 0, std::to_wstring(mn).c_str());
            } else {
                const int stops = taskbar_tray_contracts::SliderNativeStopCount();
                const int span = stops > 1 ? stops - 1 : 1;
                // Deduplicate stop values so a narrow range never emits the same
                // selectable value twice (e.g. min=0 max=2 with 5 stops).
                int lastVal = mn - 1;
                for (int s = 0; s < stops; ++s) {
                    int val = mn + (mx - mn) * s / span;
                    if (s > 0 && val == lastVal) continue;
                    lastVal = val;
                    UINT flags = MF_STRING;
                    if (val == item.value) flags |= MF_CHECKED;
                    int id = cmdId++;
                    m_menuValueMap[id] = { item.id, val };
                    AppendMenuW(hSub, flags, id, std::to_wstring(val).c_str());
                }
            }
            UINT pflags = MF_POPUP;
            if (!item.enabled) pflags |= MF_GRAYED;
            std::string plabel = !item.label.empty() ? item.label : "Volume";
            AppendMenuW(hMenu, pflags, reinterpret_cast<UINT_PTR>(hSub),
                Utf8ToWide(plabel).c_str());
        } else if (item.type == "submenu" && !item.submenu.empty()) {
            HMENU hSub = BuildMenu(item.submenu, cmdId);
            // Defense: if every child was filtered out, do not attach an empty popup.
            if (GetMenuItemCount(hSub) <= 0) {
                DestroyMenu(hSub);
                UINT flags = MF_STRING;
                if (!item.enabled) flags |= MF_GRAYED;
                if (item.checked) flags |= MF_CHECKED;
                int id = cmdId++;
                m_menuIdMap[id] = ResolveTrayMenuItemAction(item);
                AppendMenuW(hMenu, flags, id, Utf8ToWide(item.label).c_str());
            } else {
                AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSub),
                    Utf8ToWide(item.label).c_str());
            }
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
            m_menuIdMap[id] = ResolveTrayMenuItemAction(item);
            AppendMenuW(hMenu, flags, id, Utf8ToWide(item.label).c_str());
        }
    }
    return hMenu;
}

namespace {
// TrayMenuItem -> menu.* item JSON 转换器（webview 分支用）。与原生 BuildMenu 语义对齐：
// 过滤 visible=false；submenu 仅 type=="submenu" && 非空；渲染器靠 checked 打勾、
// enabled===false 置灰、label 显示；选中身份改由 MenuOverlayHost 分配的 opaque token
// 承担（渲染器只回 token，不回 public id）。富类型（nowplaying/rating/slider/segmented）
// 透传 type 及其字段（cover/title/subtitle/value/min/max/segments），并放行 icon/cover ——
// 仅 webview 分支渲染；native 分支由 BuildMenu 降级处理。富类型判定走 TaskbarTrayContracts
// 的共享契约 taskbar_tray_contracts::IsRichTrayItemType（与 native 降级 / 契约测试同源）。
// 内置项额外携带内部 _origin / _builtinAction，供 overlay 建立 token 索引并按可信 origin
// 路由（渲染器忽略这些字段；用户项永不携带）。
json TrayItemToMenuJson(const TrayMenuItem& m, const char* zone = nullptr) {
    if (m.type == "separator") {
        json j = json{ {"type", "separator"} };
        if (zone && *zone) j["_zone"] = zone;
        return j;
    }
    json j;
    j["label"] = m.label;
    j["enabled"] = m.enabled;
    // Preserve checkable field existence (including checked:false) for ARIA
    // menuitemcheckbox mapping in the overlay.
    if (m.checkable || m.checked || m.type == "checkbox") j["checked"] = m.checked;
    if (zone && *zone) j["_zone"] = zone;

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
        // Orientation: emit only when vertical so the renderer can set
        // data-orientation; missing/horizontal = horizontal default.
        if (m.orientation == "vertical") j["orientation"] = "vertical";
    } else if (m.type == "segmented") {
        // segmented：发选中段索引 value + 各段（label / 可渲染 iconSvg / enabled）。
        // 点击段经 menu.__valueChanged 走现有 value 通道 → tray:menuItemClicked{id,value}
        // 且不关菜单（与 rating/slider 同源）。
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

    // Public id echoed to the frontend on select; empty stays empty. An id-less
    // item still gets an opaque token from MenuOverlayHost and routes to
    // tray:menuItemClicked{id:""} (native m_menuItemCb("") parity) — no synthetic
    // id required. Routing NEVER re-derives origin from this id.
    if (!m.id.empty()) j["id"] = m.id;
    // Internal routing identity for the overlay token index (renderer ignores
    // it; present only for built-ins). A user item never carries _origin, so a
    // user public id spelled "_pb_next" stays Origin::User and does not suppress
    // the separately injected trusted playback control with the same public id.
    const auto actionFields = TrayMenuItemActionFields(m);
    if (actionFields.IsStamped()) {
        j["_origin"] = *actionFields.origin;
        j["_builtinAction"] = *actionFields.builtin;
    }

    if (m.type == "submenu" && !m.submenu.empty()) {
        json sub = json::array();
        for (const auto& s : m.submenu) {
            if (!s.visible) continue;
            sub.push_back(TrayItemToMenuJson(s, zone));  // submenu inherits entry zone
        }
        j["submenu"] = sub;
    }
    return j;
}

json TrayItemsToMenuJson(const std::vector<TrayMenuItem>& items, const char* zone = nullptr) {
    json arr = json::array();
    for (const auto& it : items) {
        if (!it.visible) continue;
        arr.push_back(TrayItemToMenuJson(it, zone));
    }
    return arr;
}

// Flat WebView payload: effective zones flattened with `_zone` stamps.
// Separator attribution uses FlattenEffectiveZonesTagged (last emitted zone).
json TrayEffectiveZonesToFlatJson(const EffectiveTrayZones& z) {
    json arr = json::array();
    for (const auto& tagged : FlattenEffectiveZonesTagged(z)) {
        arr.push_back(TrayItemToMenuJson(tagged.item, tagged.zone.c_str()));
    }
    return arr;
}

// Zones WebView payload: only non-empty (visible) zones.
json TrayEffectiveZonesToZonesJson(const EffectiveTrayZones& z) {
    json zones = json::array();
    auto pushZone = [&](const char* id, const std::vector<TrayMenuItem>& items) {
        json arr = TrayItemsToMenuJson(items, id);
        if (arr.empty()) return;
        zones.push_back(json{ {"id", id}, {"items", std::move(arr)} });
    };
    pushZone("top", z.top);
    pushZone("playback", z.playback);
    pushZone("bottom", z.bottom);
    return zones;
}
}  // namespace

std::vector<TrayMenuItem> TrayIcon::ComposeMenu() const {
    // Single effective-zone authority shared by native + webview.
    return FlattenEffectiveZones(BuildEffectiveTrayZones(
        m_zones[(int)TrayMenuPosition::Top],
        m_zones[(int)TrayMenuPosition::Playback],
        m_zones[(int)TrayMenuPosition::Bottom],
        m_menuConfig));
}

void TrayIcon::RouteResolvedAction(const menu_action::ResolvedAction& action) {
    // Route by TRUSTED origin, never by a public-id prefix. Effective-menu
    // composition stamps defaults and the exact historical reserved-id
    // allowlist; arbitrary `_pb_*` / `_sys_*` ids remain user callbacks. Shared
    // by native (sync) and WebView (opaque-token resolved) paths. Normalize at
    // the routing boundary as a final fail-closed guard for any future caller.
    const auto normalized = menu_action::NormalizeResolvedAction(action);
    switch (menu_action::DecideRoute(normalized)) {
        case menu_action::RouteDecision::ExecutePlayback:
            try {
                auto pc = playback_control::get();
                switch (normalized.builtin) {
                    case menu_action::Builtin::PlayPause: pc->play_or_pause(); break;
                    case menu_action::Builtin::Previous:  pc->previous();      break;
                    case menu_action::Builtin::Next:      pc->next();          break;
                    case menu_action::Builtin::Stop:      pc->stop();          break;
                    default: break;
                }
            } catch (...) {}
            return;
        case menu_action::RouteDecision::ExecuteSystem:
            if (normalized.builtin == menu_action::Builtin::Exit) {
                // 托盘"退出"必须真正退出 foobar2000，绕开 closeToTray 隐藏逻辑。
                // 直接执行标准退出命令（等价 MainWindow::OnClose 的退出路径）；
                // 不再 PostMessage(WM_CLOSE)——否则在 closeToTray 开启时会被拦成隐藏到托盘。
                try {
                    standard_commands::run_main(standard_commands::guid_main_exit);
                } catch (...) {}
            }
            return;
        case menu_action::RouteDecision::FireUserCallback:
            if (m_menuItemCb) m_menuItemCb(normalized.publicId);
            return;
    }
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

    EffectiveTrayZones effective = BuildEffectiveTrayZones(
        m_zones[(int)TrayMenuPosition::Top],
        m_zones[(int)TrayMenuPosition::Playback],
        m_zones[(int)TrayMenuPosition::Bottom],
        m_menuConfig);
    // All-hidden / empty: do not create overlay or enter measure timeout.
    if (!effective.HasAnyVisible()) return;

    // Now-playing smart fallback (generic): when config.autoNowPlaying is on, fill
    // any nowplaying field the frontend left empty (frontend-first, backend-fallback).
    // cover auto-fill is webview-only; the native degrade fills text (title/artist) only.
    // Apply on effective zones first so flat/zones/native share one filled source.
    const bool webview = m_menuConfig.render == TrayMenuRender::WebView;
    AutoFillNowPlaying(effective.top, m_menuConfig.autoNowPlaying, webview);
    AutoFillNowPlaying(effective.playback, m_menuConfig.autoNowPlaying, webview);
    AutoFillNowPlaying(effective.bottom, m_menuConfig.autoNowPlaying, webview);

    std::vector<TrayMenuItem> composed = FlattenEffectiveZones(effective);

    // render:'webview': drive the self-drawn overlay instead of
    // TrackPopupMenu. Selection routes back through tray:menuItemClicked via the
    // owner-mode select sink, which now receives a token-resolved ResolvedAction
    // (public id + trusted origin) — id-less items resolve to publicId "" (native
    // parity) and built-ins route by origin, not id prefix. No menu:select /
    // menu:dismiss leak.
    if (webview) {
        PFC_ASSERT(core_api::is_main_thread());
        const bool useZones = m_menuConfig.layoutMode == TrayMenuLayoutMode::Zones;
        json payload = useZones
            ? TrayEffectiveZonesToZonesJson(effective)
            : TrayEffectiveZonesToFlatJson(effective);
        if (payload.empty()) return;

        MenuShowOptions opts{};
        opts.windowModel = MenuWindowModel::ContentSized;
        opts.overlayModel = useZones ? MenuOverlayModel::TrayZones : MenuOverlayModel::LegacyItems;
        opts.css = m_menuConfig.css;
        opts.cssReplace = m_menuConfig.cssReplace;
        opts.backdrop = m_menuConfig.backdrop;
        opts.backdropDarkMode = m_menuConfig.backdropDarkMode;
        opts.closeAnimationMs = m_menuConfig.closeAnimationMs;

        MenuOverlayHost::GetInstance().Show(
            payload, pt.x, pt.y,
            [this](const menu_action::ResolvedAction& action) {
                try { RouteResolvedAction(action); } catch (...) {}
            },
            nullptr,
            opts,
            // value sink: rich items (rating/slider/segmented) report value WITHOUT
            // closing. MenuOverlayHost resolves the opaque token to the item's
            // public id (empty stays empty) and validates the value before calling.
            [this](const std::string& id, int value) {
                try { RouteValueChanged(id, value); } catch (...) {}
            });
        return;
    }

    // render:'native' (default): existing Win32 TrackPopupMenu path. layoutMode is
    // ignored — native always flattens from effective zones. Own the menu with our
    // hidden top-level window (m_hwnd), NOT the main window, so the mandatory
    // SetForegroundWindow does not pull fb2k to the foreground (tray bug 5). The
    // trailing PostMessage(WM_NULL) is the documented MSDN workaround.
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
    RouteResolvedAction(it->second);
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
