import type { JsonObject } from '../types/json.js';
/**
 * `smpUtils` — Shared helpers for the SMP wrapper layer.
 *
 * Migrated from `sdk/smp/utils.js`. Keeps the original public shape
 * (`getInvoke`, `toHandleId`, `normalizeHandleList`, `toHandleIdArray`,
 * `clamp`, `sleep`, `MENU_FLAGS`, `buildMenuItems`) so the tsup IIFE
 * bundle can install the module onto `window.smpUtils` without
 * forcing per-call adapters.
 */

import { formatHandleId } from './handleId.js';
import {
    SMP_MENU_FLAGS,
    type SmpHandleLike,
    type SmpMenuBuildState,
    type SmpRawMenuItem,
    type SmpStructuredMenuItem,
} from './types.js';

/** `window.fb2k.invoke`-shaped function; mirrored on `window.smp.invoke`. */
export type SmpInvokeFn = <TResp = unknown>(
    method: string,
    params?: JsonObject,
) => Promise<TResp>;

interface SmpHostShape {
    invoke?: SmpInvokeFn;
}

/**
 * Lazily resolve the `window.smp.invoke` function. Returns `null` when
 * the SMP bootstrap has not yet wired the host bridge — callers must
 * tolerate that case.
 */
export function getInvoke(): SmpInvokeFn | null {
    const smp = (globalThis as { smp?: SmpHostShape }).smp;
    const inv = smp?.invoke;
    return typeof inv === 'function' ? inv : null;
}

/**
 * Coerce a loose handle-like value to its canonical handle-id string.
 *
 * Accepts:
 * - bare strings (returned as-is, including any `|subsong:N` suffix);
 * - `FbMetadbHandle`-shaped objects (via `HandleId` getter);
 * - track-info objects with `Path` + optional `SubSong`;
 * - track-info objects with `absolutePath` / `path`.
 *
 * Returns `''` when no path-like field is present.
 */
export function toHandleId(handle: SmpHandleLike | unknown): string {
    if (!handle) return '';
    if (typeof handle === 'string') return handle;
    const h = handle as {
        HandleId?: unknown;
        Path?: unknown;
        SubSong?: unknown;
        absolutePath?: unknown;
        path?: unknown;
    };
    if (typeof h.HandleId === 'string') return h.HandleId;
    if (typeof h.Path === 'string') {
        const sub = typeof h.SubSong === 'number' ? h.SubSong : 0;
        return formatHandleId(h.Path, sub);
    }
    if (typeof h.absolutePath === 'string') return h.absolutePath;
    if (typeof h.path === 'string') return h.path;
    return '';
}

/**
 * Normalise any handle-list-like input into a plain array.
 *
 * Priority order:
 * 1. `Convert()` is honoured first — fast path for `FbMetadbHandleList`.
 * 2. `toArray()` is honoured next for ad-hoc collection wrappers.
 * 3. Plain `Array` short-circuits.
 * 4. Array-like with `length` (e.g. `arguments` / `NodeList`).
 *
 * Returns `[]` for unrecognised / falsy input.
 */
export function normalizeHandleList(
    handleList: unknown,
): Array<SmpHandleLike | unknown> {
    if (!handleList) return [];
    const obj = handleList as {
        Convert?: () => unknown[];
        toArray?: () => unknown[];
        length?: number;
        [index: number]: unknown;
    };
    if (typeof obj.Convert === 'function') return obj.Convert();
    if (typeof obj.toArray === 'function') return obj.toArray();
    if (Array.isArray(handleList)) return handleList as unknown[];
    if (typeof obj.length === 'number') {
        const result: unknown[] = [];
        for (let i = 0; i < obj.length; i++) result.push(obj[i]);
        return result;
    }
    return [];
}

/**
 * Compose {@link normalizeHandleList} + {@link toHandleId} into the
 * common "give me a flat string-id array" shape used by the SDK's
 * selection / clipboard helpers.
 */
export function toHandleIdArray(handleList: unknown): string[] {
    return normalizeHandleList(handleList).map(toHandleId).filter(Boolean);
}

/** Clamp `n` to `[min, max]`. */
export function clamp(n: number, min: number, max: number): number {
    return Math.min(max, Math.max(min, n));
}

/** Resolve after `ms` milliseconds (Promise-wrapped `setTimeout`). */
export function sleep(ms: number): Promise<void> {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

/** Re-export of the shared menu-flag bit constants. */
export const MENU_FLAGS = SMP_MENU_FLAGS;

/**
 * Recursive menu-item builder shared by `ContextMenuManager` and
 * `MainMenuManager`.
 *
 * - Walks `items` depth-first, allocating a new `id` from
 *   `state.nextId++` for each leaf command.
 * - Maps each allocated `id` to either the C++ `commandId` (context
 *   menus) or the `path` / `guid` string (main menu) inside
 *   `state.idMap` so callers can later dispatch via `ExecuteByID`.
 * - Stops once `state.limit` is hit (when set).
 *
 * Pure function: never mutates the input `items`.
 */
export function buildMenuItems(
    items: SmpRawMenuItem[] | undefined,
    state: SmpMenuBuildState,
): SmpStructuredMenuItem[] {
    const out: SmpStructuredMenuItem[] = [];
    if (!Array.isArray(items)) return out;

    for (const item of items) {
        if (state.limit !== null && state.nextId >= state.limit) break;
        if (!item || typeof item !== 'object') continue;

        const type = item.type ?? 'command';
        if (type === 'separator') {
            out.push({ type: 'separator' });
            continue;
        }

        if (type === 'submenu') {
            const children = buildMenuItems(item.children, state);
            if (children.length > 0) {
                out.push({
                    label: String(item.label ?? ''),
                    submenu: children,
                });
            }
            continue;
        }

        const menuId = state.nextId++;
        const flags = Number(item.flags) || 0;
        const checked = (flags & (MENU_FLAGS.checked | MENU_FLAGS.radiochecked)) !== 0;
        const enabled = (flags & MENU_FLAGS.disabled) === 0;

        out.push({
            id: menuId,
            label: String(item.label ?? ''),
            enabled,
            checked,
        });

        if (typeof item.commandId === 'number') {
            state.idMap.set(menuId, item.commandId);
        } else if (typeof item.path === 'string' && item.path.length > 0) {
            state.idMap.set(menuId, item.path);
        } else if (typeof item.guid === 'string' && item.guid.length > 0) {
            state.idMap.set(menuId, item.guid);
        }
    }

    return out;
}

/**
 * Aggregate `smpUtils` namespace object exported as a side-effect
 * value. The SMP bootstrap installs this onto `window.smpUtils` so
 * `<script>`-tag consumers see the same shape as the SMP runtime.
 */
export const smpUtils = {
    getInvoke,
    toHandleId,
    normalizeHandleList,
    toHandleIdArray,
    clamp,
    sleep,
    MENU_FLAGS,
    buildMenuItems,
} as const;

export type SmpUtilsNamespace = typeof smpUtils;
