import type { JsonObject } from '../types/json.js';
/**
 * `smpUtils` — Shared helpers for the SMP wrapper layer.
 *
 * Exposes `getInvoke`, `toHandleId`, `normalizeHandleList`,
 * `toHandleIdArray`, `clamp`, `sleep`, `MENU_FLAGS`, and
 * `buildMenuItems` as a flat module shape, so the IIFE bundle can
 * install it onto `window.smpUtils` without per-call adapters.
 */

import { formatHandleId } from './handleId.js';
import {
    SMP_MENU_FLAGS,
    type SmpHandleLike,
    type SmpMenuBuildState,
    type SmpMenuFamily,
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
 * Joins a dynamic main-menu child's owning command GUID to its node GUID
 * inside a single `idMap` value. Safe as a delimiter because the host formats
 * GUIDs as `{8-4-4-4-12}`, which cannot contain it.
 */
const MENU_ADDRESS_SEPARATOR = '|';

/**
 * Split an `idMap` value produced for the `mainmenu` family back into the
 * parameters `menu.runMainMenuCommand` expects.
 *
 * @param value Encoded address, either `guid` or `guid|subGuid`.
 * @returns `command` always set; `subGuid` present only for dynamic children.
 */
export function splitMenuAddress(value: string): {
    command: string;
    subGuid?: string;
} {
    const at = value.indexOf(MENU_ADDRESS_SEPARATOR);
    if (at < 0) return { command: value };
    return {
        command: value.slice(0, at),
        subGuid: value.slice(at + MENU_ADDRESS_SEPARATOR.length),
    };
}

/**
 * Pick the identifier a menu item must be dispatched by, given its family.
 *
 * Each family maps only to what its host endpoint actually accepts, rather
 * than to whichever identifier happens to be present. An item routinely
 * carries several: main-menu rows arrive with a `commandId` even though
 * `menu.runMainMenuCommand` takes a string, and passing that number through
 * makes the host reject the call on type instead of running the command.
 *
 * @returns `null` when the item carries nothing this family can dispatch; the
 * caller then leaves the id unmapped so `ExecuteByID` reports failure rather
 * than sending an identifier the host cannot parse.
 */
function resolveMenuAddress(
    item: SmpRawMenuItem,
    family: SmpMenuFamily,
): number | string | null {
    if (family === 'contextmenu') {
        // Indexes the node tree of the manager instance that produced it, and
        // `menu.runContextCommandById` accepts nothing else.
        return typeof item.commandId === 'number' ? item.commandId : null;
    }

    if (typeof item.guid === 'string' && item.guid.length > 0) {
        // A dynamic child is addressed by the owning GUID *plus* its own node
        // GUID. Dispatching the owner alone runs the container slot, which the
        // host documents as undefined behaviour.
        return typeof item.subGuid === 'string' && item.subGuid.length > 0
            ? `${item.guid}${MENU_ADDRESS_SEPARATOR}${item.subGuid}`
            : item.guid;
    }

    // Path resolution is matched against a generated menu tree, which fails on
    // localized hosts, so it ranks below a GUID but above nothing at all.
    if (typeof item.path === 'string' && item.path.length > 0) return item.path;

    return null;
}

/**
 * Recursive menu-item builder shared by `ContextMenuManager` and
 * `MainMenuManager`.
 *
 * - Walks `items` depth-first, allocating a new `id` from
 *   `state.nextId++` for each leaf command.
 * - Maps each allocated `id` to the identifier `state.family` can actually
 *   dispatch, inside `state.idMap`, so callers can later dispatch via
 *   `ExecuteByID`. Items offering no such identifier stay unmapped.
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

        // Normalized booleans win over the raw flag word. Decoding flags alone
        // cannot express "state was not observed": an item the host never
        // evaluated arrives with flags == 0, which is bit-identical to a
        // genuinely enabled, unchecked one. Flags remain the fallback so an
        // older host that only sends them keeps working.
        const flags = Number(item.flags) || 0;
        const checked =
            typeof item.checked === 'boolean'
                ? item.checked
                : (flags & (MENU_FLAGS.checked | MENU_FLAGS.radiochecked)) !== 0;
        // An item whose state is explicitly unknown is offered as enabled: the
        // host decides at dispatch time, and greying out a row that is actually
        // invocable would hide a working command from the user.
        const enabled =
            item.stateKnown === false
                ? true
                : typeof item.enabled === 'boolean'
                  ? item.enabled
                  : (flags & MENU_FLAGS.disabled) === 0;

        out.push({
            id: menuId,
            label: String(item.label ?? ''),
            enabled,
            checked,
        });

        const address = resolveMenuAddress(item, state.family ?? 'mainmenu');
        if (address !== null) state.idMap.set(menuId, address);
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
    splitMenuAddress,
} as const;

export type SmpUtilsNamespace = typeof smpUtils;
