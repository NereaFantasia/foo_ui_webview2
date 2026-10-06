/**
 * WebViewHostCrashRecovery.cpp - WebViewHost 的 WebView2 进程崩溃处理
 *
 * 注册 ProcessFailed：渲染进程崩溃时 Reload 自愈，浏览器进程退出时作废共享环境并通知
 * 上层重建，其余进程只记日志。
 */
#include "pch.h"
#include "webview/WebViewHost.h"
#include "webview/WebViewEnvironment.h"
#include "webview/WebViewCrashPolicy.h"
#include <wrl/event.h>

using namespace Microsoft::WRL;

// ==========================================================================
// 注册 WebView2 进程崩溃事件 (add_ProcessFailed)
// Visual Hosting 模式下进程崩溃会导致只剩空 Win32 窗口（DComp visual 仍在
// 但无内容渲染），且 webview_ 指针不会自动失效。此处按崩溃类型分级处理：
//   - 渲染进程崩溃/无响应 → 自动 Reload() 恢复（controller + DComp 连接仍有效）
//   - 浏览器进程退出       → 整个 WebView 失效，通知上层重建（recovered=false）
//   - 其他工具进程（GPU 等）→ 运行时通常自愈，仅记录日志
// ==========================================================================
void WebViewHost::SetupProcessFailedHandling() {
    if (!webview_) return;

    HRESULT hr = webview_->add_ProcessFailed(
        Callback<ICoreWebView2ProcessFailedEventHandler>(
            [this](ICoreWebView2* /*sender*/,
                   ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                COREWEBVIEW2_PROCESS_FAILED_KIND kind =
                    COREWEBVIEW2_PROCESS_FAILED_KIND_UNKNOWN_PROCESS_EXITED;
                if (args) {
                    args->get_ProcessFailedKind(&kind);
                }

                // 收集诊断详情（reason / exitCode / processDescription 来自 Args2）
                std::ostringstream diag;
                diag << "WebView2 ProcessFailed: kind=" << static_cast<int>(kind);

                wil::com_ptr<ICoreWebView2ProcessFailedEventArgs2> args2;
                if (args && SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&args2))) && args2) {
                    COREWEBVIEW2_PROCESS_FAILED_REASON reason =
                        COREWEBVIEW2_PROCESS_FAILED_REASON_UNEXPECTED;
                    int exitCode = 0;
                    wil::unique_cotaskmem_string description;
                    args2->get_Reason(&reason);
                    args2->get_ExitCode(&exitCode);
                    args2->get_ProcessDescription(&description);
                    diag << " reason=" << static_cast<int>(reason)
                         << " exitCode=" << exitCode;
                    if (description) {
                        diag << " process=\""
                             << pfc::stringcvt::string_utf8_from_wide(description.get()).get_ptr()
                             << "\"";
                    }
                }

                // 分级判定下沉到 webview_crash_policy（纯函数、可单测）。
                const auto failureClass = webview_crash_policy::ClassifyFailedKind(
                    kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED ||
                    kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE ||
                    kind == COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED,
                    kind == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED);

                // 渲染进程类崩溃先尝试 Reload 自愈（controller / DComp 连接仍有效）。
                bool reloadSucceeded = false;
                if (failureClass == webview_crash_policy::FailureClass::Render) {
                    const HRESULT reloadHr = webview_ ? webview_->Reload() : E_FAIL;
                    reloadSucceeded = SUCCEEDED(reloadHr);
                    diag << " action=Reload result=" << (reloadSucceeded ? "ok" : "failed");
                } else if (failureClass == webview_crash_policy::FailureClass::Browser) {
                    diag << " action=needRebuild";
                } else {
                    diag << " action=logOnly";
                }

                const auto disposition =
                    webview_crash_policy::Decide(failureClass, reloadSucceeded);

                // 共享 environment 绑定的正是已死的浏览器进程；若不作废，重建会命中
                // 缓存拿到失效 environment，导致 CreateCoreWebView2*Controller 必然失败。
                if (disposition.invalidateEnvironment) {
                    WebViewEnvironment::GetInstance().Invalidate("browserProcessExited");
                    diag << " envInvalidated=1";
                }

                const bool recovered = disposition.recovered;

                WriteCrashLog(diag.str());
                LogLifecycle("process.failed", diag.str());
                LOG("WebViewHost: ", diag.str().c_str());

                if (processFailedCallback_) {
                    processFailedCallback_(kind, recovered);
                }
                return S_OK;
            }).Get(),
        &processFailedToken_);

    if (FAILED(hr)) {
        LOG("WebViewHost: add_ProcessFailed failed, HRESULT: ", pfc::format_hex((uint32_t)hr));
    }
}
