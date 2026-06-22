import type { JsonValue } from '../../types/json.js';
/**
 * `MainMenuManager` — Main-menu (File / Edit / Playback / ...) wrapper.
 *
 * Backend mapping:
 * - `BuildMenu(menu, base_id, max_id)` → `menu.getMainMenu` (`{ root }`)
 * - `ExecuteByID(id)`                  → `menu.runMainMenuCommand` (`{ command }`)
 *
 * The root sub-menu (`Init('File')`, `Init('Playback')`, ...) is set
 * once and forwarded to the host. Allocated ids map to either the
 * host-supplied `path` string or `guid`, both of which the C++
 * handler accepts as the `command` parameter.
 */

import { buildMenuItems, getInvoke } from '../utils.js';
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

        let command: number | string | null = null;
        if (typeof id === 'number') command = this._idMap.get(id) ?? null;
        else if (typeof id === 'string' && /^[0-9]+$/.test(id)) {
            command = this._idMap.get(Number(id)) ?? null;
        } else if (typeof id === 'string') {
            command = id;
        }

        if (!command) return false;

        const res = (await inv('menu.runMainMenuCommand', {
            command,
        })) as { success?: boolean } | null;
        return !!res?.success;
    }
}
