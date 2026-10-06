// PortHub.cpp - Cross-window communication hub implementation
// Part of foo_ui_webview2 - foobar2000 WebView2 UI Plugin
#include "pch.h"
#include "PortHub.h"
#include "ErrorEnvelope.h"
#include "api/EventEmit.h"
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

    for (auto it = m_states.begin(); it != m_states.end(); ) {
        if (it->second.expiresAt && *it->second.expiresAt <= now) {
            // Broadcast state:deleted event before removing (per design doc)
            api::state::DeletedPayload payload;
            payload.key = it->first;
            payload.sourceWindowId = "";  // No source for TTL expiration
            payload.reason = "expired";
            api::emit::Broadcast<api::state::events::Deleted>(payload);
            it = m_states.erase(it);
        } else {
            ++it;
        }
    }
}

static void AnnounceDisconnected(const std::string& portId, const std::string& name,
                                 const std::string& windowId) {
    api::port::DisconnectedPayload payload;
    payload.portId = portId;
    payload.name = name;
    payload.windowId = windowId;
    api::emit::Broadcast<api::port::events::Disconnected>(payload);
}

static api::port::MessagePayload MessageTo(const std::string& targetPortId, const std::string& sourcePortId,
                                           const std::string& sourceWindowId, const json& message) {
    api::port::MessagePayload payload;
    payload.portId = targetPortId;
    payload.sourcePortId = sourcePortId;
    payload.sourceWindowId = sourceWindowId;
    payload.message = message;
    return payload;
}

// Epoch milliseconds, the form expiry times are reported in.
static std::int64_t EpochMs(std::chrono::system_clock::time_point time) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();
}

// ============================================================================
// Port API Implementation
// ============================================================================

api::port::ConnectResult PortHub::CreatePort(const std::string& name, const std::string& windowId,
                                             HWND page) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string portId = GeneratePortId();
    m_ports.Add({portId, name, windowId, page});

    api::port::ConnectedPayload event;
    event.portId = portId;
    event.name = name;
    event.windowId = windowId;
    api::emit::Broadcast<api::port::events::Connected>(event);

    api::port::ConnectResult result;
    result.portId = std::move(portId);
    result.name = name;
    result.windowId = windowId;
    return result;
}

api::Result<void> PortHub::DestroyPort(const std::string& portId, const std::string& callerWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto* info = m_ports.Find(portId);
    if (!info) {
        return api::Fail("Port not found", ApiErrorCode::PORT_NOT_FOUND);
    }

    // Ownership check: only the window that created the port can disconnect it
    if (!callerWindowId.empty() && info->windowId != callerWindowId) {
        return api::Fail("Permission denied: port belongs to another window", ApiErrorCode::PERMISSION_DENIED);
    }

    if (const auto removed = m_ports.Remove(portId)) {
        AnnounceDisconnected(removed->portId, removed->name, removed->windowId);
    }

    return api::Ok();
}

api::Result<api::port::PostMessageResult> PortHub::PostMessage(const std::string& portId, const json& message,
                                                               const std::string& sourceWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto* senderInfo = m_ports.Find(portId);
    if (!senderInfo) {
        return api::Fail("Port not found", ApiErrorCode::PORT_NOT_FOUND);
    }

    // Verify sender owns this port
    if (!sourceWindowId.empty() && senderInfo->windowId != sourceWindowId) {
        return api::Fail("Port does not belong to caller window", ApiErrorCode::PERMISSION_DENIED);
    }

    api::port::PostMessageResult result;

    // Every other port with the same name
    for (const auto* target : m_ports.ChannelPorts(senderInfo->name)) {
        if (target->portId == portId) continue; // Skip sender

        if (api::emit::SendTo<api::port::events::Message>(target->windowId,
                                                          MessageTo(target->portId, portId, sourceWindowId, message))) {
            result.recipients++;
        }
    }

    return result;
}

api::Result<void> PortHub::PostMessageTo(const std::string& portId, const std::string& targetPortId,
                                         const json& message, const std::string& sourceWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Verify sender port exists
    const auto* senderInfo = m_ports.Find(portId);
    if (!senderInfo) {
        return api::Fail("Sender port not found", ApiErrorCode::PORT_NOT_FOUND);
    }

    // Verify sender owns this port
    if (!sourceWindowId.empty() && senderInfo->windowId != sourceWindowId) {
        return api::Fail("Port does not belong to caller window", ApiErrorCode::PERMISSION_DENIED);
    }

    // Verify target port exists
    const auto* targetInfo = m_ports.Find(targetPortId);
    if (!targetInfo) {
        return api::Fail("Target port not found", ApiErrorCode::TARGET_NOT_FOUND);
    }

    // The target port is known, so a failed send means its window is gone or
    // did not take the event.
    if (!api::emit::SendTo<api::port::events::Message>(targetInfo->windowId,
                                                       MessageTo(targetPortId, portId, sourceWindowId, message))) {
        return api::Fail("Target window did not receive the message", ApiErrorCode::OPERATION_FAILED,
                         {{"targetPortId", targetPortId}});
    }
    return api::Ok();
}

api::port::GetPortsResult PortHub::GetPorts(const std::optional<std::string>& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    api::port::GetPortsResult result;
    const auto add = [&result](const port_registry::Port& info) {
        api::port::PortInfo row;
        row.portId = info.portId;
        row.windowId = info.windowId;
        row.name = info.name;
        result.ports.push_back(std::move(row));
    };

    if (name) {
        // Filter by name
        for (const auto* info : m_ports.ChannelPorts(*name)) {
            add(*info);
        }
    } else {
        // Return all ports
        for (const auto& [portId, info] : m_ports.All()) {
            add(info);
        }
    }

    return result;
}

void PortHub::CleanupWindowPorts(const std::string& windowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& port : m_ports.RemoveWindow(windowId)) {
        AnnounceDisconnected(port.portId, port.name, port.windowId);
    }
}

void PortHub::CleanupPagePorts(HWND page) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& port : m_ports.RemovePage(page)) {
        AnnounceDisconnected(port.portId, port.name, port.windowId);
    }
}

// ============================================================================
// Event API Implementation
// ============================================================================

api::event::EmitResult PortHub::EmitEvent(const std::string& event, const json& payload,
                                          const std::string& sourceWindowId, bool excludeSelf) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Build event envelope per design doc: { payload, sourceWindowId }
    json envelope = {
        {"payload", payload},
        {"sourceWindowId", sourceWindowId}
    };

    auto& ctx = WebViewContext::GetInstance();
    api::event::EmitResult result;

    if (excludeSelf && !sourceWindowId.empty()) {
        // Get HWND for source window to exclude
        HWND excludeHwnd = ctx.GetHwndByWindowId(sourceWindowId);
        if (excludeHwnd) {
            // Broadcast to caller's event name directly (not event:broadcast)
            ctx.BroadcastEventExcept(event, envelope, excludeHwnd);
            result.recipients = static_cast<std::int64_t>(ctx.GetInstanceCount()) - 1;
        } else {
            // Fallback to full broadcast if window not found
            ctx.BroadcastEvent(event, envelope);
            result.recipients = static_cast<std::int64_t>(ctx.GetInstanceCount());
        }
    } else {
        // Broadcast to caller's event name directly
        ctx.BroadcastEvent(event, envelope);
        result.recipients = static_cast<std::int64_t>(ctx.GetInstanceCount());
    }

    return result;
}

api::Result<void> PortHub::EmitEventTo(const std::string& event, const json& payload,
                                       const std::string& sourceWindowId, const std::string& targetWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Build event envelope per design doc: { payload, sourceWindowId }
    json envelope = {
        {"payload", payload},
        {"sourceWindowId", sourceWindowId}
    };

    // Send to caller's event name directly (not event:broadcast). SendEventTo
    // fails when no open window has the id.
    if (!WebViewContext::GetInstance().SendEventTo(targetWindowId, event, envelope)) {
        return api::Fail("No window has this id", ApiErrorCode::NOT_FOUND, {{"targetWindowId", targetWindowId}});
    }
    return api::Ok();
}

// ============================================================================
// State API Implementation
// ============================================================================

api::state::GetResult PortHub::GetState(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clean up expired entries first
    CleanupExpiredStates();

    api::state::GetResult result;
    auto it = m_states.find(key);
    if (it == m_states.end()) {
        result.exists = false;
        return result;
    }

    result.exists = true;
    result.key = key;
    result.value = it->second.value;
    if (it->second.expiresAt) {
        result.expiresAt = EpochMs(*it->second.expiresAt);
    }

    return result;
}

api::state::SetResult PortHub::SetState(const std::string& key, const json& value,
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

    api::state::SetResult result;

    if (ttlMs && *ttlMs > 0) {
        entry.expiresAt = std::chrono::system_clock::now() + std::chrono::milliseconds(*ttlMs);
        result.expiresAt = EpochMs(*entry.expiresAt);
    }

    m_states[key] = std::move(entry);

    // Broadcast state:changed event (unless silent)
    if (!silent) {
        api::state::ChangedPayload payload;
        payload.key = key;
        payload.value = value;
        payload.previousValue = previousValue;
        payload.sourceWindowId = sourceWindowId;
        payload.expiresAt = result.expiresAt;
        api::emit::Broadcast<api::state::events::Changed>(payload);
    }

    return result;
}

api::state::DeleteResult PortHub::DeleteState(const std::string& key, const std::string& sourceWindowId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_states.find(key);
    bool existed = (it != m_states.end());

    if (existed) {
        m_states.erase(it);

        // Broadcast state:deleted event
        api::state::DeletedPayload payload;
        payload.key = key;
        payload.sourceWindowId = sourceWindowId;
        payload.reason = "deleted";
        api::emit::Broadcast<api::state::events::Deleted>(payload);
    }

    // Always succeed, reporting whether the key was there
    api::state::DeleteResult result;
    result.existed = existed;
    return result;
}

// Helper: Match key against pattern with * wildcard
static bool MatchPattern(const std::string& key, const std::string& pattern) {
    if (pattern == "*") return true;

    // Simple wildcard matching: only support trailing * (e.g., "lyrics:*")
    if (!pattern.empty() && pattern.back() == '*') {
        return key.starts_with(std::string_view(pattern).substr(0, pattern.length() - 1));
    }

    // Exact match
    return key == pattern;
}

api::state::KeysResult PortHub::GetStateKeys(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clean up expired entries first
    CleanupExpiredStates();

    api::state::KeysResult result;
    for (const auto& [key, _] : m_states) {
        if (MatchPattern(key, pattern)) {
            result.keys.push_back(key);
        }
    }

    return result;
}
