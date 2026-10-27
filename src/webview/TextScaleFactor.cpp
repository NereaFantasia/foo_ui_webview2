// TextScaleFactor.cpp - 经 WinRT 的 UISettings 读取并监听 Windows 文本大小。
#include "pch.h"
#include "webview/TextScaleFactor.h"
#include "core/WebViewContext.h"
#include "webview/WebViewHost.h"

#include <roapi.h>
#include <windows.ui.viewmanagement.h>
#include <wrl/client.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>

#pragma comment(lib, "runtimeobject.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::Implements;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::Wrappers::HStringReference;
namespace vm = ABI::Windows::UI::ViewManagement;

namespace text_scale {
namespace {

using ChangedHandler = ABI::Windows::Foundation::ITypedEventHandler<vm::UISettings*, IInspectable*>;

struct State {
    bool initialized = false;
    double factor = 1.0;
    ComPtr<vm::IUISettings2> settings;
    EventRegistrationToken token{};
};

State& GetState() {
    static State state;
    return state;
}

double ReadFactor(vm::IUISettings2* settings) {
    double value = 1.0;
    if (!settings || FAILED(settings->get_TextScaleFactor(&value)) || !(value >= 1.0)) {
        return 1.0;
    }
    return value;
}

// 设置变化的通知不在主线程到达，转回主线程后重读，再让每个已登记的 WebView 重设比例。
// 菜单浮层的 WebView 不在 WebViewContext 登记，下一次 Resize 时跟上。
void ApplyChange() {
    auto& state = GetState();
    if (!state.settings) return;
    const double next = ReadFactor(state.settings.Get());
    if (next == state.factor) return;
    state.factor = next;
    auto& context = WebViewContext::GetInstance();
    for (HWND hwnd : context.GetAllInstances()) {
        if (WebViewHost* host = context.GetWebViewHost(hwnd)) {
            host->SyncRasterizationScale();
        }
    }
}

void Initialize(State& state) {
    state.initialized = true;
    ComPtr<IInspectable> instance;
    HRESULT hr = Windows::Foundation::ActivateInstance(
        HStringReference(RuntimeClass_Windows_UI_ViewManagement_UISettings).Get(), &instance);
    if (FAILED(hr) || FAILED(instance.As(&state.settings))) {
        LOG("TextScaleFactor: UISettings unavailable, hr=", pfc::format_hex(static_cast<uint32_t>(hr)),
            "; text size stays 1");
        state.settings.Reset();
        return;
    }
    state.factor = ReadFactor(state.settings.Get());
    // FtmBase 让处理器可在任意线程直接调用，不必排回注册它的主线程套间。
    auto handler = Callback<Implements<RuntimeClassFlags<Microsoft::WRL::ClassicCom>, ChangedHandler, FtmBase>>(
        [](vm::IUISettings*, IInspectable*) -> HRESULT {
            fb2k::inMainThread([] { ApplyChange(); });
            return S_OK;
        });
    if (!handler || FAILED(state.settings->add_TextScaleFactorChanged(handler.Get(), &state.token))) {
        state.token = {};
        LOG("TextScaleFactor: cannot watch text size changes");
    }
}

// 退出时摘掉监听：组件卸载后系统不能再回调进来。
class TextScaleInitQuit : public initquit {
public:
    void on_quit() override {
        auto& state = GetState();
        if (state.settings && state.token.value != 0) {
            state.settings->remove_TextScaleFactorChanged(state.token);
        }
        state.token = {};
        state.settings.Reset();
    }
};

initquit_factory_t<TextScaleInitQuit> g_text_scale_initquit;

}  // namespace

double Current() {
    auto& state = GetState();
    if (!state.initialized) Initialize(state);
    return state.factor;
}

}  // namespace text_scale
