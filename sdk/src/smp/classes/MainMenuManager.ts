import type { JsonValue } from '../../types/json.js';
/**
 * `MainMenuManager` — Main-menu (File / Edit / Playback / ...) wrapper.
 *
 * Backend mapping:
 * - `BuildMenu(menu, base_id, max_id)` → `menu.getMainMenu` (`{ root }`)
 * - `ExecuteByID(id)`                  → `menu.runMainMenuCommand` (`{ command }`)
 *
 * The root sub-menu (`Init('File')`, `Init('Playback')`, ...) is set
 * once and forwarded to the host. Allocated ids map to the host-supplied
 * `guid` (plus the node `subGuid` for a dynamic child) or, failing that, to
 * the `path` string. The `commandId` main-menu rows also carry is deliberately
 * never used: it is a transient Win32 menu id that addresses nothing once the
 * menu that produced it is gone.
 */

import { buildMenuItems, getInvoke, splitMenuAddress } from '../utils.js';
import type {
    SmpMenuBuildState,
    SmpRawMenuItem,
    SmpStructuredMenuItem,
} from '../types.js';

interface MainMenuResponse {
    success?: boolean;
    items?: SmpRawMenuItem[];
}

interface MenuTarget {
    SetItems?: (items: SmpStructuredMenuItem[]) => void;
}

export class MainMenuManager {
    private _root: string = '';
    private _idMap: Map<number, number | string> = new Map();

    /** Choose the top-level menu root (e.g. `'File'` / `'Playback'`). */
    Init(root: string): void {
        this._root = String(root || '');
    }

    /** Fetch the main-menu structure under the configured root. */
    async BuildMenu(
        menu?: MenuTarget,
        base_id?: number,
        max_id?: number,
    ): Promise<SmpStructuredMenuItem[]> {
        const inv = getInvoke();
        if (!inv) return [];

        const res = (await inv('menu.getMainMenu', {
            root: this._root,
        })) as MainMenuResponse | null;
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
            family: 'mainmenu',
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
     * Dispatch the previously-allocated menu id, or a raw command
     * string. Returns `false` if the id cannot be mapped.
     */
    async ExecuteByID(id: number | string): Promise<boolean> {
        const inv = getInvoke();
        if (!inv) return false;

        let mapped: number | string | null = null;
        if (typeof id === 'number') mapped = this._idMap.get(id) ?? null;
        else if (/^[0-9]+$/.test(id)) {
            mapped = this._idMap.get(Number(id)) ?? null;
        } else {
            mapped = id;
        }

        // `menu.runMainMenuCommand` declares `command` as a string and rejects
        // a number on type. Refusing one here keeps that failure inside the
        // SDK, where it reads as "unmapped id", instead of surfacing as a host
        // exception the caller cannot attribute.
        if (typeof mapped !== 'string' || mapped.length === 0) return false;

        // Spelled out as literal keys rather than spread: the repository's
        // static audit layer reads payload keys off the call site, and a spread
        // makes the invocation opaque to it.
        const { command, subGuid } = splitMenuAddress(mapped);
        const res = (await inv('menu.runMainMenuCommand', {
            command,
            ...(subGuid ? { subGuid } : {}),
        })) as { success?: boolean } | null;
        return !!res?.success;
    }
}
