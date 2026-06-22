#pragma once

#include "pch.h"
#include "window/WindowChromeState.h"

// ============================================
// WindowShellCapabilities - 窗口壳能力描述
// 只描述"能否做"，不描述"当前是否这样做"
// ============================================
struct WindowShellCapabilities {
    bool supportsFullscreen = false;
    bool supportsBackdropPolicy = true;
    bool supportsCornerPreference = false;
    bool supportsMicaAlt = false;
    bool supportsOwnerPolicy = false;
    bool supportsNoActivate = false;
    bool participatesInAppBootstrap = false;
    bool supportsBeforeClose = false;
    WindowKind windowKind = WindowKind::Main;
};

// ============================================
// WindowShellSnapshot - 统一观测模型
// 供 WindowManager::GetAllWindowsInfo() / 调试 / graph 使用
// chrome.resolved 复用现有 ResolvedWindowChromeState
// ============================================
struct WindowShellSnapshot {
    std::string windowId;
    WindowKind kind = WindowKind::Main;
    WindowShellCapabilities capabilities;

    struct Lifecycle {
        bool created = false;
        bool visible = false;
        bool active = false;
        bool minimized = false;
        bool maximized = false;
        bool fullscreen = false;
        bool pendingDestroy = false;
    } lifecycle;

    struct StartupPresentation {
        std::string phase = "waiting-navigation";
        bool navigationCompleted = false;
        bool windowReadySignaled = false;
        bool visualReadySignaled = false;
        bool revealPending = false;
        bool revealCommitted = false;
        bool revealSettling = false;
        bool fallbackArmed = false;
        bool fallbackUsed = false;
    } startupPresentation;

    struct Chrome {
        ResolvedWindowChromeState resolved;
    } chrome;
};

// ============================================
// WindowShellBase - 窗口壳统一抽象接口
//
// MainWindow 和 PopupWindow 通过此接口
// 提供统一的 shell state 观测与命令入口。
// 不引入第二套 WindowChrome* 结构，复用现有
// WindowChromeResolver / WindowChromeApplier。
// ============================================
class WindowShellBase {
public:
    virtual ~WindowShellBase() = default;

    // ========== 身份与能力 ==========
    virtual std::string GetShellWindowId() const = 0;
    virtual WindowKind GetWindowKind() const = 0;
    virtual HWND GetShellHwnd() const = 0;
    virtual WindowShellCapabilities GetCapabilities() const = 0;

    // ========== 统一观测 ==========
    virtual WindowShellSnapshot GetShellSnapshot() const = 0;
    virtual json GetBackdropPolicyInfo() const = 0;

    // ========== Chrome 命令（标准主路径） ==========
    virtual bool PatchBackdropPolicy(const json& policyPatch, std::string& error) = 0;
    virtual void PatchFrameless(bool frameless) = 0;
    virtual void RefreshChrome() = 0;

    // ========== Chrome 命令（compatibility adapter） ==========
    virtual bool PatchCompatibilityBackdrop(const std::optional<std::string>& effect,
                                            const std::optional<bool>& darkMode,
                                            bool clearBlur, bool forceRefresh = true) = 0;
    virtual bool PatchCompatibilityBlur(bool enabled, bool forceRefresh = true) = 0;
    virtual bool PatchCompatibilityDarkMode(bool enabled, bool forceRefresh = true) = 0;
    virtual bool PatchCompatibilityTransparency(bool transparent, bool forceRefresh = true) = 0;

    // ========== Fullscreen 生命周期 ==========
    virtual bool IsFullscreen() const = 0;
    virtual void NotifyFullscreenChanged(bool isFullscreen) = 0;
    // 仅更新 isFullscreen_ 标志，不触发 RefreshBackdropEffect。
    // 用于 ExitFullscreenMode 中 SC_MAXIMIZE 之前须先清标志但窗口仍隐藏的时序场景。
    virtual void SetFullscreenFlag(bool isFullscreen) = 0;

    // ========== 状态查询 ==========
    virtual bool IsActive() const = 0;
    virtual bool IsFrameless() const = 0;

    // ========== 全屏预保存状态（per-window，Chromium 风格） ==========
    // savedWindowInfo_.has_value() == true 表示窗口当前处于全屏模式。
    // EnterFullscreenMode 设置，ExitFullscreenMode 读取后 reset。
    struct SavedWindowInfo {
        bool maximized = false;
        DWORD style = 0;
        DWORD ex_style = 0;
        RECT window_rect = {};
        DWORD corner_preference = 0;
    };
    std::optional<SavedWindowInfo> savedWindowInfo_;
};
