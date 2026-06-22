// DndApi.cpp - Drag and Drop API
// Provides drag-and-drop integration between WebView and native system

#include "pch.h"
#include "api/DndApi.h"
#include "api/BridgeCore.h"
#include <map>
#include <mutex>
#include <shlobj.h>

namespace {
    using json = nlohmann::json;
    
    // Escape a string for safe embedding in a JavaScript single-quoted string literal.
    // Prevents injection via selector or event name parameters.
    static std::string EscapeJsString(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'";  break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '<':  out += "\\x3c"; break;  // prevent </script> break-out
            case '>':  out += "\\x3e"; break;
            default:   out += c; break;
            }
        }
        return out;
    }
    
    // Drop zone registry
    struct DropZone {
        std::string id;
        std::string selector;
        std::vector<std::string> acceptTypes;  // "files", "text", "tracks"
        std::string callbackEvent;
    };
    
    static std::map<std::string, DropZone> g_dropZones;
    static std::mutex g_dndMutex;
    static int g_dropZoneCounter = 0;
    
    //==========================================================================
    // dnd.registerDropZone - Register a drop zone in WebView
    //==========================================================================
    json DndRegisterDropZone(const json& params) {
        std::string selector = params.value("selector", "");
        std::string eventName = params.value("event", "dnd:drop");
        
        if (selector.empty()) {
            return {{"success", false}, {"error", "selector is required"}};
        }
        
        std::vector<std::string> acceptTypes;
        if (params.contains("accept") && params["accept"].is_array()) {
            for (const auto& t : params["accept"]) {
                if (t.is_string()) {
                    acceptTypes.push_back(t.get<std::string>());
                }
            }
        }
        if (acceptTypes.empty()) {
            acceptTypes.emplace_back("files");  // Default to files
        }
        
        std::lock_guard<std::mutex> lock(g_dndMutex);
        
        std::string zoneId = "dropzone_" + std::to_string(++g_dropZoneCounter);
        
        DropZone zone;
        zone.id = zoneId;
        zone.selector = selector;
        zone.acceptTypes = acceptTypes;
        zone.callbackEvent = eventName;
        
        g_dropZones[zoneId] = zone;
        
        // Escape user-provided strings for safe JS embedding
        std::string safeSelector = EscapeJsString(selector);
        std::string safeEventName = EscapeJsString(eventName);
        std::string safeZoneId = EscapeJsString(zoneId);
        
        // Return JavaScript to inject for drop handling
        // This sets up the drop zone in the WebView
        std::string jsSetup = R"(
(function() {
    const element = document.querySelector(')" + safeSelector + R"(');
    if (!element) return false;
    
    element.dataset.dropZoneId = ')" + safeZoneId + R"(';
    
    element.addEventListener('dragover', function(e) {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'copy';
        this.classList.add('drop-target');
    });
    
    element.addEventListener('dragleave', function(e) {
        this.classList.remove('drop-target');
    });
    
    element.addEventListener('drop', function(e) {
        e.preventDefault();
        this.classList.remove('drop-target');
        
        const files = [];
        if (e.dataTransfer.files.length > 0) {
            for (let i = 0; i < e.dataTransfer.files.length; i++) {
                files.push({
                    name: e.dataTransfer.files[i].name,
                    type: e.dataTransfer.files[i].type,
                    size: e.dataTransfer.files[i].size
                });
            }
        }
        
        // Notify via bridge
        window.fb.emit(')" + safeEventName + R"(', {
            zoneId: ')" + safeZoneId + R"(',
            files: files,
            text: e.dataTransfer.getData('text/plain'),
            html: e.dataTransfer.getData('text/html')
        });
    });
    
    return true;
})();
)";
        
        return {
            {"success", true},
            {"zoneId", zoneId},
            {"selector", selector},
            {"accept", acceptTypes},
            {"event", eventName},
            {"script", jsSetup}
        };
    }
    
    //==========================================================================
    // dnd.unregisterDropZone - Unregister a drop zone
    //==========================================================================
    json DndUnregisterDropZone(const json& params) {
        std::string zoneId = params.value("zoneId", "");
        
        if (zoneId.empty()) {
            return {{"success", false}, {"error", "zoneId is required"}};
        }
        
        std::lock_guard<std::mutex> lock(g_dndMutex);
        
        auto it = g_dropZones.find(zoneId);
        if (it == g_dropZones.end()) {
            return {{"success", false}, {"error", "Drop zone not found"}};
        }
        
        std::string selector = it->second.selector;
        g_dropZones.erase(it);
        
        std::string safeSelector = EscapeJsString(selector);
        
        // JavaScript to remove drop zone handling — clone-replace to strip all listeners
        std::string jsCleanup = R"(
(function() {
    const element = document.querySelector(')" + safeSelector + R"(');
    if (element) {
        delete element.dataset.dropZoneId;
        element.classList.remove('drop-target');
        // Clone-replace to remove anonymous event listeners (dragover, dragleave, drop)
        const clone = element.cloneNode(true);
        if (element.parentNode) {
            element.parentNode.replaceChild(clone, element);
        }
    }
    return true;
})();
)";
        
        return {
            {"success", true},
            {"zoneId", zoneId},
            {"script", jsCleanup}
        };
    }
    
    //==========================================================================
    // dnd.startDrag - Initiate a drag operation from WebView
    //==========================================================================
    json DndStartDrag(const json& params) {
        std::string type = params.value("type", "text");
        
        // For drag operations initiated from WebView:
        // - "text": Drag text data
        // - "tracks": Drag foobar2000 track references
        // - "files": Drag file references
        
        if (type == "text") {
            std::string text = params.value("data", "");
            if (text.empty()) {
                return {{"success", false}, {"error", "data is required for text drag"}};
            }
            
            return {
                {"success", true},
                {"type", "text"},
                {"note", "Use the dragstart event in JavaScript to set dataTransfer data"}
            };
        } 
        else if (type == "tracks") {
            // Get track paths from params
            if (!params.contains("paths") || !params["paths"].is_array()) {
                return {{"success", false}, {"error", "paths array is required for tracks drag"}};
            }
            
            const auto& paths = params["paths"];
            
            // For track dragging, we can create a custom data format
            // or use file paths that foobar2000 can understand
            return {
                {"success", true},
                {"type", "tracks"},
                {"trackCount", paths.size()},
                {"note", "Native drag of tracks requires IDropSource implementation. "
                        "Use playlist.add or playback.playPaths for adding tracks."}
            };
        }
        else if (type == "files") {
            if (!params.contains("paths") || !params["paths"].is_array()) {
                return {{"success", false}, {"error", "paths array is required for files drag"}};
            }
            
            return {
                {"success", true},
                {"type", "files"},
                {"note", "File drag from WebView requires native OLE drag-drop implementation. "
                        "Consider using file selection dialogs instead."}
            };
        }
        
        return {
            {"success", false},
            {"error", "Unknown drag type: " + type}
        };
    }
    
    //==========================================================================
    // dnd.getDropZones - Get list of registered drop zones
    //==========================================================================
    json DndGetDropZones(const json& /*params*/) {
        std::lock_guard<std::mutex> lock(g_dndMutex);
        
        json zones = json::array();
        for (const auto& [id, zone] : g_dropZones) {
            zones.push_back({
                {"id", zone.id},
                {"selector", zone.selector},
                {"accept", zone.acceptTypes},
                {"event", zone.callbackEvent}
            });
        }
        
        return {
            {"success", true},
            {"zones", zones},
            {"count", zones.size()}
        };
    }
    
} // anonymous namespace

//==========================================================================
// Register Drag-and-Drop API
//==========================================================================
void RegisterDndApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // dnd.registerDropZone - Register a drop zone
    bridge.RegisterApi("dnd.registerDropZone", DndRegisterDropZone);
    
    // dnd.unregisterDropZone - Unregister a drop zone
    bridge.RegisterApi("dnd.unregisterDropZone", DndUnregisterDropZone);
    
    // dnd.startDrag - Start drag operation
    bridge.RegisterApi("dnd.startDrag", DndStartDrag);
    
    // dnd.getDropZones - List drop zones
    bridge.RegisterApi("dnd.getDropZones", DndGetDropZones);
    
    LOG("Drag-and-Drop API registered (4 APIs)");
}
