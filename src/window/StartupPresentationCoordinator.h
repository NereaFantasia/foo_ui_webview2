#pragma once

#include "pch.h"

struct StartupPresentationCoordinatorDecision {
    bool armFallbackTimer = false;
    bool cancelFallbackTimer = false;
    bool commitReveal = false;
    bool fallbackUsed = false;
};

struct StartupPresentationCoordinatorSnapshot {
    std::string phase = "waiting-navigation";
    bool navigationCompleted = false;
    bool windowReadySignaled = false;
    bool visualReadySignaled = false;
    bool revealPending = true;
    bool revealCommitted = false;
    bool revealSettling = false;
    bool fallbackArmed = false;
    bool fallbackUsed = false;
};

class StartupPresentationCoordinator {
public:
    StartupPresentationCoordinator();

    void Reset();

    StartupPresentationCoordinatorDecision OnNavigationCompleted(bool success, bool chromeReady);
    StartupPresentationCoordinatorDecision OnWindowReadySignal(const std::string& source, bool chromeReady);
    StartupPresentationCoordinatorDecision OnVisualReadySignal(const std::string& source, bool chromeReady);
    StartupPresentationCoordinatorDecision OnChromeReady();
    StartupPresentationCoordinatorDecision OnFallbackTimerFired();
    StartupPresentationCoordinatorDecision ForceFallbackReveal();

    void MarkRevealStarted(bool fallbackUsed);
    void MarkRevealSettled();
    void MarkClosed();

    StartupPresentationCoordinatorSnapshot GetSnapshot() const;

private:
    static bool ShouldIgnoreAutomaticWindowReadySource(const std::string& source);
    StartupPresentationCoordinatorDecision BuildCommitDecision(bool fallbackUsed);
    bool HasReadyAuthoritySignal() const;
    std::string GetPhase() const;

    bool navigationCompleted_ = false;
    bool windowReadySignaled_ = false;
    bool visualReadySignaled_ = false;
    bool revealPending_ = true;
    bool revealCommitted_ = false;
    bool revealSettling_ = false;
    bool fallbackArmed_ = false;
    bool fallbackUsed_ = false;
};
