// UiApi.cpp - UI Components API
// Provides custom context menus, toast notifications, and system notifications

#include "pch.h"
#include "api/UiApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "window/MainWindow.h"
#include "webview/WebViewHost.h"
#include <map>

namespace {
    using json = nlohmann::json;
    
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
    HMENU BuildPopupMenu(const json& items, std::map<int, MenuItemInfo>& menuItems, int& nextId) {
        HMENU hMenu = CreatePopupMenu();
        
        for (const auto& item : items) {
            std::string type = item.value("type", "item");
            
            if (type == "separator") {
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            } else if (item.contains("submenu")) {
                // Submenu
                std::string label = item.value("label", "");
                HMENU hSubMenu = BuildPopupMenu(item["submenu"], menuItems, nextId);
                AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSubMenu),
                    Utf8ToWide(label).c_str());
            } else {
                // Regular item
                std::string id = item.value("id", "");
                std::string label = item.value("label", "");
                bool enabled = item.value("enabled", true);
                std::string shortcut = item.value("shortcut", "");
                bool checked = item.value("checked", false);
                
                // Append shortcut to label if provided
                std::wstring displayLabel = Utf8ToWide(label);
                if (!shortcut.empty()) {
                    displayLabel += L"\t" + Utf8ToWide(shortcut);
                }
                
                int menuId = nextId++;
                
                UINT flags = MF_STRING;
                if (!enabled) flags |= MF_GRAYED;
                if (checked) flags |= MF_CHECKED;
                
                AppendMenuW(hMenu, flags, menuId, displayLabel.c_str());
                
                // Store menu item info
                MenuItemInfo info;
                info.id = id;
                info.label = label;
                info.enabled = enabled;
                menuItems[menuId] = info;
            }
        }
        
        return hMenu;
    }
    
    //==========================================================================
    // Helper: 获取调用者面板 HWND（用于坐标转换）
    // DOM clientX/clientY 相对于面板客户区，需要用面板 HWND 做 ClientToScreen
    //==========================================================================
    HWND GetCallerPanelHwnd(const json& params) {
        if (params.contains("_callerHwnd")) {
            auto hwnd = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
            if (hwnd && IsWindow(hwnd)) {
                return hwnd;
            }
        }
        return nullptr;
    }
    
    //==========================================================================
    // ui.showCustomMenu - Show custom context menu
    //==========================================================================
    json UiShowCustomMenu(const json& params) {
        if (!params.contains("items") || !params["items"].is_array()) {
            return {{"success", false}, {"error", "items array is required"}};
        }
        
        int x = params.value("x", 0);
        int y = params.value("y", 0);
        bool suppressDefault = params.value("suppressDefault", false);
        
        // 在菜单弹出前捕获 caller context，用于路由 menuItemClicked 事件
        auto caller = CallerContext::FromParams(params);
        
        int itemCount = (int)params["items"].size();
        console::printf("[UiApi] showCustomMenu: x=%d, y=%d, items=%d, has_callerHwnd=%s",
            x, y, itemCount, params.contains("_callerHwnd") ? "true" : "false");
        
        // 面板模式: _callerHwnd 是面板 HWND（DOM 坐标相对于此窗口）
        // 独立窗口: 回退到主窗口
        HWND panelHwnd = GetCallerPanelHwnd(params);
        HWND hwnd = panelHwnd ? panelHwnd : GetMainWindowHandle();
        if (!hwnd) {
            console::print("[UiApi] showCustomMenu: NO HWND available!");
            return {{"success", false}, {"error", "Window not available"}};
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
        HMENU hMenu = BuildPopupMenu(params["items"], g_menuItems, g_nextMenuId);
        
        if (!hMenu) {
            return {{"success", false}, {"error", "Failed to create menu"}};
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
        int w = params.value("w", 0);
        int h = params.value("h", 0);
        
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
        
        if (result > 0) {
            auto it = g_menuItems.find(result);
            if (it != g_menuItems.end()) {
                // Emit event for clicked item — 路由到调用者实例
                caller.EmitEvent("ui:menuItemClicked", {
                    {"id", it->second.id},
                    {"label", it->second.label}
                });
                
                return {
                    {"success", true},
                    {"selectedId", it->second.id}
                };
            }
        }
        
        return {
            {"success", true},
            {"selectedId", nullptr}  // No selection (menu dismissed)
        };
    }
    
    //==========================================================================
    // ui.showToast - Show toast notification (in WebView)
    //==========================================================================
    json UiShowToast(const json& params) {
        std::string message = params.value("message", "");
        int duration = params.value("duration", 3000);
        std::string type = params.value("type", "info");  // info, success, warning, error
        std::string position = params.value("position", "bottom-right");
        
        if (message.empty()) {
            return {{"success", false}, {"error", "message is required"}};
        }
        
        // Emit event for frontend to handle rendering — 路由到调用者实例
        auto caller = CallerContext::FromParams(params);
        caller.EmitEvent("ui:toast", {
            {"message", message},
            {"duration", duration},
            {"type", type},
            {"position", position}
        });
        
        return {{"success", true}};
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
    json UiShowNotification(const json& params) {
        std::string title = params.value("title", "");
        std::string body = params.value("body", "");
        bool silent = params.value("silent", false);
        int timeout = params.value("timeout", 5000);
        
        if (title.empty() && body.empty()) {
            return {{"success", false}, {"error", "title or body is required"}};
        }
        
        HWND hwnd = GetMainWindowHandle();
        if (!hwnd) {
            return {{"success", false}, {"error", "Window not available"}};
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
                return {{"success", false}, {"error", "Failed to create notification icon"}};
            }
            g_notifyInitialized = true;
        }
        
        // Update notification
        g_notifyData.uFlags = NIF_INFO;
        wcsncpy_s(g_notifyData.szInfoTitle, Utf8ToWide(title).c_str(), _TRUNCATE);
        wcsncpy_s(g_notifyData.szInfo, Utf8ToWide(body).c_str(), _TRUNCATE);
        g_notifyData.dwInfoFlags = NIIF_INFO;
        if (silent) {
            g_notifyData.dwInfoFlags |= NIIF_NOSOUND;
        }
        g_notifyData.uTimeout = timeout;
        
        if (!Shell_NotifyIconW(NIM_MODIFY, &g_notifyData)) {
            return {{"success", false}, {"error", "Failed to show notification"}};
        }
        
        int notificationId = ++g_notificationId;
        
        return {
            {"success", true},
            {"id", notificationId}
        };
    }
    
    //==========================================================================
    // ui.hideNotification - Hide system notification
    //==========================================================================
    json UiHideNotification(const json& /*params*/) {
        if (!g_notifyInitialized) {
            return {{"success", true}};  // Nothing to hide
        }
        
        // Clear the balloon tip
        g_notifyData.uFlags = NIF_INFO;
        g_notifyData.szInfo[0] = L'\0';
        g_notifyData.szInfoTitle[0] = L'\0';
        
        if (!Shell_NotifyIconW(NIM_MODIFY, &g_notifyData)) {
            return {{"success", false}, {"error", "Failed to hide notification"}};
        }
        
        return {{"success", true}};
    }
    
} // anonymous namespace

//==========================================================================
// Register UI API
//==========================================================================
void RegisterUiApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // ui.showCustomMenu - Show custom context menu
    bridge.RegisterApi("ui.showCustomMenu", UiShowCustomMenu);
    
    // ui.showToast - Show toast notification (frontend renders)
    bridge.RegisterApi("ui.showToast", UiShowToast);
    
    // ui.showNotification - Show system notification
    bridge.RegisterApi("ui.showNotification", UiShowNotification);
    
    // ui.hideNotification - Hide system notification
    bridge.RegisterApi("ui.hideNotification", UiHideNotification);
    
    LOG("UI API registered (4 APIs)");
}
