/**
 * `file` — file-system namespace.
 */

import { bridge } from '../Bridge.js';
import {
    base64ToBytes,
    bytesToBase64,
    parseBase64DataUrl,
} from '../binaryData.js';
import type {
    BaseResponse,
    FileListResponse,
    FileGetInfoResponse,
} from '../../types/responses.js';
import type {
    FileCopyAsyncParams,
    FileCopyParams,
    FileDeleteAsyncParams,
    FileDeleteParams,
    FileListParams,
    FileReadParams,
    FileWriteParams,
} from '../../types/generated/params.js';
import type {
    FileCancelOpResponse,
    FileCopyAsyncResponse,
    FileDeleteAsyncResponse,
    FileMoveAsyncResponse,
} from '../../types/generated/responses.js';

/** @deprecated Use `Omit<FileReadParams, 'path'>`. */
export type FileReadOptions = Omit<FileReadParams, 'path'>;

/** @deprecated Use `Omit<FileWriteParams, 'path' | 'content'>`. */
export type FileWriteOptions = Omit<FileWriteParams, 'path' | 'content'>;

/** @deprecated Use `Omit<FileListParams, 'path'>`. */
export type FileListOptions = Omit<FileListParams, 'path'>;

/** Options for binary writes; the helper owns the wire encoding. */
export type FileBinaryWriteOptions = Omit<
    FileWriteParams,
    'path' | 'content' | 'encoding'
>;

/** One item of the batch passed to {@link file.copyAsync} / {@link file.moveAsync}. */
export type FileOpEntry = FileCopyAsyncParams['items'][number];

/** Options shared by {@link file.copyAsync} and {@link file.moveAsync}. */
export type FileOpAsyncOptions = Omit<FileCopyAsyncParams, 'items'>;

/** Options for {@link file.deleteAsync}. */
export type FileDeleteAsyncOptions = Omit<FileDeleteAsyncParams, 'paths'>;

async function fileReadBinary(path: string): Promise<Uint8Array> {
    const response = await bridge.invoke<BaseResponse & { content?: string }>(
        'file.read',
        { path, encoding: 'binary' },
    );
    if (response?.success === false) {
        throw new Error(response.error ?? 'file.read failed.');
    }
    if (typeof response?.content !== 'string') {
        throw new TypeError('file.read did not return binary content.');
    }
    return base64ToBytes(response.content);
}

function fileWriteBinary(
    path: string,
    bytes: ArrayBuffer | Uint8Array,
    opts?: FileBinaryWriteOptions,
) {
    return bridge.invoke<BaseResponse & { bytesWritten?: number }>('file.write', {
        ...opts,
        path,
        content: `base64:${bytesToBase64(bytes)}`,
        encoding: 'binary',
    });
}

async function fileWriteDataUrl(
    path: string,
    dataUrl: string,
    opts?: FileBinaryWriteOptions,
) {
    const { base64 } = parseBase64DataUrl(dataUrl);
    return bridge.invoke<BaseResponse & { bytesWritten?: number }>(
        'file.write',
        {
            ...opts,
            path,
            content: `base64:${base64}`,
            encoding: 'binary',
        },
    );
}

export const file = {
    read: (path: string, opts?: Omit<FileReadParams, 'path'>) =>
        bridge.invoke<{ content: string }>('file.read', {
            path,
            ...(opts || {}),
        }),
    write: (
        path: string,
        content: string,
        opts?: Omit<FileWriteParams, 'path' | 'content'>,
    ) =>
        bridge.invoke<BaseResponse & { bytesWritten?: number }>('file.write', {
            path,
            content,
            ...(opts || {}),
        }),
    exists: (path: string) =>
        bridge.invoke<{ exists: boolean }>('file.exists', { path }),
    list: (path: string, opts?: Omit<FileListParams, 'path'>) =>
        bridge.invoke<FileListResponse>('file.list', {
            path,
            ...(opts || {}),
        }),
    delete: (path: string, opts?: Omit<FileDeleteParams, 'path'>) =>
        bridge.invoke<BaseResponse>('file.delete', {
            path,
            ...(opts || {}),
        }),
    mkdir: (path: string) =>
        bridge.invoke<BaseResponse & { created?: boolean }>('file.mkdir', {
            path,
        }),
    copy: (
        source: string,
        destination: string,
        opts?: Omit<FileCopyParams, 'source' | 'destination'>,
    ) =>
        bridge.invoke<BaseResponse>('file.copy', {
            source,
            destination,
            ...(opts || {}),
        }),
    move: (source: string, destination: string) =>
        bridge.invoke<BaseResponse>('file.move', { source, destination }),
    rename: (path: string, newName: string) =>
        bridge.invoke<BaseResponse & { oldPath?: string; newPath?: string }>(
            'file.rename',
            { path, newName },
        ),
    getInfo: (path: string) =>
        bridge.invoke<FileGetInfoResponse>('file.getInfo', { path }),
    /**
     * Cancellable, non-blocking batch copy. The work runs on a host worker
     * thread, so copying a large album no longer freezes the UI the way
     * `file.copy` does.
     *
     * Returns a `{ operationId, totalCount }` receipt immediately; the outcome
     * arrives in batches on `file:opProgress`, followed by one
     * `file:opComplete`. Two paths skip that closing event - the host shutting
     * down mid-run, and an unexpected host-side failure - so a listener that
     * must not leak state should carry its own timeout rather than wait on it
     * forever.
     *
     * Both events go to the window that made the call while that window is
     * alive. Once it is gone the host can no longer resolve it and falls back
     * to the main instance, so a late event may surface in a window that did
     * not start the operation.
     *
     * One result is reported per entry, not per file: a directory entry is
     * reported once its whole tree has been walked. Copying a directory onto an
     * existing directory merges into it, and files already present there are
     * skipped without being reported individually, so the entry still reports
     * `status: 'ok'`. A file entry whose destination already exists is reported
     * as `skipped` / `already-exists` unless `overwrite` is set.
     *
     * Path validation is all-or-nothing: if any entry fails the host's read or
     * write check, the whole call is rejected with `PERMISSION_DENIED` and no
     * `operationId` is produced. At most 8 operations may be in flight
     * process-wide.
     *
     * @param items Source/destination pairs; must not be empty.
     * @param opts `overwrite` (default `false`) replaces existing destinations.
     * @returns Dispatch receipt; the actual results arrive by event.
     */
    copyAsync: (items: FileOpEntry[], opts?: FileOpAsyncOptions) =>
        bridge.invoke<FileCopyAsyncResponse>('file.copyAsync', {
            items,
            ...(opts || {}),
        }),
    /**
     * Cancellable, non-blocking batch move, with the same receipt-plus-events
     * contract as {@link file.copyAsync}.
     *
     * Within one volume a move is a rename and costs nothing regardless of
     * size. Across volumes the host falls back to copy-then-delete-source; that
     * entry still reports `status: 'ok'` but carries `reason: 'cross-volume'`
     * so the extra cost is visible. Unlike {@link file.copyAsync}, a directory
     * whose destination already exists is reported as `skipped` /
     * `already-exists` rather than merged.
     *
     * `overwrite` covers file destinations only. An existing *directory*
     * destination is never replaced, because Windows cannot swap a directory
     * in place: on the same volume such an entry ends as `skipped` or `failed`
     * instead of overwriting.
     *
     * @param items Source/destination pairs; must not be empty.
     * @param opts `overwrite` (default `false`) replaces an existing file
     *   destination. Note the synchronous `file.move` always replaces one.
     * @returns Dispatch receipt; the actual results arrive by event.
     */
    moveAsync: (items: FileOpEntry[], opts?: FileOpAsyncOptions) =>
        bridge.invoke<FileMoveAsyncResponse>('file.moveAsync', {
            items,
            ...(opts || {}),
        }),
    /**
     * Cancellable, non-blocking batch delete, with the same receipt-plus-events
     * contract as {@link file.copyAsync}. Results carry no `destination`.
     *
     * `moveToTrash: true` (the default) hands each path to the shell, which
     * requires the host's main thread, so those deletes run there in batches of
     * 16 and yield in between. `moveToTrash: false` deletes on a worker thread
     * and removes non-empty directories, which the synchronous `file.delete`
     * refuses to do in that mode.
     *
     * @param paths Paths to delete; must not be empty.
     * @param opts `moveToTrash` (default `true`) keeps deletions recoverable.
     * @returns Dispatch receipt; the actual results arrive by event.
     */
    deleteAsync: (paths: string[], opts?: FileDeleteAsyncOptions) =>
        bridge.invoke<FileDeleteAsyncResponse>('file.deleteAsync', {
            paths,
            ...(opts || {}),
        }),
    /**
     * Stop an operation started by {@link file.copyAsync},
     * {@link file.moveAsync} or {@link file.deleteAsync}.
     *
     * Cancellation takes effect part-way through a batch rather than at the end
     * of it. A copy or move stops within one file - the file in flight is
     * aborted and its partial copy removed; a delete stops at the next entry.
     * Entries already done keep their results, every remaining entry is
     * reported as `skipped` / `cancelled`, and the run still ends with a
     * `file:opComplete` carrying `cancelled: true`. Closing a popup cancels the
     * operations that popup started; a panel host has no such hook, so its
     * operations run to the end unless this method stops them.
     *
     * @param operationId The id from the dispatch receipt.
     * @returns `cancelled: false` when the operation had already finished or
     *   never existed; the two cases are deliberately indistinguishable.
     */
    cancelOp: (operationId: string) =>
        bridge.invoke<FileCancelOpResponse>('file.cancelOp', { operationId }),
    /** Read exact bytes; rejects on Host failure or malformed Base64. */
    readBinary: fileReadBinary,
    /** Write exact bytes using the host's `base64:` binary wire format. */
    writeBinary: fileWriteBinary,
    /** Write a canonical Base64 Data URL; rejects malformed input. */
    writeDataUrl: fileWriteDataUrl,
};
