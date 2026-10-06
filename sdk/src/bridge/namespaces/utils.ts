import { bridge } from '../Bridge.js';
import { call } from '../call.js';
import type { TestEchoResponse, TestPingResponse } from '../../types/responses.js';

/**
 * `utils` — diagnostic / glue helpers.
 *
 * `formatTitle` / `getFileInfo` are intentional façades over
 * `titleformat.eval` / `metadata.read` respectively.
 */
export const utils = {
    // `test.ping` and `test.echo` have no declaration, so they go through the untyped invoke.
    /** Round trip through `test.ping`; `timestamp` is the host's Unix time in seconds. */
    ping: () => bridge.invoke<TestPingResponse>('test.ping'),
    /** Round trip through `test.echo`; the message comes back as `echo`. */
    echo: (message: string) => bridge.invoke<TestEchoResponse>('test.echo', { message }),
    /** Façade over `titleformat.eval`; an omitted or empty `path` evaluates the playing track. */
    formatTitle: (pattern: string, path?: string) =>
        call('titleformat.eval', {
            pattern,
            ...(path ? { path } : {}),
        }),
    /** Façade over `metadata.read`. */
    getFileInfo: (path: string) =>
        call('metadata.read', { path }),
};
