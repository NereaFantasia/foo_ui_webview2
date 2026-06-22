// PortHub.cpp - Cross-window communication hub implementation
// Part of foo_ui_webview2 - foobar2000 WebView2 UI Plugin
#include "pch.h"
#include "PortHub.h"
#include "../core/WebViewContext.h"
#include <sstream>
#include <iomanip>

// ============================================================================
// Singleton Instance
// ============================================================================

PortHub& PortHub::Instance() {
    static PortHub instance;
    return instance;
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string PortHub::GeneratePortId() {
    std::ostringstream oss;
    oss << "port_" << std::hex << std::setfill('0') << std::setw(8) << (++m_portIdCounter);
    return oss.str();
}

void PortHub::CleanupExpiredStates() {
    auto now = std::chrono::system_clock::now();
    auto& ctx = WebViewContext::GetInstance();
    
    for (auto it = m_states.begin(); it != m_states.end(); ) {
        if (it->second.expiresAt && *it->second.expiresAt <= now) {
            // Broadcast state:deleted event before removing (per design doc)
            json payload = {
                {"key", it->first},
                {"sourceWindowId", ""},  // No source for TTL expiration
                {"reason", "expired"}
            };
            ctx.BroadcastEvent("state:deleted", payload);
            it = m_states.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Port API Implementation
// ============================================================================

json PortHub::CreatePort(const std::string& name, const std::string& windowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string portId = GeneratePortId();

    // Create port info
    PortInfo info{portId, name, windowId};
    m_ports[portId] = info;

    // Update indices
    m_portsByName[name].insert(portId);
    m_portsByWindow[windowId].insert(portId);

    // Broadcast port:connected event to all windows
    json eventPayload = {
        {"portId", portId},
        {"name", name},
        {"windowId", windowId}
    };
    WebViewContext::GetInstance().BroadcastEvent("port:connected", eventPayload);

    return {
        {"portId", portId},
        {"name", name},
        {"windowId", windowId}
    };
}

json PortHub::DestroyPort(const std::string& portId, const std::string& callerWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_ports.find(portId);
    if (it == m_ports.end()) {
        return {{"success", false}, {"error", "Port not found"}, {"code", "PORT_NOT_FOUND"}};
    }

    const auto& info = it->second;

    // Ownership check: only the window that created the port can disconnect it
    if (!callerWindowId.empty() && info.windowId != callerWindowId) {
        return {{"success", false}, {"error", "Permission denied: port belongs to another window"}, {"code", "PERMISSION_DENIED"}};
    }

    std::string name = info.name;
    std::string windowId = info.windowId;

    // Remove from indices
    m_portsByName[name].erase(portId);
    if (m_portsByName[name].empty()) {
        m_portsByName.erase(name);
    }

    m_portsByWindow[windowId].erase(portId);
    if (m_portsByWindow[windowId].empty()) {
        m_portsByWindow.erase(windowId);
    }

    // Remove port
    m_ports.erase(it);

    // Broadcast port:disconnected event
    json eventPayload = {
        {"portId", portId},
        {"name", name},
        {"windowId", windowId}
    };
    WebViewContext::GetInstance().BroadcastEvent("port:disconnected", eventPayload);

    return {{"success", true}};
}

json PortHub::PostMessage(const std::string& portId, const json& message, const std::string& sourceWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_ports.find(portId);
    if (it == m_ports.end()) {
        return {{"success", false}, {"error", "Port not found"}, {"code", "PORT_NOT_FOUND"}};
    }

    const auto& senderInfo = it->second;

    // Verify sender owns this port
    if (!sourceWindowId.empty() && senderInfo.windowId != sourceWindowId) {
        return {{"success", false}, {"error", "Port does not belong to caller window"}, {"code", "PERMISSION_DENIED"}};
    }
    const std::string& channelName = senderInfo.name;

    // Find all ports with the same name
    auto nameIt = m_portsByName.find(channelName);
    if (nameIt == m_portsByName.end()) {
        return {{"success", true}, {"recipients", 0}};
    }

    auto& ctx = WebViewContext::GetInstance();
    int delivered = 0;
    
    for (const auto& targetPortId : nameIt->second) {
        if (targetPortId == portId) continue; // Skip sender

        auto targetIt = m_ports.find(targetPortId);
        if (targetIt == m_ports.end()) continue;

        const auto& targetInfo = targetIt->second;

        // Send port:message event with correct structure per design doc
        json eventPayload = {
            {"portId", targetPortId},
            {"sourcePortId", portId},
            {"sourceWindowId", sourceWindowId},
            {"message", message}
        };
        
        if (ctx.SendEventTo(targetInfo.windowId, "port:message", eventPayload)) {
            delivered++;
        }
    }

    return {{"success", true}, {"recipients", delivered}};
}

json PortHub::PostMessageTo(const std::string& portId, const std::string& targetPortId, 
                            const json& message, const std::string& sourceWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Verify sender port exists
    auto senderIt = m_ports.find(portId);
    if (senderIt == m_ports.end()) {
        return {{"success", false}, {"error", "Sender port not found"}, {"code", "PORT_NOT_FOUND"}};
    }

    // Verify sender owns this port
    if (!sourceWindowId.empty() && senderIt->second.windowId != sourceWindowId) {
        return {{"success", false}, {"error", "Port does not belong to caller window"}, {"code", "PERMISSION_DENIED"}};
    }

    // Verify target port exists
    auto targetIt = m_ports.find(targetPortId);
    if (targetIt == m_ports.end()) {
        return {{"success", false}, {"error", "Target port not found"}, {"code", "TARGET_NOT_FOUND"}};
    }

    const auto& targetInfo = targetIt->second;

    // Send port:message event with correct structure per design doc
    json eventPayload = {
        {"portId", targetPortId},
        {"sourcePortId", portId},
        {"sourceWindowId", sourceWindowId},
        {"message", message}
    };
    
    bool sent = WebViewContext::GetInstance().SendEventTo(targetInfo.windowId, "port:message", eventPayload);
    return {{"success", sent}};
}

json PortHub::GetPorts(const std::optional<std::string>& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    json ports = json::array();

    if (name) {
        // Filter by name
        auto it = m_portsByName.find(*name);
        if (it != m_portsByName.end()) {
            for (const auto& portId : it->second) {
                auto portIt = m_ports.find(portId);
                if (portIt != m_ports.end()) {
                    ports.push_back({
                        {"portId", portIt->second.portId},
                        {"name", portIt->second.name},
                        {"windowId", portIt->second.windowId}
                    });
                }
            }
        }
    } else {
        // Return all ports
        for (const auto& [portId, info] : m_ports) {
            ports.push_back({
                {"portId", info.portId},
                {"name", info.name},
                {"windowId", info.windowId}
            });
        }
    }

    return {{"success", true}, {"ports", ports}};
}

void PortHub::CleanupWindowPorts(const std::string& windowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_portsByWindow.find(windowId);
    if (it == m_portsByWindow.end()) return;

    // Copy port IDs to avoid iterator invalidation
    std::vector<std::string> portsToRemove(it->second.begin(), it->second.end());

    auto& ctx = WebViewContext::GetInstance();
    
    for (const auto& portId : portsToRemove) {
        auto portIt = m_ports.find(portId);
        if (portIt == m_ports.end()) continue;

        const auto& info = portIt->second;

        // Remove from name index
        m_portsByName[info.name].erase(portId);
        if (m_portsByName[info.name].empty()) {
            m_portsByName.erase(info.name);
        }

        // Broadcast port:disconnected event
        json eventPayload = {
            {"portId", portId},
            {"name", info.name},
            {"windowId", windowId}
        };
        ctx.BroadcastEvent("port:disconnected", eventPayload);

        // Remove port
        m_ports.erase(portIt);
    }

    // Remove window from index
    m_portsByWindow.erase(windowId);
}

// ============================================================================
// Event API Implementation
// ============================================================================

json PortHub::EmitEvent(const std::string& event, const json& payload, 
                        const std::string& sourceWindowId, bool excludeSelf) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Build event envelope per design doc: { payload, sourceWindowId }
    json envelope = {
        {"payload", payload},
        {"sourceWindowId", sourceWindowId}
    };

    auto& ctx = WebViewContext::GetInstance();
    int delivered = 0;
    
    if (excludeSelf && !sourceWindowId.empty()) {
        // Get HWND for source window to exclude
        HWND excludeHwnd = ctx.GetHwndByWindowId(sourceWindowId);
        if (excludeHwnd) {
            // Broadcast to caller's event name directly (not event:broadcast)
            ctx.BroadcastEventExcept(event, envelope, excludeHwnd);
            delivered = static_cast<int>(ctx.GetInstanceCount()) - 1;
        } else {
            // Fallback to full broadcast if window not found
            ctx.BroadcastEvent(event, envelope);
            delivered = static_cast<int>(ctx.GetInstanceCount());
        }
    } else {
        // Broadcast to caller's event name directly
        ctx.BroadcastEvent(event, envelope);
        delivered = static_cast<int>(ctx.GetInstanceCount());
    }

    return {{"success", true}, {"recipients", delivered}};
}

json PortHub::EmitEventTo(const std::string& event, const json& payload,
                          const std::string& sourceWindowId, const std::string& targetWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Build event envelope per design doc: { payload, sourceWindowId }
    json envelope = {
        {"payload", payload},
        {"sourceWindowId", sourceWindowId}
    };

    // Send to caller's event name directly (not event:broadcast)
    bool sent = WebViewContext::GetInstance().SendEventTo(targetWindowId, event, envelope);
    return {{"success", sent}};
}

// ============================================================================
// State API Implementation
// ============================================================================

json PortHub::GetState(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clean up expired entries first
    CleanupExpiredStates();

    auto it = m_states.find(key);
    if (it == m_states.end()) {
        return {{"success", true}, {"value", nullptr}, {"exists", false}};
    }

    json result = {{"success", true}, {"key", key}, {"value", it->second.value}, {"exists", true}};
    if (it->second.expiresAt) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            it->second.expiresAt->time_since_epoch()
        ).count();
        result["expiresAt"] = ms;
    }

    return result;
}

json PortHub::SetState(const std::string& key, const json& value, 
                       const std::string& sourceWindowId,
                       bool silent,
                       std::optional<int64_t> ttlMs) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Get previous value for event
    json previousValue = nullptr;
    auto existingIt = m_states.find(key);
    if (existingIt != m_states.end()) {
        previousValue = existingIt->second.value;
    }

    StateEntry entry;
    entry.value = value;

    json result = {{"success", true}};

    if (ttlMs && *ttlMs > 0) {
        entry.expiresAt = std::chrono::system_clock::now() + std::chrono::milliseconds(*ttlMs);
        // Return expiresAt as epoch milliseconds
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            entry.expiresAt->time_since_epoch()
        ).count();
        result["expiresAt"] = ms;
    }

    m_states[key] = std::move(entry);

    // Broadcast state:changed event (unless silent)
    if (!silent) {
        json eventPayload = {
            {"key", key}, 
            {"value", value},
            {"previousValue", previousValue},
            {"sourceWindowId", sourceWindowId}
        };
        if (result.contains("expiresAt")) {
            eventPayload["expiresAt"] = result["expiresAt"];
        }
        WebViewContext::GetInstance().BroadcastEvent("state:changed", eventPayload);
    }

    return result;
}

json PortHub::DeleteState(const std::string& key, const std::string& sourceWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_states.find(key);
    bool existed = (it != m_states.end());

    if (existed) {
        m_states.erase(it);

        // Broadcast state:deleted event
        json eventPayload = {
            {"key", key},
            {"sourceWindowId", sourceWindowId},
            {"reason", "deleted"}
        };
        WebViewContext::GetInstance().BroadcastEvent("state:deleted", eventPayload);
    }

    // Always return success with existed flag per design doc
    return {{"success", true}, {"existed", existed}};
}

// Helper: Match key against pattern with * wildcard
static bool MatchPattern(const std::string& key, const std::string& pattern) {
    if (pattern == "*") return true;
    
    // Simple wildcard matching: only support trailing * (e.g., "lyrics:*")
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return key.compare(0, prefix.length(), prefix) == 0;
    }
    
    // Exact match
    return key == pattern;
}

json PortHub::GetStateKeys(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clean up expired entries first
    CleanupExpiredStates();

    json keys = json::array();
    for (const auto& [key, _] : m_states) {
        if (MatchPattern(key, pattern)) {
            keys.push_back(key);
        }
    }

    return {{"success", true}, {"keys", keys}};
}
