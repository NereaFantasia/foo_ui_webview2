#include "pch.h"
#include "window/StartupPresentationCoordinator.h"

StartupPresentationCoordinator::StartupPresentationCoordinator() {
    Reset();
}

void StartupPresentationCoordinator::Reset() {
    navigationCompleted_ = false;
    windowReadySignaled_ = false;
    visualReadySignaled_ = false;
    revealPending_ = true;
    revealCommitted_ = false;
    revealSettling_ = false;
    fallbackArmed_ = false;
    fallbackUsed_ = false;
}

StartupPresentationCoordinatorDecision StartupPresentationCoordinator::OnNavigationCompleted(bool success,
                                                                                              bool chromeReady) {
    StartupPresentationCoordinatorDecision decision;
    if (!revealPending_ || revealCommitted_) {
        return decision;
    }

    navigationCompleted_ = true;

    if (!success) {
        return BuildCommitDecision(true);
    }

    if (HasReadyAuthoritySignal() && chromeReady) {
        return BuildCommitDecision(false);
    }

    if (!fallbackArmed_) {
        fallbackArmed_ = true;
        decision.armFallbackTimer = true;
    }
    return decision;
}

StartupPresentationCoordinatorDecision StartupPresentationCoordinator::OnWindowReadySignal(const std::string& source,
                                                                                            bool chromeReady) {
    StartupPresentationCoordinatorDecision decision;
    if (!revealPending_ || revealCommitted_) {
        return decision;
    }

    if (ShouldIgnoreAutomaticWindowReadySource(source)) {
        return decision;
    }

    windowReadySignaled_ = true;
    // windowReady is a technical-ready signal, no longer cold startup reveal authority.
    // Reveal is decided by visualReady (overlay-gone), or fallback timer.
    (void)chromeReady;
    return decision;
}

StartupPresentationCoordinatorDecision StartupPresentationCoordinator::OnVisualReadySignal(const std::string& source,
                                                                                            bool chromeReady) {
    StartupPresentationCoordinatorDecision decision;
    if (!revealPending_ || revealCommitted_) {
        return decision;
    }

    (void)source;
    visualReadySignaled_ = true;
    if (navigationCompleted_ && chromeReady) {
        return BuildCommitDecision(false);
    }
    return decision;
}

StartupPresentationCoordinatorDecision StartupPresentationCoordinator::OnChromeReady() {
    StartupPresentationCoordinatorDecision decision;
    if (!revealPending_ || revealCommitted_ || !navigationCompleted_) {
        return decision;
    }

    if (!HasReadyAuthoritySignal()) {
        return decision;
    }

    return BuildCommitDecision(false);
}

StartupPresentationCoordinatorDecision StartupPresentationCoordinator::OnFallbackTimerFired() {
    StartupPresentationCoordinatorDecision decision;
    if (!revealPending_ || revealCommitted_ || !navigationCompleted_) {
        return decision;
    }

    return BuildCommitDecision(true);
}

StartupPresentationCoordinatorDecision StartupPresentationCoordinator::ForceFallbackReveal() {
    StartupPresentationCoordinatorDecision decision;
    if (!revealPending_ || revealCommitted_) {
        return decision;
    }

    return BuildCommitDecision(true);
}

void StartupPresentationCoordinator::MarkRevealStarted(bool fallbackUsed) {
    revealCommitted_ = true;
    revealPending_ = false;
    revealSettling_ = true;
    fallbackArmed_ = false;
    fallbackUsed_ = fallbackUsed;
}

void StartupPresentationCoordinator::MarkRevealSettled() {
    revealSettling_ = false;
}

void StartupPresentationCoordinator::MarkClosed() {
    navigationCompleted_ = false;
    windowReadySignaled_ = false;
    visualReadySignaled_ = false;
    revealPending_ = false;
    revealCommitted_ = false;
    revealSettling_ = false;
    fallbackArmed_ = false;
    fallbackUsed_ = false;
}

StartupPresentationCoordinatorSnapshot StartupPresentationCoordinator::GetSnapshot() const {
    StartupPresentationCoordinatorSnapshot snapshot;
    snapshot.phase = GetPhase();
    snapshot.navigationCompleted = navigationCompleted_;
    snapshot.windowReadySignaled = windowReadySignaled_;
    snapshot.visualReadySignaled = visualReadySignaled_;
    snapshot.revealPending = revealPending_;
    snapshot.revealCommitted = revealCommitted_;
    snapshot.revealSettling = revealSettling_;
    snapshot.fallbackArmed = fallbackArmed_;
    snapshot.fallbackUsed = fallbackUsed_;
    return snapshot;
}

bool StartupPresentationCoordinator::ShouldIgnoreAutomaticWindowReadySource(const std::string& source) {
    return source == "auto-dom-content-loaded" ||
        source == "auto-ready-state" ||
        source == "auto-ready-state-interactive";
}

StartupPresentationCoordinatorDecision StartupPresentationCoordinator::BuildCommitDecision(bool fallbackUsed) {
    StartupPresentationCoordinatorDecision decision;
    if (!revealPending_ || revealCommitted_) {
        return decision;
    }

    decision.commitReveal = true;
    decision.cancelFallbackTimer = fallbackArmed_;
    decision.fallbackUsed = fallbackUsed;
    fallbackArmed_ = false;
    fallbackUsed_ = fallbackUsed;
    return decision;
}

bool StartupPresentationCoordinator::HasReadyAuthoritySignal() const {
    // Only visualReady (overlay-gone) is reveal authority.
    // windowReady records technical-ready state only, does not participate in commit.
    return visualReadySignaled_;
}

std::string StartupPresentationCoordinator::GetPhase() const {
    if (revealCommitted_) {
        return fallbackUsed_ ? "fallback" : "revealed";
    }
    if (!revealPending_) {
        return "hidden";
    }
    if (navigationCompleted_) {
        return "waiting-ready";
    }
    return "waiting-navigation";
}
