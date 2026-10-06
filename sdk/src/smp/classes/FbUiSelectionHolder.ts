import type { JsonValue } from '../../types/json.js';
/**
 * `FbUiSelectionHolder` — UI selection holder wrapper.
 *
 * Surfaces the SMP API: explicitly stamp the active foobar2000 UI
 * selection (selection.set) or take it from the active playlist
 * (`selection.setPlaylistTracking`). The host holds a selection holder
 * only for the duration of each call, not for the life of this object.
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

import { getTypedInvoke, toHandleIdArray, successOf } from '../utils.js';

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
     * @param _type      Accepted for SMP compatibility and ignored; the host
     *                   has no selection types.
     */
    async SetSelection(handleList: unknown, _type?: number): Promise<boolean> {
        const inv = getTypedInvoke();
        if (!inv) return false;

        const handles = toHandleIdArray(handleList);
        const res = successOf(await inv('selection.set', { handles }));
        return !!res?.success;
    }

    /**
     * Set the selection to the active playlist's selected rows. Unlike SMP, the host takes them
     * once and does not keep tracking after the call.
     */
    async SetPlaylistSelectionTracking(): Promise<boolean> {
        const inv = getTypedInvoke();
        if (!inv) return false;
        const res = successOf(await inv('selection.setPlaylistTracking', {
            mode: 'selection',
        }));
        return !!res?.success;
    }

    /**
     * Set the selection to the whole active playlist. Unlike SMP, the host takes it once and does
     * not keep tracking after the call.
     */
    async SetPlaylistTracking(): Promise<boolean> {
        const inv = getTypedInvoke();
        if (!inv) return false;
        const res = successOf(await inv('selection.setPlaylistTracking', {
            mode: 'playlist',
        }));
        return !!res?.success;
    }
}
