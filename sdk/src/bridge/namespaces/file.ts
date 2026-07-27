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
    FileCopyParams,
    FileDeleteParams,
    FileListParams,
    FileReadParams,
    FileWriteParams,
} from '../../types/generated/params.js';

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
    /** Read exact bytes; rejects on Host failure or malformed Base64. */
    readBinary: fileReadBinary,
    /** Write exact bytes using the host's `base64:` binary wire format. */
    writeBinary: fileWriteBinary,
    /** Write a canonical Base64 Data URL; rejects malformed input. */
    writeDataUrl: fileWriteDataUrl,
};
