/**
 * DiscoveryApi.cpp - Proactive foobar2000 Service Discovery API
 * 
 * Enumerates various services in foobar2000 for frontend discovery
 */

#include "pch.h"
#include "api/DiscoveryApi.h"
#include "api/BridgeCore.h"
#include <foobar2000/SDK/menu_helpers.h>
#include "utils/GuidUtils.h"

namespace {
    using json = nlohmann::json;

    using GuidUtils::GuidToString;
    using GuidUtils::StringToGuid;
    
    // Helper: Ensure valid UTF-8 string for JSON serialization
    // Replaces invalid UTF-8 sequences with replacement character
    std::string SafeUtf8String(const char* str) {
        if (!str || !*str) return "";
        
        std::string result;
        const unsigned char* p = reinterpret_cast<const unsigned char*>(str);
        const unsigned char* end = p + std::strlen(str);
        
        while (p < end) {
            if (*p < 0x80) {
                // ASCII
                result += static_cast<char>(*p++);
            } else if ((*p & 0xE0) == 0xC0) {
                // 2-byte sequence
                if (p + 1 < end && (p[1] & 0xC0) == 0x80) {
                    result += static_cast<char>(*p++);
                    result += static_cast<char>(*p++);
                } else {
                    result += '?';
                    p++;
                }
            } else if ((*p & 0xF0) == 0xE0) {
                // 3-byte sequence (includes \u2605 U+2605)
                if (p + 2 < end && (p[1] & 0xC0) == 0x80 &&
                    (p[2] & 0xC0) == 0x80) {
                    result += static_cast<char>(*p++);
                    result += static_cast<char>(*p++);
                    result += static_cast<char>(*p++);
                } else {
                    result += '?';
                    p++;
                }
            } else if ((*p & 0xF8) == 0xF0) {
                // 4-byte sequence
                if (p + 3 < end && (p[1] & 0xC0) == 0x80 &&
                    (p[2] & 0xC0) == 0x80 &&
                    (p[3] & 0xC0) == 0x80) {
                    result += static_cast<char>(*p++);
                    result += static_cast<char>(*p++);
                    result += static_cast<char>(*p++);
                    result += static_cast<char>(*p++);
                } else {
                    result += '?';
                    p++;
                }
            } else {
                // Invalid UTF-8 start byte
                result += '?';
                p++;
            }
        }
        
        return result;
    }

    //==========================================================================
    // discovery.getMainMenuCommands - Get all main menu commands
    //==========================================================================
    json GetMainMenuCommands(const json& /*params*/) {
        json commands = json::array();
        
        service_enum_t<mainmenu_commands> e;
        service_ptr_t<mainmenu_commands> ptr;
        
        while (e.next(ptr)) {
            t_uint32 count = ptr->get_command_count();
            
            for (t_uint32 i = 0; i < count; i++) {
                pfc::string8 name;
                ptr->get_name(i, name);
                
                pfc::string8 desc;
                ptr->get_description(i, desc);
                
                GUID cmdGuid = ptr->get_command(i);
                GUID parentGuid = ptr->get_parent();
                
                commands.push_back({
                    {"name", SafeUtf8String(name.get_ptr())},
                    {"description", SafeUtf8String(desc.get_ptr())},
                    {"guid", GuidToString(cmdGuid)},
                    {"parentGuid", GuidToString(parentGuid)},
                    {"index", i}
                });
            }
        }
        
        return {
            {"success", true},
            {"commands", commands},
            {"count", commands.size()}
        };
    }
    
    //==========================================================================
    // discovery.executeMainMenuCommand - Execute a main menu command
    //==========================================================================
    json ExecuteMainMenuCommand(const json& params) {
        std::string guidStr = params.value("guid", "");
        if (guidStr.empty()) {
            return {{"success", false}, {"error", "guid is required"}};
        }
        
        GUID cmdGuid;
        if (!StringToGuid(guidStr, cmdGuid)) {
            return {{"success", false}, {"error", "Invalid GUID format"}};
        }
        
        bool executed = mainmenu_commands::g_execute(cmdGuid);
        
        return {
            {"success", executed},
            {"guid", guidStr}
        };
    }
    
    //==========================================================================
    // discovery.getMainMenuGroups - Get main menu groups
    //==========================================================================
    json GetMainMenuGroups(const json& /*params*/) {
        json groups = json::array();
        
        service_enum_t<mainmenu_group> e;
        service_ptr_t<mainmenu_group> ptr;
        
        while (e.next(ptr)) {
            GUID guid = ptr->get_guid();
            GUID parentGuid = ptr->get_parent();
            
            // Try to get group name (if it's a popup type)
            std::string name;
            service_ptr_t<mainmenu_group_popup> popup;
            if (ptr->service_query_t(popup)) {
                pfc::string8 popupName;
                popup->get_display_string(popupName);
                name = popupName.get_ptr();
            }
            
            groups.push_back({
                {"guid", GuidToString(guid)},
                {"parentGuid", GuidToString(parentGuid)},
                {"name", name},
                {"sortPriority", ptr->get_sort_priority()}
            });
        }
        
        return {
            {"success", true},
            {"groups", groups},
            {"count", groups.size()}
        };
    }
    
    //==========================================================================
    // discovery.getInputFormats - Get supported input formats
    //==========================================================================
    json GetInputFormats(const json& /*params*/) {
        json fileTypes = json::array();
        
        service_enum_t<input_file_type> eft;
        service_ptr_t<input_file_type> pft;
        
        while (eft.next(pft)) {
            t_uint32 count = pft->get_count();
            for (t_uint32 i = 0; i < count; i++) {
                pfc::string8 name, mask;
                pft->get_name(i, name);
                pft->get_mask(i, mask);
                
                fileTypes.push_back({
                    {"name", name.get_ptr()},
                    {"mask", mask.get_ptr()},
                    {"index", i}
                });
            }
        }
        
        return {
            {"success", true},
            {"fileTypes", fileTypes},
            {"count", fileTypes.size()}
        };
    }
    
    //==========================================================================
    // discovery.getComponents - Get installed components
    //==========================================================================
    json GetComponents(const json& /*params*/) {
        json components = json::array();
        
        service_enum_t<componentversion> e;
        service_ptr_t<componentversion> ptr;
        
        while (e.next(ptr)) {
            pfc::string8 filename, name, version, about;
            ptr->get_file_name(filename);
            ptr->get_component_name(name);
            ptr->get_component_version(version);
            ptr->get_about_message(about);
            
            components.push_back({
                {"filename", filename.get_ptr()},
                {"name", name.get_ptr()},
                {"version", version.get_ptr()},
                {"about", about.get_ptr()}
            });
        }
        
        return {
            {"success", true},
            {"components", components},
            {"count", components.size()}
        };
    }
    
    //==========================================================================
    // discovery.getUIElements - Get UI elements
    //==========================================================================
    json GetUIElements(const json& /*params*/) {
        json elements = json::array();
        
        service_enum_t<ui_element> e;
        service_ptr_t<ui_element> ptr;
        
        while (e.next(ptr)) {
            pfc::string8 name, desc;
            ptr->get_name(name);
            ptr->get_description(desc);
            
            GUID guid = ptr->get_guid();
            GUID subclass = ptr->get_subclass();
            
            elements.push_back({
                {"guid", GuidToString(guid)},
                {"subclassGuid", GuidToString(subclass)},
                {"name", name.get_ptr()},
                {"description", desc.get_ptr()},
                {"isUserAddable", ptr->is_user_addable()}
            });
        }
        
        return {
            {"success", true},
            {"elements", elements},
            {"count", elements.size()}
        };
    }
    
    //==========================================================================
    // discovery.getDspEntries - Get DSP entries
    //==========================================================================
    json GetDspEntries(const json& /*params*/) {
        json entries = json::array();
        
        service_enum_t<dsp_entry> e;
        service_ptr_t<dsp_entry> ptr;
        
        while (e.next(ptr)) {
            pfc::string8 name;
            ptr->get_name(name);
            
            entries.push_back({
                {"guid", GuidToString(ptr->get_guid())},
                {"name", name.get_ptr()}
            });
        }
        
        return {
            {"success", true},
            {"entries", entries},
            {"count", entries.size()}
        };
    }
    
    //==========================================================================
    // discovery.getOutputDevices - Get output devices
    //==========================================================================
    json GetOutputDevices(const json& /*params*/) {
        json devices = json::array();
        
        service_enum_t<output_entry> e;
        service_ptr_t<output_entry> ptr;
        
        while (e.next(ptr)) {
            GUID guid = ptr->get_guid();
            
            devices.push_back({
                {"guid", GuidToString(guid)}
            });
        }
        
        return {
            {"success", true},
            {"devices", devices},
            {"count", devices.size()}
        };
    }
    
    //==========================================================================
    // discovery.getContextMenuCommands - Get context menu commands (most plugins)
    //==========================================================================
    json GetContextMenuCommands(const json& params) {
        json commands = json::array();
        
        service_enum_t<contextmenu_item> e;
        service_ptr_t<contextmenu_item> ptr;
        
        while (e.next(ptr)) {
            t_uint32 count = ptr->get_num_items();
            
            // Try to get parent GUID (v2 API)
            GUID parentGuid = pfc::guid_null;
            service_ptr_t<contextmenu_item_v2> v2;
            if (ptr->service_query_t(v2)) {
                parentGuid = v2->get_parent();
            }
            
            for (t_uint32 i = 0; i < count; i++) {
                pfc::string8 name;
                ptr->get_item_name(i, name);
                
                pfc::string8 desc;
                ptr->get_item_description(i, desc);
                
                GUID cmdGuid = ptr->get_item_guid(i);
                
                commands.push_back({
                    {"name", SafeUtf8String(name.get_ptr())},
                    {"description", SafeUtf8String(desc.get_ptr())},
                    {"guid", GuidToString(cmdGuid)},
                    {"parentGuid", GuidToString(parentGuid)},
                    {"index", i}
                });
            }
        }
        
        return {
            {"success", true},
            {"commands", commands},
            {"count", commands.size()}
        };
    }
    
    //==========================================================================
    // discovery.executeContextMenuCommand - Execute a context menu command
    //==========================================================================
    json ExecuteContextMenuCommand(const json& params) {
        std::string guidStr = params.value("guid", "");
        if (guidStr.empty()) {
            return {{"success", false}, {"error", "guid is required"}};
        }
        
        GUID cmdGuid;
        if (!StringToGuid(guidStr, cmdGuid)) {
            return {{"success", false}, {"error", "Invalid GUID format"}};
        }
        
        // Get the currently playing track or first selected track
        metadb_handle_list items;
        
        // Try to get playing item first
        auto pc = playback_control::get();
        metadb_handle_ptr nowPlaying;
        if (pc->get_now_playing(nowPlaying)) {
            items.add_item(nowPlaying);
        } else {
            // Get active playlist selection
            auto pm = playlist_manager::get();
            pm->activeplaylist_get_selected_items(items);
        }
        
        if (items.get_count() == 0) {
            return {{"success", false}, {"error", "No track selected or playing"}};
        }
        
        // Execute the context menu command
        bool executed = menu_helpers::run_command_context(cmdGuid, pfc::guid_null, items);
        
        return {
            {"success", executed},
            {"guid", guidStr},
            {"itemCount", items.get_count()}
        };
    }
    
    //==========================================================================
    // discovery.executeContextMenuByPath - Execute context menu by path name
    // Supports dynamic sub-menus like "Playback Statistics/Rating/5"
    // Uses contextmenu_manager to traverse the full menu tree
    //==========================================================================
    
    // Helper: Find menu node by path (recursive)
    contextmenu_node* FindNodeByPath(contextmenu_node* node, const std::vector<std::string>& pathParts, size_t index) {
        if (!node || index >= pathParts.size()) return nullptr;
        
        const std::string& targetName = pathParts[index];
        bool isLastPart = (index == pathParts.size() - 1);
        
        // If this is a popup/folder, search its children
        if (node->get_type() == contextmenu_item_node::TYPE_POPUP) {
            t_size childCount = node->get_num_children();
            for (t_size i = 0; i < childCount; i++) {
                contextmenu_node* child = node->get_child(i);
                if (!child) continue;
                
                const char* childName = child->get_name();
                if (!childName) continue;
                
                // Compare names (case-insensitive, trim whitespace)
                std::string name = childName;
                
                // Check if name matches (exact or contains)
                bool matches = (name == targetName) || 
                               (name.find(targetName) != std::string::npos) ||
                               (targetName.find(name) != std::string::npos);
                
                if (matches) {
                    if (isLastPart) {
                        // This is the target command
                        if (child->get_type() == contextmenu_item_node::TYPE_COMMAND) {
                            return child;
                        }
                    } else {
                        // Continue search in child
                        contextmenu_node* result = FindNodeByPath(child, pathParts, index + 1);
                        if (result) return result;
                    }
                }
            }
        }
        
        return nullptr;
    }
    
    // Helper: Split path string
    std::vector<std::string> SplitPath(const std::string& path) {
        std::vector<std::string> parts;
        std::string current;
        for (char c : path) {
            if (c == '/') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            parts.push_back(current);
        }
        return parts;
    }
    
    json ExecuteContextMenuByPath(const json& params) {
        std::string path = params.value("path", "");
        std::string trackPath = params.value("trackPath", "");
        
        if (path.empty()) {
            return {{"success", false}, {"error", "path is required (e.g. 'Playback Statistics/Rating/5')"}};
        }
        
        // Get target track(s)
        metadb_handle_list items;
        
        if (!trackPath.empty()) {
            pfc::string8 canonicalPath;
            filesystem::g_get_canonical_path(trackPath.c_str(), canonicalPath);
            auto mdb = metadb::get();
            metadb_handle_ptr handle = mdb->handle_create(canonicalPath.c_str(), 0);
            if (handle.is_valid()) {
                items.add_item(handle);
            }
        }
        
        if (items.get_count() == 0) {
            auto pc = playback_control::get();
            metadb_handle_ptr nowPlaying;
            if (pc->get_now_playing(nowPlaying)) {
                items.add_item(nowPlaying);
            } else {
                auto pm = playlist_manager::get();
                pm->activeplaylist_get_selected_items(items);
            }
        }
        
        if (items.get_count() == 0) {
            return {{"success", false}, {"error", "No track selected or playing"}};
        }
        
        // Create context menu manager and initialize
        auto mgr = contextmenu_manager::g_create();
        mgr->init_context(items, contextmenu_manager::flag_view_full);
        
        contextmenu_node* root = mgr->get_root();
        if (!root) {
            return {{"success", false}, {"error", "Failed to create context menu"}};
        }
        
        // Split path and search for matching node
        std::vector<std::string> pathParts = SplitPath(path);
        if (pathParts.empty()) {
            return {{"success", false}, {"error", "path contains no valid segments"}};
        }
        
        // Search from root's children
        contextmenu_node* targetNode = nullptr;
        if (root->get_type() == contextmenu_item_node::TYPE_POPUP) {
            t_size childCount = root->get_num_children();
            for (t_size i = 0; i < childCount && !targetNode; i++) {
                contextmenu_node* child = root->get_child(i);
                if (!child) continue;
                
                const char* childName = child->get_name();
                if (!childName) continue;
                
                std::string name = childName;
                bool matches = (name == pathParts[0]) || 
                               (name.find(pathParts[0]) != std::string::npos) ||
                               (pathParts[0].find(name) != std::string::npos);
                
                if (matches) {
                    if (pathParts.size() == 1 && child->get_type() == contextmenu_item_node::TYPE_COMMAND) {
                        targetNode = child;
                    } else {
                        targetNode = FindNodeByPath(child, pathParts, 1);
                    }
                }
            }
        }
        
        if (!targetNode) {
            return {
                {"success", false},
                {"error", "Command not found in menu tree: " + path},
                {"path", path}
            };
        }
        
        // Execute the command
        try {
            targetNode->execute();
            
            pfc::string8 fullName;
            targetNode->get_full_name(fullName);
            
            return {
                {"success", true},
                {"path", path},
                {"foundName", SafeUtf8String(fullName.get_ptr())},
                {"itemCount", items.get_count()}
            };
        } catch (...) {
            return {{"success", false}, {"error", "Failed to execute command"}};
        }
    }
    
    //==========================================================================
    // discovery.getContextMenuTree - Debug: Get full context menu tree structure
    //==========================================================================
    
    // Helper: Recursively dump menu tree to JSON
    json DumpMenuNode(contextmenu_node* node, int depth = 0) {
        if (!node || depth > 10) return nullptr;  // Prevent infinite recursion
        
        json result;
        
        const char* name = node->get_name();
        result["name"] = SafeUtf8String(name ? name : "(null)");
        
        auto type = node->get_type();
        result["type"] = (type == contextmenu_item_node::TYPE_COMMAND) ? "command" :
                         (type == contextmenu_item_node::TYPE_POPUP) ? "popup" :
                         (type == contextmenu_item_node::TYPE_SEPARATOR) ? "separator" : "unknown";
        
        if (type == contextmenu_item_node::TYPE_COMMAND) {
            pfc::string8 fullName;
            node->get_full_name(fullName);
            result["fullName"] = SafeUtf8String(fullName.get_ptr());
        }
        
        if (type == contextmenu_item_node::TYPE_POPUP) {
            json children = json::array();
            t_size childCount = node->get_num_children();
            result["childCount"] = childCount;
            
            for (t_size i = 0; i < childCount && i < 50; i++) {  // Limit to 50 children
                contextmenu_node* child = node->get_child(i);
                if (child) {
                    json childJson = DumpMenuNode(child, depth + 1);
                    if (!childJson.is_null()) {
                        children.push_back(childJson);
                    }
                }
            }
            result["children"] = children;
        }
        
        return result;
    }
    
    json GetContextMenuTree(const json& /*params*/) {
        // Get target track
        metadb_handle_list items;
        
        auto pc = playback_control::get();
        metadb_handle_ptr nowPlaying;
        if (pc->get_now_playing(nowPlaying)) {
            items.add_item(nowPlaying);
        } else {
            auto pm = playlist_manager::get();
            pm->activeplaylist_get_selected_items(items);
        }
        
        if (items.get_count() == 0) {
            return {{"success", false}, {"error", "No track selected or playing"}};
        }
        
        // Create context menu manager
        auto mgr = contextmenu_manager::g_create();
        mgr->init_context(items, contextmenu_manager::flag_view_full);
        
        contextmenu_node* root = mgr->get_root();
        if (!root) {
            return {{"success", false}, {"error", "Failed to create context menu"}};
        }
        
        // Dump the tree
        json tree = DumpMenuNode(root);
        
        return {
            {"success", true},
            {"tree", tree},
            {"itemCount", items.get_count()}
        };
    }
    // discovery.getPreferencePages - Get preference pages
    //==========================================================================
    json GetPreferencePages(const json& /*params*/) {
        json pages = json::array();
        
        service_enum_t<preferences_page> e;
        service_ptr_t<preferences_page> ptr;
        
        while (e.next(ptr)) {
            const char* name = ptr->get_name();
            
            GUID guid = ptr->get_guid();
            GUID parentGuid = ptr->get_parent_guid();
            
            pages.push_back({
                {"guid", GuidToString(guid)},
                {"parentGuid", GuidToString(parentGuid)},
                {"name", name ? name : ""}
            });
        }
        
        return {
            {"success", true},
            {"pages", pages},
            {"count", pages.size()}
        };
    }
    
    //==========================================================================
    // discovery.getAllServices - Get all discoverable services summary
    //==========================================================================
    json GetAllServices(const json& /*params*/) {
        // Count each service type
        int mainMenuCommands = 0;
        int mainMenuGroups = 0;
        int inputFormats = 0;
        int uiElements = 0;
        int dspEntries = 0;
        int outputDevices = 0;
        int preferencePages = 0;
        int components = 0;
        
        // Main menu commands
        {
            service_enum_t<mainmenu_commands> e;
            service_ptr_t<mainmenu_commands> ptr;
            while (e.next(ptr)) {
                mainMenuCommands += ptr->get_command_count();
            }
        }
        
        // Main menu groups
        {
            service_enum_t<mainmenu_group> e;
            service_ptr_t<mainmenu_group> ptr;
            while (e.next(ptr)) {
                mainMenuGroups++;
            }
        }
        
        // Input formats
        {
            service_enum_t<input_file_type> e;
            service_ptr_t<input_file_type> ptr;
            while (e.next(ptr)) {
                inputFormats += ptr->get_count();
            }
        }
        
        // UI elements
        {
            service_enum_t<ui_element> e;
            service_ptr_t<ui_element> ptr;
            while (e.next(ptr)) {
                uiElements++;
            }
        }
        
        // DSP
        {
            service_enum_t<dsp_entry> e;
            service_ptr_t<dsp_entry> ptr;
            while (e.next(ptr)) {
                dspEntries++;
            }
        }
        
        // Output devices
        {
            service_enum_t<output_entry> e;
            service_ptr_t<output_entry> ptr;
            while (e.next(ptr)) {
                outputDevices++;
            }
        }
        
        // Preference pages
        {
            service_enum_t<preferences_page> e;
            service_ptr_t<preferences_page> ptr;
            while (e.next(ptr)) {
                preferencePages++;
            }
        }
        
        // Components
        {
            service_enum_t<componentversion> e;
            service_ptr_t<componentversion> ptr;
            while (e.next(ptr)) {
                components++;
            }
        }
        
        return {
            {"success", true},
            {"services", {
                {"mainMenuCommands", mainMenuCommands},
                {"mainMenuGroups", mainMenuGroups},
                {"inputFormats", inputFormats},
                {"uiElements", uiElements},
                {"dspEntries", dspEntries},
                {"outputDevices", outputDevices},
                {"preferencePages", preferencePages},
                {"components", components}
            }},
            {"totalServices", mainMenuCommands + mainMenuGroups + inputFormats + 
                              uiElements + dspEntries + 
                              outputDevices + preferencePages + components}
        };
    }
    
    //==========================================================================
    // discovery.searchCommands - Search menu commands
    //==========================================================================
    json SearchCommands(const json& params) {
        std::string query = params.value("query", "");
        if (query.empty()) {
            return {{"success", false}, {"error", "query is required"}};
        }
        
        // Convert to lowercase for search
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
        
        json results = json::array();
        
        service_enum_t<mainmenu_commands> e;
        service_ptr_t<mainmenu_commands> ptr;
        
        while (e.next(ptr)) {
            t_uint32 count = ptr->get_command_count();
            
            for (t_uint32 i = 0; i < count; i++) {
                pfc::string8 name;
                ptr->get_name(i, name);
                
                pfc::string8 desc;
                ptr->get_description(i, desc);
                
                // Search matching
                std::string lowerName = name.get_ptr();
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                
                std::string lowerDesc = desc.get_ptr();
                std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);
                
                if (lowerName.find(lowerQuery) != std::string::npos ||
                    lowerDesc.find(lowerQuery) != std::string::npos) {
                    
                    GUID cmdGuid = ptr->get_command(i);
                    
                    results.push_back({
                        {"name", name.get_ptr()},
                        {"description", desc.get_ptr()},
                        {"guid", GuidToString(cmdGuid)},
                        {"type", "mainmenu"}
                    });
                }
            }
        }
        
        return {
            {"success", true},
            {"query", query},
            {"results", results},
            {"count", results.size()}
        };
    }
    
} // anonymous namespace

namespace discovery_api {

void RegisterApis() {
    auto& bridge = BridgeCore::GetInstance();
    
    // Service discovery APIs
    bridge.RegisterApi("discovery.getAllServices", GetAllServices);
    bridge.RegisterApi("discovery.getMainMenuCommands", GetMainMenuCommands);
    bridge.RegisterApi("discovery.getMainMenuGroups", GetMainMenuGroups);
    bridge.RegisterApi("discovery.executeMainMenuCommand", ExecuteMainMenuCommand);
    bridge.RegisterApi("discovery.getContextMenuCommands", GetContextMenuCommands);
    bridge.RegisterApi("discovery.executeContextMenuCommand", ExecuteContextMenuCommand);
    bridge.RegisterApi("discovery.executeContextMenuByPath", ExecuteContextMenuByPath, {{"trackPath", SecurityLevel::MediaRead}});
    bridge.RegisterApi("discovery.getContextMenuTree", GetContextMenuTree);
    bridge.RegisterApi("discovery.getInputFormats", GetInputFormats);
    bridge.RegisterApi("discovery.getComponents", GetComponents);
    bridge.RegisterApi("discovery.getUIElements", GetUIElements);
    bridge.RegisterApi("discovery.getDspEntries", GetDspEntries);
    bridge.RegisterApi("discovery.getOutputDevices", GetOutputDevices);
    bridge.RegisterApi("discovery.getPreferencePages", GetPreferencePages);
    bridge.RegisterApi("discovery.searchCommands", SearchCommands);
    
    console::print("[DiscoveryApi] Registered 15 discovery APIs");
}

} // namespace discovery_api
