#include "pch.h"
#include "webview/WebViewHost.h"
#include "webview/WebViewEnvironment.h"
#include "webview/SdkBridgeScript.inl"
#include "window/WindowChromeTrace.h"
#include "api/ArtworkApi.h"
#include "utils/ArtworkCacheKey.h"
#include "utils/ImageUtils.h"
#include <Shlobj.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <dcomp.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <objidl.h>    // IStream, CreateStreamOnHGlobal
#include <foobar2000/SDK/menu_helpers.h>  // For standard_commands
#include <vector>
#include <algorithm>
#include <string>
#include <functional>  // std::hash for ETag generation
#include <atomic>      // std::atomic for thread-safe ref counting
#include <unordered_map>  // scaled artwork cache
#include <fstream>     // 崩溃诊断日志落盘 (webview_crash.log)
#include <ctime>       // 崩溃日志时间戳
#include <WebView2EnvironmentOptions.h>  // SDK 提供的环境选项辅助类

#pragma comment(lib, "dcomp.lib")

using namespace Microsoft::WRL;

// ==========================================================================
// 自定义协议注册
// 使用 WebView2 SDK 提供的 CoreWebView2EnvironmentOptions 和
// CoreWebView2CustomSchemeRegistration 辅助类（来自 WebView2EnvironmentOptions.h）
// 这些类正确实现了所有需要的 COM 接口 (Options1-8)
// ==========================================================================

WebViewHost::~WebViewHost() {
    try {
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

std::wstring WebViewHost::GetUserDataPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path))) {
        std::wstring userDataPath = std::wstring(path) + L"\\foobar2000\\foo_ui_webview2";
        
        // 确保目录存在
        (void)CreateDirectoryW((std::wstring(path) + L"\\foobar2000").c_str(), nullptr);
        (void)CreateDirectoryW(userDataPath.c_str(), nullptr);
        
        return userDataPath;
    }
    return L"";
}

HRESULT WebViewHost::Initialize(HWND parentHwnd, InitCallback callback, bool useVisualHosting) {
    parentHwnd_ = parentHwnd;
    useVisualHosting_ = useVisualHosting;
    
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
        [this, callback, dcompHr](ICoreWebView2Environment* env) {
            if (!env) {
                LOG("Failed to get WebView2 environment");
                if (callback) callback(false);
                return;
            }
            
            environment_ = env;
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
                [this, callback](HRESULT result, ICoreWebView2CompositionController* compositionController) -> HRESULT {
                    
                    if (FAILED(result) || !compositionController) {
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
                    
                    controller_->put_IsVisible(TRUE);
                    controller_->get_CoreWebView2(&webview_);
                    
                    // Setup Visual Hosting connection
                    if (dcompWebViewVisual_) {
                        hr = compositionController_->put_RootVisualTarget(dcompWebViewVisual_.get());
                        if (SUCCEEDED(hr)) {
                            dcompDevice_->Commit();
                            LOG("Visual Hosting: WebView connected to DirectComposition");
                        }
                    }
                    
                    // Setup cursor handling
                    SetupCursorHandling();
                    
                    SetupWebView();
                    SetupSettings();
                    SetupCustomProtocol();
                    RegisterMessageHandler();
                    SetupProcessFailedHandling();
                    InjectBridgeScript();
                    InjectSdkBridgeScript();
                    
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
            [this, callback](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                
                if (FAILED(result) || !controller) {
                    LOG("Failed to create WebView2 controller, HRESULT: ", pfc::format_hex((uint32_t)result));
                    if (callback) callback(false);
                    return result;
                }
                
                controller_ = controller;
                controller_->put_IsVisible(TRUE);
                controller->get_CoreWebView2(&webview_);
                
                SetupWebView();
                SetupSettings();
                SetupCustomProtocol();
                RegisterMessageHandler();
                SetupProcessFailedHandling();
                InjectBridgeScript();
                InjectSdkBridgeScript();
                
                LOG("WebView2 initialized successfully (standard mode)");
                if (callback) callback(true);
                
                return S_OK;
            }
        ).Get()
    );
}

void WebViewHost::SetupWebView() {
    // 设置边界 - 填满父窗口
    RECT bounds;
    GetClientRect(parentHwnd_, &bounds);
    controller_->put_Bounds(bounds);
    
    // ============================================
    // 设置透明背景以支持 DWM 效果穿透
    // 这是实现 Mica/Acrylic 效果在 WebView 中显示的关键
    // ============================================
    
    // 方法 1: ICoreWebView2Controller2::put_DefaultBackgroundColor
    // Alpha = 0 表示完全透明，让 DWM 效果能够穿透 WebView 显示
    wil::com_ptr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(controller_->QueryInterface(IID_PPV_ARGS(&controller2)))) {
        // 使用透明背景 (Alpha = 0) - 核心设置
        COREWEBVIEW2_COLOR bgColor = { 0, 0, 0, 0 };
        HRESULT hr = controller2->put_DefaultBackgroundColor(bgColor);
        if (SUCCEEDED(hr)) {
            isBackgroundTransparent_ = true;  // 标记透明状态，Resize 时需要重新应用
            if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("✓ WebView background set to TRANSPARENT (Alpha=0) for DWM passthrough");
        } else {
            LOG("⚠ Failed to set transparent background, HRESULT: ", pfc::format_hex((uint32_t)hr));
        }
    } else {
        LOG("⚠ ICoreWebView2Controller2 not available, DWM effects may not work");
    }
    
    // 方法 2: 尝试使用 ICoreWebView2Controller4（更新的 API）
    wil::com_ptr<ICoreWebView2Controller4> controller4;
    if (SUCCEEDED(controller_->QueryInterface(IID_PPV_ARGS(&controller4)))) {
        // 允许使用分层窗口（可能有助于透明效果）
        // 注意：这个设置可能不是所有版本都有
        LOG("✓ ICoreWebView2Controller4 available");
    }

    // Register focus events for panel integration
    controller_->add_GotFocus(
        Callback<ICoreWebView2FocusChangedEventHandler>(
            [this](ICoreWebView2Controller* sender, IUnknown* /*args*/) -> HRESULT {
                RECT bounds{};
                const bool hasBounds = sender && SUCCEEDED(sender->get_Bounds(&bounds));
                std::ostringstream stream;
                stream << "[ActivationEvidence] source=WebViewHost::GotFocus"
                       << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " parentHwnd=0x" << pfc::format_hex((size_t)parentHwnd_)
                       << " foregroundHwnd=0x" << pfc::format_hex((size_t)::GetForegroundWindow())
                       << " hasBounds=" << WindowChromeTrace::BoolText(hasBounds)
                       << " controllerBounds=" << (hasBounds
                           ? (std::string("(") + std::to_string(bounds.left) + "," +
                              std::to_string(bounds.top) + "," +
                              std::to_string(bounds.right) + "," +
                              std::to_string(bounds.bottom) + ")").c_str()
                           : "N/A")
                       << " parentVisible=" << WindowChromeTrace::BoolText(parentHwnd_ && IsWindowVisible(parentHwnd_) != FALSE);
                WindowChromeTrace::EmitAuxiliaryLine(stream.str());
                if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("WebViewHost: GotFocus");
                if (focusChangedCallback_) focusChangedCallback_(true);
                return S_OK;
            }).Get(),
        &gotFocusToken_);

    controller_->add_LostFocus(
        Callback<ICoreWebView2FocusChangedEventHandler>(
            [this](ICoreWebView2Controller* sender, IUnknown* /*args*/) -> HRESULT {
                RECT bounds{};
                const bool hasBounds = sender && SUCCEEDED(sender->get_Bounds(&bounds));
                std::ostringstream stream;
                stream << "[ActivationEvidence] source=WebViewHost::LostFocus"
                       << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " parentHwnd=0x" << pfc::format_hex((size_t)parentHwnd_)
                       << " foregroundHwnd=0x" << pfc::format_hex((size_t)::GetForegroundWindow())
                       << " hasBounds=" << WindowChromeTrace::BoolText(hasBounds)
                       << " controllerBounds=" << (hasBounds
                           ? (std::string("(") + std::to_string(bounds.left) + "," +
                              std::to_string(bounds.top) + "," +
                              std::to_string(bounds.right) + "," +
                              std::to_string(bounds.bottom) + ")").c_str()
                           : "N/A")
                       << " parentVisible=" << WindowChromeTrace::BoolText(parentHwnd_ && IsWindowVisible(parentHwnd_) != FALSE);
                WindowChromeTrace::EmitAuxiliaryLine(stream.str());
                if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("WebViewHost: LostFocus");
                if (focusChangedCallback_) focusChangedCallback_(false);
                return S_OK;
            }).Get(),
        &lostFocusToken_);
}

// 安全: 引入安全配置
namespace security_config {
    extern bool IsDevToolsEnabled();
    extern bool UseDevServer();
    extern const char* GetDevServerUrl();
}

void WebViewHost::SetupSettings() {
    wil::com_ptr<ICoreWebView2Settings> settings;
    if (FAILED(webview_->get_Settings(&settings))) {
        LOG("Failed to get WebView2 settings");
        return;
    }
    
    // 基本设置
    settings->put_IsScriptEnabled(TRUE);
    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    settings->put_IsWebMessageEnabled(TRUE);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_AreDefaultContextMenusEnabled(FALSE);  // 禁用默认右键菜单
    settings->put_IsZoomControlEnabled(FALSE);           // 禁用缩放
    
    // 安全: 基于高级设置的 DevTools 控制
    bool enableDevTools = security_config::IsDevToolsEnabled();
    settings->put_AreDevToolsEnabled(enableDevTools ? TRUE : FALSE);
    LOG(enableDevTools ? "DevTools ENABLED (config/debug)" : "DevTools DISABLED (default)");
    
    // 更多设置 (需要更高版本接口)
    if (wil::com_ptr<ICoreWebView2Settings3> settings3;
        SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings3))) && !enableDevTools) {
        // 仅在禁用 DevTools 时禁用快捷键
        settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);  // 禁用 F5/Ctrl+R/F12 等
    }
    
    // 启用 Non-Client Region Support，让 -webkit-app-region CSS 属性生效
    // 这样前端可以使用 app-region: drag 来定义可拖拽区域
    wil::com_ptr<ICoreWebView2Settings9> settings9;
    if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings9)))) {
        settings9->put_IsNonClientRegionSupportEnabled(TRUE);
        LOG("Non-client region support ENABLED (app-region CSS)");
    } else {
        LOG("Warning: ICoreWebView2Settings9 not available, app-region CSS won't work");
    }
    
    // ============================================
    // 拦截快捷键，转发给 foobar2000 处理
    // 解决 Ctrl+P 等快捷键被 WebView 拦截的问题
    // ============================================
    controller_->add_AcceleratorKeyPressed(
        Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [](ICoreWebView2Controller* /*sender*/, ICoreWebView2AcceleratorKeyPressedEventArgs* args) {
                COREWEBVIEW2_KEY_EVENT_KIND kind;
                args->get_KeyEventKind(&kind);
                
                // 只处理 KeyDown 和 SystemKeyDown 事件
                if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN && 
                    kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) {
                    return S_OK;
                }
                
                UINT key;
                args->get_VirtualKey(&key);
                
                bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                
                // Ctrl+P (Preferences) - 直接调用 foobar2000 命令
                if (key == 'P' && isCtrl && !isAlt && !isShift) {
                    args->put_Handled(TRUE);
                    LOG("Intercepted Ctrl+P, opening foobar2000 Preferences");
                    standard_commands::run_main(standard_commands::guid_main_preferences);
                    return S_OK;
                }
                
                // 其他常用 foobar2000 快捷键
                // Ctrl+O (Open)
                if (key == 'O' && isCtrl && !isAlt && !isShift) {
                    args->put_Handled(TRUE);
                    LOG("Intercepted Ctrl+O, opening file dialog");
                    standard_commands::run_main(standard_commands::guid_main_open);
                    return S_OK;
                }
                
                // Ctrl+N (New Playlist)
                if (key == 'N' && isCtrl && !isAlt && !isShift) {
                    args->put_Handled(TRUE);
                    LOG("Intercepted Ctrl+N, creating new playlist");
                    standard_commands::run_main(standard_commands::guid_main_create_playlist);
                    return S_OK;
                }
                
                // 注意：不要拦截 Ctrl+C/V/X/A/Z 等编辑快捷键，让 WebView 处理
                
                return S_OK;
            }
        ).Get(),
        nullptr
    );
    LOG("AcceleratorKeyPressed handler registered for foobar2000 hotkeys");
    
    LOG("WebView2 settings configured");
}

//==========================================================================
// 安全修复: Origin 验证门禁
// 只允许受信任的来源调用 Bridge API
//==========================================================================

// 严格 origin 边界匹配: 命中前缀后, 下一个字符必须是分隔符,
// 防止 "https://app.local" 误放行 "https://app.local.attacker.com"
// 之类的同源域名扩展攻击 (URL 边界绕过 / userinfo 注入).
bool WebViewHost::OriginPrefixMatch(const std::wstring& origin, const std::wstring& prefix) {
    if (prefix.empty() || origin.size() < prefix.size()) return false;
    if (origin.compare(0, prefix.size(), prefix) != 0) return false;

    // 前缀本身已经覆盖到完整 origin (例: prefix 以 ':' 结尾匹配 "https://127.0.0.1:")
    // 那么下一个字符是端口数字也是合法的 origin 边界 (前缀语义即 "scheme + host[:portPrefix]")。
    // 前缀以可识别 origin 边界字符 (':' / '/') 结尾, 或恰好等于 origin 时也算合法。
    if (origin.size() == prefix.size()) return true;

    wchar_t lastPrefixCh = prefix.back();
    if (lastPrefixCh == L':' || lastPrefixCh == L'/') {
        return true;  // dev 列表里的 "https://127.0.0.1:" / "file://" 等
    }

    // 否则要求紧跟分隔符: '/' (path) / ':' (port) / '?' / '#' / 字符串结尾
    wchar_t next = origin[prefix.size()];
    return next == L'/' || next == L':' || next == L'?' || next == L'#';
}

// 从 URL 或 origin 字符串提取 origin (scheme://host[:port]).
// 输入示例:
//   "https://staging.example.com:8443/path?x=1#frag" -> "https://staging.example.com:8443"
//   "https://app.local"                                -> "https://app.local"
//   "http://localhost:5173"                            -> "http://localhost:5173"
// 任何无法解析的输入返回空串(将被忽略,不放行).
std::wstring WebViewHost::NormalizeToOrigin(const std::wstring& urlOrOrigin) {
    size_t schemeEnd = urlOrOrigin.find(L"://");
    if (schemeEnd == std::wstring::npos) return L"";

    size_t authorityStart = schemeEnd + 3;
    if (authorityStart >= urlOrOrigin.size()) return L"";

    // path / query / fragment 起点
    size_t end = urlOrOrigin.size();
    for (wchar_t terminator : { L'/', L'?', L'#' }) {
        size_t p = urlOrOrigin.find(terminator, authorityStart);
        if (p != std::wstring::npos && p < end) end = p;
    }

    // 不允许 userinfo (origin 不应包含 user@host; 若存在则丢弃以防绕过)
    std::wstring authority = urlOrOrigin.substr(authorityStart, end - authorityStart);
    size_t at = authority.find(L'@');
    if (at != std::wstring::npos) {
        authority = authority.substr(at + 1);
    }
    if (authority.empty()) return L"";

    return urlOrOrigin.substr(0, authorityStart) + authority;
}

void WebViewHost::AddTrustedOrigin(const std::wstring& urlOrOrigin) {
    std::wstring origin = NormalizeToOrigin(urlOrOrigin);
    if (origin.empty()) {
        LOG("AddTrustedOrigin: cannot parse origin from input, ignored");
        return;
    }

    std::lock_guard<std::mutex> lock(extraTrustedOriginsMutex_);
    for (const auto& existing : extraTrustedOrigins_) {
        if (existing == origin) return;  // 去重
    }
    extraTrustedOrigins_.push_back(origin);
    LOG("AddTrustedOrigin: ", pfc::stringcvt::string_utf8_from_wide(origin.c_str()).get_ptr());
}

bool WebViewHost::IsOriginAllowed(ICoreWebView2* webview) {
    if (!webview) return false;

    wil::unique_cotaskmem_string source;
    if (FAILED(webview->get_Source(&source)) || !source) {
        return false;
    }

    std::wstring origin = source.get();

    // 生产环境白名单
    static const std::vector<std::wstring> whitelist = {
        L"https://foo-ui-webview2.local",
        L"https://app.local",
        L"https://fb2k.local",
        L"about:blank",
    };

    for (const auto& allowed : whitelist) {
        if (OriginPrefixMatch(origin, allowed)) return true;
    }

    // 运行期注册的额外可信 origin (panel.urlOverride / popup http(s) URL).
    // 与 dev mode 解耦: 不论是否启用 dev server, 显式登记的 origin 都放行;
    // 注册责任在调用方 (WebViewPanel::LoadFrontendPage / PopupWindow 导航前).
    {
        std::lock_guard<std::mutex> lock(extraTrustedOriginsMutex_);
        for (const auto& trusted : extraTrustedOrigins_) {
            if (OriginPrefixMatch(origin, trusted)) return true;
        }
    }

    //   Dev URL 是运行期开关 (security_config::UseDevServer),
    //   而非编译期开关. Release 构建在用户开启 "Use dev server" 后
    //   也必须能从 Vite (https://127.0.0.1:5174 等) 收到 invoke 请求;
    //   否则事件能从 C++ 推送给 JS (PostWebMessageAsJson 不过滤),
    //   但 JS->C++ 的 invoke 全部在此被静默早退, 导致 30s 超时.
    //   仅当用户启用了 dev server 时才扩展白名单, 生产路径仍受锁.
    if (security_config::UseDevServer()) {
        static const std::vector<std::wstring> devlist = {
            L"http://localhost:",
            L"http://127.0.0.1:",
            L"http://[::1]:",
            L"https://localhost:",
            L"https://127.0.0.1:",
            L"https://[::1]:",
            L"file://",
        };
        for (const auto& d : devlist) {
            if (OriginPrefixMatch(origin, d)) return true;
        }

        // 用户配置的 dev server URL 也放行 (规范化为 origin 后边界匹配)
        if (const char* devUrlUtf8 = security_config::GetDevServerUrl();
            devUrlUtf8 && devUrlUtf8[0]) {
            std::wstring devUrlOrigin = NormalizeToOrigin(
                pfc::stringcvt::string_wide_from_utf8(devUrlUtf8).get_ptr());
            if (!devUrlOrigin.empty() && OriginPrefixMatch(origin, devUrlOrigin)) return true;
        }
    }

    LOG("\xe2\x9a\xa0\xef\xb8\x8f Blocked message from untrusted origin: ",
        pfc::stringcvt::string_utf8_from_wide(origin.c_str()).get_ptr());
    return false;
}

void WebViewHost::RegisterMessageHandler() {
    // 监听来自 JavaScript 的消息
    webview_->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                // 安全: Origin 验证门禁
                if (!IsOriginAllowed(sender)) {
                    return S_OK;
                }
                
                if (wil::unique_cotaskmem_string message;
                    SUCCEEDED(args->get_WebMessageAsJson(&message)) && message && messageHandler_) {
                    messageHandler_(message.get());
                }
                
                return S_OK;
            }
        ).Get(),
        nullptr
    );
    
    // 导航完成事件
    webview_->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2NavigationCompletedEventArgs* args) {
                BOOL success;
                args->get_IsSuccess(&success);
                
                if (success) {
                    LOG("Navigation completed successfully");
                } else {
                    COREWEBVIEW2_WEB_ERROR_STATUS status;
                    args->get_WebErrorStatus(&status);
                    LOG("Navigation failed, error status: ", (int)status);
                }

                if (navigationCompletedCallback_) {
                    navigationCompletedCallback_(success != FALSE);
                }
                
                return S_OK;
            }
        ).Get(),
        nullptr
    );
    
    LOG("Message handlers registered");
}


void WebViewHost::InjectBridgeScript() {
    // 注入全局 bridge 脚本，在所有页面加载前执行
    const wchar_t* bridgeScript = LR"(
        // foobar2000 WebView2 UI Bridge
        window.fb2k = window.fb2k || {
            _callbacks: new Map(),
            _callId: 0,
            
            // 发送请求到 C++
            invoke: function(method, params) {
                return new Promise((resolve, reject) => {
                    const id = ++this._callId;
                    this._callbacks.set(id, { resolve, reject });
                    
                    window.chrome.webview.postMessage({
                        type: 'invoke',
                        id: id,
                        method: method,
                        params: params || {}
                    });
                    
                    // 超时处理
                    setTimeout(() => {
                        if (this._callbacks.has(id)) {
                            this._callbacks.delete(id);
                            reject(new Error('Request timeout'));
                        }
                    }, 30000);
                });
            },
            
            // 接收 C++ 响应
            _handleResponse: function(id, error, result) {
                const callback = this._callbacks.get(id);
                if (callback) {
                    this._callbacks.delete(id);
                    if (error) {
                        callback.reject(new Error(error));
                    } else {
                        callback.resolve(result);
                    }
                }
            },
            
            // 事件监听
            _eventListeners: new Map(),
            
            on: function(event, callback) {
                if (!this._eventListeners.has(event)) {
                    this._eventListeners.set(event, new Set());
                }
                this._eventListeners.get(event).add(callback);
            },
            
            off: function(event, callback) {
                const listeners = this._eventListeners.get(event);
                if (listeners) {
                    listeners.delete(callback);
                }
            },
            
            _emit: function(event, data) {
                // 1. Call registered callbacks
                const listeners = this._eventListeners.get(event);
                if (listeners) {
                    listeners.forEach(cb => {
                        try { cb(data); } catch(e) { console.error(e); }
                    });
                }
                
                // 2. Dispatch CustomEvent for window.addEventListener
                try {
                    window.dispatchEvent(new CustomEvent('fb2k:' + event, {
                        detail: data,
                        bubbles: true
                    }));
                } catch(e) { console.error('CustomEvent dispatch error:', e); }
            }
        };
        
        // 监听来自 C++ 的消息
        window.chrome.webview.addEventListener('message', (e) => {
            const msg = e.data;
            if (msg.type === 'response') {
                window.fb2k._handleResponse(msg.id, msg.error, msg.result);
            } else if (msg.type === 'event') {
                window.fb2k._emit(msg.event, msg.data);
            }
        });
        
        // 全局右键菜单 - 仅在无组件处理时调用原生菜单
        document.addEventListener('contextmenu', (e) => {
            if (e.defaultPrevented) return;  // 组件已处理(playlist-view, library-tree等)
            e.preventDefault();
            window.fb2k.invoke('ui.showContextMenu', { x: e.screenX, y: e.screenY });
        });
        
        console.log('[fb2k] Bridge initialized');
    )";
    
    webview_->AddScriptToExecuteOnDocumentCreated(bridgeScript, nullptr);
    LOG("Bridge script injected");

    // Internal windowReady auto-signal + overlay-gone detection for visualReady (true reveal authority)
    const wchar_t* readyHelperScript = LR"(
        (function() {
            var _notified = false;
            var _scheduled = false;

            function postWindowReady(detail) {
                if (_notified) return;
                _notified = true;
                try {
                    window.chrome.webview.postMessage({
                        type: 'windowReady',
                        source: (detail && detail.source) || 'explicit',
                        href: location.href,
                        readyState: document.readyState
                    });
                } catch(e) {}
                // After windowReady, start overlay-gone detection → send visualReady
                startOverlayDetection();
            }

            function scheduleWindowReady(detail) {
                if (_notified || _scheduled) return;
                _scheduled = true;
                setTimeout(function() {
                    _scheduled = false;
                    postWindowReady(detail);
                }, 0);
            }

            // === overlay-gone detection -> send visualReady ===
            var _visualReadyNotified = false;

            function postVisualReady(source) {
                if (_visualReadyNotified) return;
                _visualReadyNotified = true;
                try {
                    window.chrome.webview.postMessage({
                        type: 'visualReady',
                        source: source,
                        href: location.href,
                        readyState: document.readyState
                    });
                } catch(e) {}
            }

            function isOverlayGone() {
                var overlay = document.getElementById('server-loading');
                if (!overlay) return true;
                if (!overlay.offsetWidth && !overlay.offsetHeight && overlay.offsetParent === null) return true;
                try {
                    var style = window.getComputedStyle(overlay);
                    if (style.display === 'none' || style.visibility === 'hidden') return true;
                } catch(e) {}
                return false;
            }

            function startOverlayDetection() {
                if (_visualReadyNotified) return;
                // Immediate check: themes without overlay pass immediately
                if (isOverlayGone()) {
                    postVisualReady('overlay-none');
                    return;
                }
                // MutationObserver for DOM removal / attribute changes
                var observer = new MutationObserver(function() {
                    if (isOverlayGone()) {
                        observer.disconnect();
                        clearInterval(pollId);
                        postVisualReady('overlay-removed');
                    }
                });
                observer.observe(document.documentElement, {
                    childList: true, subtree: true, attributes: true
                });
                // Polling fallback (100ms interval, in case MutationObserver misses style changes)
                var pollId = setInterval(function() {
                    if (isOverlayGone()) {
                        observer.disconnect();
                        clearInterval(pollId);
                        postVisualReady('overlay-poll');
                    }
                }, 100);
                // Safety timeout: 10s (coordinator fallback will fire earlier)
                setTimeout(function() {
                    if (!_visualReadyNotified) {
                        observer.disconnect();
                        clearInterval(pollId);
                        postVisualReady('overlay-timeout');
                    }
                }, 10000);
            }
    )" LR"(
            function roundNumber(value) {
                if (typeof value !== 'number' || !isFinite(value)) return -1;
                return Math.round(value * 1000) / 1000;
            }

            function cropText(value) {
                if (value == null) return '';
                value = String(value);
                return value.length > 120 ? value.slice(0, 117) + '...' : value;
            }

            function getPaintMetric(name) {
                try {
                    var entries = performance.getEntriesByType ? performance.getEntriesByType('paint') : [];
                    for (var i = 0; i < entries.length; i++) {
                        if (entries[i] && entries[i].name === name) {
                            return roundNumber(entries[i].startTime);
                        }
                    }
                } catch (e) {}
                return -1;
            }

            function getStyleSnapshot(element) {
                if (!element || !window.getComputedStyle) {
                    return { backgroundColor: '', backgroundImage: '', opacity: '', visibility: '', display: '' };
                }

                try {
                    var style = window.getComputedStyle(element);
                    return {
                        backgroundColor: cropText(style.backgroundColor || ''),
                        backgroundImage: cropText(style.backgroundImage || ''),
                        opacity: cropText(style.opacity || ''),
                        visibility: cropText(style.visibility || ''),
                        display: cropText(style.display || '')
                    };
                } catch (e) {
                    return { backgroundColor: '', backgroundImage: '', opacity: '', visibility: '', display: '' };
                }
            }

            var _probeSent = Object.create(null);

            function postStartupProbe(phase) {
                if (_probeSent[phase]) return;
                _probeSent[phase] = true;

                try {
                    var html = document.documentElement;
                    var body = document.body;
                    var centerElement = null;
                    if (document.elementFromPoint && window.innerWidth > 0 && window.innerHeight > 0) {
                        centerElement = document.elementFromPoint(
                            Math.floor(window.innerWidth / 2),
                            Math.floor(window.innerHeight / 2)
                        );
                    }

                    var htmlStyle = getStyleSnapshot(html);
                    var bodyStyle = getStyleSnapshot(body);
                    var centerStyle = getStyleSnapshot(centerElement);

                    window.chrome.webview.postMessage({
                        type: 'startupProbe',
                        phase: phase,
                        href: location.href,
                        readyState: document.readyState,
                        visibilityState: document.visibilityState,
                        perfNow: roundNumber(performance.now ? performance.now() : -1),
                        firstPaint: getPaintMetric('first-paint'),
                        firstContentfulPaint: getPaintMetric('first-contentful-paint'),
                        htmlBackground: htmlStyle.backgroundColor,
                        htmlBackgroundImage: htmlStyle.backgroundImage,
                        htmlOpacity: htmlStyle.opacity,
                        bodyBackground: bodyStyle.backgroundColor,
                        bodyBackgroundImage: bodyStyle.backgroundImage,
                        bodyOpacity: bodyStyle.opacity,
                        centerTag: centerElement ? cropText(centerElement.tagName || '') : '',
                        centerId: centerElement ? cropText(centerElement.id || '') : '',
                        centerClass: centerElement ? cropText(centerElement.className || '') : '',
                        centerBackground: centerStyle.backgroundColor,
                        centerBackgroundImage: centerStyle.backgroundImage,
                        centerOpacity: centerStyle.opacity,
                        innerWidth: window.innerWidth || 0,
                        innerHeight: window.innerHeight || 0
                    });
                } catch (e) {}
            }

            function scheduleVisualProbeSeries(phaseBase) {
                if (window.requestAnimationFrame) {
                    window.requestAnimationFrame(function() {
                        window.requestAnimationFrame(function() {
                            postStartupProbe(phaseBase + '-raf2');
                        });
                    });
                } else {
                    setTimeout(function() {
                        postStartupProbe(phaseBase + '-timeout');
                    }, 0);
                }

                setTimeout(function() {
                    postStartupProbe(phaseBase + '-t250');
                }, 250);
            }

            window.__fb2kNotifyWindowReady = function(detail) {
                postStartupProbe('explicit-window-ready');
                postWindowReady(detail);
            };

            postStartupProbe('helper-injected');

            if (document.readyState === 'complete') {
                postStartupProbe('auto-ready-state-complete');
                scheduleVisualProbeSeries('auto-ready-state-complete');
                scheduleWindowReady({ source: 'auto-ready-state-complete' });
            } else {
                window.addEventListener('load', function() {
                    postStartupProbe('auto-load');
                    scheduleVisualProbeSeries('auto-load');
                    scheduleWindowReady({ source: 'auto-load' });
                }, { once: true });
            }
        })();
    )";
    webview_->AddScriptToExecuteOnDocumentCreated(readyHelperScript, nullptr);
    LOG("Window ready helper script injected");
}

HRESULT WebViewHost::Navigate(const std::wstring& url) {
    if (!webview_) return E_FAIL;
    LOG("Navigating to: ", pfc::stringcvt::string_utf8_from_wide(url.c_str()).get_ptr());
    return webview_->Navigate(url.c_str());
}

HRESULT WebViewHost::NavigateToString(const std::wstring& html) {
    if (!webview_) return E_FAIL;
    return webview_->NavigateToString(html.c_str());
}

HRESULT WebViewHost::ExecuteScript(const std::wstring& script, ScriptCallback callback) {
    if (!webview_) return E_FAIL;
    
    return webview_->ExecuteScript(
        script.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [callback](HRESULT /*result*/, LPCWSTR resultJson) {
                if (callback && resultJson) {
                    // 异常安全边界：ExecuteScript 完成回调运行在主线程的 WebView2 COM 回调里，
                    // 回调体内（如 nlohmann::json 访问）抛出的 C++ 异常一旦逃逸到原生回调边界，
                    // 会触发 terminate() 直接崩溃整个 foobar2000 进程，故必须就地兜住。
                    // 全项目 3 处 ExecuteScript 均经此 funnel，这一处兜住整类崩溃。
                    try {
                        callback(resultJson);
                    } catch (const std::exception& e) {
                        console::printf("[WebViewHost] ExecuteScript callback threw: %s", e.what());
                    } catch (...) {
                        console::print("[WebViewHost] ExecuteScript callback threw unknown exception");
                    }
                }
                return S_OK;
            }
        ).Get()
    );
}

HRESULT WebViewHost::PostMessage(const std::wstring& json) {
    if (!webview_) return E_FAIL;
    return webview_->PostWebMessageAsJson(json.c_str());
}

void WebViewHost::SetMessageHandler(MessageHandler handler) {
    messageHandler_ = std::move(handler);
}

void WebViewHost::SetFocusChangedCallback(FocusChangedCallback callback) {
    focusChangedCallback_ = std::move(callback);
}

bool WebViewHost::MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON reason) {
    // Visual Hosting 模式下 WebView2 没有子 HWND，宿主窗口拿到 WM_SETFOCUS 后
    // 必须主动把焦点交给 Chromium，否则键盘输入无处可去（Alt+Tab 回来后失灵）。
    if (!controller_) return false;
    HRESULT hr = controller_->MoveFocus(reason);
    if (FAILED(hr)) {
        LOG("WebViewHost::MoveFocus failed, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return false;
    }
    return true;
}

void WebViewHost::SetNavigationCompletedCallback(NavigationCompletedCallback callback) {
    navigationCompletedCallback_ = std::move(callback);
}

void WebViewHost::SetProcessFailedCallback(ProcessFailedCallback callback) {
    processFailedCallback_ = std::move(callback);
}

// ==========================================================================
// WebView2 进程崩溃诊断日志
// 写入 foobar2000 profile 目录下的 webview_crash.log，不污染 fb2k console。
// 崩溃时浏览器/渲染进程已死、DevTools 不可用，故必须用宿主进程的原生 IO 落盘。
// ==========================================================================
void WebViewHost::WriteCrashLog(const std::string& line) {
    try {
        // profile 目录（与 ConsoleApi 的 webview_ui.log 同目录，但独立文件）
        pfc::string8 profilePath;
        filesystem::g_get_display_path("profile://", profilePath);
        std::string dir(profilePath.get_ptr(), profilePath.get_length());

        std::wstring widePath =
            pfc::stringcvt::string_wide_from_utf8(dir.c_str()).get_ptr();
        widePath += L"\\webview_crash.log";

        // 时间戳前缀
        std::time_t now = std::time(nullptr);
        std::tm tmv{};
        localtime_s(&tmv, &now);
        char ts[32] = {0};
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);

        std::ofstream file(widePath, std::ios::app);
        if (file.is_open()) {
            file << "[" << ts << "] " << line << "\n";
        }
    } catch (...) {
        // 崩溃日志写入失败不得影响恢复流程
    }
}

// ==========================================================================
// 注册 WebView2 进程崩溃事件 (add_ProcessFailed)
// Visual Hosting 模式下进程崩溃会导致只剩空 Win32 窗口（DComp visual 仍在
// 但无内容渲染），且 webview_ 指针不会自动失效。此处按崩溃类型分级处理：
//   - 渲染进程崩溃/无响应 → 自动 Reload() 恢复（controller + DComp 连接仍有效）
//   - 浏览器进程退出       → 整个 WebView 失效，通知上层重建（recovered=false）
//   - 其他工具进程（GPU 等）→ 运行时通常自愈，仅记录日志
// ==========================================================================
void WebViewHost::SetupProcessFailedHandling() {
    if (!webview_) return;

    HRESULT hr = webview_->add_ProcessFailed(
        Callback<ICoreWebView2ProcessFailedEventHandler>(
            [this](ICoreWebView2* /*sender*/,
                   ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                COREWEBVIEW2_PROCESS_FAILED_KIND kind =
                    COREWEBVIEW2_PROCESS_FAILED_KIND_UNKNOWN_PROCESS_EXITED;
                if (args) {
                    args->get_ProcessFailedKind(&kind);
                }

                // 收集诊断详情（reason / exitCode / processDescription 来自 Args2）
                std::ostringstream diag;
                diag << "WebView2 ProcessFailed: kind=" << static_cast<int>(kind);

                wil::com_ptr<ICoreWebView2ProcessFailedEventArgs2> args2;
                if (args && SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&args2))) && args2) {
                    COREWEBVIEW2_PROCESS_FAILED_REASON reason =
                        COREWEBVIEW2_PROCESS_FAILED_REASON_UNEXPECTED;
                    int exitCode = 0;
                    wil::unique_cotaskmem_string description;
                    args2->get_Reason(&reason);
                    args2->get_ExitCode(&exitCode);
                    args2->get_ProcessDescription(&description);
                    diag << " reason=" << static_cast<int>(reason)
                         << " exitCode=" << exitCode;
                    if (description) {
                        diag << " process=\""
                             << pfc::stringcvt::string_utf8_from_wide(description.get()).get_ptr()
                             << "\"";
                    }
                }

                // 判断是否为可 Reload 恢复的渲染进程类崩溃
                const bool isRenderKind =
                    kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED ||
                    kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE ||
                    kind == COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED;

                const bool isBrowserKind =
                    kind == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;

                bool recovered = false;
                if (isRenderKind) {
                    // 渲染进程崩溃：controller / DComp 连接仍有效，重新加载页面即可恢复
                    HRESULT reloadHr = webview_ ? webview_->Reload() : E_FAIL;
                    recovered = SUCCEEDED(reloadHr);
                    diag << " action=Reload result=" << (recovered ? "ok" : "failed");
                } else if (isBrowserKind) {
                    // 浏览器进程退出：整个 WebView 失效，需上层重建
                    diag << " action=needRebuild";
                } else {
                    // GPU / 工具进程等：运行时通常自愈，仅记录
                    diag << " action=logOnly";
                }

                WriteCrashLog(diag.str());
                LOG("WebViewHost: ", diag.str().c_str());

                if (processFailedCallback_) {
                    processFailedCallback_(kind, recovered);
                }
                return S_OK;
            }).Get(),
        &processFailedToken_);

    if (FAILED(hr)) {
        LOG("WebViewHost: add_ProcessFailed failed, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
}

void WebViewHost::Resize(int width, int height) {
    RECT bounds = { 0, 0, width, height };
    Resize(bounds);
}

void WebViewHost::Resize(const RECT& bounds) {
    if (controller_) {
        LOG("WebViewHost::Resize - Bounds: ", bounds.left, ",", bounds.top, " to ", bounds.right, ",", bounds.bottom);
        controller_->put_Bounds(bounds);
        
        // 如果当前是透明模式，resize 后需要重新应用透明背景
        // WebView2 的 put_Bounds 可能会重置背景颜色
        if (isBackgroundTransparent_) {
            wil::com_ptr<ICoreWebView2Controller2> controller2;
            HRESULT hr = controller_->QueryInterface(IID_PPV_ARGS(&controller2));
            if (SUCCEEDED(hr) && controller2) {
                COREWEBVIEW2_COLOR bgColor = { 0, 0, 0, 0 };  // Alpha = 0
                controller2->put_DefaultBackgroundColor(bgColor);
            }
        }
    }
    
    // 同步 DirectComposition visual 裁剪矩形，防止合成层渲染溢出窗口边界
    if (dcompRootVisual_) {
        D2D1_RECT_F clipRect = {
            (float)bounds.left,
            (float)bounds.top,
            (float)bounds.right,
            (float)bounds.bottom
        };
        dcompRootVisual_->SetClip(clipRect);
        if (dcompDevice_) {
            dcompDevice_->Commit();
        }
    }
}

void WebViewHost::SetVisible(bool visible) {
    if (controller_) {
        controller_->put_IsVisible(visible ? TRUE : FALSE);
    }
}

void WebViewHost::SetMemoryUsageLow(bool low) {
    if (!webview_) return;
    // put_MemoryUsageTargetLevel 属于 ICoreWebView2_19。当前 pin 的 SDK 已支持；
    // 若用户安装的 WebView2 Runtime 偏老导致 query 失败，则安全 no-op。
    wil::com_ptr<ICoreWebView2_19> wv19;
    if (SUCCEEDED(webview_.try_query_to(&wv19)) && wv19) {
        wv19->put_MemoryUsageTargetLevel(
            low ? COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW
                : COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_NORMAL);
    }
}

void WebViewHost::SetBoundsVisible(bool visible) {
    // Don't use put_IsVisible(). It hides the host window.
    // Use put_Bounds({0,0,0,0}) to hide WebView content instead.
    if (!controller_) return;
    
    if (visible) {
        RECT bounds{};
        if (parentHwnd_ && GetClientRect(parentHwnd_, &bounds)) {
            Resize(bounds);
        }
    } else {
        Resize(RECT{0, 0, 0, 0});
        if (parentHwnd_) {
            InvalidateRect(parentHwnd_, nullptr, TRUE);
        }
    }
}

void WebViewHost::OpenDevTools() {
    if (webview_) {
        webview_->OpenDevToolsWindow();
    }
}

void WebViewHost::Reload() {
    if (webview_) {
        webview_->Reload();
    }
}

HRESULT WebViewHost::SetVirtualHostMapping(const std::wstring& hostName, const std::wstring& folderPath) {
    if (!webview_) return E_FAIL;
    
    // 获取 ICoreWebView2_3 接口（支持虚拟主机映射）
    wil::com_ptr<ICoreWebView2_3> webview3;
    HRESULT hr = webview_->QueryInterface(IID_PPV_ARGS(&webview3));
    if (FAILED(hr) || !webview3) {
        LOG("Failed to get ICoreWebView2_3 interface for virtual host mapping");
        return hr;
    }
    
    // 设置虚拟主机映射
    hr = webview3->SetVirtualHostNameToFolderMapping(
        hostName.c_str(),
        folderPath.c_str(),
        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
    );
    
    if (SUCCEEDED(hr)) {
        LOG("Virtual host mapping set: https://", 
            pfc::stringcvt::string_utf8_from_wide(hostName.c_str()).get_ptr(),
            "/ -> ", 
            pfc::stringcvt::string_utf8_from_wide(folderPath.c_str()).get_ptr());
        
        // 保存映射并注册 charset 修复
        virtualHostName_ = hostName;
        virtualHostFolderPath_ = folderPath;
        SetupCharsetFixForVirtualHost();
    } else {
        LOG("Failed to set virtual host mapping, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
    
    return hr;
}

HRESULT WebViewHost::ClearVirtualHostMapping(const std::wstring& hostName) {
    if (!webview_) return E_FAIL;
    
    // 获取 ICoreWebView2_3 接口
    wil::com_ptr<ICoreWebView2_3> webview3;
    HRESULT hr = webview_->QueryInterface(IID_PPV_ARGS(&webview3));
    if (SUCCEEDED(hr) && webview3) {
        hr = webview3->ClearVirtualHostNameToFolderMapping(hostName.c_str());
        if (SUCCEEDED(hr)) {
            LOG("Virtual host mapping cleared: ", 
                pfc::stringcvt::string_utf8_from_wide(hostName.c_str()).get_ptr());
        }
    }
    return hr;
}

//==========================================================================
// 修复 JavaScript 文件 UTF-8 编码问题
// WebView2 加载本地 JS 文件时可能使用系统默认编码 (如 GBK)
//==========================================================================
void WebViewHost::InjectCharsetFixScript() {
    // 此脚本会在 DOM 加载前执行，确保动态加载的脚本使用 UTF-8
    // 注意：对于已经通过 <script src="..."> 加载的文件，需要确保 HTML 中有 <meta charset="UTF-8">
    const wchar_t* charsetFixScript = LR"(
        // Fix: ensure dynamically created script tags use UTF-8 encoding
        (function() {
            const originalCreateElement = document.createElement.bind(document);
            document.createElement = function(tagName, options) {
                const element = originalCreateElement(tagName, options);
                if (tagName.toLowerCase() === 'script') {
                    element.charset = 'UTF-8';
                }
                return element;
            };
        })();
    )";
    
    webview_->AddScriptToExecuteOnDocumentCreated(charsetFixScript, nullptr);
    LOG("Charset fix script injected for dynamic scripts");
}

//==========================================================================
// 修复虚拟主机映射的 .js/.css 文件 charset
// WebView2 的 SetVirtualHostNameToFolderMapping 不设置 Content-Type charset，
// 中文 Windows 上 .js/.css 内的 UTF-8 中文会被错误按 GBK 解析导致乱码。
// 通过 WebResourceRequested 拦截这些请求，自行读取文件并附带 charset=utf-8 响应。
//==========================================================================
void WebViewHost::SetupCharsetFixForVirtualHost() {
    if (charsetFixRegistered_ || !webview_ || virtualHostName_.empty()) return;
    charsetFixRegistered_ = true;

    // 注册过滤器: https://<virtualHost>/[.js] 和 [.css]
    wil::com_ptr<ICoreWebView2_2> webview2;
    HRESULT hr = webview_->QueryInterface(IID_PPV_ARGS(&webview2));
    if (FAILED(hr) || !webview2) {
        LOG("Failed to get ICoreWebView2_2 for charset fix filter");
        return;
    }

    std::wstring jsFilter = L"https://" + virtualHostName_ + L"/*.js";
    std::wstring cssFilter = L"https://" + virtualHostName_ + L"/*.css";

    webview2->AddWebResourceRequestedFilter(jsFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_SCRIPT);
    webview2->AddWebResourceRequestedFilter(cssFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_STYLESHEET);

    webview_->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2WebResourceRequestedEventArgs* args) {
                // 纵深防御：顶层 catch-all，防止 bad_alloc/length_error 等异常逃逸 COM 回调边界触发 terminate()。
                try {
                wil::com_ptr<ICoreWebView2WebResourceRequest> request;
                if (FAILED(args->get_Request(&request)) || !request) return S_OK;

                wil::unique_cotaskmem_string uri;
                if (FAILED(request->get_Uri(&uri)) || !uri) return S_OK;
                std::wstring uriStr(uri.get());

                // 只处理虚拟主机上的 .js/.css 请求
                std::wstring prefix = L"https://" + virtualHostName_ + L"/";
                if (uriStr.find(prefix) != 0) return S_OK;

                // 提取相对路径
                std::wstring relativePath = uriStr.substr(prefix.length());

                // 去除 query string
                auto qpos = relativePath.find(L'?');
                if (qpos != std::wstring::npos) relativePath = relativePath.substr(0, qpos);

                // URL decode
                // WebView2 虚拟主机不做复杂 URL 编码，直接 %20 替换即可
                for (size_t p = 0; (p = relativePath.find(L"%20", p)) != std::wstring::npos; ) {
                    relativePath.replace(p, 3, L" ");
                    ++p;  // advance past the replacement
                }

                // 将 / 转为系统分隔符
                std::replace(relativePath.begin(), relativePath.end(), L'/', L'\\');

                // 确定 MIME 类型
                std::wstring contentType;
                if (relativePath.ends_with(L".js")) {
                    contentType = L"text/javascript; charset=utf-8";
                } else if (relativePath.ends_with(L".css")) {
                    contentType = L"text/css; charset=utf-8";
                } else {
                    return S_OK; // 非 js/css，放行给默认处理
                }

                // 构造本地文件路径
                std::wstring localPath = virtualHostFolderPath_ + L"\\" + relativePath;

                // 安全校验: 确保路径在映射目录内（防路径遍历）
                std::wstring canonicalPathBuf(MAX_PATH, L'\0');
                std::wstring canonicalBaseBuf(MAX_PATH, L'\0');
                if (!GetFullPathNameW(localPath.c_str(), MAX_PATH, canonicalPathBuf.data(), nullptr) ||
                    !GetFullPathNameW(virtualHostFolderPath_.c_str(), MAX_PATH, canonicalBaseBuf.data(), nullptr)) {
                    return S_OK;
                }
                canonicalPathBuf.resize(wcslen(canonicalPathBuf.c_str()));
                canonicalBaseBuf.resize(wcslen(canonicalBaseBuf.c_str()));
                // 确保规范化路径以映射目录为前缀
                if (canonicalPathBuf.find(canonicalBaseBuf) != 0) {
                    LOG("Charset fix: blocked path traversal attempt");
                    return S_OK;
                }

                // 读取文件
                HANDLE hFile = CreateFileW(localPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile == INVALID_HANDLE_VALUE) {
                    return S_OK; // 文件不存在，放行给默认 404 处理
                }

                LARGE_INTEGER fileSize;
                GetFileSizeEx(hFile, &fileSize);
                std::vector<uint8_t> fileData(static_cast<size_t>(fileSize.QuadPart));
                DWORD bytesRead = 0;
                ReadFile(hFile, fileData.data(), static_cast<DWORD>(fileData.size()), &bytesRead, nullptr);
                CloseHandle(hFile);

                // 创建 IStream
                IStream* stream = SHCreateMemStream(fileData.data(), static_cast<UINT>(bytesRead));
                if (!stream) return S_OK;

                // 构造带正确 charset 的响应
                std::wstring headers = L"Content-Type: " + contentType + L"\r\n"
                    L"Cache-Control: no-cache\r\n"
                    L"Access-Control-Allow-Origin: *\r\n";

                wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                if (SUCCEEDED(environment_->CreateWebResourceResponse(
                        stream, 200, L"OK", headers.c_str(), &response))) {
                    args->put_Response(response.get());
                }

                stream->Release();
                return S_OK;
                } catch (...) {
                    return S_OK;  // 异常逃逸即放行默认处理，绝不让异常穿透 COM 回调边界
                }
            }
        ).Get(),
        nullptr
    );

    LOG("Charset fix: WebResourceRequested handler registered for virtual host JS/CSS");
}

// ============================================
// DWM 透明效果支持
// ============================================

HRESULT WebViewHost::SetBackgroundTransparent(bool transparent) {
    if (!controller_) return E_FAIL;
    
    // 记录透明状态（Resize 时需要重新应用）
    isBackgroundTransparent_ = transparent;
    
    // 方法 1: 使用 ICoreWebView2Controller2 设置背景色
    wil::com_ptr<ICoreWebView2Controller2> controller2;
    HRESULT hr = controller_->QueryInterface(IID_PPV_ARGS(&controller2));
    if (SUCCEEDED(hr) && controller2) {
        COREWEBVIEW2_COLOR bgColor;
        if (transparent) {
            // 完全透明背景 - 让 DWM 效果穿透
            bgColor = { 0, 0, 0, 0 };  // Alpha = 0
        } else {
            // 不透明深色背景
            bgColor = { 255, 30, 30, 30 };  // #1e1e1e
        }
        
        hr = controller2->put_DefaultBackgroundColor(bgColor);
        if (SUCCEEDED(hr) && WindowChromeTrace::AuxiliaryTraceEnabled()) {
            LOG(transparent ? "WebView background set to TRANSPARENT (DWM passthrough)" 
                           : "WebView background set to OPAQUE");
        }
    }
    
    return hr;
}

void WebViewHost::RefreshForDwmEffect() {
    if (!controller_ || !parentHwnd_) return;
    
    // 方法 1: 触发 WebView 边界重设来刷新合成器
    RECT bounds;
    if (SUCCEEDED(controller_->get_Bounds(&bounds))) {
        // 微调边界强制刷新
        RECT adjustedBounds = bounds;
        adjustedBounds.right -= 1;
        controller_->put_Bounds(adjustedBounds);
        
        // 立即恢复
        controller_->put_Bounds(bounds);
    }

    // RefreshForDwmEffect 与 Resize 一样会经过 put_Bounds，
    // 这里也要重放当前背景色，避免透明表面回落到 WebView 默认灰底。
    wil::com_ptr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(controller_->QueryInterface(IID_PPV_ARGS(&controller2))) && controller2) {
        const COREWEBVIEW2_COLOR bgColor = isBackgroundTransparent_
            ? COREWEBVIEW2_COLOR{ 0, 0, 0, 0 }
            : COREWEBVIEW2_COLOR{ 255, 30, 30, 30 };
        controller2->put_DefaultBackgroundColor(bgColor);
    }

    // 方法 1.5: 强制 DComp visual tree 标记为 dirty 再 Commit，
    // 仅 Commit 无 pending changes 时是 no-op，DComp compositor 不会触发重新合成。
    // SetClip 即使值相同也会标记 visual 为 dirty，触发完整重新合成以拾取 DWM backdrop。
    if (dcompRootVisual_ && SUCCEEDED(controller_->get_Bounds(&bounds))) {
        D2D1_RECT_F clipRect = {
            (float)bounds.left, (float)bounds.top,
            (float)bounds.right, (float)bounds.bottom
        };
        dcompRootVisual_->SetClip(clipRect);
    }
    if (dcompDevice_) {
        dcompDevice_->Commit();
    }
    
    // 方法 2: 强制窗口重绘
    RedrawWindow(parentHwnd_, nullptr, nullptr, 
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    
    LOG("WebView refreshed for DWM effect");
}

// ============================================
// Visual Hosting 模式实现
// ============================================

HRESULT WebViewHost::InitializeDirectComposition() {
    // 创建 DirectComposition 设备
    HRESULT hr = DCompositionCreateDevice2(nullptr, IID_PPV_ARGS(&dcompDevice_));
    if (FAILED(hr)) {
        LOG("Failed to create DirectComposition device, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return hr;
    }
    
    // 为窗口创建目标
    hr = dcompDevice_->CreateTargetForHwnd(parentHwnd_, TRUE, &dcompTarget_);
    if (FAILED(hr)) {
        LOG("Failed to create DirectComposition target, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return hr;
    }
    
    // 创建根可视化对象
    hr = dcompDevice_->CreateVisual(&dcompRootVisual_);
    if (FAILED(hr)) {
        LOG("Failed to create root visual, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return hr;
    }
    
    // 设置根可视化对象
    hr = dcompTarget_->SetRoot(dcompRootVisual_.get());
    if (FAILED(hr)) {
        LOG("Failed to set root visual, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return hr;
    }
    
    // 创建 WebView 可视化对象
    hr = dcompDevice_->CreateVisual(&dcompWebViewVisual_);
    if (FAILED(hr)) {
        LOG("Failed to create WebView visual, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return hr;
    }
    
    // 将 WebView 可视化对象添加到根
    hr = dcompRootVisual_->AddVisual(dcompWebViewVisual_.get(), TRUE, nullptr);
    if (FAILED(hr)) {
        LOG("Failed to add WebView visual to root, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return hr;
    }
    
    // 提交更改
    hr = dcompDevice_->Commit();
    if (FAILED(hr)) {
        LOG("Failed to commit DirectComposition, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return hr;
    }
    
    LOG("✓ DirectComposition initialized successfully");
    return S_OK;
}

void WebViewHost::CleanupDirectComposition() {
    if (dcompWebViewVisual_) {
        dcompWebViewVisual_.reset();
    }
    if (dcompRootVisual_) {
        dcompRootVisual_.reset();
    }
    if (dcompTarget_) {
        dcompTarget_.reset();
    }
    if (dcompDevice_) {
        dcompDevice_.reset();
    }
}

void WebViewHost::SetupCursorHandling() {
    if (!compositionController_) return;
    
    // 注册光标变化事件
    HRESULT hr = compositionController_->add_CursorChanged(
        Callback<ICoreWebView2CursorChangedEventHandler>(
            [this](ICoreWebView2CompositionController* sender, IUnknown* /*args*/) {
                if (!parentHwnd_) return S_OK;
                
                // CompositionController 模式下 Chromium 不会处理 WM_SETCURSOR,
                // 必须由父窗口在每次 WM_SETCURSOR 主动 SetCursor 才能保证光标始终
                // 反映 Chromium 当前希望显示的状态。
                //
                // 历史方案曾把光标写入 GCLP_HCURSOR 让 DefWindowProc 接管恢复,
                // 但会在 alt+tab 等合成 WM_SETCURSOR 时残留旧光标(问题 B);
                // 同时 Chromium 在 CSS `cursor: none` ↔ default 切换时不触发
                // CursorChanged(实测仅在 hover 元素的 cursor 类型变化时触发),
                // 也不会把"隐藏"语义反映在 get_Cursor() 返回值里。
                //
                // 因此本回调只做即时同步: 把 Chromium 当前光标设为系统当前光标,
                // 并把它缓存到 currentCursor_, 供 MainWindow/PopupWindow 在
                // WM_SETCURSOR 时主动调用 ApplyCurrentCursor 使用。
                // CSS `cursor: none` 通过显式 cursor.setHidden(true) API 由前端
                // 主动通知 (见 CursorApi)。
                HCURSOR cursor = nullptr;
                if (SUCCEEDED(sender->get_Cursor(&cursor))) {
                    currentCursor_ = cursor;
                    if (!cursorHidden_) {
                        ::SetCursor(cursor);
                    }
                }
                return S_OK;
            }
        ).Get(),
        &cursorChangedToken_
    );
    
    if (SUCCEEDED(hr)) {
        LOG("✓ Cursor change handler registered");
    }
}

bool WebViewHost::ApplyCurrentCursor() {
    // 没有 CompositionController 说明走的是标准 Controller 模式,
    // 那种模式下 WebView2 自己拥有子 HWND 处理 WM_SETCURSOR, 父窗口不应介入。
    if (!compositionController_) {
        return false;
    }

    if (cursorHidden_) {
        // 显式隐藏: 任何来源的 WM_SETCURSOR 都强制隐藏。
        ::SetCursor(nullptr);
        return true;
    }

    if (currentCursor_) {
        ::SetCursor(currentCursor_);
        return true;
    }

    // CursorChanged 还未触发过 (例如 WebView 刚就绪、鼠标尚未进入客户区)。
    // 由调用方 fallback 到 DefWindowProc, 让系统使用类光标 (默认箭头)。
    return false;
}

bool WebViewHost::SetCursorHidden(bool hidden) {
    if (cursorHidden_ == hidden) {
        return false;
    }
    cursorHidden_ = hidden;

    // 立即生效: 模拟一次 WM_SETCURSOR, 让光标状态在不等下一次鼠标事件的
    // 情况下也能切换。GetCursorPos + WindowFromPoint 用来确认当前鼠标
    // 是否还在自己的客户区, 避免在跨窗口时误改其他窗口的光标。
    if (!parentHwnd_) {
        return true;
    }
    POINT pt;
    if (!GetCursorPos(&pt)) {
        return true;
    }
    HWND hover = WindowFromPoint(pt);
    if (hover != parentHwnd_) {
        return true;
    }
    if (hidden) {
        ::SetCursor(nullptr);
    } else if (currentCursor_) {
        ::SetCursor(currentCursor_);
    } else {
        ::SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    return true;
}

bool WebViewHost::HandleMouseMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    // 如果没有 CompositionController，不处理
    if (!compositionController_) {
        return false;
    }
    
    POINT point;
    POINTSTOPOINT(point, lParam);
    
    // 滚轮消息是屏幕坐标，需要转换为客户端坐标
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
        ScreenToClient(parentHwnd_, &point);
    }
    
    // ============================================
    // 边缘检测：不转发边缘区域的鼠标事件
    // 让 WM_NCHITTEST 处理窗口边缘调整大小
    // ============================================
    RECT clientRect;
    GetClientRect(parentHwnd_, &clientRect);
    const int EDGE_WIDTH = 6;  // 边缘检测宽度（像素）
    
    bool isOnEdge = (point.x < EDGE_WIDTH || 
                     point.x >= clientRect.right - EDGE_WIDTH ||
                     point.y < EDGE_WIDTH || 
                     point.y >= clientRect.bottom - EDGE_WIDTH);
    
    // 鼠标离开消息也需要处理
    if (message == WM_MOUSELEAVE) {
        isTrackingMouse_ = false;
        // 仍然转发给 WebView
    }
    // 边缘区域不转发（除非正在捕获鼠标）
    else if (isOnEdge && !isCapturingMouse_) {
        return false;  // 交给 DefWindowProc 处理
    }
    
    // ============================================
    // 鼠标捕获管理
    // ============================================
    if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN) {
        SetCapture(parentHwnd_);
        isCapturingMouse_ = true;
    }
    else if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) {
        ReleaseCapture();
        isCapturingMouse_ = false;
    }
    
    // 跟踪鼠标离开
    if (!isTrackingMouse_ && message == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT tme = { sizeof(tme) };
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = parentHwnd_;
        TrackMouseEvent(&tme);
        isTrackingMouse_ = true;
    }
    
    // ============================================
    // 转发给 WebView
    // ============================================
    COREWEBVIEW2_MOUSE_EVENT_KIND eventKind;
    switch (message) {
        case WM_MOUSEMOVE:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE; break;
        case WM_LBUTTONDOWN:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN; break;
        case WM_LBUTTONUP:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP; break;
        case WM_LBUTTONDBLCLK:  eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOUBLE_CLICK; break;
        case WM_RBUTTONDOWN:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN; break;
        case WM_RBUTTONUP:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP; break;
        case WM_RBUTTONDBLCLK:  eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOUBLE_CLICK; break;
        case WM_MBUTTONDOWN:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN; break;
        case WM_MBUTTONUP:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP; break;
        case WM_MBUTTONDBLCLK:  eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOUBLE_CLICK; break;
        case WM_MOUSEWHEEL:     eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL; break;
        case WM_MOUSEHWHEEL:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_HORIZONTAL_WHEEL; break;
        case WM_XBUTTONDOWN:    eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOWN; break;
        case WM_XBUTTONUP:      eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_UP; break;
        case WM_XBUTTONDBLCLK:  eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_X_BUTTON_DOUBLE_CLICK; break;
        case WM_MOUSELEAVE:     eventKind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEAVE; break;
        default:
            return false;  // 不认识的消息
    }
    
    // 虚拟键状态
    COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS virtualKeys = COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_NONE;
    if (wParam & MK_CONTROL) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL);
    if (wParam & MK_SHIFT)   virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT);
    if (wParam & MK_LBUTTON) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON);
    if (wParam & MK_MBUTTON) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON);
    if (wParam & MK_RBUTTON) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON);
    if (wParam & MK_XBUTTON1) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON1);
    if (wParam & MK_XBUTTON2) virtualKeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(virtualKeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON2);
    
    // 鼠标滚轮数据
    UINT32 mouseData = 0;
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
        mouseData = GET_WHEEL_DELTA_WPARAM(wParam);
    }
    else if (message == WM_XBUTTONDOWN || message == WM_XBUTTONUP || message == WM_XBUTTONDBLCLK) {
        mouseData = GET_XBUTTON_WPARAM(wParam);
    }
    
    // 发送鼠标输入
    HRESULT hr = compositionController_->SendMouseInput(eventKind, virtualKeys, mouseData, point);
    
    return SUCCEEDED(hr);
}
// 临时文件缺失时的后备实现

// 缩放因子
HRESULT WebViewHost::SetZoomFactor(double zoomFactor) {
    if (!controller_) return E_FAIL;

    wil::com_ptr<ICoreWebView2Controller3> controller3;
    HRESULT hr = controller_->QueryInterface(IID_PPV_ARGS(&controller3));
    if (SUCCEEDED(hr) && controller3) {
        return controller3->put_ZoomFactor(zoomFactor);
    }
    return E_NOTIMPL;
}

double WebViewHost::GetZoomFactor() const {
    if (!controller_) return 1.0;

    wil::com_ptr<ICoreWebView2Controller3> controller3;
    HRESULT hr = controller_->QueryInterface(IID_PPV_ARGS(&controller3));
    if (SUCCEEDED(hr) && controller3) {
        double zoomFactor = 1.0;
        controller3->get_ZoomFactor(&zoomFactor);
        return zoomFactor;
    }
    return 1.0;
}

HRESULT WebViewHost::SetZoomForDpi(int dpi) {
    // 96 DPI = 100% = 1.0x
    // 120 DPI = 125% = 1.25x
    // 144 DPI = 150% = 1.5x
    double zoomFactor = dpi / 96.0;
    return SetZoomFactor(zoomFactor);
}

// CSS app-region 查询
// 需要 ICoreWebView2CompositionController4 (SDK 1.0.1185+)
int WebViewHost::GetNonClientRegionAtPoint(POINT clientPt) {
    if (!compositionController_) return 0;

    // GetNonClientRegionAtPoint 在 ICoreWebView2CompositionController4 中
    wil::com_ptr<ICoreWebView2CompositionController4> controller4;
    HRESULT hr = compositionController_->QueryInterface(IID_PPV_ARGS(&controller4));
    if (FAILED(hr) || !controller4) {
        // 仅首次失败时记录，避免日志洪泛
        static bool logged = false;
        if (!logged) {
            LOG("GetNonClientRegionAtPoint: ICoreWebView2CompositionController4 QI failed, hr=", 
                pfc::format_hex((uint32_t)hr));
            logged = true;
        }
        return 0;  // 当前运行时不支持此功能
    }

    COREWEBVIEW2_NON_CLIENT_REGION_KIND regionKind;
    hr = controller4->GetNonClientRegionAtPoint(clientPt, &regionKind);

    if (FAILED(hr)) {
        static bool logged = false;
        if (!logged) {
            LOG("GetNonClientRegionAtPoint failed, hr=", pfc::format_hex((uint32_t)hr));
            logged = true;
        }
        return 0;
    }

    // 转换为简单的整数返回值
    switch (regionKind) {
        case COREWEBVIEW2_NON_CLIENT_REGION_KIND_CAPTION:
            return 1;  // drag 区域
        case COREWEBVIEW2_NON_CLIENT_REGION_KIND_CLIENT:
            return 0;  // 客户区
        default:
            return 2;  // 其他
    }
}

// ==========================================================================
// SDK Bridge 脚本注入
// 注入 window.fb SDK 到页面，提供高级 API 封装
// ==========================================================================
void WebViewHost::InjectSdkBridgeScript() {
    if (!webview_) {
        LOG("InjectSdkBridgeScript: webview_ is null");
        return;
    }
    
    // 获取 SDK 脚本内容
    std::wstring sdkScript = GetInjectedFbSdkScript();
    
    // 注入到所有页面
    webview_->AddScriptToExecuteOnDocumentCreated(
        sdkScript.c_str(),
        Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
            [](HRESULT result, LPCWSTR /*id*/) {
                if (SUCCEEDED(result)) {
                    LOG("SDK Bridge script injected successfully");
                } else {
                    LOG("Failed to inject SDK Bridge script, HRESULT: ", pfc::format_hex((uint32_t)result));
                }
                return S_OK;
            }
        ).Get()
    );
}

// ==========================================================================
// URL 解码辅助函数
// 解码 %XX 编码的 URL 路径
// ==========================================================================
static std::string UrlDecode(const std::wstring& encoded) {
    std::string result;
    result.reserve(encoded.size());
    
    for (size_t i = 0; i < encoded.size(); ++i) {
        wchar_t ch = encoded[i];
        if (ch == L'%' && i + 2 < encoded.size()) {
            // 解析 %XX
            wchar_t hex1 = encoded[i + 1];
            wchar_t hex2 = encoded[i + 2];
            int val = 0;
            
            // 解析第一个十六进制字符
            if (hex1 >= L'0' && hex1 <= L'9') val = (hex1 - L'0') << 4;
            else if (hex1 >= L'A' && hex1 <= L'F') val = (hex1 - L'A' + 10) << 4;
            else if (hex1 >= L'a' && hex1 <= L'f') val = (hex1 - L'a' + 10) << 4;
            else { result += static_cast<char>(ch); continue; }
            
            // 解析第二个十六进制字符
            if (hex2 >= L'0' && hex2 <= L'9') val |= (hex2 - L'0');
            else if (hex2 >= L'A' && hex2 <= L'F') val |= (hex2 - L'A' + 10);
            else if (hex2 >= L'a' && hex2 <= L'f') val |= (hex2 - L'a' + 10);
            else { result += static_cast<char>(ch); continue; }
            
            result += static_cast<char>(val);
            i += 2;  // 跳过两个十六进制字符
        } else if (ch == L'+') {
            result += ' ';
        } else if (ch < 128) {
            result += static_cast<char>(ch);
        } else {
            // 对于非 ASCII 字符，转换为 UTF-8
            char buf[4];
            int len = WideCharToMultiByte(CP_UTF8, 0, &ch, 1, buf, sizeof(buf), nullptr, nullptr);
            result.append(buf, len);
        }
    }
    
    return result;
}

// ==========================================================================
// Scaled artwork response cache
// key: resolved external artwork path(s) when available, otherwise request path.
// This keeps embedded artwork isolated per track while allowing tracks that
// share the same folder image to reuse the same resized JPEG entry.
// ==========================================================================
namespace {
    struct ScaledArtworkEntry {
        std::vector<uint8_t> jpegBytes;
        std::string mimeType;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::unordered_map<std::string, ScaledArtworkEntry> s_scaledArtworkCache;
    std::mutex s_scaledArtworkCacheMutex;
    // Increased capacity (200->500) and TTL (60s->300s) for better cache hit rate
    static constexpr size_t MAX_SCALED_CACHE = 500;
    static constexpr int SCALED_CACHE_TTL_SEC = 300;
}

// ==========================================================================
// Custom protocol handler
// fb2k://artwork, artwork://, https://foo-ui-webview2.local/fb2k-artwork/*
// ==========================================================================
void WebViewHost::SetupCustomProtocol() {
    if (!webview_) {
        LOG("SetupCustomProtocol: webview_ is null");
        return;
    }
    
    // 获取 ICoreWebView2_2 接口以添加 WebResourceRequested 过滤器
    wil::com_ptr<ICoreWebView2_2> webview2;
    HRESULT hr = webview_->QueryInterface(IID_PPV_ARGS(&webview2));
    if (FAILED(hr) || !webview2) {
        LOG("Failed to get ICoreWebView2_2 for custom protocol, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return;
    }
    
    // 添加 URI 过滤器 - fb2k://artwork/* 和 artwork://*
    std::wstring fb2kArtworkFilter(L"fb2k://artwork/*");
    hr = webview2->AddWebResourceRequestedFilter(fb2kArtworkFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    if (FAILED(hr)) {
        LOG("Failed to add fb2k://artwork filter, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
    
    std::wstring artworkFilter(L"artwork://*");
    hr = webview2->AddWebResourceRequestedFilter(artworkFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    if (FAILED(hr)) {
        LOG("Failed to add artwork:// filter, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
    
    // 添加同源 HTTPS 路由过滤器 - 用于 Canvas CORS 支持
    std::wstring sameOriginArtworkFilter(L"https://foo-ui-webview2.local/fb2k-artwork/*");
    hr = webview2->AddWebResourceRequestedFilter(sameOriginArtworkFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    if (FAILED(hr)) {
        LOG("Failed to add same-origin artwork filter, HRESULT: ", pfc::format_hex((uint32_t)hr));
    } else {
        LOG("✓ Added same-origin artwork route filter");
    }
    
    // 注册请求处理程序
    hr = webview_->add_WebResourceRequested(
        Callback<ICoreWebView2WebResourceRequestedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2WebResourceRequestedEventArgs* args) {
                // 纵深防御：顶层 catch-all，防止 bad_alloc/length_error 等异常逃逸 COM 回调边界触发 terminate()。
                try {
                wil::com_ptr<ICoreWebView2WebResourceRequest> request;
                if (FAILED(args->get_Request(&request)) || !request) {
                    return S_OK;
                }
                
                wil::unique_cotaskmem_string uri;
                if (FAILED(request->get_Uri(&uri)) || !uri) {
                    return S_OK;
                }
                
                std::wstring uriStr(uri.get());
                
                // 检查是否是我们的自定义协议或同源路由
                std::wstring fb2kPrefix(L"fb2k://artwork/");
                std::wstring artworkPrefix(L"artwork://");
                bool isFb2kArtwork = (uriStr.find(fb2kPrefix) == 0);
                bool isArtworkScheme = (uriStr.find(artworkPrefix) == 0);
                std::wstring sameOriginPrefix(L"https://foo-ui-webview2.local/fb2k-artwork/");
                bool isSameOriginArtwork = (uriStr.find(sameOriginPrefix) == 0);
                
                if (!isFb2kArtwork && !isArtworkScheme && !isSameOriginArtwork) {
                    return S_OK;  // 不是我们的协议
                }
                
                // 解析 URI
                std::string artworkType = "front";  // 默认类型
                int maxSize = 0;  // 0 = no resize
                std::string pathUtf8;
                
                // 计算路径开始位置（动态计算，避免硬编码长度）
                size_t pathStart = isFb2kArtwork ? 15 : (isArtworkScheme ? 10 : sameOriginPrefix.length());
                size_t queryPos = uriStr.find(L'?', pathStart);
                
                // 从 URL path 部分提取路径（向后兼容旧格式）
                std::wstring pathPartFromUrl;
                if (queryPos != std::wstring::npos) {
                    pathPartFromUrl = uriStr.substr(pathStart, queryPos - pathStart);
                } else {
                    pathPartFromUrl = uriStr.substr(pathStart);
                }
                
                // 解析查询参数的通用 lambda
                auto parseQueryParam = [](const std::wstring& query, const std::wstring& key) -> std::wstring {
                    std::wstring needle = key + L"=";
                    size_t pos = 0;
                    while ((pos = query.find(needle, pos)) != std::wstring::npos) {
                        // 确保是参数开头（pos==0 或前一字符为'&'）
                        if (pos == 0 || query[pos - 1] == L'&') {
                            size_t valStart = pos + needle.length();
                            size_t valEnd = query.find(L'&', valStart);
                            return (valEnd != std::wstring::npos)
                                ? query.substr(valStart, valEnd - valStart)
                                : query.substr(valStart);
                        }
                        pos += needle.length();
                    }
                    return {};
                };
                
                if (queryPos != std::wstring::npos) {
                    std::wstring query = uriStr.substr(queryPos + 1);
                    
                    // 优先从 query param 提取 path（新格式：fb2k://artwork/?path=ENCODED&type=...）
                    // Query string 不受 Chromium URL 路径规范化影响，%5C/%2F 保持原样
                    if (auto pathParam = parseQueryParam(query, L"path"); !pathParam.empty()) {
                        pathUtf8 = UrlDecode(pathParam);
                    }
                    
                    // Parse type parameter
                    if (auto typeParam = parseQueryParam(query, L"type"); !typeParam.empty()) {
                        artworkType = UrlDecode(typeParam);
                    }
                    
                    // Parse maxSize parameter
                    std::wstring maxSizeParam = parseQueryParam(query, L"maxSize");
                    if (!maxSizeParam.empty()) {
                        try {
                            maxSize = std::stoi(maxSizeParam);
                        } catch (...) {
                            maxSize = 0;
                        }
                    }
                }
                
                // 向后兼容：如果 query param 中没有 path，从 URL path 部分解码
                // （旧格式：fb2k://artwork/ENCODED_PATH?type=...）
                if (pathUtf8.empty() && !pathPartFromUrl.empty()) {
                    pathUtf8 = UrlDecode(pathPartFromUrl);
                }
                
                // 注意：不做 '/' → '\\' 全局替换！
                // 旧代码的 std::replace('/', '\\') 会破坏 file:// 前缀
                // g_get_canonical_path 已能正确处理正斜杠和反斜杠路径
                
                LOG("Custom protocol request: path=", pathUtf8.c_str(), ", type=", artworkType.c_str(), ", maxSize=", maxSize);
                
                // 生成 ETag（基于路径 + 类型 + maxSize）
                auto GenerateETag = [](const std::string& path, const std::string& type, int maxSize) -> std::string {
                    // 简单哈希：路径 + 类型 + maxSize
                    size_t hash = std::hash<std::string>{}(path + type + std::to_string(maxSize));
                    char buf[32];
                    snprintf(buf, sizeof(buf), "\"%zx\"", hash);
                    return buf;
                };
                
                std::string etag = GenerateETag(pathUtf8, artworkType, maxSize);
                
                // 处理 OPTIONS 预检请求 (CORS preflight)
                wil::unique_cotaskmem_string method;
                if (SUCCEEDED(request->get_Method(&method)) && method) {
                    std::wstring methodStr(method.get());
                    if (methodStr == L"OPTIONS") {
                        wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                        std::wstring corsHeaders = 
                            L"Access-Control-Allow-Origin: *\r\n"
                            L"Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                            L"Access-Control-Allow-Headers: *\r\n"
                            L"Access-Control-Max-Age: 86400\r\n";
                        if (SUCCEEDED(environment_->CreateWebResourceResponse(
                                nullptr, 204, L"No Content", corsHeaders.c_str(), &response))) {
                            args->put_Response(response.get());
                            LOG("OPTIONS preflight response sent");
                        }
                        return S_OK;
                    }
                }
                
                // 检查 If-None-Match 请求头（支持 304 响应）
                wil::com_ptr<ICoreWebView2HttpRequestHeaders> requestHeaders;
                if (wil::unique_cotaskmem_string ifNoneMatch;
                    SUCCEEDED(request->get_Headers(&requestHeaders)) && requestHeaders &&
                    SUCCEEDED(requestHeaders->GetHeader(L"If-None-Match", &ifNoneMatch)) && ifNoneMatch) {
                    std::wstring ifNoneMatchW(ifNoneMatch.get());
                    std::string ifNoneMatchStr(ifNoneMatchW.begin(), ifNoneMatchW.end());
                    
                    // ETag 匹配，返回 304 Not Modified (带 CORS 头)
                    if (ifNoneMatchStr == etag) {
                        wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                        std::wstring headers304 = L"Access-Control-Allow-Origin: *\r\n";
                        if (SUCCEEDED(environment_->CreateWebResourceResponse(
                                nullptr, 304, L"Not Modified", headers304.c_str(), &response))) {
                            args->put_Response(response.get());
                            LOG("304 Not Modified: ", pathUtf8.c_str());
                        }
                        return S_OK;
                    }
                }
                
                // === Shared scaled artwork cache lookup ===
                std::string cacheKey;
                if (maxSize > 0) {
                    std::vector<std::string> artworkSourcePaths;
                    artwork_internal::TryGetArtworkSourcePathsForPath(
                        pathUtf8, artworkType, artworkSourcePaths
                    );
                    cacheKey = BuildScaledArtworkCacheKey(
                        pathUtf8, artworkType, maxSize, std::move(artworkSourcePaths)
                    );
                }

                // 提取缓存命中检查为扁平化 lambda（消除 4-5 层嵌套）
                auto tryScaledCacheHit = [&]() {
                    if (maxSize <= 0 || cacheKey.empty()) return false;
                    std::lock_guard lock(s_scaledArtworkCacheMutex);
                    auto it = s_scaledArtworkCache.find(cacheKey);
                    if (it == s_scaledArtworkCache.end()) return false;
                    auto& entry = it->second;
                    if (auto age = std::chrono::steady_clock::now() - entry.timestamp;
                        std::chrono::duration_cast<std::chrono::seconds>(age).count() >= SCALED_CACHE_TTL_SEC)
                        return false;
                    // Cache hit: return cached JPEG directly, skip I/O + WIC
                    IStream* cacheStream = SHCreateMemStream(
                        entry.jpegBytes.data(),
                        static_cast<UINT>(entry.jpegBytes.size())
                    );
                    if (!cacheStream) return false;
                    std::wstring mimeTypeW(entry.mimeType.begin(), entry.mimeType.end());
                    std::wstring etagW(etag.begin(), etag.end());
                    std::wstring headers = L"Content-Type: " + mimeTypeW + L"\r\n";
                    headers += L"Cache-Control: public, max-age=604800, immutable\r\n";
                    headers += L"ETag: " + etagW + L"\r\n";
                    headers += L"Access-Control-Allow-Origin: *\r\n";

                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            cacheStream, 200, L"OK", headers.c_str(), &response))) {
                        args->put_Response(response.get());
                        LOG("Scaled artwork cache hit: ", entry.jpegBytes.size(), " bytes, path=", pathUtf8.c_str());
                    }
                    cacheStream->Release();
                    return true;
                };
                if (tryScaledCacheHit()) return S_OK;

                // === Cache miss: full extraction + resize pipeline ===
                artwork_internal::BinaryArtwork artwork;
                bool found = artwork_internal::GetArtworkBinaryForPath(pathUtf8, artworkType, artwork);
                
                if (!found || artwork.bytes.empty()) {
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    std::wstring headers404 = L"Access-Control-Allow-Origin: *\r\n";
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            nullptr, 404, L"Not Found", headers404.c_str(), &response))) {
                        args->put_Response(response.get());
                    }
                    return S_OK;
                }
                
                // Resize if maxSize is specified
                const uint8_t* finalData = artwork.bytes.data();
                size_t finalSize = artwork.bytes.size();
                std::string finalMimeType = artwork.mimeType;
                std::vector<uint8_t> resizedData;
                
                if (maxSize > 0) {
                    int outWidth = 0, outHeight = 0;
                    const char* outMimeType = nullptr;
                    
                    resizedData = ResizeImageWIC(
                        artwork.bytes.data(),
                        artwork.bytes.size(),
                        maxSize,
                        outWidth,
                        outHeight,
                        outMimeType
                    );
                    
                    if (!resizedData.empty()) {
                        finalData = resizedData.data();
                        finalSize = resizedData.size();
                        finalMimeType = outMimeType;
                        LOG("Image resized to ", outWidth, "x", outHeight, ", ", finalSize, " bytes");

                        // Store in scaled artwork cache (eviction logic extracted to reduce nesting)
                        auto evictScaledCache = [&]() {
                            auto now = std::chrono::steady_clock::now();
                            for (auto ci = s_scaledArtworkCache.begin(); ci != s_scaledArtworkCache.end(); ) {
                                if (std::chrono::duration_cast<std::chrono::seconds>(now - ci->second.timestamp).count() >= SCALED_CACHE_TTL_SEC) {
                                    ci = s_scaledArtworkCache.erase(ci);
                                } else {
                                    ++ci;
                                }
                            }
                            if (s_scaledArtworkCache.size() < MAX_SCALED_CACHE) return;
                            // LRU eviction of oldest 20% instead of clear()
                            std::vector<decltype(s_scaledArtworkCache)::iterator> entries;
                            entries.reserve(s_scaledArtworkCache.size());
                            for (auto it = s_scaledArtworkCache.begin(); it != s_scaledArtworkCache.end(); ++it) {
                                entries.push_back(it);
                            }
                            size_t toEvict = MAX_SCALED_CACHE / 5;  // evict 20%
                            if (toEvict > entries.size()) toEvict = entries.size();
                            std::partial_sort(entries.begin(), entries.begin() + toEvict, entries.end(),
                                [](auto a, auto b) { return a->second.timestamp < b->second.timestamp; });
                            for (size_t i = 0; i < toEvict; ++i) {
                                s_scaledArtworkCache.erase(entries[i]);
                            }
                        };
                        {
                            std::lock_guard lock(s_scaledArtworkCacheMutex);
                            if (s_scaledArtworkCache.size() >= MAX_SCALED_CACHE)
                                evictScaledCache();
                            s_scaledArtworkCache[cacheKey] = {
                                resizedData, finalMimeType,
                                std::chrono::steady_clock::now()
                            };
                        }
                    } else if (outWidth > 0) {
                        LOG("Image already ", outWidth, "x", outHeight, ", no resize needed");
                    }
                }
                
                // Build 200 response with artwork data
                IStream* stream = SHCreateMemStream(
                    finalData,
                    static_cast<UINT>(finalSize)
                );
                if (stream) {
                    std::wstring mimeTypeW(finalMimeType.begin(), finalMimeType.end());
                    std::wstring etagW(etag.begin(), etag.end());
                    
                    std::wstring headers = L"Content-Type: " + mimeTypeW + L"\r\n";
                    headers += L"Cache-Control: public, max-age=604800, immutable\r\n";
                    headers += L"ETag: " + etagW + L"\r\n";
                    headers += L"Access-Control-Allow-Origin: *\r\n";
                    
                    wil::com_ptr<ICoreWebView2WebResourceResponse> response;
                    if (SUCCEEDED(environment_->CreateWebResourceResponse(
                            stream, 200, L"OK", headers.c_str(), &response))) {
                        args->put_Response(response.get());
                        LOG("Artwork response sent: ", finalSize, " bytes, type: ", finalMimeType.c_str());
                    }
                    
                    stream->Release();
                }
                
                return S_OK;
                } catch (...) {
                    return S_OK;  // 异常逃逸即放行默认处理，绝不让异常穿透 COM 回调边界
                }
            }
        ).Get(),
        nullptr
    );
    
    if (SUCCEEDED(hr)) {
        LOG("Custom protocol handlers registered (fb2k://artwork, artwork://)");
    } else {
        LOG("Failed to register WebResourceRequested handler, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
}
