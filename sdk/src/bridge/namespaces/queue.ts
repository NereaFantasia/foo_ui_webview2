/**
 * `queue` — foobar2000 play-queue namespace (not to be confused with
 * the just-in-time queue exposed via `jitQueue`).
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse, QueueGetResponse } from '../../types/responses.js';
import type {
    QueueAddParams,
    QueueAddPathsParams,
} from '../../types/generated/params.js';

export const queue = {
    get: () => bridge.invoke<QueueGetResponse>('queue.get'),
    getCount: () => bridge.invoke<{ count: number }>('queue.getCount'),
    add: (opts: QueueAddParams) =>
        bridge.invoke<
            BaseResponse & { addedCount?: number; queueCount?: number }
        >('queue.add', opts),
    /**
     * Add the given paths to the play queue. Each path/URL is capped at
     * 2048 chars; over-length items are silently skipped and counted in
     * `invalidCount`.
     */
    addPaths: (paths: string[], opts?: Omit<QueueAddPathsParams, 'paths'>) =>
        bridge.invoke<
            BaseResponse & {
                addedCount?: number;
                invalidCount?: number;
                isLocked?: boolean;
                queueCount?: number;
            }
        >('queue.addPaths', {
            paths,
            ...(opts || {}),
        }),
    remove: (index: number) =>
        bridge.invoke<
            BaseResponse & {
                removedIndex?: number;
                removedCount?: number;
                queueCount?: number;
            }
        >('queue.remove', { index }),
    moveToTop: (index: number) =>
        bridge.invoke<
            BaseResponse & { movedIndex?: number; queueCount?: number }
        >('queue.moveToTop', { index }),
    flush: () =>
        bridge.invoke<BaseResponse & { clearedCount?: number }>('queue.flush'),
    clear: () =>
        bridge.invoke<BaseResponse & { clearedCount?: number }>('queue.clear'),
};
