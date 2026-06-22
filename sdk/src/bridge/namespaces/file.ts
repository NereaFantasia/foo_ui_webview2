/**
 * `file` — file-system namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    FileListResponse,
    FileGetInfoResponse,
} from '../../types/responses.js';
import type {
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
    delete: (path: string) =>
        bridge.invoke<BaseResponse>('file.delete', { path }),
    mkdir: (path: string) =>
        bridge.invoke<BaseResponse & { created?: boolean }>('file.mkdir', {
            path,
        }),
    copy: (source: string, destination: string) =>
        bridge.invoke<BaseResponse>('file.copy', { source, destination }),
    move: (source: string, destination: string) =>
        bridge.invoke<BaseResponse>('file.move', { source, destination }),
    rename: (path: string, newName: string) =>
        bridge.invoke<BaseResponse & { oldPath?: string; newPath?: string }>(
            'file.rename',
            { path, newName },
        ),
    getInfo: (path: string) =>
        bridge.invoke<FileGetInfoResponse>('file.getInfo', { path }),
};
