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
     */
    getMainMenuCommands: (opts?: { expandDynamic?: boolean }) =>
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
    getContextMenuCommands: () =>
        bridge.invoke<DiscoveryGetContextMenuCommandsResponse>(
            'discovery.getContextMenuCommands',
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
