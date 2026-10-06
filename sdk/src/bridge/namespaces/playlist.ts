/**
 * `playlist` — playlist management namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    PlaylistAddHandlesResponse,
    PlaylistAddPathsAsyncResponse,
    PlaylistAddPathsResponse,
    PlaylistAddPathsSequentialResponse,
    PlaylistAutoplaylistInfoResponse,
    PlaylistAvailableColumnsResponse,
    PlaylistClearResponse,
    PlaylistGroupRunsResponse,
    PlaylistInfo,
    PlaylistLockInfoResponse,
    PlaylistRemoveAutoplaylistResponse,
    PlaylistReorderPlaylistsResponse,
    PlaylistReorderResponse,
    PlaylistReplaceAllAndPlayResponse,
    PlaylistSelectedTracksResponse,
    PlaylistTrack,
    PlaylistTracksResponse,
    SelectionInfo,
    TrackInfo,
} from '../../types/responses.js';
import type {
    PlaylistCreateParams,
    PlaylistPlayTrackParams,
    PlaylistReplaceAllAndPlayParams,
} from '../../types/generated/params.js';

export const playlist = {
    // === Basic operations ===
    getAll: () => bridge.invoke<PlaylistInfo[]>('playlist.getAll'),
    getActive: () => bridge.invoke<PlaylistInfo | null>('playlist.getActive'),
    setActive: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.setActive', { playlist: index }),
    getPlaying: () => bridge.invoke<PlaylistInfo | null>('playlist.getPlaying'),
    /**
     * Fetch a slice of tracks from a playlist.
     *
     * The host returns a page envelope
     * `{ playlist, start, count, total, tracks }`; this wrapper unwraps
     * `tracks` and resolves with `PlaylistTrack[]` so callers can iterate
     * the result directly.
     *
     * Use {@link playlist.getTracksPage} when pagination metadata such as
     * `total` is needed.
     *
     * `fields` projects every row down to the requested keys plus `index`,
     * out of the same case-sensitive whitelist `library.query` uses, minus
     * `composer` and `comment`. Omitting the argument keeps the full row.
     * Projected rows are narrower than {@link PlaylistTrack} declares, so
     * read only the keys you asked for. A malformed list makes the host
     * resolve with `{ success: false, code: 'INVALID_PARAMS' }`, which has
     * no `tracks` to unwrap and therefore surfaces here as an empty array.
     * Use {@link playlist.getTracksPage} to inspect the error envelope.
     */
    getTracks: async (
        index: number,
        start?: number,
        count?: number,
        formats?: Record<string, string>,
        fields?: string[],
    ): Promise<PlaylistTrack[]> => {
        const response = await bridge.invoke<PlaylistTracksResponse>(
            'playlist.getTracks',
            {
                playlist: index,
                start,
                count,
                ...(formats && Object.keys(formats).length ? { formats } : {}),
                ...(fields !== undefined ? { fields } : {}),
            },
        );
        return Array.isArray(response?.tracks) ? response.tracks : [];
    },
    /**
     * Fetch a slice of tracks and keep the page envelope.
     *
     * Same request as {@link playlist.getTracks}, but resolves with
     * `{ playlist, start, count, total, tracks }` instead of unwrapping.
     * Compare `total` with {@link playlist.getGroupRuns}'s `total` for the
     * same playlist to detect a change in track count between the reads.
     * Equal totals do not rule out replacements or reordering: the calls
     * return independent snapshots, not a shared playlist revision.
     */
    getTracksPage: (
        index: number,
        start?: number,
        count?: number,
        formats?: Record<string, string>,
        fields?: string[],
    ) =>
        bridge.invoke<PlaylistTracksResponse>('playlist.getTracks', {
            playlist: index,
            start,
            count,
            ...(formats && Object.keys(formats).length ? { formats } : {}),
            ...(fields !== undefined ? { fields } : {}),
        }),
    /**
     * Group a whole playlist into consecutive runs and get back only the run
     * boundaries, never the rows.
     *
     * A run is a maximal stretch of adjacent tracks whose group key is equal,
     * compared case-insensitively over ASCII `A-Z`/`a-z`. Rows are never
     * reordered, so runs follow playlist order. Pass one Title Formatting
     * pattern, or two to sub-group within each run.
     *
     * Pair it with {@link playlist.getTracksPage} to drive a grouped virtual
     * list: the runs give header positions and total scroll height, each
     * visible page of rows is fetched separately.
     *
     * A pattern that groups every row on its own makes `runs` as long as
     * `total`. Payload size also depends on key lengths and second-level
     * runs. There is no cap; choose patterns that combine adjacent tracks.
     *
     * Malformed `patterns`, or a pattern that fails to compile, resolves with
     * `{ success: false, code: 'INVALID_PARAMS' }`; when one pattern is at
     * fault `details.pattern` carries its index.
     */
    getGroupRuns: (patterns: string[], index?: number) =>
        bridge.invoke<PlaylistGroupRunsResponse>('playlist.getGroupRuns', {
            patterns,
            ...(index !== undefined ? { playlist: index } : {}),
        }),
    getCount: (index: number) =>
        bridge.invoke<{ count: number }>('playlist.getTrackCount', {
            playlist: index,
        }),
    create: (name: string, options?: Omit<PlaylistCreateParams, 'name'>) =>
        bridge.invoke<{ index: number }>('playlist.create', {
            name,
            ...(options && typeof options === 'object' ? options : {}),
        }),
    remove: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.remove', { playlist: index }),
    rename: (index: number, name: string) =>
        bridge.invoke<BaseResponse>('playlist.rename', {
            playlist: index,
            name,
        }),
    duplicate: (index: number) =>
        bridge.invoke<{ index: number }>('playlist.duplicate', {
            playlist: index,
        }),
    clear: (index: number) =>
        bridge.invoke<PlaylistClearResponse>('playlist.clear', {
            playlist: index,
        }),

    // === Track addition ===
    /**
     * Add the given paths to the playlist. Each path/URL is capped at
     * 2048 chars (`INTERNET_MAX_URL_LENGTH`); over-length items are
     * silently skipped and counted in `invalidCount`.
     */
    add: (index: number, paths: string[]) =>
        bridge.invoke<PlaylistAddPathsResponse>('playlist.addPaths', {
            playlist: index,
            paths,
        }),
    /**
     * Asynchronously add the given paths to the playlist. Each path/URL
     * is capped at 2048 chars; over-length items are silently skipped
     * and counted in `invalidCount`.
     */
    addAsync: (index: number, paths: string[]) =>
        bridge.invoke<PlaylistAddPathsAsyncResponse>(
            'playlist.addPathsAsync',
            { playlist: index, paths },
        ),
    /**
     * Sequentially add the given paths to the playlist one-by-one. Each
     * path/URL is capped at 2048 chars; over-length items are silently
     * skipped.
     */
    addSequential: (index: number, paths: string[]) =>
        bridge.invoke<PlaylistAddPathsSequentialResponse>(
            'playlist.addPathsSequential',
            { playlist: index, paths },
        ),
    /**
     * Add tracks to the playlist via metadb handles. Each underlying
     * path is capped at 2048 chars; over-length items are silently
     * skipped and counted in `invalidCount`.
     */
    addHandles: (index: number, handles: unknown[]) =>
        bridge.invoke<PlaylistAddHandlesResponse>('playlist.addHandles', {
            playlist: index,
            handles,
        }),
    insertTracks: (index: number, insertIndex: number, handles: unknown[]) =>
        bridge.invoke<PlaylistAddHandlesResponse>('playlist.insertTracks', {
            playlist: index,
            position: insertIndex,
            handles,
        }),

    // === Track removal ===
    removeTracks: (index: number, indices: number[]) =>
        bridge.invoke<BaseResponse>('playlist.removeTracks', {
            playlist: index,
            items: indices,
        }),
    removeSelectedTracks: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.removeSelectedTracks', {
            playlist: index,
        }),

    // === Playback control ===
    playTrack: (
        index: number,
        trackIndex: number,
        options?: Omit<PlaylistPlayTrackParams, 'playlist' | 'track'>,
    ) =>
        bridge.invoke<BaseResponse>('playlist.playTrack', {
            playlist: index,
            index: trackIndex,
            ...options,
        }),

    // === Focused track ===
    getFocused: (index: number) =>
        bridge.invoke<{ index: number; track?: TrackInfo }>(
            'playlist.getFocusedTrack',
            { playlist: index },
        ),
    setFocused: (index: number, trackIndex: number) =>
        bridge.invoke<BaseResponse>('playlist.setFocusedTrack', {
            playlist: index,
            index: trackIndex,
        }),

    // === Selection ===
    getSelection: (index: number) =>
        bridge.invoke<SelectionInfo>('playlist.getSelection', {
            playlist: index,
        }),
    /**
     * Fetch the currently selected tracks of a playlist.
     *
     * Same envelope-unwrap pattern as {@link getTracks}; the C++ handler
     * returns `{ success, playlist, count, tracks }` and this wrapper
     * surfaces only `tracks` to keep the iteration ergonomic.
     */
    getSelectedTracks: async (index: number): Promise<PlaylistTrack[]> => {
        const response = await bridge.invoke<PlaylistSelectedTracksResponse>(
            'playlist.getSelectedTracks',
            { playlist: index },
        );
        return Array.isArray(response?.tracks) ? response.tracks : [];
    },
    setSelection: (index: number, indices: number[], clearOthers = true) =>
        bridge.invoke<BaseResponse>('playlist.setSelection', {
            playlist: index,
            indices,
            clearOthers,
        }),
    selectAll: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.selectAll', { playlist: index }),
    deselectAll: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.deselectAll', {
            playlist: index,
        }),

    // === Move and sort tracks ===
    moveTracks: (index: number, indices: number[], delta: number) =>
        bridge.invoke<BaseResponse>('playlist.moveTracks', {
            playlist: index,
            items: indices,
            delta,
        }),
    reorder: (index: number, order: number[]) =>
        bridge.invoke<PlaylistReorderResponse>('playlist.reorder', {
            playlist: index,
            newOrder: order,
        }),
    sort: (
        index: number,
        pattern: string,
        descending = false,
        selectedOnly = false,
    ) =>
        bridge.invoke<BaseResponse>('playlist.sort', {
            playlist: index,
            pattern,
            descending,
            selectedOnly,
        }),
    shuffle: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.shuffle', { playlist: index }),
    reverse: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.reverse', { playlist: index }),

    // === Undo and redo ===
    undo: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.undo', { playlist: index }),
    redo: (index: number) =>
        bridge.invoke<BaseResponse>('playlist.redo', { playlist: index }),

    // === Autoplaylist ===
    isAutoplaylist: (index: number) =>
        bridge.invoke<{ isAutoplaylist: boolean }>('playlist.isAutoplaylist', {
            playlist: index,
        }),
    getAutoplaylistInfo: (index: number) =>
        bridge.invoke<PlaylistAutoplaylistInfoResponse>(
            'playlist.getAutoplaylistInfo',
            { playlist: index },
        ),
    getAutoplaylistQuery: (index: number) =>
        bridge.invoke<{ query: string }>('playlist.getAutoplaylistQuery', {
            playlist: index,
        }),
    createAutoplaylist: (
        name: string,
        query: string,
        sort?: string,
        keepSorted?: boolean,
    ) =>
        bridge.invoke<{ index: number }>('playlist.createAutoplaylist', {
            name,
            query,
            sort,
            keepSorted: !!keepSorted,
        }),
    convertToAutoplaylist: (
        index: number,
        query: string,
        sort?: string,
        keepSorted?: boolean,
    ) =>
        bridge.invoke<BaseResponse>('playlist.convertToAutoplaylist', {
            playlist: index,
            query,
            sort,
            keepSorted: !!keepSorted,
        }),
    removeAutoplaylist: (index: number) =>
        bridge.invoke<PlaylistRemoveAutoplaylistResponse>(
            'playlist.removeAutoplaylist',
            { playlist: index },
        ),

    // === Lock state ===
    isLocked: (index: number) =>
        bridge.invoke<{ isLocked: boolean }>('playlist.isLocked', {
            playlist: index,
        }),
    getLockInfo: (index: number) =>
        bridge.invoke<PlaylistLockInfoResponse>('playlist.getLockInfo', {
            playlist: index,
        }),

    // === Playlist reordering ===
    reorderPlaylists: (order: number[]) =>
        bridge.invoke<PlaylistReorderPlaylistsResponse>(
            'playlist.reorderPlaylists',
            { newOrder: order },
        ),

    // === Advanced operations ===
    replaceAllAndPlay: (
        options: PlaylistReplaceAllAndPlayParams,
    ): Promise<PlaylistReplaceAllAndPlayResponse> =>
        bridge.invoke<PlaylistReplaceAllAndPlayResponse>(
            'playlist.replaceAllAndPlay',
            options,
        ),

    // === Column definitions ===
    getAvailableColumns: () =>
        bridge.invoke<PlaylistAvailableColumnsResponse>(
            'playlist.getAvailableColumns',
        ),

    // === Focused track (namespace supplement) ===
    focusTrack: (index: number, trackIndex: number) =>
        bridge.invoke<BaseResponse>('playlist.focusTrack', {
            playlist: index,
            index: trackIndex,
        }),
    getPlaylistCount: () =>
        bridge.invoke<{ count: number }>('playlist.getCount'),
    getFocusTrack: (index: number) =>
        bridge.invoke<{ index: number; track?: TrackInfo }>(
            'playlist.getFocusTrack',
            { playlist: index },
        ),
};
