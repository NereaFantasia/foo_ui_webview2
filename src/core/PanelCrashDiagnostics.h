#pragma once

// webview:processFailed 事件载荷里的两个字段，由 WebViewPanel::OnWebViewProcessFailed 填写。
namespace panel_crash {
    // COREWEBVIEW2_PROCESS_FAILED_KIND 转成事件的 kind 字段（camelCase）。
    const char* FailedKindToString(COREWEBVIEW2_PROCESS_FAILED_KIND kind);
    // 宿主对这类崩溃采取的处理：reload、needRebuild 或 none，与 WebViewHost::SetupProcessFailedHandling 的分级一致。
    const char* RecoveryActionFor(COREWEBVIEW2_PROCESS_FAILED_KIND kind);
}
