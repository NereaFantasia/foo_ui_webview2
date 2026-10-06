// PortApi.cpp - Port/Event/State API implementation
// Part of foo_ui_webview2 - foobar2000 WebView2 UI Plugin
#include "pch.h"
#include "PortApi.h"
#include "PortHub.h"
#include "TypedApi.h"
#include "../core/WebViewContext.h"

namespace {

namespace pt = api::port;
namespace ev = api::event;
namespace st = api::state;

// Top-level window of the caller (panel mode reports a child window).
HWND GetCallerHwnd(HWND callerHwnd) {
    if (callerHwnd && IsWindow(callerHwnd)) {
        HWND topLevel = ::GetAncestor(callerHwnd, GA_ROOT);
        return topLevel ? topLevel : callerHwnd;
    }
    return nullptr;
}

// Id of the caller's window; ports, events and state changes are attributed
// to it. Falls back to "main" when the caller cannot be matched to a window.
std::string GetCallerWindowId(const CallerContext& caller) {
    HWND callerHwnd = GetCallerHwnd(caller.callerHwnd);
    if (!callerHwnd) {
        return "main";  // Fallback
    }

    auto& ctx = WebViewContext::GetInstance();

    // Try direct lookup
    std::string windowId = ctx.GetWindowIdByHwnd(callerHwnd);
    if (!windowId.empty()) {
        return windowId;
    }

    // Panel mode: callerHwnd is top-level, need to find matching instance
    for (auto instanceHwnd : ctx.GetAllInstances()) {
        if (instanceHwnd == callerHwnd ||
            ::GetAncestor(instanceHwnd, GA_ROOT) == callerHwnd) {
            std::string wid = ctx.GetWindowIdByHwnd(instanceHwnd);
            if (!wid.empty()) return wid;
        }
    }

    return "main";  // Final fallback
}

// A payload left out (or null) is delivered as an empty object.
json PayloadOrEmpty(const std::optional<json>& payload) {
    return payload ? *payload : json::object();
}

// ============================================================================
// Port APIs
// ============================================================================

api::Result<pt::ConnectResult> PortConnect(const pt::ConnectParams& p, const CallerContext& caller) {
    // callerHwnd is the calling page's own handle (WebViewPanel::hwnd_), so the
    // port closes when that page navigates away or its WebView goes away.
    return PortHub::Instance().CreatePort(p.name, GetCallerWindowId(caller), caller.callerHwnd);
}

// Only the window that opened a port may close it; PortHub checks the caller's
// window id against the port's owner.
api::Result<void> PortDisconnect(const pt::DisconnectParams& p, const CallerContext& caller) {
    return PortHub::Instance().DestroyPort(p.portId, GetCallerWindowId(caller));
}

api::Result<pt::PostMessageResult> PortPostMessage(const pt::PostMessageParams& p, const CallerContext& caller) {
    return PortHub::Instance().PostMessage(p.portId, p.message, GetCallerWindowId(caller));
}

api::Result<void> PortPostMessageTo(const pt::PostMessageToParams& p, const CallerContext& caller) {
    return PortHub::Instance().PostMessageTo(p.portId, p.targetPortId, p.message, GetCallerWindowId(caller));
}

api::Result<pt::GetPortsResult> PortGetPorts(const pt::GetPortsParams& p) {
    return PortHub::Instance().GetPorts(p.name);
}

// ============================================================================
// Event APIs
// ============================================================================

api::Result<ev::EmitResult> EventEmit(const ev::EmitParams& p, const CallerContext& caller) {
    return PortHub::Instance().EmitEvent(p.event, PayloadOrEmpty(p.payload), GetCallerWindowId(caller),
                                         p.excludeSelf);
}

api::Result<void> EventEmitTo(const ev::EmitToParams& p, const CallerContext& caller) {
    return PortHub::Instance().EmitEventTo(p.event, PayloadOrEmpty(p.payload), GetCallerWindowId(caller),
                                           p.targetWindowId);
}

// ============================================================================
// State APIs
// ============================================================================

api::Result<st::GetResult> StateGet(const st::GetParams& p) {
    return PortHub::Instance().GetState(p.key);
}

api::Result<st::SetResult> StateSet(const st::SetParams& p, const CallerContext& caller) {
    return PortHub::Instance().SetState(p.key, p.value, GetCallerWindowId(caller), p.silent, p.ttlMs);
}

api::Result<st::DeleteResult> StateDelete(const st::DeleteParams& p, const CallerContext& caller) {
    return PortHub::Instance().DeleteState(p.key, GetCallerWindowId(caller));
}

api::Result<st::KeysResult> StateKeys(const st::KeysParams& p) {
    return PortHub::Instance().GetStateKeys(p.pattern);
}

}  // namespace

void RegisterPortApi() {
    // Parameters and results come from src/api/schema/{port,event,state}.ts
    // through the generated types.
    api::RegisterApi("port.connect", PortConnect);
    api::RegisterApi("port.disconnect", PortDisconnect);
    api::RegisterApi("port.postMessage", PortPostMessage);
    api::RegisterApi("port.postMessageTo", PortPostMessageTo);
    api::RegisterApi("port.getPorts", PortGetPorts);

    api::RegisterApi("event.emit", EventEmit);
    api::RegisterApi("event.emitTo", EventEmitTo);

    api::RegisterApi("state.get", StateGet);
    api::RegisterApi("state.set", StateSet);
    api::RegisterApi("state.delete", StateDelete);
    api::RegisterApi("state.keys", StateKeys);
}
