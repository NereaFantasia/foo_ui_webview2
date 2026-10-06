#include "pch.h"
#include "webview/WebViewHost.h"
#include "webview/WebViewEnvironment.h"
#include "webview/ArtworkWorkerQueue.h"
#include "window/WindowBehaviorTrace.h"
#include <Shlobj.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <dcomp.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <string>
#include <atomic>      // std::atomic for thread-safe ref counting
#include <WebView2EnvironmentOptions.h>  // SDK 提供的环境选项辅助类

using namespace Microsoft::WRL;

// ==========================================================================
// 自定义协议注册
// 使用 WebView2 SDK 提供的 CoreWebView2EnvironmentOptions 和
// CoreWebView2CustomSchemeRegistration 辅助类（来自 WebView2EnvironmentOptions.h）
// 这些类正确实现了所有需要的 COM 接口 (Options1-8)
// ==========================================================================

WebViewHost::~WebViewHost() {
    try {
    LogLifecycle("host.destroy.begin");
    suspendState_->alive.store(false, std::memory_order_release);
    suspendState_->generation.fetch_add(1, std::memory_order_acq_rel);
    // Mark alive-flag false so pending completions skip member access,
    // then drain the worker queue (cancels in-flight + queued items).
    artworkHostAlive_->store(false, std::memory_order_release);
    if (artworkWorkerQueue_) {
        artworkWorkerQueue_->DrainAll();
        artworkWorkerQueue_.reset();  // joins worker thread
    }
    ClearMediaRequests(true);
    RemoveArtworkEventHandlers();
    artworkLifecycle_.OnClose();
        // 清理光标事件监听
        if (compositionController_ && cursorChangedToken_.value != 0) {
            compositionController_->remove_CursorChanged(cursorChangedToken_);
        }
        // 清理焦点事件监听
        if (controller_ && gotFocusToken_.value != 0) {
            controller_->remove_GotFocus(gotFocusToken_);
        }
        if (controller_ && lostFocusToken_.value != 0) {
            controller_->remove_LostFocus(lostFocusToken_);
        }
        // 清理进程崩溃事件监听
        if (webview_ && processFailedToken_.value != 0) {
            webview_->remove_ProcessFailed(processFailedToken_);
        }
        // 清理快捷键转发与页面消息事件监听
        if (controller_ && acceleratorKeyPressedToken_.value != 0) {
            controller_->remove_AcceleratorKeyPressed(acceleratorKeyPressedToken_);
        }
        if (webview_ && webMessageReceivedToken_.value != 0) {
            webview_->remove_WebMessageReceived(webMessageReceivedToken_);
        }
        // 清理虚拟主机 charset 修复的资源请求监听
        if (webview_ && charsetResourceRequestedToken_.value != 0) {
            webview_->remove_WebResourceRequested(charsetResourceRequestedToken_);
        }
        
        // 清理 DirectComposition
        CleanupDirectComposition();
        
        if (controller_) {
            controller_->Close();
            controller_.reset();
        }
        compositionController_.reset();
        webview_.reset();
        environment_.reset();
    }
    catch (...) {
        // 析构函数禁止抛异常 — 关闭期间 WebView2 运行时可能因
        // 异步清理竞态抛出异常，在此吞掉以避免 std::terminate
    }
}

HRESULT WebViewHost::Initialize(HWND parentHwnd, InitCallback callback, bool useVisualHosting) {
    parentHwnd_ = parentHwnd;
    useVisualHosting_ = useVisualHosting;
    ownerThreadId_ = GetCurrentThreadId();
    LogLifecycle("host.initialize.begin",
        std::string("visualHosting=") + (useVisualHosting ? "true" : "false"));
    
    LOG("WebViewHost::Initialize - useVisualHosting=", useVisualHosting ? "true" : "false",
        " parentHwnd=0x", pfc::format_hex((uintptr_t)parentHwnd));
    
    // Initialize DirectComposition (only for Visual Hosting mode - standalone window)
    HRESULT dcompHr = E_FAIL;
    if (useVisualHosting_) {
        dcompHr = InitializeDirectComposition();
        if (FAILED(dcompHr)) {
            LOG("DirectComposition initialization failed, HRESULT: ", pfc::format_hex((uint32_t)dcompHr));
        }
    } else {
        LOG("Panel mode: skipping DirectComposition, using standard Controller");
    }
    WebViewEnvironment::GetInstance().GetEnvironment(
        [this, state = suspendState_, callback, dcompHr](ICoreWebView2Environment* env) {
            if (!state->alive.load(std::memory_order_acquire)) return;
            if (!env) {
                LogLifecycle("host.environment.failed", "environment=null");
                LOG("Failed to get WebView2 environment");
                if (callback) callback(false);
                return;
            }
            
            environment_ = env;
            LogLifecycle("host.environment.ready");
            LOG("Using shared WebView2 environment");
            
            // Create controller using shared environment
            CreateControllerWithEnvironment(env, callback, SUCCEEDED(dcompHr));
        }
    );
    
    return S_OK;
}

void WebViewHost::CreateControllerWithEnvironment(ICoreWebView2Environment* env, InitCallback callback, bool useDComp) {
    // Try Visual Hosting mode (ICoreWebView2Environment3)
    wil::com_ptr<ICoreWebView2Environment3> env3;
    if (useDComp && dcompDevice_ && SUCCEEDED(env->QueryInterface(IID_PPV_ARGS(&env3)))) {
        LOG("Using Visual Hosting mode (CompositionController)");
        
        env3->CreateCoreWebView2CompositionController(
            parentHwnd_,
            Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
                [this, state = suspendState_, callback](HRESULT result, ICoreWebView2CompositionController* compositionController) -> HRESULT {
                    if (!state->alive.load(std::memory_order_acquire)) return S_OK;
                    
                    if (FAILED(result) || !compositionController) {
                        LogLifecycle("controller.create.failed",
                            "mode=composition hr=" + std::to_string((long long)result));
                        LOG("Failed to create CompositionController, HRESULT: ", pfc::format_hex((uint32_t)result));
                        if (callback) callback(false);
                        return result;
                    }
                    
                    compositionController_ = compositionController;
                    
                    // Get standard Controller from CompositionController
                    HRESULT hr = compositionController->QueryInterface(IID_PPV_ARGS(&controller_));
                    if (FAILED(hr)) {
                        LOG("Failed to get Controller from CompositionController");
                        if (callback) callback(false);
                        return hr;
                    }
                    
                    window_behavior_trace::Com(parentHwnd_, "controller.put_IsVisible", 1);
                    controller_->put_IsVisible(TRUE);
                    controller_->get_CoreWebView2(&webview_);
                    artworkLifecycle_.OnControllerCreated();
                    if (!artworkWorkerQueue_) {
                        artworkWorkerQueue_ =
                            std::make_unique<artwork_worker::ArtworkWorkerQueue>();
                    } else {
                        artworkWorkerQueue_->CancelStale(
                            artworkLifecycle_.NavigationGeneration(),
                            artworkLifecycle_.HostGeneration());
                    }
                    if (dcompWebViewVisual_) {
                        window_behavior_trace::Com(parentHwnd_, "composition.put_RootVisualTarget");
                        hr = compositionController_->put_RootVisualTarget(dcompWebViewVisual_.get());
                        if (SUCCEEDED(hr)) {
                            window_behavior_trace::Com(parentHwnd_, "dcomp.Commit");
                            dcompDevice_->Commit();
                            LOG("Visual Hosting: WebView connected to DirectComposition");
                        }
                    }
                    
                    // Setup cursor handling
                    SetupCursorHandling();
                    
                    SetupWebView();
                    SetupSettings();
                    ApplyDefaultZoomPreference();
                    SetupCustomProtocol();
                    SetupMediaProtocol();
                    RegisterMessageHandler();
                    SetupProcessFailedHandling();
                    InjectBridgeScript();
                    InjectSdkBridgeScript();

                    LogLifecycle("controller.create.ready", "mode=composition");
                    
                    LOG("WebView2 initialized successfully (Visual Hosting mode)");
                    if (callback) callback(true);
                    
                    return S_OK;
                }
            ).Get()
        );
        return;
    }
    
    // Fallback: Use standard Controller mode
    LOG("Using standard Controller mode");
    env->CreateCoreWebView2Controller(
        parentHwnd_,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this, state = suspendState_, callback](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                if (!state->alive.load(std::memory_order_acquire)) return S_OK;
                
                if (FAILED(result) || !controller) {
                    LogLifecycle("controller.create.failed",
                        "mode=standard hr=" + std::to_string((long long)result));
                    LOG("Failed to create WebView2 controller, HRESULT: ", pfc::format_hex((uint32_t)result));
                    if (callback) callback(false);
                    return result;
                }
                
                controller_ = controller;
                window_behavior_trace::Com(parentHwnd_, "controller.put_IsVisible", 1);
                controller_->put_IsVisible(TRUE);
                controller->get_CoreWebView2(&webview_);
                artworkLifecycle_.OnControllerCreated();
                if (!artworkWorkerQueue_) {
                    artworkWorkerQueue_ =
                        std::make_unique<artwork_worker::ArtworkWorkerQueue>();
                } else {
                    artworkWorkerQueue_->CancelStale(
                        artworkLifecycle_.NavigationGeneration(),
                        artworkLifecycle_.HostGeneration());
                }

                SetupWebView();
                SetupSettings();
                ApplyDefaultZoomPreference();
                SetupCustomProtocol();
                SetupMediaProtocol();
                RegisterMessageHandler();
                SetupProcessFailedHandling();
                InjectBridgeScript();
                InjectSdkBridgeScript();

                LogLifecycle("controller.create.ready", "mode=standard");
                
                LOG("WebView2 initialized successfully (standard mode)");
                if (callback) callback(true);
                
                return S_OK;
            }
        ).Get()
    );
}

void WebViewHost::SetMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

void WebViewHost::SetFocusChangedCallback(FocusChangedCallback callback) {
    focusChangedCallback_ = std::move(callback);
}

void WebViewHost::SetNavigationCompletedCallback(NavigationCompletedCallback callback) {
    navigationCompletedCallback_ = std::move(callback);
}

void WebViewHost::SetNavigationStartingCallback(NavigationStartingCallback callback) {
    navigationStartingCallback_ = std::move(callback);
}

void WebViewHost::SetProcessFailedCallback(ProcessFailedCallback callback) {
    processFailedCallback_ = std::move(callback);
}
