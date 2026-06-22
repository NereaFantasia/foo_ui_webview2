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
    getMainMenuCommands: () =>
        bridge.invoke<DiscoveryGetMainMenuCommandsResponse>(
            'discovery.getMainMenuCommands',
        ),
    getMainMenuGroups: () =>
        bridge.invoke<DiscoveryGetMainMenuGroupsResponse>(
            'discovery.getMainMenuGroups',
        ),
    executeMainMenuCommand: (guid: string) =>
        bridge.invoke<BaseResponse>('discovery.executeMainMenuCommand', {
            guid,
        }),
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
    searchCommands: (query: string) =>
        bridge.invoke<DiscoverySearchCommandsResponse>(
            'discovery.searchCommands',
            { query },
        ),
};
