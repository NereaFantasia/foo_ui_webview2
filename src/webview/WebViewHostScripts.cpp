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
#include "webview/HostShortcutRelay.h"
#include <wrl/event.h>
#include <wrl/implements.h>

using namespace Microsoft::WRL;

namespace {

// 引导脚本的按键转发入口，页面经 chrome.webview.hostObjects.sync.fb2kShortcutRelay 同步调用
// （热键转发规格 §12.4）。relay(虚拟键码, 修饰键) 返回是否执行了 foobar2000 快捷键，修饰键取
// MOD_CONTROL、MOD_SHIFT。trustedPage 在宿主已析构或顶层文档不可信时返回 nullptr
class ShortcutRelayObject : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IDispatch> {
public:
    explicit ShortcutRelayObject(std::function<HWND()> trustedPage) : trustedPage_(std::move(trustedPage)) {}

    STDMETHODIMP GetTypeInfoCount(UINT* count) override {
        if (count) *count = 0;
        return S_OK;
    }
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR* names, UINT count, LCID, DISPID* ids) override {
        if (!names || !ids || count == 0) return E_INVALIDARG;
        for (UINT i = 0; i < count; ++i) ids[i] = DISPID_UNKNOWN;
        if (!names[0] || wcscmp(names[0], L"relay") != 0) return DISP_E_UNKNOWNNAME;
        ids[0] = kRelay;
        return count == 1 ? S_OK : DISP_E_UNKNOWNNAME;
    }
    STDMETHODIMP Invoke(DISPID id, REFIID, LCID, WORD flags, DISPPARAMS* params, VARIANT* result, EXCEPINFO*,
                        UINT*) override {
        if (id != kRelay || (flags & DISPATCH_METHOD) == 0) return DISP_E_MEMBERNOTFOUND;
        if (!params || params->cArgs != 2 || params->cNamedArgs != 0) return DISP_E_BADPARAMCOUNT;
        // DISPPARAMS 的参数倒序存放
        LONG vk = 0;
        LONG mods = 0;
        if (!ToLong(params->rgvarg[1], vk) || !ToLong(params->rgvarg[0], mods)) return DISP_E_TYPEMISMATCH;
        bool ran = false;
        if (vk > 0 && vk <= 0xFF && (mods & ~(MOD_CONTROL | MOD_SHIFT)) == 0) {
            const HWND page = trustedPage_();
            // 命令可能销毁本窗口与宿主，之后只写返回值
            if (page) ran = host_shortcut_relay::TryRelayFromPage(page, static_cast<UINT>(vk), static_cast<UINT>(mods));
        }
        if (result) {
            VariantInit(result);
            result->vt = VT_BOOL;
            result->boolVal = ran ? VARIANT_TRUE : VARIANT_FALSE;
        }
        return S_OK;
    }

private:
    static constexpr DISPID kRelay = 1;

    static bool ToLong(const VARIANT& in, LONG& out) {
        VARIANT v;
        VariantInit(&v);
        if (FAILED(VariantChangeType(&v, &in, 0, VT_I4))) return false;
        out = v.lVal;
        return true;
    }

    std::function<HWND()> trustedPage_;
};

}  // namespace

void WebViewHost::InjectBridgeScript() {
    // 注入全局 bridge 脚本，在所有页面加载前执行
    const wchar_t* bridgeScript = kBridgeBootstrapScript;
    
    webview_->AddScriptToExecuteOnDocumentCreated(bridgeScript, nullptr);
    LOG("Bridge script injected");

    // Internal windowReady auto-signal + overlay-gone detection for visualReady (true reveal authority)
    const wchar_t* readyHelperScript = kStartupProbeScript;
    webview_->AddScriptToExecuteOnDocumentCreated(readyHelperScript, nullptr);
    LOG("Window ready helper script injected");

    // 引导脚本里的按键转发要在 keydown 当场知道快捷键是否执行，只能同步调用。宿主析构先把
    // suspendState_->alive 置 false，WebView 晚放的引用不会再碰宿主；来源与 bridge 消息入口同一判定
    auto relay = Make<ShortcutRelayObject>([this, state = suspendState_]() -> HWND {
        if (!state->alive.load(std::memory_order_acquire)) return nullptr;
        return IsCurrentDocumentTrusted() ? parentHwnd_ : nullptr;
    });
    if (relay) {
        VARIANT object;
        VariantInit(&object);
        object.vt = VT_DISPATCH;
        object.pdispVal = relay.Detach();
        const HRESULT hr = webview_->AddHostObjectToScript(L"fb2kShortcutRelay", &object);
        VariantClear(&object);
        if (FAILED(hr)) LOG("Shortcut relay host object not added, HRESULT: ", pfc::format_hex((uint32_t)hr));
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
