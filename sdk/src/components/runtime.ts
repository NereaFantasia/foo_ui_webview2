/**
 * Lazy `window.fb` accessor for web components.
 *
 * Components MUST NOT bundle a copy of the bridge — themes load
 * `bridge.global.js` and `components.global.js` as separate `<script>`
 * tags, and a duplicate `Bridge` instance would double-subscribe every
 * native event. This helper resolves `window.fb` lazily at first call
 * and caches it for subsequent reads.
 *
 * The lookup is deferred to the first `connectedCallback` so that the
 * load order between the bridge `<script>` and the components
 * `<script>` does not matter — components register synchronously and
 * resolve `window.fb` only when they actually need it.
 */

/**
 * Type alias for the aggregate `fb` runtime object exported from
 * `../bridge/index.ts`. Pure type-space import — no value emit.
 */
type FbApi = typeof import('../bridge/index.js').fb;

let _cached: FbApi | null = null;

/**
 * Resolve the `window.fb` runtime singleton. Throws a descriptive
 * error when called outside a browser context or before the bridge
 * IIFE has installed `window.fb`.
 *
 * @returns the aggregate `fb` SDK surface (typed, never `undefined`)
 */
export function getFb(): FbApi {
    if (_cached) return _cached;
    if (typeof window === 'undefined') {
        throw new Error(
            'foo-webview-sdk components: not running in a browser context (window is undefined).',
        );
    }
    const w = window as Window & { fb?: FbApi };
    if (!w.fb) {
        throw new Error(
            'foo-webview-sdk components: window.fb is missing. Load bridge.global.js (or import the bridge entry) before mounting fb-* components.',
        );
    }
    _cached = w.fb;
    return _cached;
}

/**
 * Test-only escape hatch. Resets the cached `fb` reference so a unit
 * test can inject a fresh mock between cases. Not part of the public
 * runtime contract — consumers must not rely on this.
 *
 * @internal
 */
export function _resetFbCacheForTests(): void {
    _cached = null;
}
