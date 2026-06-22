#include "pch.h"
#include <wtsapi32.h>            // 会话锁定/解锁通知（锁屏即释放内存）
#pragma comment(lib, "wtsapi32.lib")
#include "window/MainWindow.h"
#include "window/ChromeController.h"
#include "window/WindowChromeResolver.h"
#include "window/WindowChromeTrace.h"
#include "webview/WebViewHost.h"
#include "core/SecurityConfig.h"
#include "api/BridgeCore.h"
#include "core/WebViewContext.h"
#include "api/PlaybackApi.h"
#include "api/ConfigApi.h"
#include "api/PlaylistApi.h"
#include "api/LibraryApi.h"
#include "api/WindowApi.h"
#include "api/ArtworkApi.h"
#include "api/PluginRegistry.h"
// New universal APIs
#include "api/FileApi.h"
#include "api/DialogApi.h"
#include "api/ClipboardApi.h"
#include "api/ShellApi.h"
#include "api/HttpApi.h"
#include "api/KeyboardApi.h"
#include "api/UiApi.h"
#include "api/LyricsApi.h"
#include "api/MetadataApi.h"
#include "api/AudioApi.h"
#include "api/ConsoleApi.h"
#include "api/DndApi.h"
#include "api/QueueApi.h"
#include "api/DiscoveryApi.h"
#include "api/ReplayGainApi.h"
#include "api/PlaycountApi.h"
#include "api/TitleformatApi.h"
#include "callbacks/PlaybackCallback.h"
#include "callbacks/PlaylistCallback.h"
#include "callbacks/LibraryCallback.h"
#include "callbacks/MetadbCallback.h"
#include "window/TrayIcon.h"
#include "window/TaskbarIntegration.h"
#include <windowsx.h>  // For GET_X_LPARAM, GET_Y_LPARAM
#include <foobar2000/SDK/menu_helpers.h>  // For standard_commands
#include <uxtheme.h>  // For SetWindowTheme
#include <shellapi.h>  // For ShellExecuteW
#include <fstream>
#include <sstream>
#include <iomanip>
#include <array>
#include <algorithm>
#include <cctype>
#include "utils/WindowUtils.h"
#include "core/PreferencesPage.h"
#include "window/WindowManager.h"
#include "window/MainWindowInternal.h"

#pragma comment(lib, "uxtheme.lib")

// ============================================
// 深色模式辅助函数 (使用 Windows 未公开 API)
// ============================================
namespace mainwindow_detail {
    // DWM wrappers
    HRESULT S_DwmSetWindowAttribute(HWND h, DWORD a, LPCVOID d, DWORD s) {
        return ::DwmSetWindowAttribute(h, a, d, s);
    }
    HRESULT S_DwmGetWindowAttribute(HWND h, DWORD a, LPVOID d, DWORD s) {
        return ::DwmGetWindowAttribute(h, a, d, s);
    }
    HRESULT S_DwmExtendFrameIntoClientArea(HWND h, const MARGINS* m) {
        return ::DwmExtendFrameIntoClientArea(h, m);
    }
    HRESULT S_DwmFlush() {
        return ::DwmFlush();
    }
    
    enum class PreferredAppMode {
        Default,
        AllowDark,
        ForceDark,
        ForceLight,
        Max
    };
    
    using fnSetPreferredAppMode = PreferredAppMode(WINAPI*)(PreferredAppMode appMode);
    using fnAllowDarkModeForWindow = bool(WINAPI*)(HWND hWnd, bool allow);
    using fnFlushMenuThemes = void(WINAPI*)();
    
    fnSetPreferredAppMode _SetPreferredAppMode = nullptr;
    fnAllowDarkModeForWindow _AllowDarkModeForWindow = nullptr;
    fnFlushMenuThemes _FlushMenuThemes = nullptr;
    bool g_darkModeImportsInitialized = false;
    
    void InitDarkModeImports() {
        if (g_darkModeImportsInitialized) return;
        g_darkModeImportsInitialized = true;
        
        HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hUxtheme) {
            _SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135)));
            _AllowDarkModeForWindow = reinterpret_cast<fnAllowDarkModeForWindow>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(133)));
            _FlushMenuThemes = reinterpret_cast<fnFlushMenuThemes>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(136)));
        }
    }
    
    void SetAppDarkMode(bool bDark) {
        InitDarkModeImports();
        if (_SetPreferredAppMode) {
            static PreferredAppMode lastMode = PreferredAppMode::Default;
            PreferredAppMode wantMode = bDark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight;
            if (lastMode != wantMode) {
                _SetPreferredAppMode(wantMode);
                lastMode = wantMode;
                if (_FlushMenuThemes) _FlushMenuThemes();
            }
        }
    }
    
    void AllowDarkModeForWindow(HWND hwnd, bool bDark) {
        InitDarkModeImports();
        if (_AllowDarkModeForWindow) {
            _AllowDarkModeForWindow(hwnd, bDark);
        }
    }

    bool S_IsPluginManagedBackdropEffect(const std::string& effect) {
        return effect == "mica" || effect == "mica-alt" || effect == "acrylic";
    }
    
    // 检测 foobar2000 是否处于深色模式
    bool IsFoobar2000DarkMode() {
        auto api = ui_config_manager::tryGet();
        if (api.is_valid()) {
            return api->is_dark_mode();
        }
        return false;  // 默认浅色
    }
    // DWMWA / DWMSBT constants -> MainWindowInternal.h
    constexpr const char* S_STARTUP_PROBE_VERSION = "startup-probe-20260320b";

    static std::string S_FormatFileTimeUtc(const FILETIME& fileTime) {
        SYSTEMTIME systemTime{};
        if (!FileTimeToSystemTime(&fileTime, &systemTime)) {
            return "-";
        }

        std::ostringstream stream;
        stream << std::setfill('0')
               << std::setw(4) << systemTime.wYear << '-'
               << std::setw(2) << systemTime.wMonth << '-'
               << std::setw(2) << systemTime.wDay << 'T'
               << std::setw(2) << systemTime.wHour << ':'
               << std::setw(2) << systemTime.wMinute << ':'
               << std::setw(2) << systemTime.wSecond << '.'
               << std::setw(3) << systemTime.wMilliseconds << 'Z';
        return stream.str();
    }

    static void S_LogStartupProbeIdentity(const char* phase, HWND hwnd) {
        wchar_t modulePath[MAX_PATH] = {};
        std::string modulePathUtf8 = "-";
        std::string moduleWriteTimeUtc = "-";
        unsigned long long moduleSize = 0;

        HMODULE hModule = core_api::get_my_instance();
        if (GetModuleFileNameW(hModule, modulePath, MAX_PATH) > 0) {
            modulePathUtf8 = pfc::stringcvt::string_utf8_from_wide(modulePath).get_ptr();

            WIN32_FILE_ATTRIBUTE_DATA attributes{};
            if (GetFileAttributesExW(modulePath, GetFileExInfoStandard, &attributes)) {
                moduleWriteTimeUtc = S_FormatFileTimeUtc(attributes.ftLastWriteTime);
                moduleSize = (static_cast<unsigned long long>(attributes.nFileSizeHigh) << 32) |
                    attributes.nFileSizeLow;
            }
        }

        std::ostringstream stream;
        stream << "[StartupProbe]"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " phase=" << (phase && phase[0] ? phase : "-")
               << " probeVersion=" << S_STARTUP_PROBE_VERSION
               << " pid=" << GetCurrentProcessId()
               << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
               << " modulePath=" << modulePathUtf8.c_str()
               << " moduleSize=" << moduleSize
               << " moduleWriteTimeUtc=" << moduleWriteTimeUtc.c_str();
        S_EmitEvidenceLine(stream.str());
    }

    std::string S_GetUserBackdropEffectString() {
        switch (webview_prefs::GetBackdropEffect()) {
            case webview_prefs::BackdropEffect::None:
                return "none";
            case webview_prefs::BackdropEffect::Acrylic:
                return "acrylic";
            case webview_prefs::BackdropEffect::MicaAlt:
            case webview_prefs::BackdropEffect::Tabbed:
                return "mica-alt";
            case webview_prefs::BackdropEffect::Mica:
            default:
                return "mica";
        }
    }

    static std::string S_FormatNativeRect(const RECT& rect) {
        return std::string("(") + std::to_string(rect.left) + "," +
            std::to_string(rect.top) + "," +
            std::to_string(rect.right) + "," +
            std::to_string(rect.bottom) + ")";
    }

    const char* S_GetSizingEdgeName(WPARAM edge) {
        switch (edge) {
            case WMSZ_LEFT: return "left";
            case WMSZ_RIGHT: return "right";
            case WMSZ_TOP: return "top";
            case WMSZ_TOPLEFT: return "top-left";
            case WMSZ_TOPRIGHT: return "top-right";
            case WMSZ_BOTTOM: return "bottom";
            case WMSZ_BOTTOMLEFT: return "bottom-left";
            case WMSZ_BOTTOMRIGHT: return "bottom-right";
            default: return "unknown";
        }
    }

    std::string S_FormatWindowPosFlags(UINT flags) {
        struct FlagName {
            UINT flag;
            const char* name;
        };

        static const std::array<FlagName, 13> kFlagNames = {{
            { SWP_NOSIZE, "NOSIZE" },
            { SWP_NOMOVE, "NOMOVE" },
            { SWP_NOZORDER, "NOZORDER" },
            { SWP_NOREDRAW, "NOREDRAW" },
            { SWP_NOACTIVATE, "NOACTIVATE" },
            { SWP_FRAMECHANGED, "FRAMECHANGED" },
            { SWP_SHOWWINDOW, "SHOWWINDOW" },
            { SWP_HIDEWINDOW, "HIDEWINDOW" },
            { SWP_NOCOPYBITS, "NOCOPYBITS" },
            { SWP_NOOWNERZORDER, "NOOWNERZORDER" },
            { SWP_NOSENDCHANGING, "NOSENDCHANGING" },
            { SWP_DEFERERASE, "DEFERERASE" },
            { SWP_ASYNCWINDOWPOS, "ASYNCWINDOWPOS" },
        }};

        std::string result;
        for (const auto& flagName : kFlagNames) {
            if ((flags & flagName.flag) == 0) {
                continue;
            }
            if (!result.empty()) {
                result += '|';
            }
            result += flagName.name;
        }

        if (result.empty()) {
            result = "0";
        }
        return result;
    }

    std::string S_FormatHex32(uint32_t value) {
        std::ostringstream stream;
        stream << pfc::format_hex(value);
        return stream.str();
    }

    static std::string S_FormatDomRect(const json& rect) {
        if (!rect.is_object()) {
            return "N/A";
        }

        std::ostringstream stream;
        stream << "(" << rect.value("left", 0.0)
               << "," << rect.value("top", 0.0)
               << "," << rect.value("width", 0.0)
               << "," << rect.value("height", 0.0)
               << ")";
        return stream.str();
    }

    static std::string S_TruncateUtf8(const std::string& value, size_t maxLength) {
        if (value.size() <= maxLength) {
            return value;
        }
        return value.substr(0, maxLength) + "...";
    }

    static bool S_ParseExecuteScriptPayload(const std::wstring& resultJson, json& out) {
        try {
            std::string utf8(pfc::stringcvt::string_utf8_from_wide(resultJson.c_str()).get_ptr());
            auto parsed = json::parse(utf8);
            if (parsed.is_string()) {
                out = json::parse(parsed.get<std::string>());
            } else {
                out = std::move(parsed);
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    void S_EmitEvidenceLine(const std::string& line) {
        WindowChromeTrace::EmitAuxiliaryLine(line);
    }

// 获取组件所在目录 (保留用于兼容性回退)
std::wstring GetComponentDirectory() {
    wchar_t path[MAX_PATH];
    HMODULE hModule = core_api::get_my_instance();
    if (GetModuleFileNameW(hModule, path, MAX_PATH) > 0) {
        std::wstring fullPath(path);
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            return fullPath.substr(0, lastSlash);
        }
    }
    return L"";
}

// 获取前端资源目录路径
// 优先级:
// 1. profile/webview-ui/<active-template>/ (新位置，支持多模板)
// 2. component/foo_ui_webview2_resources/dist/ (旧位置，向后兼容)
std::wstring GetFrontendResourcesDir() {
    // 1. 尝试从新的 profile 目录加载活动模板
    std::wstring profileDir = webview_prefs::GetActiveWebResourcesDir();
    if (!profileDir.empty()) {
        // 检查是否有 index.html
        std::wstring indexPath = profileDir + L"\\index.html";
        DWORD attrs = GetFileAttributesW(indexPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            LOG("Using template from profile: ",
                pfc::stringcvt::string_utf8_from_wide(profileDir.c_str()).get_ptr());
            return profileDir;
        }
    }
    
    // 2. 回退到旧的组件目录
    std::wstring componentDir = GetComponentDirectory();
    if (!componentDir.empty()) {
        std::wstring resourcesDir = componentDir + L"\\foo_ui_webview2_resources\\dist";
        DWORD attrs = GetFileAttributesW(resourcesDir.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            LOG("Using legacy resources from component dir");
            return resourcesDir;
        }
    }
    
    // 3. 最后的兜底：直接使用 profile/webview-ui/default
    // 说明：有些情况下模板目录已创建但活动模板名/配置未生效，导致返回空
    // 此处不强依赖 TemplateList 的创建逻辑，直接按约定路径检查
    try {
        pfc::string8 profilePath = core_api::get_profile_path();
        if (profilePath.startsWith("file://")) {
            profilePath = profilePath.subString(7);
        }
        std::wstring basePath = pfc::stringcvt::string_wide_from_utf8(profilePath.c_str()).get_ptr();
        std::wstring defaultDir = basePath + L"\\webview-ui\\default";
        std::wstring indexPath = defaultDir + L"\\index.html";
        DWORD attrs = GetFileAttributesW(indexPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            LOG("Using fallback template from profile: ",
                pfc::stringcvt::string_utf8_from_wide(defaultDir.c_str()).get_ptr());
            return defaultDir;
        }
    } catch (...) {
        // ignore
    }

    return L"";
}

} // namespace mainwindow_detail

using namespace mainwindow_detail;

bool MainWindow::classRegistered_ = false;

MainWindow::MainWindow() = default;

MainWindow::~MainWindow() {
    Destroy();
}

bool MainWindow::RegisterWindowClass() {
    if (classRegistered_) return true;
    
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(MainWindow*);
    wc.hInstance = core_api::get_my_instance();
        try { static_api_ptr_t<ui_control> fb_ui; wc.hIcon = fb_ui->get_main_icon(); wc.hIconSm = fb_ui->get_main_icon(); } catch (...) {}
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        // 设置为 NULL 让 DWM 处理窗口背景（Mica/Acrylic 效果穿透）
        wc.hbrBackground = nullptr;
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = CLASS_NAME;
    
    if (!RegisterClassExW(&wc)) {
        DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            LOG("Failed to register window class, error: ", error);
            return false;
        }
    }
    
    classRegistered_ = true;
    return true;
}

HWND MainWindow::Create(HWND parent) {
    if (!RegisterWindowClass()) {
        return nullptr;
    }
    
    parentHwnd_ = parent;
    
    // 恢复保存的窗口位置，或使用默认值
    int x, y, width, height, showCmd;
    RestoreWindowPosition(x, y, width, height, showCmd);
    
    LOG("Create() - Restored position: x=", x, " y=", y, " w=", width, " h=", height, " showCmd=", showCmd);
    
    // 保存恢复的 showCmd，因为创建时需要用 SW_HIDE
    int savedShowCmd = showCmd;
    isMaximized_ = (savedShowCmd == SW_SHOWMAXIMIZED);
    isMinimized_ = (savedShowCmd == SW_SHOWMINIMIZED ||
        savedShowCmd == SW_SHOWMINNOACTIVE ||
        savedShowCmd == SW_MINIMIZE);
    savedShowCmd_ = savedShowCmd;
    startupPresentationCoordinator_.Reset();
    SyncStartupRevealProjection();
    startupSurfaceAuthorityPending_ = true;
    startupSurfaceCommitPending_ = true;
    pendingStartupSurfaceBounds_ = RECT{0, 0, 0, 0};
    hasPendingStartupSurfaceBounds_ = false;
    startupSurfaceFirstPresentCommitted_ = false;
    
    // Create top-level window (foobar2000 user_interface expects this)
    // 创建时使用 WS_OVERLAPPEDWINDOW 保持 DWM 标准窗口动画和 backdrop 合成初始化。
    // OnCreate 中移除 WS_SYSMENU 消除 DWM 三大键按钮 overlay，
    // 保留 WS_CAPTION（动画）和 WS_MINIMIZEBOX/WS_MAXIMIZEBOX（Shell 行为）。
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    // WS_EX_NOREDIRECTIONBITMAP: 关键！让窗口不渲染到重定向表面
    // 这样 DWM 效果 (Mica/Acrylic) 可以正确穿透 WebView2 显示
    // 即使在窗口 resize 时也能保持透明效果
    DWORD exStyle = WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP;
    
    startupImmediateShowMode_ = false;  // cold-start reveal：延迟显示直到 WebView 就绪或短超时

    hwnd_ = CreateWindowExW(
        exStyle,                            // Extended style
        CLASS_NAME,                         // Window class
        L"foobar2000",                      // Window title
        style,                              // Style
        x, y,                               // Position
        width, height,                      // Size
        nullptr,                            // Parent (nullptr for top-level)
        nullptr,                            // Menu
        core_api::get_my_instance(),        // Instance
        this                                // Pass this pointer
    );
    
    if (!hwnd_) {
        LOG("Failed to create window, error: ", GetLastError());
        return nullptr;
    }

    S_LogStartupProbeIdentity("MainWindow::Create.afterCreateWindowEx", hwnd_);
    
    // 强制重新计算非客户区，应用客户区扩展
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    
    // 保留 maximized placement 语义：仅存储 rcNormalPosition（用户取消最大化时的恢复位置），
    // 不显示窗口。showCmd 使用 SW_HIDE 防止 SetWindowPlacement 使窗口可见。
    // 实际最大化和显示由 TryCommitStartupReveal 的 ShowWindow(SW_SHOWMAXIMIZED) 完成。
    if (savedShowCmd == SW_SHOWMAXIMIZED) {
        WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
        GetWindowPlacement(hwnd_, &wp);
        wp.rcNormalPosition = { x, y, x + width, y + height };
        wp.showCmd = SW_HIDE;
        wp.flags = 0;
        SetWindowPlacement(hwnd_, &wp);
    }

    // Cold-start reveal：不在 Create 中 ShowWindow。
    // 窗口保持隐藏，等待 WebView 首帧就绪或短超时后由 TryCommitStartupReveal 显示。
    // foobar2000 消息泵持续驱动，user_interface::create 同步返回 HWND。
    // 启动短超时 timer：WebView 在超时内就绪则首帧即内容；否则退化为空 Mica 窗口。
    if (hwnd_ && IsWindow(hwnd_)) {
        SetTimer(hwnd_, SHORT_REVEAL_TIMER_ID, SHORT_REVEAL_TIMEOUT_MS, nullptr);
        LOG("MainWindow: Short reveal timer armed (", SHORT_REVEAL_TIMEOUT_MS, "ms)");
    }

    LOG("Window created with cold-start reveal, savedShowCmd=", savedShowCmd,
        " isMaximized=", isMaximized_);
    
    return hwnd_;
}

void MainWindow::Destroy() {
    if (webView_) {
        webView_.reset();
    }
    
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->hwnd_ = hwnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    // TaskbarButtonCreated 在 Explorer 重启或首次出现时初始化 ITaskbarList3
    {
        UINT tbcMsg = TaskbarIntegration::GetInstance().GetTaskbarCreatedMsg();
        if (tbcMsg != 0 && msg == tbcMsg) {
            TaskbarIntegration::GetInstance().Initialize(hwnd_);
            TrayIcon::GetInstance().RecreateAfterShellRestart();
            return 0;
        }
    }

    // [Chrome Reconciliation] DwmDefWindowProc 分阶段路由。
    // Bootstrap 阶段：全量调用（WM_NCHITTEST 除外），帮助 backdrop 初始化。
    // SteadyState 阶段：过滤 NC visual 消息，防止三大键复发。
    if (frameless_ && ShouldRouteToDwmDefWindowProc(msg)) {
        LRESULT dwmResult;
        if (DwmDefWindowProc(hwnd_, msg, wParam, lParam, &dwmResult)) {
            return dwmResult;
        }
    }

    switch (msg) {
        // ---- 托盘图标回调 ----
        case TrayIcon::kCallbackMessage:
            return TrayIcon::GetInstance().HandleTrayCallback(wParam, lParam);

        case WM_NCCALCSIZE:
            // 无标题栏模式：完全移除非客户区（标题栏和边框）
            if (frameless_ && wParam == TRUE) {
                // 全屏模式下不需要任何边距调整
                if (IsWindowFullscreen()) {
                    return 0;
                }
                
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                
                if (IsZoomed(hwnd_)) {
                    // [最大化裁切修复] 将客户区精确对齐到监视器工作区。
                    // WM_GETMINMAXINFO 把窗口尺寸向四周扩展了 frame 厚度，
                    // 使 WS_THICKFRAME 边框被推到屏幕外不可见。
                    // 此处把客户区收回到工作区，excess frame 成为 off-screen NC 区。
                    // 三大键消除由 WS_SYSMENU 移除 + DwmDefWindowProc 路由保证，
                    // 不依赖 NC=0。（Chromium 同款做法）
                    HMONITOR hMon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                    if (hMon) {
                        MONITORINFO mi = { sizeof(MONITORINFO) };
                        if (GetMonitorInfo(hMon, &mi)) {
                            params->rgrc[0] = mi.rcWork;

                            auto hasAutoHideBar = [&](UINT edge) -> bool {
                                APPBARDATA abd = { sizeof(APPBARDATA) };
                                abd.uEdge = edge;
                                abd.rc = mi.rcMonitor;
                                return reinterpret_cast<HWND>(
                                    SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &abd)) != nullptr;
                            };
                            constexpr int kAutoHideGap = 2;
                            if (hasAutoHideBar(ABE_TOP))    params->rgrc[0].top    += kAutoHideGap;
                            if (hasAutoHideBar(ABE_BOTTOM)) params->rgrc[0].bottom -= kAutoHideGap;
                            if (hasAutoHideBar(ABE_LEFT))   params->rgrc[0].left   += kAutoHideGap;
                            if (hasAutoHideBar(ABE_RIGHT))  params->rgrc[0].right  -= kAutoHideGap;
                        }
                    }
                }
                return 0;
            }
            // 有标题栏模式：交给 DefWindowProc 绘制系统标题栏
            if (!frameless_) break;
            
        case WM_NCHITTEST: {
            // 有标题栏模式：交给 DefWindowProc 处理系统标题栏点击测试
            if (!frameless_) break;
            
            // 使用窗口坐标（不是客户区坐标）
            // 这样即使 WebView 覆盖整个客户区，WM_NCHITTEST 仍会在消息分发前被处理
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            
            // 获取窗口矩形（屏幕坐标）
            RECT rcWindow;
            GetWindowRect(hwnd_, &rcWindow);
            
            // 边缘和角落的判定范围（视觉上仍为 1-2px，但点击判定范围更大）
            // CORNER_HIT_WIDTH: 角落判定范围，更大便于用户点击
            // EDGE_HIT_WIDTH: 边缘判定范围，与 WebView 内缩一致
            // isFullscreen_ 时禁用 resize hit-zone，否则 EnterFullscreenMode 移除
            // WS_THICKFRAME 后系统 hit-test 不再触发 SC_SIZE，但本自定义路径
            // 仍会返回 HT* 边缘代码导致 Windows 启动 SC_SIZE 模态 loop
            const bool canResize = resizable_ && !isMaximized_ && !isFullscreen_;
            const int CORNER_HIT_WIDTH = canResize ? 8 : 0;
            const int EDGE_HIT_WIDTH = canResize ? 4 : 0;
            
            // 判断鼠标位置是否在角落区域（使用更大的判定范围）
            bool cornerTop    = pt.y < rcWindow.top + CORNER_HIT_WIDTH;
            bool cornerBottom = pt.y >= rcWindow.bottom - CORNER_HIT_WIDTH;
            bool cornerLeft   = pt.x < rcWindow.left + CORNER_HIT_WIDTH;
            bool cornerRight  = pt.x >= rcWindow.right - CORNER_HIT_WIDTH;
            
            // 1. 处理四个角（优先级最高，使用更大的判定范围）
            if (cornerTop && cornerLeft)     return HTTOPLEFT;
            if (cornerTop && cornerRight)    return HTTOPRIGHT;
            if (cornerBottom && cornerLeft)  return HTBOTTOMLEFT;
            if (cornerBottom && cornerRight) return HTBOTTOMRIGHT;
            
            // 判断鼠标位置是否在边缘区域（使用较小的判定范围）
            bool edgeTop    = pt.y < rcWindow.top + EDGE_HIT_WIDTH;
            bool edgeBottom = pt.y >= rcWindow.bottom - EDGE_HIT_WIDTH;
            bool edgeLeft   = pt.x < rcWindow.left + EDGE_HIT_WIDTH;
            bool edgeRight  = pt.x >= rcWindow.right - EDGE_HIT_WIDTH;
            
            // 2. 处理四条边
            if (edgeTop)    return HTTOP;
            if (edgeBottom) return HTBOTTOM;
            if (edgeLeft)   return HTLEFT;
            if (edgeRight)  return HTRIGHT;
            
            // 3. 标题栏区域（转换为客户区坐标检测）
            POINT clientPt = pt;
            ScreenToClient(hwnd_, &clientPt);
            
            RECT clientRect;
            GetClientRect(hwnd_, &clientRect);
            int width = clientRect.right;
            
            // ============================================
            // 使用 WebView2 的 CSS app-region 查询 (v1.1.5+)
            // 这让 -webkit-app-region: drag/no-drag 真正生效
            // 特别解决 Teleport 到 body 的元素无法被父级 DOM 树检测的问题
            // ============================================
            if (webView_ && webView_->IsReady()) {
                int regionKind = webView_->GetNonClientRegionAtPoint(clientPt);
                if (regionKind == 1) {
                    // CSS -webkit-app-region: drag → 可拖拽
                    return HTCAPTION;
                } else if (regionKind == 0 && clientPt.y < titlebarHeight_) {
                    // CSS -webkit-app-region: no-drag 或无设置 → 检查是否在标题栏区域
                    // 如果在标题栏高度内但 WebView 说是客户区，则让 WebView 处理（按钮可点击）
                    return HTCLIENT;
                }
                // regionKind == 2 表示不在 WebView 内，继续默认逻辑
            }

            // 检查是否在自定义非拖拽区域（如标题栏按钮）
            if (IsPointInNoDragRegion(clientPt.x, clientPt.y)) {
                return HTCLIENT;
            }
            
            // 检查是否在自定义拖拽区域
            if (IsPointInDragRegion(clientPt.x, clientPt.y)) {
                return HTCAPTION;
            }
            
            // 检查是否有自定义拖拽区域设置
            bool hasCustomDragRegions = false;
            {
                std::lock_guard<std::mutex> lock(regionsMutex_);
                hasCustomDragRegions = !dragRegions_.empty();
            }
            
            // 标题栏区域（高度 titlebarHeight_）
            // 只有在没有自定义拖拽区域时，才使用默认标题栏拖拽逻辑
            if (!hasCustomDragRegions && clientPt.y < titlebarHeight_) {
                // 检查是否在系统按钮区域（右侧 138px = 3*46）
                int captionButtonsWidth = captionButtonWidth_ * 3;
                if (clientPt.x >= width - captionButtonsWidth) {
                    // 让 WebView 处理按钮点击
                    return HTCLIENT;
                }
                // 可拖拽区域
                return HTCAPTION;
            }
            
            // 4. 其余区域交给 WebView (HTCLIENT)
            return HTCLIENT;
        }
        
        // 非客户区按下左键：全屏时先退出全屏再继续 move/resize。
        // 覆盖路径：
        //   - HTCAPTION 拖动（系统标题栏 / -webkit-app-region: drag / Visual Hosting / window.startDrag API）
        //   - HTLEFT/HTRIGHT/HTTOP/HTBOTTOM/HT*CORNER 边缘 resize（window.startResize API、第三方 SendMessage）
        // 全屏时 ExitFullscreenMode 必须被显式调用：
        //   - 恢复 savedWindowInfo_（清 WS_EX_TOPMOST 等）
        //   - 触发 NotifyFullscreenChanged(false) 广播 window:stateChanged
        //   - 否则窗口虽然视觉上 resize 走出全屏尺寸，但 isFullscreen_ / TOPMOST 残留。
        // 参考 Chrome / Edge / Electron / Tauri 行为。
        case WM_NCLBUTTONDOWN:
            if (IsWindowFullscreen()) {
                switch (wParam) {
                    case HTCAPTION:
                    case HTLEFT: case HTRIGHT: case HTTOP: case HTBOTTOM:
                    case HTTOPLEFT: case HTTOPRIGHT:
                    case HTBOTTOMLEFT: case HTBOTTOMRIGHT:
                        ExitFullscreenIfActive(hwnd_);
                        // 不 return 0：让 DefWindowProc 继续处理对应 modal loop
                        // (HTCAPTION → SC_MOVE, HT*Edge → SC_SIZE)。
                        // ExitFullscreen 已恢复 WS_THICKFRAME，size loop 可正常接管。
                        break;
                    default:
                        break;
                }
            }
            break;
        
        // 双击标题栏最大化/还原（仅无标题栏模式）
        case WM_NCLBUTTONDBLCLK:
            if (frameless_ && wParam == HTCAPTION) {
                // 全屏模式下双击标题栏退出全屏（参考 Electron / Tauri 行为）
                if (IsWindowFullscreen()) {
                    ExitFullscreenIfActive(hwnd_);
                    return 0;
                }
                
                if (isMaximized_) {
                    ShowWindow(hwnd_, SW_RESTORE);
                } else {
                    ShowWindow(hwnd_, SW_MAXIMIZE);
                }
                return 0;
            }
            break;
        
        // 系统菜单 / 键盘快捷键路径的 maximize/restore：全屏时先退出全屏
        // 覆盖 Alt+Space 系统菜单、frame=true 模式 DefWindowProc 双击标题栏、
        // 任何 PostMessage(WM_SYSCOMMAND, SC_MAXIMIZE/SC_RESTORE) 等路径。
        // API handler 层（WindowMaximize/Restore/ToggleMaximize）已在 PostMessage 之前
        // 调用 ExitFullscreenIfActive，到达此处时已不在全屏，本拦截器不会重复触发。
        case WM_SYSCOMMAND: {
            const UINT cmd = static_cast<UINT>(wParam) & 0xFFF0;
            // 最小化/关闭到托盘
            if (cmd == SC_MINIMIZE && TrayIcon::GetInstance().GetMinimizeToTray()) {
                // 隐藏到托盘前让 WebView 进入 hidden/暂停态以回收内存（与最小化同理：仅 SW_HIDE
                // 不会让页面 visibilityState=hidden，因 IsVisible 仍 TRUE + occlusion 已禁用）。
                // 恢复端由 RestoreSurfaceAfterHidden 兜 SetVisible(true)+nudge + 复原 Normal。
                if (webView_) {
                    webView_->SetVisible(false);
                    webView_->SetMemoryUsageLow(true);
                }
                ClearCoverSuspend();
                ShowWindow(hwnd_, SW_HIDE);
                return 0;
            }
            if (cmd == SC_CLOSE && TrayIcon::GetInstance().GetCloseToTray()) {
                // 关闭到托盘：同上，先让页面 hidden/暂停回收内存，恢复端已兜住。
                if (webView_) {
                    webView_->SetVisible(false);
                    webView_->SetMemoryUsageLow(true);
                }
                ClearCoverSuspend();
                ShowWindow(hwnd_, SW_HIDE);
                return 0;
            }
            if ((cmd == SC_MAXIMIZE || cmd == SC_RESTORE) && IsWindowFullscreen()) {
                ExitFullscreenIfActive(hwnd_);
                if (cmd == SC_RESTORE) {
                    // restore 语义已由 ExitFullscreen 满足，阻断后续 SC_RESTORE
                    return 0;
                }
                // SC_MAXIMIZE 让 DefWindowProc 继续处理：若 ExitFullscreen 已恢复 maximize 则 no-op
                break;
            }
            break;
        }
        
        // 右键标题栏显示系统菜单
        case WM_NCRBUTTONUP:
            if (wParam == HTCAPTION) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ShowSystemMenu(pt.x, pt.y);
                return 0;
            }
            break;
            
        case WM_CREATE:
            OnCreate();
            // 注册会话锁定/解锁通知：锁屏=必然不可见，借此 SetVisible(false)+Low 省内存，
            // 解锁复原。零误判风险（锁屏期间窗口绝不可见）。
            WTSRegisterSessionNotification(hwnd_, NOTIFY_FOR_THIS_SESSION);
            return 0;
            
        case WM_CLOSE:
            // When window is closed, exit foobar2000 properly
            OnClose();
            return 0;
            
        case WM_DESTROY:
            WTSUnRegisterSessionNotification(hwnd_);
            OnDestroy();
            return 0;

        case WM_WTSSESSION_CHANGE:
            // 锁屏 → 隐藏 WebView 释放内存；解锁 → 复原。
            if (wParam == WTS_SESSION_LOCK) {
                SetBgSuspend(kBgSuspendLocked, true, "session-lock");
            } else if (wParam == WTS_SESSION_UNLOCK) {
                SetBgSuspend(kBgSuspendLocked, false, "session-unlock");
            }
            return 0;

        case WM_SHOWWINDOW: {
            std::string extra = std::string("show=") + WindowChromeTrace::BoolText(wParam != FALSE) +
                " status=" + std::to_string((long long)lParam);
            TraceWindowPhase("WM_SHOWWINDOW", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            if (wParam != FALSE && !isMinimized_ &&
                !startupRevealPending_ && startupRevealCommitted_ && !startupRevealSettling_) {
                ApplyResolvedChrome(true);
            }
            break;
        }

        case WM_ENTERSIZEMOVE: {
            interactiveSizingActive_ = true;
            ++interactiveSizingSessionId_;
            const std::string extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_);
            TraceWindowPhase("WM_ENTERSIZEMOVE", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            LogSurfaceDiagnostics("WM_ENTERSIZEMOVE");
            EmitInteractiveResizeEvidence("WM_ENTERSIZEMOVE", "enter", nullptr, nullptr);
            CaptureRuntimeDomProbe("WM_ENTERSIZEMOVE/+0ms", 0);
            break;
        }

        case WM_SIZING: {
            const RECT* sizingRect = reinterpret_cast<const RECT*>(lParam);
            const char* edgeName = S_GetSizingEdgeName(wParam);
            const std::string extra = std::string("edge=") + edgeName +
                " session=" + std::to_string(interactiveSizingSessionId_) +
                " sizingRect=" + (sizingRect ? S_FormatNativeRect(*sizingRect) : std::string("N/A"));
            TraceWindowPhase("WM_SIZING", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            EmitInteractiveResizeEvidence("WM_SIZING", edgeName, sizingRect, nullptr);
            break;
        }

        case WM_WINDOWPOSCHANGING: {
            const WINDOWPOS* windowPos = reinterpret_cast<const WINDOWPOS*>(lParam);
            std::string extra;
            if (windowPos) {
                const std::string flagsHex = S_FormatHex32((uint32_t)windowPos->flags);
                extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                    " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_) +
                    " flags=0x" + flagsHex +
                    " flagsText=" + S_FormatWindowPosFlags(windowPos->flags) +
                    " x=" + std::to_string(windowPos->x) +
                    " y=" + std::to_string(windowPos->y) +
                    " cx=" + std::to_string(windowPos->cx) +
                    " cy=" + std::to_string(windowPos->cy);
            } else {
                extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                    " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_) +
                    " windowPos=null";
            }
            TraceWindowPhase("WM_WINDOWPOSCHANGING", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            EmitInteractiveResizeEvidence("WM_WINDOWPOSCHANGING", "window-pos", nullptr, windowPos);
            break;
        }

        case WM_WINDOWPOSCHANGED: {
            const WINDOWPOS* windowPos = reinterpret_cast<const WINDOWPOS*>(lParam);
            std::string extra;
            if (windowPos) {
                const std::string flagsHex = S_FormatHex32((uint32_t)windowPos->flags);
                extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                    " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_) +
                    " flags=0x" + flagsHex +
                    " flagsText=" + S_FormatWindowPosFlags(windowPos->flags) +
                    " x=" + std::to_string(windowPos->x) +
                    " y=" + std::to_string(windowPos->y) +
                    " cx=" + std::to_string(windowPos->cx) +
                    " cy=" + std::to_string(windowPos->cy);
            } else {
                extra = std::string("session=") + std::to_string(interactiveSizingSessionId_) +
                    " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_) +
                    " windowPos=null";
            }
            TraceWindowPhase("WM_WINDOWPOSCHANGED", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            EmitInteractiveResizeEvidence("WM_WINDOWPOSCHANGED", "window-pos", nullptr, windowPos);
            
            // WebView2 Composition 模式要求: 父窗口位置变化时通知控制器，
            // 否则 WebView2 内部输入窗口不跟随移动，产生「幽灵窗口」阻断桌面交互。
            if (webView_ && webView_->GetController()) {
                webView_->GetController()->NotifyParentWindowPositionChanged();
            }

            // -- 期望置顶状态守护（capability 级收口）--
            // 任何 z-order 路径（现有或未来新增）若让主窗口的 WS_EX_TOPMOST 偏离期望
            // ——升段 = 被相对插入隐式授予置顶（意外覆盖全桌面），
            //   降段 = 用户显式置顶被撤销路径悄悄剥除——在此统一矫正。
            // 期望值唯一由 SetExpectedTopmost 改写（用户显式 alwaysOnTop 意图）；
            // 全屏期间合法持有 HWND_TOPMOST，暂停矫正（进入路径先置 isFullscreen_，
            // 退出路径在恢复 z-order 之后才清标志，两侧均被正确豁免）。
            // 矫正触发的下一次 WM_WINDOWPOSCHANGED 中实际位已与期望一致，自然收敛。
            if (!IsFullscreen() && windowPos && !(windowPos->flags & SWP_NOZORDER)) {
                const bool actualTopmost =
                    (GetWindowLongW(hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
                if (actualTopmost != expectedTopmost_) {
                    {
                        std::ostringstream stream;
                        stream << "[ActivationEvidence] source=MainWindow::TopmostGuard"
                               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                               << " actualTopmost=" << WindowChromeTrace::BoolText(actualTopmost)
                               << " expectedTopmost=" << WindowChromeTrace::BoolText(expectedTopmost_)
                               << " correction=" << (expectedTopmost_ ? "HWND_TOPMOST" : "HWND_NOTOPMOST");
                        S_EmitEvidenceLine(stream.str());
                    }
                    SetWindowPos(hwnd_, expectedTopmost_ ? HWND_TOPMOST : HWND_NOTOPMOST,
                        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }
            // z-order/位置变化可能改变“是否被完全覆盖”，防抖后重评估
            ScheduleCoverReevaluation();
            break;
        }
        
        case WM_TIMER:
            if (wParam == STARTUP_REVEAL_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
                LOG("MainWindow: Startup reveal fallback timer fired");
                if (!startupRevealCommitted_) {
                    ApplyStartupPresentationDecision(
                        startupPresentationCoordinator_.OnFallbackTimerFired(),
                        "StartupFallbackTimer");
                }
                return 0;
            }
            if (wParam == SHORT_REVEAL_TIMER_ID) {
                KillTimer(hwnd_, SHORT_REVEAL_TIMER_ID);
                LOG("MainWindow: Short reveal timer fired (", SHORT_REVEAL_TIMEOUT_MS, "ms)");
                if (!startupRevealCommitted_) {
                    // 短超时触发：WebView 首帧 authority 未在 250ms 内齐备，
                    // 退化为空 Mica 窗口，行为与之前 immediate-show 一致。
                    // 使用 ForceFallbackReveal 而非 OnFallbackTimerFired，
                    // 因为短超时时导航可能尚未完成。
                    ApplyStartupPresentationDecision(
                        startupPresentationCoordinator_.ForceFallbackReveal(),
                        "ShortRevealTimer");
                }
                return 0;
            }
            if (wParam == STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID);
                CaptureRuntimeDomProbe("FinalizeStartupRevealSettlement/+100ms", 100);
                return 0;
            }
            if (wParam == STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID);
                CaptureRuntimeDomProbe("FinalizeStartupRevealSettlement/+250ms", 250);
                return 0;
            }
            if (wParam == STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID) {
                KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID);
                CaptureRuntimeDomProbe("FinalizeStartupRevealSettlement/+500ms", 500);
                return 0;
            }
            if (wParam == SIZE_MAXIMIZED_PROBE_T100_TIMER_ID) {
                KillTimer(hwnd_, SIZE_MAXIMIZED_PROBE_T100_TIMER_ID);
                CaptureRuntimeDomProbe("WM_SIZE.sizeType=maximized/+100ms", 100);
                return 0;
            }
            if (wParam == SIZE_RESTORED_PROBE_T100_TIMER_ID) {
                KillTimer(hwnd_, SIZE_RESTORED_PROBE_T100_TIMER_ID);
                CaptureRuntimeDomProbe("WM_SIZE.sizeType=restored/+100ms", 100);
                return 0;
            }
            if (wParam == RESTORE_SURFACE_CONVERGE_TIMER_ID) {
                KillTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID);
                // 最小化恢复后强制一次 DComp surface 重呈现，兜底 occlusion 恢复空白
                if (!isMinimized_) {
                    EnsureSurfaceConvergedAfterNativeFrame("restore-from-minimize");
                }
                return 0;
            }
            if (wParam == COVER_REEVAL_TIMER_ID) {
                KillTimer(hwnd_, COVER_REEVAL_TIMER_ID);
                EvaluateCoverState();
                return 0;
            }
            if (wParam == COVER_POLL_TIMER_ID) {
                // 覆盖态周期重检：不再被完全覆盖即恢复并停轮询（在 EvaluateCoverState 内处理）
                EvaluateCoverState();
                return 0;
            }
            if (wParam == DEFERRED_BACKDROP_TIMER_ID) {
                KillTimer(hwnd_, DEFERRED_BACKDROP_TIMER_ID);
                deferredBackdropPending_ = false;
                if (needsAuthoritativeChromeReapply_ && !isMinimized_ && IsWindowVisible(hwnd_)) {
                    needsAuthoritativeChromeReapply_ = false;
                    TraceWindowPhase("DeferredBackdropTimer.enter");
                    // [Experiment I] 简化为 Chrome 层 reapply（仅在 WebView 就绪后触发）
                    ApplyResolvedChrome(true);
                    TraceWindowPhase("DeferredBackdropTimer.done");
                } else {
                    needsAuthoritativeChromeReapply_ = false;
                }
                return 0;
            }
            if (wParam == BACKDROP_KICK_EXIT_TIMER_ID) {
                // 保留以防后续需要，当前不使用
                KillTimer(hwnd_, BACKDROP_KICK_EXIT_TIMER_ID);
                return 0;
            }
            return 0;

        // WM_DEFERRED_CHROME_REAPPLY: 保留为 legacy fallback，
        // 当前主路径使用 DEFERRED_BACKDROP_TIMER_ID。
        case WM_DEFERRED_CHROME_REAPPLY:
            needsAuthoritativeChromeReapply_ = false;
            return 0;

        // WM_DEFERRED_SURFACE_CONVERGE: startup-only one-shot ? legacy fallback
        case WM_DEFERRED_SURFACE_CONVERGE: {
            const bool isStartupOneShot = startupDeferredSurfaceConvergePending_;
            startupDeferredSurfaceConvergePending_ = false;
            const char* reason = isStartupOneShot
                ? "startup-post-first-present" : "deferred-surface-converge";
            LogSurfaceDiagnostics("WM_DEFERRED_SURFACE_CONVERGE.enter");
            {
                std::ostringstream stream;
                stream << "[DeferredSurfaceConverge]"
                       << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " reason=" << reason
                       << " isStartupOneShot=" << WindowChromeTrace::BoolText(isStartupOneShot)
                       << " firstPresentCommitted=" << WindowChromeTrace::BoolText(startupSurfaceFirstPresentCommitted_)
                       << " isMinimized=" << WindowChromeTrace::BoolText(isMinimized_);
                S_EmitEvidenceLine(stream.str());
            }
            EnsureSurfaceConvergedAfterNativeFrame(reason);
            CaptureRuntimeDomProbe("WM_DEFERRED_SURFACE_CONVERGE.done");
            FinalizeStartupRevealSettlement();
            return 0;
        }

        case WM_EXITSIZEMOVE: {
            const unsigned int sessionId = interactiveSizingSessionId_;
            interactiveSizingActive_ = false;
            const std::string extra = std::string("session=") + std::to_string(sessionId) +
                " interactiveSizingActive=" + WindowChromeTrace::BoolText(interactiveSizingActive_);
            TraceWindowPhase("WM_EXITSIZEMOVE", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            LogSurfaceDiagnostics("WM_EXITSIZEMOVE");
            EmitInteractiveResizeEvidence("WM_EXITSIZEMOVE", "exit", nullptr, nullptr);
            CaptureRuntimeDomProbe("WM_EXITSIZEMOVE/+0ms", 0);
            // 窗口移动或调整大小结束后保存位置
            SaveWindowPosition();
            return 0;
        }
            
        case WM_SIZE: {
            const bool wasMaximized = isMaximized_;
            const bool wasMinimized = isMinimized_;
            std::string sizeType = "restored";

            switch (wParam) {
                case SIZE_MINIMIZED:
                    isMaximized_ = false;
                    isMinimized_ = true;
                    sizeType = "minimized";
                    break;
                case SIZE_MAXIMIZED:
                    isMaximized_ = true;
                    isMinimized_ = false;
                    sizeType = "maximized";
                    break;
                default:
                    isMaximized_ = false;
                    isMinimized_ = false;
                    sizeType = "restored";
                    break;
            }

            std::string extra = std::string("sizeType=") + sizeType +
                " width=" + std::to_string(LOWORD(lParam)) +
                " height=" + std::to_string(HIWORD(lParam));
            TraceWindowPhase("WM_SIZE", nullptr, nullptr, nullptr, nullptr, extra.c_str());
            
            // 无标题栏模式下，最大化状态改变时重新计算非客户区
            if (frameless_ && wasMaximized != isMaximized_) {
                SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                    SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }

            if (wParam == SIZE_MINIMIZED) {
                BroadcastWindowStateChangedIfNeeded();
                // 最小化时让 WebView 进入 hidden/暂停态，才能真正回收内存。
                // 关键经验（CDP 实测）：仅 MemoryUsageTargetLevel=Low 是净零——本项目最小化时 controller
                // 仍 IsVisible=TRUE 且 occlusion 已禁用，页面 visibilityState 始终 visible、rAF/定时器不停，
                // 释放的内存立刻被回占。put_IsVisible(FALSE) 使页面 visibilityState=hidden 并暂停，此时
                // Low 才真正生效并保持；恢复见下方 SetVisible(true) + RESTORE_SURFACE_CONVERGE nudge。
                // 仍不使用 SetBoundsVisible(false)（零视口会导致 HMR 导航失败）。
                if (webView_) {
                    webView_->SetVisible(false);
                    webView_->SetMemoryUsageLow(true);
                }
                ClearCoverSuspend();  // 进入最小化：清覆盖记账+停轮询，恢复后会重新评估
                return 0;
            }

            // 从最小化恢复：先恢复可见性（put_IsVisible(TRUE) → visibilityState=visible、页面恢复运行）
            // 并复原内存目标等级；DComp 表面随后由 RESTORE_SURFACE_CONVERGE nudge 强制重呈现，防空窗。
            // 顺序与 RefreshStartupRevealSurface 一致：先 SetVisible(true) 再 Resize（OnSize）。
            if (wasMinimized && webView_) {
                webView_->SetVisible(true);
                webView_->SetMemoryUsageLow(false);
                ScheduleCoverReevaluation();  // 恢复后若仍处于被覆盖位置则重新隐藏
            }

            OnSize(LOWORD(lParam), HIWORD(lParam));
            if (wasMinimized && !startupRevealPending_ && startupRevealCommitted_ && !startupRevealSettling_) {
                ApplyResolvedChrome(true);
                // 兜底：恢复后延迟一拍强制一次真实 surface transition（nudge），防 DComp 表面留空。
                // 最小化时已显式 SetVisible(false) 隐藏 WebView，故此处 SetVisible(true)+nudge
                // 等效既有 RestoreSurfaceAfterHidden 的恢复序列（已验证可靠，非历史“无法恢复”场景）。
                if (hwnd_ && IsWindow(hwnd_)) {
                    KillTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID);
                    SetTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID,
                             RESTORE_SURFACE_CONVERGE_DELAY_MS, nullptr);
                }
            }
            BroadcastWindowStateChangedIfNeeded();
            if (!startupRevealPending_ && startupRevealCommitted_ &&
                (sizeType == "maximized" || sizeType == "restored")) {
                ScheduleManualResizeRuntimeProbes(sizeType.c_str());
            }
            return 0;
        }
            
        case WM_PAINT:
            OnPaint();
            return 0;
            
        case WM_ERASEBKGND: {
            // 始终返回 FALSE，让 DWM 处理背景绘制
            // 这样 Mica/Acrylic 效果才能正确显示
            return FALSE;
        }
            
        case WM_ACTIVATE:
            OnActivate(wParam);
            // 覆盖检测：失活 → 防抖后评估是否被完全覆盖；激活 → 立即解除覆盖挂起（获焦必然未被覆盖）
            if (LOWORD(wParam) == WA_INACTIVE) {
                ScheduleCoverReevaluation();
            } else {
                SetBgSuspend(kBgSuspendCovered, false, "activated");
                KillTimer(hwnd_, COVER_POLL_TIMER_ID);
                KillTimer(hwnd_, COVER_REEVAL_TIMER_ID);
            }
            break;

        case WM_ACTIVATEAPP:
            OnActivateApp(wParam != FALSE);
            if (wParam != FALSE) {
                SetBgSuspend(kBgSuspendCovered, false, "app-activated");
                KillTimer(hwnd_, COVER_POLL_TIMER_ID);
                KillTimer(hwnd_, COVER_REEVAL_TIMER_ID);
            } else {
                ScheduleCoverReevaluation();
            }
            return 0;
            
        case WM_SETFOCUS: {
            // Visual Hosting (CompositionController) 模式下 WebView2 没有独立子 HWND，
            // Alt+Tab / 任务栏切回窗口时系统把键盘焦点交给顶层 HWND，
            // 若不主动把焦点转交给 Chromium，页面收不到键盘输入（必须点击一次才恢复）。
            // 这里在宿主窗口拿到焦点时调用 MoveFocus 把焦点路由进 WebView 内容。
            {
                std::ostringstream stream;
                stream << "[ActivationEvidence] source=WM_SETFOCUS"
                       << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " prevFocusHwnd=0x" << pfc::format_hex((size_t)(HWND)wParam)
                       << " webViewReady=" << WindowChromeTrace::BoolText(webView_ && webView_->IsReady());
                S_EmitEvidenceLine(stream.str());
            }
            if (webView_ && webView_->IsReady() && !isMinimized_) {
                webView_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
            break;  // 交给 DefWindowProc 维持默认焦点语义
        }
            
        case WM_DPICHANGED:
            OnDpiChanged(wParam, lParam);
            return 0;
            
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            
            // 最小尺寸
            mmi->ptMinTrackSize.x = minWidth_;
            mmi->ptMinTrackSize.y = minHeight_;
            
            // 最大尺寸 (0 表示无限制)
            if (maxWidth_ > 0) {
                mmi->ptMaxTrackSize.x = maxWidth_;
            }
            if (maxHeight_ > 0) {
                mmi->ptMaxTrackSize.y = maxHeight_;
            }

            // frameless 最大化时，通过 ptMaxPosition/ptMaxSize 将窗口向四周扩展
            // frame 厚度，使 WS_THICKFRAME 边框被推到屏幕外（不可见）。
            // WM_NCCALCSIZE 再将客户区收回到工作区，excess frame 成为 off-screen NC 区。
            // 这是 Chromium 风格的无边框最大化做法。
            if (frameless_) {
                HMONITOR hMon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                if (hMon) {
                    MONITORINFO mi = { sizeof(MONITORINFO) };
                    if (GetMonitorInfo(hMon, &mi)) {
                        int frameX = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                        int frameY = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                        mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left - frameX;
                        mmi->ptMaxPosition.y = mi.rcWork.top  - mi.rcMonitor.top  - frameY;
                        mmi->ptMaxSize.x = (mi.rcWork.right  - mi.rcWork.left) + 2 * frameX;
                        mmi->ptMaxSize.y = (mi.rcWork.bottom - mi.rcWork.top)  + 2 * frameY;
                    }
                }
            }
            
            break;  // 让 DefWindowProc 也处理
        }
            
        case WM_CONTEXTMENU:
            // 不自动显示右键菜单，让前端通过 ui.showContextMenu API 控制
            // 前端可以使用 e.preventDefault() 阻止默认菜单
            // 需要菜单时调用 fb2k.invoke('ui.showContextMenu', {x, y})
            return 0;
            
        case WM_COMMAND:
            // 缩略图按钮点击
            if (HIWORD(wParam) == THBN_CLICKED) {
                TaskbarIntegration::GetInstance().HandleButtonClicked(LOWORD(wParam));
                return 0;
            }
            OnCommand(LOWORD(wParam));
            return 0;
            
        case WM_KEYDOWN:
            // F12 to open DevTools (仅开发服务器模式允许)
            if (wParam == VK_F12 && webView_ && webView_->IsReady()) {
                if (security_config::UseDevServer()) {
                    webView_->OpenDevTools();
                }
            }
            // F5 to reload
            else if (wParam == VK_F5 && webView_ && webView_->IsReady()) {
                webView_->Reload();
            }
            // Check if this key is bound to Preferences command
            else {
                // Get the GUID for Preferences command
                GUID prefsGuid = standard_commands::guid_main_preferences;
                
                // Check common keyboard shortcuts for Preferences
                bool isCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isShiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                bool isAltDown = (GetKeyState(VK_MENU) & 0x8000) != 0;
                
                // Default shortcut for Preferences is Ctrl+P
                if (wParam == 'P' && isCtrlDown && !isShiftDown && !isAltDown) {
                    LOG("Preferences shortcut detected (Ctrl+P), opening preferences");
                    standard_commands::run_main(prefsGuid);
                    return 0;  // Prevent WebView from handling this key
                }
            }
            break;
            
        // ============================================
        // Visual Hosting 模式：客户区光标同步
        // CompositionController 模式下 WebView2 不拥有任何 HWND, 所以所有
        // WM_SETCURSOR 都到父窗口。我们必须在 HTCLIENT 时主动 SetCursor
        // 为 Chromium 当前希望显示的光标 (或 cursor.setHidden(true) 时 nullptr),
        // 否则 DefWindowProc 会用 GCLP_HCURSOR (类光标) 兜底, 在 idle 隐藏
        // 期间会错误地显示箭头光标 (问题 B), 在 hover 切换时又会落后于
        // Chromium 的 CursorChanged (问题 C)。
        //
        // 边缘 / 标题栏 / 系统按钮 (HTCAPTION / HTLEFT / HTBOTTOMRIGHT 等)
        // 仍由 DefWindowProc 处理, 以便系统正确显示 resize / move 光标。
        // ============================================
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && webView_ &&
                webView_->ApplyCurrentCursor()) {
                return TRUE;
            }
            break;

        // ============================================
        // Visual Hosting 模式：鼠标消息转发
        // ============================================
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSELEAVE:
            if (webView_ && webView_->GetCompositionController()) {
                POINT pt;
                POINTSTOPOINT(pt, lParam);
                
                // ============================================
                // 拖拽区域检测：不转发拖拽区域的鼠标事件
                // 让系统处理窗口拖拽
                // ============================================
                bool inDragRegion = IsPointInDragRegion(pt.x, pt.y) && !IsPointInNoDragRegion(pt.x, pt.y);
                
                if (inDragRegion) {
                    if (msg == WM_LBUTTONDOWN) {
                        // 在拖拽区域内单击，不转发给 WebView
                        // 通过模拟非客户区消息来实现拖拽
                        ReleaseCapture();
                        SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, lParam);
                        return 0;
                    }
                    else if (msg == WM_LBUTTONDBLCLK) {
                        // 在拖拽区域内双击，切换最大化状态（全屏时禁用）
                        if (IsWindowFullscreen()) {
                            return 0;
                        }
                        if (isMaximized_) {
                            ShowWindow(hwnd_, SW_RESTORE);
                        } else {
                            ShowWindow(hwnd_, SW_MAXIMIZE);
                        }
                        return 0;
                    }
                    else if (msg == WM_RBUTTONUP) {
                        // 在拖拽区域内右键，显示系统菜单
                        POINT screenPt = pt;
                        ClientToScreen(hwnd_, &screenPt);
                        ShowSystemMenu(screenPt.x, screenPt.y);
                        return 0;
                    }
                }
                
                if (webView_->HandleMouseMessage(msg, wParam, lParam)) {
                    return 0;  // 已处理
                }
            }
            break;
    }
    
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

void MainWindow::OnCreate() {
    LOG("MainWindow::OnCreate");
    
    // [Experiment I] 完全复刻 v1.1.19 OnCreate 的 DWM 调用序列。
    // 不调用 Chrome 统一层（ApplyResolvedChrome / WindowChromeApplier），
    // 不写 corner preference、BlurBehind、MICA_EFFECT=FALSE 等额外属性，
    // 不触发额外的 SWP_FRAMECHANGED / RedrawWindow。
    // Chrome 层延迟到 OnWebViewReady 后首次激活。

    // 1. 设置应用级深色模式 (影响右键菜单等)
    bool isDark = IsFoobar2000DarkMode();
    SetAppDarkMode(isDark);
    AllowDarkModeForWindow(hwnd_, isDark);
    
    // 2. 扩展客户区到整个窗口（包括边框）
    MARGINS margins = { -1, -1, -1, -1 };
    S_DwmExtendFrameIntoClientArea(hwnd_, &margins);

    // 2.1 [frameless 模式] 移除按钮使能位防止 DWM 渲染三大键。
    // 根因：DwmExtendFrameIntoClientArea({-1,-1,-1,-1}) + transparent WebView +
    // WS_SYSMENU = DWM 三大键按钮作为玻璃框架 sub-layer 透过透明内容可见。
    // NC面积=0 无法消除此行为——按钮不在 NC 区域，而是 DWM 合成层装饰。
    // 关键：保留 WS_CAPTION 使 DWM 标准动画（最小化/恢复/最大化）正常工作，
    // 仅移除 WS_SYSMENU 以消除全部三大键按钮渲染。
    // DWM 按钮 overlay 渲染完全依赖 WS_SYSMENU；WS_MINIMIZEBOX/WS_MAXIMIZEBOX
    // 只在 WS_SYSMENU 存在时控制哪些按钮 enabled/disabled，不单独触发渲染。
    // 保留 WS_MINIMIZEBOX/WS_MAXIMIZEBOX 确保 Shell 任务栏点击最小化/恢复切换
    // 以及任务栏右键菜单中的最小化/最大化功能正常工作。
    // 恢复 frameless→non-frameless 时由 ApplyNativeFrameStrategy::Standard 重新添加。
    if (frameless_) {
        LONG_PTR curStyle = GetWindowLongPtrW(hwnd_, GWL_STYLE);
        SetWindowLongPtrW(hwnd_, GWL_STYLE,
            curStyle & ~WS_SYSMENU);
    }

    // 2.5 确保 Windows 11 圆角矩形：显式写 DWMWCP_ROUND，
    // 避免 CreateWindowEx → ShowWindow 之间出现直角首帧
    {
        constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE_V = 33;
        constexpr DWORD DWMWCP_ROUND = 2;
        DWORD cornerPref = DWMWCP_ROUND;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE_V,
            &cornerPref, sizeof(cornerPref));
    }

    // [Experiment K] 显式启用 DWM 窗口过渡动画。
    // CaptionlessOverlapped 窗口依赖此属性保持标准 DWM 动画。
    {
        constexpr DWORD DWMWA_TRANSITIONS_FORCEDISABLED = 3;
        BOOL transDisabled = FALSE;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_TRANSITIONS_FORCEDISABLED,
            &transDisabled, sizeof(transDisabled));
    }

    isActive_ = (::GetForegroundWindow() == hwnd_);
    chromeBackdropBroadcastReady_ = false;

    // 3. 直接写 DWM 属性（精确复刻 v1.1.19 EnableMicaEffect）
    //    顺序: dark mode → backdrop type → (fallback mica)
    //    不写 corner pref、不写 BlurBehind、不写 MICA_EFFECT=FALSE
    EnableMicaEffect();
    
    // 4. 设置主窗口 ID
    SetWindowId("main");
    
    // 5. 初始化 WebView2（DComp target 在此创建）
    const bool webViewInitStarted = InitializeWebView(hwnd_, WebViewPanelMode::Standalone);

    if (!webViewInitStarted) {
        LOG("ERROR: Failed to start WebView2 initialization");
        ApplyStartupPresentationDecision(
            startupPresentationCoordinator_.ForceFallbackReveal(),
            "InitializeWebViewFailed");
        MessageBoxW(hwnd_, 
            L"Failed to initialize WebView2.\n\nPlease ensure WebView2 Runtime is installed:\nhttps://go.microsoft.com/fwlink/p/?LinkId=2124703",
            L"WebView2 UI Error",
            MB_OK | MB_ICONERROR);
    }
}

void MainWindow::OnDestroy() {
    LOG("MainWindow::OnDestroy - saving position");
    KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
    KillTimer(hwnd_, SHORT_REVEAL_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID);
    KillTimer(hwnd_, SIZE_MAXIMIZED_PROBE_T100_TIMER_ID);
    KillTimer(hwnd_, SIZE_RESTORED_PROBE_T100_TIMER_ID);
    KillTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID);
    KillTimer(hwnd_, BACKDROP_KICK_EXIT_TIMER_ID);
    startupPresentationCoordinator_.MarkClosed();
    SyncStartupRevealProjection();
    startupSurfaceAuthorityPending_ = false;
    startupSurfaceCommitPending_ = false;
    hasPendingStartupSurfaceBounds_ = false;
    startupSurfaceFirstPresentCommitted_ = false;
    startupDeferredSurfaceConvergePending_ = false;
    // 确保在任何退出方式下都保存窗口位置
    SaveWindowPosition();
    chromeBackdropBroadcastReady_ = false;
    // 使用基类方法销毁 WebView
    DestroyWebView();
}

void MainWindow::OnClose() {
    LOG("MainWindow closing - saving position and exiting foobar2000");
    
    // 在退出前保存窗口位置
    SaveWindowPosition();
    
    // Use standard command to exit foobar2000 properly
    standard_commands::run_main(standard_commands::guid_main_exit);
}


void MainWindow::OnSize(int width, int height) {
    if (!webView_ || !webView_->IsReady())
        return;

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);

    if (isMinimized_) {
        // 不手动隐藏 WebView — WebView2 运行时自动处理
        return;
    }

    if (startupSurfaceAuthorityPending_) {
        pendingStartupSurfaceBounds_ = clientRect;
        hasPendingStartupSurfaceBounds_ = true;

        const std::string cachedBoundsText = S_FormatNativeRect(clientRect);
        std::ostringstream stream;
        stream << "[StartupSurfaceAuthority]"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " owner=OnSize"
               << " action=cache-only"
               << " startupSurfaceAuthorityPending=" << WindowChromeTrace::BoolText(startupSurfaceAuthorityPending_)
               << " startupSurfaceCommitPending=" << WindowChromeTrace::BoolText(startupSurfaceCommitPending_)
               << " width=" << width
               << " height=" << height
               << " cachedBounds=" << cachedBoundsText.c_str();
        S_EmitEvidenceLine(stream.str());

        std::string extra = std::string("width=") + std::to_string(width) +
            " height=" + std::to_string(height) +
            " cachedBounds=" + cachedBoundsText +
            " startupSurfaceAuthorityPending=" + WindowChromeTrace::BoolText(startupSurfaceAuthorityPending_);
        TraceWindowPhase("OnSize.startup-cache-only", nullptr, nullptr, nullptr, nullptr, extra.c_str());
        return;
    }
    
    // WebView 覆盖整个客户区，让 DWM 效果完全穿透
    // 不再内缩，窗口调整由 WM_NCHITTEST 处理
    webView_->Resize(clientRect);
}

bool MainWindow::BroadcastWindowStateChangedIfNeeded(bool force) {
    if (!webView_ || !webView_->IsReady() || !chromeBackdropBroadcastReady_) {
        return false;
    }

    if (!force && hasBroadcastWindowState_ &&
        lastBroadcastIsMaximized_ == isMaximized_ &&
        lastBroadcastIsMinimized_ == isMinimized_ &&
        lastBroadcastIsActive_    == isActive_ &&
        lastBroadcastIsFullscreen_ == isFullscreen_) {
        return false;
    }

    hasBroadcastWindowState_ = true;
    lastBroadcastIsMaximized_  = isMaximized_;
    lastBroadcastIsMinimized_  = isMinimized_;
    lastBroadcastIsActive_     = isActive_;
    lastBroadcastIsFullscreen_ = isFullscreen_;

    json stateData = {
        {"isMaximized",  isMaximized_},
        {"isMinimized",  isMinimized_},
        {"maximized",    isMaximized_},
        {"minimized",    isMinimized_},
        {"isActive",     isActive_},
        {"active",       isActive_},
        {"isFullscreen", isFullscreen_},
        {"fullscreen",   isFullscreen_}
    };
    WebViewContext::GetInstance().BroadcastEvent("window:stateChanged", stateData);

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=BroadcastWindowStateChanged"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " force=" << WindowChromeTrace::BoolText(force)
               << " active=" << WindowChromeTrace::BoolText(isActive_)
               << " isActive=" << WindowChromeTrace::BoolText(isActive_)
               << " maximized=" << WindowChromeTrace::BoolText(isMaximized_)
               << " minimized=" << WindowChromeTrace::BoolText(isMinimized_)
               << " fullscreen=" << WindowChromeTrace::BoolText(isFullscreen_);
        S_EmitEvidenceLine(stream.str());
    }

    return true;
}

void MainWindow::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    
    // 完全不绘制任何内容
    // 窗口背景由 DWM Mica/Acrylic 效果处理
    // WebView 的透明背景会显示 DWM 效果
    // "Loading" 状态由前端 HTML 处理
    
    EndPaint(hwnd_, &ps);
}

// Test page HTML moved to separate file for maintainability
#include "window/TestPageHtml.inl"

void MainWindow::OnWebViewReady() {
    LOG("MainWindow::OnWebViewReady");
    
    // 初始化 WindowManager（windowId 已在 OnCreate 中设置）
    WindowManager::GetInstance().Initialize(this);
    
    // 强制整个窗口重绘（MainWindow 特有，DWM 效果需要）
    RedrawWindow(hwnd_, nullptr, nullptr, 
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    
    // 调用基类实现（API 注册、回调初始化、页面加载）
    WebViewPanel::OnWebViewReady();
    
    // 为了向后兼容，同时绑定到 BridgeCore 单例
    // 这确保现有使用 BridgeCore::GetInstance() 的代码仍然正常工作
    BridgeCore::GetInstance().SetWebView(webView_.get());
    
    // 触发重新布局
    RECT rect;
    GetClientRect(hwnd_, &rect);
    hasBroadcastWindowState_ = true;
    lastBroadcastIsMaximized_ = isMaximized_;
    lastBroadcastIsMinimized_ = isMinimized_;
    lastBroadcastIsActive_ = isActive_;
    OnSize(rect.right, rect.bottom);

    // [Legacy] cold-start reveal 路径下 startupChromeLayerSuppressed_ 始终为 false，
    // 此赋值为无害 legacy 保留。
    startupChromeLayerSuppressed_ = false;

    // [Chrome Reconciliation] 不再在 OnWebViewReady 中切换 WS_POPUP。
    // 稳态 native frame strategy 延迟到 FinalizeStartupRevealSettlement 中
    // 由 shared chrome apply 统一处理。

    chromeBackdropBroadcastReady_ = false;
    ApplyResolvedChrome(true);

    // Cold-start reveal：WebView 就绪后取消短超时 timer（若尚未触发）
    if (hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, SHORT_REVEAL_TIMER_ID);
    }

    MaybeCommitStartupRevealAfterChromeReady("OnWebViewReady");

    // Cold-start reveal 路径：若 reveal 已在 TryCommitStartupReveal 中完成但
    // settlement 尚未执行，补做一次（幂等）。
    if (startupRevealCommitted_) {
        FinalizeStartupRevealSettlement();
    }
    
    LOG("MainWindow::OnWebViewReady completed");
}

// ============================================
// 启动 reveal 状态机
// ============================================

void MainWindow::OnNavigationCompleted(bool success) {
    ApplyStartupPresentationDecision(
        startupPresentationCoordinator_.OnNavigationCompleted(success, IsStartupRevealChromeReady()),
        success ? "OnNavigationCompleted" : "OnNavigationCompletedFailed");
}

void MainWindow::OnWindowReadySignal(const std::string& source) {
    ApplyStartupPresentationDecision(
        startupPresentationCoordinator_.OnWindowReadySignal(source, IsStartupRevealChromeReady()),
        "OnWindowReadySignal");
}

void MainWindow::OnVisualReadySignal(const std::string& source) {
    ApplyStartupPresentationDecision(
        startupPresentationCoordinator_.OnVisualReadySignal(source, IsStartupRevealChromeReady()),
        "OnVisualReadySignal");
}

void MainWindow::OnSetFocus() {
    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=MainWindow::OnSetFocus"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " foregroundOwned=" << WindowChromeTrace::BoolText(IsForegroundOwnedByMainWindow())
               << " foregroundHwnd=0x" << pfc::format_hex((size_t)::GetForegroundWindow())
               << " isActive=" << WindowChromeTrace::BoolText(isActive_);
        S_EmitEvidenceLine(stream.str());
    }
    // Only propagate panel-level focus; window activation is solely decided by WM_ACTIVATE/WM_ACTIVATEAPP.
    WebViewPanel::OnSetFocus();
}

void MainWindow::OnKillFocus() {
    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=MainWindow::OnKillFocus"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " foregroundOwned=" << WindowChromeTrace::BoolText(IsForegroundOwnedByMainWindow())
               << " foregroundHwnd=0x" << pfc::format_hex((size_t)::GetForegroundWindow())
               << " isActive=" << WindowChromeTrace::BoolText(isActive_);
        S_EmitEvidenceLine(stream.str());
    }
    // Only propagate panel-level focus; window activation is solely decided by WM_ACTIVATE/WM_ACTIVATEAPP.
    WebViewPanel::OnKillFocus();
}

void MainWindow::OnWebViewProcessFailed(COREWEBVIEW2_PROCESS_FAILED_KIND failedKind, bool recovered) {
    // Forward to the base class first so `webview:processFailed` reaches every
    // window front-end before MainWindow records its window-state evidence.
    WebViewPanel::OnWebViewProcessFailed(failedKind, recovered);

    // WebViewHost 已写入 webview_crash.log 并按崩溃类型分级处理。
    // 此处补一条 ActivationEvidence 便于把崩溃与窗口状态（最小化/前台/可见）关联定位。
    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=MainWindow::OnWebViewProcessFailed"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " failedKind=" << static_cast<int>(failedKind)
               << " recovered=" << WindowChromeTrace::BoolText(recovered)
               << " isMinimized=" << WindowChromeTrace::BoolText(isMinimized_)
               << " isActive=" << WindowChromeTrace::BoolText(isActive_)
               << " foregroundOwned=" << WindowChromeTrace::BoolText(IsForegroundOwnedByMainWindow());
        S_EmitEvidenceLine(stream.str());
    }

    // 渲染进程类崩溃：WebViewHost 已自行 Reload，恢复后重新把焦点路由进页面，
    // 避免"恢复但键盘失灵"。浏览器进程退出（recovered=false）的整窗重建留待后续阶段。
    if (recovered && webView_ && webView_->IsReady() && !isMinimized_) {
        webView_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
}

bool MainWindow::IsStartupRevealChromeReady() const {
    const bool hasPluginEffect =
        S_IsPluginManagedBackdropEffect(resolvedChromeState_.effectiveActiveEffect) ||
        S_IsPluginManagedBackdropEffect(resolvedChromeState_.effectiveInactiveEffect);
    if (!hasPluginEffect && !resolvedChromeState_.transparentBackground) {
        return false;
    }
    // chrome ready 语义不弱于一次真实 apply：WebView 必须已就绪，
    // 这样 transparent background 和 RefreshForDwmEffect 才真正生效
    return webView_ && webView_->IsReady();
}

bool MainWindow::IsForegroundOwnedByMainWindow() const {
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return false;
    }

    HWND foreground = ::GetForegroundWindow();
    if (!foreground) {
        return false;
    }

    return foreground == hwnd_ || IsChild(hwnd_, foreground) != FALSE;
}

void MainWindow::MaybeCommitStartupRevealAfterChromeReady(const char* reason) {
    const auto startupPresentation = startupPresentationCoordinator_.GetSnapshot();
    if (!startupPresentation.revealPending || startupPresentation.revealCommitted) {
        return;
    }
    if (!IsStartupRevealChromeReady()) {
        return;
    }

    LOG("MainWindow: Chrome prepared during startup reveal, reason=", reason,
        " activeEffect=", resolvedChromeState_.effectiveActiveEffect.c_str(),
        " inactiveEffect=", resolvedChromeState_.effectiveInactiveEffect.c_str(),
        " transparent=", resolvedChromeState_.transparentBackground ? "true" : "false",
        " windowReadySignaled=", startupPresentation.windowReadySignaled ? "true" : "false",
        " visualReadySignaled=", startupPresentation.visualReadySignaled ? "true" : "false");

    ApplyStartupPresentationDecision(startupPresentationCoordinator_.OnChromeReady(), reason);
}

void MainWindow::SyncStartupRevealProjection() {
    const auto startupPresentation = startupPresentationCoordinator_.GetSnapshot();
    startupRevealPending_ = startupPresentation.revealPending;
    startupNavigationCompleted_ = startupPresentation.navigationCompleted;
    startupReadySignaled_ = startupPresentation.windowReadySignaled;
    startupRevealCommitted_ = startupPresentation.revealCommitted;
    startupRevealSettling_ = startupPresentation.revealSettling;
}

void MainWindow::BypassStartupRevealForImmediateShow() {
    // [DEPRECATED] cold-start reveal 路径下已不再调用此方法；保留方法体仅用于回退诊断。
    startupPresentationCoordinator_.MarkRevealStarted(false);
    startupPresentationCoordinator_.MarkRevealSettled();
    SyncStartupRevealProjection();

    startupSurfaceAuthorityPending_ = false;
    startupSurfaceCommitPending_ = false;
    pendingStartupSurfaceBounds_ = RECT{0, 0, 0, 0};
    hasPendingStartupSurfaceBounds_ = false;
    startupSurfaceFirstPresentCommitted_ = false;
    startupDeferredSurfaceConvergePending_ = false;
    deferredBackdropPending_ = false;

    // Experiment G: 即使在 immediate show 模式下，也启动 deferred backdrop timer
    // 以触发 NRB 翻转，迫使 DWM 重建 visual tree。
    if (hwnd_ && IsWindow(hwnd_) && !isMinimized_) {
        needsAuthoritativeChromeReapply_ = true;
        SetTimer(hwnd_, DEFERRED_BACKDROP_TIMER_ID, DEFERRED_BACKDROP_DELAY_MS, nullptr);
    } else {
        needsAuthoritativeChromeReapply_ = false;
    }

    FB2K_console_formatter()
        << "[StartupRevealBypass]"
        << " t=" << WindowChromeTrace::RelativeMs() << "ms"
        << " mode=immediate-show"
        << " startupRevealPending=" << WindowChromeTrace::BoolText(startupRevealPending_)
        << " startupRevealCommitted=" << WindowChromeTrace::BoolText(startupRevealCommitted_)
        << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_);
}

void MainWindow::ApplyStartupPresentationDecision(const StartupPresentationCoordinatorDecision& decision,
                                                  const char* reason) {
    if (decision.cancelFallbackTimer && hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);
    }

    if (decision.armFallbackTimer) {
        ArmStartupFallbackTimer();
    }

    if (!decision.commitReveal) {
        SyncStartupRevealProjection();
        return;
    }

    LOG("MainWindow: Startup presentation commit requested, reason=", reason,
        " fallback=", decision.fallbackUsed ? "true" : "false");
    TryCommitStartupReveal();
}

void MainWindow::RefreshStartupRevealSurface() {
    if (!hwnd_ || !IsWindow(hwnd_) || !webView_ || !webView_->IsReady() || isMinimized_) {
        return;
    }

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    webView_->SetVisible(true);
    webView_->Resize(clientRect);
}

bool MainWindow::RestoreSurfaceAfterHidden(const char* reason) {
    const char* restoreReason = (reason && reason[0]) ? reason : "hidden-restore";

    if (!hwnd_ || !IsWindow(hwnd_) || !IsWindowVisible(hwnd_) || isMinimized_) {
        return false;
    }

    if (startupRevealPending_ || !startupRevealCommitted_ || startupRevealSettling_) {
        return false;
    }

    if (!webView_ || !webView_->IsReady()) {
        return false;
    }

    LogSurfaceDiagnostics((std::string("RestoreSurfaceAfterHidden.enter/") + restoreReason).c_str());
    // 复原内存目标等级：隐藏到托盘/后台时设过 Low，恢复可见后必须回 Normal，
    // 否则可见态下仍被激进 trim 影响性能。（SetVisible(true) 由 RefreshStartupRevealSurface 负责。）
    webView_->SetMemoryUsageLow(false);
    EnsureAuthoritativeNativeChrome(restoreReason);
    RefreshStartupRevealSurface();
    const bool restored = EnsureSurfaceConvergedAfterNativeFrame(restoreReason);

    if (auto* controller = webView_->GetController()) {
        controller->NotifyParentWindowPositionChanged();
    }

    LogSurfaceDiagnostics((std::string("RestoreSurfaceAfterHidden.exit/") + restoreReason).c_str());
    CaptureRuntimeDomProbe(std::string("RestoreSurfaceAfterHidden.exit/") + restoreReason);
    // 任何隐藏→可见的恢复后，重新评估是否（仍）被完全覆盖
    ScheduleCoverReevaluation();
    return restored;
}

void MainWindow::SetBgSuspend(unsigned reason, bool active, const char* why) {
    const unsigned prev = bgSuspendReasons_;
    if (active) bgSuspendReasons_ |= reason;
    else        bgSuspendReasons_ &= ~reason;
    if (bgSuspendReasons_ == prev) return;   // 该原因状态未变
    if (!webView_) return;

    // 最小化 / 托盘隐藏各自管理可见性（页面已被它们隐藏）。这两种状态下本路径
    // 不动可见性，只维护位掩码；待回到普通可见态时各自的恢复路径会重新呈现。
    if (isMinimized_ || !hwnd_ || !IsWindow(hwnd_) || !IsWindowVisible(hwnd_)) return;

    const bool wasSuspended = (prev != 0);
    const bool nowSuspended = (bgSuspendReasons_ != 0);
    if (nowSuspended && !wasSuspended) {
        // 首个挂起原因：让页面进入 hidden/暂停态并 trim 内存。
        webView_->SetVisible(false);
        webView_->SetMemoryUsageLow(true);
        LogSurfaceDiagnostics((std::string("BgSuspend.hide/") + (why ? why : "")).c_str());
    } else if (!nowSuspended && wasSuspended) {
        // 最后一个原因清除：复原（SetVisible(true)+nudge，RestoreSurfaceAfterHidden 内含 Normal）。
        RestoreSurfaceAfterHidden(why ? why : "bg-resume");
    }
}

bool MainWindow::IsMainWindowFullyCovered() const {
    if (!hwnd_ || !IsWindow(hwnd_)) return false;
    if (isMinimized_ || !IsWindowVisible(hwnd_)) return false;  // 最小化/隐藏不算“被覆盖”
    if (isFullscreen_) return false;                            // 自己全屏

    RECT self{};
    if (!GetWindowRect(hwnd_, &self) || IsRectEmpty(&self)) return false;

    HWND fg = GetForegroundWindow();
    if (!fg || fg == hwnd_) return false;                       // 自己在前台 → 未被覆盖
    if (GetAncestor(fg, GA_ROOTOWNER) == hwnd_) return false;   // 自己的 owned 弹窗/歌词
    if (!IsWindowVisible(fg)) return false;

    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(fg, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
        return false;                                           // UWP/异虚拟桌面 cloak

    const LONG_PTR ex = GetWindowLongPtrW(fg, GWL_EXSTYLE);
    if (ex & WS_EX_TRANSPARENT) return false;                   // 点击穿透层
    if (ex & WS_EX_LAYERED) {
        BYTE alpha = 255; COLORREF key = 0; DWORD lflags = 0;
        if (GetLayeredWindowAttributes(fg, &key, &alpha, &lflags)) {
            if ((lflags & LWA_ALPHA) && alpha < 255) return false;  // 半透明
        } else {
            return false;                                       // per-pixel alpha 无法判定 → 保守放弃
        }
    }

    HMONITOR mSelf = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONULL);
    HMONITOR mFg   = MonitorFromWindow(fg,    MONITOR_DEFAULTTONULL);
    if (!mSelf || mSelf != mFg) return false;                   // 必须同屏

    RECT fr{};
    if (!GetWindowRect(fg, &fr)) return false;
    // 前台窗口几何上完全包含本窗口 → 判定完全覆盖（保守；部分遮挡不处理）
    return (fr.left <= self.left && fr.top <= self.top &&
            fr.right >= self.right && fr.bottom >= self.bottom);
}

void MainWindow::ScheduleCoverReevaluation() {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    if (startupRevealPending_ || !startupRevealCommitted_ || startupRevealSettling_) return;
    // 防抖：连续激活/位置变化只在静默 COVER_REEVAL_DELAY_MS 后评估一次
    SetTimer(hwnd_, COVER_REEVAL_TIMER_ID, COVER_REEVAL_DELAY_MS, nullptr);
}

void MainWindow::EvaluateCoverState() {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    if (startupRevealPending_ || !startupRevealCommitted_ || startupRevealSettling_) return;

    // 最小化/托盘隐藏由各自路径管理可见性；此处仅清账并停轮询
    if (isMinimized_ || !IsWindowVisible(hwnd_)) {
        ClearCoverSuspend();
        return;
    }

    if (IsMainWindowFullyCovered()) {
        SetBgSuspend(kBgSuspendCovered, true, "covered");
        // 启动/续期轮询以检测“重新露出”（仅覆盖态运行）
        SetTimer(hwnd_, COVER_POLL_TIMER_ID, COVER_POLL_INTERVAL_MS, nullptr);
    } else {
        SetBgSuspend(kBgSuspendCovered, false, "uncovered");
        KillTimer(hwnd_, COVER_POLL_TIMER_ID);
    }
}

void MainWindow::ClearCoverSuspend() {
    bgSuspendReasons_ &= ~kBgSuspendCovered;
    if (hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, COVER_POLL_TIMER_ID);
        KillTimer(hwnd_, COVER_REEVAL_TIMER_ID);
    }
}

void MainWindow::EmitInteractiveResizeEvidence(const char* phase,
                                               const char* detail,
                                               const RECT* sizingRect,
                                               const WINDOWPOS* windowPos) const {
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return;
    }

    RECT windowRect{};
    RECT clientRect{};
    GetWindowRect(hwnd_, &windowRect);
    GetClientRect(hwnd_, &clientRect);

    RECT controllerBounds{};
    bool hasControllerBounds = false;
    if (webView_ && webView_->IsReady() && webView_->GetController()) {
        hasControllerBounds = SUCCEEDED(webView_->GetController()->get_Bounds(&controllerBounds));
    }

    int backdropRead = -1;
    const HRESULT backdropHr = S_DwmGetWindowAttribute(
        hwnd_, DWMWA_SYSTEMBACKDROP_TYPE_V, &backdropRead, sizeof(backdropRead));

    const std::string windowRectText = S_FormatNativeRect(windowRect);
    const std::string clientRectText = S_FormatNativeRect(clientRect);
    const std::string controllerBoundsText = hasControllerBounds
        ? S_FormatNativeRect(controllerBounds)
        : "N/A";
    const std::string sizingRectText = sizingRect ? S_FormatNativeRect(*sizingRect) : "N/A";

    std::ostringstream stream;
    stream << "[InteractiveResize]"
           << " t=" << WindowChromeTrace::RelativeMs() << "ms"
           << " phase=" << (phase && phase[0] ? phase : "-")
           << " detail=" << (detail && detail[0] ? detail : "-")
           << " session=" << interactiveSizingSessionId_
           << " interactiveSizingActive=" << WindowChromeTrace::BoolText(interactiveSizingActive_)
           << " startupRevealPending=" << WindowChromeTrace::BoolText(startupRevealPending_)
           << " startupRevealCommitted=" << WindowChromeTrace::BoolText(startupRevealCommitted_)
           << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_)
           << " isActive=" << WindowChromeTrace::BoolText(isActive_)
           << " isMinimized=" << WindowChromeTrace::BoolText(isMinimized_)
           << " isMaximized=" << WindowChromeTrace::BoolText(isMaximized_)
           << " visible=" << WindowChromeTrace::BoolText(IsWindowVisible(hwnd_) != FALSE)
           << " iconic=" << WindowChromeTrace::BoolText(IsIconic(hwnd_) != FALSE)
           << " zoomed=" << WindowChromeTrace::BoolText(IsZoomed(hwnd_) != FALSE)
           << " windowRect=" << windowRectText.c_str()
           << " clientRect=" << clientRectText.c_str()
           << " controllerBounds=" << controllerBoundsText.c_str()
           << " sizingRect=" << sizingRectText.c_str()
           << " backdropHr=0x" << pfc::format_hex((uint32_t)backdropHr)
           << " backdropRead=" << backdropRead;

    if (windowPos) {
        const std::string windowPosFlags = S_FormatWindowPosFlags(windowPos->flags);
        stream << " windowPos.flags=0x" << pfc::format_hex((uint32_t)windowPos->flags)
               << " windowPos.flagsText=" << windowPosFlags.c_str()
               << " windowPos.x=" << windowPos->x
               << " windowPos.y=" << windowPos->y
               << " windowPos.cx=" << windowPos->cx
               << " windowPos.cy=" << windowPos->cy;
    }

    S_EmitEvidenceLine(stream.str());
}

void MainWindow::LogSurfaceDiagnostics(const char* phase) const {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);

    RECT controllerBounds{};
    bool hasBounds = false;
    if (webView_ && webView_->IsReady() && webView_->GetController()) {
        hasBounds = SUCCEEDED(webView_->GetController()->get_Bounds(&controllerBounds));
    }

    std::ostringstream stream;
    stream << "[SurfaceDiag] phase=" << phase
           << " t=" << WindowChromeTrace::RelativeMs() << "ms"
           << " hwnd=0x" << pfc::format_hex((size_t)hwnd_)
           << " clientRect=(" << clientRect.left << "," << clientRect.top
           << "," << clientRect.right << "," << clientRect.bottom << ")"
           << " controllerBounds=" << (hasBounds
               ? (std::string("(") + std::to_string(controllerBounds.left) + ","
                  + std::to_string(controllerBounds.top) + ","
                  + std::to_string(controllerBounds.right) + ","
                  + std::to_string(controllerBounds.bottom) + ")").c_str()
               : "N/A")
           << " visible=" << WindowChromeTrace::BoolText(IsWindowVisible(hwnd_) != FALSE)
           << " iconic=" << WindowChromeTrace::BoolText(IsIconic(hwnd_) != FALSE)
           << " zoomed=" << WindowChromeTrace::BoolText(IsZoomed(hwnd_) != FALSE);
    S_EmitEvidenceLine(stream.str());
}

void MainWindow::CaptureRuntimeDomProbe(const std::string& phase, int scheduledDelayMs) {
    const double requestedAtMs = static_cast<double>(WindowChromeTrace::RelativeMs());
    const HWND hwnd = hwnd_;
    const std::string windowId = GetWindowId().empty() ? "main" : GetWindowId();

    if (!hwnd || !IsWindow(hwnd)) {
        std::ostringstream stream;
        stream << "[RuntimeDomProbe.skip]"
               << " tRequest=" << requestedAtMs << "ms"
               << " phase=" << phase.c_str()
               << " scheduledDelayMs=" << scheduledDelayMs
               << " reason=invalid-hwnd";
        S_EmitEvidenceLine(stream.str());
        return;
    }

    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);

    RECT controllerBounds{};
    bool hasBounds = false;
    const bool webViewReady = webView_ && webView_->IsReady();
    if (webViewReady && webView_->GetController()) {
        hasBounds = SUCCEEDED(webView_->GetController()->get_Bounds(&controllerBounds));
    }

    const bool visible = IsWindowVisible(hwnd) != FALSE;
    const bool iconic = IsIconic(hwnd) != FALSE;
    const bool zoomed = IsZoomed(hwnd) != FALSE;
    const bool active = isActive_;
    const bool startupRevealPending = startupRevealPending_;
    const bool startupRevealCommitted = startupRevealCommitted_;
    const bool startupRevealSettling = startupRevealSettling_;
    const std::string clientRectText = S_FormatNativeRect(clientRect);
    const std::string controllerBoundsText = hasBounds ? S_FormatNativeRect(controllerBounds) : "N/A";

    if (!webViewReady) {
        std::ostringstream stream;
        stream << "[RuntimeDomProbe.skip]"
               << " tRequest=" << requestedAtMs << "ms"
               << " phase=" << phase.c_str()
               << " scheduledDelayMs=" << scheduledDelayMs
               << " windowId=" << windowId.c_str()
               << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
               << " reason=webview-not-ready"
               << " clientRect=" << clientRectText.c_str()
               << " controllerBounds=" << controllerBoundsText.c_str();
        S_EmitEvidenceLine(stream.str());
        return;
    }

    static const wchar_t* runtimeProbeScript = LR"JS((() => {
        const de = document.documentElement;
        const body = document.body;
        const getStyleSafe = (el) => {
            try {
                return el ? window.getComputedStyle(el) : null;
            } catch (_) {
                return null;
            }
        };
        const rectOf = (el) => {
            if (!el || typeof el.getBoundingClientRect !== 'function') return null;
            const r = el.getBoundingClientRect();
            return {
                left: Number(r.left || 0),
                top: Number(r.top || 0),
                width: Number(r.width || 0),
                height: Number(r.height || 0),
                right: Number(r.right || 0),
                bottom: Number(r.bottom || 0)
            };
        };
        const classNameOf = (el) => {
            if (!el) return '';
            if (typeof el.className === 'string') return el.className;
            if (typeof el.getAttribute === 'function') return el.getAttribute('class') || '';
            return '';
        };
        const describe = (el) => {
            if (!el) return null;
            const style = getStyleSafe(el);
            return {
                tag: el.tagName || '',
                id: el.id || '',
                className: classNameOf(el),
                background: style ? (style.backgroundColor || style.background || '') : '',
                opacity: style ? (style.opacity || '') : '',
                display: style ? (style.display || '') : '',
                visibility: style ? (style.visibility || '') : '',
                rect: rectOf(el)
            };
        };
        const overlay = document.querySelector('#server-loading-overlay, #server-loading, .server-loading-overlay');
        const overlayStyle = getStyleSafe(overlay);
        const overlayRect = rectOf(overlay);
        const overlayVisible = !!overlay && !!overlayStyle &&
            overlayStyle.display !== 'none' &&
            overlayStyle.visibility !== 'hidden' &&
            Number.parseFloat(overlayStyle.opacity || '1') > 0.001 &&
            !!overlayRect && overlayRect.width > 0 && overlayRect.height > 0;
        const centerEl = document.elementFromPoint(
            Math.max(0, Math.floor((window.innerWidth || 0) / 2)),
            Math.max(0, Math.floor((window.innerHeight || 0) / 2))
        );
        const htmlStyle = getStyleSafe(de);
        const bodyStyle = getStyleSafe(body);
        return {
            href: location.href,
            readyState: document.readyState,
            visibilityState: document.visibilityState,
            innerWidth: window.innerWidth || 0,
            innerHeight: window.innerHeight || 0,
            htmlBackground: htmlStyle ? (htmlStyle.backgroundColor || htmlStyle.background || '') : '',
            htmlOpacity: htmlStyle ? (htmlStyle.opacity || '') : '',
            bodyBackground: bodyStyle ? (bodyStyle.backgroundColor || bodyStyle.background || '') : '',
            bodyOpacity: bodyStyle ? (bodyStyle.opacity || '') : '',
            overlayExists: !!overlay,
            overlayVisible,
            overlay: describe(overlay),
            center: describe(centerEl)
        };
    })())JS";

    const HRESULT executeHr = webView_->ExecuteScript(runtimeProbeScript,
        // 本回调已证明 nothrow：所有可抛操作均在外层 try 内，两个 catch 处理器各自再以 try/catch(...) 兜底
        //（日志路径 OOM 也不外抛）。clang-tidy bugprone-exception-escape 对该嵌套 try/catch 模式误报，最小范围抑制。
        // NOLINTNEXTLINE(bugprone-exception-escape)
        [phase, scheduledDelayMs, requestedAtMs, hwnd, windowId,
         clientRectText, controllerBoundsText,
         visible, iconic, zoomed, active,
         startupRevealPending, startupRevealCommitted, startupRevealSettling](const std::wstring& resultJson) {
            // 本地防御：DOM 探针完成回调跑在主线程 WebView2 回调里，下方 json 的 .value()/.contains()
            // 访问若抛 type_error 等异常并逃逸原生回调边界会触发 terminate() 崩溃整个 fb2k，故整段兜异常。
            try {
            json probe;
            if (!S_ParseExecuteScriptPayload(resultJson, probe)) {
                std::string rawUtf8(pfc::stringcvt::string_utf8_from_wide(resultJson.c_str()).get_ptr());
                std::ostringstream stream;
                stream << "[RuntimeDomProbe.error]"
                       << " tRequest=" << requestedAtMs << "ms"
                       << " tComplete=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " phase=" << phase.c_str()
                       << " scheduledDelayMs=" << scheduledDelayMs
                       << " windowId=" << windowId.c_str()
                       << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
                       << " raw=" << S_TruncateUtf8(rawUtf8, 240).c_str();
                S_EmitEvidenceLine(stream.str());
                return;
            }

            const json overlay = probe.contains("overlay") && probe["overlay"].is_object()
                ? probe["overlay"]
                : json::object();
            const json center = probe.contains("center") && probe["center"].is_object()
                ? probe["center"]
                : json::object();
            const std::string overlayRectText = S_FormatDomRect(overlay.value("rect", json()));
            const std::string centerRectText = S_FormatDomRect(center.value("rect", json()));
            const std::string overlayId = overlay.value("id", "");
            const std::string overlayClass = overlay.value("className", "");
            const std::string overlayBackground = overlay.value("background", "");
            const std::string overlayOpacity = overlay.value("opacity", "");
            const std::string centerTag = center.value("tag", "");
            const std::string centerId = center.value("id", "");
            const std::string centerClass = center.value("className", "");
            const std::string centerBackground = center.value("background", "");
            const std::string centerOpacity = center.value("opacity", "");
            const std::string htmlBackground = probe.value("htmlBackground", "");
            const std::string htmlOpacity = probe.value("htmlOpacity", "");
            const std::string bodyBackground = probe.value("bodyBackground", "");
            const std::string bodyOpacity = probe.value("bodyOpacity", "");
            const std::string href = probe.value("href", "");
            const std::string readyState = probe.value("readyState", "");
            const std::string visibilityState = probe.value("visibilityState", "");

            std::ostringstream stream;
            stream << "[RuntimeDomProbe]"
                   << " tRequest=" << requestedAtMs << "ms"
                   << " tComplete=" << WindowChromeTrace::RelativeMs() << "ms"
                   << " phase=" << phase.c_str()
                   << " scheduledDelayMs=" << scheduledDelayMs
                   << " windowId=" << windowId.c_str()
                   << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
                   << " startupRevealPending=" << WindowChromeTrace::BoolText(startupRevealPending)
                   << " startupRevealCommitted=" << WindowChromeTrace::BoolText(startupRevealCommitted)
                   << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling)
                   << " active=" << WindowChromeTrace::BoolText(active)
                   << " visible=" << WindowChromeTrace::BoolText(visible)
                   << " iconic=" << WindowChromeTrace::BoolText(iconic)
                   << " zoomed=" << WindowChromeTrace::BoolText(zoomed)
                   << " clientRect=" << clientRectText.c_str()
                   << " controllerBounds=" << controllerBoundsText.c_str()
                   << " readyState=" << readyState.c_str()
                   << " visibilityState=" << visibilityState.c_str()
                   << " inner=" << probe.value("innerWidth", 0) << "x" << probe.value("innerHeight", 0)
                   << " htmlBg=" << htmlBackground.c_str()
                   << " htmlOpacity=" << htmlOpacity.c_str()
                   << " bodyBg=" << bodyBackground.c_str()
                   << " bodyOpacity=" << bodyOpacity.c_str()
                   << " overlayExists=" << WindowChromeTrace::BoolText(probe.value("overlayExists", false))
                   << " overlayVisible=" << WindowChromeTrace::BoolText(probe.value("overlayVisible", false))
                   << " overlayId=" << overlayId.c_str()
                   << " overlayClass=" << overlayClass.c_str()
                   << " overlayBg=" << overlayBackground.c_str()
                   << " overlayOpacity=" << overlayOpacity.c_str()
                   << " overlayRect=" << overlayRectText.c_str()
                   << " centerTag=" << centerTag.c_str()
                   << " centerId=" << centerId.c_str()
                   << " centerClass=" << centerClass.c_str()
                   << " centerBg=" << centerBackground.c_str()
                   << " centerOpacity=" << centerOpacity.c_str()
                   << " centerRect=" << centerRectText.c_str()
                   << " href=" << href.c_str();
            S_EmitEvidenceLine(stream.str());
            } catch (const std::exception& e) {
                // nothrow catch：日志路径(ostringstream/string)在 OOM 下也可能抛 bad_alloc，
                // 故再包一层吞掉，确保异常绝不逃逸 WebView2 回调边界（清 bugprone-exception-escape）。
                try {
                    std::ostringstream errStream;
                    errStream << "[RuntimeDomProbe.error]"
                              << " tRequest=" << requestedAtMs << "ms"
                              << " tComplete=" << WindowChromeTrace::RelativeMs() << "ms"
                              << " phase=" << phase.c_str()
                              << " scheduledDelayMs=" << scheduledDelayMs
                              << " windowId=" << windowId.c_str()
                              << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
                              << " reason=HandlerException"
                              << " what=" << S_TruncateUtf8(e.what(), 240).c_str();
                    S_EmitEvidenceLine(errStream.str());
                } catch (...) {}
            } catch (...) {
                // nothrow catch：同上，吞掉日志路径自身可能的异常。
                try {
                    std::ostringstream errStream;
                    errStream << "[RuntimeDomProbe.error]"
                              << " tRequest=" << requestedAtMs << "ms"
                              << " tComplete=" << WindowChromeTrace::RelativeMs() << "ms"
                              << " phase=" << phase.c_str()
                              << " scheduledDelayMs=" << scheduledDelayMs
                              << " windowId=" << windowId.c_str()
                              << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
                              << " reason=HandlerException";
                    S_EmitEvidenceLine(errStream.str());
                } catch (...) {}
            }
        });

    if (FAILED(executeHr)) {
        std::ostringstream stream;
        stream << "[RuntimeDomProbe.error]"
               << " tRequest=" << requestedAtMs << "ms"
               << " phase=" << phase.c_str()
               << " scheduledDelayMs=" << scheduledDelayMs
               << " windowId=" << windowId.c_str()
               << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
               << " reason=ExecuteScriptFailed"
               << " hr=0x" << pfc::format_hex((uint32_t)executeHr);
        S_EmitEvidenceLine(stream.str());
    }
}

void MainWindow::ScheduleFinalizeStartupRuntimeProbes() {
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return;
    }

    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID);
    SetTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID, 100, nullptr);
    SetTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID, 250, nullptr);
    SetTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID, 500, nullptr);
}

void MainWindow::ScheduleManualResizeRuntimeProbes(const char* sizeType) {
    if (!sizeType || !sizeType[0]) {
        return;
    }

    const std::string normalized = sizeType;
    if (normalized == "maximized") {
        CaptureRuntimeDomProbe("WM_SIZE.sizeType=maximized/+0ms", 0);
        if (hwnd_ && IsWindow(hwnd_)) {
            KillTimer(hwnd_, SIZE_MAXIMIZED_PROBE_T100_TIMER_ID);
            SetTimer(hwnd_, SIZE_MAXIMIZED_PROBE_T100_TIMER_ID, 100, nullptr);
        }
        return;
    }

    if (normalized == "restored") {
        CaptureRuntimeDomProbe("WM_SIZE.sizeType=restored/+0ms", 0);
        if (hwnd_ && IsWindow(hwnd_)) {
            KillTimer(hwnd_, SIZE_RESTORED_PROBE_T100_TIMER_ID);
            SetTimer(hwnd_, SIZE_RESTORED_PROBE_T100_TIMER_ID, 100, nullptr);
        }
    }
}

bool MainWindow::EnsureSurfaceConvergedAfterNativeFrame(const char* reason) {
    if (!hwnd_ || !IsWindow(hwnd_) || !webView_ || !webView_->IsReady() || isMinimized_) {
        return false;
    }

    auto* controller = webView_->GetController();
    if (!controller) return false;

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);

    RECT currentBounds{};
    controller->get_Bounds(&currentBounds);

    const bool boundsMatch = (clientRect.left == currentBounds.left &&
                              clientRect.top == currentBounds.top &&
                              clientRect.right == currentBounds.right &&
                              clientRect.bottom == currentBounds.bottom);

    FB2K_console_formatter()
        << "[SurfaceConverge] reason=" << reason
        << " t=" << WindowChromeTrace::RelativeMs() << "ms"
        << " clientRect=(" << clientRect.left << "," << clientRect.top
        << "," << clientRect.right << "," << clientRect.bottom << ")"
        << " controllerBounds=(" << currentBounds.left << "," << currentBounds.top
        << "," << currentBounds.right << "," << currentBounds.bottom << ")"
        << " match=" << WindowChromeTrace::BoolText(boundsMatch);

    // bounded nudge: right-1 ? finalRect
    // 即使 bounds 相同，也需通过尺寸抖动迫使 DComp surface 重新合成；
    // 若 bounds 不同，则同时完成收敛。
    RECT nudged = clientRect;
    nudged.right = (nudged.right > 0) ? nudged.right - 1 : 0;
    webView_->Resize(nudged);
    webView_->Resize(clientRect);

    LogSurfaceDiagnostics((std::string("SurfaceConverge.done/") + reason).c_str());
    return true;
}

void MainWindow::FinalizeStartupRevealSettlement() {
    SyncStartupRevealProjection();
    if (!startupRevealCommitted_ || isMinimized_) {
        return;
    }

    if (startupRevealSettling_) {
        startupPresentationCoordinator_.MarkRevealSettled();
        SyncStartupRevealProjection();
    } else if (chromeBackdropBroadcastReady_) {
        return;
    }

    // [Chrome Reconciliation] 启动期到稳态的相位切换：
    // 在 startup first-present + settlement 完成后，将 chromePhase_ 切到 SteadyState。
    // SteadyState 后 shared chrome apply 会通过 applyNativeFrameStrategy hook
    // 把窗口切为 CaptionlessOverlapped，代替旧的 WS_POPUP 策略。
    chromePhase_ = MainChromePhase::SteadyState;

    chromeBackdropBroadcastReady_ = true;

    // [Bug Fix] 临时解除 deferredBackdropPending_ 门控，使 ApplyResolvedChrome
    // 能穿透 ApplyBackdropPolicyForActivation 中的延迟写入守卫。
    // 根因：deferredBackdropPending_ 的职责是阻止 ShowWindow 后同步消息触发的
    // DWM backdrop 重复写入，但它同时也阻止了 NativeFrameStrategy（style bits）
    // 和 applyFrameless（DWM frame extension）的应用，导致三大键在 ShowWindow
    // 后 ~200ms 内持续可见，直到 DEFERRED_BACKDROP_TIMER 触发才消失。
    const bool savedDeferredBackdrop = deferredBackdropPending_;
    deferredBackdropPending_ = false;
    ApplyResolvedChrome(true);
    deferredBackdropPending_ = savedDeferredBackdrop;

    // [Cold-start reveal] WS_CAPTION / WS_MINIMIZEBOX / WS_MAXIMIZEBOX 始终保留，
    // 三大键通过移除 WS_SYSMENU 消除（DWM 按钮 overlay 完全依赖此位），
    // 无需在 settlement 阶段恢复任何样式位。

    BroadcastWindowStateChangedIfNeeded(true);
    CaptureRuntimeDomProbe("FinalizeStartupRevealSettlement/+0ms", 0);
    ScheduleFinalizeStartupRuntimeProbes();
}

bool MainWindow::EnsureAuthoritativeNativeChrome(const char* reason) {
    TraceWindowPhase("EnsureAuthoritativeNativeChrome.enter", nullptr, "true",
        nullptr, nullptr, reason);

    if (!hwnd_ || !IsWindow(hwnd_) || isMinimized_) {
        TraceWindowPhase("EnsureAuthoritativeNativeChrome.skip", nullptr, "true",
            nullptr, nullptr, "reason=invalid-hwnd-or-minimized");
        return false;
    }

    LOG("MainWindow: Authoritative native chrome reapply, reason=", reason,
        " activeEffect=", resolvedChromeState_.effectiveActiveEffect.c_str(),
        " inactiveEffect=", resolvedChromeState_.effectiveInactiveEffect.c_str(),
        " transparent=", resolvedChromeState_.transparentBackground ? "true" : "false");

    // DWM 对隐藏窗口设置的 SYSTEMBACKDROP_TYPE 不会初始化实际的合成管线，
    // ShowWindow 后重设相同值被视为 no-op。先 reset 为 NONE 再由
    // RefreshBackdropEffect 设回正确值，迫使 DWM 看到真正的状态变更。
    int resetType = DWMSBT_NONE_V;
    S_DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE_V,
        &resetType, sizeof(resetType));

    // 重新扩展帧到客户区，确保 hidden→visible 后帧边距仍然生效
    MARGINS margins = { -1, -1, -1, -1 };
    S_DwmExtendFrameIntoClientArea(hwnd_, &margins);

    ApplyResolvedChrome(true);
    ChromeController::RefreshNativeFrame(hwnd_);
    LogSurfaceDiagnostics((std::string("EnsureAuthoritativeNativeChrome.exit/") + reason).c_str());
    CaptureRuntimeDomProbe(std::string("EnsureAuthoritativeNativeChrome.exit/") + reason);
    TraceWindowPhase("EnsureAuthoritativeNativeChrome.exit", nullptr, "true",
        nullptr, nullptr, reason);
    return true;
}

bool MainWindow::ReapplyNativeChromeAfterStartupReveal() {
    return EnsureAuthoritativeNativeChrome("startup-reveal");
}

void MainWindow::TryCommitStartupReveal() {
    SyncStartupRevealProjection();
    const auto startupPresentation = startupPresentationCoordinator_.GetSnapshot();
    const bool fallbackUsed = startupPresentation.fallbackUsed;
    const bool visualReadySignaled = startupPresentation.visualReadySignaled;
    const std::string revealReason = fallbackUsed ? "fallback" : (visualReadySignaled ? "visual" : "ready");
    std::string enterExtra = std::string("startupNavigationCompleted=") +
        WindowChromeTrace::BoolText(startupNavigationCompleted_) +
        " startupReadySignaled=" + WindowChromeTrace::BoolText(startupReadySignaled_) +
        " savedShowCmd=" + std::to_string(savedShowCmd_) +
        " revealReason=" + revealReason;
    TraceWindowPhase("TryCommitStartupReveal.enter", nullptr, nullptr, nullptr, nullptr,
        enterExtra.c_str());

    if (startupRevealCommitted_) return;

    if (!hwnd_ || !IsWindow(hwnd_)) {
        startupPresentationCoordinator_.MarkClosed();
        SyncStartupRevealProjection();
        TraceWindowPhase("TryCommitStartupReveal.invalid-hwnd");
        return;
    }

    startupPresentationCoordinator_.MarkRevealStarted(fallbackUsed);
    SyncStartupRevealProjection();

    KillTimer(hwnd_, STARTUP_REVEAL_TIMER_ID);

    const bool canRefreshSurface = webView_ && webView_->IsReady();
    startupSurfaceAuthorityPending_ = canRefreshSurface && !isMinimized_;
    startupSurfaceCommitPending_ = startupSurfaceAuthorityPending_;
    pendingStartupSurfaceBounds_ = RECT{0, 0, 0, 0};
    hasPendingStartupSurfaceBounds_ = false;
    startupSurfaceFirstPresentCommitted_ = false;

    // 不在 ShowWindow 前让 WebView 进入最终 visible/bounds 状态，
    // 将 controller 置为 hidden + zero bounds，确保 post-show 的
    // SetVisible(true) + Resize(finalBounds) 是第一笔真实的 surface transition。
    // 实测已证明：仅有 visibility delta 不够，
    // bounds 也必须是真实 transition（从 zero 到 final）。
    if (canRefreshSurface) {
        webView_->SetVisible(false);
        webView_->Resize(RECT{0, 0, 0, 0});
    }

    int showCmd = savedShowCmd_;
    if (isMinimized_) {
        showCmd = SW_SHOWMINIMIZED;
    } else if (isMaximized_) {
        showCmd = SW_SHOWMAXIMIZED;
    } else if (showCmd == SW_SHOWMAXIMIZED ||
               showCmd == SW_SHOWMINIMIZED ||
               showCmd == SW_SHOWMINNOACTIVE ||
               showCmd == SW_MINIMIZE) {
        showCmd = SW_SHOW;
    }

    // 延迟 DWM 写入策略：不在 ShowWindow 之前写入 backdrop，
    // 而是在 ShowWindow 之后、窗口已可见时首次写入。
    // DWM 对隐藏窗口的 SYSTEMBACKDROP_TYPE 写入不会初始化合成管线，
    // 且 ShowWindow 后重写相同值被 DWM 视为 no-op。
    // 只 resolve state，供 ShowWindow 后立即使用。
    if (!isMinimized_ && showCmd != SW_SHOWMINIMIZED) {
        ResolveBackdropPolicy();
    }

    // 设置延迟 backdrop 门控：阻止 ShowWindow 后所有同步 DWM 写入，
    // 直到 DEFERRED_BACKDROP_TIMER 触发（约 200ms 后）。
    // 这包括 FinalizeStartupRevealSettlement → ApplyResolvedChrome 的写入。
    if (!isMinimized_ && showCmd != SW_SHOWMINIMIZED) {
        deferredBackdropPending_ = true;
    }

    // [首帧即内容] Reveal 前 DWM 管线重置。
    // DWM 对隐藏窗口设置的 SYSTEMBACKDROP_TYPE 不会初始化合成管线，
    // ShowWindow 后重设相同值被视为 no-op。必须先 reset 为 NONE，
    // 再写回正确值，迫使 DWM 看到真正的状态变更。
    if (!isMinimized_ && showCmd != SW_SHOWMINIMIZED) {
        // 1. DWM SYSTEMBACKDROP_TYPE reset ? NONE
        int resetType = DWMSBT_NONE_V;
        S_DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE_V,
            &resetType, sizeof(resetType));

        // 2. 重新扩展帧到客户区
        MARGINS margins = { -1, -1, -1, -1 };
        S_DwmExtendFrameIntoClientArea(hwnd_, &margins);

        // 3. 仅重写 Mica/Acrylic backdrop 属性，不调用 ApplyResolvedChrome/RefreshNativeFrame。
        // 根因：ApplyResolvedChrome 在隐藏窗口上触发 ApplyFramelessState 的
        // SWP_FRAMECHANGED + RedrawWindow(RDW_FRAME)，再叠加 RefreshNativeFrame 的
        // 第二次 SWP_FRAMECHANGED + RedrawWindow(RDW_ERASE|RDW_ALLCHILDREN)，
        // 造成 DWM 帧合成管线不一致 → ShowWindow 后三大键 overlay 复现。
        // 完整 Chrome 层在 post-show FinalizeStartupRevealSettlement 中应用。
        EnableMicaEffect();
        TraceWindowPhase("PreShowDwmReset.micaRestore", nullptr, nullptr,
            nullptr, nullptr,
            canRefreshSurface ? "webview-ready=true" : "webview-ready=false");
    }

    // BackgroundService 协同：startupVisibilityHint_=false 时强制 SW_HIDE，
    // 让窗口从 cold-start reveal 起全程不可见，避免 SW_SHOWMINNOACTIVE+hideTimer
    // hack 与 reveal 流程争抢 ShowWindow 调用导致物理状态闪现。
    // chrome / backdrop 等 DWM 状态已在前面 ResolveBackdropPolicy / EnableMicaEffect
    // 中设置好，下次用户主动 Show 窗口时会立刻应用，不会丢失。
    const int finalShowCmd = startupVisibilityHint_ ? showCmd : SW_HIDE;
    std::string showExtra = std::string("showCmd=") + std::to_string(finalShowCmd) +
        " reason=" + revealReason +
        " visibilityHint=" + (startupVisibilityHint_ ? "visible" : "hidden");
    TraceWindowPhase("ShowWindow.before", nullptr, nullptr, nullptr, nullptr, showExtra.c_str());

    ShowWindow(hwnd_, finalShowCmd);

    // [Critical] ShowWindow 后立即触发 WM_NCCALCSIZE 以设置零 NC 面积。
    // Create()/OnCreate() 中的 SWP_FRAMECHANGED 在窗口隐藏 ~1.5s 后已失效，
    // DWM 在首次 ShowWindow 时重新初始化帧合成。此处仅用 SWP_FRAMECHANGED
    // 触发 WM_NCCALCSIZE（返回 0），即时消除 NC 面积。
    // 配合 startupShowWindowGuard_ 拦截 ShowWindow 内部的 WM_NCPAINT，
    // 双重保证三大键 overlay 不会在任何窗口状态（普通/最大化）下闪现。
    // 置 framelessDwmApplied_=true 短路后续 ApplyFramelessState 的
    // RedrawWindow(RDW_FRAME)——该组合操作在 cold-start 路径上会令 DWM
    // 重新评估帧装饰，复现三大键（a177ad2 证实）。
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    framelessDwmApplied_ = true;

    LogSurfaceDiagnostics("ShowWindow.after");
    CaptureRuntimeDomProbe("ShowWindow.after");
    TraceWindowPhase("ShowWindow.after", nullptr, nullptr, nullptr, nullptr, showExtra.c_str());

    // 不在 ShowWindow 同步调用中写 DWM backdrop。
    // DWM 的 backdrop 合成管线只在窗口实际呈现后才建立。
    // 在 ShowWindow 的同一消息处理中写入 SYSTEMBACKDROP_TYPE，DWM 尚未完成
    // 首次合成，写入被 DWM 视为对未呈现窗口的操作，不会初始化 backdrop 层。
    // 改为 PostMessage 延迟到下一消息循环迭代，让 DWM 有时间完成首次合成。

    UpdateWindow(hwnd_);

    bool postShowSurfaceFirstPresent = false;
    bool postShowNativeChromeReapplied = false;

    // post-show first-present surface commit.
    // pre-show 已将 WebView controller 置为 hidden + zero bounds。
    // 此处的 SetVisible(true) + Resize(finalBounds) 是 DComp surface 的
    // 第一笔真实 transition，确保 DWM 合成管线被激活。
    // 实测已证明：DwmFlush alone 不是充分条件。
    // 实测已证明：reset->reapply alone 也不是充分条件。
    // 真正缺少的是 post-show 的 first-present surface delta。
    if (canRefreshSurface && !isMinimized_ && startupSurfaceCommitPending_) {
        RECT targetBounds{};
        if (hasPendingStartupSurfaceBounds_) {
            targetBounds = pendingStartupSurfaceBounds_;
        } else {
            GetClientRect(hwnd_, &targetBounds);
        }

        const std::string pendingBoundsText = hasPendingStartupSurfaceBounds_
            ? S_FormatNativeRect(pendingStartupSurfaceBounds_)
            : "N/A";

        {
            RECT preBounds{};
            bool hasPre = false;
            if (webView_->GetController()) {
                hasPre = SUCCEEDED(webView_->GetController()->get_Bounds(&preBounds));
            }
            std::ostringstream surfaceStream;
            surfaceStream << "[FirstPresentSurface] t=" << WindowChromeTrace::RelativeMs() << "ms"
                          << " phase=before"
                          << " owner=TryCommitStartupReveal"
                          << " startupSurfaceAuthorityPending=" << WindowChromeTrace::BoolText(startupSurfaceAuthorityPending_)
                          << " startupSurfaceCommitPending=" << WindowChromeTrace::BoolText(startupSurfaceCommitPending_)
                          << " hasPendingBounds=" << WindowChromeTrace::BoolText(hasPendingStartupSurfaceBounds_)
                          << " pendingBounds=" << pendingBoundsText.c_str()
                          << " preBounds=" << (hasPre
                              ? (std::string("(") + std::to_string(preBounds.left) + ","
                                 + std::to_string(preBounds.top) + ","
                                 + std::to_string(preBounds.right) + ","
                                 + std::to_string(preBounds.bottom) + ")").c_str()
                              : "N/A")
                          << " targetBounds=(" << targetBounds.left << "," << targetBounds.top
                          << "," << targetBounds.right << "," << targetBounds.bottom << ")"
                          << " showCmd=" << showCmd;
            S_EmitEvidenceLine(surfaceStream.str());
        }

        webView_->SetVisible(true);
        webView_->Resize(targetBounds);
        postShowSurfaceFirstPresent = true;
        startupSurfaceFirstPresentCommitted_ = true;

        const RECT committedBounds = targetBounds;
        const RECT deferredBounds = pendingStartupSurfaceBounds_;
        const bool hasDeferredSync = hasPendingStartupSurfaceBounds_ &&
            (deferredBounds.left != committedBounds.left ||
             deferredBounds.top != committedBounds.top ||
             deferredBounds.right != committedBounds.right ||
             deferredBounds.bottom != committedBounds.bottom);

        startupSurfaceCommitPending_ = false;
        startupSurfaceAuthorityPending_ = false;
        hasPendingStartupSurfaceBounds_ = false;

        if (hasDeferredSync) {
            const std::string deferredBoundsText = S_FormatNativeRect(deferredBounds);
            webView_->Resize(deferredBounds);

            std::ostringstream stream;
            stream << "[StartupSurfaceAuthority]"
                   << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                   << " owner=TryCommitStartupReveal"
                   << " action=post-commit-sync"
                   << " bounds=" << deferredBoundsText.c_str();
            S_EmitEvidenceLine(stream.str());
            TraceWindowPhase("PostShowFirstPresent.post-commit-sync", nullptr, nullptr,
                nullptr, nullptr, deferredBoundsText.c_str());
        }

        {
            std::ostringstream surfaceStream;
            surfaceStream << "[FirstPresentSurface] t=" << WindowChromeTrace::RelativeMs() << "ms"
                          << " phase=after"
                          << " owner=TryCommitStartupReveal"
                          << " postShowSurfaceFirstPresent=true"
                          << " showCmd=" << showCmd;
            S_EmitEvidenceLine(surfaceStream.str());
        }
        TraceWindowPhase("PostShowFirstPresent.done");

        LOG("MainWindow: First-present surface commit after startup reveal, showCmd=", showCmd,
            " reason=", revealReason.c_str());
    }

    if (startupSurfaceAuthorityPending_) {
        std::ostringstream stream;
        stream << "[StartupSurfaceAuthority]"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " owner=TryCommitStartupReveal"
               << " action=release-without-commit"
               << " startupSurfaceCommitPending=" << WindowChromeTrace::BoolText(startupSurfaceCommitPending_)
               << " canRefreshSurface=" << WindowChromeTrace::BoolText(canRefreshSurface)
               << " isMinimized=" << WindowChromeTrace::BoolText(isMinimized_);
        S_EmitEvidenceLine(stream.str());
        startupSurfaceAuthorityPending_ = false;
        startupSurfaceCommitPending_ = false;
        hasPendingStartupSurfaceBounds_ = false;
    }

    if (!isMinimized_) {
        FinalizeStartupRevealSettlement();
    }

    // startup-only deferred surface converge: first-present + settlement 已完成，
    // 投递到下一消息周期做一次 bounded nudge，模拟"用户手动 resize"的 DComp 收敛。
    // 门控条件：cold startup reveal + first-present 已提交 + 非最小化 + 仅一次
    if (startupSurfaceFirstPresentCommitted_ && !isMinimized_ && hwnd_ && IsWindow(hwnd_)) {
        startupDeferredSurfaceConvergePending_ = true;
        PostMessage(hwnd_, WM_DEFERRED_SURFACE_CONVERGE, 0, 0);
        LOG("MainWindow: Scheduled startup-only deferred surface converge");
    }

    // 延迟首次 DWM backdrop 写入。使用 SetTimer 而非 PostMessage，
    // 确保 DWM 有足够时间完成窗口首次合成（PostMessage 仅延迟 ~16ms，
    // 常在同一 DWM 合成帧内执行）。200ms 约 12 帧(60Hz)，给 DWM 充分初始化时间。
    if (!isMinimized_ && hwnd_ && IsWindow(hwnd_)) {
        needsAuthoritativeChromeReapply_ = true;
        SetTimer(hwnd_, DEFERRED_BACKDROP_TIMER_ID, DEFERRED_BACKDROP_DELAY_MS, nullptr);
        LOG("MainWindow: Scheduled deferred backdrop apply (",
            DEFERRED_BACKDROP_DELAY_MS, "ms delay)");
    } else {
        needsAuthoritativeChromeReapply_ = false;
    }
    if (isMinimized_) {
        startupPresentationCoordinator_.MarkRevealSettled();
        SyncStartupRevealProjection();
    }

    LOG("MainWindow: Startup reveal committed, reason=",
        revealReason.c_str(), " showCmd=", showCmd,
        " postShowSurfaceFirstPresent=", postShowSurfaceFirstPresent ? "true" : "false",
        " postShowNativeChromeReapplied=", postShowNativeChromeReapplied ? "true" : "false",
        " startupSurfaceFirstPresentCommitted=", startupSurfaceFirstPresentCommitted_ ? "true" : "false");

    std::string exitExtra = std::string("showCmd=") + std::to_string(showCmd) +
        " reason=" + revealReason +
        " postShowSurfaceFirstPresent=" + WindowChromeTrace::BoolText(postShowSurfaceFirstPresent) +
        " postShowNativeChromeReapplied=" + WindowChromeTrace::BoolText(postShowNativeChromeReapplied) +
        " startupSurfaceFirstPresentCommitted=" + WindowChromeTrace::BoolText(startupSurfaceFirstPresentCommitted_);
    TraceWindowPhase("TryCommitStartupReveal.exit", nullptr, nullptr, nullptr, nullptr, exitExtra.c_str());
}

void MainWindow::ArmStartupFallbackTimer() {
    if (startupRevealCommitted_) return;
    SetTimer(hwnd_, STARTUP_REVEAL_TIMER_ID, STARTUP_REVEAL_TIMEOUT_MS, nullptr);
    LOG("MainWindow: Startup fallback timer armed (", STARTUP_REVEAL_TIMEOUT_MS, "ms)");
}


// ============================================
// 客户区扩展到标题栏 - 核心实现
// ============================================

LRESULT MainWindow::OnNcCalcSize(WPARAM wParam, LPARAM lParam) {
    // 使用标准窗口行为，保留系统标题栏和边框
    return DefWindowProcW(hwnd_, WM_NCCALCSIZE, wParam, lParam);
}

LRESULT MainWindow::OnNcHitTest(int screenX, int screenY) {
    // 使用标准窗口行为，让系统处理标题栏和边框
    return DefWindowProcW(hwnd_, WM_NCHITTEST, 0, MAKELPARAM(screenX, screenY));
}

void MainWindow::SetExpectedTopmost(bool expected) {
    if (expectedTopmost_ == expected) return;
    expectedTopmost_ = expected;
    std::ostringstream stream;
    stream << "[ActivationEvidence] source=MainWindow::SetExpectedTopmost"
           << " t=" << WindowChromeTrace::RelativeMs() << "ms"
           << " expectedTopmost=" << WindowChromeTrace::BoolText(expected);
    S_EmitEvidenceLine(stream.str());
}

void MainWindow::OnActivate(WPARAM wParam) {
    const bool active = (LOWORD(wParam) != WA_INACTIVE);

    // 第三波防御：用户点击 desktopLyrics 等 overlay 后，WebView2 在 popup 与主窗口两个
    // controller 间回授焦点，会让主窗口在点击后短时内被无意激活并覆盖外部应用。
    // 若主窗口在 overlay 交互窗口期（300ms）内被激活，则视为误激活：把主窗口压回外部
    // 窗口之后，并尝试把前台还给外部应用（理想），否则引到不可感知的 sink（兜底）。
    // 关键改进：overlayRedirect 只执行一次 SetForegroundWindow 操作，
    // 后续在同一窗口期内的激活消息直接静默跳过（不做 backdrop 切换、不调 SetForegroundWindow、
    // 不发广播），避免 SetForegroundWindow → WM_ACTIVATEAPP → overlayRedirect → SetForegroundWindow
    // 的震荡死循环。
    if (active && WindowManager::GetInstance().IsOverlayInteractionRecent(300)) {
        // -- 后续抑制：如果 redirect 已在执行中，静默跳过 --
        // 阻止 SetForegroundWindow 触发的 WM_ACTIVATEAPP 再次进入 redirect，消除震荡。
        if (overlayRedirectInProgress_) {
            // 仅保持 inactive 外观，不做任何 SetForegroundWindow 操作
            ApplyWindowActivationState(false, "WM_ACTIVATE.overlayRedirect.suppressed");
            return;
        }

        // -- 第一次 redirect：标记 in-progress，执行一次性撤销 --
        overlayRedirectInProgress_ = true;
        lastOverlayRedirectTick_ = GetTickCount64();

        HWND ext = WindowManager::GetInstance().GetOverlayInteractionForeground();
        HWND sink = WindowManager::GetInstance().GetActivationSinkHwnd();
        const bool extValid = ext && IsWindow(ext) && ext != hwnd_;
        // 跨 band 的相对插入必然改写主窗口 WS_EX_TOPMOST，一律跳过：
        // 升段（参照 topmost、主窗口非 topmost）= 隐式授予置顶 → 意外覆盖全桌面；
        // 降段（参照非 topmost、主窗口 topmost）= 用户显式 alwaysOnTop 被悄悄剥除。
        // 同 band 内移动不改置顶状态，安全执行。升段场景下主窗口位于非 topmost
        // 段顶部，天然已排在 topmost 参照之后，跳过即正确；其余残差由
        // WM_WINDOWPOSCHANGED 的期望置顶守护兜底。
        const bool extTopmost = extValid &&
            (GetWindowLongW(ext, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
        const bool selfTopmost =
            (GetWindowLongW(hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;

        // 把主窗口 z-order 压回外部窗口之后，避免即便仍短暂前台也覆盖外部应用。
        if (extValid && extTopmost == selfTopmost) {
            SetWindowPos(hwnd_, ext, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        // 优先把前台还给外部应用；若未生效（主窗口仍前台）再引到 sink（同进程，可靠）。
        if (extValid) SetForegroundWindow(ext);
        if (GetForegroundWindow() == hwnd_ && sink && IsWindow(sink) && sink != hwnd_) {
            SetForegroundWindow(sink);
        }

        {
            std::ostringstream stream;
            stream << "[ActivationEvidence] source=MainWindow::OnActivate.overlayRedirect"
                   << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                   << " ext=0x" << pfc::format_hex((size_t)ext)
                   << " extTopmost=" << WindowChromeTrace::BoolText(extTopmost)
                   << " selfTopmost=" << WindowChromeTrace::BoolText(selfTopmost)
                   << " sink=0x" << pfc::format_hex((size_t)sink)
                   << " mainHwnd=0x" << pfc::format_hex((size_t)hwnd_)
                   << " fgAfter=0x" << pfc::format_hex((size_t)::GetForegroundWindow());
            S_EmitEvidenceLine(stream.str());
        }
        // 不应用 active 外观：保持主窗口"未激活"语义。
        ApplyWindowActivationState(false, "WM_ACTIVATE.overlayRedirect");
        return;
    }

    // -- overlay 交互窗口期结束：清除 redirect 标志 --
    // 也用超时兜底（500ms），防止标志永远不被清除。
    if (overlayRedirectInProgress_) {
        const unsigned long long now = GetTickCount64();
        if (!WindowManager::GetInstance().IsOverlayInteractionRecent(500) ||
            (now - lastOverlayRedirectTick_ > 500)) {
            overlayRedirectInProgress_ = false;
        }
    }

    const bool shouldApplyBackdrop = !startupRevealPending_ && !startupRevealSettling_;
    std::string extra = std::string("wParam=") + std::to_string((unsigned long long)wParam) +
        " nextActive=" + WindowChromeTrace::BoolText(active) +
        " applyBackdrop=" + WindowChromeTrace::BoolText(shouldApplyBackdrop) +
        " startupRevealSettling=" + WindowChromeTrace::BoolText(startupRevealSettling_);
    TraceWindowPhase("WM_ACTIVATE", WindowChromeTrace::BoolText(active), nullptr,
        GetTraceEffect(active), lastBackdropEffect_.empty() ? "-" : lastBackdropEffect_.c_str(),
        extra.c_str());

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=WM_ACTIVATE"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " wParam=" << (unsigned long long)wParam
               << " nextActive=" << WindowChromeTrace::BoolText(active)
               << " shouldApplyBackdrop=" << WindowChromeTrace::BoolText(shouldApplyBackdrop)
               << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_);
        S_EmitEvidenceLine(stream.str());
    }

    ApplyWindowActivationState(active, "WM_ACTIVATE");
}

void MainWindow::OnActivateApp(bool active) {
    // -- popup 误激活抑制（与 OnActivate 中相同的逻辑）--
    // WM_ACTIVATEAPP 是线程级消息，Chromium 激活 popup 时同线程的 main 也收到 active=true。
    // 所有 popup（miniplayer、desktopLyrics 等）在 Visual Hosting 模式下点击可交互元素时，
    // Chromium 都会调 SetForegroundWindow 导致主窗口被无意激活。
    // 必须在此拦截，否则 WM_ACTIVATEAPP(active=true) 会先于 WM_ACTIVATE 把外观切换到 active，
    // 造成短暂的视觉闪烁（active→inactive→active→suppressed 的震颤）。
    if (active && WindowManager::GetInstance().IsOverlayInteractionRecent(300)) {
        if (overlayRedirectInProgress_) {
            // 抑制后续激活消息，不做任何外观切换
            ApplyWindowActivationState(false, "WM_ACTIVATEAPP.overlayRedirect.suppressed");
            return;
        }
        // WM_ACTIVATEAPP may arrive before WM_ACTIVATE. Do not mark the redirect as
        // in-progress here, otherwise the following WM_ACTIVATE would be suppressed
        // before it can run the one real z-order / foreground undo path.
        ApplyWindowActivationState(false, "WM_ACTIVATEAPP.overlayRedirect.pending");
        return;
    }

    // -- overlay 交互窗口期结束：清除 redirect 标志 --
    if (overlayRedirectInProgress_) {
        const unsigned long long now = GetTickCount64();
        if (!WindowManager::GetInstance().IsOverlayInteractionRecent(500) ||
            (now - lastOverlayRedirectTick_ > 500)) {
            overlayRedirectInProgress_ = false;
        }
    }

    const bool shouldApplyBackdrop = !startupRevealPending_ && !startupRevealSettling_;
    std::string extra = std::string("active=") + WindowChromeTrace::BoolText(active) +
        " applyBackdrop=" + WindowChromeTrace::BoolText(shouldApplyBackdrop) +
        " startupRevealSettling=" + WindowChromeTrace::BoolText(startupRevealSettling_);
    TraceWindowPhase("WM_ACTIVATEAPP", WindowChromeTrace::BoolText(active), nullptr,
        GetTraceEffect(active), lastBackdropEffect_.empty() ? "-" : lastBackdropEffect_.c_str(),
        extra.c_str());

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=WM_ACTIVATEAPP"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " nextActive=" << WindowChromeTrace::BoolText(active)
               << " shouldApplyBackdrop=" << WindowChromeTrace::BoolText(shouldApplyBackdrop)
               << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_);
        S_EmitEvidenceLine(stream.str());
    }

    ApplyWindowActivationState(active, "WM_ACTIVATEAPP");
}

void MainWindow::ApplyWindowActivationState(bool active, const char* reason) {
    const bool previousActive = isActive_;
    isActive_ = active;
    const bool activationChanged = previousActive != isActive_;
    const bool shouldApplyBackdrop = !startupRevealPending_ && !startupRevealSettling_;
    bool attemptedBackdropApply = false;
    bool usedAuthoritativeChromeReapply = false;
    bool backdropApplyResult = false;

    if (WindowChromeTrace::AuxiliaryTraceEnabled()) {
        LOG("MainWindow: Activation state update, reason=", reason,
            " previousActive=", previousActive ? "true" : "false",
            " active=", isActive_ ? "true" : "false",
            " applyBackdrop=", shouldApplyBackdrop ? "true" : "false");
    }

    // 启动 reveal 期间，直到 post-show native chrome settle 完成前都跳过 backdrop apply。
    // ShowWindow 内部的 WM_ACTIVATE 只更新 active 状态，真正的首个 authoritative apply
    // 由 ReapplyNativeChromeAfterStartupReveal 在 settle 点统一完成，避免 DWM coalesce 抢跑写入。
    if (shouldApplyBackdrop && activationChanged) {
        attemptedBackdropApply = true;
        backdropApplyResult = ApplyBackdropPolicyForActivation(isActive_, false);
    }
    
    // 通知 WebView 激活状态变化（可用于更新标题栏样式）
    bool emittedWindowActivated = false;
    if (previousActive != isActive_ && webView_ && webView_->IsReady()) {
        json event = {
            {"type", "event"},
            {"event", "window:activated"},
            {"data", {{"active", isActive_}}}
        };
        pfc::stringcvt::string_wide_from_utf8 wideStr(event.dump().c_str());
        webView_->PostMessage(std::wstring(wideStr.get_ptr()));
        emittedWindowActivated = true;
    }

    const bool emittedWindowStateChanged = BroadcastWindowStateChangedIfNeeded();

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=ApplyWindowActivationState"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " reason=" << (reason && reason[0] ? reason : "-")
               << " previousActive=" << WindowChromeTrace::BoolText(previousActive)
               << " active=" << WindowChromeTrace::BoolText(isActive_)
               << " shouldApplyBackdrop=" << WindowChromeTrace::BoolText(shouldApplyBackdrop)
               << " attemptedBackdropApply=" << WindowChromeTrace::BoolText(attemptedBackdropApply)
               << " backdropApplyResult=" << WindowChromeTrace::BoolText(backdropApplyResult)
               << " usedAuthoritativeChromeReapply=" << WindowChromeTrace::BoolText(usedAuthoritativeChromeReapply)
               << " emittedWindowActivated=" << WindowChromeTrace::BoolText(emittedWindowActivated)
               << " emittedWindowStateChanged=" << WindowChromeTrace::BoolText(emittedWindowStateChanged)
               << " webViewReady=" << WindowChromeTrace::BoolText(webView_ && webView_->IsReady())
               << " statePayload.active=" << WindowChromeTrace::BoolText(isActive_)
               << " statePayload.isActive=" << WindowChromeTrace::BoolText(isActive_);
        S_EmitEvidenceLine(stream.str());
    }
}

void MainWindow::OnDpiChanged(WPARAM wParam, LPARAM lParam) {
    // 更新 DPI 相关尺寸
    UpdateDpiDependentSizes();
    
    // 应用新的窗口大小
    RECT* suggested = reinterpret_cast<RECT*>(lParam);
    SetWindowPos(hwnd_, nullptr, 
        suggested->left, suggested->top,
        suggested->right - suggested->left,
        suggested->bottom - suggested->top,
        SWP_NOZORDER | SWP_NOACTIVATE);
    
    // 通知 WebView DPI 变化
    if (webView_ && webView_->IsReady()) {
        int dpi = HIWORD(wParam);
        double dpiScale = dpi / 96.0;
        
        // 1. 发送 DPI 变化事件
        json event = {
            {"type", "event"},
            {"event", "window:dpiChanged"},
            {"data", {
                {"dpi", dpi},
                {"dpiScale", dpiScale},
                {"titlebarHeight", titlebarHeight_},
                {"captionButtonWidth", captionButtonWidth_},
                {"captionButtonsWidth", captionButtonWidth_ * 3}
            }}
        };
        pfc::stringcvt::string_wide_from_utf8 wideStr(event.dump().c_str());
        webView_->PostMessage(std::wstring(wideStr.get_ptr()));
        
        // 2. 注入 CSS 变量更新脚本
        std::wstring cssUpdateScript = L"(function() {"
            L"const root = document.documentElement;"
            L"root.style.setProperty('--dpi', '" + std::to_wstring(dpi) + L"');"
            L"root.style.setProperty('--dpi-scale', '" + std::to_wstring(dpiScale) + L"');"
            L"root.style.setProperty('--titlebar-height', '" + std::to_wstring(titlebarHeight_) + L"px');"
            L"root.style.setProperty('--caption-button-width', '" + std::to_wstring(captionButtonWidth_) + L"px');"
            L"console.log('[DPI] CSS variables updated: dpi=' + " + std::to_wstring(dpi) + L");"
            L"})();";
        webView_->ExecuteScript(cssUpdateScript, nullptr);
        
        // 3. 刷新 WebView 渲染以防止模糊
        if (!isMinimized_) {
            RECT clientRect;
            GetClientRect(hwnd_, &clientRect);
            webView_->Resize(clientRect);
        }
        
        FB2K_console_print("[WebView2 UI] DPI changed to ", dpi, " (scale: ", dpiScale, ")");
    }
}


// ============================================
// 标题栏区域管理
// ============================================

void MainWindow::SetTitlebarHeight(int height) {
    if (height > 0 && height != titlebarHeight_) {
        titlebarHeight_ = height;
        ExtendFrameIntoClientArea();
        
        // 触发重新布局
        RECT rect;
        GetClientRect(hwnd_, &rect);
        OnSize(rect.right, rect.bottom);
    }
}

void MainWindow::SetDragRegions(const std::vector<TitlebarDragRegion>& regions) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    dragRegions_ = regions;
}

void MainWindow::ClearDragRegions() {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    dragRegions_.clear();
}

void MainWindow::SetNoDragRegions(const std::vector<TitlebarDragRegion>& regions) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    noDragRegions_ = regions;
}

void MainWindow::AddNoDragRegion(const TitlebarDragRegion& region) {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    noDragRegions_.push_back(region);
}

void MainWindow::ClearNoDragRegions() {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    noDragRegions_.clear();
}

int MainWindow::GetCaptionButtonWidth() const {
    return captionButtonWidth_;
}

int MainWindow::GetCaptionButtonsWidth() const {
    return captionButtonWidth_ * 3;  // 最小化、最大化、关闭
}

RECT MainWindow::GetCaptionButtonsRect() const {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    
    int buttonsWidth = captionButtonWidth_ * 3;
    
    return RECT{
        rc.right - buttonsWidth,  // left
        0,                        // top
        rc.right,                 // right
        titlebarHeight_           // bottom
    };
}

bool MainWindow::IsPointInDragRegion(int clientX, int clientY) const {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    for (const auto& region : dragRegions_) {
        if (clientX >= region.x && clientX < region.x + region.width &&
            clientY >= region.y && clientY < region.y + region.height) {
            return true;
        }
    }
    return false;
}

bool MainWindow::IsPointInNoDragRegion(int clientX, int clientY) const {
    std::lock_guard<std::mutex> lock(regionsMutex_);
    for (const auto& region : noDragRegions_) {
        if (clientX >= region.x && clientX < region.x + region.width &&
            clientY >= region.y && clientY < region.y + region.height) {
            return true;
        }
    }
    return false;
}

void MainWindow::ShowSystemMenu(int screenX, int screenY) {
    // 获取系统菜单
    HMENU sysMenu = GetSystemMenu(hwnd_, FALSE);
    if (!sysMenu) return;
    
    // 根据窗口状态更新菜单项
    UINT enableRestore = isMaximized_ ? MF_ENABLED : MF_GRAYED;
    UINT enableMaximize = isMaximized_ ? MF_GRAYED : MF_ENABLED;
    UINT enableMove = isMaximized_ ? MF_GRAYED : MF_ENABLED;
    UINT enableSize = isMaximized_ ? MF_GRAYED : MF_ENABLED;
    
    EnableMenuItem(sysMenu, SC_RESTORE, MF_BYCOMMAND | enableRestore);
    EnableMenuItem(sysMenu, SC_MAXIMIZE, MF_BYCOMMAND | enableMaximize);
    EnableMenuItem(sysMenu, SC_MOVE, MF_BYCOMMAND | enableMove);
    EnableMenuItem(sysMenu, SC_SIZE, MF_BYCOMMAND | enableSize);
    
    // 设置默认项（还原或最大化）
    SetMenuDefaultItem(sysMenu, isMaximized_ ? SC_RESTORE : SC_MAXIMIZE, FALSE);
    
    // 显示菜单并跟踪选择
    UINT cmd = TrackPopupMenu(sysMenu,
        TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
        screenX, screenY, 0, hwnd_, nullptr);
    
    // 执行选中的命令
    if (cmd != 0) {
        PostMessageW(hwnd_, WM_SYSCOMMAND, cmd, 0);
    }
}

// ============================================
// 窗口尺寸限制方法
// ============================================

void MainWindow::SetMinSize(int width, int height) {
    minWidth_ = width > 0 ? width : 1;
    minHeight_ = height > 0 ? height : 1;
}

void MainWindow::SetMaxSize(int width, int height) {
    maxWidth_ = width >= 0 ? width : 0;
    maxHeight_ = height >= 0 ? height : 0;
}


// ============================================
// 窗口位置记忆功能
// ============================================

// Forward declaration for window_config namespace (defined in main.cpp)
namespace window_config {
    void GetWindowPosition(int& x, int& y, int& width, int& height, bool& maximized);
    void SetWindowPosition(int x, int y, int width, int height, bool maximized);
    bool HasSavedPosition();
}

void MainWindow::SaveWindowPosition() {
    // 检查是否启用了记住窗口位置
    if (!webview_prefs::GetRememberWindowPosition()) {
        LOG("SaveWindowPosition: Disabled by user setting");
        return;
    }
    
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return;
    }
    
    // 获取窗口位置信息（包含正常状态下的位置，即使窗口最大化）
    WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
    if (!GetWindowPlacement(hwnd_, &wp)) {
        LOG("SaveWindowPosition: GetWindowPlacement failed");
        return;
    }
    
    // 使用 rcNormalPosition 获取非最大化状态的窗口位置
    // 这确保恢复时使用正确的正常尺寸
    int x = wp.rcNormalPosition.left;
    int y = wp.rcNormalPosition.top;
    int width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    int height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
    bool maximized = (wp.showCmd == SW_SHOWMAXIMIZED) || isMaximized_;
    
    window_config::SetWindowPosition(x, y, width, height, maximized);
    
    LOG("SaveWindowPosition: x=", x, " y=", y, " w=", width, " h=", height, " max=", maximized);
}

void MainWindow::RestoreWindowPosition(int& x, int& y, int& width, int& height, int& showCmd) {
    // 默认值
    width = 1280;
    height = 720;
    
    // 先居中
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    x = (screenWidth - width) / 2;
    y = (screenHeight - height) / 2;
    showCmd = SW_SHOW;
    
    // 检查是否启用了记住窗口位置
    if (!webview_prefs::GetRememberWindowPosition()) {
        LOG("RestoreWindowPosition: Disabled by user setting, using defaults");
        return;
    }
    
    // 检查是否有保存的位置
    if (!window_config::HasSavedPosition()) {
        LOG("RestoreWindowPosition: no saved position, using defaults");
        return;
    }
    
    bool maximized = false;
    window_config::GetWindowPosition(x, y, width, height, maximized);
    
    // 验证窗口尺寸是否合理
    if (width < minWidth_) width = minWidth_;
    if (height < minHeight_) height = minHeight_;
    if (maxWidth_ > 0 && width > maxWidth_) width = maxWidth_;
    if (maxHeight_ > 0 && height > maxHeight_) height = maxHeight_;
    
    // 验证窗口位置是否在任一显示器内
    // 使用 MonitorFromRect 检查窗口是否可见
    RECT windowRect = { x, y, x + width, y + height };
    HMONITOR hMonitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONULL);
    
    if (!hMonitor) {
        // 窗口不在任何显示器上，重新居中到主显示器
        LOG("RestoreWindowPosition: saved position off-screen, re-centering");
        x = (screenWidth - width) / 2;
        y = (screenHeight - height) / 2;
    }
    
    // 设置显示命令
    showCmd = maximized ? SW_SHOWMAXIMIZED : SW_SHOW;
    
    LOG("RestoreWindowPosition: x=", x, " y=", y, " w=", width, " h=", height, " showCmd=", showCmd);
}

// ============================================
// WindowShellBase 实现
// MainWindow 作为 shell adapter 接入统一抽象
// ============================================

std::string MainWindow::GetShellWindowId() const {
    return GetWindowId();
}

WindowKind MainWindow::GetWindowKind() const {
    return WindowKind::Main;
}

HWND MainWindow::GetShellHwnd() const {
    return GetHwnd();
}

WindowShellCapabilities MainWindow::GetCapabilities() const {
    WindowShellCapabilities caps;
    caps.supportsFullscreen = true;
    caps.supportsBackdropPolicy = true;
    caps.supportsCornerPreference = true;
    caps.supportsMicaAlt = true;
    caps.supportsOwnerPolicy = false;
    caps.supportsNoActivate = false;
    caps.participatesInAppBootstrap = true;
    caps.supportsBeforeClose = false;
    caps.windowKind = WindowKind::Main;
    return caps;
}

WindowShellSnapshot MainWindow::GetShellSnapshot() const {
    WindowShellSnapshot snap;
    snap.windowId = GetWindowId();
    snap.kind = WindowKind::Main;
    snap.capabilities = GetCapabilities();

    snap.lifecycle.created = (hwnd_ != nullptr);
    snap.lifecycle.visible = hwnd_ && IsWindowVisible(hwnd_);
    snap.lifecycle.active = isActive_;
    snap.lifecycle.minimized = isMinimized_;
    snap.lifecycle.maximized = isMaximized_;
    snap.lifecycle.fullscreen = isFullscreen_;
    snap.lifecycle.pendingDestroy = false;

    const auto startupPresentation = startupPresentationCoordinator_.GetSnapshot();
    snap.startupPresentation.phase = startupPresentation.phase;
    snap.startupPresentation.navigationCompleted = startupPresentation.navigationCompleted;
    snap.startupPresentation.windowReadySignaled = startupPresentation.windowReadySignaled;
    snap.startupPresentation.visualReadySignaled = startupPresentation.visualReadySignaled;
    snap.startupPresentation.revealPending = startupPresentation.revealPending;
    snap.startupPresentation.revealCommitted = startupPresentation.revealCommitted;
    snap.startupPresentation.revealSettling = startupPresentation.revealSettling;
    snap.startupPresentation.fallbackArmed = startupPresentation.fallbackArmed;
    snap.startupPresentation.fallbackUsed = startupPresentation.fallbackUsed;

    snap.chrome.resolved = resolvedChromeState_;
    return snap;
}

bool MainWindow::PatchBackdropPolicy(const json& policyPatch, std::string& error) {
    return SetBackdropPolicy(policyPatch, error);
}

void MainWindow::PatchFrameless(bool frameless) {
    SetFrameless(frameless);
}

void MainWindow::RefreshChrome() {
    RefreshBackdropEffect();
}

bool MainWindow::PatchCompatibilityBackdrop(const std::optional<std::string>& effect,
                                            const std::optional<bool>& darkMode,
                                            bool clearBlur, bool forceRefresh) {
    return UpdateCompatibilityBackdropEffect(effect, darkMode, clearBlur, forceRefresh);
}

bool MainWindow::PatchCompatibilityBlur(bool enabled, bool forceRefresh) {
    return UpdateCompatibilityBlur(enabled, forceRefresh);
}

bool MainWindow::PatchCompatibilityDarkMode(bool enabled, bool forceRefresh) {
    return UpdateCompatibilityDarkMode(enabled, forceRefresh);
}

bool MainWindow::PatchCompatibilityTransparency(bool transparent, bool forceRefresh) {
    return UpdateCompatibilityTransparentBackground(transparent, forceRefresh);
}

bool MainWindow::IsFullscreen() const {
    return savedWindowInfo_.has_value();
}

void MainWindow::NotifyFullscreenChanged(bool isFullscreen) {
    isFullscreen_ = isFullscreen;
    RefreshBackdropEffect();
    // 全屏状态变化需广播给前端（window:stateChanged 携带 isFullscreen/fullscreen 字段）
    BroadcastWindowStateChangedIfNeeded(true);
}

void MainWindow::SetFullscreenFlag(bool isFullscreen) {
    isFullscreen_ = isFullscreen;
}

bool MainWindow::IsActive() const {
    return isActive_;
}
