/**
 * `jitQueue` — just-in-time queue namespace.
 *
 * Distinct from {@link queue} (the foobar2000 play-queue): jitQueue
 * is the streaming pre-load queue for adaptive playback.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    JitQueueStateInfo,
} from '../../types/responses.js';
import type {
    JitQueueEnqueueNextParams,
    JitQueuePlayNowParams,
    JitQueuePreloadBatchParams,
} from '../../types/generated/params.js';

export const jitQueue = {
    getState: () =>
        bridge.invoke<JitQueueStateInfo>('jitQueue.getState'),
    /**
     * Enqueue the next track. URLs are capped at 2048 chars; over-length
     * URLs resolve with `{success:false, error:"URL exceeds maximum length (2048)"}`.
     */
    enqueueNext: (opts: JitQueueEnqueueNextParams) =>
        bridge.invoke<BaseResponse & { bufferSize?: number }>(
            'jitQueue.enqueueNext',
            opts,
        ),
    /**
     * Start playing the given track immediately. URLs are capped at 2048
     * chars; over-length URLs resolve with
     * `{success:false, error:"URL exceeds maximum length (2048)"}`.
     */
    playNow: (opts: JitQueuePlayNowParams) =>
        bridge.invoke<BaseResponse & { shadowPlaylist?: number }>(
            'jitQueue.playNow',
            opts,
        ),
    skip: () =>
        bridge.invoke<BaseResponse & { currentTrackId?: string }>(
            'jitQueue.skip',
        ),
    stop: () => bridge.invoke<BaseResponse>('jitQueue.stop'),
    clear: () => bridge.invoke<BaseResponse>('jitQueue.clear'),
    notifyEmpty: () => bridge.invoke<BaseResponse>('jitQueue.notifyEmpty'),
    /**
     * Batch-preload tracks. Each URL is capped at 2048 chars; over-length
     * URLs are silently skipped and counted in `invalidCount`.
     */
    preloadBatch: (opts: JitQueuePreloadBatchParams) =>
        bridge.invoke<BaseResponse & { tracksAdded?: number; invalidCount?: number }>(
            'jitQueue.preloadBatch',
            opts,
        ),
};
