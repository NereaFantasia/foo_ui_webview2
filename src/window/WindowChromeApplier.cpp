#include "pch.h"
#include "window/WindowChromeApplier.h"
#include "window/WindowChromeTrace.h"
#include "webview/WebViewHost.h"

namespace {
static HRESULT S_DwmSetWindowAttribute(HWND h, DWORD a, LPCVOID d, DWORD s) {
    return ::DwmSetWindowAttribute(h, a, d, s);
}

static HRESULT S_DwmGetWindowAttribute(HWND h, DWORD a, PVOID d, DWORD s) {
    return ::DwmGetWindowAttribute(h, a, d, s);
}

static HRESULT S_DwmEnableBlurBehindWindow(HWND h, const DWM_BLURBEHIND* blur) {
    return ::DwmEnableBlurBehindWindow(h, blur);
}

constexpr DWORD DWMWA_MICA_EFFECT_V = 1029;
constexpr DWORD DWMWA_SYSTEMBACKDROP_TYPE_V = 38;
constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_V = 20;
constexpr int DWMSBT_AUTO_V = 0;
constexpr int DWMSBT_NONE_V = 1;
constexpr int DWMSBT_MAINWINDOW_V = 2;
constexpr int DWMSBT_TRANSIENTWINDOW_V = 3;
constexpr int DWMSBT_TABBEDWINDOW_V = 4;

static const char* S_Text(const char* value) {
    return (value && value[0]) ? value : "-";
}

static int S_BackdropTypeFromEffect(const std::string& effect) {
    if (effect == "none") {
        return DWMSBT_NONE_V;
    }
    if (effect == "mica") {
        return DWMSBT_MAINWINDOW_V;
    }
    if (effect == "acrylic") {
        return DWMSBT_TRANSIENTWINDOW_V;
    }
    if (effect == "mica-alt") {
        return DWMSBT_TABBEDWINDOW_V;
    }
    return DWMSBT_AUTO_V;
}

static std::string S_GetCurrentEffect(const WindowChromeApplyRequest& request) {
    return request.active ? request.state.effectiveActiveEffect : request.state.effectiveInactiveEffect;
}

// 判断效果是否为插件托管的 DWM backdrop（需要 transparent background + frame extension）
static bool S_IsManagedBackdropEffect(const std::string& effect) {
    return effect == "mica" || effect == "mica-alt" || effect == "acrylic";
}

static HRESULT S_ApplyBackdropEffect(HWND hwnd, const std::string& effect) {
    int backdropType = S_BackdropTypeFromEffect(effect);

    HRESULT hr = S_DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE_V,
        &backdropType, sizeof(backdropType));

    BOOL oldMicaEnabled = (effect == "mica" || effect == "mica-alt") ? TRUE : FALSE;
    HRESULT oldMicaHr = S_DwmSetWindowAttribute(hwnd, DWMWA_MICA_EFFECT_V,
        &oldMicaEnabled, sizeof(oldMicaEnabled));

    if (FAILED(hr) && (effect == "mica" || effect == "mica-alt" || effect == "none" || effect == "system")) {
        return oldMicaHr;
    }
    return hr;
}

static HRESULT S_ApplyBlurFallback(HWND hwnd, bool enabled) {
    DWM_BLURBEHIND blur = {};
    blur.dwFlags = DWM_BB_ENABLE;
    blur.fEnable = enabled ? TRUE : FALSE;
    return S_DwmEnableBlurBehindWindow(hwnd, &blur);
}
}

WindowChromeApplyResult WindowChromeApplier::Apply(const WindowChromeApplyHooks& hooks,
                                                   const WindowChromeApplyRequest& request) {
    WindowChromeApplyResult result;
    result.mode = request.active ? "active" : "inactive";
    result.effect = S_GetCurrentEffect(request);

    const std::string previousMode = request.previousMode.empty() ? "-" : request.previousMode;
    const char* previousEffect = request.previousEffect.empty() ? "-" : request.previousEffect.c_str();
    const char* effect = result.effect.empty() ? "-" : result.effect.c_str();
    const std::string extra = std::string("previousMode=") + previousMode;

    WindowChromeTraceState traceState;
    traceState.phase = "WindowChromeApplier::Apply";
    traceState.windowKind = hooks.diagnosticWindowKind;
    traceState.windowId = hooks.diagnosticWindowId;
    traceState.hwnd = hooks.hwnd;
    traceState.startupRevealPending = hooks.diagnosticStartupRevealPending;
    traceState.startupRevealCommitted = hooks.diagnosticStartupRevealCommitted;
    traceState.startupRevealSettling = hooks.diagnosticStartupRevealSettling;
    traceState.isActive = hooks.diagnosticIsActive;
    traceState.active = WindowChromeTrace::BoolText(request.active);
    traceState.forceRefresh = WindowChromeTrace::BoolText(request.forceRefresh);
    traceState.effect = effect;
    traceState.previousEffect = previousEffect;
    traceState.extra = extra.c_str();
    WindowChromeTrace::Emit(traceState);

    if (!hooks.hwnd || !IsWindow(hooks.hwnd)) {
        result.success = false;
        return result;
    }

    if (hooks.applyCornerPreference && request.state.cornerPreference != "unsupported" &&
        !request.state.fullscreen) {
        hooks.applyCornerPreference(request.state.cornerPreference);
    }

    BOOL darkMode = request.state.backdropPolicy.darkMode ? TRUE : FALSE;
    HRESULT darkModeHr = S_DwmSetWindowAttribute(hooks.hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_V,
        &darkMode, sizeof(darkMode));

    const bool isSystemEffect = (result.effect == "system");

    // 当任一激活态（active / inactive）使用托管 DWM 效果时，保持 WebView 背景透明，
    // 以便 DWM 合成器在焦点切换时能渲染平滑过渡动画（如 acrylic → system → acrylic）。
    // 仅在双态均为 system/none 等非托管效果时才恢复不透明。
    const bool hasAnyManagedEffect =
        S_IsManagedBackdropEffect(request.state.effectiveActiveEffect) ||
        S_IsManagedBackdropEffect(request.state.effectiveInactiveEffect);
    const bool webViewTransparent = hasAnyManagedEffect
        ? request.state.transparentBackground
        : (isSystemEffect ? false : request.state.transparentBackground);

    const bool webViewReady = hooks.webView && hooks.webView->IsReady();
    if (webViewReady) {
        hooks.webView->SetBackgroundTransparent(webViewTransparent);
    }

    const bool shouldUseBlurFallback = !isSystemEffect && request.state.useBlurBehind &&
        (result.effect == "none");
    // 无论当前效果是否为 system，都调用 DWM backdrop API：
    // - 非 system → 设置对应 backdrop type（mica/acrylic 等）
    // - system → 设置 DWMSBT_AUTO (0)，让 DWM 合成器播放淡出过渡动画
    // 旧逻辑跳过 system 的 DWM 调用，导致 DWM 不知道要淡出，焦点切换时效果瞬变无动画。
    HRESULT backdropHr = S_ApplyBackdropEffect(hooks.hwnd, result.effect);
    HRESULT blurHr = S_ApplyBlurFallback(hooks.hwnd, shouldUseBlurFallback);

    // [Chrome Reconciliation] 调用顺序关键：
    // 1. 先切 native frame strategy（style bits），确保 WS_CAPTION 已移除
    // 2. 再执行 applyFrameless（DWM frame 扩展 + SWP_FRAMECHANGED + RedrawWindow）
    // 若反过来，DWM 会按"仍带 caption 的窗口"重绘一次三大键按钮残影
    if (hooks.applyNativeFrameStrategy && !request.state.fullscreen) {
        hooks.applyNativeFrameStrategy(request.state.nativeFrameStrategy);
    }

    // applyFrameless 包含 SetWindowPos(SWP_FRAMECHANGED)，会触发 DWM 重新评估帧合成。
    // 必须在 S_ApplyBackdropEffect 之后执行，确保 DWM 在处理 FRAMECHANGED 时
    // 读到的是正确的 backdrop type，而非旧值。
    if (hooks.applyFrameless && !request.state.fullscreen) {
        hooks.applyFrameless(request.state.frameless);
    }

    if (webViewReady &&
        (request.forceRefresh || request.state.backdropPolicy.reapplyOnActivate)) {
        hooks.webView->SetBoundsVisible(IsWindowVisible(hooks.hwnd) != FALSE && IsIconic(hooks.hwnd) == FALSE);
    }

    if (hooks.afterBackdropApplied) {
        hooks.afterBackdropApplied();
    }

    result.success = isSystemEffect || SUCCEEDED(backdropHr) || (shouldUseBlurFallback && SUCCEEDED(blurHr));

    const bool backdropStateChanged = request.forceRefresh ||
        result.mode != request.previousMode || result.effect != request.previousEffect;
    const bool canBroadcastBackdropStateChanged = !hooks.canBroadcastBackdropStateChanged ||
        hooks.canBroadcastBackdropStateChanged();

    if (hooks.broadcastBackdropStateChanged &&
        result.success &&
        backdropStateChanged &&
        canBroadcastBackdropStateChanged) {
        hooks.broadcastBackdropStateChanged(request.active, result.mode, result.effect);
    }

    int readBackdropType = -1;
    HRESULT readBackdropHr = S_DwmGetWindowAttribute(
        hooks.hwnd, DWMWA_SYSTEMBACKDROP_TYPE_V, &readBackdropType, sizeof(readBackdropType));
    BOOL readDarkMode = FALSE;
    HRESULT readDarkModeHr = S_DwmGetWindowAttribute(
        hooks.hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_V, &readDarkMode, sizeof(readDarkMode));

    std::ostringstream stream;
    stream << "[WindowChromeProbe]"
           << " t=" << WindowChromeTrace::RelativeMs() << "ms"
           << " kind=" << S_Text(hooks.diagnosticWindowKind)
           << " windowId=" << S_Text(hooks.diagnosticWindowId)
           << " hwnd=0x" << pfc::format_hex((size_t)hooks.hwnd)
           << " mode=" << result.mode.c_str()
           << " requestedEffect=" << result.effect.c_str()
           << " requestedBackdropType=" << S_BackdropTypeFromEffect(result.effect)
           << " transparentBackground=" << WindowChromeTrace::BoolText(webViewTransparent)
           << " isSystemEffect=" << WindowChromeTrace::BoolText(isSystemEffect)
           << " forceRefresh=" << WindowChromeTrace::BoolText(request.forceRefresh)
           << " darkModeRequested=" << WindowChromeTrace::BoolText(request.state.backdropPolicy.darkMode)
           << " darkModeSetHr=0x" << pfc::format_hex((uint32_t)darkModeHr)
           << " darkModeReadHr=0x" << pfc::format_hex((uint32_t)readDarkModeHr)
           << " darkModeRead=" << WindowChromeTrace::BoolText(readDarkMode != FALSE)
           << " backdropHr=0x" << pfc::format_hex((uint32_t)backdropHr)
           << " backdropReadHr=0x" << pfc::format_hex((uint32_t)readBackdropHr)
           << " backdropRead=" << readBackdropType
           << " blurFallback=" << WindowChromeTrace::BoolText(shouldUseBlurFallback)
           << " blurHr=0x" << pfc::format_hex((uint32_t)blurHr)
           << " webViewReady=" << WindowChromeTrace::BoolText(webViewReady);
    WindowChromeTrace::EmitAuxiliaryLine(stream.str());

    return result;
}

void WindowChromeApplier::RefreshNativeFrame(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    RedrawWindow(hwnd, nullptr, nullptr,
        RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}