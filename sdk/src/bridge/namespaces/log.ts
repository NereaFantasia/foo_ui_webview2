/**
 * `log` — log-file read/write namespace.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';
import type { LogWriteParams } from '../../types/generated/params.js';

export const log = {
    write: (message: string, opts?: LogWriteParams) =>
        bridge.invoke<BaseResponse & { path?: string }>('log.write', {
            message,
            ...(opts || {}),
        }),
    read: (lines?: number) =>
        bridge.invoke<{ lines: string[] }>('log.read', {
            ...(lines != null ? { lines } : {}),
        }),
    clear: () => bridge.invoke<BaseResponse>('log.clear'),
};
