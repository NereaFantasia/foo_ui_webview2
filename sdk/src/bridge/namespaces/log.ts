import { call } from '../call.js';
import type {
    LogWriteParams,
} from '../../types/generated/params.js';

/**
 * `log` — log-file read/write namespace.
 */
export const log = {
    write: (message: string, opts?: LogWriteParams) =>
        call('log.write', {
            message,
            ...(opts || {}),
        }),
    /** `lines` and `totalLines` are absent when the log file does not exist yet. */
    read: (lines?: number) =>
        call('log.read', {
            ...(lines != null ? { lines } : {}),
        }),
    clear: () => call('log.clear'),
};
