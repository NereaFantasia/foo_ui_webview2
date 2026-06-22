/**
 * `playcount` — play-count read/write namespace.
 *
 * `get(path)` is a convenience wrapper around the batch variant.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    PlaycountGetResponse,
    PlaycountInfo,
    PlaycountStats,
} from '../../types/responses.js';

export const playcount = {
    /**
     * Fetch the playcount entry for a single track.
     *
     * Internally batches into the registered `playcount.get` handler
     * (which requires `paths: string[]`) and unwraps the first result.
     * Resolves to `null` when the host returns an empty / failed envelope.
     */
    get: async (path: string): Promise<PlaycountInfo | null> => {
        const r = await bridge.invoke<PlaycountGetResponse>('playcount.get', {
            paths: [path],
        });
        if (!r || r.success === false) return null;
        return r.results?.[0] ?? null;
    },
    /**
     * Batch fetch playcount entries for multiple tracks. Returns the full
     * envelope so callers can inspect the aggregate `count` field as well
     * as per-track `success` flags inside `results`.
     */
    getBatch: (paths: string[]) =>
        bridge.invoke<PlaycountGetResponse>('playcount.getBatch', { paths }),
    /**
     * @deprecated foo_playcount does not currently expose a public API for
     * mutating playback statistics. The C++ handler is a placeholder that
     * always responds with `{success:false, error:"Direct playcount
     * modification not supported. Use rating.set for ratings."}`. The
     * `count` argument is accepted for backwards compatibility but is
     * silently dropped on the host side. Use `fb.rating.set` for
     * rating mutation, or trigger play counts via actual playback.
     */
    set: (path: string, count: number) =>
        bridge.invoke<BaseResponse>('playcount.set', { path, count }),
    /** Aggregate library-wide playcount statistics. */
    getStats: () => bridge.invoke<PlaycountStats>('playcount.getStats'),
};
