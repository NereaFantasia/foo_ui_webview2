import { call } from '../call.js';
import type {
    KeyboardRegisterHotkeyParams,
    KeyboardUnregisterHotkeyParams,
} from '../../types/generated/params.js';

/**
 * `keyboard` — hotkey / shortcut registration namespace.
 */
export const keyboard = {
    /** Register a system-wide hotkey; the response carries the numeric `id`. */
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
    /** Register a key combination as an in-window local shortcut. */
    registerShortcut: (key: string, action: string) =>
        call('keyboard.registerShortcut', {
            key,
            action,
        }),
    /** Takes exactly one of the numeric `id` and the original `key` string. */
    unregisterHotkey: (opts: KeyboardUnregisterHotkeyParams) =>
        call('keyboard.unregisterHotkey', opts),
    /** List registered system hotkeys followed by local shortcuts. */
    getRegisteredHotkeys: () =>
        call(
            'keyboard.getRegisteredHotkeys',
        ),
};
