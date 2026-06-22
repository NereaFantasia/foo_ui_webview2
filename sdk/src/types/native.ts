/**
 * Ambient type declarations for the host-injected `window.fb2k` bridge.
 *
 * The fb2k host (foo_ui_webview2.dll) injects `window.fb2k` into every
 * WebView2 instance during initialization. All SDK calls flow through
 * the four methods declared here.
 *
 * The transitional `window._nativeFb2k` alias is also typed because
 * the runtime probes both names.
 */

/**
 * Native bridge surface injected by the host into the WebView2 page.
 *
 * Every method shape is intentionally untyped on the value side
 * (`unknown` / `Record<string, unknown>`); higher-level wrappers in
 * `src/bridge/Bridge.ts` narrow the types using the response
 * interfaces from {@link "./responses"} and the payload map from
 * {@link "./events"}.
 */
export interface NativeFb2k {
    /**
     * Invoke a registered C++ API by canonical `<namespace>.<method>` name.
     *
     * @param method Canonical API name in dot notation (e.g. `playback.play`).
     * @param params Optional parameter object; structure determined by the
     *               registered C++ handler.
     * @returns Promise resolving to the handler's JSON-serialised return
     *          value, or rejecting with an `ErrorEnvelope`-shaped object.
     */
    invoke<TResp = unknown>(
        method: string,
        params?: object,
    ): Promise<TResp>;

    /**
     * Subscribe to a C++-emitted event.
     *
     * @param event   Canonical event name in colon notation (e.g.
     *                `playback:stateChanged`).
     * @param handler Handler invoked with the event payload on every dispatch.
     * @returns       Unsubscribe callback. Calling the returned function
     *                detaches the handler.
     */
    on(event: string, handler: (data: unknown) => void): () => void;

    /**
     * Detach a previously registered event handler.
     *
     * @param event   Same canonical name used in {@link NativeFb2k.on}.
     * @param handler Reference to the same handler function instance that was
     *                originally subscribed.
     */
    off(event: string, handler: (data: unknown) => void): void;

    /**
     * One-shot subscription; the handler is automatically unsubscribed after
     * the first dispatch.
     */
    once(event: string, handler: (data: unknown) => void): () => void;
}

declare global {
    interface Window {
        /** Canonical native bridge handle (host-injected). */
        fb2k?: NativeFb2k;

        /**
         * Cached alias retained for back-compat with consumers that
         * probe both names.
         *
         * @deprecated Use {@link Window.fb2k} instead.
         */
        _nativeFb2k?: NativeFb2k;
    }
}
