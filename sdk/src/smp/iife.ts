/**
 * IIFE entry — bootstrap the SMP compatibility layer at script load.
 *
 * Loaded via `<script src="dist/smp-compat.global.js">` after
 * `dist/bridge.global.js`. Reads `window.fb` (set by the bridge IIFE)
 * and feeds it to {@link bootstrapSmpCompat}, then hangs the result
 * onto `window.smp` / `window.plman` / `window.smpUtils` /
 * `window.utils` plus every wrapper class via
 * {@link installSmpGlobals}.
 *
 * If the bridge bundle has not loaded (no `window.fb`), the
 * bootstrap is skipped with a warning instead of throwing.
 */

import { bootstrapSmpCompat, installSmpGlobals } from './bootstrap.js';
import type { SmpBridgeShape } from './bridgeShape.js';

const LOG_PREFIX = '[SMP-Compat]';

(function autoBootstrap(): void {
    if (typeof globalThis === 'undefined') return;
    const g = globalThis as { fb?: unknown };
    const fb = g.fb as SmpBridgeShape | undefined;

    if (!fb || typeof fb.on !== 'function' || typeof fb.invoke !== 'function') {
        try {
            // eslint-disable-next-line no-console
            console.warn(
                LOG_PREFIX,
                'Bridge bundle not detected (fb.on / fb.invoke missing); SMP compatibility layer disabled.',
            );
        } catch {
            /* ignore */
        }
        return;
    }

    try {
        const result = bootstrapSmpCompat(fb);
        installSmpGlobals(result);
    } catch (e) {
        try {
            // eslint-disable-next-line no-console
            console.error(LOG_PREFIX, 'bootstrap failed:', e);
        } catch {
            /* ignore */
        }
    }
})();

// Re-export the bootstrap helper so consumers that load this bundle
// programmatically (rather than via `<script>`) can still reach the
// helpers from the resulting global slot.
export { bootstrapSmpCompat, installSmpGlobals } from './bootstrap.js';
export default bootstrapSmpCompat;
