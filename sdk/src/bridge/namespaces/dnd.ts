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
 * A dragged shortcut puts the `.lnk` file itself in the list, which foobar2000
 * cannot play, so every path source above is joined by a parallel array of
 * shortcut targets: `resolvedPaths` on the payloads and on
 * {@link dnd.getPathsAsync}, and {@link dnd.getResolvedPaths} for the snapshot.
 *
 * Paths are withheld from untrusted origins. Standard HTML5 drag events keep
 * working in that case, so check {@link dnd.getCapabilities} before showing UI
 * that depends on paths.
 *
 * The page-side snapshot is published to the top-level document only, so
 * {@link dnd.getPaths} and {@link dnd.getResolvedPaths} answer with an empty
 * array inside an `<iframe>`. A framed page that needs paths has to receive
 * them from the main frame over `postMessage`.
 */

import { bridge } from '../Bridge.js';
import type { DndCapabilities, DndSessionPaths } from '../../types/responses.js';
import type { DndStartDragResponse } from '../../types/generated/responses.js';

/**
 * Snapshot the host publishes to the page as `window.__fbDndSession`.
 *
 * Updated before any `dnd:*` listener runs, so a handler observes the session
 * it was notified about. Retained briefly after the session ends so a handler
 * that awaits something can still read it.
 *
 * Published to the top-level document only. In an iframe the slot stays `null`
 * for the life of the document, so the synchronous accessors below answer with
 * an empty array there.
 */
interface DndSessionSnapshot {
    sessionId: string;
    paths: string[];
    /** Shortcut targets parallel to `paths`; see {@link dnd.getResolvedPaths}. */
    resolvedPaths: (string | null)[];
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
     * Shortcut targets for the current drag session, parallel to
     * {@link dnd.getPaths}.
     *
     * Windows puts the `.lnk` file itself in a dropped file list, which
     * foobar2000 cannot play, so the host reads each shortcut's target and
     * publishes it at the same index. The two arrays are always the same
     * length, and an entry is `null` whenever no target is available: the path
     * is not a shortcut, the shortcut names a shell namespace object such as
     * the recycle bin instead of a file, the recorded target is too long to
     * come back intact (Windows caps it at `MAX_PATH`, and a truncated path
     * would name a different file), COM was unavailable, or resolution was
     * skipped to keep the drop responsive. Never an empty string, so a
     * truthiness test is enough.
     *
     * A target says where the shortcut points, not that the file is there: a
     * BROKEN shortcut reports the path its `.lnk` recorded rather than `null`,
     * because Windows hands that path back whether or not the target still
     * exists, and the host cannot afford a filesystem check on the thread the
     * drag blocks. Expect a non-null entry to occasionally name nothing.
     *
     * Only `.lnk` is resolved. `.url`, `.library-ms` and virtual search results
     * report `null`.
     *
     * Synchronous snapshot read, so it carries the same timing caveat as
     * {@link dnd.getPaths} and returns an empty array in an iframe. The
     * `resolvedPaths` field of {@link dnd.getPathsAsync} and of the `dnd:enter`
     * / `dnd:drop` payloads is the reliable equivalent.
     *
     * ```js
     * const paths = fb.dnd.getPaths();
     * const targets = fb.dnd.getResolvedPaths();
     * const playable = paths.map((p, i) => targets[i] ?? p);
     * ```
     */
    getResolvedPaths: (): (string | null)[] => {
        const snap = readSnapshot();
        if (!snap) {
            return [];
        }
        // Padded from paths rather than returned as-is, so a host that predates
        // this field yields nulls of the right length instead of a short array
        // that would silently misalign an index-paired loop.
        const resolved = snap.resolvedPaths;
        return Array.isArray(resolved)
            ? snap.paths.map((_, i) => resolved[i] ?? null)
            : snap.paths.map(() => null);
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
     * pair them by index. `resolvedPaths` carries the `.lnk` target for each
     * index, or `null`, and is always the same length as `paths`.
     *
     * Reads host memory only: the shortcut targets were resolved once when the
     * drag arrived, so calling this repeatedly costs no filesystem access.
     *
     * @param sessionId Session to query, from a `dnd:*` payload. Omit to query
     *                  the session that is active or most recently ended for
     *                  this window.
     * @returns Resolved session id, its paths, and the parallel shortcut
     *          targets. Both arrays are empty when the session expired, carried
     *          no file list, or the origin is not trusted with paths.
     */
    getPathsAsync: (sessionId?: string) =>
        bridge.invoke<DndSessionPaths>(
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
