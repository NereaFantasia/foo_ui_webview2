import { call } from '../call.js';
import { playlistTarget, type PlaylistRef } from '../playlistRef.js';
import type {
    PlaylistHandleRef,
    PlaylistTrack,
    PlaylistTrackPartial,
} from '../../types/responses.js';
import type {
    PlaylistCreateParams,
    PlaylistPlayTrackParams,
    PlaylistReplaceAllAndPlayParams,
} from '../../types/generated/params.js';

/** Whether a playlist order names its playlists by GUID; an empty order is sent as indices. */
function isGuidOrder(order: readonly number[] | readonly string[]): order is readonly string[] {
    return typeof order[0] === 'string';
}

/** The `playlist.getTracks` request behind `getTracks` and `getTracksPage`. */
function getTracksPage(
    playlist: PlaylistRef,
    start?: number,
    count?: number,
    formats?: Record<string, string>,
    fields?: string[],
) {
    return call('playlist.getTracks', {
        ...playlistTarget(playlist),
        ...(start != null ? { start } : {}),
        ...(count != null ? { count } : {}),
        ...(formats && Object.keys(formats).length ? { formats } : {}),
        ...(fields != null ? { fields } : {}),
    });
}

/**
 * `playlist` — playlist management namespace.
 *
 * Methods that take a playlist accept a {@link PlaylistRef}: its index, or the `guid` from
 * `getAll`, which still names the same playlist after others are added, removed or reordered.
 */
export const playlist = {
    // === Basic operations ===
    /** Every playlist in playlist order, in `playlists`. */
    getAll: () => call('playlist.getAll'),
    /** Resolves with `found: false` and no other fields when there is no active playlist. */
    getActive: () => call('playlist.getActive'),
    setActive: (playlist: PlaylistRef) =>
        call('playlist.setActive', {
            ...playlistTarget(playlist),
        }),
    /** Resolves with `found: false` and no other fields when nothing is playing. */
    getPlaying: () => call('playlist.getPlaying'),
    /**
     * Fetch a slice of tracks from a playlist, as the page
     * `{ playlist, start, count, total, tracks }`.
     *
     * Without `fields` each row is a whole {@link PlaylistTrack}, with the
     * play statistics foo_playcount provides. `fields` narrows every row to
     * the requested keys plus `index`, out of the same case-sensitive list
     * `library.query` takes, which is why rows are typed as
     * `PlaylistTrackPartial`. `formats` maps column names to Title
     * Formatting patterns; each row carries their values under `formats`.
     * An unknown field name resolves with
     * `{ success: false, code: 'INVALID_PARAMS' }`.
     *
     * Compare `total` with {@link playlist.getGroupRuns}'s `total` for the
     * same playlist to detect a change in track count between the reads.
     * Equal totals do not rule out replacements or reordering: the calls
     * return independent snapshots, not a shared playlist revision.
     */
    getTracks: getTracksPage,
    /** @deprecated Use {@link playlist.getTracks}, which resolves with the same page. */
    getTracksPage,
    /**
     * Group a whole playlist into consecutive runs and get back only the run
     * boundaries, never the rows.
     *
     * A run is a maximal stretch of adjacent tracks whose group key is equal,
     * compared case-insensitively over ASCII `A-Z`/`a-z`. Rows are never
     * reordered, so runs follow playlist order. Pass one Title Formatting
     * pattern, or two to sub-group within each run.
     *
     * Pair it with {@link playlist.getTracks} to drive a grouped virtual
     * list: the runs give header positions and total scroll height, each
     * visible page of rows is fetched separately.
     *
     * A pattern that groups every row on its own makes `runs` as long as
     * `total`. Payload size also depends on key lengths and second-level
     * runs. There is no cap; choose patterns that combine adjacent tracks.
     *
     * Malformed `patterns`, or a pattern that fails to compile, resolves with
     * `{ success: false, code: 'INVALID_PARAMS' }`; for an empty pattern or
     * one that fails to compile, `details.pattern` carries its index. An
     * index past the last playlist resolves with `INVALID_INDEX`.
     */
    getGroupRuns: (patterns: string[], playlist?: PlaylistRef) =>
        call('playlist.getGroupRuns', {
            patterns,
            ...(playlist !== undefined ? playlistTarget(playlist) : {}),
        }),
    /**
     * The rows of a playlist whose tracks match a foobar2000 query, as
     * `{ playlist, playlistGuid, total, items, count }` with `items` ascending;
     * one call however long the playlist is. Read the rows with
     * {@link playlist.getTracksAt}.
     *
     * A query the parser rejects, or one carrying `SORT BY`, resolves with
     * `{ success: false, code: 'INVALID_PARAMS', details: { param: 'query' } }`;
     * the `error` text is in the host's language. A lowercase letter matches
     * either case in the tag, an uppercase one only the same case. The answer
     * is a snapshot: drop it on `playlist:itemsAdded`, `itemsRemoved`,
     * `itemsReordered`, `itemsReplaced` and `metadb:changed`. A query that
     * depends on the time (`DURING LAST`) also drifts with no event.
     *
     * @param playlist - Omitted, the active playlist.
     */
    getMatchingRows: (query: string, playlist?: PlaylistRef) =>
        call('playlist.getMatchingRows', {
            query,
            ...(playlist !== undefined ? playlistTarget(playlist) : {}),
        }),
    /**
     * Rows of a playlist picked by row number, as
     * `{ playlist, total, count, tracks }`, the rows in the order given. Rows
     * past the last one are skipped and every row carries its `index`, so
     * match rows by `index` rather than by position in `tracks`. `formats`
     * and `fields` work as in {@link playlist.getTracks}; a playlist that does
     * not exist answers an empty result, as there.
     */
    getTracksAt: (
        playlist: PlaylistRef,
        rows: readonly number[],
        formats?: Record<string, string>,
        fields?: string[],
    ) =>
        call('playlist.getTracksAt', {
            ...playlistTarget(playlist),
            rows: [...rows],
            ...(formats && Object.keys(formats).length ? { formats } : {}),
            ...(fields != null ? { fields } : {}),
        }),
    /**
     * Number of tracks in `playlist`, sent as the host's
     * `playlist.getTrackCount`; the envelope carries it in `count`. Resolves
     * with `count` `0` when the playlist does not exist. The number of
     * playlists is {@link playlist.getPlaylistCount}, which is what the host's
     * `playlist.getCount` returns.
     */
    getCount: (playlist: PlaylistRef) =>
        call('playlist.getTrackCount', {
            ...playlistTarget(playlist),
        }),
    create: (name: string, options?: Omit<PlaylistCreateParams, 'name'>) =>
        call('playlist.create', {
            name,
            ...(options && typeof options === 'object' ? options : {}),
        }),
    remove: (playlist: PlaylistRef) =>
        call('playlist.remove', {
            ...playlistTarget(playlist),
        }),
    rename: (playlist: PlaylistRef, name: string) =>
        call('playlist.rename', {
            ...playlistTarget(playlist),
            name,
        }),
    /** `name` omitted or empty, the copy is named after the original with ` (Copy)` appended. */
    duplicate: (playlist: PlaylistRef, name?: string) =>
        call('playlist.duplicate', {
            ...playlistTarget(playlist),
            ...(name !== undefined ? { name } : {}),
        }),
    clear: (playlist: PlaylistRef) =>
        call('playlist.clear', {
            ...playlistTarget(playlist),
        }),

    // === Track addition ===
    /**
     * Append files, folders or URLs. foobar2000 resolves the batch as its own
     * Add Files does, so the rows do not keep the given order; use
     * {@link playlist.addSequential} for that. Entries over 2048 characters and
     * entries that resolve to nothing are counted in `invalidCount`; when
     * nothing is added the call fails with `NOT_FOUND`.
     */
    add: (playlist: PlaylistRef, paths: string[]) =>
        call('playlist.addPaths', {
            ...playlistTarget(playlist),
            paths,
        }),
    /**
     * Start adding paths without waiting; `playlist:addComplete` with the
     * returned `operationId` reports the outcome. Empty entries and entries
     * over 2048 characters are counted in `invalidCount`.
     */
    addAsync: (playlist: PlaylistRef, paths: string[]) =>
        call('playlist.addPathsAsync', {
            ...playlistTarget(playlist),
            paths,
        }),
    /**
     * Append paths in the given order and report the row of each added track.
     * Entries over 2048 characters and entries that resolve to nothing are
     * skipped.
     */
    addSequential: (playlist: PlaylistRef, paths: string[]) =>
        call('playlist.addPathsSequential', {
            ...playlistTarget(playlist),
            paths,
        }),
    /**
     * Append tracks by path or `{ path, subsong }`. Unusable entries are counted
     * in `invalidCount`; when none is usable the call fails with `NOT_FOUND`.
     */
    addHandles: (playlist: PlaylistRef, handles: PlaylistHandleRef[]) =>
        call('playlist.addHandles', {
            ...playlistTarget(playlist),
            handles,
        }),
    /** Insert tracks before row `insertIndex`; past the last row, they are appended. */
    insertTracks: (playlist: PlaylistRef, insertIndex: number, handles: PlaylistHandleRef[]) =>
        call('playlist.insertTracks', {
            ...playlistTarget(playlist),
            position: insertIndex,
            handles,
        }),

    // === Track removal ===
    removeTracks: (playlist: PlaylistRef, indices: number[]) =>
        call('playlist.removeTracks', {
            ...playlistTarget(playlist),
            items: indices,
        }),
    removeSelectedTracks: (playlist: PlaylistRef) =>
        call('playlist.removeSelectedTracks', {
            ...playlistTarget(playlist),
        }),

    // === Playback control ===
    playTrack: (
        playlist: PlaylistRef,
        trackIndex: number,
        options?: Omit<PlaylistPlayTrackParams, 'playlist' | 'playlistGuid' | 'index'>,
    ) =>
        call('playlist.playTrack', {
            ...playlistTarget(playlist),
            index: trackIndex,
            ...options,
        }),

    // === Focused track ===
    /** `index` is `-1` when the playlist has no focus or does not exist. */
    getFocused: (playlist: PlaylistRef) =>
        call('playlist.getFocusedTrack', {
            ...playlistTarget(playlist),
        }),
    /** A negative `trackIndex` removes the focus. */
    setFocused: (playlist: PlaylistRef, trackIndex: number) =>
        call('playlist.setFocusedTrack', {
            ...playlistTarget(playlist),
            index: trackIndex,
        }),

    // === Selection ===
    getSelection: (playlist: PlaylistRef) =>
        call('playlist.getSelection', {
            ...playlistTarget(playlist),
        }),
    /**
     * The selected tracks of a playlist, as `{ playlist, count, tracks }`.
     * Rows are whole {@link PlaylistTrack}s without the play statistics. An
     * empty selection succeeds with an empty `tracks`; a playlist that does
     * not exist resolves with a failure envelope.
     */
    getSelectedTracks: (playlist: PlaylistRef) =>
        call('playlist.getSelectedTracks', playlistTarget(playlist)),
    /** Rows past the last one are ignored; a negative row fails with `INVALID_PARAMS`. */
    setSelection: (playlist: PlaylistRef, indices: number[], clearOthers = true) =>
        call('playlist.setSelection', {
            ...playlistTarget(playlist),
            indices,
            clearOthers,
        }),
    selectAll: (playlist: PlaylistRef) =>
        call('playlist.selectAll', {
            ...playlistTarget(playlist),
        }),
    deselectAll: (playlist: PlaylistRef) =>
        call('playlist.deselectAll', {
            ...playlistTarget(playlist),
        }),

    // === Move and sort tracks ===
    /**
     * Replace the selection with `indices`, then move the selection by `delta`
     * rows; the caller's own selection is lost. An empty `indices` moves the
     * current selection.
     */
    moveTracks: (playlist: PlaylistRef, indices: number[], delta: number) =>
        call('playlist.moveTracks', {
            ...playlistTarget(playlist),
            items: indices,
            delta,
        }),
    reorder: (playlist: PlaylistRef, order: number[]) =>
        call('playlist.reorder', {
            ...playlistTarget(playlist),
            newOrder: order,
        }),
    /**
     * `descending` reverses the whole playlist after sorting, the unselected
     * tracks too when `selectedOnly` is set.
     */
    sort: (
        playlist: PlaylistRef,
        pattern: string,
        descending = false,
        selectedOnly = false,
    ) =>
        call('playlist.sort', {
            ...playlistTarget(playlist),
            pattern,
            descending,
            selectedOnly,
        }),
    shuffle: (playlist: PlaylistRef) =>
        call('playlist.shuffle', {
            ...playlistTarget(playlist),
        }),
    reverse: (playlist: PlaylistRef) =>
        call('playlist.reverse', {
            ...playlistTarget(playlist),
        }),

    // === Undo and redo ===
    /** Fails with `NOT_FOUND` when there is nothing to undo. */
    undo: (playlist: PlaylistRef) =>
        call('playlist.undo', {
            ...playlistTarget(playlist),
        }),
    /** Fails with `NOT_FOUND` when there is nothing to redo. */
    redo: (playlist: PlaylistRef) =>
        call('playlist.redo', {
            ...playlistTarget(playlist),
        }),

    // === Autoplaylist ===
    isAutoplaylist: (playlist: PlaylistRef) =>
        call('playlist.isAutoplaylist', {
            ...playlistTarget(playlist),
        }),
    getAutoplaylistInfo: (playlist: PlaylistRef) =>
        call('playlist.getAutoplaylistInfo', {
            ...playlistTarget(playlist),
        }),
    /** `query` is always `null`: foobar2000 does not expose an autoplaylist's query. */
    getAutoplaylistQuery: (playlist: PlaylistRef) =>
        call('playlist.getAutoplaylistQuery', {
            ...playlistTarget(playlist),
        }),
    createAutoplaylist: (
        name: string,
        query: string,
        sort?: string,
        keepSorted?: boolean,
    ) =>
        call('playlist.createAutoplaylist', {
            name,
            query,
            ...(sort !== undefined ? { sort } : {}),
            keepSorted: !!keepSorted,
        }),
    convertToAutoplaylist: (
        playlist: PlaylistRef,
        query: string,
        sort?: string,
        keepSorted?: boolean,
    ) =>
        call('playlist.convertToAutoplaylist', {
            ...playlistTarget(playlist),
            query,
            ...(sort !== undefined ? { sort } : {}),
            keepSorted: !!keepSorted,
        }),
    removeAutoplaylist: (playlist: PlaylistRef) =>
        call('playlist.removeAutoplaylist', {
            ...playlistTarget(playlist),
        }),

    // === Lock state ===
    isLocked: (playlist: PlaylistRef) =>
        call('playlist.isLocked', {
            ...playlistTarget(playlist),
        }),
    getLockInfo: (playlist: PlaylistRef) =>
        call('playlist.getLockInfo', {
            ...playlistTarget(playlist),
        }),

    // === Playlist reordering ===
    /**
     * Reorder the playlist list. `order[i]` names the playlist that moves to
     * position `i`, every playlist exactly once: all current indices, sent as
     * `newOrder`, or all `guid`s from {@link playlist.getAll}, sent as
     * `newOrderGuids`. Indices are read as the call arrives, so an order built
     * from an earlier `getAll` silently moves the wrong playlists once another
     * one was added and removed in between; GUIDs keep naming the playlists
     * they were read from and fail with `NOT_FOUND` for one that is gone.
     */
    reorderPlaylists: (order: readonly number[] | readonly string[]) =>
        call(
            'playlist.reorderPlaylists',
            isGuidOrder(order) ? { newOrderGuids: [...order] } : { newOrder: [...order] },
        ),

    // === Advanced operations ===
    /**
     * Stop, clear, add the paths, activate the playlist and play or focus
     * `playIndex`. When nothing could be added the call fails with `NOT_FOUND`
     * and the playlist stays empty.
     */
    replaceAllAndPlay: (options: PlaylistReplaceAllAndPlayParams) =>
        call('playlist.replaceAllAndPlay', options),

    // === Column definitions ===
    /** The Default UI playlist columns, in `columns`. */
    getAvailableColumns: () => call('playlist.getAvailableColumns'),

    // === Focused track (namespace supplement) ===
    /** @deprecated Use {@link playlist.setFocused}. */
    focusTrack: (playlist: PlaylistRef, trackIndex: number) =>
        call('playlist.focusTrack', {
            ...playlistTarget(playlist),
            index: trackIndex,
        }),
    /**
     * Number of playlists, sent as the host's `playlist.getCount`; the
     * envelope carries it in `count`. For the number of tracks in one
     * playlist use {@link playlist.getCount}.
     */
    getPlaylistCount: () => call('playlist.getCount'),
    /** @deprecated Use {@link playlist.getFocused}. */
    getFocusTrack: (playlist: PlaylistRef) =>
        call('playlist.getFocusTrack', {
            ...playlistTarget(playlist),
        }),
};
