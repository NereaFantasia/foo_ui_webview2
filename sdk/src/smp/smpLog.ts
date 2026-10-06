/**
 * Console helpers shared by the SMP compatibility layer.
 *
 * Every line is prefixed with {@link LOG_PREFIX} so SMP-layer output can
 * be told apart from theme and bridge logs in DevTools. The helpers
 * swallow anything thrown by `console` itself, so a missing or patched
 * console never interrupts the caller.
 */

/** First argument of every SMP-layer `console` call. */
export const LOG_PREFIX = '[SMP-Compat]';

/** `console.error` with {@link LOG_PREFIX} prepended; never throws. */
export function smpError(...args: unknown[]): void {
    try {
        // eslint-disable-next-line no-console
        console.error(LOG_PREFIX, ...args);
    } catch {
        /* ignore */
    }
}

/** `console.warn` with {@link LOG_PREFIX} prepended; never throws. */
export function smpWarn(...args: unknown[]): void {
    try {
        // eslint-disable-next-line no-console
        console.warn(LOG_PREFIX, ...args);
    } catch {
        /* ignore */
    }
}
