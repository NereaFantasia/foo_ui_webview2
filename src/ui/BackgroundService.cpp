// ============================================
// BackgroundService.cpp - Background Mode Support Implementation
// ============================================

#include "pch.h"
#include "ui/BackgroundService.h"
#include "ui/AppQuit.h"
#include "window/MainWindow.h"
#include "ui/UserInterface.h"
#include "prefs/PreferencesPage.h"
#include "api/AudioApi.h"
#include "api/EventEmit.h"
#include "api/generated/AppSchema.h"
#include "window/MenuOverlayHost.h"
#include "window/WindowManager.h"
#include "settings/SecurityConfig.h"
#include "settings/WindowStateConfig.h"

namespace {
    // Background window instance (only created when not using WebView2 UI as main UI)
    std::unique_ptr<MainWindow> g_backgroundWindow;
    HWND g_backgroundHwnd = nullptr;
    bool g_initialized = false;
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
        // 自己决定 SW_SHOW 或 SW_HIDE。不得在外部叠加额外的 SW_SHOWMINNOACTIVE +
        // 延迟隐藏序列：那会与 reveal 自身的 SW_SHOW 争抢，使物理窗口可见性与
        // 菜单读到的 IsWindowVisible 在数百毫秒内不一致。
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
    
    // 前置通知（允许失败，不阻断关键清理）。与本插件另一个 initquit 的 on_quit 谁先跑都只发一次。
    app_quit::AnnounceBeforeQuit();
    
    try {
        ShutdownAudioVisualizationRuntime();
    } catch (const std::exception& e) {
        console::printf("[WebView2 UI] WARNING: Audio visualization shutdown failed: %s", e.what());
    } catch (...) {
        console::print("[WebView2 UI] WARNING: Unknown exception during audio visualization shutdown");
    }
    
    // 关键清理（窗口销毁 + 状态复位，必须执行）。与 WebViewUI::shutdown 同一
    // 顺序：先关菜单 overlay 与 popup，再关承载它们的窗口。必须在静态析构
    // 之前完成，否则 OnDestroy 可能访问已经析构的异步操作注册表。
    try {
        MenuOverlayHost::GetInstance().Shutdown();
    } catch (const std::exception& e) {
        console::printf("[WebView2 UI] WARNING: MenuOverlayHost shutdown failed: %s", e.what());
    } catch (...) {
        console::print("[WebView2 UI] WARNING: Unknown exception during MenuOverlayHost shutdown");
    }

    try {
        WindowManager::GetInstance().Shutdown();
    } catch (const std::exception& e) {
        console::printf("[WebView2 UI] WARNING: WindowManager shutdown failed: %s", e.what());
    } catch (...) {
        console::print("[WebView2 UI] WARNING: Unknown exception during WindowManager shutdown");
    }

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
