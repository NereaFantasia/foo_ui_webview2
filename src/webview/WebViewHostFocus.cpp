/**
 * WebViewHostFocus.cpp - WebViewHost 的首次布局与焦点
 *
 * controller 就绪后设置边界与透明背景、注册 GotFocus/LostFocus，以及把键盘焦点交给
 * WebView（MoveFocus，Visual Hosting 模式必需）。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "window/WindowBehaviorTrace.h"
#include "window/WindowChromeTrace.h"
#include <wrl/event.h>

using namespace Microsoft::WRL;

void WebViewHost::SetupWebView() {
    // 设置边界 - 填满父窗口
    RECT bounds;
    GetClientRect(parentHwnd_, &bounds);
    window_behavior_trace::Com(parentHwnd_, "controller.put_Bounds", bounds);
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
        window_behavior_trace::Com(parentHwnd_, "controller.put_DefaultBackgroundColor", 0);
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
        // Controller4 只暴露 get/put_AllowExternalDrop（外部拖放开关），与透明
        // 效果无关。此处仅探测接口可用性并记录，不修改任何设置：
        // AllowExternalDrop 默认已是 TRUE，设为 FALSE 会让
        // CompositionController3 的拖放转发方法返回 E_FAIL。
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

bool WebViewHost::MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON reason) {
    // Visual Hosting 模式下 WebView2 没有子 HWND，宿主窗口拿到 WM_SETFOCUS 后
    // 必须主动把焦点交给 Chromium，否则键盘输入无处可去（Alt+Tab 回来后失灵）。
    if (!controller_) return false;
    window_behavior_trace::Com(parentHwnd_, "controller.MoveFocus", static_cast<long long>(reason));
    HRESULT hr = controller_->MoveFocus(reason);
    if (FAILED(hr)) {
        LOG("WebViewHost::MoveFocus failed, HRESULT: ", pfc::format_hex((uint32_t)hr));
        return false;
    }
    return true;
}
