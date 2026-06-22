#pragma once

#include "pch.h"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class BridgeCore;

// ============================================
// CallerContext — API caller routing context
//
// Extracts the caller's bridge instance from handler params via _callerHwnd.
// Used to route events back to the originating WebView instance.
//
// Panel mode (DUI/CUI):
//   _callerHwnd is the WebViewPanel's hwnd_ (child window), matched via
//   WebViewContext to find the registered per-instance bridge.
//
// Standalone mode:
//   _callerHwnd is the top-level window, matched directly or via
//   fallback to the BridgeCore registry.
//
// Usage:
//   auto caller = CallerContext::FromParams(params);
//   caller.EmitEvent("audio:spectrum", data);
// ============================================
struct CallerContext {
    HWND callerHwnd = nullptr;
    std::string windowId;
    BridgeCore* bridge = nullptr;

    // Build CallerContext from API params
    static CallerContext FromParams(const json& params);

    // Whether the caller has a routable bridge
    bool IsValid() const { return bridge != nullptr; }

    // Emit event to the caller's instance
    // Falls back to BridgeCore broadcast if bridge is invalid
    void EmitEvent(const std::string& event, const json& data) const;

    // Try emitting event to the caller's instance (no fallback)
    // Returns true if successfully dispatched
    bool TryEmitEvent(const std::string& event, const json& data) const;
};
