/**
 * `keyboard` — hotkey / shortcut registration namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    KeyboardGetRegisteredHotkeysResponse,
} from '../../types/responses.js';
import type {
    KeyboardRegisterHotkeyParams,
    KeyboardUnregisterHotkeyParams,
} from '../../types/generated/params.js';

export const keyboard = {
    registerHotkey: (
        key: string,
        action: string,
        opts?: Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>,
    ) =>
        bridge.invoke<BaseResponse & { id?: string }>(
            'keyboard.registerHotkey',
            {
                key,
                action,
                ...(opts || {}),
            },
        ),
    registerShortcut: (key: string, action: string) =>
        bridge.invoke<BaseResponse>('keyboard.registerShortcut', {
            key,
            action,
        }),
    unregisterHotkey: (opts: KeyboardUnregisterHotkeyParams) =>
        bridge.invoke<BaseResponse>('keyboard.unregisterHotkey', opts),
    getRegisteredHotkeys: () =>
        bridge.invoke<KeyboardGetRegisteredHotkeysResponse>(
            'keyboard.getRegisteredHotkeys',
        ),
};
