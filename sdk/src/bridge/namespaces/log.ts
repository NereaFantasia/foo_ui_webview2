import { call } from '../call.js';
import type {
    LogWriteParams,
} from '../../types/generated/params.js';

/**
 * `log` — log-file read/write namespace.
 */
export const log = {
    /** Write a line to the component log file in the foobar2000 profile directory. */
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
    /** Empty the component log file (`webview_ui.log`). */
    clear: () => call('log.clear'),
};
