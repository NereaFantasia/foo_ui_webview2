/**
 * WebViewPanel - WebView 面板基类实现
 */
#include "pch.h"
#include "core/WebViewPanel.h"
#include "core/WebViewContext.h"
#include "core/SecurityConfig.h"
#include "core/PreferencesPage.h"
#include "webview/WebViewHost.h"
#include "api/BridgeCore.h"
#include "window/WindowChromeTrace.h"

// API 注册函数声明
#include "api/PlaybackApi.h"
#include "api/ConfigApi.h"
#include "api/PlaylistApi.h"
#include "api/LibraryApi.h"
#include "api/WindowApi.h"
#include "api/ArtworkApi.h"
#include "api/FileApi.h"
#include "api/DialogApi.h"
#include "api/ClipboardApi.h"
#include "api/ShellApi.h"
#include "api/HttpApi.h"
#include "api/KeyboardApi.h"
#include "api/UiApi.h"
#include "api/CursorApi.h"
#include "api/LyricsApi.h"
#include "api/MetadataApi.h"
#include "api/AudioApi.h"
#include "api/ConsoleApi.h"
#include "api/MiscApi.h"
#include "api/MenuApi.h"
#include "api/DndApi.h"
#include "api/QueueApi.h"
#include "api/DiscoveryApi.h"
#include "api/ReplayGainApi.h"
#include "api/PlaycountApi.h"
#include "api/TitleformatApi.h"
#include "api/SelectionApi.h"
#include "api/TrayApi.h"
#include "api/TaskbarApi.h"
#include "api/PortApi.h"
#include "api/PluginRegistry.h"
// 回调Initializing函数声明
#include "callbacks/PlaybackCallback.h"
#include "callbacks/PlaylistCallback.h"
#include "callbacks/LibraryCallback.h"
#include "callbacks/MetadbCallback.h"

// Selection Watcher
#include "selection/SelectionWatcher.h"
#include "selection/SelectionHolder.h"

// ============================================
// 辅助函数：获取组件所在目录
// ============================================
static std::wstring GetComponentDirectory() {
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
    
    // Set focus change callback for panel:focus/blur events
    webView_->SetFocusChangedCallback([this](bool gotFocus) {
        if (gotFocus) {
            OnSetFocus();
        } else {
            OnKillFocus();
        }
    });

    // 导航完成回调（启动可见性收敛用）
    webView_->SetNavigationCompletedCallback([this](bool success) {
        OnNavigationCompleted(success);
    });

    // 进程崩溃回调（Visual Hosting 模式下进程崩溃只剩空窗口的诊断/恢复入口）
    webView_->SetProcessFailedCallback(
        [this](COREWEBVIEW2_PROCESS_FAILED_KIND kind, bool recovered) {
            OnWebViewProcessFailed(kind, recovered);
        });

    bool useVisualHosting = (mode_ == WebViewPanelMode::Standalone);
    HRESULT hr = webView_->Initialize(hwnd_, [this](bool success) {
        if (success) {
            webViewReady_ = true;
            
            // 注册到 WebViewContext（带 windowId 支持多窗口系统）
            if (!windowId_.empty()) {
                WebViewContext::GetInstance().RegisterInstance(hwnd_, webView_.get(), bridge_.get(), windowId_);
            } else {
                WebViewContext::GetInstance().RegisterInstance(hwnd_, webView_.get(), bridge_.get());
            }
            
            // 调用虚函数，允许子类添加额外Initializing
            OnWebViewReady();
        } else {
            LOG("ERROR: WebView2 initialization failed");
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

void WebViewPanel::DestroyWebView() {
    // Unregister from SelectionWatcher first
    SelectionWatcher::GetInstance().UnregisterPanel(this);

    if (webView_) {
        // 在销毁窗口前先断开宿主回调，避免启动隐藏阶段的延迟消息回灌到已关闭窗口。
        webView_->SetMessageHandler({});
        webView_->SetNavigationCompletedCallback({});
        webView_->SetFocusChangedCallback({});
        webView_->SetProcessFailedCallback({});
    }
    
    if (hwnd_ && webViewReady_) {
        // Unregister from WebViewContext
        WebViewContext::GetInstance().UnregisterInstance(hwnd_);
    }
    
    webView_.reset();
    bridge_.reset();
    webViewReady_ = false;
    hwnd_ = nullptr;
}

void WebViewPanel::ResizeWebView() {
    if (!webView_ || !webView_->IsReady() || !hwnd_) {
        return;
    }
    
    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    webView_->Resize(clientRect);
}

bool WebViewPanel::Navigate(const std::wstring& url) {
    if (!webView_ || !webView_->IsReady()) {
        return false;
    }
    return SUCCEEDED(webView_->Navigate(url));
}

bool WebViewPanel::NavigateToString(const std::wstring& html) {
    if (!webView_ || !webView_->IsReady()) {
        return false;
    }
    return SUCCEEDED(webView_->NavigateToString(html));
}

void WebViewPanel::Reload() {
    if (webView_ && webView_->IsReady()) {
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
        json data = json::object();
        bridge_->EmitEvent("panel:focus", data);
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
        json data = json::object();
        bridge_->EmitEvent("panel:blur", data);
    }
}

void WebViewPanel::OnNavigationCompleted(bool /*success*/) {
    // 默认空实现；Standalone 窗口重写用于启动可见性收敛
}

namespace {
    // Map COREWEBVIEW2_PROCESS_FAILED_KIND -> JS-friendly camelCase string.
    // Used as the `kind` field of the `webview:processFailed` event payload.
    const char* FailedKindToString(COREWEBVIEW2_PROCESS_FAILED_KIND kind) {
        switch (kind) {
            case COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED:        return "browserProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED:         return "renderProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE:   return "renderProcessUnresponsive";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED:   return "frameRenderProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_UTILITY_PROCESS_EXITED:        return "utilityProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_SANDBOX_HELPER_PROCESS_EXITED: return "sandboxHelperProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_GPU_PROCESS_EXITED:            return "gpuProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_PLUGIN_PROCESS_EXITED:   return "ppapiPluginProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_BROKER_PROCESS_EXITED:   return "ppapiBrokerProcessExited";
            default: return "unknownProcessExited";
        }
    }

    // Mirror the tiered handling in WebViewHost::SetupProcessFailedHandling:
    //   render-process kinds  -> "reload"       (Reload() was attempted, see `recovered`)
    //   browser-process exit  -> "needRebuild"  (entire WebView is dead, host must rebuild)
    //   others (GPU/utility)  -> "none"         (runtime self-heals, just observe)
    const char* RecoveryActionFor(COREWEBVIEW2_PROCESS_FAILED_KIND kind) {
        switch (kind) {
            case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED:
            case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE:
            case COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED:
                return "reload";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED:
                return "needRebuild";
            default:
                return "none";
        }
    }
}  // namespace

void WebViewPanel::OnWebViewProcessFailed(COREWEBVIEW2_PROCESS_FAILED_KIND failedKind, bool recovered) {
    // Base-class default: broadcast `webview:processFailed` so every window
    // can react. Subclasses that override this method MUST call
    // WebViewPanel::OnWebViewProcessFailed(failedKind, recovered) before
    // their custom logic, otherwise the broadcast is skipped. Broadcast
    // failures must not block crash handling -- detailed diagnostics are
    // already written to profile://webview_crash.log by WriteCrashLog().
    try {
        json payload = {
            {"kind", FailedKindToString(failedKind)},
            {"kindRaw", static_cast<int>(failedKind)},
            {"recovered", recovered},
            {"recoveryAction", RecoveryActionFor(failedKind)}
        };
        WebViewContext::GetInstance().BroadcastEvent("webview:processFailed", payload);
    } catch (...) {}
}

void WebViewPanel::OnWindowReadySignal(const std::string& source) {
    (void)source;
    // 默认空实现；Standalone 窗口重写用于启动可见性收敛
}

void WebViewPanel::OnVisualReadySignal(const std::string& source) {
    (void)source;
}

std::wstring WebViewPanel::GetFrontendResourcesDir() const {
    // 0. 面板级别模板覆盖：优先使用面板自己的 templateName
    if (!panelConfig_.templateName.empty()) {
        std::wstring baseDir = webview_prefs::GetWebResourcesBaseDir();
        std::wstring panelDir = baseDir + L"\\" + 
            pfc::stringcvt::string_wide_from_utf8(panelConfig_.templateName.c_str()).get_ptr();
        std::wstring indexPath = panelDir + L"\\index.html";
        DWORD attrs = GetFileAttributesW(indexPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return panelDir;
        }
        LOG("WebViewPanel::GetFrontendResourcesDir - Panel template '", panelConfig_.templateName.c_str(), "' not found, falling back to global");
    }
    
    // 1. 尝试从新的 profile 目录加载活动模板
    std::wstring profileDir = webview_prefs::GetActiveWebResourcesDir();
    if (!profileDir.empty()) {
        std::wstring indexPath = profileDir + L"\\index.html";
        DWORD attrs = GetFileAttributesW(indexPath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return profileDir;
        }
    }
    
    // 2. 回退到旧的组件目录
    std::wstring componentDir = GetComponentDirectory();
    if (!componentDir.empty()) {
        std::wstring resourcesDir = componentDir + L"\\foo_ui_webview2_resources\\dist";
        DWORD attrs = GetFileAttributesW(resourcesDir.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            return resourcesDir;
        }
    }
    
    // 3. 最后的兜底：直接使用 profile/webview-ui/default
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
            return defaultDir;
        }
    } catch (...) {
        // ignore
    }

    return L"";
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

void WebViewPanel::RegisterAllApis() {
    LOG("Registering all APIs...");
    
    RegisterPlaybackApi();
    RegisterConfigApi();
    RegisterPlaylistApi();
    RegisterLibraryApi();
    RegisterWindowApi();
    RegisterArtworkApi();
    RegisterFileApi();
    RegisterDialogApi();
    RegisterClipboardApi();
    RegisterShellApi();
    RegisterHttpApi();
    RegisterKeyboardApi();
    RegisterUiApi();
    RegisterCursorApi();
    RegisterLyricsApi();
    RegisterMetadataApi();
    RegisterAudioApi();
    RegisterConsoleApi();
    RegisterMiscApi();
    RegisterMenuApi();
    RegisterDndApi();
    RegisterQueueApi();
    
    // JIT Queue API
    InitializeJitQueue();
    RegisterJitQueueApi();
    
    // Plugin Registry
    PluginRegistry::GetInstance().Initialize();
    
    // Discovery API
    discovery_api::RegisterApis();
    
    // ReplayGain API
    RegisterReplayGainApi();
    
    // Playcount API
    RegisterPlaycountApi();
    
    // Titleformat API
    RegisterTitleformatApi();
    
    // Selection API
    RegisterSelectionApi();
    
    // Port/Event/State API (PortHub)
    RegisterPortApi();
    
    // Tray & Taskbar APIs
    RegisterTrayApi();
    RegisterTaskbarApi();

    LOG("All APIs registered");
}

void WebViewPanel::InitializeCallbacks() {
    LOG("Initializing callbacks...");
    
    InitPlaybackCallbacks();
    InitPlaylistCallbacks();
    InitLibraryCallbacks();
    InitMetadbCallbacks();
    
    LOG("Callbacks initialized");
}

void WebViewPanel::ReloadFrontend() {
    if (!webViewReady_ || !webView_ || !webView_->IsReady()) {
        LOG("WebViewPanel::ReloadFrontend - WebView not ready, skipping");
        return;
    }
    
    LOG("WebViewPanel::ReloadFrontend - Reloading with template: ", 
        panelConfig_.templateName.empty() ? "(global)" : panelConfig_.templateName.c_str());
    
    // 重新执行前端加载流程（会使用新的 GetFrontendResourcesDir 结果）
    LoadFrontendPage();
}

void WebViewPanel::ApplyConfig(const PanelConfig& oldCfg, const PanelConfig& newCfg) {
    if (!webViewReady_ || !webView_ || !webView_->IsReady()) {
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
        json data = {
            {"panelName", newCfg.panelName},
            {"templateName", newCfg.templateName},
            {"edgeStyle", newCfg.edgeStyle},
            {"transparentBackground", newCfg.transparentBackground},
            {"grabFocus", newCfg.grabFocus},
            {"enableDragDrop", newCfg.enableDragDrop},
            {"enableDevTools", newCfg.enableDevTools},
            {"urlOverride", newCfg.urlOverride}
        };
        bridge_->EmitEvent("panel:configChanged", data);
    }
    
    LOG("WebViewPanel::ApplyConfig completed");
}

void WebViewPanel::LoadFrontendPage() {
    LOG("Loading frontend page...");
    
    // 检查是否使用开发服务器
    bool useDevServer = security_config::UseDevServer();
    bool navigated = false;
    
    if (useDevServer) {
        const char* devServerUrl = security_config::GetDevServerUrl();
        if (devServerUrl && devServerUrl[0]) {
            std::wstring wDevUrl = pfc::stringcvt::string_wide_from_utf8(devServerUrl).get_ptr();
            LOG("Navigating to dev server: ", devServerUrl);
            
            if (Navigate(wDevUrl)) {
                // 独立窗口模式下自动打开 DevTools
                if (mode_ == WebViewPanelMode::Standalone && webView_) {
                    webView_->OpenDevTools();
                }
                navigated = true;
            }
        }
    }
    
    if (!navigated && !panelConfig_.urlOverride.empty()) {
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
            navigated = true;
        }
    }
    
    if (!navigated) {
        std::wstring resourcesDir = GetFrontendResourcesDir();
        
        if (!resourcesDir.empty() && SetupVirtualHostMapping(resourcesDir)) {
            std::wstring url = std::wstring(L"https://") + GetVirtualHostName() + L"/index.html";
            if (Navigate(url)) {
                navigated = true;
            }
        }
        
        if (!navigated) {
            // 加载内嵌测试页面
            LOG("Loading embedded test page");
            NavigateToString(GetTestPageHtml());
        }
    }
    
    LOG("Frontend page load ", navigated ? "initiated" : "fallback to test page");
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

