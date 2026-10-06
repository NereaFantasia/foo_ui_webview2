import { call } from '../call.js';

/**
 * `misc` — miscellaneous host actions (exit, paths, popups).
 */
export const misc = {
    exit: () => call('misc.exit'),
    restart: () => call('misc.restart'),
    getComponentPath: () =>
        call('misc.getComponentPath'),
    getFoobarPath: () =>
        call('misc.getFoobarPath'),
    getProfilePath: () =>
        call('misc.getProfilePath'),
    showConsole: () => call('misc.showConsole'),
    /** An empty or omitted query opens a blank search window. */
    showLibrarySearch: (query?: string) =>
        call('misc.showLibrarySearch', {
            ...(query ? { query } : {}),
        }),
    /** `message` must not be empty; `title` defaults to "Message" on the host. */
    showPopupMessage: (message: string, title?: string) =>
        call('misc.showPopupMessage', {
            message,
            ...(title ? { title } : {}),
        }),
    showPreferences: () =>
        call('misc.showPreferences'),
};
