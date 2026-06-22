/**
 * `consoleApi` — fb2k console-window logging.
 *
 * NB: file is named `consoleApi.ts` to avoid shadowing the global
 * `console` symbol in this module scope; consumed as `fb.console` on
 * the runtime aggregate object.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

export const consoleApi = {
    log: (message: string) =>
        bridge.invoke<BaseResponse>('console.log', { message }),
    warn: (message: string) =>
        bridge.invoke<BaseResponse>('console.warn', { message }),
    error: (message: string) =>
        bridge.invoke<BaseResponse>('console.error', { message }),
};
