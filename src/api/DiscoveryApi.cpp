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

        // A dynamic leaf is addressed by the owner GUID plus this node's subGuid,
        // so the owner GUID is what has to exist. Reported on the entry itself so
        // a caller never has to infer executability from field presence.
        const menu_node::Unaddressable reason =
            menu_node::ClassifyAddressability(
                menu_node::Kind::Command, /*isDynamicParent=*/false,
                !label.empty(), !owner.guid.empty());

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
            {"stateKnown", state.stateKnown},
            {"source", menu_node::ToString(menu_node::Source::MainMenuDynamic)},
            {"executable", menu_node::IsExecutable(reason)},
            {"unaddressableReason", menu_node::ToString(reason)}
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
                    {"stateKnown", state.stateKnown},
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

    // Outcome of one full pass over the registered context-menu items, kept
    // separate from the entries so the summary numbers survive being reused by
    // more than one endpoint.
    struct ContextMenuScan {
        int hiddenFiltered = 0;
        bool stateKnown = false;
        t_size selectionCount = 0;
    };

    // Shared context-menu enumeration. Extracted so search and the aggregate
    // summary see exactly the entries `discovery.getContextMenuCommands`
    // reports, instead of each endpoint growing its own partial walk.
    json CollectContextMenuCommands(bool includeHidden, ContextMenuScan& scan) {
        // `item_get_display_data_root()` takes a metadb_handle_list, so display
        // flags are only readable when something is selected or playing. Without
        // a selection the listing still works (it did before this change and
        // must keep working), but enabled/checked are reported as unknown rather
        // than fabricated.
        metadb_handle_list selection;
        const bool haveSelection = TryGetContextSelection(selection);
        const GUID caller = contextmenu_item::caller_active_playlist_selection;

        scan.stateKnown = haveSelection;
        scan.selectionCount = selection.get_count();
        scan.hiddenFiltered = 0;

        json commands = json::array();

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
                    ++scan.hiddenFiltered;
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

        return commands;
    }

    json GetContextMenuCommands(const json& params) {
        // Hidden entries are filtered by default for the same reason as the main
        // menu: FORCE_OFF items are shortcut-list-only per the SDK, so listing
        // them as invocable commands misleads callers.
        const bool includeHidden = params.value("includeHidden", false);

        ContextMenuScan scan;
        json commands = CollectContextMenuCommands(includeHidden, scan);

        return {
            {"success", true},
            {"commands", commands},
            {"count", commands.size()},
            {"includeHidden", includeHidden},
            {"hiddenFiltered", scan.hiddenFiltered},
            {"stateKnown", scan.stateKnown},
            {"selectionCount", scan.selectionCount}
        };
    }
    
    //==========================================================================
    // discovery.executeContextMenuCommand - Execute a context menu command
    //==========================================================================
    // Locates a context-menu command by GUID and reports whether the host would
    // let it run. Returns false when no registered item owns the GUID.
    bool TryResolveContextCommand(const GUID& cmdGuid,
                                  menu_node::ContextEnabledState& outState,
                                  std::string& outName) {
        service_enum_t<contextmenu_item> e;
        service_ptr_t<contextmenu_item> ptr;

        while (e.next(ptr)) {
            t_uint32 count = 0;
            try {
                count = ptr->get_num_items();
            } catch (...) {
                continue;
            }

            for (t_uint32 i = 0; i < count; i++) {
                GUID candidate = pfc::guid_null;
                try {
                    candidate = ptr->get_item_guid(i);
                } catch (...) {
                    continue;
                }
                if (candidate != cmdGuid) continue;

                outState = ReadEnabledState(ptr, i);

                pfc::string8 name;
                try {
                    ptr->get_item_name(i, name);
                    outName = SafeUtf8String(name.get_ptr());
                } catch (...) {
                    outName.clear();
                }
                return true;
            }
        }
        return false;
    }

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

        // Pre-flight check. Previously any GUID was handed straight to
        // run_command_context, so a FORCE_OFF command — which the SDK documents
        // as shortcut-list-only and never shows in the real menu — was dispatched
        // as if it were an ordinary entry. Callers can still opt out.
        const bool force = params.value("force", false);
        menu_node::ContextEnabledState enabledState =
            menu_node::ContextEnabledState::DefaultOn;
        std::string resolvedName;
        const bool resolved =
            TryResolveContextCommand(cmdGuid, enabledState, resolvedName);

        // FORCE_OFF is the only state that means "the host would never draw
        // this"; an unresolved GUID has no state to report at all.
        const bool hidden =
            resolved && enabledState == menu_node::ContextEnabledState::ForceOff;

        if (resolved &&
            menu_node::ShouldRefuseExecution(enabledState, force)) {
            // FORCE_OFF is not an addressability problem — the command has a
            // perfectly good GUID — so this is reported as "hidden for the
            // current selection", not as an unaddressable node.
            return {
                {"success", false},
                {"error", "Command is not available for the current selection"},
                {"guid", guidStr},
                {"name", resolvedName},
                {"hidden", true},
                {"resolved", resolved},
                {"force", force}
            };
        }
        
        // Execute the context menu command
        bool executed = menu_helpers::run_command_context(cmdGuid, pfc::guid_null, items);
        
        // `hidden` is reported on both paths on purpose: with it only on the
        // refusal branch, a caller could not tell "was not refused" apart from
        // "this build does not report the field".
        return {
            {"success", executed},
            {"guid", guidStr},
            {"name", resolvedName},
            {"hidden", hidden},
            {"resolved", resolved},
            {"force", force},
            {"itemCount", items.get_count()}
        };
    }
    
    //==========================================================================
    // discovery.executeContextMenuByPath - Execute context menu by path name
    // Supports dynamic sub-menus like "Playback Statistics/Rating/5"
    // Uses contextmenu_manager to traverse the full menu tree
    //==========================================================================
    
    // Collects EVERY node whose path matches, rather than returning the first
    // hit. The previous matcher accepted a substring hit in either direction and
    // took the first winner, so "Rating/1" could resolve to "Rating/10" and
    // silently execute the wrong command. Matching now goes through
    // menu_node::SegmentsEqual (exact after normalization), and an ambiguous
    // request is reported as such instead of being resolved arbitrarily.
    void CollectNodesByPath(contextmenu_node* node,
                            const std::vector<std::string>& pathParts,
                            size_t index,
                            std::vector<contextmenu_node*>& out,
                            std::vector<std::string>& outNames,
                            int depth) {
        if (!node || index >= pathParts.size()) return;
        if (menu_node::DepthExceeded(depth)) return;
        if (node->get_type() != contextmenu_item_node::TYPE_POPUP) return;

        const bool isLastPart = (index == pathParts.size() - 1);

        t_size childCount = 0;
        try {
            childCount = node->get_num_children();
        } catch (...) {
            return;
        }

        for (t_size i = 0; i < childCount; i++) {
            contextmenu_node* child = nullptr;
            try {
                child = node->get_child(i);
            } catch (...) {
                continue;
            }
            if (!child) continue;

            const char* childName = child->get_name();
            if (!childName) continue;

            if (!menu_node::SegmentsEqual(childName, pathParts[index])) continue;

            if (isLastPart) {
                if (child->get_type() == contextmenu_item_node::TYPE_COMMAND) {
                    out.push_back(child);
                    pfc::string8 fullName;
                    try {
                        child->get_full_name(fullName);
                        outNames.push_back(SafeUtf8String(fullName.get_ptr()));
                    } catch (...) {
                        outNames.push_back(SafeUtf8String(childName));
                    }
                }
            } else {
                CollectNodesByPath(child, pathParts, index + 1, out, outNames,
                                   depth + 1);
            }
        }
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
        const std::vector<std::string> pathParts = menu_node::SplitPath(path);
        if (pathParts.empty()) {
            return {{"success", false}, {"error", "path contains no valid segments"}};
        }

        // Walk from the root's children, collecting every match so an ambiguous
        // path can be reported instead of silently resolved to the first hit.
        std::vector<contextmenu_node*> matches;
        std::vector<std::string> matchNames;
        if (root->get_type() == contextmenu_item_node::TYPE_POPUP) {
            t_size childCount = 0;
            try {
                childCount = root->get_num_children();
            } catch (...) {
                childCount = 0;
            }

            for (t_size i = 0; i < childCount; i++) {
                contextmenu_node* child = nullptr;
                try {
                    child = root->get_child(i);
                } catch (...) {
                    continue;
                }
                if (!child) continue;

                const char* childName = child->get_name();
                if (!childName) continue;

                if (!menu_node::SegmentsEqual(childName, pathParts[0])) continue;

                if (pathParts.size() == 1) {
                    if (child->get_type() == contextmenu_item_node::TYPE_COMMAND) {
                        matches.push_back(child);
                        pfc::string8 fullName;
                        try {
                            child->get_full_name(fullName);
                            matchNames.push_back(SafeUtf8String(fullName.get_ptr()));
                        } catch (...) {
                            matchNames.push_back(SafeUtf8String(childName));
                        }
                    }
                } else {
                    CollectNodesByPath(child, pathParts, 1, matches, matchNames, 1);
                }
            }
        }

        const menu_node::MatchKind matchKind =
            menu_node::ClassifyMatch(matches.size());

        if (matchKind == menu_node::MatchKind::NotFound) {
            return {
                {"success", false},
                {"error", "Command not found in menu tree: " + path},
                {"path", path},
                {"match", menu_node::ToString(matchKind)},
                {"candidateCount", 0}
            };
        }

        // Refusing to guess is deliberate: the live host has duplicated labels
        // (e.g. three separate "Reset position" entries), so executing whichever
        // one happened to be enumerated first is a correctness bug, not a
        // convenience.
        if (matchKind == menu_node::MatchKind::Ambiguous) {
            return {
                {"success", false},
                {"error", "Path is ambiguous; refine it to address one command: " + path},
                {"path", path},
                {"match", menu_node::ToString(matchKind)},
                {"candidateCount", matches.size()},
                {"candidates", matchNames}
            };
        }

        contextmenu_node* targetNode = matches.front();

        // Execute the command
        try {
            targetNode->execute();
            
            pfc::string8 fullName;
            targetNode->get_full_name(fullName);
            
            return {
                {"success", true},
                {"path", path},
                {"foundName", SafeUtf8String(fullName.get_ptr())},
                {"match", menu_node::ToString(matchKind)},
                {"candidateCount", matches.size()},
                {"itemCount", items.get_count()}
            };
        } catch (...) {
            return {{"success", false}, {"error", "Failed to execute command"}};
        }
    }
    
    //==========================================================================
    // discovery.getContextMenuTree - Full context menu tree structure
    //==========================================================================

    // Recursively dumps the menu tree. Both traversal limits now come from the
    // shared contract and, more importantly, are REPORTED: the previous walk
    // capped children at 50 and depth at 10 while still emitting the true
    // childCount, so `children.length != childCount` with nothing explaining it.
    json DumpMenuNode(contextmenu_node* node,
                      int depth,
                      menu_node::Truncation& truncation) {
        if (!node) return nullptr;

        json result;

        const char* name = node->get_name();
        result["name"] = SafeUtf8String(name ? name : "(null)");

        auto type = node->get_type();
        result["type"] = (type == contextmenu_item_node::TYPE_COMMAND) ? "command" :
                         (type == contextmenu_item_node::TYPE_POPUP) ? "popup" :
                         (type == contextmenu_item_node::TYPE_SEPARATOR) ? "separator" : "unknown";
        result["depth"] = depth;

        // A separator carries no state and no identity, so only its kind is
        // meaningful; commands and popups both report display flags.
        if (type != contextmenu_item_node::TYPE_SEPARATOR) {
            unsigned displayFlags = 0;
            try {
                displayFlags = node->get_display_flags();
            } catch (...) {
                displayFlags = 0;
            }
            // The manager builds this tree against a real selection and omits
            // anything the host would not draw, so the observed state is
            // trustworthy and nothing here is hidden.
            const menu_node::State state = menu_node::NormalizeContextMenu(
                displayFlags, /*displayReturnedTrue=*/true,
                menu_node::ContextEnabledState::DefaultOn);
            result["enabled"] = state.enabled;
            result["checked"] = state.checked;
            result["radioChecked"] = state.radioChecked;
            result["hidden"] = state.hidden;
            result["stateKnown"] = state.stateKnown;
            result["flags"] = state.flags;
        }

        if (type == contextmenu_item_node::TYPE_COMMAND) {
            pfc::string8 fullName;
            try {
                node->get_full_name(fullName);
            } catch (...) {
            }
            result["fullName"] = SafeUtf8String(fullName.get_ptr());
        }

        menu_node::Truncation local;

        if (type == contextmenu_item_node::TYPE_POPUP) {
            t_size childCount = 0;
            try {
                childCount = node->get_num_children();
            } catch (...) {
                childCount = 0;
            }
            result["childCount"] = childCount;

            const menu_node::ChildWalkPlan plan =
                menu_node::PlanChildWalk(depth, childCount);
            local.merge(plan.truncation);

            json children = json::array();
            for (t_size i = 0; i < plan.visitCount; i++) {
                contextmenu_node* child = nullptr;
                try {
                    child = node->get_child(i);
                } catch (...) {
                    continue;
                }
                if (!child) continue;

                json childJson = DumpMenuNode(child, depth + 1, local);
                if (!childJson.is_null()) {
                    children.push_back(childJson);
                }
            }
            // Emitted so `childCount` can be reconciled with what was actually
            // returned without the caller having to count the array itself.
            result["childrenReturned"] = children.size();
            result["children"] = children;
        }

        result["truncated"] = local.any();
        result["depthExceeded"] = local.depthExceeded;
        result["childrenExceeded"] = local.childrenExceeded;

        truncation.merge(local);
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
        
        // Dump the tree, carrying truncation up to the response so a caller can
        // tell a complete tree from a clipped one.
        menu_node::Truncation truncation;
        json tree = DumpMenuNode(root, /*depth=*/0, truncation);
        
        return {
            {"success", true},
            {"tree", tree},
            {"truncated", truncation.any()},
            {"depthExceeded", truncation.depthExceeded},
            {"childrenExceeded", truncation.childrenExceeded},
            {"maxDepth", menu_node::kMaxMenuTreeDepth},
            {"maxChildrenPerNode", menu_node::kMaxChildrenPerNode},
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
        
        // Context-menu commands, counted through the same walk
        // discovery.getContextMenuCommands uses. Previously absent entirely, so
        // the summary claimed to describe the discoverable surface while omitting
        // one of its two menu families.
        int contextMenuCommands = 0;
        int contextMenuHiddenFiltered = 0;
        bool contextMenuStateKnown = false;
        {
            ContextMenuScan scan;
            json commands = CollectContextMenuCommands(/*includeHidden=*/false, scan);
            contextMenuCommands = static_cast<int>(commands.size());
            contextMenuHiddenFiltered = scan.hiddenFiltered;
            contextMenuStateKnown = scan.stateKnown;
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
                {"contextMenuCommands", contextMenuCommands},
                {"inputFormats", inputFormats},
                {"uiElements", uiElements},
                {"dspEntries", dspEntries},
                {"outputDevices", outputDevices},
                {"preferencePages", preferencePages},
                {"components", components}
            }},
            // Both menu families are filtered to what the host would show, so the
            // counts stay comparable with the listing endpoints. The number of
            // entries that filtering removed is reported rather than lost.
            {"contextMenuHiddenFiltered", contextMenuHiddenFiltered},
            {"stateKnown", contextMenuStateKnown},
            {"totalServices", mainMenuCommands + mainMenuGroups +
                              contextMenuCommands + inputFormats +
                              uiElements + dspEntries + 
                              outputDevices + preferencePages + components}
        };
    }
    
    //==========================================================================
    // discovery.searchCommands - Search menu commands
    //==========================================================================

    // Copies the state vocabulary from an enumerated entry onto a search hit.
    // Search results previously carried no state at all, so a caller had to
    // re-enumerate to find out whether a hit was even invocable.
    void CopyCommandStateToHit(const json& command, json& hit) {
        static const char* const kStateKeys[] = {
            "enabled", "checked", "radioChecked", "hidden",
            "stateKnown", "flags", "source", "executable",
            "unaddressableReason"
        };
        for (const char* key : kStateKeys) {
            if (command.contains(key)) hit[key] = command[key];
        }
    }

    bool CommandMatchesQuery(const json& command, const std::string& query) {
        // Path is included because a command's identity is often only
        // distinguishable through its parent labels.
        return menu_node::ContainsFolded(command.value("name", ""), query) ||
               menu_node::ContainsFolded(command.value("description", ""), query) ||
               menu_node::ContainsFolded(command.value("path", ""), query);
    }

    json SearchCommands(const json& params) {
        std::string query = params.value("query", "");
        if (query.empty()) {
            return {{"success", false}, {"error", "query is required"}};
        }

        const bool expandDynamic = params.value("expandDynamic", true);
        // Both menu families are searched by default. Restricting search to the
        // main menu while hard-coding `type: "mainmenu"` made every right-click
        // command unfindable and left the field carrying no information.
        const menu_node::SearchScope scope =
            menu_node::ParseSearchScope(params.value("scope", std::string()));
        // Search matches against what the host would actually show, mirroring the
        // enumeration endpoints; the historical superset is still reachable.
        const bool includeHidden = params.value("includeHidden", false);

        json results = json::array();
        int mainMenuHits = 0;
        int contextMenuHits = 0;
        int dynamicCount = 0;
        ContextMenuScan contextScan;

        if (menu_node::ScopeIncludesMainMenu(scope)) {
            json commands =
                CollectMainMenuCommands(expandDynamic, includeHidden, dynamicCount);

            for (const auto& command : commands) {
                // A dynamic parent slot is only a container; its expanded children
                // carry the executable identity, so skip it to avoid duplicate hits.
                if (command.value("isDynamicParent", false)) continue;
                if (!CommandMatchesQuery(command, query)) continue;

                json hit = {
                    {"name", command.value("name", "")},
                    {"description", command.value("description", "")},
                    {"guid", command.value("guid", "")},
                    {"path", command.value("path", "")},
                    {"isDynamic", command.value("isDynamic", false)},
                    {"type", menu_node::ToString(menu_node::SearchScope::MainMenu)}
                };
                if (command.contains("subGuid")) {
                    hit["subGuid"] = command["subGuid"];
                }
                CopyCommandStateToHit(command, hit);

                results.push_back(hit);
                ++mainMenuHits;
            }
        }

        if (menu_node::ScopeIncludesContextMenu(scope)) {
            json commands =
                CollectContextMenuCommands(includeHidden, contextScan);

            for (const auto& command : commands) {
                if (!CommandMatchesQuery(command, query)) continue;

                // Context entries have no menu path of their own — they are
                // registered flat and placed by the host — so `path` falls back to
                // the label rather than being fabricated.
                json hit = {
                    {"name", command.value("name", "")},
                    {"description", command.value("description", "")},
                    {"guid", command.value("guid", "")},
                    {"path", command.value("name", "")},
                    {"isDynamic", false},
                    {"type", menu_node::ToString(menu_node::SearchScope::ContextMenu)}
                };
                CopyCommandStateToHit(command, hit);

                results.push_back(hit);
                ++contextMenuHits;
            }
        }

        return {
            {"success", true},
            {"query", query},
            {"results", results},
            {"count", results.size()},
            {"expandDynamic", expandDynamic},
            {"scope", menu_node::ToString(scope)},
            {"includeHidden", includeHidden},
            {"mainMenuHits", mainMenuHits},
            {"contextMenuHits", contextMenuHits},
            // False when the context-menu side was searched without a selection:
            // its enabled/checked values are unobservable then, so a caller must
            // not filter hits on them.
            {"stateKnown", menu_node::ScopeIncludesContextMenu(scope)
                               ? contextScan.stateKnown : true}
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
