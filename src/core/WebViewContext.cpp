#include "pch.h"
#include "core/WebViewContext.h"
#include "api/BridgeCore.h"

WebViewContext& WebViewContext::GetInstance() {
    static WebViewContext instance;
    return instance;
}

void WebViewContext::RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Set as primary if this is the first instance
    if (instances_.empty()) {
        primaryHwnd_ = hwnd;
    }
    
    instances_[hwnd] = { host, bridge };
    
    FB2K_console_formatter() << "[WebViewContext] Registered instance: HWND=0x" 
        << pfc::format_hex((size_t)hwnd) << ", total=" << (unsigned)instances_.size();
}

void WebViewContext::RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge, const std::string& windowId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (instances_.empty()) {
        primaryHwnd_ = hwnd;
    }
    
    instances_[hwnd] = { host, bridge, windowId };
    
    FB2K_console_formatter() << "[WebViewContext] Registered instance: HWND=0x" 
        << pfc::format_hex((size_t)hwnd) << ", windowId=" << windowId.c_str() 
        << ", total=" << (unsigned)instances_.size();
}

void WebViewContext::RegisterInstance(HWND hwnd, WebViewHost* host, BridgeCore* bridge, const std::string& windowId, WebViewPanel* panel) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (instances_.empty()) {
        primaryHwnd_ = hwnd;
    }
    
    instances_[hwnd] = { host, bridge, windowId, panel };
    
    FB2K_console_formatter() << "[WebViewContext] Registered instance: HWND=0x" 
        << pfc::format_hex((size_t)hwnd) << ", windowId=" << windowId.c_str() 
        << ", panel=" << (panel ? "yes" : "no")
        << ", total=" << (unsigned)instances_.size();
}

void WebViewContext::UnregisterInstance(HWND hwnd) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    instances_.erase(hwnd);
    
    // Update primary if needed
    if (primaryHwnd_ == hwnd) {
        if (!instances_.empty()) {
            primaryHwnd_ = instances_.begin()->first;
        } else {
            primaryHwnd_ = nullptr;
        }
    }
    
    FB2K_console_formatter() << "[WebViewContext] Unregistered instance: HWND=0x" 
        << pfc::format_hex((size_t)hwnd) << ", remaining=" << (unsigned)instances_.size();
}

bool WebViewContext::HasInstance(HWND hwnd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instances_.count(hwnd) > 0;
}

size_t WebViewContext::GetInstanceCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instances_.size();
}

BridgeCore* WebViewContext::GetBridge(HWND hwnd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = instances_.find(hwnd);
    if (it != instances_.end()) {
        return it->second.bridge;
    }
    return nullptr;
}

WebViewHost* WebViewContext::GetWebViewHost(HWND hwnd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = instances_.find(hwnd);
    if (it != instances_.end()) {
        return it->second.host;
    }
    return nullptr;
}

BridgeCore* WebViewContext::GetPrimaryBridge() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (primaryHwnd_ && instances_.count(primaryHwnd_)) {
        return instances_.at(primaryHwnd_).bridge;
    }
    return nullptr;
}

std::vector<HWND> WebViewContext::GetAllInstances() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HWND> result;
    result.reserve(instances_.size());
    for (const auto& [hwnd, _] : instances_) {
        result.push_back(hwnd);
    }
    return result;
}

WebViewHost* WebViewContext::GetHostByHwnd(HWND hwnd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 直接查找
    auto it = instances_.find(hwnd);
    if (it != instances_.end()) {
        return it->second.host;
    }
    
    // 若 hwnd 是子窗口，获取顶级窗口后再查找
    HWND topLevel = ::GetAncestor(hwnd, GA_ROOT);
    if (topLevel && topLevel != hwnd) {
        auto it2 = instances_.find(topLevel);
        if (it2 != instances_.end()) {
            return it2->second.host;
        }
    }
    
    return nullptr;
}

WebViewPanel* WebViewContext::GetPanelByHwnd(HWND hwnd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 直接查找
    auto it = instances_.find(hwnd);
    if (it != instances_.end()) {
        return it->second.panel;
    }
    
    // 若 hwnd 是子窗口，获取顶级窗口后再查找
    HWND topLevel = ::GetAncestor(hwnd, GA_ROOT);
    if (topLevel && topLevel != hwnd) {
        auto it2 = instances_.find(topLevel);
        if (it2 != instances_.end()) {
            return it2->second.panel;
        }
    }
    
    return nullptr;
}

void WebViewContext::BroadcastEvent(const std::string& event, const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t targetCount = instances_.size();
    if (targetCount == 0) {
        return;
    }

    // Sampling log: trace broadcast dispatch volume and payload size
    static std::atomic<uint64_t> broadcastCounter{0};
    uint64_t count = ++broadcastCounter;
    if (count % 200 == 1) {
        FB2K_console_formatter()
            << "[EventFlow] broadcast #" << count
            << " event=" << event.c_str()
            << " target_windows=" << (unsigned)targetCount
            << " method=BroadcastEvent";
    }

    for (const auto& [hwnd, info] : instances_) {
        if (info.bridge) {
            try {
                info.bridge->EmitEvent(event, data);
            } catch (const std::exception& e) {
                FB2K_console_formatter()
                    << "[foo_ui_webview2] BroadcastEvent exception: event="
                    << event.c_str() << " what=" << e.what();
            } catch (...) {
                FB2K_console_formatter()
                    << "[foo_ui_webview2] BroadcastEvent unknown exception: event="
                    << event.c_str();
            }
        }
    }
}

void WebViewContext::BroadcastEventExcept(const std::string& event, const json& data, HWND excludeHwnd) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [hwnd, info] : instances_) {
        if (hwnd != excludeHwnd && info.bridge) {
            try {
                info.bridge->EmitEvent(event, data);
            } catch (const std::exception& e) {
                FB2K_console_formatter()
                    << "[foo_ui_webview2] BroadcastEventExcept exception: event="
                    << event.c_str() << " what=" << e.what();
            } catch (...) {
                FB2K_console_formatter()
                    << "[foo_ui_webview2] BroadcastEventExcept unknown exception: event="
                    << event.c_str();
            }
        }
    }
}

// ============================================
// 窗口 ID 查询（多窗口系统用）
// ============================================

bool WebViewContext::SendEventTo(const std::string& windowId, const std::string& event, const json& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [hwnd, info] : instances_) {
        if (info.windowId == windowId && info.bridge) {
            try {
                info.bridge->EmitEvent(event, data);
            } catch (...) {
                return false;
            }
            return true;
        }
    }
    return false;
}

BridgeCore* WebViewContext::GetBridgeByWindowId(const std::string& windowId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [hwnd, info] : instances_) {
        if (info.windowId == windowId) {
            return info.bridge;
        }
    }
    return nullptr;
}

HWND WebViewContext::GetHwndByWindowId(const std::string& windowId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [hwnd, info] : instances_) {
        if (info.windowId == windowId) {
            return hwnd;
        }
    }
    return nullptr;
}

std::string WebViewContext::GetWindowIdByHwnd(HWND hwnd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = instances_.find(hwnd);
    if (it != instances_.end()) {
        return it->second.windowId;
    }
    return "";
}
