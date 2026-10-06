import { call } from '../call.js';
import type {
    JitQueueEnqueueNextParams,
    JitQueuePlayNowParams,
    JitQueuePreloadBatchParams,
    JitQueueStopParams,
} from '../../types/generated/params.js';

/**
 * `jitQueue` — just-in-time queue namespace.
 *
 * Distinct from {@link queue} (the foobar2000 play-queue): jitQueue
 * is the streaming pre-load queue for adaptive playback.
 */
export const jitQueue = {
    /** `shadowPlaylist` is `-1` until the shadow playlist has been created. */
    getState: () =>
        call('jitQueue.getState'),
    /**
     * Enqueue the next track. URLs are capped at 2048 chars; an over-length
     * URL fails with `INVALID_PARAMS`, and a session that is not playing
     * refuses with `NO_ACTIVE_ITEM`.
     */
    enqueueNext: (opts: JitQueueEnqueueNextParams) =>
        call(
            'jitQueue.enqueueNext',
            opts,
        ),
    /**
     * Start playing the given track immediately. URLs are capped at 2048
     * chars; an over-length URL fails with `INVALID_PARAMS`.
     */
    playNow: (opts: JitQueuePlayNowParams) =>
        call('jitQueue.playNow', opts),
    /** Refuses with `NO_ACTIVE_ITEM` while no JIT session is active. */
    skip: () => call('jitQueue.skip'),
    /** `clearBuffer` defaults to `true` on the host. */
    stop: (opts?: JitQueueStopParams) =>
        call('jitQueue.stop', opts || {}),
    clear: () => call('jitQueue.clear'),
    notifyEmpty: () =>
        call('jitQueue.notifyEmpty'),
    /**
     * Batch-preload tracks. Each URL is capped at 2048 chars; over-length
     * URLs are silently skipped and counted in `invalidCount`.
     */
    preloadBatch: (opts: JitQueuePreloadBatchParams) =>
        call(
            'jitQueue.preloadBatch',
            opts,
        ),
};
