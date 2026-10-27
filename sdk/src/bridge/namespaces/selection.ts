import { call } from '../call.js';
import type {
    SelectionGetParams,
    SelectionGetViewingTrackParams,
    SelectionSetPlaylistTrackingParams,
} from '../../types/generated/params.js';

/** @deprecated Use {@link SelectionGetParams}. */
export type SelectionGetOptions = SelectionGetParams & {
    includeTrackInfo?: boolean;
};

/**
 * `selection` — current selection / viewer-mode namespace.
 */
export const selection = {
    /** Omitting `limit` caps the page at 100 and reports `truncated`; `limit: 0` returns everything. */
    get: (opts?: SelectionGetParams) =>
        call('selection.get', opts || {}),
    /** Report the type of the current global selection, as a numeric index and name. */
    getType: () => call('selection.getType'),
    /** Replace the global selection with the given track paths. Handles can have `|subsong:N` suffixes. */
    set: (handles: string[]) =>
        call('selection.set', {
            handles,
        }),
    /**
     * Takes the selection from the active playlist: its selected rows with `'selection'` (default),
     * all of it with `'playlist'`. Taken once; the host does not keep tracking after the call.
     */
    setPlaylistTracking: (mode: NonNullable<SelectionSetPlaylistTrackingParams['mode']> = 'selection') =>
        call('selection.setPlaylistTracking', {
            mode,
        }),
    /** Report which source the user's Selection Viewer preference favours (`prefer_playing` or `prefer_selection`). */
    getViewerMode: () =>
        call('selection.getViewerMode'),
    /** With `includeTrackInfo` the response carries the shared `Track` row as `track`. */
    getViewingTrack: (opts?: SelectionGetViewingTrackParams) =>
        call(
            'selection.getViewingTrack',
            opts || {},
        ),
};
