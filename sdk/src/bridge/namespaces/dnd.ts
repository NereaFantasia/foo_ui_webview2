import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import type { DndDragEndedPayload } from '../../types/events.js';

/**
 * Marks the `text/plain` entry of a drag as carrying a drag token rather than
 * page text. This exact prefix is part of the host's drag-token protocol;
 * changing it prevents the host from attaching files to the drag.
 */
const DRAG_TOKEN_PREFIX = 'fb2k-dnd-token/1:';

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
 * A drop lands only where the page accepts it by calling `preventDefault()` in
 * its HTML5 `dragover` handler. Elsewhere the host refuses it: the cursor shows
 * "forbidden" and no `dnd:drop` follows, except for a drop released in the
 * first half second, before the page has answered.
 *
 * Paths are withheld from untrusted origins. Standard HTML5 drag events keep
 * working in that case, so check {@link dnd.getCapabilities} before showing UI
 * that depends on paths.
 *
 * The page-side snapshot is published to the top-level document only, so
 * {@link dnd.getPaths} and {@link dnd.getResolvedPaths} answer with an empty
 * array inside an `<iframe>`. A framed page that needs paths has to receive
 * them from the main frame over `postMessage`.
 *
 * The reverse direction, dragging tracks OUT of the window into Explorer or
 * another application, goes through {@link dnd.prepareDrag} and
 * {@link dnd.applyDragToken}; see those for the handshake.
 */
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
        call(
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
        call('dnd.getCapabilities'),

    /**
     * Exchanges a list of tracks for a one-shot token that lets the next drag
     * out of this window carry those files.
     *
     * The token, not the paths, is what the page hands to the drag: call this
     * BEFORE the drag begins (on `pointerdown` or `mousedown`), keep the token,
     * and in the `dragstart` handler pass it synchronously to
     * {@link dnd.applyDragToken}. Awaiting inside `dragstart` is too late: the
     * drag data store is writable only while that handler runs synchronously,
     * so a token written after an `await` is silently dropped and the drag
     * goes out empty. A very fast press-and-move can also outrun this call, in
     * which case the drag has no token yet; a page can detect that and treat
     * the drag as an ordinary one.
     *
     * Token rules: valid for 30 seconds, spent by the first drag that uses it,
     * bound to the window that minted it, and superseded by the next successful
     * call from the same window (only the most recent token is live). Any other use is
     * refused at drag start with a `dnd:dragEnded` of `PERMISSION_DENIED`, and the
     * drag goes on as an ordinary page drag without files.
     *
     * What lands at the drop target is always a physical file. A track inside
     * a cue sheet or a multi-track container drags the whole container (a
     * `path|subsong:N` entry loses its suffix), an `archive://` or `unpack://`
     * entry drags the whole archive, and several requested entries can
     * collapse into one file; the container-internal position is not carried.
     * Paths are validated the same way as for `queue.addPaths`, after being
     * normalised to native paths. Whether the file still exists is not
     * checked: a stale path can get a token, but an accepted drop does not
     * guarantee that the target copied a file.
     *
     * Handler failures resolve with an error envelope rather than rejecting;
     * transport failures can still reject. Codes:
     * `NOT_FOUND` (calling window has no drag-drop registration),
     * `ORIGIN_DENIED` (document origin not trusted to drag files out; checked
     * before any path is inspected), `NOT_SUPPORTED` (see `dragOut` on
     * {@link dnd.getCapabilities}), `INVALID_PARAMS` (`paths` missing, empty
     * or not all strings), `INVALID_PATH` (an entry has no local file behind
     * it, such as a stream or a `cdda://` track; `paths[i]` names the offending
     * index in the list you passed), `PERMISSION_DENIED` (an entry was refused
     * by path security; here `paths[i]` indexes the normalised, de-duplicated
     * list, which may be shorter than yours), `OPERATION_FAILED` (a token
     * could not be created). Error messages never contain a path.
     *
     * @param paths Locations to drag out: native paths, `file://` URLs,
     *              `file-relative://` URLs (what a portable install reports for
     *              media on the program's volume, resolved by foobar2000),
     *              `archive://` / `unpack://` entries, `path|subsong:N`. A
     *              track's `path` and its `absolutePath` are both accepted.
     * @returns The token to hand to {@link dnd.applyDragToken}.
     *
     * @example
     *   let token = null;
     *   el.addEventListener('pointerdown', async () => {
     *     token = null;
     *     const r = await fb.dnd.prepareDrag([trackPath]);
     *     if (r.success) token = r.token;
     *   });
     *   el.addEventListener('dragstart', (e) => {
     *     if (token) fb.dnd.applyDragToken(e.dataTransfer, token);
     *   });
     *   el.addEventListener('dragend', (e) => {
     *     // 'copy' when a target took the files, 'none' otherwise.
     *     console.log(e.dataTransfer.dropEffect);
     *   });
     */
    prepareDrag: (paths: string[]) =>
        call('dnd.prepareDrag', { paths }),

    /**
     * Writes a drag token into a `dragstart` event's data transfer so the host
     * attaches the token's files to the drag.
     *
     * Must run synchronously inside the `dragstart` handler. It sets the
     * `text/plain` entry to the token carrier, replacing anything the page put
     * there, and sets `effectAllowed` to `'copy'`. Both are required: the host
     * identifies a drag-out by that `text/plain` marker, and it attaches no
     * files to a drag whose allowed effects are wider than copy, because a drop
     * target given a move effect would relocate the user's files. Do not change
     * `effectAllowed` afterwards.
     *
     * The `text/plain` slot is therefore not available for page text during a
     * drag-out. Text-oriented drop targets receive an empty string, never the
     * token.
     *
     * Once the host accepts the token, the drag proceeds like any other page
     * drag: it draws the drag image, and the page's `dragend` reports the
     * outcome through `dataTransfer.dropEffect`. When the host refuses the
     * token or the effects, the drag still proceeds, as a page drag carrying no
     * files, and `dnd:dragEnded` says why. While a drag is in progress the host
     * window waits for the drop target, exactly as it does for a plain HTML5
     * drag.
     *
     * @param dataTransfer `event.dataTransfer` of the `dragstart` event.
     * @param token        Token from {@link dnd.prepareDrag}.
     */
    applyDragToken: (dataTransfer: DataTransfer, token: string): void => {
        dataTransfer.setData('text/plain', DRAG_TOKEN_PREFIX + token);
        dataTransfer.effectAllowed = 'copy';
    },

    /**
     * Subscribes to `dnd:dragEnded`, which fires only when the host declines,
     * at the moment the drag starts, to attach files to a drag-out. The drag
     * itself goes on as an ordinary page drag without files, so a drop inside
     * the page still works; it is cancelled only in the rare case that the
     * host cannot remove the token text from the drag data.
     *
     * A successful hand-over produces no event; watch the element's own
     * `dragend` for the outcome. The payload's `code` tells the page whether
     * to mint a new token (`PERMISSION_DENIED`) or fix its `dragstart`
     * handler (`INVALID_PARAMS`). `OPERATION_FAILED` means the file list
     * could not be attached. A valid token is consumed before these last
     * two checks, so request a fresh token before retrying either failure.
     *
     * @returns Unsubscribe function.
     */
    onDragEnded: (handler: (payload: DndDragEndedPayload) => void) =>
        subscribe('dnd:dragEnded', handler),

    /**
     * Report that programmatically starting a native drag is unsupported.
     *
     * This endpoint does not start a drag. The host handler resolves with a
     * `{ success: false, code: 'NOT_SUPPORTED' }` envelope rather than
     * rejecting, because the host delivers handler-returned error envelopes as
     * a normal result. Test `success`; transport failures can still reject.
     *
     * Use {@link dnd.prepareDrag} with {@link dnd.applyDragToken} instead.
     *
     * @param _type Accepted but ignored; the host reads no parameters.
     */
    startDrag: (_type?: string) =>
        call('dnd.startDrag'),
};
