// MainWindowActivation.cpp — 主窗口的激活处理：期望置顶守护、WM_ACTIVATE / WM_ACTIVATEAPP 与激活态投影
#include "pch.h"
#include "window/MainWindow.h"
#include "window/MainWindowInternal.h"
#include "window/MenuOverlayHost.h"
#include "window/WindowChromeTrace.h"
#include "window/WindowManager.h"
#include "webview/WebViewHost.h"
#include "api/EventEmit.h"
#include "api/generated/WindowSchema.h"
#include <sstream>

using namespace mainwindow_detail;

void MainWindow::SetExpectedTopmost(bool expected) {
    if (expectedTopmost_ == expected) return;
    expectedTopmost_ = expected;
    std::ostringstream stream;
    stream << "[ActivationEvidence] source=MainWindow::SetExpectedTopmost"
           << " t=" << WindowChromeTrace::RelativeMs() << "ms"
           << " expectedTopmost=" << WindowChromeTrace::BoolText(expected);
    S_EmitEvidenceLine(stream.str());
}

void MainWindow::OnActivate(WPARAM wParam, LPARAM lParam) {
    const bool active = (LOWORD(wParam) != WA_INACTIVE);

    // 自绘菜单夺走前台期间维持激活状态（原生菜单语义）：跳过 inactive backdrop
    // 切换与 window:deactivated 广播。菜单关闭后由 FinalizeHide 归还前台，主窗口
    // 收到的 WM_ACTIVATE(TRUE) 与保持中的状态一致，无二次切换。外点击他窗时对端
    // 非菜单面，不进此分支，真实失活语义不受影响。
    if (!active && MenuOverlayHost::GetInstance().ShouldHoldOwnerActivation(
            reinterpret_cast<HWND>(lParam))) {
        ApplyWindowActivationState(true, "WM_ACTIVATE.menuOverlayHold");
        return;
    }

    // 第三波防御：用户点击 desktopLyrics 等 overlay 后，WebView2 在 popup 与主窗口两个
    // controller 间回授焦点，会让主窗口在点击后短时内被无意激活并覆盖外部应用。
    // 若主窗口在 overlay 交互窗口期（300ms）内被激活，则视为误激活：把主窗口压回外部
    // 窗口之后，并尝试把前台还给外部应用（理想），否则引到不可感知的 sink（兜底）。
    // 关键改进：overlayRedirect 只执行一次 SetForegroundWindow 操作，
    // 后续在同一窗口期内的激活消息直接静默跳过（不做 backdrop 切换、不调 SetForegroundWindow、
    // 不发广播），避免 SetForegroundWindow → WM_ACTIVATEAPP → overlayRedirect → SetForegroundWindow
    // 的震荡死循环。
    if (active && WindowManager::GetInstance().IsOverlayInteractionRecent(300)) {
        // -- 后续抑制：如果 redirect 已在执行中，静默跳过 --
        // 阻止 SetForegroundWindow 触发的 WM_ACTIVATEAPP 再次进入 redirect，消除震荡。
        if (overlayRedirectInProgress_) {
            // 仅保持 inactive 外观，不做任何 SetForegroundWindow 操作
            ApplyWindowActivationState(false, "WM_ACTIVATE.overlayRedirect.suppressed");
            return;
        }

        // -- 第一次 redirect：标记 in-progress，执行一次性撤销 --
        overlayRedirectInProgress_ = true;
        lastOverlayRedirectTick_ = GetTickCount64();

        HWND ext = WindowManager::GetInstance().GetOverlayInteractionForeground();
        HWND sink = WindowManager::GetInstance().GetActivationSinkHwnd();
        const bool extValid = ext && IsWindow(ext) && ext != hwnd_;
        // 跨 band 的相对插入必然改写主窗口 WS_EX_TOPMOST，一律跳过：
        // 升段（参照 topmost、主窗口非 topmost）= 隐式授予置顶 → 意外覆盖全桌面；
        // 降段（参照非 topmost、主窗口 topmost）= 用户显式 alwaysOnTop 被悄悄剥除。
        // 同 band 内移动不改置顶状态，安全执行。升段场景下主窗口位于非 topmost
        // 段顶部，天然已排在 topmost 参照之后，跳过即正确；其余残差由
        // WM_WINDOWPOSCHANGED 的期望置顶守护兜底。
        const bool extTopmost = extValid &&
            (GetWindowLongW(ext, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
        const bool selfTopmost =
            (GetWindowLongW(hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;

        // 把主窗口 z-order 压回外部窗口之后，避免即便仍短暂前台也覆盖外部应用。
        if (extValid && extTopmost == selfTopmost) {
            SetWindowPos(hwnd_, ext, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        // 优先把前台还给外部应用；若未生效（主窗口仍前台）再引到 sink（同进程，可靠）。
        if (extValid) SetForegroundWindow(ext);
        if (GetForegroundWindow() == hwnd_ && sink && IsWindow(sink) && sink != hwnd_) {
            SetForegroundWindow(sink);
        }

        {
            std::ostringstream stream;
            stream << "[ActivationEvidence] source=MainWindow::OnActivate.overlayRedirect"
                   << " t=" << WindowChromeTrace::RelativeMs() << "ms"
                   << " ext=0x" << pfc::format_hex((size_t)ext)
                   << " extTopmost=" << WindowChromeTrace::BoolText(extTopmost)
                   << " selfTopmost=" << WindowChromeTrace::BoolText(selfTopmost)
                   << " sink=0x" << pfc::format_hex((size_t)sink)
                   << " mainHwnd=0x" << pfc::format_hex((size_t)hwnd_)
                   << " fgAfter=0x" << pfc::format_hex((size_t)::GetForegroundWindow());
            S_EmitEvidenceLine(stream.str());
        }
        // 不应用 active 外观：保持主窗口"未激活"语义。
        ApplyWindowActivationState(false, "WM_ACTIVATE.overlayRedirect");
        return;
    }

    // -- overlay 交互窗口期结束：清除 redirect 标志 --
    // 也用超时兜底（500ms），防止标志永远不被清除。
    if (overlayRedirectInProgress_) {
        const unsigned long long now = GetTickCount64();
        if (!WindowManager::GetInstance().IsOverlayInteractionRecent(500) ||
            (now - lastOverlayRedirectTick_ > 500)) {
            overlayRedirectInProgress_ = false;
        }
    }

    const bool shouldApplyBackdrop = !startupRevealPending_ && !startupRevealSettling_;
    std::string extra = std::string("wParam=") + std::to_string((unsigned long long)wParam) +
        " nextActive=" + WindowChromeTrace::BoolText(active) +
        " applyBackdrop=" + WindowChromeTrace::BoolText(shouldApplyBackdrop) +
        " startupRevealSettling=" + WindowChromeTrace::BoolText(startupRevealSettling_);
    TraceWindowPhase("WM_ACTIVATE", WindowChromeTrace::BoolText(active), nullptr,
        GetTraceEffect(active), lastBackdropEffect_.empty() ? "-" : lastBackdropEffect_.c_str(),
        extra.c_str());

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=WM_ACTIVATE"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " wParam=" << (unsigned long long)wParam
               << " nextActive=" << WindowChromeTrace::BoolText(active)
               << " shouldApplyBackdrop=" << WindowChromeTrace::BoolText(shouldApplyBackdrop)
               << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_);
        S_EmitEvidenceLine(stream.str());
    }

    ApplyWindowActivationState(active, "WM_ACTIVATE");
}

void MainWindow::OnActivateApp(bool active) {
    // -- popup 误激活抑制（与 OnActivate 中相同的逻辑）--
    // WM_ACTIVATEAPP 是线程级消息，Chromium 激活 popup 时同线程的 main 也收到 active=true。
    // 所有 popup（miniplayer、desktopLyrics 等）在 Visual Hosting 模式下点击可交互元素时，
    // Chromium 都会调 SetForegroundWindow 导致主窗口被无意激活。
    // 必须在此拦截，否则 WM_ACTIVATEAPP(active=true) 会先于 WM_ACTIVATE 把外观切换到 active，
    // 造成短暂的视觉闪烁（active→inactive→active→suppressed 的震颤）。
    if (active && WindowManager::GetInstance().IsOverlayInteractionRecent(300)) {
        if (overlayRedirectInProgress_) {
            // 抑制后续激活消息，不做任何外观切换
            ApplyWindowActivationState(false, "WM_ACTIVATEAPP.overlayRedirect.suppressed");
            return;
        }
        // WM_ACTIVATEAPP may arrive before WM_ACTIVATE. Do not mark the redirect as
        // in-progress here, otherwise the following WM_ACTIVATE would be suppressed
        // before it can run the one real z-order / foreground undo path.
        ApplyWindowActivationState(false, "WM_ACTIVATEAPP.overlayRedirect.pending");
        return;
    }

    // -- overlay 交互窗口期结束：清除 redirect 标志 --
    if (overlayRedirectInProgress_) {
        const unsigned long long now = GetTickCount64();
        if (!WindowManager::GetInstance().IsOverlayInteractionRecent(500) ||
            (now - lastOverlayRedirectTick_ > 500)) {
            overlayRedirectInProgress_ = false;
        }
    }

    const bool shouldApplyBackdrop = !startupRevealPending_ && !startupRevealSettling_;
    std::string extra = std::string("active=") + WindowChromeTrace::BoolText(active) +
        " applyBackdrop=" + WindowChromeTrace::BoolText(shouldApplyBackdrop) +
        " startupRevealSettling=" + WindowChromeTrace::BoolText(startupRevealSettling_);
    TraceWindowPhase("WM_ACTIVATEAPP", WindowChromeTrace::BoolText(active), nullptr,
        GetTraceEffect(active), lastBackdropEffect_.empty() ? "-" : lastBackdropEffect_.c_str(),
        extra.c_str());

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=WM_ACTIVATEAPP"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " nextActive=" << WindowChromeTrace::BoolText(active)
               << " shouldApplyBackdrop=" << WindowChromeTrace::BoolText(shouldApplyBackdrop)
               << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_);
        S_EmitEvidenceLine(stream.str());
    }

    ApplyWindowActivationState(active, "WM_ACTIVATEAPP");
}

void MainWindow::ApplyWindowActivationState(bool active, const char* reason) {
    const bool previousActive = isActive_;
    isActive_ = active;
    const bool activationChanged = previousActive != isActive_;
    const bool shouldApplyBackdrop = !startupRevealPending_ && !startupRevealSettling_;
    bool attemptedBackdropApply = false;
    bool usedAuthoritativeChromeReapply = false;
    bool backdropApplyResult = false;

    if (WindowChromeTrace::AuxiliaryTraceEnabled()) {
        LOG("MainWindow: Activation state update, reason=", reason,
            " previousActive=", previousActive ? "true" : "false",
            " active=", isActive_ ? "true" : "false",
            " applyBackdrop=", shouldApplyBackdrop ? "true" : "false");
    }

    // 启动 reveal 期间，直到 post-show native chrome settle 完成前都跳过 backdrop apply。
    // ShowWindow 内部的 WM_ACTIVATE 只更新 active 状态，真正的首个 authoritative apply
    // 由 ReapplyNativeChromeAfterStartupReveal 在 settle 点统一完成，避免 DWM coalesce 抢跑写入。
    if (shouldApplyBackdrop && activationChanged) {
        attemptedBackdropApply = true;
        backdropApplyResult = ApplyBackdropPolicyForActivation(isActive_, false);
    }
    
    // 通知 WebView 激活状态变化（可用于更新标题栏样式）
    bool emittedWindowActivated = false;
    if (previousActive != isActive_ && webView_ && webView_->IsReady()) {
        api::window::ActivatedPayload activated;
        activated.active = isActive_;
        api::emit::Post<api::window::events::Activated>(*webView_, activated);
        emittedWindowActivated = true;
    }

    const bool emittedWindowStateChanged = BroadcastWindowStateChangedIfNeeded();

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=ApplyWindowActivationState"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " reason=" << (reason && reason[0] ? reason : "-")
               << " previousActive=" << WindowChromeTrace::BoolText(previousActive)
               << " active=" << WindowChromeTrace::BoolText(isActive_)
               << " shouldApplyBackdrop=" << WindowChromeTrace::BoolText(shouldApplyBackdrop)
               << " attemptedBackdropApply=" << WindowChromeTrace::BoolText(attemptedBackdropApply)
               << " backdropApplyResult=" << WindowChromeTrace::BoolText(backdropApplyResult)
               << " usedAuthoritativeChromeReapply=" << WindowChromeTrace::BoolText(usedAuthoritativeChromeReapply)
               << " emittedWindowActivated=" << WindowChromeTrace::BoolText(emittedWindowActivated)
               << " emittedWindowStateChanged=" << WindowChromeTrace::BoolText(emittedWindowStateChanged)
               << " webViewReady=" << WindowChromeTrace::BoolText(webView_ && webView_->IsReady())
               << " statePayload.active=" << WindowChromeTrace::BoolText(isActive_)
               << " statePayload.isActive=" << WindowChromeTrace::BoolText(isActive_);
        S_EmitEvidenceLine(stream.str());
    }
}
