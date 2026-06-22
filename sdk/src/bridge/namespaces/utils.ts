/**
 * `utils` — diagnostic / glue helpers.
 *
 * `formatTitle` / `getFileInfo` are intentional façades over
 * `titleformat.eval` / `metadata.read` respectively.
 */

import { bridge } from '../Bridge.js';
import type { MetadataReadResponse } from '../../types/responses.js';

export const utils = {
    ping: () => bridge.invoke<{ pong: boolean }>('test.ping'),
    echo: (message: string) =>
        bridge.invoke<{ message: string }>('test.echo', { message }),
    /** Façade over `titleformat.eval`. */
    formatTitle: (pattern: string, path?: string) =>
        bridge.invoke<{ result: string }>('titleformat.eval', {
            pattern,
            path,
        }),
    /** Façade over `metadata.read`. */
    getFileInfo: (path: string) =>
        bridge.invoke<MetadataReadResponse>('metadata.read', { path }),
};
