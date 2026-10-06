/**
 * WebViewHostVisibility.cpp - WebViewHost 的尺寸、可见性与挂起
 *
 * Resize、SetVisible、SetBoundsVisible、内存占用等级与深度挂起（TrySuspend），以及
 * OpenDevTools、Reload。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "window/WindowBehaviorTrace.h"
#include "window/WindowChromeTrace.h"
#include <wrl/event.h>

using namespace Microsoft::WRL;

void WebViewHost::Resize(int width, int height) {
    RECT bounds = { 0, 0, width, height };
    Resize(bounds);
}

void WebViewHost::Resize(const RECT& bounds) {
    if (controller_) {
        // Resize 在拖拽期间每条 WM_SIZE 都被调用，无条件写控制台会在逐帧路径上
        // 产生真实开销（console::print 需格式化并投递到 fb2k 控制台）。
        if (WindowChromeTrace::AuxiliaryTraceEnabled()) {
            LOG("WebViewHost::Resize - Bounds: ", bounds.left, ",", bounds.top, " to ", bounds.right, ",", bounds.bottom);
        }
        window_behavior_trace::Com(parentHwnd_, "controller.put_Bounds", bounds);
        controller_->put_Bounds(bounds);
        
        // 如果当前是透明模式，resize 后需要重新应用透明背景
        // WebView2 的 put_Bounds 可能会重置背景颜色
        if (isBackgroundTransparent_) {
            wil::com_ptr<ICoreWebView2Controller2> controller2;
            HRESULT hr = controller_->QueryInterface(IID_PPV_ARGS(&controller2));
            if (SUCCEEDED(hr) && controller2) {
                COREWEBVIEW2_COLOR bgColor = { 0, 0, 0, 0 };  // Alpha = 0
                window_behavior_trace::Com(parentHwnd_, "controller.put_DefaultBackgroundColor", 0);
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
        window_behavior_trace::Com(parentHwnd_, "dcomp.SetClip", bounds);
        dcompRootVisual_->SetClip(clipRect);
        if (dcompDevice_) {
            window_behavior_trace::Com(parentHwnd_, "dcomp.Commit");
            dcompDevice_->Commit();
        }
    }
}

bool WebViewHost::SetVisible(bool visible) {
    LogLifecycle("visibility.request",
        std::string("requested=") + (visible ? "1" : "0"));
    if (visible) {
        // 任何恢复可见的动作都使在途 TrySuspend 完成回调过期。
        suspendState_->generation.fetch_add(1, std::memory_order_acq_rel);
    }

    if (!controller_) {
        LogLifecycle("visibility.failed", "reason=no-controller");
        return false;
    }

    HRESULT resumeHr = S_OK;
    if (visible && webview_) {
        // WebView2 官方恢复示例使用 Resume() -> put_IsVisible(TRUE)。虽然显示
        // controller 通常会自动 Resume，锁屏解除的系统切换窗口存在低频竞态；
        // 显式 Resume 使 renderer 恢复先于 DComp surface 重新呈现。
        wil::com_ptr<ICoreWebView2_3> wv3;
        if (SUCCEEDED(webview_.try_query_to(&wv3)) && wv3) {
            // 官方示例直接 Resume，不先读 IsSuspended。避免 get=false 后
            // TrySuspend 才完成的 TOCTOU 窗口。
            window_behavior_trace::Com(parentHwnd_, "webview.Resume");
            resumeHr = wv3->Resume();
        }
    }

    window_behavior_trace::Com(parentHwnd_, "controller.put_IsVisible", visible ? 1 : 0);
    const HRESULT visibleHr = controller_->put_IsVisible(visible ? TRUE : FALSE);
    BOOL actualVisible = FALSE;
    const HRESULT queryHr = controller_->get_IsVisible(&actualVisible);
    const bool confirmed = SUCCEEDED(resumeHr) && SUCCEEDED(visibleHr) &&
        SUCCEEDED(queryHr) && ((actualVisible != FALSE) == visible);

    if (!confirmed) {
        std::ostringstream detail;
        detail << "requested=" << (visible ? 1 : 0)
               << " resumeHr=0x" << pfc::format_hex((uint32_t)resumeHr)
               << " putHr=0x" << pfc::format_hex((uint32_t)visibleHr)
               << " getHr=0x" << pfc::format_hex((uint32_t)queryHr)
               << " actual=" << (actualVisible != FALSE ? 1 : 0);
        LogLifecycle("visibility.failed", detail.str());
        console::printf(
            "[WebView2 UI] visibility transition failed (requested=%d resume=0x%08X put=0x%08X get=0x%08X actual=%d)",
            visible ? 1 : 0, (unsigned)resumeHr, (unsigned)visibleHr,
            (unsigned)queryHr, actualVisible != FALSE ? 1 : 0);
        return false;
    }

    // 可见性记账：put_IsVisible 即页面 hidden/visible 的执行动作。生产侧的
    // 可再生流节约（频谱 / timeHighRes）以此为权威判定，恢复后下一拍自然出帧。
    suspendState_->pageHidden.store(!visible, std::memory_order_release);

    if (visible && dcompDevice_) {
        // 锁屏解除后即使 controller 已恢复，DComp target 也可能尚未收到新的
        // present。提交现有视觉树，后续 surface convergence 再执行 bounds nudge。
        window_behavior_trace::Com(parentHwnd_, "dcomp.Commit");
        dcompDevice_->Commit();
    }
    LogLifecycle("visibility.confirmed",
        std::string("requested=") + (visible ? "1" : "0"));
    return true;
}

void WebViewHost::SetMemoryUsageLow(bool low) {
    if (!webview_) return;
    // put_MemoryUsageTargetLevel 属于 ICoreWebView2_19。当前 pin 的 SDK 已支持；
    // 若用户安装的 WebView2 Runtime 偏老导致 query 失败，则安全 no-op。
    wil::com_ptr<ICoreWebView2_19> wv19;
    if (SUCCEEDED(webview_.try_query_to(&wv19)) && wv19) {
        window_behavior_trace::Com(parentHwnd_, "webview.put_MemoryUsageTargetLevel", low ? 1 : 0);
        wv19->put_MemoryUsageTargetLevel(
            low ? COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW
                : COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_NORMAL);
    }
}

// ==========================================================================
// 深度挂起（TrySuspend）
// 调用前提：SetVisible(false) 已执行（页面 hidden、事件门控已生效）。
// 互斥铁律：TrySuspend 自动置 Low、Resume/put_IsVisible(TRUE) 自动回 Normal，
// 本路径成功后不得再手动 SetMemoryUsageLow（由 MainWindow 投影应用层保证）。
// ==========================================================================
void WebViewHost::TrySuspendDeep() {
    if (!webview_) {
        LogLifecycle("suspend.skipped", "reason=no-webview");
        return;
    }

    // 本请求的代数：回调只认它；期间任何 SetVisible(true) 或新的挂起请求
    // 都会使其过期（"最小化→秒恢复"时恢复动作先于完成回调到达）。
    const auto state = suspendState_;
    const uint64_t generation =
        state->generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    LogLifecycle("suspend.request",
        "generation=" + std::to_string(generation));

    wil::com_ptr<ICoreWebView2_3> wv3;
    if (FAILED(webview_.try_query_to(&wv3)) || !wv3) {
        // Runtime 过老：深挂起降级为现状 Low 路径（失败是降级不是错误）
        console::print("[WebView2 UI] deep suspend unavailable (ICoreWebView2_3), falling back to low memory target");
        SetMemoryUsageLow(true);
        LogLifecycle("suspend.unavailable",
            "generation=" + std::to_string(generation));
        return;
    }

    const wil::com_ptr<ICoreWebView2> webview = webview_;
    window_behavior_trace::Com(parentHwnd_, "webview.TrySuspend");
    HRESULT hr = wv3->TrySuspend(
        Callback<ICoreWebView2TrySuspendCompletedHandler>(
            [state, webview, generation](HRESULT errorCode, BOOL isSuccessful) -> HRESULT {
                std::ostringstream completion;
                completion << "event=suspend.completed"
                           << " uptimeMs=" << GetTickCount64()
                           << " pid=" << GetCurrentProcessId()
                           << " tid=" << GetCurrentThreadId()
                           << " generation=" << generation
                           << " currentGeneration="
                           << state->generation.load(std::memory_order_acquire)
                           << " alive="
                           << (state->alive.load(std::memory_order_acquire) ? 1 : 0)
                           << " hidden="
                           << (state->pageHidden.load(std::memory_order_acquire) ? 1 : 0)
                           << " hr=0x" << pfc::format_hex((uint32_t)errorCode)
                           << " success=" << (isSuccessful != FALSE ? 1 : 0);
                WebViewHost::WriteLifecycleLog(completion.str());
                if (!state->alive.load(std::memory_order_acquire)) return S_OK;

                if (generation != state->generation.load(std::memory_order_acquire)) {
                    // 代数过期：请求发出后页面已恢复可见（put_IsVisible(TRUE)
                    // 自动 resume）或有更新的挂起请求接管。不写状态、不回退 Low；
                    // 若页面此刻应可见而运行时仍报告挂起（回调晚于恢复的窗口期），
                    // 立即 Resume 兜底。
                    if (!state->pageHidden.load(std::memory_order_acquire)) {
                        wil::com_ptr<ICoreWebView2_3> wv3Late;
                        if (SUCCEEDED(webview.try_query_to(&wv3Late)) && wv3Late) {
                            BOOL suspended = FALSE;
                            if (SUCCEEDED(wv3Late->get_IsSuspended(&suspended)) && suspended) {
                                wv3Late->Resume();
                                console::print("[WebView2 UI] stale deep-suspend completion resumed (page visible again)");
                            }
                        }
                    }
                    return S_OK;
                }
                if (SUCCEEDED(errorCode) && isSuccessful) {
                    console::print("[WebView2 UI] deep suspend engaged (renderer timers frozen)");
                } else {
                    // sleeping tabs 排除条件（如 DevTools 打开）→ 回退 Low
                    console::printf(
                        "[WebView2 UI] deep suspend declined (hr=0x%08X success=%d), falling back to low memory target",
                        (unsigned)errorCode, (int)(isSuccessful != FALSE));
                    wil::com_ptr<ICoreWebView2_19> wv19;
                    if (SUCCEEDED(webview.try_query_to(&wv19)) && wv19) {
                        wv19->put_MemoryUsageTargetLevel(
                            COREWEBVIEW2_MEMORY_USAGE_TARGET_LEVEL_LOW);
                    }
                }
                return S_OK;
            })
            .Get());
    if (FAILED(hr)) {
        LogLifecycle("suspend.call-failed",
            "generation=" + std::to_string(generation) +
            " hr=" + std::to_string((long long)hr));
        console::printf(
            "[WebView2 UI] TrySuspend call failed (hr=0x%08X), falling back to low memory target",
            (unsigned)hr);
        SetMemoryUsageLow(true);
    }
}

bool WebViewHost::IsSuspended() const {
    if (!webview_) return false;
    wil::com_ptr<ICoreWebView2_3> wv3;
    if (FAILED(webview_.try_query_to(&wv3)) || !wv3) return false;
    BOOL suspended = FALSE;
    if (FAILED(wv3->get_IsSuspended(&suspended))) return false;
    return suspended != FALSE;
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
    window_behavior_trace::Com(parentHwnd_, "webview.Reload");
    const HRESULT hr = webview_ ? webview_->Reload() : E_POINTER;
    LogLifecycle("page.reload",
        "hr=" + std::to_string((long long)hr));
}
