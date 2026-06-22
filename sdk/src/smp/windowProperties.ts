/**
 * `window.GetProperty` / `SetProperty` / `NotifyOthers` —
 * Spider Monkey Panel cross-window IPC compatibility layer.
 *
 * The Spider Monkey Panel runtime exposes three globals on `window`
 * for persisted per-panel properties and broadcast signalling:
 *
 * - `window.GetProperty(name, default)`  → `config.get`  (`smp.prop.<name>` namespace).
 * - `window.SetProperty(name, value)`    → `config.set` / `config.remove`.
 * - `window.NotifyOthers(name, info)`    → `window.broadcast` with a wrapped envelope.
 *
 * Migrated from `sdk/smp-compat.js:1671-1705`.
 */

import type { SmpBridgeShape } from './bridgeShape.js';
import type { NativeFb2k } from '../types/native.js';
import type { JsonValue } from '../types/json.js';

const PROP_PREFIX = 'smp.prop.';

interface ConfigGetResponse {
    value?: JsonValue;
}

declare global {
    interface Window {
        /** SMP property reader (returns `defaultVal` when absent). */
        GetProperty?: (
            name: string,
            defaultVal?: JsonValue,
        ) => Promise<JsonValue>;
        /** SMP property writer; passing `null` / `undefined` removes the entry. */
        SetProperty?: (name: string, val?: unknown) => Promise<void>;
        /** SMP cross-window broadcast (best-effort; ignores rejections). */
        NotifyOthers?: (name: string, info?: unknown) => void;
    }
}

/**
 * Resolve the host-injected native bridge or `null` if missing. Both
 * `window._nativeFb2k` and `window.fb2k` are probed so consumers see
 * the same handle regardless of which alias the host installed.
 */
function _getNativeFb2k(): NativeFb2k | null {
    if (typeof window === 'undefined') return null;
    const w = window as Window & {
        _nativeFb2k?: NativeFb2k;
        fb2k?: NativeFb2k;
    };
    if (w._nativeFb2k && typeof w._nativeFb2k.invoke === 'function') {
        return w._nativeFb2k;
    }
    if (w.fb2k && typeof w.fb2k.invoke === 'function') {
        return w.fb2k;
    }
    return null;
}

/**
 * Install the three SMP-style window properties. Idempotent: an
 * existing assignment on `window` is never overwritten.
 */
export function attachWindowProperties(fb: SmpBridgeShape): void {
    if (typeof window === 'undefined') return;
    const _invoke = fb.invoke.bind(fb);

    if (typeof window.GetProperty !== 'function') {
        window.GetProperty = async (
            name: string,
            defaultVal?: JsonValue,
        ): Promise<JsonValue> => {
            const key = PROP_PREFIX + String(name ?? '');
            const res = (await _invoke('config.get', {
                key,
                default: defaultVal ?? null,
            })) as ConfigGetResponse | null;
            const value = res?.value;
            if (value !== undefined) return value as JsonValue;
            return defaultVal ?? null;
        };
    }

    if (typeof window.SetProperty !== 'function') {
        window.SetProperty = async (
            name: string,
            val?: unknown,
        ): Promise<void> => {
            const key = PROP_PREFIX + String(name ?? '');
            if (val === null || val === undefined) {
                await _invoke('config.remove', { key });
            } else {
                await _invoke('config.set', { key, value: val });
            }
        };
    }

    if (typeof window.NotifyOthers !== 'function') {
        window.NotifyOthers = (name: string, info?: unknown): void => {
            // Use the native bridge directly because `window.broadcast`
            // accepts only `params["message"]`, not arbitrary payload
            // keys — and we don't want to surface this method on `fb`
            // proper since it's SMP-specific.
            const native = _getNativeFb2k();
            if (native) {
                Promise.resolve(
                    native.invoke('window.broadcast', {
                        message: {
                            type: 'smp.notifyOthers',
                            name: String(name ?? ''),
                            info: info ?? null,
                        },
                    }),
                ).catch(() => {
                    /* swallow — best-effort signalling */
                });
            }
        };
    }
}
