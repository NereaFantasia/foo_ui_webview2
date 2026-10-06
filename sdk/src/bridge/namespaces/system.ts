import { bridge } from '../Bridge.js';
import { call } from '../call.js';
/**
 * `system` — API discovery / plugin / locale namespace.
 */
export const system = {
    /** Methods the bridge accepts, in `apis`; calls `system.listAvailableApis`. */
    listApis: (includeInternal?: boolean, includeExternal?: boolean) =>
        call('system.listAvailableApis', { includeInternal, includeExternal }),
    /** Methods of one namespace, in `apis`; empty for an unknown namespace. */
    getApisByNamespace: (namespace: string) =>
        call('system.getApisByNamespace', { namespace }),
    /** Methods whose name or description contains `query`, in `apis`. */
    searchApis: (query: string) =>
        call('system.searchApis', { query }),
    getApiStats: () =>
        call('system.getApiStats'),
    /** External plugins registered with the bridge, in `plugins`. */
    getRegisteredPlugins: () =>
        call('system.getRegisteredPlugins'),
    isPluginRegistered: (namespace: string) =>
        call('system.isPluginRegistered', {
            namespace,
        }),
    getDPI: () => call('system.getDPI'),
    getLocale: () => call('system.getLocale'),
    getTheme: () => call('system.getTheme'),
};
