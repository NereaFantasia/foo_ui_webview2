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

import { bridge } from '../Bridge.js';
import type {
    AlbumInfo,
    BaseResponse,
    LibraryAddToPlaylistResponse,
    LibraryAlbumTracksResponse,
    LibraryAlbumsResponse,
    LibraryArtistTracksResponse,
    LibraryArtistsResponse,
    LibraryBrowseDirectoryResponse,
    LibraryBrowseTreeResponse,
    LibraryCacheStatsResponse,
    LibraryDirectoryBatch,
    LibraryEnumerateDirectoriesOptions,
    LibraryInvalidateCacheResponse,
    LibraryRandomTracksResponse,
    LibraryEnumerateDirectoriesSummary,
    LibraryEnumerateFieldValuesOptions,
    LibraryEnumerateTracksOptions,
    LibraryEnumerateTracksPage,
    LibraryEnumerateTracksSummary,
    LibraryEnumerateTreeOptions,
    LibraryEnumerateTreeSummary,
    LibraryFieldValuesResponse,
    LibraryGenresResponse,
    LibraryGetByPathResponse,
    LibraryGetRootsResponse,
    LibraryPagedTracksResponse,
    LibraryQueryResponse,
    LibraryRecentlyAddedResponse,
    LibrarySearchResponse,
    LibraryStats,
    LibraryStatus,
    LibraryTreeBatch,
    TrackInfo,
} from '../../types/responses.js';
import type {
    LibraryGetAlbumsParams,
    LibraryGetArtistsParams,
    LibrarySearchParams,
} from '../../types/generated/params.js';
import type { LibraryGetAllResultPayload } from '../../types/events.js';

/** @deprecated Use `Omit<LibrarySearchParams, 'query' | 'limit'>`. */
export type LibrarySearchOptions = Omit<LibrarySearchParams, 'query' | 'limit'>;

/** @deprecated Use `LibraryGetAlbumsParams`. */
export type LibraryGetAlbumsOptions = Pick<LibraryGetAlbumsParams, 'limit'>;

export interface LibraryBrowseTreeParams {
    rootId: string;
    pathId?: string;
    includeFiles?: boolean;
    recursiveFiles?: boolean;
}

export const library = {
    // ── Search / aggregation ────────────────────────────────────────────
    search: (
        query: string,
        limit?: number,
        options?: Omit<LibrarySearchParams, 'query' | 'limit'>,
    ) =>
        bridge.invoke<LibrarySearchResponse>('library.search', {
            query,
            limit,
            ...(options && typeof options === 'object' ? options : {}),
        }),
    getAlbums: (options?: number | LibraryGetAlbumsParams) =>
        bridge.invoke<LibraryAlbumsResponse>(
            'library.getAlbums',
            typeof options === 'number'
                ? { limit: options }
                : { ...(options || {}) },
        ),
    getArtists: (
        limit?: number,
        options?: Omit<LibraryGetArtistsParams, 'limit'>,
    ) =>
        bridge.invoke<LibraryArtistsResponse>('library.getArtists', {
            limit,
            ...(options && typeof options === 'object' ? options : {}),
        }),
    getGenres: () => bridge.invoke<LibraryGenresResponse>('library.getGenres'),
    getStats: () => bridge.invoke<LibraryStats>('library.getStats'),
    getStatus: () => bridge.invoke<LibraryStatus>('library.getStatus'),
    getCount: () => bridge.invoke<{ count: number }>('library.getCount'),
    /**
     * Fetch library tracks. Resolves synchronously for paged requests and
     * cache hits. When the host offloads a full-library serialization to a
     * background worker it returns `{ pending: true, requestId }`; this
     * wrapper then awaits the `library:getAllResult` event (filtered by
     * `requestId`) with a client-side timeout (default 60 s, override via
     * `opts.timeout`). The resolved shape is always a
     * {@link LibraryPagedTracksResponse}, so callers are unaffected by the
     * threading model.
     */
    getAll: async (
        start?: number,
        count?: number,
        opts: { timeout?: number } = {},
    ): Promise<LibraryPagedTracksResponse> => {
        const result = await bridge.invoke<
            LibraryPagedTracksResponse & {
                pending?: boolean;
                requestId?: string;
            }
        >('library.getAll', {
            start,
            count,
            asyncResult: true,
        });

        if (result?.pending === true && result.requestId) {
            const timeoutMs = opts.timeout ?? 60000;
            const requestId = result.requestId;
            return new Promise<LibraryPagedTracksResponse>(
                (resolve, reject) => {
                    let timer: ReturnType<typeof setTimeout> | null = null;
                    const cleanup = (): void => {
                        off();
                        if (timer) {
                            clearTimeout(timer);
                            timer = null;
                        }
                    };
                    const off = bridge.on(
                        'library:getAllResult',
                        (raw: unknown) => {
                            const e = raw as LibraryGetAllResultPayload;
                            if (e?.requestId !== requestId) return;
                            cleanup();
                            if (e.error) {
                                reject(e);
                                return;
                            }
                            resolve(e as LibraryPagedTracksResponse);
                        },
                    );
                    if (timeoutMs > 0) {
                        timer = setTimeout(() => {
                            cleanup();
                            reject({
                                success: false,
                                error: 'TIMEOUT',
                                message:
                                    'library.getAll timed out after ' +
                                    timeoutMs +
                                    'ms',
                                requestId,
                            });
                        }, timeoutMs);
                    }
                },
            );
        }

        return result;
    },
    refresh: () => bridge.invoke<BaseResponse>('library.refresh'),
    getByPath: (path: string) =>
        bridge.invoke<LibraryGetByPathResponse>('library.getByPath', {
            path,
        }),
    addToPlaylist: (paths: string[], playlist?: number) =>
        bridge.invoke<LibraryAddToPlaylistResponse>('library.addToPlaylist', {
            paths,
            ...(playlist != null ? { playlist } : {}),
        }),

    // ── Roots / typed tree ──────────────────────────────────────────────
    getRoots: () => bridge.invoke<LibraryGetRootsResponse>('library.getRoots'),
    browseTree: (params: LibraryBrowseTreeParams) =>
        bridge.invoke<LibraryBrowseTreeResponse>('library.browseTree', {
            rootId: params.rootId,
            ...(params.pathId != null ? { pathId: params.pathId } : {}),
            ...(params.includeFiles != null
                ? { includeFiles: params.includeFiles }
                : {}),
            ...(params.recursiveFiles != null
                ? { recursiveFiles: params.recursiveFiles }
                : {}),
        }),

    // ── Filesystem-based directory listing (legacy entry point) ─────────
    browseDirectory: (path: string, includeFiles?: boolean) =>
        bridge.invoke<LibraryBrowseDirectoryResponse>(
            'library.browseDirectory',
            {
                path,
                ...(includeFiles != null ? { includeFiles } : {}),
            },
        ),
    getAlbumTracks: (album: string, artist?: string) =>
        bridge.invoke<LibraryAlbumTracksResponse>('library.getAlbumTracks', {
            album,
            ...(artist ? { artist } : {}),
        }),
    getArtistAlbums: (artist: string, limit?: number) =>
        bridge.invoke<{ albums: AlbumInfo[] }>('library.getArtistAlbums', {
            artist,
            ...(limit != null ? { limit } : {}),
        }),
    getArtistTracks: (artist: string, limit?: number) =>
        bridge.invoke<LibraryArtistTracksResponse>('library.getArtistTracks', {
            artist,
            ...(limit != null ? { limit } : {}),
        }),
    getCacheStats: () =>
        bridge.invoke<LibraryCacheStatsResponse>('library.getCacheStats'),
    getFieldValues: (field: string, limit?: number, separator?: string) =>
        bridge.invoke<LibraryFieldValuesResponse>('library.getFieldValues', {
            field,
            ...(limit != null ? { limit } : {}),
            ...(separator ? { separator } : {}),
        }),
    /** Semantic alias for {@link library.getFieldValues}. */
    enumerateFieldValues: (
        field: string,
        options: LibraryEnumerateFieldValuesOptions = {},
    ) =>
        bridge.invoke<LibraryFieldValuesResponse>('library.getFieldValues', {
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

        const countResult = await bridge.invoke<{ count: number }>(
            'library.getCount',
            {},
        );
        const total = Math.max(0, Number(countResult?.count || 0));

        let pages = 0;
        let fetched = 0;
        let fromCacheHits = 0;

        while (offset < total) {
            if (signal?.aborted) {
                return { total, fetched, pages, fromCacheHits, aborted: true };
            }

            const page = await bridge.invoke<
                LibraryPagedTracksResponse & { fromCache?: boolean }
            >('library.getAll', { offset, limit: pageSize, useCache });
            const tracks: TrackInfo[] = Array.isArray(page?.tracks)
                ? page.tracks
                : [];
            const items: TrackInfo[] = Array.isArray(page?.items)
                ? page.items
                : tracks;
            const currentOffset = Number.isFinite(Number(page?.offset))
                ? Number(page.offset)
                : offset;
            const currentLimit = Number.isFinite(Number(page?.limit))
                ? Number(page.limit)
                : pageSize;
            const fromCache = !!page?.fromCache;

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
     * depth-first via the legacy `library.browseDirectory` endpoint.
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

            const result = await bridge.invoke<LibraryBrowseDirectoryResponse>(
                'library.browseDirectory',
                { path: current, includeFiles },
            );
            const directories: string[] = Array.isArray(result?.directories)
                ? result.directories
                : [];
            const files: TrackInfo[] = Array.isArray(result?.files)
                ? result.files
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
                error: result?.error,
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

            const result = await bridge.invoke<LibraryBrowseTreeResponse>(
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
        bridge.invoke<LibraryRandomTracksResponse>('library.getRandomTracks', {
            ...(count != null ? { count } : {}),
        }),
    getRecentlyAdded: (limit?: number, sortBy?: string) =>
        bridge.invoke<LibraryRecentlyAddedResponse>('library.getRecentlyAdded', {
            ...(limit != null ? { limit } : {}),
            ...(sortBy ? { sortBy } : {}),
        }),
    invalidateCache: () =>
        bridge.invoke<LibraryInvalidateCacheResponse>('library.invalidateCache'),
    isEnabled: () => bridge.invoke<{ enabled: boolean }>('library.isEnabled'),
    query: (query: string, sort?: string, limit?: number) =>
        bridge.invoke<LibraryQueryResponse>('library.query', {
            query,
            ...(sort ? { sort } : {}),
            ...(limit != null ? { limit } : {}),
        }),
    rescan: () => bridge.invoke<BaseResponse>('library.rescan'),
};
