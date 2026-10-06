/**
 * SDK accessor for web components.
 *
 * Components never construct a bridge of their own: a second `Bridge`
 * instance would double-subscribe every native event. They use the SDK
 * instance the theme already loaded, resolved in this order:
 *
 * 1. The instance bound with {@link bindFb}. The ESM entry's
 *    `registerComponents()` binds the `fb` object exported by
 *    `foo-webview-sdk`, so bundled themes need no global.
 * 2. `window.fb`, installed by `bridge.global.js` for `<script>`-tag themes.
 *
 * The lookup is deferred to the first `connectedCallback`, so the load order
 * of the bridge and component `<script>` tags does not matter.
 */

/**
 * Type alias for the aggregate `fb` runtime object exported from
 * `../bridge/index.ts`. Pure type-space import — no value emit.
 */
type FbApi = typeof import('../bridge/index.js').fb;

let _cached: FbApi | null = null;

/**
 * Make every component use `fb`, ahead of anything on `window.fb`.
 *
 * @internal
 */
export function bindFb(fb: FbApi): void {
    _cached = fb;
}

/**
 * Resolve the SDK the components talk to: the instance bound with
 * {@link bindFb}, otherwise `window.fb`.
 *
 * `window.fb` counts only when it has the SDK's `on` and `invoke` functions.
 * Inside foobar2000 the host injects a smaller `window.fb` of its own, without
 * `invoke` or the namespaces, which stays there until `bridge.global.js` loads.
 * Nothing is cached until a usable SDK is found.
 *
 * @returns the aggregate `fb` SDK surface
 * @throws Error outside a browser, or when neither source holds the SDK.
 */
export function getFb(): FbApi {
    if (_cached) return _cached;
    if (typeof window === 'undefined') {
        throw new Error(
            'foo-webview-sdk components: not running in a browser context (window is undefined).',
        );
    }
    const candidate = (window as Window & { fb?: Partial<FbApi> }).fb;
    if (
        !candidate ||
        typeof candidate.on !== 'function' ||
        typeof candidate.invoke !== 'function'
    ) {
        throw new Error(
            'foo-webview-sdk components: the SDK is not loaded. Load bridge.global.js before ' +
                'mounting fb-* elements, or register them with registerComponents() from ' +
                "'foo-webview-sdk/components'.",
        );
    }
    _cached = candidate as FbApi;
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
