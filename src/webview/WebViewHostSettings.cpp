/**
 * WebViewHostSettings.cpp - WebViewHost 的 WebView2 设置
 *
 * 脚本、DevTools、自动填充、SmartScreen 与 app-region 等开关，以及 WebView 有焦点时的
 * AcceleratorKeyPressed 处理：F5 重载与转交 foobar2000 快捷键。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "settings/SecurityConfig.h"
#include "webview/HostShortcutRelay.h"
#include <wrl/event.h>

using namespace Microsoft::WRL;

namespace {

// 只让 Chromium 不处理这一次按键，按键仍送到页面。运行时没有这个接口时返回 false
bool DisableBrowserAccelerator(ICoreWebView2AcceleratorKeyPressedEventArgs* args) {
    wil::com_ptr<ICoreWebView2AcceleratorKeyPressedEventArgs2> args2;
    return SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&args2))) &&
           SUCCEEDED(args2->put_IsBrowserAcceleratorKeyEnabled(FALSE));
}

}  // namespace

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
    // 浏览器快捷键开着时 Chromium 自己处理 F5，按键回调把 F5 交给页面前要先对这次按键关掉它
    bool browserAcceleratorKeys = true;
    if (wil::com_ptr<ICoreWebView2Settings3> settings3;
        SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings3))) && !enableDevTools) {
        // 仅在禁用 DevTools 时禁用快捷键
        browserAcceleratorKeys = FAILED(settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE));  // 禁用 F5/Ctrl+R/F12 等
    }

    // ============================================
    // 设置减法：关闭播放器场景用不到的 WebView2 功能，减少每次导航与
    // 资源请求的额外工作。
    // ============================================

    // Settings4: 播放器 UI 无表单登录场景，关闭自动填充与密码保存，
    // 减少每次导航/表单交互的额外工作
    wil::com_ptr<ICoreWebView2Settings4> settings4;
    if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings4)))) {
        settings4->put_IsPasswordAutosaveEnabled(FALSE);
        settings4->put_IsGeneralAutofillEnabled(FALSE);
    }

    // Settings8: 关闭 SmartScreen 信誉检查 — 导航面受控（fb2k://、虚拟主机、
    // dev server），所有 WebView 均为 FALSE 时 SmartScreen 组件进程不再启动。
    // 若未来引入任意外部 URL 浏览能力，须重新评估此项。
    wil::com_ptr<ICoreWebView2Settings8> settings8;
    if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&settings8)))) {
        settings8->put_IsReputationCheckingRequired(FALSE);
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
    // WebView 有焦点时的按键：F5 由组件重载，其余按转发规则交给 foobar2000 快捷键
    // （webview/HostShortcutRelay.h，热键转发规格 §5）
    // ============================================
    controller_->add_AcceleratorKeyPressed(
        Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
            [this, browserAcceleratorKeys](ICoreWebView2Controller* /*sender*/,
                                           ICoreWebView2AcceleratorKeyPressedEventArgs* args) {
                COREWEBVIEW2_KEY_EVENT_KIND kind;
                args->get_KeyEventKind(&kind);
                
                // 只处理 KeyDown 和 SystemKeyDown 事件
                if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN && 
                    kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN) {
                    return S_OK;
                }
                
                UINT key;
                args->get_VirtualKey(&key);

                COREWEBVIEW2_PHYSICAL_KEY_STATUS status{};
                args->get_PhysicalKeyStatus(&status);
                const bool repeat = status.WasKeyDown != FALSE;

                // F5 重载是组件自带的按键，不交给 foobar2000 快捷键，各类窗口一致（规格 §5.6）。可信页面载入完成后，
                // 按键先交给页面：页面的 keydown 监听都没有取消它时，由引导脚本重载（BridgeBootstrapScript.inl）。
                // 页面在加载中、加载失败或来源不可信时，由组件直接重载并置 Handled：加载中页面的监听可能还没挂上，
                // 错误页里由组件重载才确定会重试原地址。开发者模式下置 Handled 同时挡住 Chromium 自己的重载
                if (key == VK_F5) {
                    if (currentDocumentLoaded_ && IsCurrentDocumentTrusted() &&
                        (!browserAcceleratorKeys || DisableBrowserAccelerator(args))) {
                        return S_OK;
                    }
                    args->put_Handled(TRUE);
                    if (!repeat) Reload();
                    return S_OK;
                }

                // 其余按键按转发规则交给 foobar2000 快捷键（规格 §5.1）；编辑快捷键、打字键等由规则排除，留给页面。
                // 执行的命令可能关掉本窗口，TryRelay 返回后只碰 args。
                const HWND page = parentHwnd_;
                if (host_shortcut_relay::TryRelay(page, key, host_shortcut_relay::CurrentModifiers(), repeat)) {
                    args->put_Handled(TRUE);
                }
                return S_OK;
            }
        ).Get(),
        &acceleratorKeyPressedToken_
    );
    LOG("AcceleratorKeyPressed handler registered for foobar2000 hotkeys");
    
    LOG("WebView2 settings configured");
}
