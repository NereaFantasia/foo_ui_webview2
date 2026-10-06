#include "pch.h"
#include "core/PanelCrashDiagnostics.h"

namespace panel_crash {
    // Map COREWEBVIEW2_PROCESS_FAILED_KIND -> JS-friendly camelCase string.
    // Used as the `kind` field of the `webview:processFailed` event payload.
    const char* FailedKindToString(COREWEBVIEW2_PROCESS_FAILED_KIND kind) {
        switch (kind) {
            case COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED:        return "browserProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED:         return "renderProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE:   return "renderProcessUnresponsive";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED:   return "frameRenderProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_UTILITY_PROCESS_EXITED:        return "utilityProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_SANDBOX_HELPER_PROCESS_EXITED: return "sandboxHelperProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_GPU_PROCESS_EXITED:            return "gpuProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_PLUGIN_PROCESS_EXITED:   return "ppapiPluginProcessExited";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_BROKER_PROCESS_EXITED:   return "ppapiBrokerProcessExited";
            default: return "unknownProcessExited";
        }
    }

    // Mirror the tiered handling in WebViewHost::SetupProcessFailedHandling:
    //   render-process kinds  -> "reload"       (Reload() was attempted, see `recovered`)
    //   browser-process exit  -> "needRebuild"  (entire WebView is dead, host must rebuild)
    //   others (GPU/utility)  -> "none"         (runtime self-heals, just observe)
    const char* RecoveryActionFor(COREWEBVIEW2_PROCESS_FAILED_KIND kind) {
        switch (kind) {
            case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED:
            case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE:
            case COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED:
                return "reload";
            case COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED:
                return "needRebuild";
            default:
                return "none";
        }
    }
}  // namespace panel_crash
