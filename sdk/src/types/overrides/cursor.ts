/**
 * Hand-written overrides for the `cursor.*` namespace.
 *
 * Companion to `src/api/CursorApi.cpp`. Cursor visibility is exposed
 * as an explicit JS → C++ channel because Visual Hosting
 * (CompositionController) leaves the WebView2 without an HWND of its
 * own — `WM_SETCURSOR` is dispatched to the parent foobar2000 frame
 * and Chromium's `CursorChanged` event cannot reflect a CSS
 * `cursor: none` rule reliably. Themes that want idle-hide behaviour
 * (full-screen players, desktop-lyrics overlays) must therefore opt
 * in via this namespace.
 *
 * The host scopes both the request and the follow-up
 * `cursor:hiddenChanged` event to the calling window, so multiple
 * popups can manage cursor state independently.
 */

/**
 * Parameters for `cursor.setHidden`.
 *
 * @codegen-override params:cursor.setHidden
 * @codegen-snapshot hidden:primitive
 */
export interface CursorSetHiddenParams {
    /** `true` to hide the client-area cursor; `false` to restore it. */
    hidden: boolean;
}

/**
 * Response from `cursor.setHidden`.
 *
 * `changed` is `true` only when the host actually flipped the
 * visibility flag — repeated calls with the same value resolve with
 * `success: true, changed: false`. Validation failures (missing /
 * non-boolean `hidden`, caller-window resolver miss) resolve with
 * `success: false` and a human-readable `error` string.
 *
 * @codegen-override response:cursor.setHidden
 * @codegen-snapshot changed:primitive,error:primitive,success:primitive
 */
export interface CursorSetHiddenResponse {
    /** `true` when the request succeeded. */
    success: boolean;
    /** `true` when the call actually flipped the visibility flag. */
    changed?: boolean;
    /** Human-readable failure description; only present when `success === false`. */
    error?: string;
}

/**
 * Response from `cursor.isHidden`.
 *
 * @codegen-override response:cursor.isHidden
 * @codegen-snapshot hidden:primitive
 */
export interface CursorIsHiddenResponse {
    /** Current hidden state of the calling window's client-area cursor. */
    hidden: boolean;
}

/**
 * Payload for `cursor:hiddenChanged`. Routed to the calling window
 * only — other popups do not receive cross-window cursor events.
 *
 * @codegen-override event:cursor:hiddenChanged
 * @codegen-snapshot hidden:primitive
 */
export interface CursorHiddenChangedPayload {
    /** New hidden state of the originating window. */
    hidden: boolean;
}
