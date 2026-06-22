/**
 * `artwork` — album-art retrieval namespace.
 *
 * Returns either embedded image data (`getCurrent` / `getByPath` /
 * `getForTrack`) or `fb2k://` URLs (`getFb2kUrl*`) suitable for direct
 * `<img src>` consumption.
 *
 * {@link artwork.withMaxSize} is a pure URL helper (no host call); it
 * appends a `?maxSize=N` query parameter so callers do not have to
 * splice strings by hand.
 */

import { bridge } from '../Bridge.js';
import type {
    AlbumArtType,
    ArtworkAvailableArtworkResponse,
    ArtworkBatchItem,
    ArtworkBatchResponse,
    ArtworkLyricsResult,
    ArtworkMetadataResponse,
    ArtworkResponse,
} from '../../types/responses.js';
import type {
    ArtworkGetFb2kUrlByPathParams,
    ArtworkGetFb2kUrlParams,
} from '../../types/generated/params.js';
import type {
    ArtworkGetFb2kUrlByPathResponse,
    ArtworkGetFb2kUrlResponse,
} from '../../types/generated/responses.js';

/** Optional artwork-retrieval flags shared across the `artwork.*` APIs. */
export interface ArtworkOptions {
    /** Maximum dimension in pixels; the host downsamples larger images. */
    maxSize?: number;
}

export const artwork = {
    getCurrent: (type?: AlbumArtType) =>
        bridge.invoke<ArtworkResponse>('artwork.getCurrent', { type }),
    getByPath: (path: string, type?: AlbumArtType) =>
        bridge.invoke<ArtworkResponse>('artwork.getByPath', { path, type }),
    getForTrack: (
        path: string,
        type?: AlbumArtType,
        options?: ArtworkOptions,
    ) =>
        bridge.invoke<ArtworkResponse>('artwork.getForTrack', {
            path,
            type,
            ...(options || {}),
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
        bridge.invoke<ArtworkGetFb2kUrlResponse>('artwork.getFb2kUrl', {
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
        bridge.invoke<ArtworkGetFb2kUrlByPathResponse>(
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
    getAvailableArtwork: (path?: string) =>
        bridge.invoke<ArtworkAvailableArtworkResponse>(
            'artwork.getAvailableArtwork',
            { ...(path ? { path } : {}) },
        ),
    getAvailableTypes: () =>
        bridge.invoke<AlbumArtType[]>('artwork.getAvailableTypes'),
    getBatch: (paths: string[]) =>
        bridge.invoke<ArtworkResponse[]>('artwork.getBatch', { paths }),
    getByPlaylistItem: (
        playlist: number,
        index: number,
        type?: AlbumArtType,
    ) =>
        bridge.invoke<ArtworkResponse>('artwork.getByPlaylistItem', {
            playlist,
            index,
            ...(type ? { type } : {}),
        }),
    /**
     * Batch variant of {@link getFb2kUrlByPath}. Accepts either bare
     * paths or `ArtworkBatchItem` objects with per-track overrides.
     * Returns the full `{ artworks: ArtworkBatchEntry[] }` envelope so
     * callers can map per-row availability and error reasons.
     *
     * Aligns the SDK signature with the C++ handler
     * `ArtworkGetFb2kUrlByPathBatch` which accepts `items[]` or
     * `paths[]` plus batch-wide `type` / `maxSize`.
     */
    getFb2kUrlByPathBatch: (
        items: string[] | ArtworkBatchItem[],
        opts?: { type?: AlbumArtType; maxSize?: number },
    ) =>
        bridge.invoke<ArtworkBatchResponse>(
            'artwork.getFb2kUrlByPathBatch',
            {
                // C++ accepts either `paths` (string[]) or `items` (object[]).
                // We forward whichever the caller actually passed for clarity.
                ...(items.length > 0 && typeof items[0] === 'string'
                    ? { paths: items as string[] }
                    : { items: items as ArtworkBatchItem[] }),
                ...(opts?.type ? { type: opts.type } : {}),
                ...(opts?.maxSize != null ? { maxSize: opts.maxSize } : {}),
            },
        ),
    getFolderImages: (directory: string) =>
        bridge.invoke<{ images: string[] }>('artwork.getFolderImages', {
            directory,
        }),
    getLyrics: (path: string) =>
        bridge.invoke<ArtworkLyricsResult>('artwork.getLyrics', { path }),
    getMetadata: (path: string) =>
        bridge.invoke<ArtworkMetadataResponse>('artwork.getMetadata', { path }),
};
