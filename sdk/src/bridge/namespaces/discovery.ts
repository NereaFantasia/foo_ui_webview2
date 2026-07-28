/**
 * `discovery` — service / menu / component discovery namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    DiscoveryGetAllServicesResponse,
    DiscoveryGetMainMenuCommandsResponse,
    DiscoveryGetMainMenuGroupsResponse,
    DiscoveryGetContextMenuCommandsResponse,
    DiscoveryGetContextMenuTreeResponse,
    DiscoveryGetInputFormatsResponse,
    DiscoveryGetComponentsResponse,
    DiscoveryGetUIElementsResponse,
    DiscoveryGetDspEntriesResponse,
    DiscoveryGetOutputDevicesResponse,
    DiscoveryGetPreferencePagesResponse,
    DiscoverySearchCommandsResponse,
} from '../../types/responses.js';
import type {
    DiscoveryExecuteContextMenuByPathParams,
    DiscoveryExecuteContextMenuCommandParams,
    DiscoveryGetContextMenuCommandsParams,
    DiscoveryGetMainMenuCommandsParams,
} from '../../types/generated/params.js';

export const discovery = {
    getAllServices: () =>
        bridge.invoke<DiscoveryGetAllServicesResponse>(
            'discovery.getAllServices',
        ),
    /**
     * Lists main-menu commands. Components that build their submenu at runtime
     * (`mainmenu_commands_v2`, e.g. ESLyric) are expanded by default, so the
     * result includes their child commands in addition to the parent slot.
     * Pass `{ expandDynamic: false }` for the raw static registry only.
     *
     * Entries the host would not show — a command whose `get_display()` returns
     * false, or one carrying `flag_defaulthidden` — are omitted by default,
     * because they are not reachable from the real menu. Pass
     * `{ includeHidden: true }` to get the unfiltered superset.
     */
    getMainMenuCommands: (opts?: DiscoveryGetMainMenuCommandsParams) =>
        bridge.invoke<DiscoveryGetMainMenuCommandsResponse>(
            'discovery.getMainMenuCommands',
            opts,
        ),
    getMainMenuGroups: () =>
        bridge.invoke<DiscoveryGetMainMenuGroupsResponse>(
            'discovery.getMainMenuGroups',
        ),
    /**
     * Executes a main-menu command. For a command expanded from a dynamic
     * submenu, pass the entry's `subGuid` as well; without it only the static
     * command GUID is dispatched.
     */
    executeMainMenuCommand: (guid: string, subGuid?: string) =>
        bridge.invoke<BaseResponse & { subGuid?: string; dynamic?: boolean }>(
            'discovery.executeMainMenuCommand',
            subGuid ? { guid, subGuid } : { guid },
        ),
    /**
     * Lists context-menu commands with their state.
     *
     * `enabled` / `checked` are only observable when a track is selected or
     * playing, because the SDK evaluates display data against a track set.
     * Check the response's `stateKnown` before trusting them: when it is false,
     * only `hidden` is meaningful. `FORCE_OFF` entries are shortcut-list-only
     * per the SDK and are omitted unless `includeHidden` is set.
     */
    getContextMenuCommands: (opts?: DiscoveryGetContextMenuCommandsParams) =>
        bridge.invoke<DiscoveryGetContextMenuCommandsResponse>(
            'discovery.getContextMenuCommands',
            opts,
        ),
    executeContextMenuCommand: (opts: DiscoveryExecuteContextMenuCommandParams) =>
        bridge.invoke<BaseResponse & { itemCount?: number }>(
            'discovery.executeContextMenuCommand',
            opts,
        ),
    executeContextMenuByPath: (opts: DiscoveryExecuteContextMenuByPathParams) =>
        bridge.invoke<
            BaseResponse & { foundName?: string; itemCount?: number }
        >('discovery.executeContextMenuByPath', opts),
    getContextMenuTree: () =>
        bridge.invoke<DiscoveryGetContextMenuTreeResponse>(
            'discovery.getContextMenuTree',
        ),
    getInputFormats: () =>
        bridge.invoke<DiscoveryGetInputFormatsResponse>(
            'discovery.getInputFormats',
        ),
    getComponents: () =>
        bridge.invoke<DiscoveryGetComponentsResponse>('discovery.getComponents'),
    getUIElements: () =>
        bridge.invoke<DiscoveryGetUIElementsResponse>('discovery.getUIElements'),
    getDspEntries: () =>
        bridge.invoke<DiscoveryGetDspEntriesResponse>('discovery.getDspEntries'),
    getOutputDevices: () =>
        bridge.invoke<DiscoveryGetOutputDevicesResponse>(
            'discovery.getOutputDevices',
        ),
    getPreferencePages: () =>
        bridge.invoke<DiscoveryGetPreferencePagesResponse>(
            'discovery.getPreferencePages',
        ),
    /**
     * Case-insensitive substring search over main-menu command names,
     * descriptions and menu paths. Dynamic submenus are expanded by default;
     * pass `{ expandDynamic: false }` to search the static registry only.
     */
    searchCommands: (query: string, opts?: { expandDynamic?: boolean }) =>
        bridge.invoke<DiscoverySearchCommandsResponse>(
            'discovery.searchCommands',
            { query, ...opts },
        ),
};
