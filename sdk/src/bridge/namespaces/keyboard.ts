import { call } from '../call.js';
import type {
    KeyboardRegisterHotkeyParams,
    KeyboardUnregisterHotkeyParams,
} from '../../types/generated/params.js';

/**
 * `keyboard` — hotkey / shortcut registration namespace.
 */
export const keyboard = {
    /** Registers a system-wide hotkey; the response carries the numeric `id`. */
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
    registerShortcut: (key: string, action: string) =>
        call('keyboard.registerShortcut', {
            key,
            action,
        }),
    /** Takes exactly one of the numeric `id` and the original `key` string. */
    unregisterHotkey: (opts: KeyboardUnregisterHotkeyParams) =>
        call('keyboard.unregisterHotkey', opts),
    getRegisteredHotkeys: () =>
        call(
            'keyboard.getRegisteredHotkeys',
        ),
};
