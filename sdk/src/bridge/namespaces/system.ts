/**
 * `system` — API discovery / plugin / locale namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    SystemApiInfo,
    SystemApiStatsResponse,
    SystemPluginInfo,
} from '../../types/responses.js';

export const system = {
    listApis: (includeInternal?: boolean, includeExternal?: boolean) =>
        bridge.invoke<SystemApiInfo[]>('system.listAvailableApis', {
            includeInternal,
            includeExternal,
        }),
    getApisByNamespace: (namespace: string) =>
        bridge.invoke<SystemApiInfo[]>('system.getApisByNamespace', {
            namespace,
        }),
    searchApis: (query: string) =>
        bridge.invoke<SystemApiInfo[]>('system.searchApis', { query }),
    getApiStats: () =>
        bridge.invoke<SystemApiStatsResponse>('system.getApiStats'),
    getRegisteredPlugins: () =>
        bridge.invoke<SystemPluginInfo[]>('system.getRegisteredPlugins'),
    isPluginRegistered: (namespace: string) =>
        bridge.invoke<{ registered: boolean }>('system.isPluginRegistered', {
            namespace,
        }),
    getDPI: () => bridge.invoke<{ dpi: number }>('system.getDPI'),
    getLocale: () => bridge.invoke<{ locale: string }>('system.getLocale'),
    getTheme: () => bridge.invoke<{ theme: string }>('system.getTheme'),
};
