// KeyboardApi.cpp - Keyboard Hotkey API
// Provides global hotkey registration and keyboard shortcuts

#include "pch.h"
#include "api/KeyboardApi.h"
#include "api/BridgeCore.h"
#include "api/CallerContext.h"
#include "core/WebViewContext.h"
#include <map>
#include <mutex>

namespace {
    using json = nlohmann::json;
    
    // Get main window handle
    HWND GetMainWindowHandle() {
        HWND hwnd = core_api::get_main_window();
        if (hwnd) return hwnd;
        return GetActiveWindow();
    }
    
    //==========================================================================
    // Hotkey management
    //==========================================================================
    
    struct HotkeyInfo {
        int id;
        std::string key;
        std::string action;
        bool isGlobal;
        UINT modifiers;
        UINT vk;
        HWND callerHwnd = nullptr;     // 注册者的 panel hwnd
        std::string callerWindowId;    // 注册者的 windowId
    };
    
    std::map<int, HotkeyInfo> g_hotkeys;
    std::map<std::string, HotkeyInfo> g_shortcuts;  // WebView shortcuts (non-global)
    std::mutex g_hotkeyMutex;
    int g_nextHotkeyId = 0x0001;  // Start from 1
    
    //==========================================================================
    // Helper: Parse key string to modifiers and virtual key
    //==========================================================================
    bool ParseKeyString(const std::string& keyStr, UINT& modifiers, UINT& vk) {
        modifiers = 0;
        vk = 0;
        
        std::string key = keyStr;
        
        // Parse modifiers
        auto findAndRemove = [&key](const std::string& mod, UINT flag, UINT& mods) {
            size_t pos;
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            std::string lowerMod = mod;
            std::transform(lowerMod.begin(), lowerMod.end(), lowerMod.begin(), ::tolower);
            
            while ((pos = lowerKey.find(lowerMod + "+")) != std::string::npos) {
                mods |= flag;
                key.erase(pos, mod.length() + 1);
                lowerKey.erase(pos, lowerMod.length() + 1);
            }
        };
        
        findAndRemove("Ctrl", MOD_CONTROL, modifiers);
        findAndRemove("Control", MOD_CONTROL, modifiers);
        findAndRemove("Alt", MOD_ALT, modifiers);
        findAndRemove("Shift", MOD_SHIFT, modifiers);
        findAndRemove("Win", MOD_WIN, modifiers);
        
        // Trim whitespace
        while (!key.empty() && (key[0] == ' ' || key[0] == '+'))
            key = key.substr(1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '+'))
            key.pop_back();
        
        // Convert key to uppercase for comparison
        std::string upperKey = key;
        std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::toupper);
        
        // Map key name to virtual key
        static const std::map<std::string, UINT> keyMap = {
            // Letters
            {"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'},
            {"F", 'F'}, {"G", 'G'}, {"H", 'H'}, {"I", 'I'}, {"J", 'J'},
            {"K", 'K'}, {"L", 'L'}, {"M", 'M'}, {"N", 'N'}, {"O", 'O'},
            {"P", 'P'}, {"Q", 'Q'}, {"R", 'R'}, {"S", 'S'}, {"T", 'T'},
            {"U", 'U'}, {"V", 'V'}, {"W", 'W'}, {"X", 'X'}, {"Y", 'Y'}, {"Z", 'Z'},
            
            // Numbers
            {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
            {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},
            
            // Function keys
            {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
            {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
            {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
            
            // Special keys
            {"SPACE", VK_SPACE}, {"ENTER", VK_RETURN}, {"RETURN", VK_RETURN},
            {"TAB", VK_TAB}, {"ESCAPE", VK_ESCAPE}, {"ESC", VK_ESCAPE},
            {"BACKSPACE", VK_BACK}, {"DELETE", VK_DELETE}, {"DEL", VK_DELETE},
            {"INSERT", VK_INSERT}, {"INS", VK_INSERT},
            {"HOME", VK_HOME}, {"END", VK_END},
            {"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT},
            {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT},
            
            // Media keys
            {"PLAYPAUSE", VK_MEDIA_PLAY_PAUSE}, {"MEDIASTOP", VK_MEDIA_STOP},
            {"NEXTTRACK", VK_MEDIA_NEXT_TRACK}, {"PREVTRACK", VK_MEDIA_PREV_TRACK},
            {"VOLUMEUP", VK_VOLUME_UP}, {"VOLUMEDOWN", VK_VOLUME_DOWN},
            {"VOLUMEMUTE", VK_VOLUME_MUTE},
            
            // Punctuation
            {"-", VK_OEM_MINUS}, {"=", VK_OEM_PLUS},
            {"[", VK_OEM_4}, {"]", VK_OEM_6},
            {";", VK_OEM_1}, {"'", VK_OEM_7},
            {",", VK_OEM_COMMA}, {".", VK_OEM_PERIOD},
            {"/", VK_OEM_2}, {"\\", VK_OEM_5},
            {"`", VK_OEM_3},
        };
        
        auto it = keyMap.find(upperKey);
        if (it != keyMap.end()) {
            vk = it->second;
            return true;
        }
        
        // Single character
        if (upperKey.length() == 1) {
            unsigned char c = static_cast<unsigned char>(upperKey[0]);
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                vk = static_cast<UINT>(c);
                return true;
            }
        }
        
        return false;
    }
    
    //==========================================================================
    // keyboard.registerHotkey - Register global hotkey
    //==========================================================================
    json KeyboardRegisterHotkey(const json& params) {
        std::string key = params.value("key", "");
        std::string action = params.value("action", "");
        bool isGlobal = params.value("global", true);
        
        if (key.empty()) {
            return {{"success", false}, {"error", "key is required"}};
        }
        
        if (action.empty()) {
            return {{"success", false}, {"error", "action is required"}};
        }
        
        UINT modifiers, vk;
        if (!ParseKeyString(key, modifiers, vk)) {
            return {{"success", false}, {"error", "Invalid key string: " + key}};
        }
        
        if (vk == 0) {
            return {{"success", false}, {"error", "Could not determine virtual key from: " + key}};
        }
        
        HWND hwnd = GetMainWindowHandle();
        if (!hwnd) {
            return {{"success", false}, {"error", "Window not available"}};
        }
        
        std::lock_guard<std::mutex> lock(g_hotkeyMutex);
        
        int id = g_nextHotkeyId++;
        
        // Register with Windows
        // Note: Add MOD_NOREPEAT to prevent repeated WM_HOTKEY when key is held
        if (!RegisterHotKey(hwnd, id, modifiers | MOD_NOREPEAT, vk)) {
            DWORD error = GetLastError();
            return {
                {"success", false}, 
                {"error", "Failed to register hotkey (error " + std::to_string(error) + "). Key may already be registered."}
            };
        }
        
        // Store hotkey info (capture caller identity for panel-mode routing)
        HotkeyInfo info;
        info.id = id;
        info.key = key;
        info.action = action;
        info.isGlobal = isGlobal;
        info.modifiers = modifiers;
        info.vk = vk;
        
        auto caller = CallerContext::FromParams(params);
        info.callerHwnd = caller.callerHwnd;
        info.callerWindowId = caller.windowId;
        
        g_hotkeys[id] = info;
        
        return {
            {"success", true},
            {"id", id}
        };
    }
    
    //==========================================================================
    // keyboard.registerShortcut - Register WebView shortcut (non-global)
    //==========================================================================
    json KeyboardRegisterShortcut(const json& params) {
        std::string key = params.value("key", "");
        std::string action = params.value("action", "");
        
        if (key.empty()) {
            return {{"success", false}, {"error", "key is required"}};
        }
        
        if (action.empty()) {
            return {{"success", false}, {"error", "action is required"}};
        }
        
        UINT modifiers, vk;
        if (!ParseKeyString(key, modifiers, vk)) {
            return {{"success", false}, {"error", "Invalid key string: " + key}};
        }
        
        std::lock_guard<std::mutex> lock(g_hotkeyMutex);
        
        HotkeyInfo info;
        info.id = 0;  // Not a Windows hotkey
        info.key = key;
        info.action = action;
        info.isGlobal = false;
        info.modifiers = modifiers;
        info.vk = vk;
        
        g_shortcuts[key] = info;
        
        return {{"success", true}};
    }
    
    //==========================================================================
    // keyboard.unregisterHotkey - Unregister a hotkey
    //==========================================================================
    json KeyboardUnregisterHotkey(const json& params) {
        std::string key = params.value("key", "");
        
        // Handle id as either number or string
        int id = -1;
        if (params.contains("id")) {
            if (params["id"].is_number()) {
                id = params["id"].get<int>();
            } else if (params["id"].is_string()) {
                try {
                    id = std::stoi(params["id"].get<std::string>());
                } catch (...) {
                    // If conversion fails, use the string as key
                    if (key.empty()) {
                        key = params["id"].get<std::string>();
                    }
                }
            }
        }
        
        HWND hwnd = GetMainWindowHandle();
        
        std::lock_guard<std::mutex> lock(g_hotkeyMutex);
        
        if (id > 0) {
            // Unregister by ID
            auto it = g_hotkeys.find(id);
            if (it != g_hotkeys.end()) {
                if (hwnd) {
                    UnregisterHotKey(hwnd, id);
                }
                g_hotkeys.erase(it);
                return {{"success", true}};
            }
        } else if (!key.empty()) {
            // Unregister by key string
            for (auto it = g_hotkeys.begin(); it != g_hotkeys.end(); ++it) {
                if (it->second.key == key) {
                    if (hwnd) {
                        UnregisterHotKey(hwnd, it->first);
                    }
                    g_hotkeys.erase(it);
                    return {{"success", true}};
                }
            }
            
            // Also check shortcuts
            auto sit = g_shortcuts.find(key);
            if (sit != g_shortcuts.end()) {
                g_shortcuts.erase(sit);
                return {{"success", true}};
            }
        }
        
        return {{"success", false}, {"error", "Hotkey not found"}};
    }
    
    //==========================================================================
    // keyboard.getRegisteredHotkeys - List all registered hotkeys
    //==========================================================================
    json KeyboardGetRegisteredHotkeys(const json& /*params*/) {
        std::lock_guard<std::mutex> lock(g_hotkeyMutex);
        
        json hotkeys = json::array();
        
        for (const auto& [id, info] : g_hotkeys) {
            hotkeys.push_back({
                {"id", id},
                {"key", info.key},
                {"action", info.action},
                {"global", info.isGlobal}
            });
        }
        
        for (const auto& [key, info] : g_shortcuts) {
            hotkeys.push_back({
                {"id", 0},
                {"key", info.key},
                {"action", info.action},
                {"global", false}
            });
        }
        
        return {
            {"success", true},
            {"hotkeys", hotkeys}
        };
    }
    
} // anonymous namespace

//==========================================================================
// Process hotkey message from WM_HOTKEY
//==========================================================================
void ProcessHotkeyMessage(int id) {
    std::lock_guard<std::mutex> lock(g_hotkeyMutex);
    
    auto it = g_hotkeys.find(id);
    if (it != g_hotkeys.end()) {
        json data = {
            {"id", id},
            {"key", it->second.key},
            {"action", it->second.action}
        };
        // 路由到注册此 hotkey 的调用者实例
        auto& wvc = WebViewContext::GetInstance();
        bool sent = false;
        if (!it->second.callerWindowId.empty()) {
            sent = wvc.SendEventTo(it->second.callerWindowId, "keyboard:hotkey", data);
        }
        if (!sent && it->second.callerHwnd) {
            if (auto* bridge = wvc.GetBridge(it->second.callerHwnd)) {
                bridge->EmitEvent("keyboard:hotkey", data);
                sent = true;
            }
        }
        if (!sent) {
            BridgeCore::GetInstance().EmitEvent("keyboard:hotkey", data);
        }
    }
}

//==========================================================================
// Register Keyboard API
//==========================================================================
void RegisterKeyboardApi() {
    auto& bridge = BridgeCore::GetInstance();
    
    // keyboard.registerHotkey - Register global hotkey
    bridge.RegisterApi("keyboard.registerHotkey", KeyboardRegisterHotkey);
    
    // keyboard.registerShortcut - Register WebView shortcut
    bridge.RegisterApi("keyboard.registerShortcut", KeyboardRegisterShortcut);
    
    // keyboard.unregisterHotkey - Unregister hotkey
    bridge.RegisterApi("keyboard.unregisterHotkey", KeyboardUnregisterHotkey);
    
    // keyboard.getRegisteredHotkeys - List registered hotkeys
    bridge.RegisterApi("keyboard.getRegisteredHotkeys", KeyboardGetRegisteredHotkeys);
    
    LOG("Keyboard API registered (4 APIs)");
}
