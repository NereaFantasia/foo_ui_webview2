#pragma once

#include "pch.h"

enum class WindowKind {
    Main,
    Popup,
    Panel
};

// 窗口原生边框策略
enum class WindowNativeFrameStrategy {
    Standard,               // WS_OVERLAPPEDWINDOW 族标准语义
    CaptionlessOverlapped,  // 移除 WS_CAPTION，保留 overlapped 行为位
    Popup,                  // WS_POPUP 族策略
    Fullscreen              // fullscreen 专用策略
};

struct WindowChromeBackdropPolicy {
    std::string activeEffect = "inherit";
    std::string inactiveEffect = "inherit";
    bool darkMode = true;
    bool reapplyOnActivate = false;
};

struct WindowChromeBaseState {
    WindowChromeBackdropPolicy backdropPolicy;
    std::string inheritedBackdropEffect = "none";
    bool frameless = true;
    std::string cornerPreference = "unsupported";
    bool transparentBackground = false;
};

struct WindowChromeCompatibilityOverrides {
    std::optional<std::string> legacyBackdropEffect;
    std::optional<bool> legacyDarkMode;
    std::optional<bool> legacyUseBlurBehind;
    std::optional<bool> legacyTransparentBackground;
};

struct WindowChromeStandardState {
    json backdropPolicy = json::object();
    std::optional<bool> frameless;
    std::optional<std::string> cornerPreference;
};

struct WindowChromeDerivedState {
    bool active = true;
    bool fullscreen = false;
    bool supportsCornerPreference = false;
    bool supportsBackdropPolicy = false;
    bool supportsMicaAlt = false;
    WindowKind windowKind = WindowKind::Main;
};

struct WindowChromeResolverSnapshot {
    WindowChromeBaseState base;
    WindowChromeCompatibilityOverrides compatibility;
    WindowChromeStandardState standard;
    WindowChromeDerivedState derived;
};

struct ResolvedWindowChromeState {
    WindowChromeBackdropPolicy backdropPolicy;
    std::string effectiveActiveEffect = "none";
    std::string effectiveInactiveEffect = "inherit";
    bool frameless = true;
    std::string cornerPreference = "unsupported";
    bool transparentBackground = false;
    bool useBlurBehind = false;
    bool fullscreen = false;
    WindowNativeFrameStrategy nativeFrameStrategy = WindowNativeFrameStrategy::Standard;
};