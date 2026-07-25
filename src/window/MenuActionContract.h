#pragma once
// MenuActionContract.h - internal routing identity for tray / self-drawn menu
// actions. Origin and built-in action are INTERNAL metadata: they are never
// parsed from a public API field and never serialized to a public event. A
// Prefixes NEVER decide routing. A narrow compatibility allowlist may promote
// an exact historical reserved id to a fixed built-in action before the menu is
// composed; similar ids and arbitrary prefix matches remain user actions. After
// composition, only the trusted origin decides whether a selection executes a
// built-in command or reaches the frontend callback.
//
// Shared by TrayIcon (assigns origin to composed items, routes ResolvedAction)
// and MenuOverlayHost (indexes it per opaque token). Kept dependency-light
// (<string> only) so the generic MenuOverlayHost need not depend on TrayIcon.h.

#include <optional>
#include <string>
#include <vector>

namespace menu_action {

// Trusted source of a menu item. Built-in origins run a fixed command; user
// origin always reaches the frontend callback / event.
enum class Origin { User, BuiltinPlayback, BuiltinSystem };

// The concrete built-in command a built-in item runs. Meaningful only for
// built-in origins; user items are always None.
enum class Builtin { None, PlayPause, Previous, Next, Stop, Exit };

// The resolved identity of a selected menu action: the public id echoed to the
// frontend plus the trusted origin/built-in that decides routing.
struct ResolvedAction {
    std::string publicId;
    Origin origin = Origin::User;
    Builtin builtin = Builtin::None;
};

// Historical public id that is an explicit tray-only compatibility contract.
// The mapping is exact and case-sensitive; no prefix or caller-supplied action
// is accepted. Public 1.9.0 frontends used `_sys_exit` as a real application
// exit command. The playback ids were implementation identifiers for injected
// defaults, not a documented caller-supplied command contract, so they remain
// ordinary user ids when supplied by a frontend.
inline std::optional<ResolvedAction> ReservedBuiltinForPublicId(const std::string& publicId) {
    if (publicId == "_sys_exit") {
        return ResolvedAction{ publicId, Origin::BuiltinSystem, Builtin::Exit };
    }
    return std::nullopt;
}

// Where a resolved action routes. Derived from origin ONLY (never the id).
enum class RouteDecision { FireUserCallback, ExecutePlayback, ExecuteSystem };

inline RouteDecision DecideRoute(const ResolvedAction& action) {
    switch (action.origin) {
        case Origin::BuiltinPlayback: return RouteDecision::ExecutePlayback;
        case Origin::BuiltinSystem:   return RouteDecision::ExecuteSystem;
        case Origin::User:            return RouteDecision::FireUserCallback;
    }
    return RouteDecision::FireUserCallback;
}

// ── string transport (for the opaque-token index JSON in DESIGN 8.3) ────────
// Origin / Builtin cross the C++ <-> internal-JSON boundary as short strings so
// MenuOverlayHost can index them without a compile-time dependency on TrayIcon.

inline const char* OriginToString(Origin o) {
    switch (o) {
        case Origin::User:            return "user";
        case Origin::BuiltinPlayback: return "builtin-playback";
        case Origin::BuiltinSystem:   return "builtin-system";
    }
    return "user";
}

inline Origin OriginFromString(const std::string& s) {
    if (s == "builtin-playback") return Origin::BuiltinPlayback;
    if (s == "builtin-system")   return Origin::BuiltinSystem;
    return Origin::User;  // unknown / absent -> user (never trusted as built-in)
}

inline const char* BuiltinToString(Builtin b) {
    switch (b) {
        case Builtin::None:      return "none";
        case Builtin::PlayPause: return "play-pause";
        case Builtin::Previous:  return "previous";
        case Builtin::Next:      return "next";
        case Builtin::Stop:      return "stop";
        case Builtin::Exit:      return "exit";
    }
    return "none";
}

inline Builtin BuiltinFromString(const std::string& s) {
    if (s == "play-pause") return Builtin::PlayPause;
    if (s == "previous")   return Builtin::Previous;
    if (s == "next")       return Builtin::Next;
    if (s == "stop")       return Builtin::Stop;
    if (s == "exit")       return Builtin::Exit;
    return Builtin::None;
}

// Caller-declared native playback action (public `TrayMenuItem.playbackAction`).
// Maps the four accepted tokens to their built-in action; `exit` is deliberately
// NOT accepted so this public field can never declare a system-exit route (that
// stays exclusive to the exact `_sys_exit` compatibility id). An unknown token
// yields nullopt, which the API layer rejects fail-loud.
inline std::optional<Builtin> PlaybackActionFromString(const std::string& s) {
    if (s == "play-pause") return Builtin::PlayPause;
    if (s == "previous")   return Builtin::Previous;
    if (s == "next")       return Builtin::Next;
    if (s == "stop")       return Builtin::Stop;
    return std::nullopt;
}

// Inverse of PlaybackActionFromString for round-tripping the public field via
// getMenuItems. Only the four playback actions have a public token; every other
// built-in (including Exit) and None return an empty string.
inline const char* PlaybackActionToString(Builtin b) {
    switch (b) {
        case Builtin::PlayPause: return "play-pause";
        case Builtin::Previous:  return "previous";
        case Builtin::Next:      return "next";
        case Builtin::Stop:      return "stop";
        case Builtin::None:
        case Builtin::Exit:      return "";
    }
    return "";
}

// Internal action fields are trusted only when they describe one complete,
// known origin/action pair. This keeps malformed or partial private metadata
// on the user-action path rather than granting a built-in route.
inline bool IsTrustedBuiltinPair(Origin origin, Builtin builtin) {
    switch (origin) {
        case Origin::BuiltinPlayback:
            return builtin == Builtin::PlayPause || builtin == Builtin::Previous ||
                builtin == Builtin::Next || builtin == Builtin::Stop;
        case Origin::BuiltinSystem:
            return builtin == Builtin::Exit;
        case Origin::User:
            return false;
    }
    return false;
}

inline ResolvedAction NormalizeResolvedAction(ResolvedAction action) {
    if (IsTrustedBuiltinPair(action.origin, action.builtin)) return action;
    action.origin = Origin::User;
    action.builtin = Builtin::None;
    return action;
}

// These fields cross only the tray owner-mode C++ -> internal JSON boundary.
// They are absent for user actions, so a public menu payload cannot create a
// built-in route merely by spelling a private property.
struct InternalActionFields {
    std::optional<std::string> origin;
    std::optional<std::string> builtin;

    bool IsStamped() const { return origin.has_value() && builtin.has_value(); }
};

inline InternalActionFields MakeInternalActionFields(const ResolvedAction& action) {
    const auto normalized = NormalizeResolvedAction(action);
    if (normalized.origin == Origin::User) return {};
    return InternalActionFields{
        OriginToString(normalized.origin), BuiltinToString(normalized.builtin)
    };
}

inline ResolvedAction ResolveInternalActionFields(
    const std::string& publicId,
    const std::optional<std::string>& origin,
    const std::optional<std::string>& builtin) {
    if (!origin.has_value() || !builtin.has_value()) {
        return ResolvedAction{ publicId, Origin::User, Builtin::None };
    }
    return NormalizeResolvedAction(ResolvedAction{
        publicId, OriginFromString(*origin), BuiltinFromString(*builtin)
    });
}

// ── select / value eligibility (opaque-token resolution in DESIGN 8.3) ──────
// The overlay resolves a renderer-echoed token to its indexed entry, then gates
// the action with these pure predicates. Extracted so the security rules are
// unit-testable without a live overlay window.

// A menu row selects-and-closes only when it is an enabled, non-submenu row that
// is NOT a value control (rating/slider/segmented report through valueChanged,
// submenu parents open a child menu). A disabled row is never selectable.
inline bool CanSelectLeaf(const std::string& kind, bool enabled, bool hasSubmenu) {
    return enabled && !hasSubmenu
        && kind != "rating" && kind != "slider" && kind != "segmented";
}

// A value-control change is accepted only for a value-control kind AND an
// in-range value: rating 0..5, slider within its (normalized) [min,max],
// segmented an existing AND enabled segment index. Everything else is rejected
// (a non-value kind, an out-of-range value, or a disabled/absent segment).
// Constant sliders (normalized min==max) never accept value changes — they are
// display-only. Out-of-range values are rejected as-is
// (no clamp-and-accept).
inline bool RichValueInRange(const std::string& kind, int value,
                             int sliderMin, int sliderMax,
                             const std::vector<char>& enabledSegments) {
    if (kind == "rating")    return value >= 0 && value <= 5;
    if (kind == "slider") {
        if (sliderMin == sliderMax) return false;  // constant: no value change
        return value >= sliderMin && value <= sliderMax;
    }
    if (kind == "segmented") return value >= 0
                                 && value < static_cast<int>(enabledSegments.size())
                                 && enabledSegments[static_cast<size_t>(value)] != 0;
    return false;
}

// ── internal IPC caller / menuId gate (DESIGN 8.1 / 8.2) ────────────────────
// The overlay accepts an internal menu.__* call only from its own window and,
// for select/dismiss/ready/valueChanged, only for the currently-visible menu.
// Extracted as pure predicates (window handles as opaque void* to stay Win32-
// header-free) so the gate is unit-testable without a live overlay.

// The caller must be the overlay's own window; a null overlay (menu not shown)
// rejects everything.
inline bool IsSameOverlayWindow(const void* caller, const void* overlayWindow) {
    return overlayWindow != nullptr && caller == overlayWindow;
}

// menuId must exactly match the current, visible menu; an empty current id (no
// menu) or a mismatch is rejected.
inline bool IsCurrentMenuId(const std::string& menuId,
                            const std::string& currentMenuId, bool visible) {
    return visible && !currentMenuId.empty() && menuId == currentMenuId;
}

}  // namespace menu_action
