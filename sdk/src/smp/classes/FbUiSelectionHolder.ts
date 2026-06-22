import type { JsonValue } from '../../types/json.js';
/**
 * `FbUiSelectionHolder` — UI selection holder wrapper.
 *
 * Surfaces the SMP API: explicitly stamp the active foobar2000 UI
 * selection (selection.set) and toggle which playlist event source
 * tracks the selection (`selection.setPlaylistTracking`).
 *
 * Backend mapping:
 * - `SetSelection(handles, type)`            → `selection.set`
 * - `SetPlaylistSelectionTracking()`         → `selection.setPlaylistTracking` (`{ mode: 'selection' }`)
 * - `SetPlaylistTracking()`                  → `selection.setPlaylistTracking` (`{ mode: 'playlist' }`)
 *
 * Returns a Promise that resolves with the C++ handler envelope.
 * Callers should always `await` the result because the host may
 * refuse the request asynchronously.
 */

import { getInvoke, toHandleIdArray } from '../utils.js';

interface SelectionResponse {
    success?: boolean;
    [key: string]: JsonValue;
}

export class FbUiSelectionHolder {
    /**
     * Replace the current UI selection.
     *
     * @param handleList Any value accepted by
     *                   {@link toHandleIdArray} (`FbMetadbHandleList`,
     *                   plain array, etc.).
     * @param type       Selection-type integer (defaults to 0 — generic).
     */
    async SetSelection(handleList: unknown, type?: number): Promise<boolean> {
        const inv = getInvoke();
        if (!inv) return false;

        const handles = toHandleIdArray(handleList);
        const res = (await inv('selection.set', {
            handles,
            type: typeof type === 'number' ? type : 0,
        })) as SelectionResponse | null;
        return !!res?.success;
    }

    /** Track current playlist's *selection* events. */
    async SetPlaylistSelectionTracking(): Promise<boolean> {
        const inv = getInvoke();
        if (!inv) return false;
        const res = (await inv('selection.setPlaylistTracking', {
            mode: 'selection',
        })) as SelectionResponse | null;
        return !!res?.success;
    }

    /** Track current playlist's *change* events. */
    async SetPlaylistTracking(): Promise<boolean> {
        const inv = getInvoke();
        if (!inv) return false;
        const res = (await inv('selection.setPlaylistTracking', {
            mode: 'playlist',
        })) as SelectionResponse | null;
        return !!res?.success;
    }
}
