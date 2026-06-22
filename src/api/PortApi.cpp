// PortApi.cpp - Port/Event/State API implementation
// Part of foo_ui_webview2 - foobar2000 WebView2 UI Plugin
#include "pch.h"
#include "PortApi.h"
#include "PortHub.h"
#include "BridgeCore.h"
#include "../core/WebViewContext.h"

// Helper: Get caller HWND from params (injected by BridgeCore)
static HWND GetCallerHwnd(const json& params) {
    if (params.contains("_callerHwnd")) {
        auto hwnd = reinterpret_cast<HWND>(params["_callerHwnd"].get<intptr_t>());
        if (hwnd && IsWindow(hwnd)) {
            // Get top-level window for panel mode
            HWND topLevel = ::GetAncestor(hwnd, GA_ROOT);
            return topLevel ? topLevel : hwnd;
        }
    }
    return nullptr;
}

// Helper: Get caller window ID using _callerHwnd + WebViewContext
static std::string GetCallerWindowId(const json& params) {
    HWND callerHwnd = GetCallerHwnd(params);
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

void RegisterPortApi() {
    auto& bridge = BridgeCore::GetInstance();
    auto& hub = PortHub::Instance();

    // ========================================================================
    // Port APIs
    // ========================================================================

    // port.connect - Create a named port
    bridge.RegisterApi("port.connect", [&hub](const json& params) -> json {
        std::string name = params.value("name", "");
        if (name.empty()) {
            return {{"error", "Port name is required"}, {"code", "INVALID_PARAMS"}};
        }
        std::string windowId = GetCallerWindowId(params);
        return hub.CreatePort(name, windowId);
    });

    // port.disconnect - Destroy a port
    bridge.RegisterApi("port.disconnect", [&hub](const json& params) -> json {
        std::string portId = params.value("portId", "");
        if (portId.empty()) {
            return {{"error", "Port ID is required"}, {"code", "INVALID_PARAMS"}};
        }
        return hub.DestroyPort(portId);
    });

    // port.postMessage - Send message to all ports with same name
    bridge.RegisterApi("port.postMessage", [&hub](const json& params) -> json {
        std::string portId = params.value("portId", "");
        if (portId.empty()) {
            return {{"success", false}, {"error", "Port ID is required"}, {"code", "INVALID_PARAMS"}};
        }
        // message is required per design doc
        if (!params.contains("message")) {
            return {{"success", false}, {"error", "Message is required"}, {"code", "INVALID_PARAMS"}};
        }
        const auto& message = params["message"];
        std::string sourceWindowId = GetCallerWindowId(params);
        return hub.PostMessage(portId, message, sourceWindowId);
    });

    // port.postMessageTo - Send message to a specific port
    bridge.RegisterApi("port.postMessageTo", [&hub](const json& params) -> json {
        std::string portId = params.value("portId", "");
        std::string targetPortId = params.value("targetPortId", "");
        if (portId.empty() || targetPortId.empty()) {
            return {{"success", false}, {"error", "Port ID and target port ID are required"}, {"code", "INVALID_PARAMS"}};
        }
        // message is required per design doc
        if (!params.contains("message")) {
            return {{"success", false}, {"error", "Message is required"}, {"code", "INVALID_PARAMS"}};
        }
        const auto& message = params["message"];
        std::string sourceWindowId = GetCallerWindowId(params);
        return hub.PostMessageTo(portId, targetPortId, message, sourceWindowId);
    });

    // port.getPorts - Get all ports (optionally filtered by name)
    bridge.RegisterApi("port.getPorts", [&hub](const json& params) -> json {
        std::optional<std::string> name;
        if (params.contains("name") && !params["name"].is_null()) {
            name = params["name"].get<std::string>();
        }
        return hub.GetPorts(name);
    });

    // ========================================================================
    // Event APIs
    // ========================================================================

    // event.emit - Broadcast event to all windows
    bridge.RegisterApi("event.emit", [&hub](const json& params) -> json {
        std::string event = params.value("event", "");
        if (event.empty()) {
            return {{"success", false}, {"error", "Event name is required"}, {"code", "INVALID_PARAMS"}};
        }
        json payload = params.value("payload", json::object());
        bool excludeSelf = params.value("excludeSelf", false);
        std::string sourceWindowId = GetCallerWindowId(params);
        return hub.EmitEvent(event, payload, sourceWindowId, excludeSelf);
    });

    // event.emitTo - Send event to a specific window
    bridge.RegisterApi("event.emitTo", [&hub](const json& params) -> json {
        std::string event = params.value("event", "");
        std::string targetWindowId = params.value("targetWindowId", "");
        if (event.empty() || targetWindowId.empty()) {
            return {{"success", false}, {"error", "Event name and target window ID are required"}, {"code", "INVALID_PARAMS"}};
        }
        json payload = params.value("payload", json::object());
        std::string sourceWindowId = GetCallerWindowId(params);
        return hub.EmitEventTo(event, payload, sourceWindowId, targetWindowId);
    });

    // ========================================================================
    // State APIs
    // ========================================================================

    // state.get - Get a value from shared state
    bridge.RegisterApi("state.get", [&hub](const json& params) -> json {
        std::string key = params.value("key", "");
        if (key.empty()) {
            return {{"error", "Key is required"}, {"code", "INVALID_PARAMS"}};
        }
        return hub.GetState(key);
    });

    // state.set - Set a value in shared state
    bridge.RegisterApi("state.set", [&hub](const json& params) -> json {
        std::string key = params.value("key", "");
        if (key.empty()) {
            return {{"success", false}, {"error", "Key is required"}, {"code", "INVALID_PARAMS"}};
        }
        // value is required per design doc
        if (!params.contains("value")) {
            return {{"success", false}, {"error", "Value is required"}, {"code", "INVALID_PARAMS"}};
        }
        const auto& value = params["value"];
        std::string sourceWindowId = GetCallerWindowId(params);
        bool silent = params.value("silent", false);
        std::optional<int64_t> ttlMs;
        if (params.contains("ttlMs") && !params["ttlMs"].is_null()) {
            ttlMs = params["ttlMs"].get<int64_t>();
        }
        return hub.SetState(key, value, sourceWindowId, silent, ttlMs);
    });

    // state.delete - Delete a value from shared state
    bridge.RegisterApi("state.delete", [&hub](const json& params) -> json {
        std::string key = params.value("key", "");
        if (key.empty()) {
            return {{"success", false}, {"error", "Key is required"}, {"code", "INVALID_PARAMS"}};
        }
        std::string sourceWindowId = GetCallerWindowId(params);
        return hub.DeleteState(key, sourceWindowId);
    });

    // state.keys - Get all keys in shared state (supports * wildcard pattern)
    bridge.RegisterApi("state.keys", [&hub](const json& params) -> json {
        std::string pattern = params.value("pattern", "*");
        return hub.GetStateKeys(pattern);
    });
}
