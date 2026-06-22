/**
 * `metadata` — tag read / write namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    MetadataReadBatchResponse,
    MetadataReadRawResponse,
    MetadataReadResponse,
} from '../../types/responses.js';
import type { JsonObject } from '../../types/json.js';
import type { MetadataWriteCompletePayload } from '../../types/events.js';
import type {
    MetadataEmbedArtworkParams,
    MetadataReadRawParams,
    MetadataRemoveEmbeddedArtParams,
} from '../../types/generated/params.js';
import type { MetadataEmbedArtworkResponse } from '../../types/generated/responses.js';

/**
 * Default failure logger for `metadata:writeComplete`.
 *
 * Async writes (`metadata.write` / `metadata.removeTag` / batch variants)
 * dispatch immediately and signal their result later through the
 * `metadata:writeComplete` event. Without a subscriber, write errors
 * silently disappear into the host log. This module-level subscriber
 * surfaces every failure into the JS console so theme authors notice
 * them during development.
 *
 * Theme code wanting custom handling can detach the default logger via
 * {@link disableDefaultMetadataLogger}, register its own
 * `bridge.on('metadata:writeComplete', ...)` listener, or both.
 */
let _defaultMetadataLoggerOff: (() => void) | null = bridge.on(
    'metadata:writeComplete',
    (event: MetadataWriteCompletePayload) => {
        if (event && event.success === false) {
            console.warn('[fb.metadata] write failed:', {
                operation: event.operation,
                path: event.path,
                subsong: event.subsong,
                status: event.status,
                code: event.code,
            });
        }
    },
);

/**
 * Detach the default `metadata:writeComplete` logger installed at module
 * load. Idempotent; safe to call from theme bootstrap before installing
 * a custom toast / dialog handler.
 */
export function disableDefaultMetadataLogger(): void {
    if (_defaultMetadataLoggerOff) {
        _defaultMetadataLoggerOff();
        _defaultMetadataLoggerOff = null;
    }
}

export const metadata = {
    /**
     * Read structured metadata for a single track.
     *
     * Returns `{ success, path, tags, info }`. `tags` preserves upstream
     * key casing and may hold a single string or a `string[]` per
     * multi-value field.
     */
    read: (path: string) =>
        bridge.invoke<MetadataReadResponse>('metadata.read', { path }),
    /**
     * Batch variant; returns a `results[]` array with one envelope entry
     * per requested path.
     */
    readBatch: (paths: string[]) =>
        bridge.invoke<MetadataReadBatchResponse>('metadata.readBatch', { paths }),
    /**
     * Flat-form single-track read — every tag becomes a top-level field
     * alongside `success` / `path` / `canonicalPath`. Keys use upstream
     * casing (typically UPPERCASE); the return shape is intentionally
     * loose because the C++ host forwards whatever tags the file
     * happens to carry.
     */
    readByPath: (path: string) =>
        bridge.invoke<JsonObject>('metadata.readByPath', { path }),
    readRaw: (
        path: string,
        opts?: Omit<MetadataReadRawParams, 'path'>,
    ) =>
        bridge.invoke<MetadataReadRawResponse>('metadata.readRaw', {
            path,
            ...(opts || {}),
        }),
    /**
     * Async write — dispatches immediately and signals completion via
     * `metadata:writeComplete`. The receipt below describes the dispatch
     * envelope; the final outcome is on the event payload.
     */
    write: (path: string, tags: JsonObject) =>
        bridge.invoke<
            BaseResponse & {
                dispatched?: boolean;
                canonicalPath?: string;
                handlePath?: string;
                subsong?: number;
                tagsApplied?: number;
                tagsSet?: number;
                tagsRemoved?: number;
                note?: string;
            }
        >('metadata.write', { path, tags }),
    writeBatch: (items: Array<{ path: string; tags: JsonObject }>) =>
        bridge.invoke<
            BaseResponse & {
                successCount?: number;
                failCount?: number;
                errors?: Array<{ path: string; error: string }>;
            }
        >('metadata.writeBatch', { items }),
    /**
     * Write artwork into the audio file or alongside it.
     *
     * `opts.target` selects the destination:
     * - `"embedded"` (default) — write into the file's tag container via
     *   `album_art_editor`. Fails for formats the SDK cannot edit (e.g. CUE).
     * - `"file"` — write a sibling image file in the audio's directory using
     *   fb2k's external artwork naming (`cover.<ext>` / `back.<ext>` / ...).
     *   The extension is inferred from the image's magic bytes.
     * - `"all"` or `["embedded", "file"]` — run both targets and return a
     *   `results` map; top-level `success` is true when any target succeeded.
     *
     * `opts.filename` overrides the auto-generated sidecar name (file mode only).
     * Path separators or `..` sequences are rejected.
     *
     * CUE / subsong paths share one sidecar per directory — this matches
     * fb2k's per-directory external artwork lookup.
     */
    embedArtwork: (
        path: string,
        opts?: Omit<MetadataEmbedArtworkParams, 'path'>,
    ) =>
        bridge.invoke<MetadataEmbedArtworkResponse>(
            'metadata.embedArtwork',
            { path, ...(opts || {}) },
        ),
    removeEmbeddedArt: (
        path: string,
        opts?: Omit<MetadataRemoveEmbeddedArtParams, 'path'>,
    ) =>
        bridge.invoke<BaseResponse & { removedTypes?: string[] }>(
            'metadata.removeEmbeddedArt',
            { path, ...(opts || {}) },
        ),
    /** Removes a single tag field. */
    removeField: (path: string, field: string) =>
        bridge.invoke<
            BaseResponse & {
                dispatched?: boolean;
                subsong?: number;
                removedTags?: string[];
                removedCount?: number;
                note?: string;
            }
        >('metadata.removeField', { path, tags: [field] }),
    removeTag: (path: string, tags: string[]) =>
        bridge.invoke<
            BaseResponse & {
                dispatched?: boolean;
                subsong?: number;
                removedTags?: string[];
                removedCount?: number;
                note?: string;
            }
        >('metadata.removeTag', { path, tags }),
    /** Detach the default failure logger; see {@link disableDefaultMetadataLogger}. */
    disableDefaultLogger: disableDefaultMetadataLogger,
};
