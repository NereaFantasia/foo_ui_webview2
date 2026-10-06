import { call } from '../call.js';
import type {
    ConsoleErrorParams,
    ConsoleLogParams,
    ConsoleWarnParams,
} from '../../types/generated/params.js';

/**
 * Builds the call parameters: the message alone as `message`, or, when extra
 * values follow, everything as `args`, because the host reads `args` only
 * when `message` is absent.
 */
function lineParams(
    message: string,
    args: NonNullable<ConsoleLogParams['args']>,
): ConsoleLogParams & ConsoleWarnParams & ConsoleErrorParams {
    return args.length > 0 ? { args: [message, ...args] } : { message };
}

/**
 * `consoleApi` — fb2k console-window logging.
 *
 * Each method writes one line. With only `message`, the line is the message
 * as it is. With extra values, the line is the message followed by each
 * value, separated by spaces: strings as they are, any other JSON value as
 * its JSON text, like the browser console.
 *
 * NB: file is named `consoleApi.ts` to avoid shadowing the global
 * `console` symbol in this module scope; consumed as `fb.console` on
 * the runtime aggregate object.
 */
export const consoleApi = {
    /** Writes a line prefixed with `[WebView]`. */
    log: (message: string, ...args: NonNullable<ConsoleLogParams['args']>) =>
        call('console.log', lineParams(message, args)),
    /** Writes a warning line prefixed with `[WebView][WARN]`. */
    warn: (message: string, ...args: NonNullable<ConsoleWarnParams['args']>) =>
        call('console.warn', lineParams(message, args)),
    /** Writes an error line prefixed with `[WebView][ERROR]`. */
    error: (message: string, ...args: NonNullable<ConsoleErrorParams['args']>) =>
        call('console.error', lineParams(message, args)),
};
