// MainWindowShell.cpp — MainWindow 对 WindowShellBase 的实现（窗口 id、类型、能力、快照与各项补丁入口）
#include "pch.h"
#include "window/MainWindow.h"

// ============================================
// WindowShellBase 实现
// MainWindow 作为 shell adapter 接入统一抽象
// ============================================

std::string MainWindow::GetShellWindowId() const {
    return GetWindowId();
}

WindowKind MainWindow::GetWindowKind() const {
    return WindowKind::Main;
}

HWND MainWindow::GetShellHwnd() const {
    return GetHwnd();
}

WindowShellCapabilities MainWindow::GetCapabilities() const {
    WindowShellCapabilities caps;
    caps.supportsFullscreen = true;
    caps.supportsBackdropPolicy = true;
    caps.supportsCornerPreference = true;
    caps.supportsMicaAlt = true;
    caps.supportsOwnerPolicy = false;
    caps.supportsNoActivate = false;
    caps.participatesInAppBootstrap = true;
    caps.supportsBeforeClose = false;
    caps.windowKind = WindowKind::Main;
    return caps;
}

WindowShellSnapshot MainWindow::GetShellSnapshot() const {
    WindowShellSnapshot snap;
    snap.windowId = GetWindowId();
    snap.kind = WindowKind::Main;
    snap.capabilities = GetCapabilities();

    snap.lifecycle.created = (hwnd_ != nullptr);
    snap.lifecycle.visible = hwnd_ && IsWindowVisible(hwnd_);
    snap.lifecycle.active = isActive_;
    snap.lifecycle.minimized = isMinimized_;
    snap.lifecycle.maximized = isMaximized_;
    snap.lifecycle.fullscreen = isFullscreen_;
    snap.lifecycle.pendingDestroy = false;

    const auto startupPresentation = startupPresentationCoordinator_.GetSnapshot();
    snap.startupPresentation.phase = startupPresentation.phase;
    snap.startupPresentation.navigationCompleted = startupPresentation.navigationCompleted;
    snap.startupPresentation.windowReadySignaled = startupPresentation.windowReadySignaled;
    snap.startupPresentation.visualReadySignaled = startupPresentation.visualReadySignaled;
    snap.startupPresentation.revealPending = startupPresentation.revealPending;
    snap.startupPresentation.revealCommitted = startupPresentation.revealCommitted;
    snap.startupPresentation.revealSettling = startupPresentation.revealSettling;
    snap.startupPresentation.fallbackArmed = startupPresentation.fallbackArmed;
    snap.startupPresentation.fallbackUsed = startupPresentation.fallbackUsed;

    snap.chrome.resolved = resolvedChromeState_;
    return snap;
}

bool MainWindow::PatchBackdropPolicy(const json& policyPatch, std::string& error) {
    return SetBackdropPolicy(policyPatch, error);
}

void MainWindow::PatchFrameless(bool frameless) {
    SetFrameless(frameless);
}

void MainWindow::RefreshChrome() {
    RefreshBackdropEffect();
}

bool MainWindow::PatchCompatibilityBackdrop(const std::optional<std::string>& effect,
                                            const std::optional<bool>& darkMode,
                                            bool clearBlur, bool forceRefresh) {
    return UpdateCompatibilityBackdropEffect(effect, darkMode, clearBlur, forceRefresh);
}

bool MainWindow::PatchCompatibilityBlur(bool enabled, bool forceRefresh) {
    return UpdateCompatibilityBlur(enabled, forceRefresh);
}

bool MainWindow::PatchCompatibilityDarkMode(bool enabled, bool forceRefresh) {
    return UpdateCompatibilityDarkMode(enabled, forceRefresh);
}

bool MainWindow::PatchCompatibilityTransparency(bool transparent, bool forceRefresh) {
    return UpdateCompatibilityTransparentBackground(transparent, forceRefresh);
}

bool MainWindow::IsFullscreen() const {
    return savedWindowInfo_.has_value();
}

void MainWindow::NotifyFullscreenChanged(bool isFullscreen) {
    isFullscreen_ = isFullscreen;
    RefreshBackdropEffect();
    // 全屏状态变化需广播给前端（window:stateChanged 携带 isFullscreen/fullscreen 字段）
    BroadcastWindowStateChangedIfNeeded(true);
}

void MainWindow::SetFullscreenFlag(bool isFullscreen) {
    isFullscreen_ = isFullscreen;
}

bool MainWindow::IsActive() const {
    return isActive_;
}
