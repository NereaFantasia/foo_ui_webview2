// ============================================
// BackgroundService.cpp - Background Mode Support Implementation
// ============================================

#include "pch.h"
#include "core/BackgroundService.h"
#include "window/MainWindow.h"
#include "core/UserInterface.h"
#include "core/PreferencesPage.h"
#include "core/WebViewContext.h"
#include "api/AudioApi.h"
#include "utils/I18n.h"

namespace {
    // Background window instance (only created when not using WebView2 UI as main UI)
    std::unique_ptr<MainWindow> g_backgroundWindow;
    HWND g_backgroundHwnd = nullptr;
    bool g_initialized = false;
}

// Forward declaration from main.cpp
namespace security_config {
    bool IsBackgroundModeEnabled();
}

// Forward declaration from main.cpp - DUI 后台窗口可见性持久化
namespace window_config {
    bool IsBackgroundWindowVisible();
    void SetBackgroundWindowVisible(bool visible);
}

namespace background_service {

bool IsBackgroundModeEnabled() {
    return security_config::IsBackgroundModeEnabled();
}

bool IsWebViewUIActive() {
    // Check if WebView2 UI is registered as the active user interface
    // by checking if WebViewUI singleton exists
    return WebViewUI::GetInstance() != nullptr;
}

// Internal initialization function
static void DoInitialize() {
    if (g_initialized) {
        return;
    }
    
    // Check if background mode is enabled
    if (!IsBackgroundModeEnabled()) {
        console::print("[WebView2 UI] Background mode disabled in settings");
        return;
    }
    
    // Check if WebView2 UI is already the active interface
    // If so, we don't need the background service
    if (IsWebViewUIActive()) {
        console::print("[WebView2 UI] WebView2 UI is active interface, background service not needed");
        return;
    }
    
    console::print("[WebView2 UI] Initializing background service...");
    
    try {
        // 启动可见性决策：偏好与持久化状态双条件合取
        //   - "Start with foobar2000" (startWithFoobar) 表达用户偏好：启动时允许显示后台窗口
        //   - cfg_background_window_visible (lastVisible) 记录上次关闭时菜单 Show/Hide 状态
        // 只有两者均为 true 才显示，否则窗口从启动到关闭全程不可见。
        const bool startWithFoobar = webview_prefs::GetStartWithFoobar();
        const bool lastVisible = window_config::IsBackgroundWindowVisible();
        const bool showOnStartup = startWithFoobar && lastVisible;

        // Create main window for background operation。
        // 在 Create() 之前提示 cold-start reveal 的最终可见性，让 reveal 流程
        // 自己决定 SW_SHOW 或 SW_HIDE，避免在外面叠加 SW_SHOWMINNOACTIVE/hideTimer
        // hack（旧实现会在 ~250ms 内被 cold-start reveal 的 SW_SHOW 覆盖再
        // 1500ms 后 SW_HIDE，物理窗口与菜单 IsWindowVisible 状态在该窗口内不一致）。
        g_backgroundWindow = std::make_unique<MainWindow>();
        g_backgroundWindow->SetStartupVisibility(showOnStartup);
        g_backgroundHwnd = g_backgroundWindow->Create(nullptr);

        if (!g_backgroundHwnd) {
            console::print("[WebView2 UI] ERROR: Failed to create background window");
            g_backgroundWindow.reset();
            return;
        }

        console::printf("[WebView2 UI] Background window created (startWithFoobar=%s, lastVisible=%s, showOnStartup=%s)",
            startWithFoobar ? "true" : "false",
            lastVisible ? "true" : "false",
            showOnStartup ? "true" : "false");

        g_initialized = true;
        console::print("[WebView2 UI] Background service initialized successfully");
        console::print("[WebView2 UI] APIs are available via JavaScript bridge");
        
    } catch (const std::exception& e) {
        console::printf("[WebView2 UI] ERROR: Background service init failed: %s", e.what());
        g_backgroundWindow.reset();
        g_backgroundHwnd = nullptr;
    } catch (...) {
        console::print("[WebView2 UI] ERROR: Unknown exception during background service init");
        g_backgroundWindow.reset();
        g_backgroundHwnd = nullptr;
    }
}

void Initialize() {
    if (g_initialized) {
        return;
    }
    
    // First attempt - might be too early if WebView2 UI is being initialized
    DoInitialize();
}

void Shutdown() {
    if (!g_initialized) {
        return;
    }
    
    console::print("[WebView2 UI] Shutting down background service...");
    
    // 前置通知（允许失败，不阻断关键清理）
    try {
        WebViewContext::GetInstance().BroadcastEvent("app:beforeQuit", json::object());
    } catch (const std::exception& e) {
        console::printf("[WebView2 UI] WARNING: beforeQuit broadcast failed: %s", e.what());
    } catch (...) {
        console::print("[WebView2 UI] WARNING: Unknown exception during beforeQuit broadcast");
    }
    
    try {
        ShutdownAudioVisualizationRuntime();
    } catch (const std::exception& e) {
        console::printf("[WebView2 UI] WARNING: Audio visualization shutdown failed: %s", e.what());
    } catch (...) {
        console::print("[WebView2 UI] WARNING: Unknown exception during audio visualization shutdown");
    }
    
    // 关键清理（窗口销毁 + 状态复位，必须执行）
    try {
        if (g_backgroundWindow) {
            g_backgroundWindow->Destroy();
            g_backgroundWindow.reset();
        }
    } catch (const std::exception& e) {
        console::printf("[WebView2 UI] ERROR: Background window destroy failed: %s", e.what());
        g_backgroundWindow.reset();
    } catch (...) {
        console::print("[WebView2 UI] ERROR: Unknown exception during background window destroy");
        g_backgroundWindow.reset();
    }
    
    g_backgroundHwnd = nullptr;
    g_initialized = false;
    
    console::print("[WebView2 UI] Background service shutdown complete");
}

MainWindow* GetBackgroundWindow() {
    return g_backgroundWindow.get();
}

void ShowWindow() {
    if (g_backgroundHwnd && IsWindow(g_backgroundHwnd)) {
        const bool wasHidden = !IsWindowVisible(g_backgroundHwnd);
        ::ShowWindow(g_backgroundHwnd, SW_SHOW);
        if (wasHidden && g_backgroundWindow) {
            g_backgroundWindow->RestoreSurfaceAfterHidden("background-service-show");
        }
        ::SetForegroundWindow(g_backgroundHwnd);
        // 持久化状态：下次启动时恢复“可见”
        window_config::SetBackgroundWindowVisible(true);
        console::print("[WebView2 UI] Background window shown");
    }
}

void HideWindow() {
    if (g_backgroundHwnd && IsWindow(g_backgroundHwnd)) {
        ::ShowWindow(g_backgroundHwnd, SW_HIDE);
        // 持久化状态：下次启动时保持“隐藏”
        window_config::SetBackgroundWindowVisible(false);
        console::print("[WebView2 UI] Background window hidden");
    }
}

void ToggleWindow() {
    if (g_backgroundHwnd && IsWindow(g_backgroundHwnd)) {
        if (::IsWindowVisible(g_backgroundHwnd)) {
            HideWindow();
        } else {
            ShowWindow();
        }
    }
}

bool HasBackgroundWindow() {
    return g_backgroundHwnd != nullptr && IsWindow(g_backgroundHwnd);
}

} // namespace background_service


// ============================================
// initquit service for background mode
// ============================================
class BackgroundServiceInitQuit : public initquit {
public:
    void on_init() override {
        // Delay initialization to ensure all services are ready
        // Use main thread callback with additional delay to ensure user_interface::init() has been called
        fb2k::inMainThread([]() {
            // First attempt after initial delay
            background_service::Initialize();
            
            // Schedule a second check in case the first was too early
            // This handles the timing uncertainty between initquit and user_interface initialization
            if (!background_service::HasBackgroundWindow() && security_config::IsBackgroundModeEnabled()) {
                // Use a timer to retry after 500ms
                static UINT_PTR timerId = 0;
                timerId = SetTimer(nullptr, 0, 500, [](HWND, UINT, UINT_PTR id, DWORD) {
                    KillTimer(nullptr, id);
                    // Only initialize if WebView2 UI is not active and we don't have a background window
                    if (!background_service::IsWebViewUIActive() && !background_service::HasBackgroundWindow()) {
                        background_service::Initialize();
                    }
                });
            }
        });
    }
    
    void on_quit() override {
        background_service::Shutdown();
    }
};

// Register initquit service
static initquit_factory_t<BackgroundServiceInitQuit> g_background_service_factory;


// ============================================
// Main menu command to toggle background window
// ============================================
static const GUID guid_mainmenu_group_webview = 
    { 0xc8d9e0f1, 0x2a3b, 0x4c5d, { 0x6e, 0x7f, 0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd5 } };

static const GUID guid_cmd_toggle_window = 
    { 0xc8d9e0f2, 0x2a3b, 0x4c5d, { 0x6e, 0x7f, 0x80, 0x91, 0xa2, 0xb3, 0xc4, 0xd6 } };

static mainmenu_group_popup_factory g_mainmenu_group(
    guid_mainmenu_group_webview,
    mainmenu_groups::view,
    mainmenu_commands::sort_priority_dontcare,
    "WebView2 UI"
);

class MainMenuCommands : public mainmenu_commands {
public:
    t_uint32 get_command_count() override {
        return 1;
    }
    
    GUID get_command(t_uint32 index) override {
        switch (index) {
            case 0: return guid_cmd_toggle_window;
            default: return pfc::guid_null;
        }
    }
    
    void get_name(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
            case 0: out = TRU("Show/Hide Window", "显示/隐藏窗口"); break;
            default: break;
        }
    }
    
    bool get_description(t_uint32 index, pfc::string_base& out) override {
        switch (index) {
            case 0: 
                out = TRU("Toggle visibility of WebView2 UI window", "切换 WebView2 UI 窗口的可见性");
                return true;
            default:
                return false;
        }
    }
    
    GUID get_parent() override {
        return guid_mainmenu_group_webview;
    }
    
    void execute(t_uint32 index, service_ptr_t<service_base> callback) override {
        switch (index) {
            case 0:
                // Check if WebView2 UI is active as main interface
                if (WebViewUI::GetInstance()) {
                    WebViewUI::GetInstance()->activate();
                } else if (background_service::HasBackgroundWindow()) {
                    background_service::ToggleWindow();
                } else if (WebViewContext::GetInstance().GetInstanceCount() > 0) {
                    // 面板模式 - 没有独立窗口可切换
                    if (background_service::IsBackgroundModeEnabled()) {
                        // 后台模式已启用但窗口尚未创建，重新初始化
                        background_service::Initialize();
                        if (background_service::HasBackgroundWindow()) {
                            background_service::ShowWindow();
                        } else {
                            // 初始化失败，打开偏好设置页
                            ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                        }
                    } else {
                        // 后台模式未启用，直接打开偏好设置页
                        ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                    }
                } else {
                    // 无窗口也无面板，打开偏好设置页
                    ui_control::get()->show_preferences(webview_prefs::GetPreferencesPageGuid());
                }
                break;
            default: break;
        }
    }
    
    bool get_display(t_uint32 index, pfc::string_base& text, t_uint32& flags) override {
        flags = 0;
        
        if (index == 0) {
            if (WebViewUI::GetInstance()) {
                // 独立 UI 模式
                text = TRU("Show/Hide Window", "显示/隐藏窗口");
                if (WebViewUI::GetInstance()->is_visible()) flags |= flag_checked;
            } else if (background_service::HasBackgroundWindow()) {
                // 后台窗口模式
                text = TRU("Show/Hide Window", "显示/隐藏窗口");
                HWND hwnd = background_service::GetBackgroundWindow()->GetHwnd();
                if (hwnd && IsWindowVisible(hwnd)) flags |= flag_checked;
            } else {
                // 面板模式或无实例 - 显示为偏好设置入口
                text = TRU("Preferences...", "偏好设置...");
            }
        }
        
        return true;
    }
};

static mainmenu_commands_factory_t<MainMenuCommands> g_mainmenu_commands_factory;
