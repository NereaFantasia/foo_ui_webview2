/**
 * DiscoveryApi.cpp - Proactive foobar2000 Service Discovery API
 *
 * Enumerates various services in foobar2000 for frontend discovery. Every
 * shape here is declared in src/api/schema/discovery.ts; the structs and the
 * parameter parsing come from the generated DiscoverySchema.h.
 */

#include "pch.h"
#include "api/DiscoveryApi.h"
#include "api/MenuNodeContract.h"
#include "api/TypedApi.h"
#include "api/generated/DiscoverySchema.h"
#include <foobar2000/SDK/menu_helpers.h>
#include "utils/GuidUtils.h"
#include "utils/StringUtils.h"
#include "utils/SubsongUtils.h"

namespace {
    namespace discovery = api::discovery;

    using GuidUtils::GuidToString;
    using GuidUtils::StringToGuid;

    // Thin alias: shared implementation lives in StringUtils.h
    inline std::string SafeUtf8String(const char* str) {
        return StringUtils::SafeUtf8(str);
    }

    // Copies the normalized display state onto the declared state members.
    void AssignState(discovery::MenuNodeState& out, const menu_node::State& state) {
        out.enabled = state.enabled;
        out.checked = state.checked;
        out.radioChecked = state.radioChecked;
        out.hidden = state.hidden;
        out.stateKnown = state.stateKnown;
        out.flags = static_cast<std::int64_t>(state.flags);
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
                                 std::vector<discovery::DiscoveryMainMenuCommand>& out,
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

        discovery::DiscoveryMainMenuCommand item;
        item.name = label;
        item.description = description;
        item.guid = owner.guid;
        item.parentGuid = owner.parentGuid;
        item.index = owner.index;
        item.path = path;
        item.isDynamic = true;
        item.isDynamicParent = false;
        AssignState(item, state);
        item.source = menu_node::ToString(menu_node::Source::MainMenuDynamic);
        item.executable = menu_node::IsExecutable(reason);
        item.unaddressableReason = menu_node::ToString(reason);
        if (subGuid != pfc::guid_null) {
            item.subGuid = GuidToString(subGuid);
        }
        out.push_back(std::move(item));
    }

    // Enumerates every static command slot, optionally expanding v2 dynamic
    // subtrees. Static entries keep their historical shape; expansion is purely
    // additive so existing callers keep seeing the parent slot.
    std::vector<discovery::DiscoveryMainMenuCommand> CollectMainMenuCommands(
        bool expandDynamic, bool includeHidden, int& dynamicCount) {
        std::vector<discovery::DiscoveryMainMenuCommand> commands;
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

                // Static slots also go through get_display(): without it disabled /
                // checked is invisible, and a command whose get_display() returns false
                // (shortcut-only) looks like an ordinary invocable entry. `label` comes
                // from get_name() rather than the display text, so `name` / `path` are
                // unaffected by get_display().
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

                discovery::DiscoveryMainMenuCommand item;
                item.name = label;
                item.description = SafeUtf8String(desc.get_ptr());
                item.guid = cmdGuidStr;
                item.parentGuid = parentGuidStr;
                item.index = i;
                item.path = label;
                item.isDynamic = isDynamic;
                item.isDynamicParent = isDynamic;
                AssignState(item, state);
                item.source = menu_node::ToString(menu_node::Source::MainMenuStatic);
                item.executable = menu_node::IsExecutable(reason);
                item.unaddressableReason = menu_node::ToString(reason);
                commands.push_back(std::move(item));

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
    api::Result<discovery::GetMainMenuCommandsResult> GetMainMenuCommands(
        const discovery::GetMainMenuCommandsParams& params) {
        // Hidden entries (get_display() == false, or flag_defaulthidden) are
        // filtered by default: they are not reachable from the real menu, so
        // listing them as invocable commands was misleading. Callers that want
        // the historical superset can opt back in.
        int dynamicCount = 0;
        discovery::GetMainMenuCommandsResult result;
        result.commands = CollectMainMenuCommands(params.expandDynamic, params.includeHidden, dynamicCount);
        result.count = static_cast<std::int64_t>(result.commands.size());
        result.expandDynamic = params.expandDynamic;
        result.includeHidden = params.includeHidden;
        result.dynamicCount = dynamicCount;
        return result;
    }

    //==========================================================================
    // discovery.executeMainMenuCommand - Execute a main menu command
    //==========================================================================
    api::Result<discovery::ExecuteMainMenuCommandResult> ExecuteMainMenuCommand(
        const discovery::ExecuteMainMenuCommandParams& params) {
        GUID cmdGuid;
        if (!StringToGuid(params.guid, cmdGuid)) {
            return api::Fail("Invalid GUID format", ApiErrorCode::INVALID_PARAMS);
        }

        // Dynamic children reported by getMainMenuCommands are addressed by the
        // owning command GUID plus the node subGuid; g_execute alone cannot
        // reach them.
        if (params.subGuid.has_value() && !params.subGuid->empty()) {
            GUID subGuid;
            if (!StringToGuid(*params.subGuid, subGuid)) {
                return api::Fail("Invalid subGuid format", ApiErrorCode::INVALID_PARAMS);
            }

            if (!mainmenu_commands::g_execute_dynamic(cmdGuid, subGuid)) {
                return api::Fail("No dynamic main-menu command owns this guid and subGuid",
                                 ApiErrorCode::NOT_FOUND,
                                 {{"guid", params.guid}, {"subGuid", *params.subGuid}, {"dynamic", true}});
            }
            discovery::ExecuteMainMenuCommandResult result;
            result.guid = params.guid;
            result.subGuid = *params.subGuid;
            result.dynamic = true;
            return result;
        }

        if (!mainmenu_commands::g_execute(cmdGuid)) {
            return api::Fail("No main-menu command owns this guid", ApiErrorCode::NOT_FOUND,
                             {{"guid", params.guid}, {"dynamic", false}});
        }
        discovery::ExecuteMainMenuCommandResult result;
        result.guid = params.guid;
        result.dynamic = false;
        return result;
    }

    //==========================================================================
    // discovery.getMainMenuGroups - Get main menu groups
    //==========================================================================
    api::Result<discovery::GetMainMenuGroupsResult> GetMainMenuGroups(
        const discovery::GetMainMenuGroupsParams& /*params*/) {
        discovery::GetMainMenuGroupsResult result;

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

            discovery::DiscoveryMainMenuGroup group;
            group.guid = GuidToString(guid);
            group.parentGuid = GuidToString(parentGuid);
            group.name = name;
            group.sortPriority = ptr->get_sort_priority();
            result.groups.push_back(std::move(group));
        }

        result.count = static_cast<std::int64_t>(result.groups.size());
        return result;
    }

    //==========================================================================
    // discovery.getInputFormats - Get supported input formats
    //==========================================================================
    api::Result<discovery::GetInputFormatsResult> GetInputFormats(
        const discovery::GetInputFormatsParams& /*params*/) {
        discovery::GetInputFormatsResult result;

        service_enum_t<input_file_type> eft;
        service_ptr_t<input_file_type> pft;

        while (eft.next(pft)) {
            t_uint32 count = pft->get_count();
            for (t_uint32 i = 0; i < count; i++) {
                pfc::string8 name, mask;
                pft->get_name(i, name);
                pft->get_mask(i, mask);

                discovery::DiscoveryInputFormatType type;
                type.name = SafeUtf8String(name.get_ptr());
                type.mask = SafeUtf8String(mask.get_ptr());
                type.index = i;
                result.fileTypes.push_back(std::move(type));
            }
        }

        result.count = static_cast<std::int64_t>(result.fileTypes.size());
        return result;
    }

    //==========================================================================
    // discovery.getComponents - Get installed components
    //==========================================================================
    api::Result<discovery::GetComponentsResult> GetComponents(
        const discovery::GetComponentsParams& /*params*/) {
        discovery::GetComponentsResult result;

        service_enum_t<componentversion> e;
        service_ptr_t<componentversion> ptr;

        while (e.next(ptr)) {
            pfc::string8 filename, name, version, about;
            ptr->get_file_name(filename);
            ptr->get_component_name(name);
            ptr->get_component_version(version);
            ptr->get_about_message(about);

            discovery::DiscoveryComponentInfo component;
            component.filename = SafeUtf8String(filename.get_ptr());
            component.name = SafeUtf8String(name.get_ptr());
            component.version = SafeUtf8String(version.get_ptr());
            component.about = SafeUtf8String(about.get_ptr());
            result.components.push_back(std::move(component));
        }

        result.count = static_cast<std::int64_t>(result.components.size());
        return result;
    }

    //==========================================================================
    // discovery.getUIElements - Get UI elements
    //==========================================================================
    api::Result<discovery::GetUIElementsResult> GetUIElements(
        const discovery::GetUIElementsParams& /*params*/) {
        discovery::GetUIElementsResult result;

        service_enum_t<ui_element> e;
        service_ptr_t<ui_element> ptr;

        while (e.next(ptr)) {
            pfc::string8 name, desc;
            ptr->get_name(name);
            ptr->get_description(desc);

            discovery::DiscoveryUIElementInfo element;
            element.guid = GuidToString(ptr->get_guid());
            element.subclassGuid = GuidToString(ptr->get_subclass());
            element.name = SafeUtf8String(name.get_ptr());
            element.description = SafeUtf8String(desc.get_ptr());
            element.isUserAddable = ptr->is_user_addable();
            result.elements.push_back(std::move(element));
        }

        result.count = static_cast<std::int64_t>(result.elements.size());
        return result;
    }

    //==========================================================================
    // discovery.getDspEntries - Get DSP entries
    //==========================================================================
    api::Result<discovery::GetDspEntriesResult> GetDspEntries(
        const discovery::GetDspEntriesParams& /*params*/) {
        discovery::GetDspEntriesResult result;

        service_enum_t<dsp_entry> e;
        service_ptr_t<dsp_entry> ptr;

        while (e.next(ptr)) {
            pfc::string8 name;
            ptr->get_name(name);

            discovery::DiscoveryDspEntryInfo entry;
            entry.guid = GuidToString(ptr->get_guid());
            entry.name = SafeUtf8String(name.get_ptr());
            result.entries.push_back(std::move(entry));
        }

        result.count = static_cast<std::int64_t>(result.entries.size());
        return result;
    }

    //==========================================================================
    // discovery.getOutputDevices - Get output devices
    //==========================================================================
    api::Result<discovery::GetOutputDevicesResult> GetOutputDevices(
        const discovery::GetOutputDevicesParams& /*params*/) {
        discovery::GetOutputDevicesResult result;

        service_enum_t<output_entry> e;
        service_ptr_t<output_entry> ptr;

        while (e.next(ptr)) {
            discovery::DiscoveryOutputDeviceEntry device;
            device.guid = GuidToString(ptr->get_guid());
            result.devices.push_back(std::move(device));
        }

        result.count = static_cast<std::int64_t>(result.devices.size());
        return result;
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
    std::vector<discovery::DiscoveryContextMenuCommand> CollectContextMenuCommands(
        bool includeHidden, ContextMenuScan& scan) {
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

        std::vector<discovery::DiscoveryContextMenuCommand> commands;

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
                    // Only fill `description` when the SDK actually returns one; when
                    // get_item_description() returns false the buffer holds whatever the
                    // implementation happened to leave in it.
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

                discovery::DiscoveryContextMenuCommand item;
                item.name = label;
                item.description = haveDesc ? SafeUtf8String(desc.get_ptr()) : std::string();
                item.guid = GuidToString(cmdGuid);
                item.parentGuid = GuidToString(parentGuid);
                item.index = i;
                AssignState(item, state);
                item.source = menu_node::ToString(menu_node::Source::ContextMenuStatic);
                item.executable = menu_node::IsExecutable(reason);
                item.unaddressableReason = menu_node::ToString(reason);
                commands.push_back(std::move(item));
            }
        }

        return commands;
    }

    api::Result<discovery::GetContextMenuCommandsResult> GetContextMenuCommands(
        const discovery::GetContextMenuCommandsParams& params) {
        // Hidden entries are filtered by default for the same reason as the main
        // menu: FORCE_OFF items are shortcut-list-only per the SDK, so listing
        // them as invocable commands misleads callers.
        ContextMenuScan scan;
        discovery::GetContextMenuCommandsResult result;
        result.commands = CollectContextMenuCommands(params.includeHidden, scan);
        result.count = static_cast<std::int64_t>(result.commands.size());
        result.includeHidden = params.includeHidden;
        result.hiddenFiltered = scan.hiddenFiltered;
        result.stateKnown = scan.stateKnown;
        result.selectionCount = static_cast<std::int64_t>(scan.selectionCount);
        return result;
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

    // The track set a context command runs against: the playing track, else the
    // active playlist selection. Returns false when there is neither.
    bool TryGetContextTarget(metadb_handle_list& items) {
        items.remove_all();
        auto pc = playback_control::get();
        metadb_handle_ptr nowPlaying;
        if (pc->get_now_playing(nowPlaying)) {
            items.add_item(nowPlaying);
        } else {
            playlist_manager::get()->activeplaylist_get_selected_items(items);
        }
        return items.get_count() != 0;
    }

    api::Result<discovery::ExecuteContextMenuCommandResult> ExecuteContextMenuCommand(
        const discovery::ExecuteContextMenuCommandParams& params) {
        GUID cmdGuid;
        if (!StringToGuid(params.guid, cmdGuid)) {
            return api::Fail("Invalid GUID format", ApiErrorCode::INVALID_PARAMS);
        }

        metadb_handle_list items;
        if (!TryGetContextTarget(items)) {
            return api::Fail("No track selected or playing", ApiErrorCode::NO_ACTIVE_ITEM);
        }

        // Pre-flight check before run_command_context: the SDK documents a
        // FORCE_OFF command as shortcut-list-only and never shown in the real
        // menu, so it is not dispatched like an ordinary entry. Callers can opt
        // out with `force`.
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
            menu_node::ShouldRefuseExecution(enabledState, params.force)) {
            // FORCE_OFF is not an addressability problem — the command has a
            // perfectly good GUID — so this is reported as "hidden for the
            // current selection", not as an unaddressable node.
            return api::Fail("Command is not available for the current selection",
                             ApiErrorCode::NOT_SUPPORTED,
                             {{"guid", params.guid}, {"name", resolvedName}, {"hidden", true},
                              {"resolved", resolved}, {"force", params.force}});
        }

        if (!menu_helpers::run_command_context(cmdGuid, pfc::guid_null, items)) {
            if (!resolved) {
                return api::Fail("No context-menu command owns this guid", ApiErrorCode::NOT_FOUND,
                                 {{"guid", params.guid}, {"hidden", false}, {"resolved", false},
                                  {"force", params.force}});
            }
            return api::Fail("foobar2000 did not run the command", ApiErrorCode::OPERATION_FAILED,
                             {{"guid", params.guid}, {"name", resolvedName}, {"hidden", hidden},
                              {"resolved", resolved}, {"force", params.force},
                              {"itemCount", items.get_count()}});
        }

        // `hidden` is reported on both paths on purpose: with it only on the
        // refusal branch, a caller could not tell "was not refused" apart from
        // "this build does not report the field".
        discovery::ExecuteContextMenuCommandResult result;
        result.guid = params.guid;
        result.name = resolvedName;
        result.hidden = hidden;
        result.resolved = resolved;
        result.force = params.force;
        result.itemCount = static_cast<std::int64_t>(items.get_count());
        return result;
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

    api::Result<discovery::ExecuteContextMenuByPathResult> ExecuteContextMenuByPath(
        const discovery::ExecuteContextMenuByPathParams& params) {
        // Get target track(s)
        metadb_handle_list items;

        if (params.trackPath.has_value() && !params.trackPath->empty()) {
            // 曲目路径可带 "|subsong:N"：拆掉后缀再规范化，CUE 子曲目按自身执行命令，
            // 不带后缀时指第一首。
            metadb_handle_ptr handle = SubsongUtils::CreateTrackHandle(*params.trackPath);
            if (handle.is_valid()) {
                items.add_item(handle);
            }
        }

        if (items.get_count() == 0 && !TryGetContextTarget(items)) {
            return api::Fail("No track selected or playing", ApiErrorCode::NO_ACTIVE_ITEM);
        }

        // Create context menu manager and initialize
        auto mgr = contextmenu_manager::g_create();
        mgr->init_context(items, contextmenu_manager::flag_view_full);

        contextmenu_node* root = mgr->get_root();
        if (!root) {
            return api::Fail("Failed to create context menu", ApiErrorCode::OPERATION_FAILED);
        }

        // Split path and search for matching node
        const std::vector<std::string> pathParts = menu_node::SplitPath(params.path);
        if (pathParts.empty()) {
            return api::Fail("path contains no valid segments", ApiErrorCode::INVALID_PARAMS);
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
            return api::Fail("Command not found in menu tree: " + params.path, ApiErrorCode::NOT_FOUND,
                             {{"path", params.path}, {"match", menu_node::ToString(matchKind)},
                              {"candidateCount", 0}});
        }

        // Refusing to guess is deliberate: the live host has duplicated labels
        // (e.g. three separate "Reset position" entries), so executing whichever
        // one happened to be enumerated first is a correctness bug, not a
        // convenience.
        if (matchKind == menu_node::MatchKind::Ambiguous) {
            return api::Fail("Path is ambiguous; refine it to address one command: " + params.path,
                             ApiErrorCode::INVALID_PARAMS,
                             {{"path", params.path}, {"match", menu_node::ToString(matchKind)},
                              {"candidateCount", matches.size()}, {"candidates", matchNames}});
        }

        contextmenu_node* targetNode = matches.front();

        // Execute the command
        try {
            targetNode->execute();

            pfc::string8 fullName;
            targetNode->get_full_name(fullName);

            discovery::ExecuteContextMenuByPathResult result;
            result.path = params.path;
            result.foundName = SafeUtf8String(fullName.get_ptr());
            result.match = menu_node::ToString(matchKind);
            result.candidateCount = static_cast<std::int64_t>(matches.size());
            result.itemCount = static_cast<std::int64_t>(items.get_count());
            return result;
        } catch (...) {
            return api::Fail("Failed to execute command", ApiErrorCode::OPERATION_FAILED);
        }
    }

    //==========================================================================
    // discovery.getContextMenuTree - Full context menu tree structure
    //==========================================================================

    // Recursively dumps the menu tree. Both traversal limits come from the
    // shared contract and, more importantly, are REPORTED: a walk that clipped
    // children or depth says so on the node, and the flags propagate upward.
    discovery::DiscoveryContextMenuTreeNode DumpMenuNode(contextmenu_node* node,
                                                         int depth,
                                                         menu_node::Truncation& truncation) {
        discovery::DiscoveryContextMenuTreeNode result;

        const char* name = node->get_name();
        result.name = SafeUtf8String(name ? name : "(null)");

        auto type = node->get_type();
        result.type = (type == contextmenu_item_node::TYPE_COMMAND) ? "command" :
                      (type == contextmenu_item_node::TYPE_POPUP) ? "popup" :
                      (type == contextmenu_item_node::TYPE_SEPARATOR) ? "separator" : "unknown";
        result.depth = depth;

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
            result.enabled = state.enabled;
            result.checked = state.checked;
            result.radioChecked = state.radioChecked;
            result.hidden = state.hidden;
            result.stateKnown = state.stateKnown;
            result.flags = static_cast<std::int64_t>(state.flags);
        }

        if (type == contextmenu_item_node::TYPE_COMMAND) {
            pfc::string8 fullName;
            try {
                node->get_full_name(fullName);
            } catch (...) {
            }
            result.fullName = SafeUtf8String(fullName.get_ptr());
        }

        menu_node::Truncation local;

        if (type == contextmenu_item_node::TYPE_POPUP) {
            t_size childCount = 0;
            try {
                childCount = node->get_num_children();
            } catch (...) {
                childCount = 0;
            }
            result.childCount = static_cast<std::int64_t>(childCount);

            const menu_node::ChildWalkPlan plan =
                menu_node::PlanChildWalk(depth, childCount);
            local.merge(plan.truncation);

            std::vector<discovery::DiscoveryContextMenuTreeNode> children;
            for (t_size i = 0; i < plan.visitCount; i++) {
                contextmenu_node* child = nullptr;
                try {
                    child = node->get_child(i);
                } catch (...) {
                    continue;
                }
                if (!child) continue;

                children.push_back(DumpMenuNode(child, depth + 1, local));
            }
            // Emitted so `childCount` can be reconciled with what was actually
            // returned without the caller having to count the array itself.
            result.childrenReturned = static_cast<std::int64_t>(children.size());
            result.children = std::move(children);
        }

        result.truncated = local.any();
        result.depthExceeded = local.depthExceeded;
        result.childrenExceeded = local.childrenExceeded;

        truncation.merge(local);
        return result;
    }

    api::Result<discovery::GetContextMenuTreeResult> GetContextMenuTree(
        const discovery::GetContextMenuTreeParams& /*params*/) {
        metadb_handle_list items;
        if (!TryGetContextTarget(items)) {
            return api::Fail("No track selected or playing", ApiErrorCode::NO_ACTIVE_ITEM);
        }

        // Create context menu manager
        auto mgr = contextmenu_manager::g_create();
        mgr->init_context(items, contextmenu_manager::flag_view_full);

        contextmenu_node* root = mgr->get_root();
        if (!root) {
            return api::Fail("Failed to create context menu", ApiErrorCode::OPERATION_FAILED);
        }

        // Dump the tree, carrying truncation up to the response so a caller can
        // tell a complete tree from a clipped one.
        menu_node::Truncation truncation;
        discovery::GetContextMenuTreeResult result;
        result.tree = DumpMenuNode(root, /*depth=*/0, truncation);
        result.truncated = truncation.any();
        result.depthExceeded = truncation.depthExceeded;
        result.childrenExceeded = truncation.childrenExceeded;
        result.maxDepth = menu_node::kMaxMenuTreeDepth;
        result.maxChildrenPerNode = menu_node::kMaxChildrenPerNode;
        result.itemCount = static_cast<std::int64_t>(items.get_count());
        return result;
    }

    //==========================================================================
    // discovery.getPreferencePages - Get preference pages
    //==========================================================================
    api::Result<discovery::GetPreferencePagesResult> GetPreferencePages(
        const discovery::GetPreferencePagesParams& /*params*/) {
        discovery::GetPreferencePagesResult result;

        service_enum_t<preferences_page> e;
        service_ptr_t<preferences_page> ptr;

        while (e.next(ptr)) {
            const char* name = ptr->get_name();

            discovery::DiscoveryPreferencePageInfo page;
            page.guid = GuidToString(ptr->get_guid());
            page.parentGuid = GuidToString(ptr->get_parent_guid());
            page.name = SafeUtf8String(name ? name : "");
            result.pages.push_back(std::move(page));
        }

        result.count = static_cast<std::int64_t>(result.pages.size());
        return result;
    }

    //==========================================================================
    // discovery.getAllServices - Get all discoverable services summary
    //==========================================================================
    // Number of registered services of one kind.
    template <class Service>
    std::int64_t CountServices() {
        service_enum_t<Service> e;
        service_ptr_t<Service> ptr;
        std::int64_t n = 0;
        while (e.next(ptr)) ++n;
        return n;
    }

    api::Result<discovery::GetAllServicesResult> GetAllServices(
        const discovery::GetAllServicesParams& /*params*/) {
        discovery::DiscoveryServiceCounts counts;

        // Main menu commands (dynamic v2 subtrees included, mirroring
        // discovery.getMainMenuCommands so the summary matches the listing).
        // includeHidden tracks that endpoint's default, otherwise this count
        // would silently stop matching the list it claims to summarize.
        int mainMenuDynamicCommands = 0;
        {
            const auto commands = CollectMainMenuCommands(
                /*expandDynamic=*/true, /*includeHidden=*/false,
                mainMenuDynamicCommands);
            counts.mainMenuCommands = static_cast<std::int64_t>(commands.size());
            counts.mainMenuDynamicCommands = mainMenuDynamicCommands;
        }

        // Context-menu commands, counted through the same walk
        // discovery.getContextMenuCommands uses, so the summary covers both menu
        // families of the discoverable surface.
        ContextMenuScan scan;
        {
            const auto commands = CollectContextMenuCommands(/*includeHidden=*/false, scan);
            counts.contextMenuCommands = static_cast<std::int64_t>(commands.size());
        }

        // The remaining families are plain service counts.
        counts.mainMenuGroups = CountServices<mainmenu_group>();
        counts.uiElements = CountServices<ui_element>();
        counts.dspEntries = CountServices<dsp_entry>();
        counts.outputDevices = CountServices<output_entry>();
        counts.preferencePages = CountServices<preferences_page>();
        counts.components = CountServices<componentversion>();
        {
            service_enum_t<input_file_type> e;
            service_ptr_t<input_file_type> ptr;
            std::int64_t n = 0;
            while (e.next(ptr)) n += ptr->get_count();
            counts.inputFormats = n;
        }

        discovery::GetAllServicesResult result;
        // Both menu families are filtered to what the host would show, so the
        // counts stay comparable with the listing endpoints. The number of
        // entries that filtering removed is reported rather than lost.
        result.contextMenuHiddenFiltered = scan.hiddenFiltered;
        result.stateKnown = scan.stateKnown;
        result.totalServices = counts.mainMenuCommands + counts.mainMenuGroups +
                               counts.contextMenuCommands + counts.inputFormats +
                               counts.uiElements + counts.dspEntries +
                               counts.outputDevices + counts.preferencePages + counts.components;
        result.services = std::move(counts);
        return result;
    }

    //==========================================================================
    // discovery.searchCommands - Search menu commands
    //==========================================================================

    bool CommandMatchesQuery(const std::string& name, const std::string& description,
                             const std::string& path, const std::string& query) {
        // Path is included because a command's identity is often only
        // distinguishable through its parent labels.
        return menu_node::ContainsFolded(name, query) ||
               menu_node::ContainsFolded(description, query) ||
               menu_node::ContainsFolded(path, query);
    }

    api::Result<discovery::SearchCommandsResult> SearchCommands(
        const discovery::SearchCommandsParams& params) {
        // The generated parser has already limited `scope` to the three names.
        const menu_node::SearchScope scope = menu_node::ParseSearchScope(params.scope);

        discovery::SearchCommandsResult result;
        int mainMenuHits = 0;
        int contextMenuHits = 0;
        int dynamicCount = 0;
        ContextMenuScan contextScan;

        if (menu_node::ScopeIncludesMainMenu(scope)) {
            const auto commands =
                CollectMainMenuCommands(params.expandDynamic, params.includeHidden, dynamicCount);

            for (const auto& command : commands) {
                // A dynamic parent slot is only a container; its expanded children
                // carry the executable identity, so skip it to avoid duplicate hits.
                if (command.isDynamicParent) continue;
                if (!CommandMatchesQuery(command.name, command.description, command.path, params.query)) continue;

                discovery::DiscoverySearchResult hit;
                static_cast<discovery::MenuNodeState&>(hit) = command;
                hit.name = command.name;
                hit.description = command.description;
                hit.guid = command.guid;
                hit.path = command.path;
                hit.isDynamic = command.isDynamic;
                hit.type = menu_node::ToString(menu_node::SearchScope::MainMenu);
                hit.subGuid = command.subGuid;
                hit.source = command.source;
                hit.executable = command.executable;
                hit.unaddressableReason = command.unaddressableReason;
                result.results.push_back(std::move(hit));
                ++mainMenuHits;
            }
        }

        if (menu_node::ScopeIncludesContextMenu(scope)) {
            const auto commands =
                CollectContextMenuCommands(params.includeHidden, contextScan);

            for (const auto& command : commands) {
                // Context entries have no menu path of their own — they are
                // registered flat and placed by the host — so `path` falls back to
                // the label rather than being fabricated.
                if (!CommandMatchesQuery(command.name, command.description, command.name, params.query)) continue;

                discovery::DiscoverySearchResult hit;
                static_cast<discovery::MenuNodeState&>(hit) = command;
                hit.name = command.name;
                hit.description = command.description;
                hit.guid = command.guid;
                hit.path = command.name;
                hit.isDynamic = false;
                hit.type = menu_node::ToString(menu_node::SearchScope::ContextMenu);
                hit.source = command.source;
                hit.executable = command.executable;
                hit.unaddressableReason = command.unaddressableReason;
                result.results.push_back(std::move(hit));
                ++contextMenuHits;
            }
        }

        result.query = params.query;
        result.count = static_cast<std::int64_t>(result.results.size());
        result.expandDynamic = params.expandDynamic;
        result.scope = menu_node::ToString(scope);
        result.includeHidden = params.includeHidden;
        result.mainMenuHits = mainMenuHits;
        result.contextMenuHits = contextMenuHits;
        // False when the context-menu side was searched without a selection:
        // its enabled/checked values are unobservable then, so a caller must
        // not filter hits on them.
        result.stateKnown = menu_node::ScopeIncludesContextMenu(scope) ? contextScan.stateKnown : true;
        return result;
    }

} // anonymous namespace

namespace discovery_api {

void RegisterApis() {
    api::RegisterApi("discovery.getAllServices", GetAllServices);
    api::RegisterApi("discovery.getMainMenuCommands", GetMainMenuCommands);
    api::RegisterApi("discovery.getMainMenuGroups", GetMainMenuGroups);
    api::RegisterApi("discovery.executeMainMenuCommand", ExecuteMainMenuCommand);
    api::RegisterApi("discovery.getContextMenuCommands", GetContextMenuCommands);
    api::RegisterApi("discovery.executeContextMenuCommand", ExecuteContextMenuCommand);
    api::RegisterApi("discovery.executeContextMenuByPath", ExecuteContextMenuByPath);
    api::RegisterApi("discovery.getContextMenuTree", GetContextMenuTree);
    api::RegisterApi("discovery.getInputFormats", GetInputFormats);
    api::RegisterApi("discovery.getComponents", GetComponents);
    api::RegisterApi("discovery.getUIElements", GetUIElements);
    api::RegisterApi("discovery.getDspEntries", GetDspEntries);
    api::RegisterApi("discovery.getOutputDevices", GetOutputDevices);
    api::RegisterApi("discovery.getPreferencePages", GetPreferencePages);
    api::RegisterApi("discovery.searchCommands", SearchCommands);

    console::print("[DiscoveryApi] Registered 15 discovery APIs");
}

} // namespace discovery_api
