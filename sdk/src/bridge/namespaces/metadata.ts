import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import { bytesToBase64, parseBase64DataUrl } from '../binaryData.js';
import type { JsonObject } from '../../types/json.js';
import type { MetadataWriteCompletePayload } from '../../types/events.js';
import type {
    MetadataEmbedArtworkParams,
    MetadataProbeBatchAsyncParams,
    MetadataReadByPathParams,
    MetadataReadParams,
    MetadataReadRawParams,
    MetadataRemoveEmbeddedArtParams,
    MetadataRemoveFieldParams,
    MetadataRemoveTagParams,
    MetadataWriteParams,
} from '../../types/generated/params.js';
import type { MetadataWriteBatchItem } from '../../types/generated/schema-types.js';

/** Options for artwork byte helpers; the helper owns `imageData`. */
export type MetadataArtworkBytesOptions = Omit<
    MetadataEmbedArtworkParams,
    'path' | 'imageData' | 'target'
> & {
    target?: 'embedded' | 'file' | 'all' | Array<'embedded' | 'file'>;
};

// The host takes `target` as an array only; a single destination is sent as a
// one-element array, and an omitted one stays omitted so the host writes the
// embedded target.
function artworkTarget(target: string | string[] | undefined): { target?: string[] } {
    if (target === undefined) return {};
    return { target: Array.isArray(target) ? target : [target] };
}

function embedArtworkParams(
    path: string,
    opts: Omit<MetadataEmbedArtworkParams, 'path' | 'target'> & {
        target?: string | string[];
    },
) {
    const { target, ...rest } = opts;
    return { ...rest, path, ...artworkTarget(target) } satisfies MetadataEmbedArtworkParams;
}

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
let _defaultMetadataLoggerOff: (() => void) | null = subscribe(
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

function metadataEmbedArtworkBytes(
    path: string,
    bytes: ArrayBuffer | Uint8Array,
    opts?: MetadataArtworkBytesOptions,
) {
    const { target, ...rest } = opts ?? {};
    return call('metadata.embedArtwork', {
        ...rest,
        path,
        imageData: bytesToBase64(bytes),
        ...artworkTarget(target),
    });
}

async function metadataEmbedArtworkFromDataUrl(
    path: string,
    dataUrl: string,
    opts?: MetadataArtworkBytesOptions,
) {
    const { mediaType, base64 } = parseBase64DataUrl(dataUrl);
    if (!mediaType.startsWith('image/')) {
        throw new TypeError('Expected an image Base64 data URL.');
    }
    const { target, ...rest } = opts ?? {};
    return call('metadata.embedArtwork', {
        ...rest,
        path,
        imageData: base64,
        ...artworkTarget(target),
    });
}

/**
 * `metadata` — tag read / write namespace.
 */
export const metadata = {
    /**
     * Read structured metadata for a single track.
     *
     * Returns `{ success, path, tags, info }`. `tags` preserves upstream
     * key casing and may hold a single string or a `string[]` per
     * multi-value field.
     *
     * For a track inside a container (CUE sheet, ISO image, multi-track
     * file), either append `|subsong:N` to `path` or pass
     * `opts.cueIndex`; the option wins when both are given. CUE track
     * numbering starts at 1.
     */
    read: (path: string, opts?: Omit<MetadataReadParams, 'path'>) =>
        call('metadata.read', {
            path,
            ...(opts || {}),
        }),
    /**
     * Batch variant; returns a `results[]` array with one envelope entry
     * per requested path. Each path is resolved independently and may
     * carry its own `|subsong:N` suffix; there is no batch-wide
     * subsong option.
     */
    readBatch: (paths: string[]) =>
        call('metadata.readBatch', {
            paths,
        }),
    /**
     * Flat-form single-track read — every tag and technical-info field
     * becomes a top-level field, upper-cased by the host, alongside
     * `success` / `path`. The return shape is intentionally loose because
     * the host forwards whatever tags the file happens to carry.
     * `canonicalPath` appears only on the file-open failure envelope.
     *
     * Container tracks are addressed the same way as `read()`.
     */
    readByPath: (path: string, opts?: Omit<MetadataReadByPathParams, 'path'>) =>
        call('metadata.readByPath', {
            path,
            ...(opts || {}),
        }),
    /**
     * Read structured metadata straight from the file, bypassing the
     * host's metadb cache; the result carries `source: 'file'`.
     */
    readRaw: (path: string, opts?: Omit<MetadataReadRawParams, 'path'>) =>
        call('metadata.readRaw', {
            path,
            ...(opts || {}),
        }),
    /**
     * Cancellable, non-blocking batch probe. Reads happen on a host worker
     * thread, so unlike `readBatch`, a few hundred paths do not stall the UI.
     *
     * Returns a `{ operationId, totalCount }` receipt immediately; the results
     * arrive in batches on `metadata:probeProgress` and are followed by
     * exactly one `metadata:probeComplete`. Each result reports where its info
     * came from (`infoSource: 'cached' | 'direct'`) and, on failure, which of
     * `'not-found'` / `'unsupported-format'` / `'read-error'` applies - the
     * distinction `readBatch` collapses into one generic error string.
     *
     * Paths may carry a `|subsong:N` suffix and are resolved independently;
     * they are echoed back verbatim so they work as lookup keys. Unlike
     * `metadata.read`, the batch surface does not honour the legacy `#N`
     * subsong spelling, which would mis-split an extensionless filename that
     * happens to end in `#<digits>`.
     *
     * Path validation is all-or-nothing: if any path fails the host's media
     * read check the whole call is rejected with `PERMISSION_DENIED` and no
     * `operationId` is produced. Per-path rejection is not available.
     *
     * @param paths Paths to probe; must not be empty.
     * @param opts `includeTags` (default `true`) attaches the flat tag map to
     *   each successful result. Pass `false` when only technical info is
     *   wanted.
     * @returns Dispatch receipt; the actual results arrive by event.
     */
    probeBatchAsync: (paths: string[], opts?: Omit<MetadataProbeBatchAsyncParams, 'paths'>) =>
        call('metadata.probeBatchAsync', {
            paths,
            ...(opts || {}),
        }),
    /**
     * Stop a probe started by {@link metadata.probeBatchAsync}.
     *
     * Cancellation interrupts the in-progress disk read rather than waiting
     * for it, and the run always finishes with a `metadata:probeComplete`
     * carrying `cancelled: true`. Paths not yet reached are never reported,
     * and the interrupted path is reported as neither success nor failure.
     *
     * @param operationId The id from the `probeBatchAsync` receipt.
     * @returns `cancelled: false` when the operation had already finished or
     *   never existed.
     */
    cancelProbe: (operationId: string) =>
        call('metadata.cancelProbe', {
            operationId,
        }),
    /**
     * Queue tag updates and signal completion via
     * `metadata:writeComplete`. The returned receipt describes the dispatch;
     * the final outcome is on the event payload.
     *
     * String arrays replace all values of a tag; an empty array removes it.
     * Non-string, empty or NUL-containing array elements fail the entire track
     * with `INVALID_PARAMS` before any of its tags are queued.
     */
    write: (
        path: string,
        tags: JsonObject,
        opts?: Omit<MetadataWriteParams, 'path' | 'tags'>,
    ) =>
        call('metadata.write', {
            path,
            tags,
            ...(opts || {}),
        }),
    /**
     * Queue one write per entry. When any entry fails (for example one
     * without `tags` or with an invalid tag array), the call resolves as a
     * failure envelope that still carries `successCount`, `failCount` and
     * `errors`; the other entries were dispatched all the same.
     */
    writeBatch: (items: MetadataWriteBatchItem[]) =>
        call('metadata.writeBatch', {
            items,
        }),
    /**
     * Write artwork into the audio file or alongside it.
     *
     * `opts.imageData` is the image as raw Base64.
     *
     * `opts.target` selects the destination. The host takes an array; a
     * single string is sent as a one-element array:
     * - `"embedded"` (default) — write into the file's tag container via
     *   `album_art_editor`. Fails for formats the SDK cannot edit (e.g. CUE).
     * - `"file"` — write a sibling image file in the audio's directory using
     *   fb2k's external artwork naming (`cover.<ext>` / `back.<ext>` / ...).
     *   The extension is inferred from the image's magic bytes.
     * - `"all"` or `["embedded", "file"]` — run both targets and return a
     *   `results` map; the call succeeds when either target succeeded.
     *
     * `opts.filename` overrides the auto-generated sidecar name (file mode only).
     * It must be a plain file name: path separators are rejected, and so is a
     * name made only of periods (`.`, `..`). Periods inside a name (`cover..jpg`) are fine.
     *
     * CUE / subsong paths share one sidecar per directory — this matches
     * fb2k's per-directory external artwork lookup.
     */
    embedArtwork: (
        path: string,
        opts: Omit<MetadataEmbedArtworkParams, 'path' | 'target'> & {
            target?: string | string[];
        },
    ) =>
        call(
            'metadata.embedArtwork',
            embedArtworkParams(path, opts),
        ),
    /**
     * Remove embedded artwork: one `type`, or every picture when `type` is
     * omitted or `removeAll` is set.
     */
    removeEmbeddedArt: (
        path: string,
        opts?: Omit<MetadataRemoveEmbeddedArtParams, 'path'>,
    ) =>
        call('metadata.removeEmbeddedArt', {
            path,
            ...(opts || {}),
        }),
    /** Removes a single tag field. */
    removeField: (
        path: string,
        field: string,
        opts?: Omit<MetadataRemoveFieldParams, 'path' | 'tags'>,
    ) =>
        call('metadata.removeField', {
            path,
            tags: [field],
            ...(opts || {}),
        }),
    removeTag: (
        path: string,
        tags: string[],
        opts?: Omit<MetadataRemoveTagParams, 'path' | 'tags'>,
    ) =>
        call('metadata.removeTag', {
            path,
            tags,
            ...(opts || {}),
        }),
    /** Embed exact image bytes without exposing the raw Base64 wire format. */
    embedArtworkBytes: metadataEmbedArtworkBytes,
    /** Embed a Base64 `data:image/*` URL; rejects malformed input. */
    embedArtworkFromDataUrl: metadataEmbedArtworkFromDataUrl,
    /** Detach the default failure logger; see {@link disableDefaultMetadataLogger}. */
    disableDefaultLogger: disableDefaultMetadataLogger,
};
