// PortHub.h - Cross-window communication hub (Port/Event/State)
// Part of foo_ui_webview2 - foobar2000 WebView2 UI Plugin
#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <optional>
#include <chrono>
#include <nlohmann/json.hpp>

#include "api/ApiResult.h"
#include "api/PortRegistry.h"
#include "api/generated/EventSchema.h"
#include "api/generated/PortSchema.h"
#include "api/generated/StateSchema.h"

using json = nlohmann::json;

// ============================================================================
// PortHub - Singleton for cross-window communication
// ============================================================================
// Provides three communication mechanisms:
// 1. Port - Named channels for targeted messaging between windows
// 2. Event - Global event broadcasting across all windows
// 3. State - Shared key-value storage with optional TTL
//
// Results are the types declared in src/api/schema/{port,event,state}.ts, so
// the port.*, event.* and state.* handlers pass them straight through.
// ============================================================================

class PortHub {
public:
    // Singleton access
    static PortHub& Instance();

    // ========================================================================
    // Port API - Named communication channels
    // ========================================================================

    // Create a named port for the given window. page is the handle the opening
    // page's bridge messages arrive with (nullptr if unknown); CleanupPagePorts
    // closes the port with that page.
    api::port::ConnectResult CreatePort(const std::string& name, const std::string& windowId,
                                        HWND page = nullptr);

    // Destroy a port by ID (callerWindowId for ownership check; empty = skip check)
    api::Result<void> DestroyPort(const std::string& portId, const std::string& callerWindowId = "");

    // Send message to all ports with the same name (excluding sender)
    // sourceWindowId: the window that initiated the message
    api::Result<api::port::PostMessageResult> PostMessage(const std::string& portId, const json& message,
                                                          const std::string& sourceWindowId);

    // Send message to a specific port
    // sourceWindowId: the window that initiated the message
    api::Result<void> PostMessageTo(const std::string& portId, const std::string& targetPortId,
                                    const json& message, const std::string& sourceWindowId);

    // Get all ports (optionally filtered by name)
    api::port::GetPortsResult GetPorts(const std::optional<std::string>& name = std::nullopt);

    // Clean up all ports for a window (called when window closes)
    void CleanupWindowPorts(const std::string& windowId);

    // Close every port the page behind this handle opened, announcing each with
    // port:disconnected. Called when the page starts a top-level navigation and
    // when its WebView goes away.
    void CleanupPagePorts(HWND page);

    // ========================================================================
    // Event API - Global event broadcasting
    // ========================================================================

    // Emit event to all windows (optionally excluding sender)
    // sourceWindowId: the window that initiated the event
    // excludeSelf: if true, don't send to sourceWindowId
    api::event::EmitResult EmitEvent(const std::string& event, const json& payload,
                                     const std::string& sourceWindowId, bool excludeSelf = false);

    // Emit event to a specific window
    // sourceWindowId: the window that initiated the event
    api::Result<void> EmitEventTo(const std::string& event, const json& payload,
                                  const std::string& sourceWindowId, const std::string& targetWindowId);

    // ========================================================================
    // State API - Shared key-value storage with TTL
    // ========================================================================

    // Get a value from shared state; value is null if not found
    api::state::GetResult GetState(const std::string& key);

    // Set a value in shared state (with optional TTL in milliseconds)
    // silent: if true, don't broadcast state:changed event
    // sourceWindowId: the window that initiated the change
    api::state::SetResult SetState(const std::string& key, const json& value,
                                   const std::string& sourceWindowId,
                                   bool silent = false,
                                   std::optional<int64_t> ttlMs = std::nullopt);

    // Delete a value from shared state
    // sourceWindowId: the window that initiated the deletion
    api::state::DeleteResult DeleteState(const std::string& key, const std::string& sourceWindowId);

    // Get all keys in shared state (supports * wildcard pattern)
    api::state::KeysResult GetStateKeys(const std::string& pattern = "*");

private:
    PortHub() = default;
    ~PortHub() = default;
    PortHub(const PortHub&) = delete;
    PortHub& operator=(const PortHub&) = delete;

    // State entry with optional expiration
    struct StateEntry {
        json value;
        std::optional<std::chrono::system_clock::time_point> expiresAt;
    };

    // Generate unique port ID
    std::string GeneratePortId();

    // Clean up expired state entries
    void CleanupExpiredStates();

    // Thread safety
    mutable std::mutex m_mutex;

    // Open ports, indexed by id, channel name, window and opening page
    port_registry::PortRegistry m_ports;

    // Shared state storage: key -> StateEntry
    std::unordered_map<std::string, StateEntry> m_states;

    // Port ID counter
    uint64_t m_portIdCounter = 0;
};
