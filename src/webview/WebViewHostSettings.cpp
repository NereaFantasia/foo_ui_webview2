/**
 * WebViewHostSettings.cpp - WebViewHost 的 WebView2 设置
 *
 * 脚本、DevTools、自动填充、SmartScreen 与 app-region 等开关，以及把 Ctrl+P、Ctrl+O、
 * Ctrl+N 转给 foobar2000 的 AcceleratorKeyPressed 处理。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "settings/SecurityConfig.h"
#include <wrl/event.h>
#include <foobar2000/SDK/menu_helpers.h>  // For standard_commands

using namespace Microsoft::WRL;

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
        &acceleratorKeyPressedToken_
    );
    LOG("AcceleratorKeyPressed handler registered for foobar2000 hotkeys");
    
    LOG("WebView2 settings configured");
}
