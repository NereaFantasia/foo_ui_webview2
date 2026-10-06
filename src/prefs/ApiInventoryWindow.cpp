// ============================================
// ApiInventoryWindow.cpp - 总览页「API 与服务...」打开的清单窗口
// 列出已注册的 API、外部插件的菜单命令、组件、输入格式、右键命令与 DSP。
// ============================================

#include "pch.h"
#include "prefs/PreferencesPage.h"
#include "prefs/PreferencesPageInternal.h"
#include "api/BridgeCore.h"
#include <uxtheme.h>
#include <algorithm>
#include <string>
#include "utils/I18n.h"
#include "utils/GuidUtils.h"

namespace webview_prefs {

// ============================================
// API List Dialog
// ============================================

namespace {

constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_V = 20;
constexpr int IDC_API_LIST_EDIT = 1001;

// 每窗口状态：字体与背景画刷统一在 WM_DESTROY 释放
struct ApiListState {
    HFONT monoFont = nullptr;
    HBRUSH bkBrush = nullptr;
    COLORREF bkColor = 0;
    COLORREF textColor = 0;
    bool isDark = false;
};

bool IsFb2kDarkMode() {
    auto api = ui_config_manager::tryGet();
    return api.is_valid() && api->is_dark_mode();
}

std::wstring WideFromUtf8(const char* s) {
    return pfc::stringcvt::string_wide_from_utf8(s ? s : "").get_ptr();
}

std::wstring GuidToWide(const GUID& guid) {
    return WideFromUtf8(GuidUtils::GuidToString(guid).c_str());
}

// 按当前 fb2k 深浅色重建配色，并同步标题栏与滚动条主题
void ApplyApiListTheme(HWND hwnd, ApiListState* state) {
    if (!state) return;

    state->isDark = IsFb2kDarkMode();
    // 本对话框不经 CCoreDarkModeHooks，深色用固定的深灰底与浅灰字自绘；浅色沿用系统 Window 配色
    state->bkColor = state->isDark ? RGB(32, 32, 32) : GetSysColor(COLOR_WINDOW);
    state->textColor = state->isDark ? RGB(222, 222, 222) : GetSysColor(COLOR_WINDOWTEXT);

    if (state->bkBrush) DeleteObject(state->bkBrush);
    state->bkBrush = CreateSolidBrush(state->bkColor);

    BOOL darkTitleBar = state->isDark ? TRUE : FALSE;
    ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_V,
        &darkTitleBar, sizeof(darkTitleBar));

    // EDIT 的非客户区滚动条只能靠 UxTheme 切换，WM_CTLCOLOR 管不到
    HWND hEdit = GetDlgItem(hwnd, IDC_API_LIST_EDIT);
    if (hEdit) {
        SetWindowTheme(hEdit, state->isDark ? L"DarkMode_Explorer" : nullptr, nullptr);
    }

    InvalidateRect(hwnd, nullptr, TRUE);
    if (hEdit) InvalidateRect(hEdit, nullptr, TRUE);
}

// mainmenu_commands_v2 组件（ESLyric 等）只注册一个父槽位，真正的子命令由
// dynamic_instantiate() 在运行时构建。不展开这棵树，枚举结果里只剩下不可执行
// 的容器 GUID。语义与 DiscoveryApi::CollectDynamicMenuNodes 保持一致。
constexpr int kMaxDynamicMenuDepth = 16;

void AppendDynamicMenuNodes(const mainmenu_node::ptr& node,
                            const std::wstring& pathPrefix,
                            const std::wstring& ownerGuid,
                            std::wstring& out,
                            int& count,
                            int depth) {
    if (!node.is_valid() || depth > kMaxDynamicMenuDepth) return;

    t_uint32 type = mainmenu_node::type_separator;
    try { type = node->get_type(); } catch (...) { return; }
    if (type == mainmenu_node::type_separator) return;

    pfc::string8 display;
    t_uint32 flags = 0;
    try { node->get_display(display, flags); } catch (...) {
        // 标签取不到时继续遍历子树，不整棵丢弃
    }

    std::wstring label = WideFromUtf8(display.get_ptr());
    std::wstring path = pathPrefix;
    // 部分组件把动态子树根节点的标签写成与静态父槽位同名，再拼一次会得到
    // "桌面歌词/桌面歌词/显示" 这样的重复路径
    const bool duplicatesOwnerLabel = (depth == 0 && label == pathPrefix);
    if (!label.empty() && !duplicatesOwnerLabel) {
        if (!path.empty()) path += L'/';
        path += label;
    }

    if (type == mainmenu_node::type_group) {
        t_size childCount = 0;
        try { childCount = node->get_children_count(); } catch (...) { return; }
        for (t_size i = 0; i < childCount; i++) {
            mainmenu_node::ptr child;
            try { child = node->get_child(i); } catch (...) { continue; }
            AppendDynamicMenuNodes(child, path, ownerGuid, out, count, depth + 1);
        }
        return;
    }

    GUID subGuid = pfc::guid_null;
    try { subGuid = node->get_guid(); } catch (...) {}
    if (subGuid == pfc::guid_null) return;  // 无法定址，无法执行

    out += L"    { path: \"" + path + L"\", guid: \"" + ownerGuid +
           L"\", subGuid: \"" + GuidToWide(subGuid) + L"\" },\r\n";
    count++;
}

} // anonymous namespace

// API 列表对话框窗口过程
static LRESULT CALLBACK ApiListWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<ApiListState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE:
        {
            RECT rc;
            GetClientRect(hwnd, &rc);

            state = new ApiListState();
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            // 创建只读编辑框
            HWND hEdit = CreateWindowExW(
                0,
                L"EDIT",
                L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | 
                ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                0, 0, rc.right, rc.bottom,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_API_LIST_EDIT)),
                (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE),
                nullptr);
            
            // 列表按等宽字体对齐各列；首选 Cascadia Code，其次 Consolas。
            HFONT hMonoFont = CreateFontW(
                -16,        // 字体高度 (负值表示字符高度，正值表示单元格高度)
                0,          // 字体宽度 (0=自动)
                0, 0,       // 倾斜和旋转角度
                FW_NORMAL,  // 字体粗细
                FALSE,      // 斜体
                FALSE,      // 下划线
                FALSE,      // 删除线
                DEFAULT_CHARSET,
                OUT_TT_PRECIS,      // 优先TrueType字体
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,  // ClearType渲染，更清晰
                FIXED_PITCH | FF_MODERN,
                L"Cascadia Code");  // 优先使用Cascadia Code
            
            // GetObjectW 读回的是创建时请求的 LOGFONT，不是实际映射到的字体：字体缺失时 CreateFontW
            // 仍返回句柄并由 GDI 代换，这个分支只在 CreateFontW 本身失败时才切到 Consolas。
            LOGFONTW lf = {};
            if (!GetObjectW(hMonoFont, sizeof(lf), &lf) || wcscmp(lf.lfFaceName, L"Cascadia Code") != 0) {
                DeleteObject(hMonoFont);
                hMonoFont = CreateFontW(
                    -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
            }
            SendMessageW(hEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);
            state->monoFont = hMonoFont;

            ApplyApiListTheme(hwnd, state);
            
            // 获取所有注册的 API
            auto apiNames = BridgeCore::GetInstance().GetRegisteredApiNames();
            
            // 按字母顺序排序
            std::sort(apiNames.begin(), apiNames.end());
            
            // 构建显示文本
            std::wstring text;
            text += L"//==============================================================================\r\n";
            text += L"// WebView2 UI - API & External Plugin Services Reference\r\n";
            text += L"// Usage: window.fb2k.invoke('method', {params})\r\n";
            text += L"//==============================================================================\r\n\r\n";
            
            // Section 1: Registered APIs
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 1: Registered APIs (" + std::to_wstring(apiNames.size()) + L" APIs)\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            text += L"const fb2kApis = [\r\n";
            
            for (size_t i = 0; i < apiNames.size(); i++) {
                std::wstring wname = pfc::stringcvt::string_wide_from_utf8(apiNames[i].c_str()).get_ptr();
                text += L"    \"" + wname + L"\"";
                if (i < apiNames.size() - 1) {
                    text += L",";
                }
                text += L"\r\n";
            }
            text += L"];\r\n\r\n";
            
            // Section 2: Main Menu Commands from external plugins
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 2: External Plugin Menu Commands\r\n";
            text += L"// Use: discovery.executeMainMenuCommand({ guid: '...' })\r\n";
            text += L"//\r\n";
            text += L"// GUIDs are compile-time constants inside each component, so they are\r\n";
            text += L"// identical on English and localized foobar2000 installs. Only the 'name'\r\n";
            text += L"// field follows the UI language - never match on it across machines.\r\n";
            text += L"//\r\n";
            text += L"// Entries with dynamic: true are parent slots created by\r\n";
            text += L"// mainmenu_commands_v2 (ESLyric and most SMP-era components). The parent\r\n";
            text += L"// GUID is only a container and is NOT executable on its own - see the\r\n";
            text += L"// dynamicMenuCommands list below for the executable children.\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<mainmenu_commands> e;
                service_ptr_t<mainmenu_commands> ptr;
                int cmdCount = 0;
                int dynamicParentCount = 0;
                // 动态子命令单独成表，避免与静态命令混在一起被误当作可直接执行
                std::wstring dynamicText;
                int dynamicCount = 0;

                text += L"const externalMenuCommands = [\r\n";
                while (e.next(ptr)) {
                    t_uint32 count = 0;
                    try { count = ptr->get_command_count(); } catch (...) { continue; }

                    service_ptr_t<mainmenu_commands_v2> v2;
                    const bool hasV2 = ptr->service_query_t(v2);

                    for (t_uint32 i = 0; i < count; i++) {
                        pfc::string8 name;
                        GUID cmdGuid = pfc::guid_null;
                        try { ptr->get_name(i, name); } catch (...) {}
                        try { cmdGuid = ptr->get_command(i); } catch (...) {}

                        std::wstring wname = WideFromUtf8(name.get_ptr());
                        std::wstring wguid = GuidToWide(cmdGuid);

                        bool isDynamic = false;
                        if (hasV2) {
                            try { isDynamic = v2->is_command_dynamic(i); } catch (...) {}
                        }

                        text += L"    { name: \"" + wname + L"\", guid: \"" + wguid + L"\"";
                        if (isDynamic) text += L", dynamic: true";
                        text += L" },\r\n";
                        cmdCount++;

                        if (!isDynamic) continue;
                        dynamicParentCount++;

                        mainmenu_node::ptr root;
                        try { root = v2->dynamic_instantiate(i); } catch (...) { continue; }
                        if (!root.is_valid()) continue;

                        AppendDynamicMenuNodes(root, wname, wguid, dynamicText, dynamicCount, 0);
                    }
                }
                text += L"];  // " + std::to_wstring(cmdCount) + L" commands, " +
                        std::to_wstring(dynamicParentCount) + L" dynamic parent slots\r\n\r\n";

                text += L"//------------------------------------------------------------------------------\r\n";
                text += L"// SECTION 2b: Dynamic Submenu Commands (expanded at runtime)\r\n";
                text += L"// Use: discovery.executeMainMenuCommand({ guid: '...', subGuid: '...' })\r\n";
                text += L"//\r\n";
                text += L"// These are the executable leaves of the dynamic: true parents above.\r\n";
                text += L"// Both guid and subGuid are required. subGuid may change between\r\n";
                text += L"// component versions, so resolve it at runtime via\r\n";
                text += L"// discovery.getMainMenuCommands() instead of hardcoding it.\r\n";
                text += L"//------------------------------------------------------------------------------\r\n\r\n";
                text += L"const dynamicMenuCommands = [\r\n";
                text += dynamicText;
                text += L"];  // " + std::to_wstring(dynamicCount) + L" dynamic commands\r\n\r\n";
            }
            
            // Section 3: Installed Components
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 3: Installed Components\r\n";
            text += L"// Use: discovery.getComponents()\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<componentversion> e;
                service_ptr_t<componentversion> ptr;
                text += L"const installedComponents = [\r\n";
                int compCount = 0;
                while (e.next(ptr)) {
                    pfc::string8 name, version;
                    ptr->get_component_name(name);
                    ptr->get_component_version(version);
                    
                    std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                    std::wstring wver = pfc::stringcvt::string_wide_from_utf8(version.get_ptr()).get_ptr();
                    
                    text += L"    { name: \"" + wname + L"\", version: \"" + wver + L"\" },\r\n";
                    compCount++;
                }
                text += L"];  // " + std::to_wstring(compCount) + L" components\r\n\r\n";
            }
            
            // Section 4: Input Formats
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 4: Supported Input Formats\r\n";
            text += L"// Use: discovery.getInputFormats()\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<input_file_type> eft;
                service_ptr_t<input_file_type> pft;
                text += L"const inputFormats = [\r\n";
                int formatCount = 0;
                while (eft.next(pft)) {
                    t_uint32 count = pft->get_count();
                    for (t_uint32 i = 0; i < count; i++) {
                        pfc::string8 name, mask;
                        pft->get_name(i, name);
                        pft->get_mask(i, mask);
                        
                        std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                        std::wstring wmask = pfc::stringcvt::string_wide_from_utf8(mask.get_ptr()).get_ptr();
                        
                        text += L"    { name: \"" + wname + L"\", mask: \"" + wmask + L"\" },\r\n";
                        formatCount++;
                    }
                }
                text += L"];  // " + std::to_wstring(formatCount) + L" formats\r\n\r\n";
            }
            
            // Section 5: Context Menu Commands (external plugins)
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 5: External Plugin Context Menu Commands\r\n";
            text += L"// Use: discovery.executeContextMenuCommand({ guid: '...' })\r\n";
            text += L"// Note: These commands operate on currently playing or selected tracks\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<contextmenu_item> e;
                service_ptr_t<contextmenu_item> ptr;
                int cmdCount = 0;
                text += L"const contextMenuCommands = [\r\n";
                while (e.next(ptr)) {
                    t_uint32 count = ptr->get_num_items();
                    for (t_uint32 i = 0; i < count; i++) {
                        pfc::string8 name, desc;
                        ptr->get_item_name(i, name);
                        ptr->get_item_description(i, desc);
                        GUID cmdGuid = ptr->get_item_guid(i);
                        
                        char guidBuf[64];
                        sprintf_s(guidBuf, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                            cmdGuid.Data1, cmdGuid.Data2, cmdGuid.Data3,
                            cmdGuid.Data4[0], cmdGuid.Data4[1], cmdGuid.Data4[2], cmdGuid.Data4[3],
                            cmdGuid.Data4[4], cmdGuid.Data4[5], cmdGuid.Data4[6], cmdGuid.Data4[7]);
                        
                        std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                        std::wstring wdesc = pfc::stringcvt::string_wide_from_utf8(desc.get_ptr()).get_ptr();
                        std::wstring wguid = pfc::stringcvt::string_wide_from_utf8(guidBuf).get_ptr();
                        
                        text += L"    { name: \"" + wname + L"\", desc: \"" + wdesc + L"\", guid: \"" + wguid + L"\" },\r\n";
                        cmdCount++;
                    }
                }
                text += L"];  // " + std::to_wstring(cmdCount) + L" context commands\r\n\r\n";
            }
            
            // Section 6: DSP Entries
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// SECTION 6: DSP Processors\r\n";
            text += L"// Use: discovery.getDspEntries()\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            
            {
                service_enum_t<dsp_entry> e;
                service_ptr_t<dsp_entry> ptr;
                text += L"const dspProcessors = [\r\n";
                int dspCount = 0;
                while (e.next(ptr)) {
                    pfc::string8 name;
                    ptr->get_name(name);
                    GUID guid = ptr->get_guid();
                    
                    char guidBuf[64];
                    sprintf_s(guidBuf, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                        guid.Data1, guid.Data2, guid.Data3,
                        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
                    
                    std::wstring wname = pfc::stringcvt::string_wide_from_utf8(name.get_ptr()).get_ptr();
                    std::wstring wguid = pfc::stringcvt::string_wide_from_utf8(guidBuf).get_ptr();
                    
                    text += L"    { name: \"" + wname + L"\", guid: \"" + wguid + L"\" },\r\n";
                    dspCount++;
                }
                text += L"];  // " + std::to_wstring(dspCount) + L" DSPs\r\n\r\n";
            }
            
            // Example calls
            text += L"//------------------------------------------------------------------------------\r\n";
            text += L"// EXAMPLE CALLS\r\n";
            text += L"//------------------------------------------------------------------------------\r\n\r\n";
            text += L"// Basic playback:\r\n";
            text += L"// window.fb2k.invoke('playback.getState')\r\n";
            text += L"// window.fb2k.invoke('playback.play')\r\n";
            text += L"// window.fb2k.invoke('playback.pause')\r\n\r\n";
            text += L"// Playlist operations:\r\n";
            text += L"// window.fb2k.invoke('playlist.getItems', { playlistIndex: 0 })\r\n";
            text += L"// window.fb2k.invoke('playlist.getAll')\r\n\r\n";
            text += L"// Library search:\r\n";
            text += L"// window.fb2k.invoke('library.search', { query: 'artist:Beatles' })\r\n\r\n";
            text += L"// Discovery - execute a static main menu command:\r\n";
            text += L"// window.fb2k.invoke('discovery.executeMainMenuCommand', { guid: '{...}' })\r\n\r\n";
            text += L"// Discovery - execute a dynamic child command (ESLyric etc.):\r\n";
            text += L"// Both guid and subGuid are required; the parent slot alone is not executable.\r\n";
            text += L"// window.fb2k.invoke('discovery.executeMainMenuCommand', { guid: '{...}', subGuid: '{...}' })\r\n\r\n";
            text += L"// Resolve commands at runtime instead of hardcoding GUIDs from this dump.\r\n";
            text += L"// GUIDs are locale-independent, but they change across plugin versions:\r\n";
            text += L"// const { commands } = await fb2k.invoke('discovery.getMainMenuCommands', {});\r\n";
            text += L"// const cmd = commands.find(c => c.isDynamic && /lyric/i.test(c.path));\r\n";
            text += L"// await fb2k.invoke('discovery.executeMainMenuCommand', { guid: cmd.guid, subGuid: cmd.subGuid });\r\n\r\n";
            text += L"// Discovery - execute external plugin context menu command:\r\n";
            text += L"// window.fb2k.invoke('discovery.executeContextMenuCommand', { guid: '{...}' })\r\n";
            text += L"// Note: Context menu commands operate on currently playing or selected tracks\r\n\r\n";
            text += L"// Get all available context menu commands:\r\n";
            text += L"// window.fb2k.invoke('discovery.getContextMenuCommands')\r\n";
            
            SetWindowTextW(hEdit, text.c_str());
            return 0;
        }
        
    case WM_SIZE:
        {
            HWND hEdit = GetDlgItem(hwnd, IDC_API_LIST_EDIT);
            if (hEdit) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                MoveWindow(hEdit, 0, 0, rc.right, rc.bottom, TRUE);
            }
            return 0;
        }
        
    // 系统主题切换时（浅色 <-> 深色）重新应用配色
    case WM_SETTINGCHANGE:
        if (state && lParam &&
            _wcsicmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0) {
            ApplyApiListTheme(hwnd, state);
        }
        break;
        
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        if (state && state->bkBrush) {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, state->textColor);
            SetBkColor(hdc, state->bkColor);
            return (LRESULT)state->bkBrush;
        }
        break;
        
    case WM_ERASEBKGND:
        if (state && state->bkBrush) {
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, state->bkBrush);
            return TRUE;
        }
        break;
        
    case WM_DESTROY:
        {
            if (state) {
                if (state->monoFont) DeleteObject(state->monoFont);
                if (state->bkBrush) DeleteObject(state->bkBrush);
                delete state;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            }
            return 0;
        }
        
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WebViewPreferencesInstance::OnShowApiList(HWND hwnd) {
    // 注册窗口类
    static bool classRegistered = false;
    const wchar_t* className = L"WebViewUIApiListWindow";
    
    if (!classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ApiListWndProc;
        wc.hInstance = core_api::get_my_instance();
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        // 背景由 WM_ERASEBKGND 按当前深浅色自绘，避免深色模式下闪白
        wc.hbrBackground = nullptr;
        wc.lpszClassName = className;
        wc.hIcon = nullptr;
        wc.hIconSm = nullptr;
        try { static_api_ptr_t<ui_control> fb_ui; wc.hIcon = fb_ui->get_main_icon(); wc.hIconSm = fb_ui->get_main_icon(); } catch (...) {}
        
        if (RegisterClassExW(&wc)) {
            classRegistered = true;
        } else {
            MessageBoxW(hwnd, TR("Failed to register window class.", "注册窗口类失败。"), TR("Error", "错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }
    
    // 计算窗口位置 (居中于屏幕)
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int width = 900;
    int height = 700;
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    
    // 创建窗口
    HWND hApiWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        className,
        TR("WebView2 UI - API && External Plugin Services", "WebView2 UI - API && 外部插件服务"),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, width, height,
        nullptr,  // 无父窗口，独立窗口
        nullptr,
        core_api::get_my_instance(),
        nullptr);
    
    if (!hApiWnd) {
        MessageBoxW(hwnd, TR("Failed to create API list window.", "创建 API 列表窗口失败。"), TR("Error", "错误"), MB_OK | MB_ICONERROR);
    }
}

} // namespace webview_prefs
