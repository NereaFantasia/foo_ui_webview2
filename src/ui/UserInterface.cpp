#include "pch.h"
#include "ui/UserInterface.h"
#include "ui/AppQuit.h"
#include "core/WebViewContext.h"
#include "api/AudioApi.h"
#include "api/EventEmit.h"
#include "api/generated/AppSchema.h"
#include "window/MainWindow.h"
#include "window/MenuOverlayHost.h"
#include "window/WindowManager.h"

// ============================================
// GUID definition — must not collide with any other component's UI element
// (including foobar2000 built-ins), otherwise fb2k rejects registration.
// ============================================
// {3BF13CCC-477E-459A-A093-990D801AD37E}
const GUID WebViewUI::guid = 
{ 0x3bf13ccc, 0x477e, 0x459a, { 0xa0, 0x93, 0x99, 0x0d, 0x80, 0x1a, 0xd3, 0x7e } };

WebViewUI* WebViewUI::s_instance = nullptr;

fb2k::hwnd_t WebViewUI::init(HookProc_t hook) {
    LOG("Initializing WebView2 UI...");
    
    hookProc_ = hook;
    
    try {
        // Create main window as top-level window
        mainWindow_ = std::make_unique<MainWindow>();
        mainHwnd_ = mainWindow_->Create(nullptr);  // Top-level window
        
        if (!mainHwnd_) {
            LOG("ERROR: Failed to create main window!");
            mainWindow_.reset();
            hookProc_ = nullptr;
            return nullptr;
        }
        
        // 只在窗口创建成功后才发布单例
        s_instance = this;
        
        LOG("Main window created successfully: ", pfc::format_hex((size_t)mainHwnd_));
        
        return mainHwnd_;
    }
    catch (const std::exception& e) {
        LOG("ERROR: Exception during initialization: ", e.what());
        mainWindow_.reset();
        mainHwnd_ = nullptr;
        hookProc_ = nullptr;
        return nullptr;
    }
    catch (...) {
        LOG("ERROR: Unknown exception during initialization");
        mainWindow_.reset();
        mainHwnd_ = nullptr;
        hookProc_ = nullptr;
        return nullptr;
    }
}

void WebViewUI::shutdown() {
    LOG("Shutting down WebView2 UI...");
    
    // 前置通知（允许失败，不阻断关键清理）。通常 initquit::on_quit 已经发过，这里不再重复发。
    app_quit::AnnounceBeforeQuit();
    
    try {
        ShutdownAudioVisualizationRuntime();
    } catch (const std::exception& e) {
        LOG("WARNING: Audio visualization shutdown failed: ", e.what());
    } catch (...) {
        LOG("WARNING: Unknown exception during audio visualization shutdown");
    }
    
    // 关键清理（必须执行）。每一个 PopupWindow 都必须在这里被同步销毁：
    // 它的 OnDestroy 会回调文件操作 / 探测 / HTTP 三个注册表、PortHub 与
    // WebViewContext，这些都是函数级 static，进程退出的静态析构期一旦开始就
    // 不能再碰。菜单 overlay 不经 WindowManager 管理，要单独关。
    try {
        MenuOverlayHost::GetInstance().Shutdown();
    } catch (const std::exception& e) {
        LOG("WARNING: MenuOverlayHost shutdown failed: ", e.what());
    } catch (...) {
        LOG("WARNING: Unknown exception during MenuOverlayHost shutdown");
    }

    try {
        WindowManager::GetInstance().Shutdown();
    } catch (const std::exception& e) {
        LOG("WARNING: WindowManager shutdown failed: ", e.what());
    } catch (...) {
        LOG("WARNING: Unknown exception during WindowManager shutdown");
    }
    
    try {
        if (mainWindow_) {
            mainWindow_->Destroy();
            mainWindow_.reset();
        }
    } catch (const std::exception& e) {
        LOG("ERROR: Main window destroy failed: ", e.what());
        mainWindow_.reset();
    } catch (...) {
        LOG("ERROR: Unknown exception during main window destroy");
        mainWindow_.reset();
    }
    
    // 单例复位（必定执行，无异常风险）
    s_instance = nullptr;
    mainHwnd_ = nullptr;
    hookProc_ = nullptr;
    
    LOG("Shutdown complete");
}

void WebViewUI::activate() {
    if (mainHwnd_ && IsWindow(mainHwnd_)) {
        const bool wasHidden = !IsWindowVisible(mainHwnd_);
        // 检查窗口当前状态，避免覆盖最大化/最小化状态
        WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
        if (GetWindowPlacement(mainHwnd_, &wp)) {
            // 如果窗口最小化，恢复它（保持之前的最大化/正常状态）
            if (wp.showCmd == SW_SHOWMINIMIZED) {
                ShowWindow(mainHwnd_, SW_RESTORE);
            } else if (!IsWindowVisible(mainHwnd_)) {
                // 窗口隐藏时，使用之前保存的状态显示
                ShowWindow(mainHwnd_, wp.showCmd);
            }
            // 如果窗口已经可见且非最小化，不改变其状态
        } else {
            // 获取窗口状态失败，使用默认行为
            ShowWindow(mainHwnd_, SW_SHOW);
        }
        if (wasHidden && mainWindow_) {
            mainWindow_->RestoreSurfaceAfterHidden("webview-ui-activate");
        }
        SetForegroundWindow(mainHwnd_);
    }
}

void WebViewUI::hide() {
    if (mainHwnd_ && IsWindow(mainHwnd_)) {
        ShowWindow(mainHwnd_, SW_MINIMIZE);
    }
}

bool WebViewUI::is_visible() {
    if (mainHwnd_ && IsWindow(mainHwnd_)) {
        return IsWindowVisible(mainHwnd_) && !IsIconic(mainHwnd_);
    }
    return false;
}

// ============================================
// Register user_interface with foobar2000
// ============================================
static user_interface_factory<WebViewUI> g_webview_ui_factory;
