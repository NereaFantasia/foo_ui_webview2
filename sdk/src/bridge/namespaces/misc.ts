import { call } from '../call.js';

/**
 * `misc` — miscellaneous host actions (exit, paths, popups).
 */
export const misc = {
    /** Ask foobar2000 to quit. */
    exit: () => call('misc.exit'),
    /** Ask foobar2000 to restart. */
    restart: () => call('misc.restart'),
    /** Report the directory the component DLL was loaded from. */
    getComponentPath: () =>
        call('misc.getComponentPath'),
    /** Report the foobar2000 installation directory. */
    getFoobarPath: () =>
        call('misc.getFoobarPath'),
    /** Report the foobar2000 profile directory, where configuration and components live. */
    getProfilePath: () =>
        call('misc.getProfilePath'),
    /** Open the foobar2000 console window. */
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
    /** Open the foobar2000 Preferences dialog. */
    showPreferences: () =>
        call('misc.showPreferences'),
};
