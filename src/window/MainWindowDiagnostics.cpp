// MainWindowDiagnostics.cpp — 主窗口的取证输出：交互式缩放证据、surface 诊断、WebView 生命周期日志与运行时 DOM 探针
#include "pch.h"
#include "window/MainWindow.h"
#include "window/MainWindowInternal.h"
#include "window/WindowChromeTrace.h"
#include "webview/WebViewHost.h"
#include <sstream>

using namespace mainwindow_detail;
using json = nlohmann::json;

void MainWindow::EmitInteractiveResizeEvidence(const char* phase,
                                               const char* detail,
                                               const RECT* sizingRect,
                                               const WINDOWPOS* windowPos) const {
    // 门控必须在函数入口，不能只依赖末尾 S_EmitEvidenceLine：本函数在
    // WM_SIZING / WM_WINDOWPOSCHANGING / WM_WINDOWPOSCHANGED 上逐帧调用，
    // 下方 get_Bounds + DwmGetWindowAttribute + 数十段 ostringstream 拼装
    // 在 trace 关闭时是纯浪费（结果会被 EmitAuxiliaryLine 直接丢弃）。
    if (!WindowChromeTrace::AuxiliaryTraceEnabled()) {
        return;
    }
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return;
    }

    RECT windowRect{};
    RECT clientRect{};
    GetWindowRect(hwnd_, &windowRect);
    GetClientRect(hwnd_, &clientRect);

    RECT controllerBounds{};
    bool hasControllerBounds = false;
    if (webView_ && webView_->IsReady() && webView_->GetController()) {
        hasControllerBounds = SUCCEEDED(webView_->GetController()->get_Bounds(&controllerBounds));
    }

    int backdropRead = -1;
    const HRESULT backdropHr = S_DwmGetWindowAttribute(
        hwnd_, DWMWA_SYSTEMBACKDROP_TYPE_V, &backdropRead, sizeof(backdropRead));

    const std::string windowRectText = S_FormatNativeRect(windowRect);
    const std::string clientRectText = S_FormatNativeRect(clientRect);
    const std::string controllerBoundsText = hasControllerBounds
        ? S_FormatNativeRect(controllerBounds)
        : "N/A";
    const std::string sizingRectText = sizingRect ? S_FormatNativeRect(*sizingRect) : "N/A";

    std::ostringstream stream;
    stream << "[InteractiveResize]"
           << " t=" << WindowChromeTrace::RelativeMs() << "ms"
           << " phase=" << (phase && phase[0] ? phase : "-")
           << " detail=" << (detail && detail[0] ? detail : "-")
           << " session=" << interactiveSizingSessionId_
           << " interactiveSizingActive=" << WindowChromeTrace::BoolText(interactiveSizingActive_)
           << " startupRevealPending=" << WindowChromeTrace::BoolText(startupRevealPending_)
           << " startupRevealCommitted=" << WindowChromeTrace::BoolText(startupRevealCommitted_)
           << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_)
           << " isActive=" << WindowChromeTrace::BoolText(isActive_)
           << " isMinimized=" << WindowChromeTrace::BoolText(isMinimized_)
           << " isMaximized=" << WindowChromeTrace::BoolText(isMaximized_)
           << " visible=" << WindowChromeTrace::BoolText(IsWindowVisible(hwnd_) != FALSE)
           << " iconic=" << WindowChromeTrace::BoolText(IsIconic(hwnd_) != FALSE)
           << " zoomed=" << WindowChromeTrace::BoolText(IsZoomed(hwnd_) != FALSE)
           << " windowRect=" << windowRectText.c_str()
           << " clientRect=" << clientRectText.c_str()
           << " controllerBounds=" << controllerBoundsText.c_str()
           << " sizingRect=" << sizingRectText.c_str()
           << " backdropHr=0x" << pfc::format_hex((uint32_t)backdropHr)
           << " backdropRead=" << backdropRead;

    if (windowPos) {
        const std::string windowPosFlags = S_FormatWindowPosFlags(windowPos->flags);
        stream << " windowPos.flags=0x" << pfc::format_hex((uint32_t)windowPos->flags)
               << " windowPos.flagsText=" << windowPosFlags.c_str()
               << " windowPos.x=" << windowPos->x
               << " windowPos.y=" << windowPos->y
               << " windowPos.cx=" << windowPos->cx
               << " windowPos.cy=" << windowPos->cy;
    }

    S_EmitEvidenceLine(stream.str());
}

void MainWindow::LogSurfaceDiagnostics(const char* phase) const {
    // 同 EmitInteractiveResizeEvidence：门控前移到入口，避免 trace 关闭时
    // 仍执行 get_Bounds 与字符串拼装。
    if (!WindowChromeTrace::AuxiliaryTraceEnabled()) return;
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);

    RECT controllerBounds{};
    bool hasBounds = false;
    if (webView_ && webView_->IsReady() && webView_->GetController()) {
        hasBounds = SUCCEEDED(webView_->GetController()->get_Bounds(&controllerBounds));
    }

    std::ostringstream stream;
    stream << "[SurfaceDiag] phase=" << phase
           << " t=" << WindowChromeTrace::RelativeMs() << "ms"
           << " hwnd=0x" << pfc::format_hex((size_t)hwnd_)
           << " clientRect=(" << clientRect.left << "," << clientRect.top
           << "," << clientRect.right << "," << clientRect.bottom << ")"
           << " controllerBounds=" << (hasBounds
               ? (std::string("(") + std::to_string(controllerBounds.left) + ","
                  + std::to_string(controllerBounds.top) + ","
                  + std::to_string(controllerBounds.right) + ","
                  + std::to_string(controllerBounds.bottom) + ")").c_str()
               : "N/A")
           << " visible=" << WindowChromeTrace::BoolText(IsWindowVisible(hwnd_) != FALSE)
           << " iconic=" << WindowChromeTrace::BoolText(IsIconic(hwnd_) != FALSE)
           << " zoomed=" << WindowChromeTrace::BoolText(IsZoomed(hwnd_) != FALSE);
    S_EmitEvidenceLine(stream.str());
}

void MainWindow::LogWebViewLifecycle(const char* event, const std::string& detail) const {
    try {
        RECT clientRect{};
        if (hwnd_ && IsWindow(hwnd_)) {
            GetClientRect(hwnd_, &clientRect);
        }

        std::ostringstream stream;
        stream << "windowId=" << (GetWindowId().empty() ? "main" : GetWindowId())
               << " reasons=0x" << pfc::format_hex(bgSuspendReasons_)
               << " appliedHidden=" << (bgSuspendPageHidden_ ? 1 : 0)
               << " deepRequested=" << (bgSuspendDeepRequested_ ? 1 : 0)
               << " resumeRetries=" << bgSuspendResumeRetryCount_
               << " healthAttempts=" << bgSuspendHealthRecoveryAttempts_
               << " healthPending=" << (pageHealthProbePending_ ? 1 : 0)
               << " healthGeneration=" << pageHealthProbeGeneration_
               << " rebuildInProgress=" << (webViewRebuildInProgress_ ? 1 : 0)
               << " minimized=" << (isMinimized_ ? 1 : 0)
               << " windowVisible="
               << (hwnd_ && IsWindow(hwnd_) && IsWindowVisible(hwnd_) ? 1 : 0)
               << " client=(" << clientRect.left << ',' << clientRect.top << ','
               << clientRect.right << ',' << clientRect.bottom << ')';
        if (!detail.empty()) {
            stream << ' ' << detail;
        }

        if (webView_) {
            webView_->LogLifecycle(event, stream.str());
            return;
        }

        std::ostringstream fallback;
        fallback << "event=" << (event && event[0] ? event : "-")
                 << " uptimeMs=" << GetTickCount64()
                 << " pid=" << GetCurrentProcessId()
                 << " tid=" << GetCurrentThreadId()
                 << " hwnd=0x" << pfc::format_hex((size_t)hwnd_)
                 << " host=none detail=" << stream.str();
        WebViewHost::WriteLifecycleLog(fallback.str());
    } catch (...) {
        // Diagnostics must never alter window lifecycle behavior.
    }
}

void MainWindow::CaptureRuntimeDomProbe(const std::string& phase, int scheduledDelayMs) {
    // 门控必须在 ExecuteScript 之前：本函数向页面注入 DOM 探针脚本
    // （getComputedStyle / elementFromPoint / getBoundingClientRect），
    // 是跨进程调用且会在渲染进程主线程触发样式与布局计算。trace 关闭时
    // 结果被丢弃，但注入与页面侧开销照发生，故在入口直接早退。
    if (!WindowChromeTrace::AuxiliaryTraceEnabled()) {
        return;
    }

    const double requestedAtMs = static_cast<double>(WindowChromeTrace::RelativeMs());
    const HWND hwnd = hwnd_;
    const std::string windowId = GetWindowId().empty() ? "main" : GetWindowId();

    if (!hwnd || !IsWindow(hwnd)) {
        std::ostringstream stream;
        stream << "[RuntimeDomProbe.skip]"
               << " tRequest=" << requestedAtMs << "ms"
               << " phase=" << phase.c_str()
               << " scheduledDelayMs=" << scheduledDelayMs
               << " reason=invalid-hwnd";
        S_EmitEvidenceLine(stream.str());
        return;
    }

    RECT clientRect{};
    GetClientRect(hwnd, &clientRect);

    RECT controllerBounds{};
    bool hasBounds = false;
    const bool webViewReady = webView_ && webView_->IsReady();
    if (webViewReady && webView_->GetController()) {
        hasBounds = SUCCEEDED(webView_->GetController()->get_Bounds(&controllerBounds));
    }

    const bool visible = IsWindowVisible(hwnd) != FALSE;
    const bool iconic = IsIconic(hwnd) != FALSE;
    const bool zoomed = IsZoomed(hwnd) != FALSE;
    const bool active = isActive_;
    const bool startupRevealPending = startupRevealPending_;
    const bool startupRevealCommitted = startupRevealCommitted_;
    const bool startupRevealSettling = startupRevealSettling_;
    const std::string clientRectText = S_FormatNativeRect(clientRect);
    const std::string controllerBoundsText = hasBounds ? S_FormatNativeRect(controllerBounds) : "N/A";

    if (!webViewReady) {
        std::ostringstream stream;
        stream << "[RuntimeDomProbe.skip]"
               << " tRequest=" << requestedAtMs << "ms"
               << " phase=" << phase.c_str()
               << " scheduledDelayMs=" << scheduledDelayMs
               << " windowId=" << windowId.c_str()
               << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
               << " reason=webview-not-ready"
               << " clientRect=" << clientRectText.c_str()
               << " controllerBounds=" << controllerBoundsText.c_str();
        S_EmitEvidenceLine(stream.str());
        return;
    }

    static const wchar_t* runtimeProbeScript = LR"JS((() => {
        const de = document.documentElement;
        const body = document.body;
        const getStyleSafe = (el) => {
            try {
                return el ? window.getComputedStyle(el) : null;
            } catch (_) {
                return null;
            }
        };
        const rectOf = (el) => {
            if (!el || typeof el.getBoundingClientRect !== 'function') return null;
            const r = el.getBoundingClientRect();
            return {
                left: Number(r.left || 0),
                top: Number(r.top || 0),
                width: Number(r.width || 0),
                height: Number(r.height || 0),
                right: Number(r.right || 0),
                bottom: Number(r.bottom || 0)
            };
        };
        const classNameOf = (el) => {
            if (!el) return '';
            if (typeof el.className === 'string') return el.className;
            if (typeof el.getAttribute === 'function') return el.getAttribute('class') || '';
            return '';
        };
        const describe = (el) => {
            if (!el) return null;
            const style = getStyleSafe(el);
            return {
                tag: el.tagName || '',
                id: el.id || '',
                className: classNameOf(el),
                background: style ? (style.backgroundColor || style.background || '') : '',
                opacity: style ? (style.opacity || '') : '',
                display: style ? (style.display || '') : '',
                visibility: style ? (style.visibility || '') : '',
                rect: rectOf(el)
            };
        };
        const overlay = document.querySelector('#server-loading-overlay, #server-loading, .server-loading-overlay');
        const overlayStyle = getStyleSafe(overlay);
        const overlayRect = rectOf(overlay);
        const overlayVisible = !!overlay && !!overlayStyle &&
            overlayStyle.display !== 'none' &&
            overlayStyle.visibility !== 'hidden' &&
            Number.parseFloat(overlayStyle.opacity || '1') > 0.001 &&
            !!overlayRect && overlayRect.width > 0 && overlayRect.height > 0;
        const centerEl = document.elementFromPoint(
            Math.max(0, Math.floor((window.innerWidth || 0) / 2)),
            Math.max(0, Math.floor((window.innerHeight || 0) / 2))
        );
        const htmlStyle = getStyleSafe(de);
        const bodyStyle = getStyleSafe(body);
        return {
            href: location.href,
            readyState: document.readyState,
            visibilityState: document.visibilityState,
            innerWidth: window.innerWidth || 0,
            innerHeight: window.innerHeight || 0,
            htmlBackground: htmlStyle ? (htmlStyle.backgroundColor || htmlStyle.background || '') : '',
            htmlOpacity: htmlStyle ? (htmlStyle.opacity || '') : '',
            bodyBackground: bodyStyle ? (bodyStyle.backgroundColor || bodyStyle.background || '') : '',
            bodyOpacity: bodyStyle ? (bodyStyle.opacity || '') : '',
            overlayExists: !!overlay,
            overlayVisible,
            overlay: describe(overlay),
            center: describe(centerEl)
        };
    })())JS";

    const HRESULT executeHr = webView_->ExecuteScript(runtimeProbeScript,
        // 本回调已证明 nothrow：所有可抛操作均在外层 try 内，两个 catch 处理器各自再以 try/catch(...) 兜底
        //（日志路径 OOM 也不外抛）。clang-tidy bugprone-exception-escape 对该嵌套 try/catch 模式误报，最小范围抑制。
        // NOLINTNEXTLINE(bugprone-exception-escape)
        [phase, scheduledDelayMs, requestedAtMs, hwnd, windowId,
         clientRectText, controllerBoundsText,
         visible, iconic, zoomed, active,
         startupRevealPending, startupRevealCommitted, startupRevealSettling](const std::wstring& resultJson) {
            // 本地防御：DOM 探针完成回调跑在主线程 WebView2 回调里，下方 json 的 .value()/.contains()
            // 访问若抛 type_error 等异常并逃逸原生回调边界会触发 terminate() 崩溃整个 fb2k，故整段兜异常。
            try {
            json probe;
            if (!S_ParseExecuteScriptPayload(resultJson, probe)) {
                std::string rawUtf8(pfc::stringcvt::string_utf8_from_wide(resultJson.c_str()).get_ptr());
                std::ostringstream stream;
                stream << "[RuntimeDomProbe.error]"
                       << " tRequest=" << requestedAtMs << "ms"
                       << " tComplete=" << WindowChromeTrace::RelativeMs() << "ms"
                       << " phase=" << phase.c_str()
                       << " scheduledDelayMs=" << scheduledDelayMs
                       << " windowId=" << windowId.c_str()
                       << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
                       << " raw=" << S_TruncateUtf8(rawUtf8, 240).c_str();
                S_EmitEvidenceLine(stream.str());
                return;
            }

            const json overlay = probe.contains("overlay") && probe["overlay"].is_object()
                ? probe["overlay"]
                : json::object();
            const json center = probe.contains("center") && probe["center"].is_object()
                ? probe["center"]
                : json::object();
            const std::string overlayRectText = S_FormatDomRect(overlay.value("rect", json()));
            const std::string centerRectText = S_FormatDomRect(center.value("rect", json()));
            const std::string overlayId = overlay.value("id", "");
            const std::string overlayClass = overlay.value("className", "");
            const std::string overlayBackground = overlay.value("background", "");
            const std::string overlayOpacity = overlay.value("opacity", "");
            const std::string centerTag = center.value("tag", "");
            const std::string centerId = center.value("id", "");
            const std::string centerClass = center.value("className", "");
            const std::string centerBackground = center.value("background", "");
            const std::string centerOpacity = center.value("opacity", "");
            const std::string htmlBackground = probe.value("htmlBackground", "");
            const std::string htmlOpacity = probe.value("htmlOpacity", "");
            const std::string bodyBackground = probe.value("bodyBackground", "");
            const std::string bodyOpacity = probe.value("bodyOpacity", "");
            const std::string href = probe.value("href", "");
            const std::string readyState = probe.value("readyState", "");
            const std::string visibilityState = probe.value("visibilityState", "");

            std::ostringstream stream;
            stream << "[RuntimeDomProbe]"
                   << " tRequest=" << requestedAtMs << "ms"
                   << " tComplete=" << WindowChromeTrace::RelativeMs() << "ms"
                   << " phase=" << phase.c_str()
                   << " scheduledDelayMs=" << scheduledDelayMs
                   << " windowId=" << windowId.c_str()
                   << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
                   << " startupRevealPending=" << WindowChromeTrace::BoolText(startupRevealPending)
                   << " startupRevealCommitted=" << WindowChromeTrace::BoolText(startupRevealCommitted)
                   << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling)
                   << " active=" << WindowChromeTrace::BoolText(active)
                   << " visible=" << WindowChromeTrace::BoolText(visible)
                   << " iconic=" << WindowChromeTrace::BoolText(iconic)
                   << " zoomed=" << WindowChromeTrace::BoolText(zoomed)
                   << " clientRect=" << clientRectText.c_str()
                   << " controllerBounds=" << controllerBoundsText.c_str()
                   << " readyState=" << readyState.c_str()
                   << " visibilityState=" << visibilityState.c_str()
                   << " inner=" << probe.value("innerWidth", 0) << "x" << probe.value("innerHeight", 0)
                   << " htmlBg=" << htmlBackground.c_str()
                   << " htmlOpacity=" << htmlOpacity.c_str()
                   << " bodyBg=" << bodyBackground.c_str()
                   << " bodyOpacity=" << bodyOpacity.c_str()
                   << " overlayExists=" << WindowChromeTrace::BoolText(probe.value("overlayExists", false))
                   << " overlayVisible=" << WindowChromeTrace::BoolText(probe.value("overlayVisible", false))
                   << " overlayId=" << overlayId.c_str()
                   << " overlayClass=" << overlayClass.c_str()
                   << " overlayBg=" << overlayBackground.c_str()
                   << " overlayOpacity=" << overlayOpacity.c_str()
                   << " overlayRect=" << overlayRectText.c_str()
                   << " centerTag=" << centerTag.c_str()
                   << " centerId=" << centerId.c_str()
                   << " centerClass=" << centerClass.c_str()
                   << " centerBg=" << centerBackground.c_str()
                   << " centerOpacity=" << centerOpacity.c_str()
                   << " centerRect=" << centerRectText.c_str()
                   << " href=" << href.c_str();
            S_EmitEvidenceLine(stream.str());
            } catch (const std::exception& e) {
                // nothrow catch：日志路径(ostringstream/string)在 OOM 下也可能抛 bad_alloc，
                // 故再包一层吞掉，确保异常绝不逃逸 WebView2 回调边界（清 bugprone-exception-escape）。
                try {
                    std::ostringstream errStream;
                    errStream << "[RuntimeDomProbe.error]"
                              << " tRequest=" << requestedAtMs << "ms"
                              << " tComplete=" << WindowChromeTrace::RelativeMs() << "ms"
                              << " phase=" << phase.c_str()
                              << " scheduledDelayMs=" << scheduledDelayMs
                              << " windowId=" << windowId.c_str()
                              << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
                              << " reason=HandlerException"
                              << " what=" << S_TruncateUtf8(e.what(), 240).c_str();
                    S_EmitEvidenceLine(errStream.str());
                } catch (...) {}
            } catch (...) {
                // nothrow catch：同上，吞掉日志路径自身可能的异常。
                try {
                    std::ostringstream errStream;
                    errStream << "[RuntimeDomProbe.error]"
                              << " tRequest=" << requestedAtMs << "ms"
                              << " tComplete=" << WindowChromeTrace::RelativeMs() << "ms"
                              << " phase=" << phase.c_str()
                              << " scheduledDelayMs=" << scheduledDelayMs
                              << " windowId=" << windowId.c_str()
                              << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
                              << " reason=HandlerException";
                    S_EmitEvidenceLine(errStream.str());
                } catch (...) {}
            }
        });

    if (FAILED(executeHr)) {
        std::ostringstream stream;
        stream << "[RuntimeDomProbe.error]"
               << " tRequest=" << requestedAtMs << "ms"
               << " phase=" << phase.c_str()
               << " scheduledDelayMs=" << scheduledDelayMs
               << " windowId=" << windowId.c_str()
               << " hwnd=0x" << pfc::format_hex((size_t)hwnd)
               << " reason=ExecuteScriptFailed"
               << " hr=0x" << pfc::format_hex((uint32_t)executeHr);
        S_EmitEvidenceLine(stream.str());
    }
}

void MainWindow::ScheduleFinalizeStartupRuntimeProbes() {
    if (!hwnd_ || !IsWindow(hwnd_)) {
        return;
    }

    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID);
    KillTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID);
    SetTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T100_TIMER_ID, 100, nullptr);
    SetTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T250_TIMER_ID, 250, nullptr);
    SetTimer(hwnd_, STARTUP_SETTLEMENT_PROBE_T500_TIMER_ID, 500, nullptr);
}

void MainWindow::ScheduleManualResizeRuntimeProbes(const char* sizeType) {
    // 门控在入口，避免 trace 关闭时仍为每条 WM_SIZE 排布 +0ms / +100ms 两发
    // DOM 探针。+100ms 定时器在连续 resize 中被反复 KillTimer/SetTimer 重置，
    // 其注入会在渲染进程主线程制造额外样式/布局 pass。
    if (!WindowChromeTrace::AuxiliaryTraceEnabled()) {
        return;
    }

    if (!sizeType || !sizeType[0]) {
        return;
    }

    const std::string normalized = sizeType;
    if (normalized == "maximized") {
        CaptureRuntimeDomProbe("WM_SIZE.sizeType=maximized/+0ms", 0);
        if (hwnd_ && IsWindow(hwnd_)) {
            KillTimer(hwnd_, SIZE_MAXIMIZED_PROBE_T100_TIMER_ID);
            SetTimer(hwnd_, SIZE_MAXIMIZED_PROBE_T100_TIMER_ID, 100, nullptr);
        }
        return;
    }

    if (normalized == "restored") {
        CaptureRuntimeDomProbe("WM_SIZE.sizeType=restored/+0ms", 0);
        if (hwnd_ && IsWindow(hwnd_)) {
            KillTimer(hwnd_, SIZE_RESTORED_PROBE_T100_TIMER_ID);
            SetTimer(hwnd_, SIZE_RESTORED_PROBE_T100_TIMER_ID, 100, nullptr);
        }
    }
}
