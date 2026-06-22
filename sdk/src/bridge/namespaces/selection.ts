/**
 * `selection` — current selection / viewer-mode namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    SelectionGetResult,
    SelectionGetViewingTrackResponse,
} from '../../types/responses.js';
import type {
    SelectionGetParams,
    SelectionGetViewingTrackParams,
} from '../../types/generated/params.js';

/** @deprecated Use {@link SelectionGetParams}. */
export type SelectionGetOptions = SelectionGetParams & {
    includeTrackInfo?: boolean;
};

export const selection = {
    get: (opts?: SelectionGetParams) =>
        bridge.invoke<SelectionGetResult>('selection.get', opts || {}),
    getType: () => bridge.invoke<{ type: string }>('selection.getType'),
    set: (handles: unknown[]) =>
        bridge.invoke<BaseResponse & { count?: number }>(
            'selection.set',
            { handles },
        ),
    /** Tracking source: `'selection'` (default) or `'playlist'`. */
    setPlaylistTracking: (mode: 'selection' | 'playlist' = 'selection') =>
        bridge.invoke<BaseResponse>('selection.setPlaylistTracking', { mode }),
    getViewerMode: () => bridge.invoke<{ mode: string }>('selection.getViewerMode'),
    getViewingTrack: (opts?: SelectionGetViewingTrackParams) =>
        bridge.invoke<SelectionGetViewingTrackResponse>(
            'selection.getViewingTrack',
            opts || {},
        ),
};
