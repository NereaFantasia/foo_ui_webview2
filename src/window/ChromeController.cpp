#include "pch.h"
#include "window/ChromeController.h"

ResolvedWindowChromeState ChromeController::Resolve(const WindowChromeResolverSnapshot& snapshot) {
    return WindowChromeResolver::Resolve(snapshot);
}

WindowChromeApplyResult ChromeController::Apply(const WindowChromeApplyHooks& hooks,
                                                 const WindowChromeApplyRequest& request) {
    return WindowChromeApplier::Apply(hooks, request);
}

WindowChromeApplyResult ChromeController::ResolveAndApply(
    const WindowChromeResolverSnapshot& snapshot,
    const WindowChromeApplyHooks& hooks,
    bool forceRefresh,
    const std::string& previousMode,
    const std::string& previousEffect) {

    ResolvedWindowChromeState resolved = Resolve(snapshot);

    WindowChromeApplyRequest request;
    request.state = resolved;
    request.active = snapshot.derived.active;
    request.forceRefresh = forceRefresh;
    request.previousMode = previousMode;
    request.previousEffect = previousEffect;

    return Apply(hooks, request);
}

void ChromeController::RefreshNativeFrame(HWND hwnd) {
    WindowChromeApplier::RefreshNativeFrame(hwnd);
}
