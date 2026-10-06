/**
 * WebViewHostScripts.cpp - WebViewHost 的注入脚本
 *
 * 在每个文档创建时注入 window.fb2k 桥接脚本、启动辅助脚本与 window.fb SDK。脚本正文
 * 分别在 BridgeBootstrapScript.inl、StartupProbeScript.inl 与 SdkBridgeScript.inl。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "webview/SdkBridgeScript.inl"
#include "webview/BridgeBootstrapScript.inl"
#include "webview/StartupProbeScript.inl"
#include <wrl/event.h>

using namespace Microsoft::WRL;

void WebViewHost::InjectBridgeScript() {
    // 注入全局 bridge 脚本，在所有页面加载前执行
    const wchar_t* bridgeScript = kBridgeBootstrapScript;
    
    webview_->AddScriptToExecuteOnDocumentCreated(bridgeScript, nullptr);
    LOG("Bridge script injected");

    // Internal windowReady auto-signal + overlay-gone detection for visualReady (true reveal authority)
    const wchar_t* readyHelperScript = kStartupProbeScript;
    webview_->AddScriptToExecuteOnDocumentCreated(readyHelperScript, nullptr);
    LOG("Window ready helper script injected");
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
