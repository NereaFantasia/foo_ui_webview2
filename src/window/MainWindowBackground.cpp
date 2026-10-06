// MainWindowBackground.cpp — 主窗口的后台挂起投影、完全遮挡检测与轮询、隐藏后恢复、页面健康探测与 WebView 重建
#include "pch.h"
#include "window/MainWindow.h"
#include "window/WindowBehaviorTrace.h"
#include "webview/WebViewHost.h"
#include "webview/WebViewCrashPolicy.h"
#include "settings/SecurityConfig.h"
#include "api/BridgeCore.h"

bool MainWindow::RestoreSurfaceAfterHidden(const char* reason) {
    const char* restoreReason = (reason && reason[0]) ? reason : "hidden-restore";

    if (!hwnd_ || !IsWindow(hwnd_) || !IsWindowVisible(hwnd_) || isMinimized_) {
        return false;
    }

    if (startupRevealPending_ || !startupRevealCommitted_ || startupRevealSettling_) {
        return false;
    }

    if (!webView_ || !webView_->IsReady()) {
        return false;
    }

    LogSurfaceDiagnostics((std::string("RestoreSurfaceAfterHidden.enter/") + restoreReason).c_str());
    LogWebViewLifecycle("restore.begin", std::string("reason=") + restoreReason);
    EnsureAuthoritativeNativeChrome(restoreReason);
    // 恢复顺序必须先 put_IsVisible(TRUE)：挂起态由运行时自动 Resume 并把内存目标
    // 恢复为 Normal。随后 SetBgSuspend 以当前 reasons 重新投影，必要时（例如仍被
    // covered）再手动补 Low；禁止在 TrySuspend 尚未 Resume 时手动改内存目标。
    const bool visible = RefreshStartupRevealSurface();
    const bool restored = visible && EnsureSurfaceConvergedAfterNativeFrame(restoreReason);

    if (auto* controller = webView_->GetController()) {
        window_behavior_trace::Com(hwnd_, "controller.NotifyParentWindowPositionChanged");
        controller->NotifyParentWindowPositionChanged();
    }

    LogSurfaceDiagnostics((std::string("RestoreSurfaceAfterHidden.exit/") + restoreReason).c_str());
    LogWebViewLifecycle(restored ? "restore.surface-confirmed" : "restore.surface-failed",
                        std::string("reason=") + restoreReason);
    CaptureRuntimeDomProbe(std::string("RestoreSurfaceAfterHidden.exit/") + restoreReason);
    if (restored) {
        bgSuspendHealthRecoveryAttempts_ = 0;
        KillTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID);
        SetTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID,
                 PAGE_HEALTH_RETRY_DELAY_MS, nullptr);
    } else if (IsWebViewProcessDead()) {
        // 收敛失败本身是已知正常瞬态（安全桌面切换、DComp 首帧未提交），由
        // RESTORE_SURFACE_CONVERGE_TIMER_ID 重试即可，不能据此重建。只有僵尸
        // 标记（ProcessFailed 判定不可恢复时置位）才是确定性失效，此时重试永
        // 远不会成功，必须重建。
        ScheduleProcessDeadRebuild("restore-process-dead");
    }
    // 任何隐藏→可见的恢复后，重新评估是否（仍）被完全覆盖
    ScheduleCoverReevaluation();
    return restored;
}

void MainWindow::ProbeRestoredPageHealth(const char* reason) {
    if (!hwnd_ || !IsWindow(hwnd_) || !webView_ ||
        bgSuspendPageHidden_ || isMinimized_ || pageHealthProbePending_) {
        return;
    }

    LogSurfaceDiagnostics((std::string("PageHealthProbe/") +
        (reason && reason[0] ? reason : "restore") + "/start").c_str());
    const HWND expectedHwnd = hwnd_;
    WebViewHost* const expectedHost = webView_.get();
    const uint64_t generation = ++pageHealthProbeGeneration_;
    LogWebViewLifecycle("health.probe-start",
        std::string("reason=") + (reason && reason[0] ? reason : "restore") +
        " generation=" + std::to_string(generation));
    pageHealthProbePending_ = true;
    KillTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID);
    SetTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID,
             PAGE_HEALTH_PROBE_TIMEOUT_MS, nullptr);
    webView_->ProbePageHealth([expectedHwnd, expectedHost, generation](bool healthy) noexcept {
        try {
            if (!expectedHwnd || !IsWindow(expectedHwnd)) return;
            auto* self = reinterpret_cast<MainWindow*>(
                GetWindowLongPtrW(expectedHwnd, GWLP_USERDATA));
            if (!self || self->hwnd_ != expectedHwnd ||
                self->webView_.get() != expectedHost || self->bgSuspendPageHidden_ ||
                !self->pageHealthProbePending_ ||
                self->pageHealthProbeGeneration_ != generation) {
                return;
            }
            KillTimer(expectedHwnd, PAGE_HEALTH_RETRY_TIMER_ID);
            self->pageHealthProbePending_ = false;
            self->HandleRestoredPageHealthResult(healthy, "page-health-callback");
        } catch (...) {
            // WebViewHost also guards its COM boundary; keep this callback safe
            // if future recovery bookkeeping gains throwing operations.
        }
    });
}

void MainWindow::HandleRestoredPageHealthResult(bool healthy, const char* reason) {
    const char* probeReason = reason && reason[0] ? reason : "page-health";
    const auto action = background_suspend_policy::DecidePageHealthRecovery(
        healthy, bgSuspendHealthRecoveryAttempts_, PAGE_HEALTH_RECOVERY_LIMIT);
    const char* actionText = action == background_suspend_policy::PageHealthRecoveryAction::Accept
        ? "accept"
        : (action == background_suspend_policy::PageHealthRecoveryAction::Reload
            ? "reload" : "rebuild");
    LogWebViewLifecycle("health.decision",
        std::string("reason=") + probeReason +
        " healthy=" + (healthy ? "1" : "0") +
        " attempts=" + std::to_string(bgSuspendHealthRecoveryAttempts_) +
        " action=" + actionText);
    if (action == background_suspend_policy::PageHealthRecoveryAction::Accept) {
        bgSuspendHealthRecoveryAttempts_ = 0;
        LogSurfaceDiagnostics((std::string(probeReason) + "/healthy").c_str());
        return;
    }

    LogSurfaceDiagnostics((std::string(probeReason) + "/unhealthy").c_str());
    if (action == background_suspend_policy::PageHealthRecoveryAction::Rebuild) {
        console::printf(
            "[WebView2 UI] page health did not recover after %u reload attempts; rebuilding",
            bgSuspendHealthRecoveryAttempts_);
        RebuildWebViewAfterHealthFailure(probeReason);
        return;
    }

    ++bgSuspendHealthRecoveryAttempts_;
    webView_->Reload();
    KillTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID);
    SetTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID,
             PAGE_HEALTH_RETRY_DELAY_MS, nullptr);
}

void MainWindow::ScheduleProcessDeadRebuild(const char* reason) {
    const char* rebuildReason = (reason && reason[0]) ? reason : "browser-process-exited";
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return;
    }

    const auto decision = webview_crash_policy::DecideBrowserExitRebuild(
        browserExitRebuildAttempts_,
        browserExitRebuildWindowStartMs_,
        GetTickCount64(),
        webViewRebuildInProgress_);
    browserExitRebuildAttempts_ = decision.nextAttempts;
    browserExitRebuildWindowStartMs_ = decision.nextWindowStartMs;

    if (!decision.shouldRebuild) {
        LogWebViewLifecycle("rebuild.skipped",
            std::string("reason=") + rebuildReason +
            " limitExhausted=" + (decision.limitExhausted ? "1" : "0") +
            " attempts=" + std::to_string(decision.nextAttempts));
        if (decision.limitExhausted) {
            console::printf("[WebView2 UI] WebView browser process keeps dying "
                            "(%u rebuilds within the throttle window); giving up. "
                            "Third-party overlay/injection hooks are a common cause "
                            "- see webview_crash.log. Restart foobar2000 to retry.",
                            decision.nextAttempts);
        }
        return;
    }

    LogWebViewLifecycle("rebuild.scheduled",
        std::string("reason=") + rebuildReason +
        " attempts=" + std::to_string(decision.nextAttempts));
    // 延迟到下一消息周期：ProcessFailed 是 COM 回调，不能在其栈上销毁宿主。
    KillTimer(hwnd_, PROCESS_DEAD_REBUILD_TIMER_ID);
    SetTimer(hwnd_, PROCESS_DEAD_REBUILD_TIMER_ID,
             PROCESS_DEAD_REBUILD_DELAY_MS, nullptr);
}

bool MainWindow::RebuildWebViewAfterHealthFailure(const char* reason) {
    if (webViewRebuildInProgress_ || !hwnd_ || !IsWindow(hwnd_)) {
        return false;
    }

    webViewRebuildInProgress_ = true;
    LogWebViewLifecycle("rebuild.begin",
        std::string("reason=") + (reason && reason[0] ? reason : "page-health"));
    bgSuspendHealthRecoveryAttempts_ = 0;
    bgSuspendPageHidden_ = false;
    bgSuspendDeepRequested_ = false;
    pageHealthProbePending_ = false;
    ++pageHealthProbeGeneration_;
    KillTimer(hwnd_, PAGE_HEALTH_RETRY_TIMER_ID);
    KillTimer(hwnd_, BG_RESUME_RETRY_TIMER_ID);
    KillTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID);
    KillTimer(hwnd_, PROCESS_DEAD_REBUILD_TIMER_ID);

    console::printf("[WebView2 UI] rebuilding unhealthy WebView, reason=%s",
                    reason && reason[0] ? reason : "page-health");
    const HWND rebuildHwnd = hwnd_;
    BridgeCore::GetInstance().SetWebView(nullptr);
    DestroyWebView();
    const bool started = InitializeWebView(rebuildHwnd, WebViewPanelMode::Standalone);
    LogWebViewLifecycle(started ? "rebuild.started" : "rebuild.start-failed",
        std::string("reason=") + (reason && reason[0] ? reason : "page-health"));
    if (!started) {
        webViewRebuildInProgress_ = false;
        console::print("[WebView2 UI] WebView rebuild failed to start");
    }
    return started;
}

void MainWindow::SetBgSuspend(unsigned reason, bool active, const char* why) {
    const unsigned prev = bgSuspendReasons_;
    if (active) bgSuspendReasons_ |= reason;
    else        bgSuspendReasons_ &= ~reason;
    if (!webView_) return;

    // 仅保护"窗口尚未创建/已销毁"。最小化/托盘隐藏是投影驱动的 reasons
    //（kMinimized/kTrayHidden），必须落到下方投影分支；不再有 isMinimized_/
    // !IsWindowVisible 旁路豁免。
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    const bool keepAlive = security_config::IsAutomationKeepAliveActive();
    const auto currentProjection =
        background_suspend_policy::Project(bgSuspendReasons_, keepAlive);
    const bool deepSuspendEnabled = security_config::IsDeepSuspendEnabled();

    // 原因未变化时通常是 no-op；但上次 hidden→visible 命令失败后，实际状态
    // 仍与投影不一致，重试定时器必须能再次进入同一收敛逻辑。
    if (bgSuspendReasons_ == prev &&
        currentProjection.hideSurface == bgSuspendPageHidden_) {
        return;
    }

    {
        std::ostringstream detail;
        detail << "why=" << (why && why[0] ? why : "-")
               << " reason=0x" << pfc::format_hex(reason)
               << " active=" << (active ? 1 : 0)
               << " prevReasons=0x" << pfc::format_hex(prev)
               << " reasons=0x" << pfc::format_hex(bgSuspendReasons_)
               << " projectedHidden=" << (currentProjection.hideSurface ? 1 : 0)
               << " projectedLow=" << (currentProjection.useLowMemory ? 1 : 0)
               << " projectedDeep=" << (currentProjection.deepSuspend ? 1 : 0)
               << " deepEnabled=" << (deepSuspendEnabled ? 1 : 0)
               << " keepAlive=" << (keepAlive ? 1 : 0);
        LogWebViewLifecycle("background.project", detail.str());
    }

    // CDP 自动化 keep-alive 豁免命中时留一条运行时证据（仅状态迁移时到达此处，
    // 不刷屏）。
    if (keepAlive && !currentProjection.hideSurface &&
        background_suspend_policy::Project(bgSuspendReasons_).hideSurface) {
        console::print("[WebView2 UI] background suspend skipped (automation keep-alive)");
    }

    // 迁移检测以"上次实际应用的隐藏状态"为准，而非用当前 keep-alive 值重算
    // prev 投影：挂起期间子开关变化会漏检 hidden→visible 迁移。
    if (currentProjection.hideSurface && !bgSuspendPageHidden_) {
        // 锁屏/最小化/托盘隐藏都没有可见 surface 消费者，可暂停页面并回收内存。
        // 顺序：SetVisible(false)（页面 hidden，事件门控随之生效）
        // → TrySuspend。深挂起与手动 Low 互斥（TrySuspend 自动置 Low、Resume 自动
        // 回 Normal）；advconfig 关闭时退化为现状 Low 路径（尾部统一投影）。
        const bool hidden = webView_->SetVisible(false);
        const auto applied = background_suspend_policy::ApplyVisibilityResult(
            bgSuspendPageHidden_, /*desiredHidden=*/true, hidden);
        bgSuspendPageHidden_ = applied.pageHidden;
        bgSuspendResumeRetryCount_ = 0;
        KillTimer(hwnd_, BG_RESUME_RETRY_TIMER_ID);
        if (hidden && currentProjection.deepSuspend && deepSuspendEnabled) {
            bgSuspendDeepRequested_ = true;
            webView_->TrySuspendDeep();
        }
        LogSurfaceDiagnostics((std::string("BgSuspend.hide/") + (why ? why : "")).c_str());
    }

    if (!currentProjection.hideSurface && bgSuspendPageHidden_) {
        // 即使仍处于 covered/Low，也必须先恢复 surface，供窗口本体与 DWM 预览消费。
        // put_IsVisible(TRUE)（在 RefreshStartupRevealSurface 内）会自动 resume 挂起态。
        bool restored = RestoreSurfaceAfterHidden(why ? why : "bg-resume");
        if (!restored) {
            // 启动 reveal 门槛或窗口尚未真正可见（如 WM_SHOWWINDOW 时刻）拒绝了
            // 完整 nudge：页面可见性记账仍必须收敛（否则事件门控卡在 hidden、
            // 挂起态无法 resume）。完整收敛由调用方后置 nudge 或下一次恢复补齐。
            restored = webView_->SetVisible(true);
        }

        const auto applied = background_suspend_policy::ApplyVisibilityResult(
            bgSuspendPageHidden_, /*desiredHidden=*/false, restored);
        bgSuspendPageHidden_ = applied.pageHidden;
        if (!bgSuspendPageHidden_) {
            bgSuspendDeepRequested_ = false;
            bgSuspendResumeRetryCount_ = 0;
            KillTimer(hwnd_, BG_RESUME_RETRY_TIMER_ID);
            // 覆盖首次 API 成功但安全桌面/DComp 尚未重新 present 的窗口期。
            KillTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID);
            SetTimer(hwnd_, RESTORE_SURFACE_CONVERGE_TIMER_ID,
                     RESTORE_SURFACE_CONVERGE_DELAY_MS, nullptr);
        } else if (applied.retryResume &&
                   bgSuspendResumeRetryCount_ < BG_RESUME_RETRY_LIMIT) {
            ++bgSuspendResumeRetryCount_;
            SetTimer(hwnd_, BG_RESUME_RETRY_TIMER_ID,
                     BG_RESUME_RETRY_DELAY_MS, nullptr);
        } else if (applied.retryResume) {
            console::printf(
                "[WebView2 UI] background resume did not converge after %u attempts",
                bgSuspendResumeRetryCount_);
        }
    }

    // 按当前 reasons 统一重投影内存等级（统一恢复序列的收尾）：覆盖态只降内存
    // 不隐藏 surface（DWM 任务栏预览仍消费）；"锁屏解除但仍被覆盖"等组合在此
    // 自然落位 Low，不等覆盖轮询兜底。深挂起在途/生效期间跳过——互斥铁律
    //（TrySuspend 自动管理内存目标，手动设置会被忽略且破坏状态推理）。
    if (!bgSuspendDeepRequested_) {
        // kill-switch 关闭时，deepSuspend 投影显式降级为手动 Low；正常深挂起路径
        // 的 useLowMemory=false，由 TrySuspend/Resume 独占内存目标控制。
        const bool useManualLow = currentProjection.useLowMemory ||
            (currentProjection.deepSuspend && !deepSuspendEnabled);
        webView_->SetMemoryUsageLow(useManualLow);
    }
}

bool MainWindow::IsMainWindowFullyCovered() const {
    if (!hwnd_ || !IsWindow(hwnd_)) return false;
    if (isMinimized_ || !IsWindowVisible(hwnd_)) return false;  // 最小化/隐藏不算“被覆盖”
    if (isFullscreen_) return false;                            // 自己全屏

    RECT self{};
    if (!GetWindowRect(hwnd_, &self) || IsRectEmpty(&self)) return false;

    HWND fg = GetForegroundWindow();
    if (!fg || fg == hwnd_) return false;                       // 自己在前台 → 未被覆盖
    if (GetAncestor(fg, GA_ROOTOWNER) == hwnd_) return false;   // 自己的 owned 弹窗/歌词
    if (!IsWindowVisible(fg)) return false;

    // 桌面外壳例外：点击桌面会让 Progman/WorkerW 成为前台，其矩形覆盖整屏，
    // 下方"前台几何完全包含本窗口"会把"桌面在前台"误判为"本窗口被完全覆盖"，
    // 进而错误地把 WebView SetVisible(false) 挂起、内容消失（聚焦才恢复）。
    // 桌面成为前台时本窗口实际仍完全可见，因此在覆盖谓词本身排除该情况。
    if (fg == ::GetShellWindow()) return false;
    {
        wchar_t fgClass[64] = {};
        if (GetClassNameW(fg, fgClass, ARRAYSIZE(fgClass)) &&
            (wcscmp(fgClass, L"Progman") == 0 || wcscmp(fgClass, L"WorkerW") == 0)) {
            return false;
        }
    }

    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(fg, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
        return false;                                           // UWP/异虚拟桌面 cloak

    const LONG_PTR ex = GetWindowLongPtrW(fg, GWL_EXSTYLE);
    if (ex & WS_EX_TRANSPARENT) return false;                   // 点击穿透层
    if (ex & WS_EX_LAYERED) {
        BYTE alpha = 255; COLORREF key = 0; DWORD lflags = 0;
        if (GetLayeredWindowAttributes(fg, &key, &alpha, &lflags)) {
            if ((lflags & LWA_ALPHA) && alpha < 255) return false;  // 半透明
        } else {
            return false;                                       // per-pixel alpha 无法判定 → 保守放弃
        }
    }

    HMONITOR mSelf = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONULL);
    HMONITOR mFg   = MonitorFromWindow(fg,    MONITOR_DEFAULTTONULL);
    if (!mSelf || mSelf != mFg) return false;                   // 必须同屏

    RECT fr{};
    if (!GetWindowRect(fg, &fr)) return false;
    // 前台窗口几何上完全包含本窗口 → 判定完全覆盖（保守；部分遮挡不处理）
    return (fr.left <= self.left && fr.top <= self.top &&
            fr.right >= self.right && fr.bottom >= self.bottom);
}

void MainWindow::ScheduleCoverReevaluation() {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    if (startupRevealPending_ || !startupRevealCommitted_ || startupRevealSettling_) return;
    // 防抖：连续激活/位置变化只在静默 COVER_REEVAL_DELAY_MS 后评估一次
    SetTimer(hwnd_, COVER_REEVAL_TIMER_ID, COVER_REEVAL_DELAY_MS, nullptr);
}

void MainWindow::EvaluateCoverState() {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    if (startupRevealPending_ || !startupRevealCommitted_ || startupRevealSettling_) return;

    // 最小化/托盘隐藏由各自路径管理可见性；此处仅清账并停轮询
    if (isMinimized_ || !IsWindowVisible(hwnd_)) {
        ClearCoverSuspend();
        return;
    }

    if (IsMainWindowFullyCovered()) {
        SetBgSuspend(kBgSuspendCovered, true, "covered");
        // 启动/续期轮询以检测“重新露出”（仅覆盖态运行）
        SetTimer(hwnd_, COVER_POLL_TIMER_ID, COVER_POLL_INTERVAL_MS, nullptr);
    } else {
        SetBgSuspend(kBgSuspendCovered, false, "uncovered");
        KillTimer(hwnd_, COVER_POLL_TIMER_ID);
    }
}

void MainWindow::ClearCoverSuspend() {
    bgSuspendReasons_ &= ~kBgSuspendCovered;
    if (hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, COVER_POLL_TIMER_ID);
        KillTimer(hwnd_, COVER_REEVAL_TIMER_ID);
    }
}
