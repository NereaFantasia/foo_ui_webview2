import { call } from '../call.js';
import { playlistTarget, type PlaylistRef } from '../playlistRef.js';
import type { AlbumArtType } from '../../types/responses.js';
import type {
    ArtworkGetFb2kUrlByPathParams,
    ArtworkGetFb2kUrlParams,
} from '../../types/generated/params.js';
import type {
    ArtworkGetFb2kUrlByPathResponse,
    ArtworkGetFb2kUrlResponse,
} from '../../types/generated/responses.js';
import type { ArtworkBatchItem } from '../../types/generated/schema-types.js';

/** Optional artwork-retrieval flags shared across the `artwork.*` APIs. */
export interface ArtworkOptions {
    /** Maximum dimension in pixels; the host downsamples larger images. */
    maxSize?: number;
}

/**
 * `artwork` — album art retrieval namespace.
 *
 * Every reader answers with the host's `success` envelope. `available`
 * tells whether a picture (or a URL) exists for the request; a refused
 * call has `success: false` with a `code`.
 */
export const artwork = {
    getCurrent: (type?: AlbumArtType) =>
        call('artwork.getCurrent', { type }),
    getByPath: (path: string, type?: AlbumArtType) =>
        call('artwork.getByPath', { path, type }),
    /**
     * `options` is accepted for source compatibility only: the host reads
     * the picture as stored and does not scale it, so `maxSize` is not sent.
     */
    getForTrack: (
        path: string,
        type?: AlbumArtType,
        // eslint-disable-next-line @typescript-eslint/no-unused-vars
        options?: ArtworkOptions,
    ) =>
        call('artwork.getForTrack', {
            path,
            type,
        }),
    /**
     * Resolve a `fb2k://` URL for the currently playing track suitable
     * for direct `<img src>` consumption.
     *
     * The resolved URL is returned in the `dataUrl` field of the
     * response envelope (see {@link ArtworkGetFb2kUrlResponse}).
     */
    getFb2kUrl: (
        type?: AlbumArtType,
        options?: Pick<ArtworkGetFb2kUrlParams, 'maxSize'>,
    ) =>
        call('artwork.getFb2kUrl', {
            type,
            ...(options || {}),
        }),
    /**
     * Resolve a `fb2k://` URL for the track at `path` suitable for
     * direct `<img src>` consumption.
     *
     * The resolved URL is returned in the `dataUrl` field of the
     * response envelope (see {@link ArtworkGetFb2kUrlByPathResponse}).
     */
    getFb2kUrlByPath: (
        path: string,
        type?: AlbumArtType,
        options?: Pick<ArtworkGetFb2kUrlByPathParams, 'maxSize'>,
    ) =>
        call(
            'artwork.getFb2kUrlByPath',
            {
                path,
                type,
                ...(options || {}),
            },
        ),
    /**
     * Append `?maxSize=N` (or `&maxSize=N`) to a `fb2k://` artwork URL.
     * Returns the original URL untouched when `maxSize` is missing,
     * non-positive, or the URL is empty.
     */
    withMaxSize: (url: string, maxSize?: number): string => {
        if (!url || !maxSize || maxSize <= 0) return url;
        const sep = url.includes('?') ? '&' : '?';
        return url + sep + 'maxSize=' + encodeURIComponent(String(maxSize));
    },
    getAvailableArtwork: (path: string) => call('artwork.getAvailableArtwork', { path }),
    /** Without `path` the host reads the playing track. */
    getAvailableTypes: (path?: string) =>
        call('artwork.getAvailableTypes', {
            ...(path ? { path } : {}),
        }),
    getBatch: (paths: string[], type?: AlbumArtType) =>
        call('artwork.getBatch', {
            paths,
            ...(type ? { type } : {}),
        }),
    /** A negative `playlist` index names the active playlist. */
    getByPlaylistItem: (
        playlist: PlaylistRef,
        index: number,
        type?: AlbumArtType,
    ) =>
        call('artwork.getByPlaylistItem', {
            ...playlistTarget(playlist),
            index,
            ...(type ? { type } : {}),
        }),
    /**
     * Batch variant of {@link getFb2kUrlByPath}. Accepts either bare
     * paths or `ArtworkBatchItem` objects whose own `type` / `maxSize`
     * win over the batch-wide ones. Returns the full `{ artworks }`
     * envelope so callers can map per-row availability and error reasons.
     */
    getFb2kUrlByPathBatch: (
        items: string[] | ArtworkBatchItem[],
        opts?: { type?: AlbumArtType; maxSize?: number },
    ) =>
        call(
            'artwork.getFb2kUrlByPathBatch',
            {
                // The host accepts exactly one of `paths` (string[]) or `items` (object[]).
                ...(items.length > 0 && typeof items[0] === 'string'
                    ? { paths: items as string[] }
                    : { items: items as ArtworkBatchItem[] }),
                ...(opts?.type ? { type: opts.type } : {}),
                ...(opts?.maxSize != null ? { maxSize: opts.maxSize } : {}),
            },
        ),
    getFolderImages: (directory: string) =>
        call('artwork.getFolderImages', {
            directory,
        }),
    /** Without `path` the host reads the playing track. */
    getLyrics: (path?: string) =>
        call('artwork.getLyrics', {
            ...(path ? { path } : {}),
        }),
    /** Without `path` the host reads the playing track. */
    getMetadata: (path?: string) =>
        call('artwork.getMetadata', {
            ...(path ? { path } : {}),
        }),
};
