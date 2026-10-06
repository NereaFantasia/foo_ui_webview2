/**
 * WebViewPanel - WebView 面板基类实现
 */
#include "pch.h"
#include "core/WebViewPanel.h"
#include "core/WebViewContext.h"
#include "settings/SecurityConfig.h"
#include "prefs/PreferencesPage.h"
#include "webview/WebViewHost.h"
#include "webview/dnd/DndRegistrar.h"
#include "api/BridgeCore.h"
#include "api/EventEmit.h"
#include "api/PortHub.h"
#include "api/generated/PanelSchema.h"
#include "api/generated/WebviewSchema.h"
#include "window/WindowChromeTrace.h"
#include "core/PanelCrashDiagnostics.h"
#include "core/FrontendDirectoryResolver.h"

// Selection Watcher
#include "selection/SelectionWatcher.h"
#include "selection/SelectionHolder.h"

// ============================================
// WebViewPanel 实现
// ============================================

// 虚拟主机名
std::wstring WebViewPanel::GetVirtualHostName() {
    return L"foo-ui-webview2.local";
}

WebViewPanel::WebViewPanel() 
    : selectionHolder_(std::make_unique<SelectionHolder>())
{
}

WebViewPanel::~WebViewPanel() {
    DestroyWebView();
}

bool WebViewPanel::InitializeWebView(HWND hwnd, WebViewPanelMode mode) {
    if (!hwnd || !IsWindow(hwnd)) {
        LOG("WebViewPanel::InitializeWebView - Invalid HWND");
        return false;
    }
    
    hwnd_ = hwnd;
    mode_ = mode;
    
    LOG("WebViewPanel::InitializeWebView - Mode: ", 
        mode == WebViewPanelMode::Standalone ? "Standalone" :
        mode == WebViewPanelMode::DuiPanel ? "DUI" : "CUI");
    
    // 创建 per-instance BridgeCore
    bridge_ = std::make_unique<BridgeCore>();
    
    // 创建 WebView2 宿主
    webView_ = std::make_unique<WebViewHost>();

    // Declared before the callbacks below so each of them can capture the pair
    // and discard itself when the panel is gone or a newer WebView superseded it.
    const uint64_t myGeneration = ++webViewGeneration_;
    std::weak_ptr<PanelAlive> weakAlive = alive_;

    // Set focus change callback for panel:focus/blur events
    webView_->SetFocusChangedCallback([this](bool gotFocus) {
        if (gotFocus) {
            OnSetFocus();
        } else {
            OnKillFocus();
        }
    });

    // 导航完成回调（启动可见性收敛用）
    webView_->SetNavigationCompletedCallback([this, myGeneration, weakAlive](bool success) {
        if (weakAlive.expired()) return;
        if (myGeneration != webViewGeneration_) return;
        HandleNavigationCompleted(success);
    });

    // 页面开始顶层导航时，旧页面打开的端口随它关闭。回调在新文档存在之前、UI 线程上
    // 运行，新页面的消息同样在 UI 线程上分发且只能晚于它，所以新页面重新打开的端口不会被清掉。
    webView_->SetNavigationStartingCallback([this, myGeneration, weakAlive]() {
        if (weakAlive.expired()) return;
        if (myGeneration != webViewGeneration_) return;
        PortHub::Instance().CleanupPagePorts(hwnd_);
        OnTopLevelNavigationStarting();
    });

    // 进程崩溃回调（Visual Hosting 模式下进程崩溃只剩空窗口的诊断/恢复入口）
    webView_->SetProcessFailedCallback(
        [this](COREWEBVIEW2_PROCESS_FAILED_KIND kind, bool recovered) {
            OnWebViewProcessFailed(kind, recovered);
        });

    bool useVisualHosting = (mode_ == WebViewPanelMode::Standalone);

    HRESULT hr = webView_->Initialize(hwnd_, [this, myGeneration, weakAlive](bool success) {
        // Discard callbacks from a destroyed panel or a superseded generation.
        if (weakAlive.expired()) return;
        if (myGeneration != webViewGeneration_) return;

        if (success) {
            CompleteWebViewInit(myGeneration);
        } else {
            LOG("ERROR: WebView2 initialization failed");
            // 先让子类清理初始化/重建状态，再触发导航失败信号允许窗口显示。
            OnWebViewInitFailed();
            // WebView 初始化失败，触发导航失败信号以允许窗口显示
            OnNavigationCompleted(false);
        }
    }, useVisualHosting);
    
    if (FAILED(hr)) {
        LOG("ERROR: Failed to start WebView2 initialization, HRESULT: ", 
            pfc::format_hex((uint32_t)hr));
        return false;
    }
    
    return true;
}

// Success half of the WebView creation callback. Extracted from the lambda so
// the callback stays a thin dispatcher; the generation is passed through because
// the registrar records it to reject a stale Unregister.
void WebViewPanel::CompleteWebViewInit(uint64_t generation) {
    webViewReady_ = true;

    // 注册到 WebViewContext（带 windowId 支持多窗口系统）
    if (!windowId_.empty()) {
        WebViewContext::GetInstance().RegisterInstance(hwnd_, webView_.get(), bridge_.get(), windowId_);
    } else {
        WebViewContext::GetInstance().RegisterInstance(hwnd_, webView_.get(), bridge_.get());
    }

    // 调用虚函数，允许子类添加额外Initializing
    OnWebViewReady();

    // Registered after OnWebViewReady rather than inside it: PopupWindow
    // overrides that virtual without calling the base, so a virtual hook would
    // silently skip the main Visual Hosting host.
    if (!dndRegistrar_) {
        dndRegistrar_ = std::make_unique<fb2k_dnd::DndRegistrar>();
    }
    dndRegistrar_->Register(
        hwnd_, webView_.get(), generation,
        [this](const char* event, const nlohmann::json& data) {
            if (bridge_) bridge_->EmitEvent(event, data);
        });
}

void WebViewPanel::DestroyWebView() {
    // First: OLE must stop delivering drops before the interfaces the delegate
    // forwards to are released.
    if (dndRegistrar_) {
        dndRegistrar_->Unregister(webViewGeneration_);
        dndRegistrar_.reset();
    }
    ++webViewGeneration_;   // invalidate any in-flight creation callback

    // Unregister from SelectionWatcher first
    SelectionWatcher::GetInstance().UnregisterPanel(this);

    if (webView_) {
        // 在销毁窗口前先断开宿主回调，避免启动隐藏阶段的延迟消息回灌到已关闭窗口。
        webView_->SetMessageHandler({});
        webView_->SetNavigationCompletedCallback({});
        webView_->SetNavigationStartingCallback({});
        webView_->SetFocusChangedCallback({});
        webView_->SetProcessFailedCallback({});
    }
    
    if (hwnd_ && webViewReady_) {
        // Unregister from WebViewContext
        WebViewContext::GetInstance().UnregisterInstance(hwnd_);
        // 页面随 WebView 一起消失，它打开的端口也关闭；先注销再清理，
        // port:disconnected 不再投给这个正在销毁的页面。
        PortHub::Instance().CleanupPagePorts(hwnd_);
    }
    
    webView_.reset();
    bridge_.reset();
    webViewReady_ = false;
    webViewProcessDead_ = false;
    hwnd_ = nullptr;
}

bool WebViewPanel::IsWebViewOperable() const {
    return webView_ && webView_->IsReady() && !webViewProcessDead_;
}

void WebViewPanel::ResizeWebView() {
    if (!IsWebViewOperable() || !hwnd_) {
        return;
    }
    
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    webView_->Resize(clientRect);
}

// 宿主提交的每次导航都取代尚未结束的开发服务器导航：被取代的那次以失败结束时不该触发回退。
// NavigateToString 取消旧导航时，旧导航的完成事件先于新导航的开始事件到达，按导航 ID 判断不出来，
// 所以在这里直接清掉。经开发服务器的导航由调用方在提交之后重新置位。
bool WebViewPanel::Navigate(const std::wstring& url) {
    if (!IsWebViewOperable()) {
        return false;
    }
    devServerNavPending_ = false;
    return SUCCEEDED(webView_->Navigate(url));
}

bool WebViewPanel::NavigateToString(const std::wstring& html) {
    if (!IsWebViewOperable()) {
        return false;
    }
    devServerNavPending_ = false;
    return SUCCEEDED(webView_->NavigateToString(html));
}

void WebViewPanel::Reload() {
    if (IsWebViewOperable()) {
        devServerNavPending_ = false;
        webView_->Reload();
    }
}

// ============================================
// 虚函数默认实现
// ============================================

void WebViewPanel::OnWebViewReady() {
    LOG("WebViewPanel::OnWebViewReady");
    
    // 确保 WebView 覆盖整个客户区
    ResizeWebView();
    
    // 连接 WebView 到 BridgeCore
    bridge_->SetWebView(webView_.get());
    
    // 注册所有 API
    RegisterAllApis();
    
    // Initializing回调
    InitializeCallbacks();
    
    // 设置消息处理器
    webView_->SetMessageHandler([this](const std::wstring& json) {
        HandleWebMessage(json);
    });
    
    // 面板级 DevTools 初始化（全局 OR 面板级 → DevTools 可用）
    if (panelConfig_.enableDevTools && !security_config::IsDevToolsEnabled()) {
        if (auto* wv = webView_->GetWebView()) {
            wil::com_ptr<ICoreWebView2Settings> settings;
            if (SUCCEEDED(wv->get_Settings(&settings)) && settings) {
                settings->put_AreDevToolsEnabled(TRUE);
                LOG("WebViewPanel: Panel-level DevTools ENABLED");
            }
        }
    }

    // Load frontend page
    LoadFrontendPage();
    
    // Register with SelectionWatcher for selection change events
    SelectionWatcher::GetInstance().RegisterPanel(this);
    
    LOG("WebViewPanel::OnWebViewReady completed");
}

void WebViewPanel::HandleWebMessage(const std::wstring& json) {
    // 拦截内部 windowReady / startupProbe 消息，不交给 BridgeCore
    if (json.find(L"\"windowReady\"") != std::wstring::npos ||
        json.find(L"\"visualReady\"") != std::wstring::npos ||
        json.find(L"\"startupProbe\"") != std::wstring::npos) {
        try {
            std::string utf8(pfc::stringcvt::string_utf8_from_wide(json.c_str()).get_ptr());
            auto msg = nlohmann::json::parse(utf8);
            if (msg.value("type", "") == "windowReady") {
                const std::string source = msg.value("source", "");
                LOG("WebViewPanel: windowReady signal received, source=",
                    source, " readyState=", msg.value("readyState", ""));
                OnWindowReadySignal(source);
                return;
            }
            if (msg.value("type", "") == "visualReady") {
                const std::string source = msg.value("source", "");
                LOG("WebViewPanel: visualReady signal received, source=",
                    source, " readyState=", msg.value("readyState", ""));
                OnVisualReadySignal(source);
                return;
            }
            if (msg.value("type", "") == "startupProbe") {
                std::ostringstream stream;
                stream << "[StartupProbeDom]"
                       << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " windowId=" << (GetWindowId().empty() ? "-" : GetWindowId().c_str())
                       << " hwnd=0x" << pfc::format_hex((size_t)hwnd_)
                       << " phase=" << msg.value("phase", "")
                       << " readyState=" << msg.value("readyState", "")
                       << " visibilityState=" << msg.value("visibilityState", "")
                       << " perfNow=" << msg.value("perfNow", -1.0)
                       << " fp=" << msg.value("firstPaint", -1.0)
                       << " fcp=" << msg.value("firstContentfulPaint", -1.0)
                       << " htmlBg=" << msg.value("htmlBackground", "")
                       << " htmlOpacity=" << msg.value("htmlOpacity", "")
                       << " bodyBg=" << msg.value("bodyBackground", "")
                       << " bodyOpacity=" << msg.value("bodyOpacity", "")
                       << " centerTag=" << msg.value("centerTag", "")
                       << " centerId=" << msg.value("centerId", "")
                       << " centerClass=" << msg.value("centerClass", "")
                       << " centerBg=" << msg.value("centerBackground", "")
                       << " centerOpacity=" << msg.value("centerOpacity", "")
                       << " viewport=" << msg.value("innerWidth", 0) << "x" << msg.value("innerHeight", 0)
                       << " href=" << msg.value("href", "");
                WindowChromeTrace::EmitAuxiliaryLine(stream.str());
                return;
            }
        } catch (...) {
            // JSON 解析失败，继续交给 BridgeCore
        }
    }
    // 使用单例 BridgeCore 处理消息（API 都注册在单例上）
    // 响应发送到当前实例的 WebViewHost，同时注入调用者 HWND
    BridgeCore::GetInstance().HandleMessage(json, webView_.get(), hwnd_);
}

void WebViewPanel::OnSetFocus() {
    hasFocus_ = true;
    if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("WebViewPanel::OnSetFocus");
    
    // grabFocus 检查：禁用时不获取 selection，也不触发事件
    if (!panelConfig_.grabFocus) {
        return;
    }
    
    // Acquiring selection holder
    if (selectionHolder_) {
        selectionHolder_->Acquire();
    }
    
    // Broadcasting panel:focus 事件
    if (IsWebViewReady() && bridge_) {
        api::emit::Emit<api::panel::events::Focus>(*bridge_, {});
    }
}

void WebViewPanel::OnKillFocus() {
    hasFocus_ = false;
    if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("WebViewPanel::OnKillFocus");
    
    // grabFocus 检查：禁用时不释放 selection，也不触发事件
    if (!panelConfig_.grabFocus) {
        return;
    }
    
    // Releasing selection holder
    if (selectionHolder_) {
        selectionHolder_->Release();
    }
    
    // Broadcasting panel:blur 事件
    if (IsWebViewReady() && bridge_) {
        api::emit::Emit<api::panel::events::Blur>(*bridge_, {});
    }
}

void WebViewPanel::OnNavigationCompleted(bool /*success*/) {
    // 默认空实现；Standalone 窗口重写用于启动可见性收敛
}

void WebViewPanel::OnWebViewProcessFailed(COREWEBVIEW2_PROCESS_FAILED_KIND failedKind, bool recovered) {
    // A crash the host could not self-heal leaves every COM pointer non-null
    // but permanently unusable (ERROR_INVALID_STATE on each call). Mark the
    // panel so IsWebViewReady() starts rejecting, and drop the WebViewContext
    // registration so broadcasts stop targeting a dead instance. Subclasses
    // may still rebuild afterwards; RebuildWebView* clears the flag on success.
    //
    // `!recovered` is the panel-visible projection of
    // webview_crash_policy::Disposition::markPanelDead -- the policy keeps the
    // two mutually exclusive in every branch (see
    // WebViewCrashPolicyTest.RecoveredAndMarkDeadAreMutuallyExclusive), so the
    // host does not need a second callback parameter to carry it.
    if (!recovered) {
        webViewProcessDead_ = true;
        if (hwnd_) {
            WebViewContext::GetInstance().UnregisterInstance(hwnd_);
            // 页面已不可用，它打开的端口不再有人接收。
            PortHub::Instance().CleanupPagePorts(hwnd_);
        }
    }

    // Base-class default: broadcast `webview:processFailed` so every window
    // can react. Subclasses that override this method MUST call
    // WebViewPanel::OnWebViewProcessFailed(failedKind, recovered) before
    // their custom logic, otherwise the broadcast is skipped. Broadcast
    // failures must not block crash handling -- detailed diagnostics are
    // already written to profile://webview_crash.log by WriteCrashLog().
    try {
        api::webview::ProcessFailedPayload payload;
        payload.kind = panel_crash::FailedKindToString(failedKind);
        payload.kindRaw = static_cast<int>(failedKind);
        payload.recovered = recovered;
        payload.recoveryAction = panel_crash::RecoveryActionFor(failedKind);
        api::emit::Broadcast<api::webview::events::ProcessFailed>(payload);
    } catch (...) {}
}

void WebViewPanel::OnWebViewInitFailed() {
    // 默认空实现；持有重建状态的子类必须重写以释放重建锁，
    // 否则一次异步创建失败会让后续重建被永久堵死。
}

void WebViewPanel::OnWindowReadySignal(const std::string& source) {
    (void)source;
    // 默认空实现；Standalone 窗口重写用于启动可见性收敛
}

void WebViewPanel::OnVisualReadySignal(const std::string& source) {
    (void)source;
}

frontend_directory_policy::Resolution WebViewPanel::ResolveFrontendDirectory() const {
    auto resolution = frontend_directory::Resolve(panelConfig_.templateName);
    if (!panelConfig_.templateName.empty() &&
        resolution.source != frontend_directory_policy::Source::PanelTemplate) {
        LOG("WebViewPanel::ResolveFrontendDirectory - Panel template '", panelConfig_.templateName.c_str(),
            "' has no index.html, falling back");
    }
    return resolution;
}

WebViewPanel::FrontendOrigin WebViewPanel::LocalFrontendOrigin(
    const frontend_directory_policy::Resolution& resolution) const {
    FrontendOrigin origin;
    origin.kind = FrontendOrigin::Kind::Directory;
    origin.directorySource = resolution.source;
    origin.directory = resolution.directory;
    origin.templateName = frontend_directory::TemplateNameOf(resolution.source, panelConfig_.templateName);
    return origin;
}

std::wstring WebViewPanel::GetTestPageHtml() const {
    // 返回简单的测试页面
    return LR"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>WebView Panel</title>
    <style>
        body {
            font-family: 'Segoe UI', sans-serif;
            display: flex;
            align-items: center;
            justify-content: center;
            height: 100vh;
            margin: 0;
            background: transparent;
            color: #fff;
        }
        .container {
            text-align: center;
            padding: 20px;
        }
        h1 { font-size: 24px; margin-bottom: 10px; }
        p { font-size: 14px; opacity: 0.7; }
    </style>
</head>
<body>
    <div class="container">
        <h1>WebView Panel</h1>
        <p>Frontend not found. Please install a template.</p>
    </div>
</body>
</html>)";
}

// ============================================
// 辅助方法实现
// ============================================

void WebViewPanel::ReloadFrontend() {
    if (!webViewReady_ || !IsWebViewOperable()) {
        LOG("WebViewPanel::ReloadFrontend - WebView not ready, skipping");
        return;
    }
    
    LOG("WebViewPanel::ReloadFrontend - Reloading with template: ", 
        panelConfig_.templateName.empty() ? "(global)" : panelConfig_.templateName.c_str());
    
    // 重新执行前端加载流程，按当前配置重新解析前端目录
    LoadFrontendPage();
}

void WebViewPanel::ApplyConfig(const PanelConfig& oldCfg, const PanelConfig& newCfg) {
    if (!webViewReady_ || !IsWebViewOperable()) {
        LOG("WebViewPanel::ApplyConfig - WebView not ready, skipping");
        return;
    }
    
    bool needNavigate = false;
    
    // 1. 边框样式变更
    if (oldCfg.edgeStyle != newCfg.edgeStyle) {
        LOG("WebViewPanel::ApplyConfig - Edge style changed: ", (int)oldCfg.edgeStyle, " -> ", (int)newCfg.edgeStyle);
        DWORD exStyle = GetWindowLongW(hwnd_, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
        if (newCfg.edgeStyle == 1) exStyle |= WS_EX_CLIENTEDGE;   // Sunken
        if (newCfg.edgeStyle == 2) exStyle |= WS_EX_STATICEDGE;   // Grey
        SetWindowLongW(hwnd_, GWL_EXSTYLE, exStyle);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    
    // 2. 透明背景变更
    if (oldCfg.transparentBackground != newCfg.transparentBackground) {
        LOG("WebViewPanel::ApplyConfig - Transparent background changed: ", 
            oldCfg.transparentBackground ? "true" : "false", " -> ", 
            newCfg.transparentBackground ? "true" : "false");
        webView_->SetBackgroundTransparent(newCfg.transparentBackground);
    }
    
    // 3. 焦点策略变更（运行时读取 panelConfig_.grabFocus，无需额外操作）
    if (oldCfg.grabFocus != newCfg.grabFocus) {
        LOG("WebViewPanel::ApplyConfig - Grab focus changed: ", 
            oldCfg.grabFocus ? "true" : "false", " -> ", 
            newCfg.grabFocus ? "true" : "false");
    }
    
    // 4. 拖放开关变更
    if (oldCfg.enableDragDrop != newCfg.enableDragDrop) {
        LOG("WebViewPanel::ApplyConfig - Drag drop changed: ", 
            oldCfg.enableDragDrop ? "true" : "false", " -> ", 
            newCfg.enableDragDrop ? "true" : "false");
        // 拖放开关变更目前仅记录日志；注入/移除全局 drop 拦截脚本尚未实现。
    }
    
    // 5. DevTools 开关变更
    if (oldCfg.enableDevTools != newCfg.enableDevTools) {
        LOG("WebViewPanel::ApplyConfig - DevTools changed: ", 
            oldCfg.enableDevTools ? "true" : "false", " -> ", 
            newCfg.enableDevTools ? "true" : "false");
        if (auto* host = GetWebView()) {
            if (auto* wv = host->GetWebView()) {
                wil::com_ptr<ICoreWebView2Settings> settings;
                if (SUCCEEDED(wv->get_Settings(&settings)) && settings) {
                    settings->put_AreDevToolsEnabled(
                        newCfg.enableDevTools || security_config::IsDevToolsEnabled());
                }
            }
        }
    }
    
    // 6. 模板或 URL 变更 → 需要重新导航
    if (oldCfg.templateName != newCfg.templateName ||
        oldCfg.urlOverride != newCfg.urlOverride) {
        LOG("WebViewPanel::ApplyConfig - Template/URL changed, will reload");
        needNavigate = true;
    }
    
    // 7. 重新导航（如需要）
    if (needNavigate) {
        LoadFrontendPage();
    }
    
    // 8. 通知前端配置变更
    if (bridge_) {
        api::panel::ConfigChangedPayload config;
        FillPanelConfig(newCfg, config);
        api::emit::Emit<api::panel::events::ConfigChanged>(*bridge_, config);
    }
    
    LOG("WebViewPanel::ApplyConfig completed");
}

void WebViewPanel::LoadFrontendPage() {
    LOG("Loading frontend page...");

    devServerNavPending_ = false;

    // 检查是否使用开发服务器
    if (security_config::UseDevServer()) {
        const char* devServerUrl = security_config::GetDevServerUrl();
        if (devServerUrl && devServerUrl[0]) {
            std::wstring wDevUrl = pfc::stringcvt::string_wide_from_utf8(devServerUrl).get_ptr();
            LOG("Navigating to dev server: ", devServerUrl);
            // 宿主自己导航过去的来源才可信；登记在导航之前，页面第一条消息就能通过。
            if (webView_) {
                webView_->AddTrustedOrigin(wDevUrl);
            }

            if (Navigate(wDevUrl)) {
                // 连不上时 HandleNavigationCompleted 回退到本地页面，那次提交会改写这份记录
                frontendOrigin_ = FrontendOrigin{};
                frontendOrigin_.kind = FrontendOrigin::Kind::DevServer;
                frontendOrigin_.url = wDevUrl;
                // 独立窗口模式下自动打开 DevTools
                if (mode_ == WebViewPanelMode::Standalone && webView_) {
                    webView_->OpenDevTools();
                }
                // 成败要等 NavigationCompleted; 回调据此标记回退到本地资源.
                devServerNavPending_ = true;
                LOG("Frontend page load initiated (dev server)");
                return;
            }
        }
    }

    if (!LoadFallbackFrontendPage()) {
        LOG("ERROR: no frontend page could be submitted for navigation");
    }
}

void WebViewPanel::HandleNavigationCompleted(bool success) {
    // 开发服务器导航失败在此回退。回退成功后本函数会被再触发一次，由那一次
    // 决定启动可见性；回退连提交都失败才继续往下宣告导航结束。
    // 被后来的导航取消不算连不上：页面自己跳转或换成内联页面时不能回退把它冲掉，
    // 取消它的那次导航会自己报完成。
    if (!success && devServerNavPending_) {
        devServerNavPending_ = false;
        if (webView_ && webView_->LastNavigationSuperseded()) {
            return;
        }
        LOG("Dev server navigation failed; falling back to local frontend");
        if (LoadFallbackFrontendPage()) {
            return;
        }
    }
    devServerNavPending_ = false;

    // Re-evaluate the path gate here rather than inside the virtual
    // OnNavigationCompleted: MainWindow and PopupWindow both override it,
    // so a virtual hook would skip them. The gate cannot be decided once at
    // registration time, because registration happens before the first
    // navigation and the document origin is not known yet.
    if (success && dndRegistrar_) {
        dndRegistrar_->ApplyOriginGate(webView_.get());
    }

    OnNavigationCompleted(success);
}

bool WebViewPanel::LoadFallbackFrontendPage() {
    if (!panelConfig_.urlOverride.empty()) {
        // URL 覆盖检查（优先级高于模板）
        std::wstring wUrl = pfc::stringcvt::string_wide_from_utf8(
            panelConfig_.urlOverride.c_str()).get_ptr();
        LOG("Navigating to URL override: ", panelConfig_.urlOverride.c_str());
        // 显式登记到 origin 白名单, 否则该来源的 invoke 会被
        // WebViewHost::IsOriginAllowed 静默早退, 表现为 30s 超时.
        if (webView_) {
            webView_->AddTrustedOrigin(wUrl);
        }
        if (Navigate(wUrl)) {
            frontendOrigin_ = FrontendOrigin{};
            frontendOrigin_.kind = FrontendOrigin::Kind::Url;
            frontendOrigin_.url = wUrl;
            LOG("Frontend page load initiated (URL override)");
            return true;
        }
    }

    if (NavigateToLocalFrontend()) {
        LOG("Frontend page load initiated (local resources)");
        return true;
    }

    // 加载内嵌测试页面
    LOG("Loading embedded test page");
    if (!NavigateToString(GetTestPageHtml())) return false;
    frontendOrigin_ = FrontendOrigin{};
    frontendOrigin_.kind = FrontendOrigin::Kind::BuiltInPage;
    return true;
}

bool WebViewPanel::NavigateToLocalFrontend() {
    const auto resolution = ResolveFrontendDirectory();
    if (resolution.source == frontend_directory_policy::Source::None ||
        !SetupVirtualHostMapping(resolution.directory)) {
        return false;
    }
    const std::wstring url = std::wstring(L"https://") + GetVirtualHostName() + L"/index.html";
    if (!Navigate(url)) return false;
    frontendOrigin_ = LocalFrontendOrigin(resolution);
    return true;
}

bool WebViewPanel::SetupVirtualHostMapping(const std::wstring& resourcesDir) {
    if (!webView_ || resourcesDir.empty()) {
        return false;
    }
    
    std::wstring indexPath = resourcesDir + L"\\index.html";
    DWORD attrs = GetFileAttributesW(indexPath.c_str());
    
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }
    
    HRESULT hr = webView_->SetVirtualHostMapping(GetVirtualHostName(), resourcesDir);
    return SUCCEEDED(hr);
}

