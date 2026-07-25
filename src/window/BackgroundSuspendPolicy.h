#pragma once

namespace background_suspend_policy {

inline constexpr unsigned kLocked     = 1u << 0;
inline constexpr unsigned kCovered    = 1u << 1;
inline constexpr unsigned kMinimized  = 1u << 2;
inline constexpr unsigned kTrayHidden = 1u << 3;

struct Projection {
    bool hideSurface;
    bool useLowMemory;
    // Deep suspend (TrySuspend): freeze renderer timers/animations so the OS
    // can reclaim renderer memory. Only meaningful after put_IsVisible(FALSE),
    // hence always equal to hideSurface (keep-alive veto folded in). The
    // advconfig kill-switch degrades application back to the Low path; that is
    // an apply-site concern, not a projection concern.
    bool deepSuspend;
};

struct VisibilityApplyResult {
    bool pageHidden;
    bool retryResume;
};

enum class PageHealthRecoveryAction {
    Accept,
    Reload,
    Rebuild,
};

constexpr PageHealthRecoveryAction DecidePageHealthRecovery(
    bool healthy, unsigned reloadAttempts, unsigned reloadLimit) {
    if (healthy) return PageHealthRecoveryAction::Accept;
    return reloadAttempts >= reloadLimit
        ? PageHealthRecoveryAction::Rebuild
        : PageHealthRecoveryAction::Reload;
}

// A requested visibility projection is not applied until WebView2 confirms
// the controller state. In particular, a failed hidden-to-visible transition
// must retain pageHidden=true so a later lifecycle signal can retry it.
constexpr VisibilityApplyResult ApplyVisibilityResult(
    bool pageHidden, bool desiredHidden, bool commandSucceeded) {
    if (pageHidden == desiredHidden) {
        return {pageHidden, false};
    }
    if (commandSucceeded) {
        return {desiredHidden, false};
    }
    return {pageHidden, pageHidden && !desiredHidden};
}

// Suspending the page (visibilityState=hidden) breaks CDP automation
// consumers: screenshots need frames and hidden-page timers are intensively
// throttled. Automation keep-alive therefore vetoes every page-hiding path;
// memory-target trimming stays allowed because it does not stop frame output.
constexpr bool MayHidePage(bool automationKeepAlive) {
    return !automationKeepAlive;
}

constexpr Projection Project(unsigned reasons, bool automationKeepAlive) {
    // A covered foreground window is not the only possible consumer of the
    // DirectComposition surface: DWM may still need it for taskbar previews,
    // so kCovered alone never removes it. Session lock, minimize and tray
    // hiding have no visible consumer, so any of them may remove it.
    const bool hideSurface = MayHidePage(automationKeepAlive) &&
        (reasons & (kLocked | kMinimized | kTrayHidden)) != 0;
    return {
        hideSurface,
        // Manual Low is exclusive to paths that keep the page visible
        // (covered-only or keep-alive veto). Deep-suspend paths let
        // TrySuspend manage the target (auto Low on suspend, auto Normal on
        // resume); the advconfig kill-switch is handled at the apply site.
        reasons != 0 && !hideSurface,
        hideSurface,
    };
}

constexpr Projection Project(unsigned reasons) {
    return Project(reasons, false);
}

}  // namespace background_suspend_policy
