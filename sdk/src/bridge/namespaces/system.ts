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
    /** Count registered methods overall and per namespace, and registered plugins. */
    getApiStats: () =>
        call('system.getApiStats'),
    /** External plugins registered with the bridge, in `plugins`. */
    getRegisteredPlugins: () =>
        call('system.getRegisteredPlugins'),
    /** Report whether an external plugin owns the namespace. */
    isPluginRegistered: (namespace: string) =>
        call('system.isPluginRegistered', {
            namespace,
        }),
    /** Report the DPI of the display showing the calling window. */
    getDPI: () => call('system.getDPI'),
    /** Report the active system locale name. */
    getLocale: () => call('system.getLocale'),
    /** Report Windows personalization settings: app theme, accent color, and transparency. */
    getTheme: () => call('system.getTheme'),
};
