/**
 * DiscoveryApi.cpp - Proactive foobar2000 Service Discovery API
 * 
 * Enumerates various services in foobar2000 for frontend discovery
 */

#include "pch.h"
#include "api/DiscoveryApi.h"
#include "api/BridgeCore.h"
#include "api/MenuNodeContract.h"
#include <foobar2000/SDK/menu_helpers.h>
#include "utils/GuidUtils.h"
#include "utils/StringUtils.h"

namespace {
    using json = nlohmann::json;

    using GuidUtils::GuidToString;
    using GuidUtils::StringToGuid;

    // Thin alias: shared implementation lives in StringUtils.h
    inline std::string SafeUtf8String(const char* str) {
        return StringUtils::SafeUtf8(str);
    }

    //==========================================================================
    // Shared main-menu enumeration
    //
    // Plain service_enum_t<mainmenu_commands> only sees statically registered
    // command slots. Components built on mainmenu_commands_v2 (ESLyric and most
    // SMP-era plugins) register a single parent slot and build their real
    // submenu at runtime via dynamic_instantiate(). Enumerating without
    // expanding that node tree hides every dynamic child command.
    //==========================================================================

    // Guards against a malformed / self-referencing node tree.
    constexpr int kMaxDynamicMenuDepth = 16;

    struct DynamicCommandOwner {
        std::string guid;        // owning static command GUID
        std::string parentGuid;  // owning service group GUID
        t_uint32 index;          // owning static command index
    };

    // Walks a mainmenu_node tree and appends one flat entry per leaf command.
    void CollectDynamicMenuNodes(const mainmenu_node::ptr& node,
                                 const std::string& pathPrefix,
                                 const DynamicCommandOwner& owner,
                                 bool includeHidden,
                                 json& out,
                                 int depth) {
        if (!node.is_valid() || depth > kMaxDynamicMenuDepth) return;

        t_uint32 type = mainmenu_node::type_separator;
        try {
            type = node->get_type();
        } catch (...) {
            return;
        }

        if (type == mainmenu_node::type_separator) return;

        pfc::string8 text;
        t_uint32 flags = 0;
        try {
            node->get_display(text, flags);
        } catch (...) {
            // Keep walking with an empty label rather than dropping the subtree.
        }

        std::string label = SafeUtf8String(text.get_ptr());
        std::string path = pathPrefix;
        // Some components label the root of their dynamic subtree with the same
        // text as the owning static slot; appending it again would yield paths
        // like "Desktop Lyrics/Desktop Lyrics/Show".
        const bool duplicatesOwnerLabel = (depth == 0 && label == pathPrefix);
        if (!label.empty() && !duplicatesOwnerLabel) {
            if (!path.empty()) path += '/';
            path += label;
        }

        if (type == mainmenu_node::type_group) {
            t_size childCount = 0;
            try {
                childCount = node->get_children_count();
            } catch (...) {
                return;
            }

            for (t_size i = 0; i < childCount; i++) {
                mainmenu_node::ptr child;
                try {
                    child = node->get_child(i);
                } catch (...) {
                    continue;
                }
                CollectDynamicMenuNodes(child, path, owner, includeHidden, out,
                                        depth + 1);
            }
            return;
        }

        // type_command: executable leaf, addressed by owner GUID + node subGuid.
        GUID subGuid = pfc::guid_null;
        try {
            subGuid = node->get_guid();
        } catch (...) {
            // Leave null; caller can still fall back to path-based execution.
        }

        std::string description;
        try {
            pfc::string8 desc;
            if (node->get_description(desc)) {
                description = SafeUtf8String(desc.get_ptr());
            }
        } catch (...) {
            // Description is optional.
        }

        // mainmenu_node::get_display() returns void, unlike the
        // mainmenu_commands::get_display() used for static slots. A dynamic node
        // therefore has no "return false to hide" signal at all; the only hidden
        // indication available here is flag_defaulthidden, so displayed is
        // unconditionally true.
        const menu_node::State state =
            menu_node::NormalizeMainMenu(flags, /*displayReturnedTrue=*/true);
        if (state.hidden && !includeHidden) return;

        json item = {
            {"name", label},
            {"description", description},
            {"guid", owner.guid},
            {"parentGuid", owner.parentGuid},
            {"index", owner.index},
            {"path", path},
            {"isDynamic", true},
            {"isDynamicParent", false},
            {"flags", flags},
            {"enabled", state.enabled},
            {"checked", state.checked},
            {"radioChecked", state.radioChecked},
            {"hidden", state.hidden},
            {"source", menu_node::ToString(menu_node::Source::MainMenuDynamic)}
        };

        if (subGuid != pfc::guid_null) {
            item["subGuid"] = GuidToString(subGuid);
        }

        out.push_back(item);
    }

    // Enumerates every static command slot, optionally expanding v2 dynamic
    // subtrees. Static entries keep their historical shape; expansion is purely
    // additive so existing callers keep seeing the parent slot.
    json CollectMainMenuCommands(bool expandDynamic, bool includeHidden,
                                 int& dynamicCount) {
        json commands = json::array();
        dynamicCount = 0;

        service_enum_t<mainmenu_commands> e;
        service_ptr_t<mainmenu_commands> ptr;

        while (e.next(ptr)) {
            t_uint32 count = 0;
            try {
                count = ptr->get_command_count();
            } catch (...) {
                continue;
            }

            service_ptr_t<mainmenu_commands_v2> v2;
            const bool hasV2 = ptr->service_query_t(v2);

            GUID parentGuid = pfc::guid_null;
            try {
                parentGuid = ptr->get_parent();
            } catch (...) {
                // Fall back to null group.
            }
            const std::string parentGuidStr = GuidToString(parentGuid);

            for (t_uint32 i = 0; i < count; i++) {
                pfc::string8 name;
                pfc::string8 desc;
                GUID cmdGuid = pfc::guid_null;

                try {
                    ptr->get_name(i, name);
                } catch (...) {
                }
                try {
                    ptr->get_description(i, desc);
                } catch (...) {
                }
                try {
                    cmdGuid = ptr->get_command(i);
                } catch (...) {
                }

                const std::string label = SafeUtf8String(name.get_ptr());
                const std::string cmdGuidStr = GuidToString(cmdGuid);

                // Static slots previously reported no state at all: get_display()
                // was never called, so disabled / checked was invisible and a
                // command that returns false (shortcut-only) looked like an
                // ordinary invocable entry. `label` still comes from get_name()
                // so the existing `name` / `path` fields keep their shape.
                t_uint32 flags = 0;
                bool displayed = true;
                try {
                    pfc::string8 displayText;
                    displayed = ptr->get_display(i, displayText, flags);
                } catch (...) {
                    // Treat a throwing component as "shown with no flags".
                }
                const menu_node::State state =
                    menu_node::NormalizeMainMenu(flags, displayed);
                if (state.hidden && !includeHidden) continue;

                bool isDynamic = false;
                if (expandDynamic && hasV2) {
                    try {
                        isDynamic = v2->is_command_dynamic(i);
                    } catch (...) {
                        isDynamic = false;
                    }
                }

                // A dynamic parent slot is a container, not a command: executing
                // it is undefined behaviour in the SDK.
                const menu_node::Unaddressable reason =
                    menu_node::ClassifyAddressability(
                        menu_node::Kind::Command, isDynamic, !label.empty(),
                        cmdGuid != pfc::guid_null);

                commands.push_back({
                    {"name", label},
                    {"description", SafeUtf8String(desc.get_ptr())},
                    {"guid", cmdGuidStr},
                    {"parentGuid", parentGuidStr},
                    {"index", i},
                    {"path", label},
                    {"isDynamic", isDynamic},
                    {"isDynamicParent", isDynamic},
                    {"flags", flags},
                    {"enabled", state.enabled},
                    {"checked", state.checked},
                    {"radioChecked", state.radioChecked},
                    {"hidden", state.hidden},
                    {"source", menu_node::ToString(menu_node::Source::MainMenuStatic)},
                    {"executable", menu_node::IsExecutable(reason)},
                    {"unaddressableReason", menu_node::ToString(reason)}
                });

                if (!isDynamic) continue;

                mainmenu_node::ptr root;
                try {
                    root = v2->dynamic_instantiate(i);
                } catch (...) {
                    continue;
                }
                if (!root.is_valid()) continue;

                const size_t before = commands.size();
                const DynamicCommandOwner owner{cmdGuidStr, parentGuidStr, i};
                CollectDynamicMenuNodes(root, label, owner, includeHidden,
                                        commands, 0);
                dynamicCount += static_cast<int>(commands.size() - before);
            }
        }

        return commands;
    }

    //==========================================================================
    // discovery.getMainMenuCommands - Get all main menu commands
    //==========================================================================
    json GetMainMenuCommands(const json& params) {
        const bool expandDynamic = params.value("expandDynamic", true);
        // Hidden entries (get_display() == false, or flag_defaulthidden) are
        // filtered by default: they are not reachable from the real menu, so
        // listing them as invocable commands was misleading. Callers that want
        // the historical superset can opt back in.
        const bool includeHidden = params.value("includeHidden", false);

        int dynamicCount = 0;
        json commands =
            CollectMainMenuCommands(expandDynamic, includeHidden, dynamicCount);

        return {
            {"success", true},
            {"commands", commands},
            {"count", commands.size()},
            {"expandDynamic", expandDynamic},
            {"includeHidden", includeHidden},
            {"dynamicCount", dynamicCount}
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

        // Dynamic children reported by getMainMenuCommands are addressed by the
        // owning command GUID plus the node subGuid; g_execute alone cannot
        // reach them.
        std::string subGuidStr = params.value("subGuid", "");
        if (!subGuidStr.empty()) {
            GUID subGuid;
            if (!StringToGuid(subGuidStr, subGuid)) {
                return {{"success", false}, {"error", "Invalid subGuid format"}};
            }

            bool executedDynamic = mainmenu_commands::g_execute_dynamic(cmdGuid, subGuid);
            return {
                {"success", executedDynamic},
                {"guid", guidStr},
                {"subGuid", subGuidStr},
                {"dynamic", true}
            };
        }
        
        bool executed = mainmenu_commands::g_execute(cmdGuid);
        
        return {
            {"success", executed},
            {"guid", guidStr},
            {"dynamic", false}
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
                name = SafeUtf8String(popupName.get_ptr());
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

    // Resolves the track set that context-menu state is evaluated against.
    // Mirrors the selection policy used when actually executing a command, so
    // reported state matches what execution would see. Returns false when
    // nothing is selected or playing.
    bool TryGetContextSelection(metadb_handle_list& out) {
        out.remove_all();
        try {
            metadb_handle_ptr nowPlaying;
            if (playback_control::get()->get_now_playing(nowPlaying)) {
                out.add_item(nowPlaying);
                return true;
            }
            playlist_manager::get()->activeplaylist_get_selected_items(out);
        } catch (...) {
            return false;
        }
        return out.get_count() != 0;
    }

    menu_node::ContextEnabledState ReadEnabledState(
        const service_ptr_t<contextmenu_item>& item, t_uint32 index) {
        try {
            switch (item->get_enabled_state(index)) {
                case contextmenu_item::FORCE_OFF:
                    return menu_node::ContextEnabledState::ForceOff;
                case contextmenu_item::DEFAULT_OFF:
                    return menu_node::ContextEnabledState::DefaultOff;
                default:
                    return menu_node::ContextEnabledState::DefaultOn;
            }
        } catch (...) {
            // A throwing component is treated as ordinarily visible rather than
            // silently dropped from the listing.
            return menu_node::ContextEnabledState::DefaultOn;
        }
    }

    json GetContextMenuCommands(const json& params) {
        // Hidden entries are filtered by default for the same reason as the main
        // menu: FORCE_OFF items are shortcut-list-only per the SDK, so listing
        // them as invocable commands misleads callers.
        const bool includeHidden = params.value("includeHidden", false);

        // `item_get_display_data_root()` takes a metadb_handle_list, so display
        // flags are only readable when something is selected or playing. Without
        // a selection the listing still works (it did before this change and
        // must keep working), but enabled/checked are reported as unknown rather
        // than fabricated.
        metadb_handle_list selection;
        const bool haveSelection = TryGetContextSelection(selection);
        const GUID caller = contextmenu_item::caller_active_playlist_selection;

        json commands = json::array();
        int hiddenFiltered = 0;

        service_enum_t<contextmenu_item> e;
        service_ptr_t<contextmenu_item> ptr;

        while (e.next(ptr)) {
            t_uint32 count = 0;
            try {
                count = ptr->get_num_items();
            } catch (...) {
                continue;
            }

            // Try to get parent GUID (v2 API)
            GUID parentGuid = pfc::guid_null;
            service_ptr_t<contextmenu_item_v2> v2;
            if (ptr->service_query_t(v2)) {
                parentGuid = v2->get_parent();
            }

            for (t_uint32 i = 0; i < count; i++) {
                pfc::string8 name;
                try {
                    ptr->get_item_name(i, name);
                } catch (...) {
                }

                pfc::string8 desc;
                bool haveDesc = false;
                try {
                    // Only fill `description` when the SDK actually returns one;
                    // the previous code kept whatever the buffer happened to
                    // hold (SPEC D16).
                    haveDesc = ptr->get_item_description(i, desc);
                } catch (...) {
                }

                GUID cmdGuid = pfc::guid_null;
                try {
                    cmdGuid = ptr->get_item_guid(i);
                } catch (...) {
                }

                const menu_node::ContextEnabledState enabledState =
                    ReadEnabledState(ptr, i);

                menu_node::State state;
                if (haveSelection) {
                    pfc::string8 displayText;
                    unsigned displayFlags = 0;
                    bool displayed = true;
                    try {
                        displayed = ptr->item_get_display_data_root(
                            displayText, displayFlags, i, selection, caller);
                    } catch (...) {
                        displayed = true;
                        displayFlags = 0;
                    }
                    state = menu_node::NormalizeContextMenu(
                        displayFlags, displayed, enabledState);
                } else {
                    state = menu_node::NormalizeContextMenuStateUnknown(enabledState);
                }

                if (state.hidden && !includeHidden) {
                    ++hiddenFiltered;
                    continue;
                }

                const std::string label = SafeUtf8String(name.get_ptr());
                const menu_node::Unaddressable reason =
                    menu_node::ClassifyAddressability(
                        menu_node::Kind::Command, /*isDynamicParent=*/false,
                        !label.empty(), cmdGuid != pfc::guid_null);

                commands.push_back({
                    {"name", label},
                    {"description", haveDesc ? SafeUtf8String(desc.get_ptr()) : std::string()},
                    {"guid", GuidToString(cmdGuid)},
                    {"parentGuid", GuidToString(parentGuid)},
                    {"index", i},
                    {"enabled", state.enabled},
                    {"checked", state.checked},
                    {"radioChecked", state.radioChecked},
                    {"hidden", state.hidden},
                    {"stateKnown", state.stateKnown},
                    {"flags", state.flags},
                    {"source", menu_node::ToString(menu_node::Source::ContextMenuStatic)},
                    {"executable", menu_node::IsExecutable(reason)},
                    {"unaddressableReason", menu_node::ToString(reason)}
                });
            }
        }

        return {
            {"success", true},
            {"commands", commands},
            {"count", commands.size()},
            {"includeHidden", includeHidden},
            {"hiddenFiltered", hiddenFiltered},
            {"stateKnown", haveSelection},
            {"selectionCount", selection.get_count()}
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
        
        // Main menu commands (dynamic v2 subtrees included, mirroring
        // discovery.getMainMenuCommands so the summary matches the listing).
        // includeHidden tracks that endpoint's default, otherwise this count
        // would silently stop matching the list it claims to summarize.
        int mainMenuDynamicCommands = 0;
        {
            json commands = CollectMainMenuCommands(
                /*expandDynamic=*/true, /*includeHidden=*/false,
                mainMenuDynamicCommands);
            mainMenuCommands = static_cast<int>(commands.size());
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
                {"mainMenuDynamicCommands", mainMenuDynamicCommands},
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

        const bool expandDynamic = params.value("expandDynamic", true);

        // Convert to lowercase for search
        std::string lowerQuery = query;
        std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

        int dynamicCount = 0;
        // Left at the historical superset on purpose: this endpoint keeps its
        // current result set until it is migrated with the rest of the menu
        // surface, so a pilot change here cannot alter search behaviour.
        json commands = CollectMainMenuCommands(
            expandDynamic, /*includeHidden=*/true, dynamicCount);

        json results = json::array();

        for (const auto& command : commands) {
            // A dynamic parent slot is only a container; its expanded children
            // carry the executable identity, so skip it to avoid duplicate hits.
            if (command.value("isDynamicParent", false)) continue;

            std::string safeName = command.value("name", "");
            std::string safeDesc = command.value("description", "");
            std::string safePath = command.value("path", "");

            std::string lowerName = safeName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            std::string lowerDesc = safeDesc;
            std::transform(lowerDesc.begin(), lowerDesc.end(), lowerDesc.begin(), ::tolower);

            std::string lowerPath = safePath;
            std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

            if (lowerName.find(lowerQuery) == std::string::npos &&
                lowerDesc.find(lowerQuery) == std::string::npos &&
                lowerPath.find(lowerQuery) == std::string::npos) {
                continue;
            }

            json hit = {
                {"name", safeName},
                {"description", safeDesc},
                {"guid", command.value("guid", "")},
                {"path", safePath},
                {"isDynamic", command.value("isDynamic", false)},
                {"type", "mainmenu"}
            };

            if (command.contains("subGuid")) {
                hit["subGuid"] = command["subGuid"];
            }

            results.push_back(hit);
        }

        return {
            {"success", true},
            {"query", query},
            {"results", results},
            {"count", results.size()},
            {"expandDynamic", expandDynamic}
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
