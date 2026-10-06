import { call } from '../call.js';

/**
 * `cursor` — explicit client-area cursor visibility control.
 *
 * Companion to `src/api/CursorApi.cpp`. Visual Hosting
 * (CompositionController) leaves the WebView2 without an HWND of its
 * own — `WM_SETCURSOR` is dispatched to the parent foobar2000 frame
 * and Chromium's `CursorChanged` callback cannot reflect a CSS
 * `cursor: none` rule reliably. Themes that want idle-hide behaviour
 * (full-screen players, desktop-lyrics overlays) drive the host
 * directly through this namespace.
 *
 * State is per-window: each popup tracks its own hidden flag and the
 * `cursor:hiddenChanged` event is routed only to the originating
 * window.
 */
export const cursor = {
    /**
     * Hide or restore the calling window's client-area cursor.
     *
     * Repeated calls with the same `hidden` value resolve with
     * `success: true, changed: false` — only flips that actually move
     * the visibility flag emit a follow-up `cursor:hiddenChanged`
     * event. When the host cannot resolve the calling window the call
     * fails with `code: 'OPERATION_FAILED'`.
     *
     * @param hidden - `true` to hide, `false` to restore.
     */
    setHidden: (hidden: boolean) =>
        call('cursor.setHidden', {
            hidden,
        }),

    /**
     * Read the current hidden state for the calling window. Reports
     * `hidden: false` when the caller window cannot be resolved.
     */
    isHidden: () =>
        call('cursor.isHidden'),
};
