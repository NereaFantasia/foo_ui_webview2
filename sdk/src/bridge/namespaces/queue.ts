/**
 * `queue` — foobar2000 play-queue namespace (not to be confused with
 * the just-in-time queue exposed via `jitQueue`).
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    QueueGetResponse,
    QueueInsertNextResponse,
    QueuePlayNowResponse,
    QueueSetContentsResponse,
} from '../../types/responses.js';
import type {
    QueueAddParams,
    QueueAddPathsParams,
} from '../../types/generated/params.js';

/**
 * A single entry accepted by {@link queue.setContents}.
 *
 * `{ queueIndex }` keeps (and repositions) a track already in the queue;
 * `{ playlist, item }` adds a playlist track that is not queued yet. A call
 * may freely mix both forms across its `items` array.
 */
export type QueueContentRef =
    | { queueIndex: number }
    | { playlist: number; item: number };

/**
 * The playlist-coordinate half of {@link QueueContentRef}: a track
 * addressed by its playlist index plus its item index inside that
 * playlist.
 *
 * A queue entry created from a coordinate keeps it, so playback continues
 * from that playlist position once the entry is consumed. Accepted by
 * {@link queue.insertNext} alongside plain paths.
 */
export type QueueListRef = Extract<QueueContentRef, { playlist: number }>;

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
    /**
     * Replace the entire queue with the given ordered list of references.
     *
     * Every entry must resolve to `{ queueIndex }` (keep/reorder an
     * existing queue slot) or `{ playlist, item }` (add a playlist track);
     * an unrecognized entry shape fails the whole call before anything is
     * written, unlike {@link queue.add}, which skips bad entries one at a
     * time. Passing an empty array clears the queue, equivalent to
     * {@link queue.clear}.
     *
     * `items.length` is capped at `max(256, current queue length)`;
     * exceeding the cap fails without changing the queue.
     *
     * Intended for a single commit at the end of a drag-to-reorder
     * gesture, not for per-frame calls while dragging — each call flushes
     * and rebuilds the queue from scratch.
     */
    setContents: (items: QueueContentRef[]) =>
        bridge.invoke<QueueSetContentsResponse>('queue.setContents', {
            items,
        }),
    /**
     * Insert tracks so they play next, ahead of everything already queued.
     *
     * An entry is either a path string (`path|subsong:N` accepted) or a
     * {@link QueueListRef} playlist coordinate, and one array may hold
     * both. The two forms travel as separate blocks: at the target
     * position the coordinate entries land first, then the path entries,
     * each block in the order it was given. A mixed array therefore loses
     * its interleaving — `[p1, I1, p2]` reaches the queue as `I1, p1, p2`.
     * To commit an exact interleaved order, call this and then
     * {@link queue.setContents} with the order wanted.
     *
     * An entry whose track is already queued moves to its new position
     * instead of being queued a second time, and only one queued entry
     * moves per track: for a coordinate, the entry holding that exact
     * coordinate if there is one, otherwise the first entry of the track,
     * which then takes the coordinate given; for a path, the first entry
     * of the track. Tracks not already queued are inserted as new entries.
     * Other queued copies remain in place. `position`
     * is the index within the queue *after* any moved entries have been
     * removed, and defaults to `0` (the very front).
     *
     * All input entries are de-duplicated by track. For duplicate
     * coordinates, the first one already present in the queue is retained;
     * if none matches, the first supplied coordinate is used. The track's
     * position within the input block remains its first occurrence.
     * Unlike {@link queue.add} and {@link queue.setContents}, two references
     * to one track therefore produce one entry. `insertedCount + movedCount`
     * can be below `entries.length` because of de-duplication or invalid
     * paths; a folder or container path can also expand to multiple tracks.
     *
     * A coordinate that does not address a queueable playlist item fails
     * the whole call before anything is written; the paths passed in the
     * same call are not queued either.
     *
     * The response separates `insertedCount` (new tracks), `movedCount`
     * (existing entries relocated). `invalidCount` is input path count minus
     * resolved track count, floored at zero. Folder or container expansion
     * can hide failed paths in that difference. Coordinates are excluded;
     * a bad coordinate fails the call outright.
     *
     * A newly queued path has no playlist coordinates, so playback cannot
     * continue from that track's position in a playlist. Moving an existing
     * entry by path preserves its usable coordinate. Pass a
     * {@link QueueListRef} to select a particular playlist position.
     *
     * A path that reached the queue through the `path|subsong:N` suffix
     * accepted by the older {@link queue.addPaths} call may fail to match
     * as "already queued" here; this only affects tracks queued before
     * upgrading, not new calls. A bare path to a multi-subsong file and
     * that same path with an explicit `|subsong:0` suffix are also treated
     * as distinct identities rather than equivalent.
     */
    insertNext: (entries: Array<string | QueueListRef>, position?: number) => {
        const paths = entries.filter(
            (entry): entry is string => typeof entry === 'string',
        );
        const items = entries.filter(
            (entry): entry is QueueListRef => typeof entry !== 'string',
        );
        return bridge.invoke<QueueInsertNextResponse>('queue.insertNext', {
            ...(paths.length > 0 ? { paths } : {}),
            ...(items.length > 0 ? { items } : {}),
            ...(position !== undefined ? { position } : {}),
        });
    },
    /**
     * Play the queue entry at `index` immediately, moving it to the front
     * of the queue first if it is not already there.
     *
     * Defaults to `index: 0` (the current queue head). Fails with
     * `"Queue is empty"` when nothing is queued, or `"Invalid queue index"`
     * for a negative, non-integer, or out-of-range value.
     *
     * `queueCount` is read immediately after playback starts; the host
     * does not guarantee that its consumption of the queue head happens
     * synchronously with that read, so the value may reflect the queue
     * either just before or just after the played entry is removed. Call
     * {@link queue.getCount} afterward if the exact post-play length
     * matters.
     */
    playNow: (index?: number) =>
        bridge.invoke<QueuePlayNowResponse>('queue.playNow', {
            ...(index !== undefined ? { index } : {}),
        }),
    flush: () =>
        bridge.invoke<BaseResponse & { clearedCount?: number }>('queue.flush'),
    clear: () =>
        bridge.invoke<BaseResponse & { clearedCount?: number }>('queue.clear'),
};
