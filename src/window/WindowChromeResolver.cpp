#include "pch.h"
#include "window/WindowChromeResolver.h"
#include "utils/WindowUtils.h"

namespace {
// 工具 -- 与 PopupWindow / MainWindow 共享的小工具
inline std::string S_ToLower(std::string v) { return WindowUtils::ToLower(std::move(v)); }
inline bool S_TryGetBool(const json& obj, const char* key, bool& out) {
    return WindowUtils::TryGetBool(obj, key, out);
}

static bool S_IsExplicitBackdropField(const WindowChromeStandardState& state, const char* key) {
    return state.backdropPolicy.is_object() && state.backdropPolicy.contains(key) &&
        !state.backdropPolicy[key].is_null();
}

static std::string S_ResolveEffectiveEffect(const std::string& semanticEffect,
                                            const WindowChromeResolverSnapshot& snapshot) {
    std::string effect = semanticEffect;
    if (effect == "inherit") {
        effect = WindowChromeResolver::NormalizeBackdropEffect(
            snapshot.base.inheritedBackdropEffect, false, "none");
    }

    if (!snapshot.derived.supportsBackdropPolicy || snapshot.derived.windowKind == WindowKind::Panel) {
        return "system";
    }

    if (effect == "mica-alt") {
        if (snapshot.derived.windowKind == WindowKind::Popup) {
            return "acrylic";
        }
        if (!snapshot.derived.supportsMicaAlt) {
            return snapshot.derived.windowKind == WindowKind::Main ? "mica" : "acrylic";
        }
    }

    return effect;
}

static bool S_IsPluginManagedEffect(const std::string& effect) {
    return effect == "mica" || effect == "mica-alt" || effect == "acrylic";
}
}

std::string WindowChromeResolver::NormalizeBackdropEffect(const std::string& effect,
                                                          bool allowSystem,
                                                          const std::string& fallback) {
    std::string value = S_ToLower(effect);
    if (value == "inherit" || value == "none" || value == "mica" ||
        value == "mica-alt" || value == "acrylic") {
        return value;
    }
    if (allowSystem && value == "system") {
        return value;
    }
    return fallback;
}

ResolvedWindowChromeState WindowChromeResolver::Resolve(const WindowChromeResolverSnapshot& snapshot) {
    ResolvedWindowChromeState resolved;
    resolved.backdropPolicy = snapshot.base.backdropPolicy;
    resolved.frameless = snapshot.base.frameless;
    resolved.cornerPreference = snapshot.base.cornerPreference;
    resolved.transparentBackground = snapshot.base.transparentBackground;
    resolved.fullscreen = snapshot.derived.fullscreen;

    const bool hasStandardActiveEffect = S_IsExplicitBackdropField(snapshot.standard, "activeEffect");
    const bool hasStandardInactiveEffect = S_IsExplicitBackdropField(snapshot.standard, "inactiveEffect");
    const bool hasStandardDarkMode = S_IsExplicitBackdropField(snapshot.standard, "darkMode");

    if (snapshot.compatibility.legacyBackdropEffect.has_value()) {
        const std::string compatEffect = NormalizeBackdropEffect(
            snapshot.compatibility.legacyBackdropEffect.value(), false,
            resolved.backdropPolicy.activeEffect);
        if (!hasStandardActiveEffect) {
            resolved.backdropPolicy.activeEffect = compatEffect;
        }
        // Do NOT copy legacyBackdropEffect to inactiveEffect.
        // System-backdrop-first baseline: inactiveEffect defaults to "inherit"
        // (same backdrop as active). The platform handles inactive visual
        // distinction natively (DWM dims Mica/Acrylic when inactive).
    }

    if (snapshot.compatibility.legacyDarkMode.has_value() && !hasStandardDarkMode) {
        resolved.backdropPolicy.darkMode = snapshot.compatibility.legacyDarkMode.value();
    }
    if (snapshot.compatibility.legacyUseBlurBehind.has_value()) {
        resolved.useBlurBehind = snapshot.compatibility.legacyUseBlurBehind.value();
    }
    if (snapshot.compatibility.legacyTransparentBackground.has_value()) {
        resolved.transparentBackground = snapshot.compatibility.legacyTransparentBackground.value();
    }

    if (snapshot.standard.backdropPolicy.is_object()) {
        if (snapshot.standard.backdropPolicy.contains("activeEffect") &&
            snapshot.standard.backdropPolicy["activeEffect"].is_string()) {
            resolved.backdropPolicy.activeEffect = NormalizeBackdropEffect(
                snapshot.standard.backdropPolicy["activeEffect"].get<std::string>(), false,
                resolved.backdropPolicy.activeEffect);
        }
        if (snapshot.standard.backdropPolicy.contains("inactiveEffect") &&
            snapshot.standard.backdropPolicy["inactiveEffect"].is_string()) {
            resolved.backdropPolicy.inactiveEffect = NormalizeBackdropEffect(
                snapshot.standard.backdropPolicy["inactiveEffect"].get<std::string>(), true,
                resolved.backdropPolicy.inactiveEffect);
        }
        S_TryGetBool(snapshot.standard.backdropPolicy, "darkMode", resolved.backdropPolicy.darkMode);
        S_TryGetBool(snapshot.standard.backdropPolicy, "reapplyOnActivate",
            resolved.backdropPolicy.reapplyOnActivate);
    }

    if (snapshot.standard.frameless.has_value()) {
        resolved.frameless = snapshot.standard.frameless.value();
    }
    if (snapshot.standard.cornerPreference.has_value()) {
        resolved.cornerPreference = S_ToLower(snapshot.standard.cornerPreference.value());
    }

    if (!snapshot.derived.supportsCornerPreference) {
        resolved.cornerPreference = "unsupported";
    } else if (resolved.fullscreen) {
        resolved.cornerPreference = "default";
    }

    resolved.backdropPolicy.activeEffect = NormalizeBackdropEffect(
        resolved.backdropPolicy.activeEffect, false, snapshot.base.backdropPolicy.activeEffect);
    resolved.backdropPolicy.inactiveEffect = NormalizeBackdropEffect(
        resolved.backdropPolicy.inactiveEffect, true, snapshot.base.backdropPolicy.inactiveEffect);

    resolved.effectiveActiveEffect = S_ResolveEffectiveEffect(resolved.backdropPolicy.activeEffect, snapshot);
    // "inherit" for inactiveEffect means "same backdrop as active" -- inherit from the
    // resolved active effect (which may include compatibility overrides), not independently
    // from inheritedBackdropEffect. This ensures platform-native inactive dimming.
    if (resolved.backdropPolicy.inactiveEffect == "inherit") {
        resolved.effectiveInactiveEffect = resolved.effectiveActiveEffect;
    } else {
        resolved.effectiveInactiveEffect = S_ResolveEffectiveEffect(resolved.backdropPolicy.inactiveEffect, snapshot);
    }

    resolved.transparentBackground = resolved.transparentBackground || resolved.useBlurBehind ||
        S_IsPluginManagedEffect(resolved.effectiveActiveEffect) ||
        S_IsPluginManagedEffect(resolved.effectiveInactiveEffect);

    // 计算 nativeFrameStrategy
    if (resolved.fullscreen) {
        resolved.nativeFrameStrategy = WindowNativeFrameStrategy::Fullscreen;
    } else if (snapshot.derived.windowKind == WindowKind::Main && resolved.frameless) {
        resolved.nativeFrameStrategy = WindowNativeFrameStrategy::CaptionlessOverlapped;
    } else if (!resolved.frameless) {
        resolved.nativeFrameStrategy = WindowNativeFrameStrategy::Standard;
    } else {
        // Popup frameless 走标准边框
        resolved.nativeFrameStrategy = WindowNativeFrameStrategy::Standard;
    }

    return resolved;
}