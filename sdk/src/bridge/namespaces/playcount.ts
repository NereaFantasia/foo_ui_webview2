import { call } from '../call.js';

/**
 * `playcount` — play-count read/write namespace.
 *
 * `get(path)` is a convenience wrapper around the batch variant.
 */
export const playcount = {
    /**
     * The playcount entry of one track: `playcount.get` with `paths: [path]`,
     * so the entry is `results[0]`.
     */
    get: (path: string) => call('playcount.get', { paths: [path] }),
    /**
     * Batch fetch playcount entries for multiple tracks. Returns the full
     * envelope so callers can inspect the aggregate `count` field as well
     * as per-track `success` flags inside `results`.
     */
    getBatch: (paths: string[]) =>
        call('playcount.getBatch', {
            paths,
        }),
    /**
     * @deprecated foo_playcount does not currently expose a public API for
     * mutating playback statistics. The C++ handler is a placeholder that
     * always fails with `code: 'NOT_SUPPORTED'`. The
     * `count` argument is retained for signature compatibility but is
     * not sent to the host (the handler never reads it). Use
     * `fb.rating.set` for rating mutation, or trigger play counts via
     * actual playback.
     */
    set: (path: string, count: number) =>
        call('playcount.set', { path }),
    /** Aggregate library-wide playcount statistics. */
    getStats: () => call('playcount.getStats'),
};
