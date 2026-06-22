/**
 * `misc` — miscellaneous host actions (exit, paths, popups).
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

export const misc = {
    exit: () => bridge.invoke<BaseResponse>('misc.exit'),
    restart: () => bridge.invoke<BaseResponse>('misc.restart'),
    getComponentPath: () =>
        bridge.invoke<{ path: string }>('misc.getComponentPath'),
    getFoobarPath: () =>
        bridge.invoke<{ path: string }>('misc.getFoobarPath'),
    getProfilePath: () =>
        bridge.invoke<{ path: string }>('misc.getProfilePath'),
    showConsole: () => bridge.invoke<BaseResponse>('misc.showConsole'),
    showLibrarySearch: (query?: string) =>
        bridge.invoke<BaseResponse>('misc.showLibrarySearch', {
            ...(query ? { query } : {}),
        }),
    showPopupMessage: (message: string, title?: string) =>
        bridge.invoke<BaseResponse>('misc.showPopupMessage', {
            message,
            ...(title ? { title } : {}),
        }),
    showPreferences: () =>
        bridge.invoke<BaseResponse>('misc.showPreferences'),
};
