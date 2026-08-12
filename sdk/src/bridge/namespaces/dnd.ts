/**
 * `dnd` — external file drop.
 *
 * Windows hands a dropped file list to the native window, not to the page, and
 * the HTML5 `File` object deliberately hides its filesystem path. This
 * namespace exposes the host's own view of a drag session so a page can obtain
 * real paths and pass them to playlist or library calls.
 *
 * Three ways to read paths, in decreasing reliability:
 *
 * 1. {@link dnd.getPathsAsync} — queries the host directly. Use this inside a
 *    HTML5 `drop` handler.
 * 2. The `dnd:drop` event payload — authoritative, but arrives on its own
 *    schedule relative to the page's `drop` handler.
 * 3. {@link dnd.getPaths} — synchronous snapshot read. Best-effort only.
 *
 * Paths are withheld from untrusted origins. Standard HTML5 drag events keep
 * working in that case, so check {@link dnd.getCapabilities} before showing UI
 * that depends on paths.
 */

import { bridge } from '../Bridge.js';
import type { DndCapabilities } from '../../types/responses.js';
import type {
    DndGetPathsAsyncResponse,
    DndStartDragResponse,
} from '../../types/generated/responses.js';

/**
 * Snapshot the host publishes to the page as `window.__fbDndSession`.
 *
 * Updated before any `dnd:*` listener runs, so a handler observes the session
 * it was notified about. Retained briefly after the session ends so a handler
 * that awaits something can still read it.
 */
interface DndSessionSnapshot {
    sessionId: string;
    paths: string[];
    hasFiles: boolean;
    phase: 'active' | 'ended';
    /** `performance.now()` when this snapshot was written. */
    publishedAt: number;
}

/** Reads the snapshot without assuming the host has published one yet. */
function readSnapshot(): DndSessionSnapshot | null {
    const slot = (globalThis as { __fbDndSession?: DndSessionSnapshot | null })
        .__fbDndSession;
    return slot ?? null;
}

export const dnd = {
    /**
     * Paths of the current drag session from the page-side snapshot.
     *
     * Synchronous and therefore best-effort: returns an empty array when no
     * session is active, when the drag carries no file list, when the origin is
     * not trusted with paths, or when the host has not published the session
     * yet. A fast drag can reach the page's `drop` handler before publication
     * completes, so do not use this as the only source of paths — prefer
     * {@link dnd.getPathsAsync} and keep this for optimistic UI.
     */
    getPaths: (): string[] => {
        const snap = readSnapshot();
        return snap ? snap.paths.slice() : [];
    },

    /**
     * Whether the snapshot says the current drag carries a file list.
     *
     * Useful during `dragover`, where the browser withholds
     * `dataTransfer.files` and exposes only `items.length`, leaving the page
     * unable to tell files from other payloads. Carries the same timing caveat
     * as {@link dnd.getPaths}; the authoritative value is the `hasFiles` field
     * of the `dnd:enter` payload.
     */
    hasFiles: (): boolean => {
        const snap = readSnapshot();
        return snap ? snap.hasFiles : false;
    },

    /**
     * Queries the host for a drag session's real filesystem paths.
     *
     * The reliable way to obtain paths from inside a HTML5 `drop` handler: it
     * reads the host's session state rather than a snapshot pushed to the page,
     * so it does not depend on message delivery order. Safe to call after
     * `await`, since it never touches `event.dataTransfer`.
     *
     * Paths come back in the same order as `DataTransfer.files`, so a page can
     * pair them by index.
     *
     * @param sessionId Session to query, from a `dnd:*` payload. Omit to query
     *                  the session that is active or most recently ended for
     *                  this window.
     * @returns Resolved session id and its paths. `paths` is empty when the
     *          session expired, carried no file list, or the origin is not
     *          trusted with paths.
     */
    getPathsAsync: (sessionId?: string) =>
        bridge.invoke<DndGetPathsAsyncResponse>(
            'dnd.getPathsAsync',
            sessionId ? { sessionId } : {},
        ),

    /**
     * What this window's drag-drop integration can currently deliver.
     *
     * Not constant for the window's lifetime: navigating to a different origin,
     * or Chromium re-registering its own drop target, can withdraw path access
     * while leaving HTML5 drag events intact. Subscribe to
     * `dnd:capabilitiesChanged` to react to that.
     */
    getCapabilities: () =>
        bridge.invoke<DndCapabilities>('dnd.getCapabilities'),

    /**
     * Dragging tracks out of the window into other applications.
     *
     * Not implemented: it requires a native `IDropSource`, which this component
     * does not provide. The returned promise always RESOLVES with a
     * `{ success: false, code: 'NOT_SUPPORTED' }` envelope rather than
     * rejecting, because the host delivers handler-returned error envelopes as
     * a normal result. Test `success`; a `catch` block will never run.
     *
     * @param _type Accepted and ignored, so existing call sites still compile.
     *              The host reads no parameters from this call.
     */
    startDrag: (_type?: string) =>
        bridge.invoke<DndStartDragResponse>('dnd.startDrag'),
};
