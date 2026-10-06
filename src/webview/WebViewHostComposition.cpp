/**
 * WebViewHostComposition.cpp - WebViewHost 的透明背景与 DirectComposition
 *
 * 背景透明度、DWM 效果变更后的刷新，以及 Visual Hosting 模式下 DirectComposition 设备与
 * visual 树的建立和释放。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "window/WindowBehaviorTrace.h"
#include "window/WindowChromeTrace.h"
#include <dcomp.h>

#pragma comment(lib, "dcomp.lib")

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
        
        window_behavior_trace::Com(parentHwnd_, "controller.put_DefaultBackgroundColor", transparent ? 0 : 255);
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
        window_behavior_trace::Com(parentHwnd_, "controller.put_Bounds", adjustedBounds);
        controller_->put_Bounds(adjustedBounds);
        
        // 立即恢复
        window_behavior_trace::Com(parentHwnd_, "controller.put_Bounds", bounds);
        controller_->put_Bounds(bounds);
    }

    // RefreshForDwmEffect 与 Resize 一样会经过 put_Bounds，
    // 这里也要重放当前背景色，避免透明表面回落到 WebView 默认灰底。
    wil::com_ptr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(controller_->QueryInterface(IID_PPV_ARGS(&controller2))) && controller2) {
        const COREWEBVIEW2_COLOR bgColor = isBackgroundTransparent_
            ? COREWEBVIEW2_COLOR{ 0, 0, 0, 0 }
            : COREWEBVIEW2_COLOR{ 255, 30, 30, 30 };
        window_behavior_trace::Com(parentHwnd_, "controller.put_DefaultBackgroundColor",
                                   isBackgroundTransparent_ ? 0 : 255);
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
        window_behavior_trace::Com(parentHwnd_, "dcomp.SetClip", bounds);
        dcompRootVisual_->SetClip(clipRect);
    }
    if (dcompDevice_) {
        window_behavior_trace::Com(parentHwnd_, "dcomp.Commit");
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
    window_behavior_trace::Com(parentHwnd_, "dcomp.SetRoot");
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
    window_behavior_trace::Com(parentHwnd_, "dcomp.Commit");
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
