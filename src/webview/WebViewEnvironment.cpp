#include "pch.h"
#include "webview/WebViewEnvironment.h"
#include "core/WebViewContext.h"
#include "core/SecurityConfig.h"
#include <Shlobj.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2EnvironmentOptions.h>

using namespace Microsoft::WRL;

// ============================================
// WebViewEnvironment 实现
// ============================================

WebViewEnvironment& WebViewEnvironment::GetInstance() {
    static WebViewEnvironment instance;
    return instance;
}

std::wstring WebViewEnvironment::GetUserDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        std::wstring userDataPath = std::wstring(path) + L"\\foobar2000\\foo_ui_webview2";
        
        // Ensure directory exists; both calls intentionally ignore the return
        // value because ERROR_ALREADY_EXISTS is the normal steady-state result.
        (void)CreateDirectoryW((std::wstring(path) + L"\\foobar2000").c_str(), nullptr);
        (void)CreateDirectoryW(userDataPath.c_str(), nullptr);
        
        return userDataPath;
    }
    return L"";
}

void WebViewEnvironment::Preheat() {
    bool shouldCreate = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (ready_ || creating_) {
            return;  // Already ready or creating
        }
        
        creating_ = true;  // Set flag while holding lock
        shouldCreate = true;
    }
    // Lock released here - important to avoid deadlock if callback is synchronous
    
    if (shouldCreate) {
        console::print("[WebView2 UI] Preheating WebView2 environment...");
        CreateEnvironmentInternal();
    }
}

void WebViewEnvironment::GetEnvironment(const EnvironmentCallback& callback) {
    ICoreWebView2Environment* envToReturn = nullptr;
    bool shouldStartCreating = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (ready_ && environment_) {
            // Environment is ready, will call callback outside lock
            envToReturn = environment_.get();
        } else {
            // Add to pending callbacks
            if (callback) {
                pendingCallbacks_.push_back(callback);
            }
            
            // Start creating if not already
            if (!creating_) {
                creating_ = true;  // Set flag while holding lock
                shouldStartCreating = true;
            }
        }
    }
    // Lock released here - important to avoid deadlock if callback is synchronous
    
    // Call callback outside lock to avoid deadlock
    if (envToReturn && callback) {
        callback(envToReturn);
        return;
    }
    
    if (shouldStartCreating) {
        CreateEnvironmentInternal();
    }
}

void WebViewEnvironment::CreateEnvironmentInternal() {
    // Note: creating_ flag should already be set by caller
    
    auto userDataPath = GetUserDataPath();
    
    // Check WebView2 Runtime availability
    wil::unique_cotaskmem_string version;
    HRESULT checkHr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    if (FAILED(checkHr) || !version) {
        console::print("[WebView2 UI] WebView2 Runtime not available");
        DrainPendingCallbacksOnFailure();
        return;
    }
    
    FB2K_console_formatter() << "[WebView2 UI] WebView2 Runtime version: " 
                             << pfc::stringcvt::string_utf8_from_wide(version.get()).get_ptr();
    
    // Create environment options with custom schemes
    auto envOptions = Make<CoreWebView2EnvironmentOptions>();
    
    // Register fb2k:// and artwork:// protocols
    auto schemeRegistrationFb2k = Make<CoreWebView2CustomSchemeRegistration>(L"fb2k");
    schemeRegistrationFb2k->put_TreatAsSecure(TRUE);
    schemeRegistrationFb2k->put_HasAuthorityComponent(TRUE);
    LPCWSTR allowedOriginsFb2k[] = { L"*" };
    schemeRegistrationFb2k->SetAllowedOrigins(1, allowedOriginsFb2k);
    
    auto schemeRegistrationArtwork = Make<CoreWebView2CustomSchemeRegistration>(L"artwork");
    schemeRegistrationArtwork->put_TreatAsSecure(TRUE);
    schemeRegistrationArtwork->put_HasAuthorityComponent(FALSE);
    LPCWSTR allowedOriginsArtwork[] = { L"*" };
    schemeRegistrationArtwork->SetAllowedOrigins(1, allowedOriginsArtwork);
    
    ICoreWebView2CustomSchemeRegistration* schemeRegistrations[] = {
        schemeRegistrationFb2k.Get(),
        schemeRegistrationArtwork.Get()
    };
    envOptions->SetCustomSchemeRegistrations(2, schemeRegistrations);
    
    // ============================================
    // 浏览器命令行参数
    // ============================================
    // Visual Hosting (CompositionController + DirectComposition) 模式下，
    // Chromium 的 native window occlusion 计算会在窗口最小化/被完全遮挡时
    // 把 WebView 判定为 occluded 并挂起呈现；恢复时在 Composition 模式下
    // 运行时不一定能可靠收到“重新可见”信号，导致 DComp 表面停留空白
    // （现象：最小化→恢复后只剩空 Win32 窗口壳，且不触发 ProcessFailed）。
    // 关闭该特性可让 WebView 始终保持呈现，规避恢复空白。
    // 代价：窗口最小化时后台仍持续渲染，CPU/GPU 占用略增（音乐播放器常驻后台，可接受）。
    std::wstring browserArgs = L"--disable-features=CalculateNativeWinOcclusion";

    // CDP remote debugging port — 仅在 CDP 远程调试开关启用时开放
    // 允许 MCP 工具集 / AI 智能体通过 CDP 操控 WebView2
    if (security_config::IsCdpRemoteEnabled()) {
        browserArgs += L" --remote-debugging-port=9222";
        console::print("[WebView2 UI] CDP remote debugging enabled on port 9222");
    }

    envOptions->put_AdditionalBrowserArguments(browserArgs.c_str());
    
    // Create environment
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,                    // Browser executable path
        userDataPath.c_str(),       // User data directory
        envOptions.Get(),           // Environment options
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                std::vector<EnvironmentCallback> callbacksToInvoke;
                ICoreWebView2Environment* envToReturn = nullptr;
                bool failed = false;
                
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    
                    if (FAILED(result) || !env) {
                        FB2K_console_formatter() << "[WebView2 UI] Failed to create environment, HRESULT: 0x" 
                                                 << pfc::format_hex((uint32_t)result);
                        // 提取等待者，在锁外以 nullptr 回调
                        callbacksToInvoke = std::move(pendingCallbacks_);
                        pendingCallbacks_.clear();
                        creating_ = false;
                        failed = true;
                    } else {
                        environment_ = env;
                        ready_ = true;
                        creating_ = false;
                        envToReturn = environment_.get();
                        
                        // Move callbacks to local vector to call outside lock
                        callbacksToInvoke = std::move(pendingCallbacks_);
                        pendingCallbacks_.clear();
                    }
                }
                
                if (failed) {
                    // 通知所有等待者环境创建失败
                    for (auto& cb : callbacksToInvoke) {
                        if (cb) cb(nullptr);
                    }
                    return result;
                }
                
                console::print("[WebView2 UI] WebView2 environment preheated successfully");
                
                // Call all pending callbacks outside lock to avoid deadlock
                for (auto& cb : callbacksToInvoke) {
                    if (cb) {
                        cb(envToReturn);
                    }
                }
                
                return S_OK;
            }
        ).Get()
    );
    
    if (FAILED(hr)) {
        FB2K_console_formatter() << "[WebView2 UI] CreateCoreWebView2EnvironmentWithOptions failed: 0x" 
                                 << pfc::format_hex((uint32_t)hr);
        DrainPendingCallbacksOnFailure();
    }
}

void WebViewEnvironment::DrainPendingCallbacksOnFailure() {
    std::vector<EnvironmentCallback> failedCallbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        creating_ = false;
        failedCallbacks = std::move(pendingCallbacks_);
        pendingCallbacks_.clear();
    }
    // 在锁外回调，避免死锁
    for (auto& cb : failedCallbacks) {
        if (cb) cb(nullptr);
    }
}

void WebViewEnvironment::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 不再主动释放 environment_ COM 引用。
    // controller_->Close() 是异步操作，WebView2 运行时在关闭后仍有后台清理任务。
    // 若在此处释放最后一个 ICoreWebView2Environment 引用，运行时的异步回调会
    // 访问已销毁对象，导致未捕获异常 → std::terminate → abort。
    // 让 COM 运行时在进程退出时自然回收。
    if (environment_) {
        console::print("[WebView2 UI] WebView2 environment marked for natural release");
    }
    
    ready_ = false;
    creating_ = false;
    pendingCallbacks_.clear();
    
    console::print("[WebView2 UI] WebView2 environment shutdown complete");
}

// ============================================
// initquit 服务 - 在启动时预热环境
// ============================================

class WebViewEnvironmentInitQuit : public initquit {
public:
    void on_init() override {
        // Preheat environment on startup
        // Use main thread callback to ensure proper COM context
        fb2k::inMainThread([]() {
            WebViewEnvironment::GetInstance().Preheat();
        });
    }
    
    void on_quit() override {
        // 显式释放 WebView2 环境的全局 COM 引用
        // 此时所有 Controller 已由 user_interface::shutdown() 关闭
        // 释放环境引用后 msedgewebview2.exe 子进程将自动退出
        WebViewEnvironment::GetInstance().Shutdown();
    }
};

// Register with high priority to start early
static initquit_factory_t<WebViewEnvironmentInitQuit> g_webview_env_factory;

void InitWebViewEnvironment() {
    WebViewEnvironment::GetInstance().Preheat();
}

void ShutdownWebViewEnvironment() {
    WebViewEnvironment::GetInstance().Shutdown();
}
