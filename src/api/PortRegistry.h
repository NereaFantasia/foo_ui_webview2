// PortRegistry.h - Bookkeeping of the open ports behind PortHub
// Part of foo_ui_webview2 - foobar2000 WebView2 UI Plugin
//
// A plain data structure with no WebView or foobar2000 dependency, so the
// tests drive it directly. PortHub owns one and serializes access to it;
// announcing a removed port is left to the caller.
#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace port_registry {

// The page that opened a port: the window handle its bridge messages arrive
// with. nullptr when the opener is unknown; such a port closes only by id or
// with its window.
using PageKey = const void*;

struct Port {
    std::string portId;
    std::string name;
    std::string windowId;
    PageKey page = nullptr;
};

class PortRegistry {
public:
    void Add(const Port& port) {
        byName_[port.name].insert(port.portId);
        byWindow_[port.windowId].insert(port.portId);
        if (port.page) byPage_[port.page].insert(port.portId);
        ports_[port.portId] = port;
    }

    const Port* Find(const std::string& portId) const {
        const auto it = ports_.find(portId);
        return it == ports_.end() ? nullptr : &it->second;
    }

    // The ports on a channel, in no particular order. The pointers stay valid
    // until the next change to the registry.
    std::vector<const Port*> ChannelPorts(const std::string& name) const {
        std::vector<const Port*> result;
        const auto it = byName_.find(name);
        if (it == byName_.end()) return result;
        for (const auto& portId : it->second) {
            if (const Port* port = Find(portId)) result.push_back(port);
        }
        return result;
    }

    const std::unordered_map<std::string, Port>& All() const { return ports_; }

    std::optional<Port> Remove(const std::string& portId) {
        const auto it = ports_.find(portId);
        if (it == ports_.end()) return std::nullopt;
        Port port = it->second;
        ports_.erase(it);
        EraseFrom(byName_, port.name, port.portId);
        EraseFrom(byWindow_, port.windowId, port.portId);
        if (port.page) EraseFrom(byPage_, port.page, port.portId);
        return port;
    }

    // Remove every port of a window, or of a page, and return them sorted by
    // id; PortHub's fixed-width ids sort in the order the ports were opened.
    std::vector<Port> RemoveWindow(const std::string& windowId) { return RemoveAll(byWindow_, windowId); }
    std::vector<Port> RemovePage(PageKey page) {
        if (!page) return {};
        return RemoveAll(byPage_, page);
    }

private:
    template <class Key>
    using Index = std::unordered_map<Key, std::unordered_set<std::string>>;

    template <class Key>
    std::vector<Port> RemoveAll(Index<Key>& index, const Key& key) {
        std::vector<Port> removed;
        const auto it = index.find(key);
        if (it == index.end()) return removed;
        std::vector<std::string> ids(it->second.begin(), it->second.end());
        std::sort(ids.begin(), ids.end());
        for (const auto& portId : ids) {
            if (auto port = Remove(portId)) removed.push_back(std::move(*port));
        }
        return removed;
    }

    template <class Key>
    static void EraseFrom(Index<Key>& index, const Key& key, const std::string& portId) {
        const auto it = index.find(key);
        if (it == index.end()) return;
        it->second.erase(portId);
        if (it->second.empty()) index.erase(it);
    }

    std::unordered_map<std::string, Port> ports_;
    Index<std::string> byName_;
    Index<std::string> byWindow_;
    Index<PageKey> byPage_;
};

}  // namespace port_registry
