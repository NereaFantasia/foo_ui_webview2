import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import { playlistTarget, type PlaylistRef } from '../playlistRef.js';
import type {
    LibraryDirectoryBatch,
    LibraryEnumerateDirectoriesOptions,
    LibraryEnumerateDirectoriesSummary,
    LibraryEnumerateFieldValuesOptions,
    LibraryEnumerateTracksOptions,
    LibraryEnumerateTracksPage,
    LibraryEnumerateTracksSummary,
    LibraryEnumerateTreeOptions,
    LibraryEnumerateTreeSummary,
    ApiFailure,
    LibraryPagedTracksResponse,
    LibraryTrack,
    LibraryTreeBatch,
} from '../../types/responses.js';
import type {
    LibraryBrowseTreeParams,
    LibraryGetAlbumsParams,
    LibraryGetArtistAlbumsParams,
    LibraryGetAllParams,
    LibraryGetArtistsParams,
    LibraryGetRecentlyAddedParams,
    LibrarySearchParams,
} from '../../types/generated/params.js';
import type { LibraryGetAllResultPayload } from '../../types/events.js';

/** @deprecated Use `Omit<LibrarySearchParams, 'query' | 'limit'>`. */
export type LibrarySearchOptions = Omit<LibrarySearchParams, 'query' | 'limit'>;

/** @deprecated Use `LibraryGetAlbumsParams`. */
export type LibraryGetAlbumsOptions = Pick<LibraryGetAlbumsParams, 'limit'>;

/** Parameters of {@link library.browseTree}; the generated declaration type. */
export type { LibraryBrowseTreeParams };

/**
 * `library` — media-library namespace.
 *
 * Combines low-level invokes (search / paging / tag aggregation) with
 * three high-level async generators that walk the library:
 *
 * - {@link library.enumerateTracks}      — paged scan of every track.
 * - {@link library.enumerateDirectories} — BFS/DFS over filesystem paths.
 * - {@link library.enumerateTree}        — root-aware BFS/DFS over the
 *                                          typed library tree.
 */
export const library = {
    // ── Search / aggregation ────────────────────────────────────────────
    /**
     * Run a foobar2000 query expression against the media library.
     *
     * `options.offset` / `limit` page the hit list; `total` and `hasMore`
     * report the full extent. `options.fields` narrows each row to the named
     * keys of a library track row, which is why rows are typed as
     * `LibraryTrackPartial`. An unknown name resolves — never rejects — with
     * `{ success: false, code: 'INVALID_PARAMS' }` and the names under
     * `details.unknownFields`.
     */
    search: (
        query: string,
        limit?: number,
        options?: Omit<LibrarySearchParams, 'query' | 'limit'>,
    ) =>
        call('library.search', {
            query,
            ...(limit != null ? { limit } : {}),
            ...(options && typeof options === 'object' ? options : {}),
        }),
    getAlbums: (options?: number | LibraryGetAlbumsParams) =>
        call(
            'library.getAlbums',
            (typeof options === 'number'
                ? { limit: options }
                : { ...(options || {}) }),
        ),
    /**
     * List credited artists with per-artist aggregates.
     *
     * `options.includeAlbums` adds an `albums` array to every row — the
     * `(name, artist)` identities of the albums that artist is credited on,
     * so an "artist → albums" section can be built from one call instead of
     * one `getArtistAlbums` scan per artist. `limit` caps the rows, never
     * the `albums` inside them.
     */
    getArtists: (
        limit?: number,
        options?: Omit<LibraryGetArtistsParams, 'limit'>,
    ) =>
        call('library.getArtists', {
            limit,
            ...(options && typeof options === 'object' ? options : {}),
        }),
    getGenres: () => call('library.getGenres'),
    getStats: () => call('library.getStats'),
    getStatus: () => call('library.getStatus'),
    getCount: () => call('library.getCount'),
    /**
     * Fetch library tracks: `count` tracks from position `start`, sent as the
     * host's `offset` and `limit` (host defaults `0` and `100`). Resolves
     * synchronously for paged requests and cache hits.
     *
     * Options:
     * - `useCache` — from `start` 0, answer from the list the host kept after
     *   an earlier request from 0 that covered every track. Sent only when
     *   given; the host default is `true`. A request from 0 covering every
     *   track is kept either way.
     * - `asyncResult` — let the host build a request from 0 that covers every
     *   track off the main thread. Defaults to `true` here (the host's own
     *   default is `false`); it takes effect only with `useCache` and not when
     *   the kept list answers. The host then answers `{ pending: true,
     *   requestId }` and delivers the page as the `library:getAllResult`
     *   event; this wrapper listens for that event before sending the call,
     *   so an early delivery is not missed, and resolves with the page.
     *   Pass `false` to have the host build the page on the main thread.
     * - `timeout` — milliseconds to wait for the event, default 60 000;
     *   `0` or less waits indefinitely.
     *
     * Either way it resolves with a {@link LibraryPagedTracksResponse} or an
     * `ApiFailure`, so callers are unaffected by the threading model. A
     * failure envelope from the call resolves as it is. A list the host failed
     * to build and a timeout both resolve with `OPERATION_FAILED`;
     * `details.requestId` names the request, and a timeout also carries
     * `details.timeoutMs`. The returned promise rejects only when the call
     * itself rejects. The event listener is removed in every case.
     */
    getAll: async (
        start?: number,
        count?: number,
        opts: Pick<LibraryGetAllParams, 'useCache' | 'asyncResult'> & {
            timeout?: number;
        } = {},
    ): Promise<LibraryPagedTracksResponse | ApiFailure> => {
        const asyncResult = opts.asyncResult !== false;
        const params = {
            ...(start != null ? { offset: start } : {}),
            ...(count != null ? { limit: count } : {}),
            ...(opts.useCache != null ? { useCache: opts.useCache } : {}),
            asyncResult,
        } satisfies LibraryGetAllParams;

        if (!asyncResult) {
            const direct = await call('library.getAll', params);
            return direct.success === false ? direct : (direct as LibraryPagedTracksResponse);
        }

        // The worker can post the page before the pending answer is handled,
        // so listen first and hold what arrives until the request id is known.
        const early: LibraryGetAllResultPayload[] = [];
        let expectedId: string | undefined;
        let deliver: ((e: LibraryGetAllResultPayload) => void) | undefined;
        const off = subscribe('library:getAllResult', (e: LibraryGetAllResultPayload) => {
            if (!deliver) {
                early.push(e);
                return;
            }
            if (e?.requestId === expectedId) deliver(e);
        });

        const result = await call('library.getAll', params).catch((err: unknown) => {
            off();
            throw err;
        });

        if (result.success === false || result.pending !== true || !result.requestId) {
            off();
            // A synchronous page carries the page fields; only a pending answer lacks them.
            return result.success === false ? result : (result as LibraryPagedTracksResponse);
        }

        const requestId = result.requestId;
        const timeoutMs = opts.timeout ?? 60000;
        return new Promise<LibraryPagedTracksResponse | ApiFailure>((resolve) => {
            let timer: ReturnType<typeof setTimeout> | null = null;
            const settle = (e: LibraryGetAllResultPayload): void => {
                off();
                if (timer) {
                    clearTimeout(timer);
                    timer = null;
                }
                if (e.error) {
                    resolve({
                        success: false,
                        error: e.error,
                        code: 'OPERATION_FAILED',
                        details: { requestId },
                    });
                    return;
                }
                resolve({ ...e, success: true });
            };
            const held = early.find((e) => e?.requestId === requestId);
            early.length = 0;
            if (held) {
                settle(held);
                return;
            }
            expectedId = requestId;
            deliver = settle;
            if (timeoutMs > 0) {
                timer = setTimeout(() => {
                    off();
                    timer = null;
                    resolve({
                        success: false,
                        error: `library.getAll timed out after ${timeoutMs} ms`,
                        code: 'OPERATION_FAILED',
                        details: { requestId, timeoutMs },
                    });
                }, timeoutMs);
            }
        });
    },
    refresh: () => call('library.refresh'),
    getByPath: (path: string) =>
        call('library.getByPath', {
            path,
        }),
    /**
     * Append `paths` to a playlist, by default the active one. Pass the playlist's `guid` rather
     * than its index when the playlist list may change between choosing the target and the call,
     * for example while a menu is open.
     */
    addToPlaylist: (paths: string[], playlist?: PlaylistRef) =>
        call('library.addToPlaylist', {
            paths,
            ...(playlist != null ? playlistTarget(playlist) : {}),
        }),

    // ── Roots / typed tree ──────────────────────────────────────────────
    getRoots: () => call('library.getRoots'),
    browseTree: (params: LibraryBrowseTreeParams) =>
        call('library.browseTree', {
            rootId: params.rootId,
            ...(params.pathId != null ? { pathId: params.pathId } : {}),
            ...(params.includeFiles != null
                ? { includeFiles: params.includeFiles }
                : {}),
            ...(params.recursiveFiles != null
                ? { recursiveFiles: params.recursiveFiles }
                : {}),
        }),

    // ── Filesystem-based directory listing ──────────────────────────────
    browseDirectory: (path: string, includeFiles?: boolean) =>
        call(
            'library.browseDirectory',
            {
                path,
                ...(includeFiles != null ? { includeFiles } : {}),
            },
        ),
    /**
     * Tracks of one album from {@link getAlbums}, sorted by disc number, then
     * track number, then library order.
     *
     * Pass the row's `name` and `albumArtist` as they are: both are compared
     * byte for byte, and `total` then equals the row's `trackCount`. The row's
     * `artist` is not the same key, and an album artist that is `""` must be
     * passed as `""`. A pair no row has resolves with no tracks and no `row`.
     * The grouping is kept by the host until the library changes, so repeated
     * calls do not rescan the library.
     *
     * @example
     *   const page = await fb.library.getAlbums({ limit: 1 });
     *   const album = page.success ? page.albums[0] : undefined;
     *   if (album) {
     *       const res = await fb.library.getAlbumTracks(album.name, album.albumArtist);
     *   }
     */
    getAlbumTracks: (album: string, albumArtist: string) =>
        call('library.getAlbumTracks', { album, albumArtist }),
    /**
     * Albums an artist appears on, as rows shaped like `library.getAlbums`.
     *
     * `artist` is compared byte for byte against each atomic tag value, so a
     * name taken from `getArtists` also matches tracks where it is not the
     * first of several credited artists, and an artist differing only in case
     * is never pulled in. Pass one artist name: on a multi-value track the
     * joined `track.artist` string matches nothing, and on a single-value
     * track it happens to equal the atomic value, so it is not a key you can
     * rely on. Pass `match: 'substring'` to match on containment instead,
     * which also returns albums by any other artist whose name contains the
     * argument; that mode is looser about case, matching either case wherever
     * the argument is lowercase.
     *
     * `trackCount`, `duration` and `discCount` count only the tracks this
     * artist appears on, not the whole album — `getAlbums` returns the
     * album-wide figures for the same row. Rendering a row here as an album
     * card will therefore understate it.
     *
     * Rows are grouped by album name alone, so identically titled albums by
     * different artists collapse into one row; tracks whose `album` tag is
     * missing are grouped under `(Unknown Album)` rather than dropped. Both
     * differ from `getAlbums`, which keys on album plus album artist and skips
     * tracks with no album. Cover art is not inlined: pass
     * `firstTrackAbsolutePath` to `artwork.getForTrack`, since
     * `firstTrackPath` can be a `file-relative://` URI that endpoint refuses.
     *
     * There is no `offset`: when `hasMore` is true the only way to see the
     * rest is a larger `limit`.
     */
    getArtistAlbums: (
        artist: string,
        limit?: number,
        options?: Omit<LibraryGetArtistAlbumsParams, 'artist' | 'limit'>,
    ) =>
        call('library.getArtistAlbums', {
            artist,
            ...(limit != null ? { limit } : {}),
            ...(options && typeof options === 'object' ? options : {}),
        }),
    /**
     * Tracks an artist appears on.
     *
     * `artist` is matched exactly as in `getArtistAlbums` under
     * `match: 'exact'`: compared byte for byte against each atomic tag value,
     * so a name from `getArtists` matches even when it is not the first of
     * several credited artists, and an artist differing only in case is never
     * pulled in.
     */
    getArtistTracks: (artist: string, limit?: number) =>
        call('library.getArtistTracks', {
            artist,
            ...(limit != null ? { limit } : {}),
        }),
    getCacheStats: () =>
        call('library.getCacheStats'),
    getFieldValues: (field: string, limit?: number, separator?: string) =>
        call('library.getFieldValues', {
            field,
            ...(limit != null ? { limit } : {}),
            ...(separator ? { separator } : {}),
        }),
    /** Semantic alias for {@link library.getFieldValues}. */
    enumerateFieldValues: (
        field: string,
        options: LibraryEnumerateFieldValuesOptions = {},
    ) =>
        call('library.getFieldValues', {
            field,
            ...(options?.limit != null ? { limit: options.limit } : {}),
            ...(options?.separator ? { separator: options.separator } : {}),
        }),

    // ── Async generators: walk the entire library / tree ────────────────

    /**
     * Async generator that pages through every track in the library.
     * Honours `signal` for cooperative cancellation and emits a
     * progress callback after each page fetch.
     */
    enumerateTracks: async function* (
        options: LibraryEnumerateTracksOptions = {},
    ): AsyncGenerator<
        LibraryEnumerateTracksPage,
        LibraryEnumerateTracksSummary,
        void
    > {
        const pageSizeRaw =
            options?.pageSize != null ? Number(options.pageSize) : 500;
        const pageSize =
            Number.isFinite(pageSizeRaw) && pageSizeRaw > 0
                ? Math.floor(pageSizeRaw)
                : 500;
        const startRaw = options?.start != null ? Number(options.start) : 0;
        let offset =
            Number.isFinite(startRaw) && startRaw >= 0
                ? Math.floor(startRaw)
                : 0;
        const useCache = options?.useCache !== false;
        const signal = options?.signal;
        const onProgress =
            typeof options?.onProgress === 'function'
                ? options.onProgress
                : null;

        const countResult = await call(
            'library.getCount',
            {},
        );
        const total = Math.max(0, Number((countResult?.success !== false && countResult?.count) || 0));

        let pages = 0;
        let fetched = 0;
        let fromCacheHits = 0;

        while (offset < total) {
            if (signal?.aborted) {
                return { total, fetched, pages, fromCacheHits, aborted: true };
            }

            const page = await call(
                'library.getAll',
                { offset, limit: pageSize, useCache },
            );
            const ok = page?.success !== false ? page : undefined;
            const tracks: LibraryTrack[] = Array.isArray(ok?.tracks)
                ? ok.tracks
                : [];
            const items: LibraryTrack[] = Array.isArray(ok?.items)
                ? ok.items
                : tracks;
            const currentOffset = Number.isFinite(Number(ok?.offset))
                ? Number(ok?.offset)
                : offset;
            const currentLimit = Number.isFinite(Number(ok?.limit))
                ? Number(ok?.limit)
                : pageSize;
            const fromCache = !!ok?.fromCache;

            if (fromCache) fromCacheHits++;
            pages += 1;
            fetched += tracks.length;

            if (onProgress) {
                try {
                    onProgress({
                        fetched,
                        total,
                        pages,
                        offset: currentOffset,
                        limit: currentLimit,
                    });
                } catch {
                    /* swallow user callback errors */
                }
            }

            yield {
                tracks,
                items,
                total,
                offset: currentOffset,
                limit: currentLimit,
                fromCache,
                fetched,
                pages,
            };

            if (tracks.length === 0) break;
            offset = currentOffset + tracks.length;
        }

        return {
            total,
            fetched,
            pages,
            fromCacheHits,
            aborted: !!signal?.aborted,
        };
    },

    /**
     * Async generator that walks library directories breadth- or
     * depth-first via the `library.browseDirectory` endpoint.
     *
     * @deprecated Prefer {@link library.enumerateTree} for root-aware
     *             traversal.
     */
    enumerateDirectories: async function* (
        options: LibraryEnumerateDirectoriesOptions = {},
    ): AsyncGenerator<
        LibraryDirectoryBatch,
        LibraryEnumerateDirectoriesSummary,
        void
    > {
        const rootPath =
            typeof options?.rootPath === 'string' ? options.rootPath : '';
        const includeFiles = !!options?.includeFiles;
        const strategy = options?.strategy === 'dfs' ? 'dfs' : 'bfs';
        const signal = options?.signal;
        const onProgress =
            typeof options?.onProgress === 'function'
                ? options.onProgress
                : null;

        const pending: string[] = [rootPath];
        const seen = new Set<string>();
        let visited = 0;

        while (pending.length > 0) {
            if (signal?.aborted) {
                return { visited, aborted: true };
            }

            const current =
                strategy === 'dfs' ? pending.pop() : pending.shift();
            if (typeof current !== 'string') continue;
            if (seen.has(current)) continue;
            seen.add(current);

            const result = await call(
                'library.browseDirectory',
                { path: current, includeFiles },
            );
            const ok = result?.success !== false ? result : undefined;
            const directories: string[] = Array.isArray(ok?.directories)
                ? ok.directories
                : [];
            const files: LibraryTrack[] = Array.isArray(ok?.files)
                ? ok.files
                : [];

            for (const dir of directories) {
                if (typeof dir === 'string' && !seen.has(dir)) {
                    pending.push(dir);
                }
            }

            visited += 1;

            if (onProgress) {
                try {
                    onProgress({
                        visited,
                        pending: pending.length,
                        path: current,
                    });
                } catch {
                    /* swallow user callback errors */
                }
            }

            yield {
                path: current,
                directories,
                files,
                visited,
                pending: pending.length,
                success: result?.success !== false,
                error: result?.success === false ? result.error : undefined,
            };
        }

        return { visited, aborted: !!signal?.aborted };
    },

    /**
     * Async generator that walks the typed library tree starting at a
     * given `rootId`. Failed nodes are skipped (still counted) so a
     * partial outage does not abort the traversal.
     */
    enumerateTree: async function* (
        options: LibraryEnumerateTreeOptions,
    ): AsyncGenerator<LibraryTreeBatch, LibraryEnumerateTreeSummary, void> {
        const rootId = options?.rootId;
        if (!rootId) throw new Error('enumerateTree: rootId is required');
        const startPathId =
            typeof options?.pathId === 'string' ? options.pathId : '';
        const includeFiles = !!options?.includeFiles;
        const strategy = options?.strategy === 'dfs' ? 'dfs' : 'bfs';
        const signal = options?.signal;
        const onProgress =
            typeof options?.onProgress === 'function'
                ? options.onProgress
                : null;

        const pending: string[] = [startPathId];
        const seen = new Set<string>();
        let visited = 0;

        while (pending.length > 0) {
            if (signal?.aborted) {
                return { rootId, visited, aborted: true };
            }

            const currentPathId =
                strategy === 'dfs' ? pending.pop() : pending.shift();
            if (typeof currentPathId !== 'string') continue;
            if (seen.has(currentPathId)) continue;
            seen.add(currentPathId);

            const result = await call(
                'library.browseTree',
                {
                    rootId,
                    pathId: currentPathId,
                    includeFiles,
                    recursiveFiles: false,
                },
            );

            if (!result || !result.success) {
                visited += 1;
                continue;
            }

            const directories = Array.isArray(result.directories)
                ? result.directories
                : [];

            for (const dir of directories) {
                if (
                    dir &&
                    typeof dir.pathId === 'string' &&
                    !seen.has(dir.pathId)
                ) {
                    pending.push(dir.pathId);
                }
            }

            visited += 1;

            if (onProgress) {
                try {
                    onProgress({
                        rootId,
                        pathId: currentPathId,
                        absolutePath: result.absolutePath || '',
                        visited,
                        pending: pending.length,
                    });
                } catch {
                    /* swallow user callback errors */
                }
            }

            yield {
                ...result,
                visited,
                pending: pending.length,
            };
        }

        return { rootId, visited, aborted: !!signal?.aborted };
    },

    // ── Convenience queries ─────────────────────────────────────────────
    getRandomTracks: (count?: number) =>
        call('library.getRandomTracks', {
            ...(count != null ? { count } : {}),
        }),
    getRecentlyAdded: (
        limit?: number,
        sortBy?: LibraryGetRecentlyAddedParams['sortBy'],
    ) =>
        call('library.getRecentlyAdded', {
            ...(limit != null ? { limit } : {}),
            ...(sortBy ? { sortBy } : {}),
        }),
    invalidateCache: () =>
        call('library.invalidateCache'),
    isEnabled: () => call('library.isEnabled'),
    /**
     * Run a foobar2000 query expression with an optional Title Formatting
     * sort expression. Sorting is applied before `limit` truncation and
     * `total` reports the untruncated hit count.
     *
     * `fields` narrows the rows exactly as documented on
     * {@link library.search}.
     */
    query: (
        query: string,
        sort?: string,
        limit?: number,
        fields?: string[],
    ) =>
        call('library.query', {
            query,
            ...(sort ? { sort } : {}),
            ...(limit != null ? { limit } : {}),
            ...(fields != null ? { fields } : {}),
        }),
    rescan: () => call('library.rescan'),
};
