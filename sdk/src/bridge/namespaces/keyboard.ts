import { call } from '../call.js';
import type {
    KeyboardRegisterHotkeyParams,
    KeyboardUnregisterHotkeyParams,
} from '../../types/generated/params.js';

/**
 * `keyboard` — hotkey / shortcut registration namespace.
 */
export const keyboard = {
    /**
     * Register a system-wide hotkey; the response carries the numeric `id`. The hotkey is
     * released when this page navigates or reloads and when its window closes.
     */
    registerHotkey: (
        key: string,
        action: string,
        opts?: Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>,
    ) =>
        call('keyboard.registerHotkey', {
            key,
            action,
            ...(opts || {}),
        }),
    /**
     * Store a key combination under an action name. Nothing matches key presses against it.
     * @deprecated Has no effect and will be removed in 3.0; listen for `keydown` in the page instead.
     */
    registerShortcut: (key: string, action: string) =>
        call('keyboard.registerShortcut', {
            key,
            action,
        }),
    /**
     * Takes exactly one of the numeric `id` and the original `key` string. Removing a shortcut
     * by `key` goes away with `registerShortcut` in 3.0.
     */
    unregisterHotkey: (opts: KeyboardUnregisterHotkeyParams) =>
        call('keyboard.unregisterHotkey', opts),
    /**
     * List registered system hotkeys followed by local shortcuts. The shortcut entries go away
     * with `registerShortcut` in 3.0.
     */
    getRegisteredHotkeys: () =>
        call(
            'keyboard.getRegisteredHotkeys',
        ),
    /**
     * Replace this page's reserved keys: combinations that stay with the page while it has the
     * focus, even when foobar2000 has a keyboard shortcut for them. Cleared when the page
     * navigates or reloads; an empty array releases them all. With `{ all: true }` every key
     * stays with the page, for example while it records a shortcut; call again without it to
     * keep only `keys`.
     */
    setReservedKeys: (keys: string[], options?: { all?: boolean }) =>
        call('keyboard.setReservedKeys', options?.all ? { keys, all: true } : { keys }),
};
