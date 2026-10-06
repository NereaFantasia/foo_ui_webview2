// UiApi.cpp - UI Components API
// Provides custom context menus, toast notifications, and system notifications
// 形状由 src/api/schema/ui.ts 声明，结构体与参数解析来自生成的 UiSchema.h；
// ui.showContextMenu 的 handler 在 WindowApi.cpp。

#include "pch.h"
#include "api/UiApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "api/EventEmit.h"
#include "api/TypedApi.h"
#include "api/generated/UiSchema.h"
#include "window/MainWindow.h"
#include "webview/WebViewHost.h"
#include <map>

namespace {
    using json = nlohmann::json;
    namespace ui = api::ui;

    // Get main window handle
    HWND GetMainWindowHandle() {
        HWND hwnd = core_api::get_main_window();
        if (hwnd) return hwnd;
        return GetActiveWindow();
    }

    //==========================================================================
    // Custom Menu State
    //==========================================================================

    struct MenuItemInfo {
        std::string id;
        std::string label;
        bool enabled;
    };

    std::map<int, MenuItemInfo> g_menuItems;
    int g_nextMenuId = 1000;

    //==========================================================================
    // Helper: Build popup menu recursively
    //==========================================================================
    HMENU BuildPopupMenu(const std::vector<ui::UiMenuItem>& items, std::map<int, MenuItemInfo>& menuItems, int& nextId) {
        HMENU hMenu = CreatePopupMenu();

        for (const auto& item : items) {
            if (item.type == "separator") {
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            } else if (item.submenu) {
                // Submenu
                const std::string label = item.label.value_or("");
                HMENU hSubMenu = BuildPopupMenu(*item.submenu, menuItems, nextId);
                AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSubMenu),
                    Utf8ToWide(label).c_str());
            } else {
                // Regular item
                const std::string id = item.id.value_or("");
                const std::string label = item.label.value_or("");
                const std::string shortcut = item.shortcut.value_or("");

                // Append shortcut to label if provided
                std::wstring displayLabel = Utf8ToWide(label);
                if (!shortcut.empty()) {
                    displayLabel += L"\t" + Utf8ToWide(shortcut);
                }

                int menuId = nextId++;

                UINT flags = MF_STRING;
                if (!item.enabled) flags |= MF_GRAYED;
                if (item.checked) flags |= MF_CHECKED;

                AppendMenuW(hMenu, flags, menuId, displayLabel.c_str());

                // Store menu item info
                MenuItemInfo info;
                info.id = id;
                info.label = label;
                info.enabled = item.enabled;
                menuItems[menuId] = info;
            }
        }

        return hMenu;
    }

    //==========================================================================
    // ui.showCustomMenu - Show custom context menu
    //==========================================================================
    api::Result<ui::ShowCustomMenuResult> UiShowCustomMenu(const ui::ShowCustomMenuParams& p, const CallerContext& caller) {
        const int itemCount = static_cast<int>(p.items.size());
        console::printf("[UiApi] showCustomMenu: x=%lld, y=%lld, items=%d, has_callerHwnd=%s",
            static_cast<long long>(p.x), static_cast<long long>(p.y), itemCount, caller.callerHwnd ? "true" : "false");

        // 面板模式: 调用方 HWND 是面板 HWND（DOM 坐标相对于此窗口）
        // 独立窗口: 回退到主窗口
        HWND panelHwnd = (caller.callerHwnd && IsWindow(caller.callerHwnd)) ? caller.callerHwnd : nullptr;
        HWND hwnd = panelHwnd ? panelHwnd : GetMainWindowHandle();
        if (!hwnd) {
            console::print("[UiApi] showCustomMenu: NO HWND available!");
            return api::Fail("Window not available", ApiErrorCode::OPERATION_FAILED);
        }

        // 获取顶级窗口（用于 SetForegroundWindow，子窗口不支持）
        HWND topLevel = ::GetAncestor(hwnd, GA_ROOT);
        if (!topLevel) topLevel = hwnd;

        console::printf("[UiApi] showCustomMenu: panelHwnd=0x%p, hwnd=0x%p, topLevel=0x%p",
            (void*)panelHwnd, (void*)hwnd, (void*)topLevel);

        // Clear previous menu items
        g_menuItems.clear();
        g_nextMenuId = 1000;

        // Build menu
        HMENU hMenu = BuildPopupMenu(p.items, g_menuItems, g_nextMenuId);

        if (!hMenu) {
            return api::Fail("Failed to create menu", ApiErrorCode::OPERATION_FAILED);
        }

        // 直接使用系统光标位置（最可靠，不受 DPI/CSS 像素差异影响）
        // JS clientX/clientY 是 CSS 像素，在 DPI ≠ 100% 时与物理像素不匹配
        POINT pt;
        GetCursorPos(&pt);

        // Constrain menu position within screen bounds (use monitor rect instead of window rect)
        HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(hMonitor, &mi)) {
            if (pt.x < mi.rcWork.left) pt.x = mi.rcWork.left;
            if (pt.x > mi.rcWork.right) pt.x = mi.rcWork.right;
            if (pt.y < mi.rcWork.top) pt.y = mi.rcWork.top;
            if (pt.y > mi.rcWork.bottom) pt.y = mi.rcWork.bottom;
        }

        // 支持宽高参数 - 用于定义排除区域（菜单不会覆盖此区域）
        const int w = static_cast<int>(p.w);
        const int h = static_cast<int>(p.h);

        console::printf("[UiApi] showCustomMenu: screen pt=(%d,%d), menuItemCount=%d",
            (int)pt.x, (int)pt.y, GetMenuItemCount(hMenu));

        // TrackPopupMenu 要求窗口为前台窗口，否则可能静默失败
        SetForegroundWindow(topLevel);

        int result = 0;
        if (w > 0 && h > 0) {
            TPMPARAMS tpm = { sizeof(TPMPARAMS) };
            tpm.rcExclude.left = pt.x;
            tpm.rcExclude.top = pt.y;
            tpm.rcExclude.right = pt.x + w;
            tpm.rcExclude.bottom = pt.y + h;

            result = TrackPopupMenuEx(hMenu,
                TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY | TPM_VERTICAL,
                pt.x, pt.y + h, topLevel, &tpm);
        } else {
            result = TrackPopupMenu(hMenu,
                TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                pt.x, pt.y, 0, topLevel, nullptr);
        }

        PostMessage(topLevel, WM_NULL, 0, 0);

        DestroyMenu(hMenu);

        ui::ShowCustomMenuResult out;
        if (result > 0) {
            auto it = g_menuItems.find(result);
            if (it != g_menuItems.end()) {
                // Emit event for clicked item — 路由到调用者实例
                ui::MenuItemClickedPayload payload;
                payload.id = it->second.id;
                payload.label = it->second.label;
                api::emit::EmitTo<ui::events::MenuItemClicked>(caller, payload);
                out.selectedId = it->second.id;
                return out;
            }
        }

        // No selection (menu dismissed): selectedId stays null
        return out;
    }

    //==========================================================================
    // ui.showToast - Show toast notification (in WebView)
    //==========================================================================
    api::Result<void> UiShowToast(const ui::ShowToastParams& p, const CallerContext& caller) {
        // Emit event for frontend to handle rendering — 路由到调用者实例
        ui::ToastPayload payload;
        payload.message = p.message;
        payload.duration = p.duration;
        payload.type = p.type;
        payload.position = p.position;
        api::emit::EmitTo<ui::events::Toast>(caller, payload);
        return api::Ok();
    }

    //==========================================================================
    // Notification state
    //==========================================================================
    static NOTIFYICONDATAW g_notifyData = {};
    static bool g_notifyInitialized = false;
    static int g_notificationId = 0;

    //==========================================================================
    // ui.showNotification - Show system tray notification
    //==========================================================================
    api::Result<ui::ShowNotificationResult> UiShowNotification(const ui::ShowNotificationParams& p) {
        if (p.title.empty() && p.body.empty()) {
            return api::Fail("title or body is required", ApiErrorCode::INVALID_PARAMS);
        }

        HWND hwnd = GetMainWindowHandle();
        if (!hwnd) {
            return api::Fail("Window not available", ApiErrorCode::OPERATION_FAILED);
        }

        // Initialize notification icon if needed
        if (!g_notifyInitialized) {
            g_notifyData.cbSize = sizeof(NOTIFYICONDATAW);
            g_notifyData.hWnd = hwnd;
            g_notifyData.uID = 1;
            g_notifyData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            g_notifyData.uCallbackMessage = WM_USER + 100;
            g_notifyData.hIcon = nullptr;
            try { static_api_ptr_t<ui_control> fb_ui; g_notifyData.hIcon = fb_ui->get_main_icon(); } catch (...) {
                g_notifyData.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
            }
            wcscpy_s(g_notifyData.szTip, L"foo_ui_webview2");

            if (!Shell_NotifyIconW(NIM_ADD, &g_notifyData)) {
                return api::Fail("Failed to create notification icon", ApiErrorCode::OPERATION_FAILED);
            }
            g_notifyInitialized = true;
        }

        // Update notification
        g_notifyData.uFlags = NIF_INFO;
        wcsncpy_s(g_notifyData.szInfoTitle, Utf8ToWide(p.title).c_str(), _TRUNCATE);
        wcsncpy_s(g_notifyData.szInfo, Utf8ToWide(p.body).c_str(), _TRUNCATE);
        g_notifyData.dwInfoFlags = NIIF_INFO;
        if (p.silent) {
            g_notifyData.dwInfoFlags |= NIIF_NOSOUND;
        }
        g_notifyData.uTimeout = static_cast<UINT>(p.timeout);

        if (!Shell_NotifyIconW(NIM_MODIFY, &g_notifyData)) {
            return api::Fail("Failed to show notification", ApiErrorCode::OPERATION_FAILED);
        }

        ui::ShowNotificationResult result;
        result.id = ++g_notificationId;
        return result;
    }

    //==========================================================================
    // ui.hideNotification - Hide system notification
    //==========================================================================
    api::Result<void> UiHideNotification(const ui::HideNotificationParams&) {
        if (!g_notifyInitialized) {
            return api::Ok();  // Nothing to hide
        }

        // Clear the balloon tip
        g_notifyData.uFlags = NIF_INFO;
        g_notifyData.szInfo[0] = L'\0';
        g_notifyData.szInfoTitle[0] = L'\0';

        if (!Shell_NotifyIconW(NIM_MODIFY, &g_notifyData)) {
            return api::Fail("Failed to hide notification", ApiErrorCode::OPERATION_FAILED);
        }

        return api::Ok();
    }

} // anonymous namespace

//==========================================================================
// Register UI API
//==========================================================================
void RegisterUiApi() {
    // ui.showCustomMenu - Show custom context menu
    api::RegisterApi("ui.showCustomMenu", UiShowCustomMenu);

    // ui.showToast - Show toast notification (frontend renders)
    api::RegisterApi("ui.showToast", UiShowToast);

    // ui.showNotification - Show system notification
    api::RegisterApi("ui.showNotification", UiShowNotification);

    // ui.hideNotification - Hide system notification
    api::RegisterApi("ui.hideNotification", UiHideNotification);

    LOG("UI API registered (4 APIs)");
}
