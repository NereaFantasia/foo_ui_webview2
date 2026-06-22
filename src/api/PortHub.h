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

using json = nlohmann::json;

// ============================================================================
// PortHub - Singleton for cross-window communication
// ============================================================================
// Provides three communication mechanisms:
// 1. Port - Named channels for targeted messaging between windows
// 2. Event - Global event broadcasting across all windows
// 3. State - Shared key-value storage with optional TTL
// ============================================================================

class PortHub {
public:
    // Singleton access
    static PortHub& Instance();

    // ========================================================================
    // Port API - Named communication channels
    // ========================================================================

    // Create a named port for the given window
    // Returns: { portId, name, windowId }
    json CreatePort(const std::string& name, const std::string& windowId);

    // Destroy a port by ID (callerWindowId for ownership check; empty = skip check)
    // Returns: { success: true } or { success: false, error, code }
    json DestroyPort(const std::string& portId, const std::string& callerWindowId = "");

    // Send message to all ports with the same name (excluding sender)
    // sourceWindowId: the window that initiated the message
    // Returns: { success: true/false, recipients: count }
    json PostMessage(const std::string& portId, const json& message, const std::string& sourceWindowId);

    // Send message to a specific port
    // sourceWindowId: the window that initiated the message
    // Returns: { success: true/false }
    json PostMessageTo(const std::string& portId, const std::string& targetPortId, 
                       const json& message, const std::string& sourceWindowId);

    // Get all ports (optionally filtered by name)
    // Returns: { success: true, ports: [...] }
    json GetPorts(const std::optional<std::string>& name = std::nullopt);

    // Clean up all ports for a window (called when window closes)
    void CleanupWindowPorts(const std::string& windowId);

    // ========================================================================
    // Event API - Global event broadcasting
    // ========================================================================

    // Emit event to all windows (optionally excluding sender)
    // sourceWindowId: the window that initiated the event
    // excludeSelf: if true, don't send to sourceWindowId
    // Returns: { success: true, recipients: count }
    json EmitEvent(const std::string& event, const json& payload, 
                   const std::string& sourceWindowId, bool excludeSelf = false);

    // Emit event to a specific window
    // sourceWindowId: the window that initiated the event
    // Returns: { success: true/false }
    json EmitEventTo(const std::string& event, const json& payload,
                     const std::string& sourceWindowId, const std::string& targetWindowId);

    // ========================================================================
    // State API - Shared key-value storage with TTL
    // ========================================================================

    // Get a value from shared state
    // Returns: { success: true, value, exists } -- value is null if not found
    json GetState(const std::string& key);

    // Set a value in shared state (with optional TTL in milliseconds)
    // silent: if true, don't broadcast state:changed event
    // sourceWindowId: the window that initiated the change
    // Returns: { success: true, key, expiresAt? }
    json SetState(const std::string& key, const json& value, 
                  const std::string& sourceWindowId,
                  bool silent = false,
                  std::optional<int64_t> ttlMs = std::nullopt);

    // Delete a value from shared state
    // sourceWindowId: the window that initiated the deletion
    // Returns: { success: true, existed: bool }
    json DeleteState(const std::string& key, const std::string& sourceWindowId);

    // Get all keys in shared state (supports * wildcard pattern)
    // Returns: { success: true, keys: [...] }
    json GetStateKeys(const std::string& pattern = "*");

private:
    PortHub() = default;
    ~PortHub() = default;
    PortHub(const PortHub&) = delete;
    PortHub& operator=(const PortHub&) = delete;

    // Port information structure
    struct PortInfo {
        std::string portId;
        std::string name;
        std::string windowId;
    };

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

    // Port storage: portId -> PortInfo
    std::unordered_map<std::string, PortInfo> m_ports;

    // Port name index: name -> set of portIds
    std::unordered_map<std::string, std::unordered_set<std::string>> m_portsByName;

    // Window port index: windowId -> set of portIds
    std::unordered_map<std::string, std::unordered_set<std::string>> m_portsByWindow;

    // Shared state storage: key -> StateEntry
    std::unordered_map<std::string, StateEntry> m_states;

    // Port ID counter
    uint64_t m_portIdCounter = 0;
};
