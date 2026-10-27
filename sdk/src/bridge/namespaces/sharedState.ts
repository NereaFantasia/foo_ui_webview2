import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import type { JsonValue } from '../../types/json.js';

/**
 * `sharedState` — cross-window key/value state store.
 *
 * Distinct from {@link state} (the reactive playback mirror): this is
 * a shared persistent / TTL-bound key/value bag with change events.
 * Events use the `state:*` colon namespace.
 */
export const sharedState = {
    /** Read a value from the cross-window shared state store by key. */
    get: (key: string) =>
        call('state.get', { key }),
    /**
     * `ttlMs` is the lifetime in milliseconds. `null` is not a value; remove a
     * key with {@link sharedState.delete} instead.
     */
    set: (key: string, value: JsonValue, silent = false, ttlMs?: number) =>
        call('state.set', {
            key,
            value,
            silent,
            ...(ttlMs != null ? { ttlMs } : {}),
        }),
    /** Remove a key from the shared state store. */
    delete: (key: string) =>
        call('state.delete', { key }),
    /** List keys in the shared state store matching an optional glob pattern. */
    keys: (pattern = '*') =>
        call('state.keys', { pattern }),
    /** Subscribe to shared state change events. Returns an unsubscribe function. */
    onChange: (handler: (data: unknown) => void) =>
        subscribe('state:changed', handler),
    /** Subscribe to shared state deletion events. Returns an unsubscribe function. */
    onDelete: (handler: (data: unknown) => void) =>
        subscribe('state:deleted', handler),
};
