import type { JsonValue } from '../../types/json.js';
/**
 * `ContextMenuManager` — Context-menu builder for tracks / playlists.
 *
 * Backend mapping:
 * - `BuildMenu(menu, base_id, max_id)` → `menu.getContextMenu` (`{ mode, handles }`)
 * - `ExecuteByID(id)`                  → `menu.runContextCommandById` (`{ id, mode, handles }`)
 *
 * Three context modes mirror the SMP API:
 * - `InitContext(handles)`     → `mode = 'handles'`, attach handle ids.
 * - `InitContextPlaylist()`    → `mode = 'playlist'`, no handles.
 * - `InitNowPlaying()`         → `mode = 'nowPlaying'`, no handles.
 *
 * `ExecuteByID` also accepts a numeric C++ command id directly; this
 * keeps theme code that bypasses `BuildMenu` working.
 */

import { buildMenuItems, getInvoke, normalizeHandleList, toHandleId } from '../utils.js';
import type {
    SmpMenuBuildState,
    SmpRawMenuItem,
    SmpStructuredMenuItem,
} from '../types.js';

type ContextMode = 'handles' | 'playlist' | 'nowPlaying' | 'auto';

interface ContextMenuResponse {
    success?: boolean;
    items?: SmpRawMenuItem[];
    mode?: ContextMode;
}

interface MenuTarget {
    SetItems?: (items: SmpStructuredMenuItem[]) => void;
}

export class ContextMenuManager {
    private _mode: ContextMode = 'auto';
    private _effectiveMode: ContextMode = 'auto';
    private _handles: string[] = [];
    private _idMap: Map<number, number | string> = new Map();

    /** Configure the menu against an explicit handle list. */
    InitContext(handles: unknown): void {
        this._mode = 'handles';
        this._handles = normalizeHandleList(handles).map(toHandleId).filter(Boolean);
    }

    /** Configure the menu against the active playlist (no per-track scope). */
    InitContextPlaylist(): void {
        this._mode = 'playlist';
        this._handles = [];
    }

    /** Configure the menu against the now-playing track. */
    InitNowPlaying(): void {
        this._mode = 'nowPlaying';
        this._handles = [];
    }

    /**
     * Fetch the menu structure from the host and populate `menu.SetItems`
     * (when provided). Allocates ids starting at `base_id` (default 1)
     * and capped to `max_id` slots.
     *
     * @returns Structured menu tree (sub-menus / separators preserved).
     */
    async BuildMenu(
        menu?: MenuTarget,
        base_id?: number,
        max_id?: number,
    ): Promise<SmpStructuredMenuItem[]> {
        const inv = getInvoke();
        if (!inv) return [];

        const res = (await inv('menu.getContextMenu', {
            mode: this._mode,
            handles: this._handles,
        })) as ContextMenuResponse | null;

        this._effectiveMode = res?.mode ?? this._mode;

        const items = Array.isArray(res?.items) ? res!.items! : [];
        const baseId = (typeof base_id === 'number' ? base_id : 1) | 0;
        const limit =
            typeof max_id === 'number' && max_id > 0
                ? baseId + (max_id | 0)
                : null;

        this._idMap.clear();
        const state: SmpMenuBuildState = {
            nextId: baseId,
            limit,
            idMap: this._idMap,
        };
        const out = buildMenuItems(items, state);

        if (menu && typeof menu.SetItems === 'function') {
            try {
                menu.SetItems(out);
            } catch {
                /* ignore */
            }
        }

        return out;
    }

    /**
     * Dispatch the previously-allocated menu id (or a raw numeric C++
     * command id). Returns `false` when the command cannot be resolved.
     */
    async ExecuteByID(id: number | string): Promise<boolean> {
        const inv = getInvoke();
        if (!inv) return false;

        let cmdId: number | string | null = null;
        if (typeof id === 'number') cmdId = this._idMap.get(id) ?? null;
        else if (typeof id === 'string' && /^[0-9]+$/.test(id)) {
            cmdId = this._idMap.get(Number(id)) ?? null;
        }

        if (cmdId == null && typeof id === 'number') cmdId = id;
        if (cmdId == null) return false;

        const res = (await inv('menu.runContextCommandById', {
            id: cmdId,
            mode: this._effectiveMode || this._mode,
            handles: this._handles,
        })) as { success?: boolean } | null;
        return !!res?.success;
    }
}
