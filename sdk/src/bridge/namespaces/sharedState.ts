/**
 * `sharedState` — cross-window key/value state store.
 *
 * Distinct from {@link state} (the reactive playback mirror): this is
 * a shared persistent / TTL-bound key/value bag with change events.
 * Events use the `state:*` colon namespace.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

export const sharedState = {
    get: (key: string) =>
        bridge.invoke<{ value: unknown }>('state.get', { key }),
    set: (key: string, value: unknown, silent = false, ttlMs?: number) =>
        bridge.invoke<BaseResponse>('state.set', {
            key,
            value,
            silent,
            ...(ttlMs != null ? { ttlMs } : {}),
        }),
    delete: (key: string) =>
        bridge.invoke<BaseResponse>('state.delete', { key }),
    keys: (pattern = '*') =>
        bridge.invoke<{ keys: string[] }>('state.keys', { pattern }),
    onChange: (handler: (data: unknown) => void) =>
        bridge.on('state:changed', handler),
    onDelete: (handler: (data: unknown) => void) =>
        bridge.on('state:deleted', handler),
};
