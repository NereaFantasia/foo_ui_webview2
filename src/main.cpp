#include "pch.h"
#include "version.h"

// ============================================
// Window Position Configuration
// ============================================

// GUIDs for window position storage
// {8F7E3A21-5B4C-4D2E-9A1F-6C8D7E9F0A1B}
static constexpr GUID guid_cfg_window_x = 
    { 0x8f7e3a21, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1b } };
// {8F7E3A22-5B4C-4D2E-9A1F-6C8D7E9F0A1C}
static constexpr GUID guid_cfg_window_y = 
    { 0x8f7e3a22, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1c } };
// {8F7E3A23-5B4C-4D2E-9A1F-6C8D7E9F0A1D}
static constexpr GUID guid_cfg_window_width = 
    { 0x8f7e3a23, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1d } };
// {8F7E3A24-5B4C-4D2E-9A1F-6C8D7E9F0A1E}
static constexpr GUID guid_cfg_window_height = 
    { 0x8f7e3a24, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1e } };
// {8F7E3A25-5B4C-4D2E-9A1F-6C8D7E9F0A1F}
static constexpr GUID guid_cfg_window_maximized = 
    { 0x8f7e3a25, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x1f } };
// {8F7E3A26-5B4C-4D2E-9A1F-6C8D7E9F0A20}
// DUI 后台窗口上次关闭时的可见性（菜单 Show/Hide Window 持久化）
static constexpr GUID guid_cfg_background_window_visible =
    { 0x8f7e3a26, 0x5b4c, 0x4d2e, { 0x9a, 0x1f, 0x6c, 0x8d, 0x7e, 0x9f, 0x0a, 0x20 } };

// Configuration variables for window position (using foobar2000 configStore)
// Default values: centered 1280x720
cfg_var_modern::cfg_int cfg_window_x(guid_cfg_window_x, INT_MIN);  // INT_MIN = not set
cfg_var_modern::cfg_int cfg_window_y(guid_cfg_window_y, INT_MIN);
cfg_var_modern::cfg_int cfg_window_width(guid_cfg_window_width, 1280);
cfg_var_modern::cfg_int cfg_window_height(guid_cfg_window_height, 720);
cfg_var_modern::cfg_bool cfg_window_maximized(guid_cfg_window_maximized, false);
// 默认 true：首次安装/未 toggle 过的用户保持旧行为（启动后显示后台窗口）
cfg_var_modern::cfg_bool cfg_background_window_visible(guid_cfg_background_window_visible, true);

// Export getters/setters for MainWindow to use
namespace window_config {
    void GetWindowPosition(int& x, int& y, int& width, int& height, bool& maximized) {
        x = static_cast<int>(cfg_window_x.get());
        y = static_cast<int>(cfg_window_y.get());
        width = static_cast<int>(cfg_window_width.get());
        height = static_cast<int>(cfg_window_height.get());
        maximized = cfg_window_maximized.get();
        
        // 调试日志
        console::printf("[WindowConfig] GetWindowPosition: x=%d, y=%d, w=%d, h=%d, max=%d",
            x, y, width, height, maximized ? 1 : 0);
    }
    
    void SetWindowPosition(int x, int y, int width, int height, bool maximized) {
        console::printf("[WindowConfig] SetWindowPosition: x=%d, y=%d, w=%d, h=%d, max=%d",
            x, y, width, height, maximized ? 1 : 0);
        
        cfg_window_x.set(x);
        cfg_window_y.set(y);
        cfg_window_width.set(width);
        cfg_window_height.set(height);
        cfg_window_maximized.set(maximized);
    }
    
    bool HasSavedPosition() {
        bool hasSaved = cfg_window_x.get() != INT_MIN;
        console::printf("[WindowConfig] HasSavedPosition: %s (x=%d)",
            hasSaved ? "true" : "false", static_cast<int>(cfg_window_x.get()));
        return hasSaved;
    }

    // 后台窗口上次关闭时的可见性，DUI 菜单 Show/Hide Window 持久化用
    bool IsBackgroundWindowVisible() {
        return cfg_background_window_visible.get();
    }

    void SetBackgroundWindowVisible(bool visible) {
        cfg_background_window_visible.set(visible);
        console::printf("[WindowConfig] SetBackgroundWindowVisible: %s",
            visible ? "true" : "false");
    }
}

// ============================================
// 安全: 高级设置 (Advanced Preferences)
// ============================================

// 高级设置分支 GUID
// {A1B2C3D4-E5F6-7890-1234-56789ABCDEF0}
static constexpr GUID guid_adv_branch_webview = 
    { 0xa1b2c3d4, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 } };

// DevTools 开关 GUID
// {A1B2C3D5-E5F6-7890-1234-56789ABCDEF1}
static constexpr GUID guid_adv_devtools = 
    { 0xa1b2c3d5, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1 } };

// 本地网络访问开关 GUID (预留)
// {A1B2C3D6-E5F6-7890-1234-56789ABCDEF2}
static constexpr GUID guid_adv_local_network = 
    { 0xa1b2c3d6, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf2 } };

// 允许 HTTP 明文连接 GUID (禁用 HSTS)
// {A1B2C3D7-E5F6-7890-1234-56789ABCDEF3}
static constexpr GUID guid_adv_allow_insecure = 
    { 0xa1b2c3d7, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf3 } };

// 允许自签 / 无效 TLS 证书 GUID (绕过证书校验,fb.http.* 用)
// {A1B2C3DC-E5F6-7890-1234-56789ABCDEF8}
static constexpr GUID guid_adv_allow_insecure_tls =
    { 0xa1b2c3dc, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf8 } };

// CDP 远程调试开关 GUID
// {A1B2C3DB-E5F6-7890-1234-56789ABCDEF7}
static constexpr GUID guid_adv_cdp_remote =
    { 0xa1b2c3db, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf7 } };

// 高级设置分支: Preferences → Advanced → Tools → WebView2 UI
advconfig_branch_factory g_adv_branch(
    "WebView2 UI",
    guid_adv_branch_webview,
    advconfig_branch::guid_branch_tools,
    0.0
);

// DevTools 开关 (默认关闭)
advconfig_checkbox_factory g_cfg_devtools(
    "Enable Developer Tools (F12) - requires restart",
    guid_adv_devtools,
    guid_adv_branch_webview,
    0.0,
    false  // 默认关闭 (安全)
);

// CDP 远程调试开关 (默认关闭) - MCP/AI Agent 用
advconfig_checkbox_factory g_cfg_cdp_remote(
    "Enable CDP remote debugging on port 9222 (for MCP/AI agents) - requires restart",
    guid_adv_cdp_remote,
    guid_adv_branch_webview,
    0.5,
    false  // 默认关闭 (安全)
);

// 本地网络访问开关 (默认关闭) - SSRF 防护用
advconfig_checkbox_factory g_cfg_local_network(
    "Allow HTTP access to local network (127.0.0.1, 192.168.x.x)",
    guid_adv_local_network,
    guid_adv_branch_webview,
    1.0,
    false  // 默认关闭 (安全)
);

// 允许 HTTP 明文连接 (默认关闭) - 本地开发用
advconfig_checkbox_factory g_cfg_allow_insecure(
    "Allow insecure HTTP connections (disable HSTS) - requires restart",
    guid_adv_allow_insecure,
    guid_adv_branch_webview,
    2.0,
    false  // 默认关闭 (安全)
);

// 允许自签 / 无效 TLS 证书 (默认关闭)
// 仅作用于 fb.http.* 请求,且每个请求还需显式 opt-in (HttpRequestOptions.insecureTls = true)。
// WebView2 自身的 fetch / 资源加载始终走严格证书校验,不受此开关影响。
advconfig_checkbox_factory g_cfg_allow_insecure_tls(
    "Allow self-signed / invalid TLS certificates for fb.http.* (per-request opt-in still required)",
    guid_adv_allow_insecure_tls,
    guid_adv_branch_webview,
    2.5,
    false  // 默认关闭 (安全)
);

// ============================================
// 后台模式配置
// ============================================

// {A1B2C3D8-E5F6-7890-1234-56789ABCDEF4}
static constexpr GUID guid_adv_background_mode =
    { 0xa1b2c3d8, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf4 } };

// {A1B2C3D9-E5F6-7890-1234-56789ABCDEF5}
static constexpr GUID guid_cfg_dev_server_url =
    { 0xa1b2c3d9, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf5 } };

// {A1B2C3DA-E5F6-7890-1234-56789ABCDEF6}
static constexpr GUID guid_cfg_use_dev_server =
    { 0xa1b2c3da, 0xe5f6, 0x7890, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf6 } };

// 后台模式开关 (默认关闭)
advconfig_checkbox_factory g_cfg_background_mode(
    "Enable background mode (run WebView when using other UIs) - requires restart",
    guid_adv_background_mode,
    guid_adv_branch_webview,
    3.0,
    false  // 默认关闭
);

// 开发服务器 URL (默认空)
advconfig_string_factory g_cfg_dev_server_url(
    "Development Server URL (e.g., http://localhost:5173)",
    guid_cfg_dev_server_url,
    guid_adv_branch_webview,
    4.0,
    ""
);

// 使用开发服务器开关 (默认关闭)
advconfig_checkbox_factory g_cfg_use_dev_server(
    "Use development server (HMR hot reload) - requires restart",
    guid_cfg_use_dev_server,
    guid_adv_branch_webview,
    5.0,
    false  // 默认关闭
);

// 全局访问函数
namespace security_config {
    bool IsDevToolsEnabled() {
        #ifndef NDEBUG
            return true;  // Debug: DevTools always available for development
        #else
            return g_cfg_devtools.get();
        #endif
    }
    
    bool IsCdpRemoteEnabled() {
        // CDP remote port must always respect user config
        // even in Debug builds, to avoid unintended network exposure
        return g_cfg_cdp_remote.get();
    }

    bool IsLocalNetworkAccessAllowed() {
        return g_cfg_local_network.get();
    }
    
    bool IsInsecureHttpAllowed() {
        return g_cfg_allow_insecure.get();
    }

    bool IsInsecureTlsAllowed() {
        return g_cfg_allow_insecure_tls.get();
    }
    
    bool IsBackgroundModeEnabled() {
        return g_cfg_background_mode.get();
    }
    
    // HMR 开发服务器配置
    bool UseDevServer() {
        return g_cfg_use_dev_server.get();
    }
    
    const char* GetDevServerUrl() {
        return g_cfg_dev_server_url.get();
    }
    
    void SetDevServerUrl(const char* url) {
        g_cfg_dev_server_url.set(url);
    }
    
    void SetUseDevServer(bool use) {
        g_cfg_use_dev_server.set(use);
    }
}

// ============================================
// Component Version Declaration
// ============================================
DECLARE_COMPONENT_VERSION(
    "WebView2 UI",
    PLUGIN_VERSION_STR,
    
    "Modern UI framework for foobar2000 based on WebView2\n"
    "\n"
    "========================================\n"
    "  OVERVIEW\n"
    "========================================\n"
    "\n"
    "WebView2 UI brings modern web technologies to foobar2000.\n"
    "Build beautiful music player interfaces using HTML/CSS/JS\n"
    "while retaining full access to foobar2000 core features.\n"
    "\n"
    "========================================\n"
    "  CORE FEATURES\n"
    "========================================\n"
    "\n"
    "[WebView2 Integration]\n"
    "  - Microsoft Edge (Chromium) engine\n"
    "  - ES2024+ JavaScript support\n"
    "  - Full CSS Grid/Flexbox/Animation\n"
    "  - Hardware-accelerated rendering\n"
    "\n"
    "[Modern Window Effects]\n"
    "  - Windows 11 Mica/Acrylic backdrop\n"
    "  - Client area extended to titlebar\n"
    "  - Native Snap Layout support\n"
    "  - Custom drag regions\n"
    "\n"
"[Bridge API System]\n"
    "  - 368 JavaScript APIs\n"
    "  - Async Promise-style calls\n"
    "  - Real-time event callbacks\n"
    "  - Type-safe bidirectional communication\n"
    "\n"
    "========================================\n"
"  API MODULES (368 APIs)\n"
    "========================================\n"
    "\n"
    "  Playback  (27)  - Play, pause, seek, volume\n"
    "  Playlist  (45)  - Playlist & track management\n"
    "  Library   (21)  - Media library queries\n"
    "  Artwork   (12)  - Cover art, batch, folder images\n"
    "  Config    (29)  - Settings, components, DSP, output\n"
    "  Window    (72)  - Window, titlebar, DWM, multi-window\n"
    "  System    (9)   - Theme, DPI, locale, API discovery\n"
    "  File      (10)  - Read, write, list, copy, move\n"
    "  Dialog    (4)   - Open/save file, folder, confirm\n"
    "  Clipboard (4)   - Read/write text, HTML, files\n"
"  Shell     (4)   - Explorer, openExternal, exec\n"
    "  HTTP      (4)   - GET, POST, HEAD, download\n"
    "  UI        (5)   - Custom menu, toast, notification\n"
    "  Keyboard  (4)   - Global hotkeys, shortcuts\n"
    "  Lyrics    (3)   - Get, save, exists\n"
    "  Metadata  (9)   - Read, write, batch, readByPath\n"
    "  Audio     (12)  - Spectrum, BPM, waveform, analyzer\n"
    "  Console   (6)   - log, warn, error, file logging\n"
    "  DnD       (4)   - Drag and drop zones\n"
    "  Queue     (8)   - Playback queue\n"
    "  JIT Queue (7)   - Streaming media queue\n"
    "  Discovery (15)  - Service & context menu discovery\n"
    "  Titlefmt  (5)   - Expression evaluation\n"
    "  DSP       (8)   - DSP presets\n"
    "  Output    (3)   - Output devices\n"
    "  ReplayGain(8)   - RG info, scan, settings\n"
    "  Selection (6)   - UI selection holder\n"
    "  Playcount (6)   - Play stats, rating\n"
    "  Menu      (5)   - Main/context menu commands\n"
    "  Misc      (9)   - Paths, preferences, restart\n"
    "  Panel     (2)   - Panel mode detection\n"
    "  Test      (2)   - Echo, version check\n"
"\n"
"  NEW IN v" PLUGIN_VERSION_STR "\n"
    "========================================\n"
    "\n"
    "  + Real library roots: library.getRoots / fb.library.getRoots\n"
    "  + Typed directory tree: library.browseTree / fb.library.browseTree\n"
    "  + Async traversal helper: fb.library.enumerateTree()\n"
    "  + New component: <fb-library-filesystem-tree>\n"
    "  + browseDirectory()/enumerateDirectories() marked legacy\n"
    "\n"
"  NEW IN v1.2.0\n"
    "========================================\n"
    "\n"
    "  + SMP (Spider Monkey Panel) Compatibility Layer\n"
    "  + sdk/smp-compat.js shim: run existing SMP scripts\n"
    "  + FbMetadbHandle, FbMetadbHandleList, FbTitleFormat\n"
    "  + FbProfiler, FbFileInfo, FbUiSelectionHolder\n"
    "  + ContextMenuManager, MainMenuManager wrappers\n"
    "  + plman.* playlist management (SMP-style)\n"
    "  + fb.onSMP() event system (SMP event names)\n"
    "  + Menu API (5): main/context menu commands\n"
    "  + Misc API (9): paths, console, preferences\n"
    "  + Config: cursor/playback follow, ReplayGain mode\n"
    "  + Playback: volumeUp/volumeDown\n"
    "  + Selection API (6): UI selection holder\n"
    "  + Multi-Window: popup, cross-window messaging\n"
    "  + API count: 305 to 368\n"
    "\n"
"  NEW IN v1.1.13\n"
    "========================================\n"
    "\n"
    "  + Titleformat API (5 APIs)\n"
    "  + Universal titleformat expression evaluation\n"
    "  + Supports foo_playcount, custom columns, etc.\n"
    "  + Renamed to WebView2 UI (distinguish from foo_webview)\n"
    "\n"
    "  NEW IN v1.1.6\n"
    "========================================\n"
    "\n"
    "  + DSP API (5 APIs)\n"
    "  + Output API (4 APIs)\n"
    "  + ReplayGain API (6 APIs)\n"
    "  + File API enhancements (copy, move, rename, getInfo)\n"
    "  + WebView2 SDK updated to 1.0.3650.58\n"
    "  + Documentation audit and expansion\n"
    "\n"
    "  NEW IN v1.1.8\n"
    "========================================\n"
    "\n"
    "  + foo_playcount rating integration\n"
    "  + discovery.executeContextMenuByPath (dynamic menus)\n"
    "  + discovery.getContextMenuTree (debug)\n"
    "  + UTF-8 encoding fix for special characters\n"
    "  + API count: 287 to 289\n"
    "\n"
    "  NEW IN v1.1.9\n"
    "========================================\n"
    "\n"
    "  + metadb:changed event (real-time metadata updates)\n"
    "  + Similar to SMP on_metadb_changed callback\n"
    "  + Rating/tag changes pushed instantly to WebView\n"
    "\n"
    "========================================\n"
    "  NEW IN v1.1.3\n"
    "========================================\n"
    "\n"
    "  + JIT Queue local file support\n"
    "  + playback.playPath CUE subsong fix\n"
    "  + Async HTTP API (async parameter)\n"
    "  + API Performance Tracker\n"
    "  + http:response event\n"
    "\n"
    "========================================\n"
    "  SECURITY\n"
    "========================================\n"
    "\n"
"  - shell.exec: Whitelist only (explorer, notepad, start)\n"
    "  - shell.openWith: Executable blacklist\n"
    "  - file.read: System path protection\n"
    "  - file.write: Config/temp only\n"
    "  - http.*: SSRF protection (optional bypass)\n"
    "  - DevTools: Disabled by default\n"
    "\n"
    "========================================\n"
    "  REQUIREMENTS\n"
    "========================================\n"
    "\n"
    "  - Windows 10 1809+ / Windows 11\n"
    "  - foobar2000 v2.0+ (32/64-bit)\n"
    "  - WebView2 Runtime\n"
    "\n"
    "========================================\n"
    "  AUTHOR\n"
    "========================================\n"
    "\n"
    "  NereaFantasia\n"
    "  QQ: 1251094727\n"
    "  License: GPL-3.0-or-later\n"
    "\n"
    "  Frontend: Vue 3 + TypeScript + Vite\n"
    "  Backend:  C++20 + Win32 + WebView2\n"
    "\n"
    "  Build: " __DATE__ " " __TIME__ "\n"
);

// 组件文件名验证 - 防止 DLL 被重命名后运行
VALIDATE_COMPONENT_FILENAME("foo_ui_webview2.dll");
