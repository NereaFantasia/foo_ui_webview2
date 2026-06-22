#include "pch.h"
#include "window/MainWindow.h"
#include "window/MainWindowInternal.h"
#include "window/ChromeController.h"
#include "window/WindowChromeResolver.h"
#include "window/WindowChromeTrace.h"
#include "webview/WebViewHost.h"
#include "core/WebViewContext.h"
#include "core/PreferencesPage.h"
#include "api/WindowApi.h"

using namespace mainwindow_detail;

// ============================================
// Mica 效果 (Windows 11)
// ============================================
void MainWindow::EnableMicaEffect() {
    // [Experiment I] 精确复刻 v1.1.19 的直接 DWM 写入。
    // 不走 Chrome 统一层，不写 corner preference、BlurBehind、MICA_EFFECT=FALSE。
    // Chrome 层在 OnWebViewReady 后首次激活。

    // 启用深色模式
    BOOL darkMode = TRUE;
    S_DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE_V,
        &darkMode, sizeof(darkMode));

    // 读取用户设置的背景效果
    auto userBackdrop = webview_prefs::GetBackdropEffect();
    int backdropType = DWMSBT_MAINWINDOW_V; // 默认 Mica
    switch (userBackdrop) {
        case webview_prefs::BackdropEffect::None:
            backdropType = DWMSBT_NONE_V;
            break;
        case webview_prefs::BackdropEffect::Mica:
            backdropType = DWMSBT_MAINWINDOW_V;
            break;
        case webview_prefs::BackdropEffect::MicaAlt:
        case webview_prefs::BackdropEffect::Tabbed:
            backdropType = DWMSBT_TABBEDWINDOW_V;
            break;
        case webview_prefs::BackdropEffect::Acrylic:
            backdropType = DWMSBT_TRANSIENTWINDOW_V;
            break;
        default:
            backdropType = DWMSBT_MAINWINDOW_V;
            break;
    }

    if (backdropType == DWMSBT_NONE_V) {
        LOG("Backdrop effect disabled by user setting");
        UpdateDpiDependentSizes();
        return;
    }

    // 写入 DWMWA_SYSTEMBACKDROP_TYPE (Windows 11 22H2+)
    HRESULT hr = S_DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE_V,
        &backdropType, sizeof(backdropType));

    if (SUCCEEDED(hr)) {
        LOG("Mica effect enabled (DWMWA_SYSTEMBACKDROP_TYPE)");
    } else {
        // Fallback: Windows 11 21H2 旧 API
        BOOL enable = TRUE;
        constexpr DWORD DWMWA_MICA_EFFECT_V = 1029;
        hr = S_DwmSetWindowAttribute(hwnd_, DWMWA_MICA_EFFECT_V,
            &enable, sizeof(enable));
        if (SUCCEEDED(hr)) {
            LOG("Mica effect enabled (DWMWA_MICA_EFFECT fallback)");
        } else {
            LOG("Failed to enable any backdrop effect");
        }
    }

    UpdateDpiDependentSizes();
}

void MainWindow::RefreshBackdropEffect() {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    
    LOG("Refreshing backdrop effect...");

    ApplyResolvedChrome(true);
    MaybeCommitStartupRevealAfterChromeReady("RefreshBackdropEffect");
    
    LOG("Backdrop effect refreshed");
}

const char* MainWindow::GetTraceEffect(bool active) const {
    const std::string& effect = active
        ? resolvedChromeState_.effectiveActiveEffect
        : resolvedChromeState_.effectiveInactiveEffect;
    return effect.empty() ? "-" : effect.c_str();
}

void MainWindow::TraceWindowPhase(const char* phase,
                                  const char* active,
                                  const char* forceRefresh,
                                  const char* effect,
                                  const char* previousEffect,
                                  const char* extra) const {
    WindowChromeTrace::EmitWindowPhase({
        .phase = phase,
        .windowKind = "main",
        .defaultWindowId = "main",
        .windowId = GetWindowId(),
        .hwnd = hwnd_,
        .startupRevealPending = startupRevealPending_,
        .startupRevealCommitted = startupRevealCommitted_,
        .startupRevealSettling = startupRevealSettling_,
        .isActive = isActive_,
        .active = active,
        .forceRefresh = forceRefresh,
        .effect = (effect && effect[0]) ? effect : GetTraceEffect(isActive_),
        .lastBackdropEffect = lastBackdropEffect_,
        .previousEffect = previousEffect,
        .extra = extra
    });
}

WindowChromeBaseState MainWindow::BuildChromeBaseState() const {
    WindowChromeBaseState base;
    base.backdropPolicy.activeEffect = "inherit";
    base.backdropPolicy.inactiveEffect = "inherit";
    base.backdropPolicy.darkMode = true;
    base.backdropPolicy.reapplyOnActivate = false;
    base.inheritedBackdropEffect = S_GetUserBackdropEffectString();
    base.frameless = frameless_;
    base.cornerPreference = cornerPreference_;
    base.transparentBackground = false;
    return base;
}

WindowChromeStandardState MainWindow::BuildChromeStandardState() const {
    WindowChromeStandardState standard;
    standard.backdropPolicy = backdropPolicyOverrides_;
    return standard;
}

WindowChromeDerivedState MainWindow::BuildChromeDerivedState() const {
    WindowChromeDerivedState derived;
    derived.active = isActive_;
    derived.fullscreen = IsWindowFullscreen();
    derived.supportsCornerPreference = true;
    derived.supportsBackdropPolicy = true;
    derived.supportsMicaAlt = true;
    derived.windowKind = WindowKind::Main;
    return derived;
}

WindowChromeApplyHooks MainWindow::BuildChromeApplyHooks() {
    SyncStartupRevealProjection();
    WindowChromeApplyHooks hooks;
    hooks.hwnd = hwnd_;
    hooks.webView = webView_ ? webView_.get() : nullptr;
    hooks.diagnosticWindowKind = "main";
    hooks.diagnosticWindowId = GetWindowId().empty() ? "main" : GetWindowId().c_str();
    hooks.diagnosticStartupRevealPending = startupRevealPending_;
    hooks.diagnosticStartupRevealCommitted = startupRevealCommitted_;
    hooks.diagnosticStartupRevealSettling = startupRevealSettling_;
    hooks.diagnosticIsActive = isActive_;
    hooks.applyFrameless = [this](bool frameless) {
        ApplyFramelessState(frameless);
    };
    hooks.applyNativeFrameStrategy = [this](WindowNativeFrameStrategy strategy) {
        ApplyNativeFrameStrategy(strategy);
    };
    hooks.applyCornerPreference = [this](const std::string& mode) {
        ApplyCornerPreferenceState(mode);
    };
    hooks.afterBackdropApplied = [this]() {
        UpdateDpiDependentSizes();
    };
    hooks.canBroadcastBackdropStateChanged = [this]() {
        return CanBroadcastBackdropStateChanged();
    };
    hooks.broadcastBackdropStateChanged = [this](bool active, const std::string& mode,
                                                 const std::string& effect) {
        BroadcastBackdropStateChanged(active, mode, effect);
    };
    return hooks;
}

bool MainWindow::ApplyResolvedChrome(bool forceRefresh) {
    ResolveBackdropPolicy();
    const bool applied = ApplyBackdropPolicyForActivation(isActive_, forceRefresh);
    return applied;
}

bool MainWindow::UpdateCompatibilityBackdropEffect(const std::optional<std::string>& effect,
                                                   const std::optional<bool>& darkMode,
                                                   bool clearBlur, bool forceRefresh) {
    chromeCompatibilityOverrides_.legacyBackdropEffect = effect;
    if (darkMode.has_value()) {
        chromeCompatibilityOverrides_.legacyDarkMode = darkMode;
    }
    if (clearBlur) {
        chromeCompatibilityOverrides_.legacyUseBlurBehind = false;
    }
    const bool applied = ApplyResolvedChrome(forceRefresh);
    MaybeCommitStartupRevealAfterChromeReady("UpdateCompatibilityBackdropEffect");
    return applied;
}

bool MainWindow::UpdateCompatibilityBlur(bool enabled, bool forceRefresh) {
    chromeCompatibilityOverrides_.legacyUseBlurBehind = enabled;
    const bool applied = ApplyResolvedChrome(forceRefresh);
    MaybeCommitStartupRevealAfterChromeReady("UpdateCompatibilityBlur");
    return applied;
}

bool MainWindow::UpdateCompatibilityDarkMode(bool enabled, bool forceRefresh) {
    chromeCompatibilityOverrides_.legacyDarkMode = enabled;
    const bool applied = ApplyResolvedChrome(forceRefresh);
    MaybeCommitStartupRevealAfterChromeReady("UpdateCompatibilityDarkMode");
    return applied;
}

bool MainWindow::UpdateCompatibilityTransparentBackground(bool transparent, bool forceRefresh) {
    chromeCompatibilityOverrides_.legacyTransparentBackground = transparent;
    const bool applied = ApplyResolvedChrome(forceRefresh);
    MaybeCommitStartupRevealAfterChromeReady("UpdateCompatibilityTransparentBackground");
    return applied;
}

json MainWindow::GetBackdropPolicyInfo() const {
    return {
        {"backdropPolicy", backdropPolicyOverrides_},
        {"resolvedBackdropPolicy", {
            {"activeEffect", resolvedBackdropPolicy_.activeEffect},
            {"inactiveEffect", resolvedBackdropPolicy_.inactiveEffect},
            {"darkMode", resolvedBackdropPolicy_.darkMode},
            {"reapplyOnActivate", resolvedBackdropPolicy_.reapplyOnActivate}
        }}
    };
}

bool MainWindow::SetBackdropPolicy(const json& policyPatch, std::string& error) {
    if (!policyPatch.is_object()) {
        error = "backdropPolicy must be an object";
        return false;
    }

    for (auto it = policyPatch.begin(); it != policyPatch.end(); ++it) {
        if (it.value().is_null()) {
            backdropPolicyOverrides_.erase(it.key());
        } else {
            backdropPolicyOverrides_[it.key()] = it.value();
        }
    }

    const bool applied = ApplyResolvedChrome(true);
    MaybeCommitStartupRevealAfterChromeReady("SetBackdropPolicy");
    return applied;
}

void MainWindow::ResolveBackdropPolicy() {
    WindowChromeResolverSnapshot snapshot;
    snapshot.base = BuildChromeBaseState();
    snapshot.compatibility = chromeCompatibilityOverrides_;
    snapshot.standard = BuildChromeStandardState();
    snapshot.derived = BuildChromeDerivedState();

    resolvedChromeState_ = ChromeController::Resolve(snapshot);
    resolvedBackdropPolicy_.activeEffect = resolvedChromeState_.backdropPolicy.activeEffect;
    resolvedBackdropPolicy_.inactiveEffect = resolvedChromeState_.backdropPolicy.inactiveEffect;
    resolvedBackdropPolicy_.darkMode = resolvedChromeState_.backdropPolicy.darkMode;
    resolvedBackdropPolicy_.reapplyOnActivate = resolvedChromeState_.backdropPolicy.reapplyOnActivate;
}

bool MainWindow::ApplyBackdropPolicyForActivation(bool active, bool forceRefresh) {
    // [Legacy] immediate-show 旧实验门控（Experiment J），cold-start reveal 路径下
    // startupChromeLayerSuppressed_ 始终为 false，此分支不再触发。
    // 保留代码以便调试历史行为，待后续审计决定是否物理删除。
    if (startupChromeLayerSuppressed_) {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=ApplyBackdropPolicyForActivation.chromeLayerSuppressed"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " active=" << WindowChromeTrace::BoolText(active)
               << " forceRefresh=" << WindowChromeTrace::BoolText(forceRefresh)
               << " reason=startup-chrome-layer-suppressed-until-webview-ready";
        S_EmitEvidenceLine(stream.str());
        return false;
    }

    // 隐藏阶段抑制 DWM 写入：DWM 对不可见窗口写入 SYSTEMBACKDROP_TYPE 不会
    // 初始化合成管线，ShowWindow 后重写相同值被视为 no-op。
    // resolved state 已由 ResolveBackdropPolicy() 缓存，ShowWindow 后首次可见时写入。
    if (!startupImmediateShowMode_ && startupRevealPending_ && hwnd_ && !IsWindowVisible(hwnd_)) {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=ApplyBackdropPolicyForActivation.suppressed"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " active=" << WindowChromeTrace::BoolText(active)
               << " forceRefresh=" << WindowChromeTrace::BoolText(forceRefresh)
               << " resolvedActiveEffect=" << resolvedChromeState_.effectiveActiveEffect.c_str()
               << " resolvedInactiveEffect=" << resolvedChromeState_.effectiveInactiveEffect.c_str()
               << " reason=startup-hidden-phase-suppression";
        S_EmitEvidenceLine(stream.str());
        return false;
    }

    // 延迟 backdrop 写入门控：ShowWindow 后需等待 DWM 完成首次合成，
    // 在 DEFERRED_BACKDROP_TIMER_ID timer 触发前阻止所有 DWM 写入。
    if (!startupImmediateShowMode_ && deferredBackdropPending_) {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=ApplyBackdropPolicyForActivation.deferred"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " active=" << WindowChromeTrace::BoolText(active)
               << " forceRefresh=" << WindowChromeTrace::BoolText(forceRefresh)
               << " resolvedActiveEffect=" << resolvedChromeState_.effectiveActiveEffect.c_str()
               << " resolvedInactiveEffect=" << resolvedChromeState_.effectiveInactiveEffect.c_str()
               << " reason=deferred-backdrop-pending";
        S_EmitEvidenceLine(stream.str());
        return false;
    }

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=ApplyBackdropPolicyForActivation.begin"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " active=" << WindowChromeTrace::BoolText(active)
               << " forceRefresh=" << WindowChromeTrace::BoolText(forceRefresh)
               << " previousMode=" << (lastBackdropMode_.empty() ? "-" : lastBackdropMode_.c_str())
               << " previousEffect=" << (lastBackdropEffect_.empty() ? "-" : lastBackdropEffect_.c_str())
               << " resolvedActiveEffect=" << resolvedChromeState_.effectiveActiveEffect.c_str()
               << " resolvedInactiveEffect=" << resolvedChromeState_.effectiveInactiveEffect.c_str()
               << " startupRevealPending=" << WindowChromeTrace::BoolText(startupRevealPending_)
               << " startupRevealCommitted=" << WindowChromeTrace::BoolText(startupRevealCommitted_)
               << " startupRevealSettling=" << WindowChromeTrace::BoolText(startupRevealSettling_);
        S_EmitEvidenceLine(stream.str());
    }

    WindowChromeApplyRequest request;
    request.state = resolvedChromeState_;
    request.active = active;
    request.forceRefresh = forceRefresh;
    request.previousMode = lastBackdropMode_;
    request.previousEffect = lastBackdropEffect_;

    WindowChromeApplyResult result = ChromeController::Apply(BuildChromeApplyHooks(), request);
    if (result.success) {
        lastBackdropMode_ = result.mode;
        lastBackdropEffect_ = result.effect;
    }

    {
        std::ostringstream stream;
        stream << "[ActivationEvidence] source=ApplyBackdropPolicyForActivation.end"
               << " t=" << WindowChromeTrace::RelativeMs() << "ms"
               << " active=" << WindowChromeTrace::BoolText(active)
               << " forceRefresh=" << WindowChromeTrace::BoolText(forceRefresh)
               << " success=" << WindowChromeTrace::BoolText(result.success)
               << " mode=" << (result.mode.empty() ? "-" : result.mode.c_str())
               << " effect=" << (result.effect.empty() ? "-" : result.effect.c_str());
        S_EmitEvidenceLine(stream.str());
    }

    return result.success;
}

void MainWindow::ApplyBackdropEffect(const std::string& effect, bool forceRefresh) {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    WindowChromeResolverSnapshot snapshot;
    snapshot.base = BuildChromeBaseState();
    snapshot.compatibility = chromeCompatibilityOverrides_;
    snapshot.standard = BuildChromeStandardState();
    snapshot.standard.backdropPolicy["activeEffect"] = effect;
    snapshot.standard.backdropPolicy["inactiveEffect"] = effect;
    snapshot.derived = BuildChromeDerivedState();

    WindowChromeApplyRequest request;
    request.state = ChromeController::Resolve(snapshot);
    request.active = isActive_;
    request.forceRefresh = forceRefresh;
    request.previousMode = lastBackdropMode_;
    request.previousEffect = lastBackdropEffect_;

    WindowChromeApplyResult result = ChromeController::Apply(BuildChromeApplyHooks(), request);
    if (result.success) {
        lastBackdropMode_ = result.mode;
        lastBackdropEffect_ = result.effect;
    }
}

bool MainWindow::CanBroadcastBackdropStateChanged() const {
    return chromeBackdropBroadcastReady_ &&
        !startupRevealPending_ &&
        !startupRevealSettling_ &&
        hwnd_ &&
        IsWindow(hwnd_) &&
        webView_ &&
        webView_->IsReady() &&
        WebViewContext::GetInstance().HasInstance(hwnd_);
}

void MainWindow::BroadcastBackdropStateChanged(bool active, const std::string& mode,
                                               const std::string& effect) const {
    json data = {
        {"windowId", "main"},
        {"active", active},
        {"mode", mode},
        {"effect", effect}
    };
    WebViewContext::GetInstance().BroadcastEvent("window:backdropStateChanged", data);
}

std::string MainWindow::NormalizeBackdropEffect(const std::string& effect, bool allowSystem,
                                                const std::string& fallback) {
    return WindowChromeResolver::NormalizeBackdropEffect(effect, allowSystem, fallback);
}

void MainWindow::ExtendFrameIntoClientArea() {
    // 使用标准系统标题栏时，不需要扩展边框
    // 只设置一个小的边距以确保 Mica 效果正常工作
    MARGINS margins = { 0, 0, 0, 0 };
    S_DwmExtendFrameIntoClientArea(hwnd_, &margins);
}

void MainWindow::UpdateDpiDependentSizes() {
    int dpi = GetDpiForWindow(hwnd_);
    
    // 根据 DPI 调整尺寸
    titlebarHeight_ = MulDiv(DEFAULT_TITLEBAR_HEIGHT, dpi, 96);
    resizeBorderWidth_ = MulDiv(8, dpi, 96);
    captionButtonWidth_ = MulDiv(46, dpi, 96);
}

// ============================================
// 运行时标题栏切换
// ============================================

void MainWindow::SetFrameless(bool frameless) {
    if (!hwnd_ || !IsWindow(hwnd_)) return;
    if (frameless_ == frameless) return;  // 无变化
    
    frameless_ = frameless;
    ApplyResolvedChrome(true);
    MaybeCommitStartupRevealAfterChromeReady("SetFrameless");
    
    LOG("MainWindow::SetFrameless(", frameless ? "true" : "false", ")");
}

bool MainWindow::ShouldRouteToDwmDefWindowProc(UINT msg) const {
    // WM_NCHITTEST 始终不路由给 DwmDefWindowProc（由 OnNcHitTest 自行处理）
    if (msg == WM_NCHITTEST) return false;

    // Bootstrap 和 SteadyState 阶段均全量路由给 DwmDefWindowProc。
    // v1.1.19 验证结论：DwmDefWindowProc 全量路由是三大键不可见的前提条件。
    // 三大键的视觉由 WS_SYSMENU 移除消除（DWM 按钮 overlay 依赖 WS_SYSMENU），
    // DwmDefWindowProc 负责将 NC 消息在 DWM composited 管线内部消化，
    // 阻止 DefWindowProcW 介入 NC 绘制。
    // 若将 WM_NCACTIVATE 等从路由中移除，DefWindowProcW 会尝试自行绘制
    // NC 装饰，导致三大键 overlay 残影。
    return true;
}

void MainWindow::ApplyFramelessState(bool frameless) {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    // [v1.1.19 对齐] 短路守卫：v1.1.19 的 SetFrameless 在 frameless_==frameless 时直接 return。
    // 当前 Chrome Apply 链会在启动期多次调用此函数，但 frameless 状态不变，
    // 重复执行 DwmExtendFrameIntoClientArea + SWP_FRAMECHANGED + RedrawWindow(RDW_FRAME)
    // 会触发 DWM 重新评估帧合成，导致三大键 overlay 复现。
    if (framelessDwmApplied_ && frameless_ == frameless) {
        return;
    }
    framelessDwmApplied_ = true;

    if (frameless) {
        MARGINS margins = { -1, -1, -1, -1 };
        S_DwmExtendFrameIntoClientArea(hwnd_, &margins);
    } else {
        MARGINS margins = { 0, 0, 0, 0 };
        S_DwmExtendFrameIntoClientArea(hwnd_, &margins);
    }

    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_FRAME);
}

void MainWindow::ApplyNativeFrameStrategy(WindowNativeFrameStrategy strategy) {
    if (!hwnd_ || !IsWindow(hwnd_)) return;

    // Bootstrap 阶段不改 style bits（由 OnCreate 初始化的 WS_OVERLAPPEDWINDOW 保持）
    if (chromePhase_ == MainChromePhase::Bootstrap) return;

    LONG_PTR curStyle = GetWindowLongPtrW(hwnd_, GWL_STYLE);
    LONG_PTR newStyle = curStyle;

    switch (strategy) {
    case WindowNativeFrameStrategy::Standard:
        // 恢复 WS_OVERLAPPEDWINDOW 族标准位
        newStyle |= (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
        newStyle &= ~WS_POPUP;
        if (resizable_) {
            newStyle |= WS_THICKFRAME;
        } else {
            newStyle &= ~WS_THICKFRAME;
        }
        break;

    case WindowNativeFrameStrategy::CaptionlessOverlapped:
        // 保留 WS_CAPTION 使 DWM 标准动画正常工作。
        // 仅移除 WS_SYSMENU 消除全部 DWM 三大键按钮 overlay 渲染。
        // WS_MINIMIZEBOX/WS_MAXIMIZEBOX 仅在 WS_SYSMENU 存在时影响按钮，
        // 保留它们确保 Shell 任务栏点击最小化/恢复切换功能正常。
        // WM_NCCALCSIZE return 0（NC=0）+ DwmDefWindowProc 全量路由保持不变。
        newStyle &= ~WS_SYSMENU;
        if (resizable_) {
            newStyle |= WS_THICKFRAME;
        } else {
            newStyle &= ~WS_THICKFRAME;
        }
        break;

    case WindowNativeFrameStrategy::Fullscreen:
        // Fullscreen style 由 WindowApi.cpp 的 EnterFullscreenMode/ExitFullscreenMode 管理
        // 此处不抢 authority
        return;

    case WindowNativeFrameStrategy::Popup:
        // 主窗口稳态禁止使用 WS_POPUP
        return;
    }

    if (newStyle != curStyle) {
        SetWindowLongPtrW(hwnd_, GWL_STYLE, newStyle);
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void MainWindow::SetResizable(bool resizable) {
    LOG("SetResizable called: ", resizable ? "true" : "false", ", current: ", resizable_ ? "true" : "false");
    if (resizable_ == resizable) {
        LOG("SetResizable: no change needed");
        return;
    }
    resizable_ = resizable;
    LOG("SetResizable: updated to ", resizable_ ? "true" : "false");
    
    if (hwnd_) {
        // [Chrome Reconciliation] 不再直接写 WS_THICKFRAME，通过 shared chrome apply 统一刷新样式。
        ApplyResolvedChrome(true);
        
        // 触发重新布局以更新 WebView 边距
        RECT rect;
        GetClientRect(hwnd_, &rect);
        OnSize(rect.right, rect.bottom);
        
        LOG("SetResizable: window style updated via shared chrome apply");
    }
}

void MainWindow::SetCornerPreference(const std::string& mode) {
    if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("SetCornerPreference called: ", mode);
    
    if (!hwnd_) {
        LOG("SetCornerPreference: no window handle");
        return;
    }
    
    cornerPreference_ = mode;

    ApplyResolvedChrome(true);
    MaybeCommitStartupRevealAfterChromeReady("SetCornerPreference");
}

void MainWindow::ApplyCornerPreferenceState(const std::string& mode) {
    // WS_EX_NOREDIRECTIONBITMAP + DwmExtendFrameIntoClientArea({-1}) + NCCALCSIZE→0 配置下，
    // DWMWCP_DEFAULT(0) 会让 DWM 解析为直角（因为没有标准 NC 区域框架）。
    // 将 "default" 映射到 DWMWCP_ROUND(2)，与用户对 Win11 圆角窗口的预期一致。
    DWORD cornerPreference = 2;  // DWMWCP_ROUND

    if (mode == "none") {
        cornerPreference = 1;  // DWMWCP_DONOTROUND
    } else if (mode == "round" || mode == "default") {
        cornerPreference = 2;  // DWMWCP_ROUND
    } else if (mode == "small") {
        cornerPreference = 3;  // DWMWCP_ROUNDSMALL
    }

    HRESULT hr = S_DwmSetWindowAttribute(hwnd_, 33, &cornerPreference, sizeof(cornerPreference));
    if (SUCCEEDED(hr)) {
        if (WindowChromeTrace::AuxiliaryTraceEnabled()) LOG("SetCornerPreference: set to ", mode, " (", cornerPreference, ")");
    } else {
        LOG("SetCornerPreference: DwmSetWindowAttribute failed, hr=", hr);
    }
}
