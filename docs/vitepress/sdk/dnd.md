# fb.dnd Drag and Drop

`fb.dnd` exposes the host's view of an external file drag so a page can obtain
real filesystem paths, which the HTML5 `File` object deliberately withholds. It also
lets a page drag tracks *out* of the window as real files; see
[Dragging files out](#dragging-files-out).

## How it works

Windows delivers a dropped file list to the native window, not to the page, and
`File.path` is always `null` in a browser engine. This namespace does not replace
HTML5 drag and drop: the standard `dragenter` / `dragover` / `drop` events keep
firing exactly as before, and `fb.dnd` runs alongside them as a side channel that
answers the one question HTML5 cannot — where the files actually live on disk.

The host tracks each drag as a *session* with an id, and publishes it to the
top-level document as `window.__fbDndSession` before any `dnd:*` listener runs.

## Accepting a drop

A page accepts a drop the HTML5 way: `dragover` calls `preventDefault()` over the
element that takes it. Where no handler does, the host refuses the drop for the
page. The cursor shows "forbidden", releasing there drops nothing, emits no
`dnd:drop`, and prevents the browser from opening the dropped file. Text fields
keep their default text drop.

During the first 500 ms after entering the window, the cursor defaults to copy before the page responds, preventing transient flashes of the forbidden icon; drops during this interval are delivered normally. Thereafter, the cursor updates dynamically based on page responses.

```javascript
document.getElementById('playlist')?.addEventListener('dragover', (event) => {
    if (fb.dnd.hasFiles()) {
        event.preventDefault(); // accept here; everywhere else is refused
        if (event.dataTransfer) event.dataTransfer.dropEffect = 'copy';
    }
});
```

## Reading paths

Three ways, in decreasing reliability.

### getPathsAsync(sessionId?)

Signature: `fb.dnd.getPathsAsync(sessionId?: string): Promise<DndGetPathsAsyncResponse>`

Queries the host directly. This is the reliable choice, and the one to use inside
a HTML5 `drop` handler: it reads host state rather than a snapshot pushed to the
page, so it does not depend on message delivery order. Safe to call after `await`,
since it never touches `event.dataTransfer`.

Paths come back in the same order as `DataTransfer.files`, so a page can pair them
by index.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | No | Session to query, from a `dnd:*` payload. Omit to query the session that is active or most recently ended for this window |

Returns `{ sessionId, paths, resolvedPaths }`. `paths` and `resolvedPaths` are
always the same length, and both are empty when the session expired, carried no
file list, or the origin is not trusted with paths. See
[Shortcut targets](#shortcut-targets) for `resolvedPaths`.

Reads host memory only. The shortcut targets were resolved once when the drag
arrived, so calling this repeatedly costs no filesystem access.

```javascript
document.addEventListener('drop', async (event) => {
    event.preventDefault();
    const res = await fb.dnd.getPathsAsync();
    if (res.success === false) throw new Error(res.error);
    const { paths } = res;
    if (paths.length) {
        await fb2k.invoke('playlist.addPaths', { paths });
    }
});
```

### The `dnd:drop` event

Authoritative, because the drop payload carries the final list — a source may
change it after `dnd:enter`. It arrives on its own schedule relative to the page's
own `drop` handler, so treat it as a notification rather than a replacement for
the handler.

```javascript
fb.on('dnd:drop', (data) => {
    console.log(data.sessionId, data.paths, data.x, data.y);
});
```

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | Correlates `dnd:enter`, `dnd:leave` and `dnd:drop` for one drag gesture |
| `paths` | `string[]` | Absolute filesystem paths, in `DataTransfer.files` order; empty when withheld |
| `resolvedPaths` | `(string \| null)[]` | Shortcut target per index, or `null`. Always the same length as `paths`. See [Shortcut targets](#shortcut-targets) |
| `x`, `y` | `number` | Cursor position in client-area physical pixels — divide by `devicePixelRatio` for CSS pixels |
| `keyState` | `number` | Win32 `MK_*` modifier / mouse-button mask at drop time |
| `source` | `'self' \| 'other-window' \| 'external'` | Where the drag came from. See [Where a drag came from](#where-a-drag-came-from) |

`dnd:enter` carries `sessionId`, `paths`, `resolvedPaths`, `hasFiles`, `source` and
the same cursor fields; `dnd:leave` carries only `sessionId`.

### Where a drag came from

The `source` field in `dnd:enter`, `dnd:drop`, and `getPathsAsync` distinguishes between gestures originating from the current window and files dragged from outside. If the page already handles internal drag-and-drop via HTML5 events, this property allows ignoring duplicate host-dispatched paths:

| Value | The drag started in |
| --- | --- |
| `'self'` | this window's page, with or without a drag token |
| `'other-window'` | another window of the same foobar2000: the main window or a popup |
| `'external'` | anywhere else: Explorer, another application, another foobar2000 process, or a Default UI / Columns UI panel |

The host tags drag gestures initiated within its managed windows and reads the tag when entering destination windows. Because external applications could fabricate similar tags, `source` should only guide drop handling and must not serve as a security boundary.

```javascript
fb.on('dnd:drop', ({ source, paths }) => {
    if (source === 'self') return; // the page's own drop handler has it
    void fb.queue.addPaths(paths);
});
```

### getPaths()

Signature: `fb.dnd.getPaths(): string[]`

Synchronous snapshot read, and therefore best-effort. Returns an empty array when
no session is active, when the drag carries no file list, when the origin is not
trusted with paths, or when the host has not published the session yet — a fast
drag can reach the page's `drop` handler before publication completes. This method is
recommended only for optimistic UI rendering; use `getPathsAsync` for reliable data fetching.

```javascript
const optimistic = fb.dnd.getPaths();
```

### hasFiles()

Signature: `fb.dnd.hasFiles(): boolean`

Useful during `dragover`, where the browser withholds `dataTransfer.files` and
exposes only `items.length`, leaving the page unable to tell a file drag from
other payloads. Carries the same timing caveat as `getPaths`; the authoritative
value is the `hasFiles` field of the `dnd:enter` payload.

```javascript
element.addEventListener('dragover', (event) => {
    if (fb.dnd.hasFiles()) {
        event.preventDefault();
    }
});
```

## Shortcut targets

Dragging a shortcut puts the `.lnk` file itself in the dropped list, and
foobar2000 cannot play it directly. So each path source above is accompanied by a corresponding
array of shortcut targets: `resolvedPaths` on the `dnd:enter` / `dnd:drop`
payloads and on `getPathsAsync`, and `getResolvedPaths()` for the snapshot.

`paths` is never rewritten. Its indices align strictly with `DataTransfer.files`; resolved targets are provided alongside original paths rather than replacing them, allowing applications to select the appropriate path.

### getResolvedPaths()

Signature: `fb.dnd.getResolvedPaths(): (string | null)[]`

Always the same length as `getPaths()`. An entry is `null` whenever no target is
available:

- the path is not a `.lnk` shortcut
- the shortcut points at a shell namespace object such as the recycle bin rather
  than at a file
- the recorded target is too long to be read back intact. Windows caps the target
  it hands out at `MAX_PATH`, and a truncated path would name a *different* file,
  so it is refused rather than reported
- COM was not available on the host thread that reads shortcuts
- resolution was skipped to keep the drop responsive

`null` is used in every one of those cases — never an empty string — so a
truthiness test is enough to tell "resolved" from "not resolved".

::: warning Resolved targets represent recorded paths and do not guarantee file existence
Broken shortcuts do not return `null`. Windows returns the static target path recorded inside the `.lnk` file regardless of whether the file still exists on disk, so a non-null entry indicates only that a target path was recorded.

The host deliberately omits disk probing to avoid blocking the drag-source UI thread during synchronous drag interactions. Applications should handle nonexistent targets when consuming paths, for example by checking `addedCount` and `invalidCount` returned by `playlist.addPaths`.
:::

```javascript
const paths = fb.dnd.getPaths();
const targets = fb.dnd.getResolvedPaths();

// A shortcut plays its target; anything else plays itself
const playable = paths.map((path, i) => targets[i] ?? path);
await fb2k.invoke('playlist.addPaths', { paths: playable });
```

Synchronous snapshot read, so it carries the same timing caveat as `getPaths()`.
The reliable equivalent is the `resolvedPaths` field of `getPathsAsync()`.

Only `.lnk` is resolved. `.url` internet shortcuts, `.library-ms` library
definitions and virtual search results all report `null`.

::: tip Why resolution can be skipped
The host reads shortcut targets on its UI thread while the source application
waits for the drop to be accepted, so blocking there would freeze drag and drop
system-wide. A shortcut pointing at an unreachable network share can block for
seconds inside Windows, with no way to interrupt it. The host therefore works to
a time budget for the whole drop and reports `null` for whatever is left, rather
than making the user wait. Ordinary files never consume any of that budget.
:::

## Capabilities

### getCapabilities()

Signature: `fb.dnd.getCapabilities(): Promise<DndGetCapabilitiesResponse>`

What this window's integration can currently deliver. Not constant for the
window's lifetime: navigating to a different origin can withdraw path access while
leaving HTML5 drag events intact.

| Field | Type | Description |
| --- | --- | --- |
| `html5` | `boolean` | Page still receives standard HTML5 drag events |
| `paths` | `boolean` | Real filesystem paths are obtainable |
| `hosting` | `'visual' \| 'standard'` | How the window hosts its WebView |
| `pathsUnavailableReason` | `string` | Present only when `paths` is `false` |
| `dragOut` | `boolean` | Files can be dragged out through [`prepareDrag`](#preparedrag-paths) |
| `dragOutUnavailableReason` | `string` | Present only when `dragOut` is `false` |

```javascript
const caps = await fb.dnd.getCapabilities();
if (caps.success === false) throw new Error(caps.error);
if (!caps.paths) {
    console.warn('paths unavailable:', caps.pathsUnavailableReason);
}
```

`pathsUnavailableReason` is one of `origin-untrusted`, `inner-target-not-found`,
`forward-unavailable`, `chain-failed`, or `register-failed`.

`dragOutUnavailableReason` is one of `not-visual-hosting` (a DUI / CUI panel, where
Chromium owns the drag source), `runtime-too-old` (the WebView2 Runtime predates
the drag-start event this feature needs; Edge 144 or newer is required), or
`register-failed`. `paths` and `dragOut` are independent: check the one you need.

Subscribe to `dnd:capabilitiesChanged` to react to a change. Its payload carries
the same `html5`, `paths`, `hosting`, `pathsUnavailableReason`, `dragOut` and
`dragOutUnavailableReason` fields.

```javascript
fb.on('dnd:capabilitiesChanged', (caps) => {
    dropZoneEl.hidden = !caps.paths;
});
```

## Where paths are available

| Host | HTML5 drag events | Real paths |
| --- | --- | --- |
| Main window, popup window (`hosting: 'visual'`) | Yes | Yes |
| DUI / CUI panel (`hosting: 'standard'`) | Yes | No — reports `inner-target-not-found` |

A panel's WebView owns its drop target in a separate process, which the host
cannot take over. Check `getCapabilities()` before showing UI that depends on
paths, rather than assuming the outcome from the window type.

Paths are also withheld from untrusted origins. HTML5 drag events keep working in
that case, so a page that hides all drop affordances when `paths` is `false` loses
working functionality — branch on the two flags independently.

### Iframes

Paths are published to the top-level document only. Inside an `<iframe>`,
`window.__fbDndSession` stays `null`, and `getPaths()` / `getResolvedPaths()`
return an empty array rather than throwing. A framed page that needs paths must
receive them from the main frame over `postMessage`, which puts the decision to
share them where it belongs.

## Dragging files out

Reverse drag-out: allows users to drag draggable page elements directly into Windows File Explorer or external applications as real files. The page never directly handles raw paths during the drag gesture; instead, it requests a single-use token in advance. When the drag starts, the token is written to DataTransfer, and the host resolves it to the validated file list. Standard browser drag mechanics manage the remaining interaction, and the outcome is reported via `dragend`.

Check `dragOut` on [`getCapabilities()`](#getcapabilities) before offering the
gesture.

### prepareDrag(paths)

Signature: `fb.dnd.prepareDrag(paths: string[]): Promise<DndPrepareDragResponse>`

Validates the paths and returns a token for the next drag out of this window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Locations to drag out, at least one. Accepts native paths, `file://` URLs, `file-relative://` URLs (portable-mode relative paths resolved by foobar2000), `archive://` and `unpack://` entries, and `path\|subsong:N` syntax (the subsong suffix is stripped to drag the container); accepts either `path` or `absolutePath` from track objects |

Returns `{ success: true, token }`. The token is opaque; the only thing to do with
it is pass it to [`applyDragToken`](#applydragtoken-datatransfer-token).

::: warning Call before drag begins, not inside `dragstart`
The drag data store is writable only during synchronous execution of the `dragstart` handler. Performing `await` inside `dragstart` causes subsequent `setData` calls to fail silently, resulting in drops without files. Request and cache tokens during `pointerdown` (or `mousedown`), then inject the token synchronously inside `dragstart`. If dragging begins before token acquisition completes, the operation proceeds as a standard web drag gesture.
:::

Token rules:

- valid for **30 seconds**
- **spent** by the first drag that uses it
- bound to the **window** that requested it
- **superseded** by the next successful `prepareDrag` from the same window, even if it has
  not expired — only the most recent token is live

A token that breaks any of these is refused when the drag starts, with a
`dnd:dragEnded` of `PERMISSION_DENIED`, and the drag goes on as an ordinary page
drag without files.

::: tip What arrives at the target is always a physical file
A track inside a CUE sheet or multi-track container exports the underlying container file; `archive://` and `unpack://` items export the entire archive file. Multiple track entries may coalesce into a single physical file, and internal cuesheet/subsong offsets do not accompany the exported files.
:::

Paths are normalised to native paths and then validated the same way as for
`queue.addPaths`. Handler failures resolve with an error envelope rather than
rejecting; test `success`. Transport failures can still reject.

::: warning No existence check
Validation does not guarantee that the file exists. A deleted or renamed file
can still get a token, but an accepted drop does not prove that the target copied
it. If the list can go stale, check the files before calling or verify the copied
files independently of `dragend`.
:::

| `code` | Meaning |
| --- | --- |
| `NOT_FOUND` | The calling window has no drag-drop registration |
| `ORIGIN_DENIED` | The document origin is not trusted to drag files out. Checked before any path is inspected |
| `NOT_SUPPORTED` | `dragOut` is `false` for this window |
| `INVALID_PARAMS` | `paths` missing, empty, or not all strings |
| `INVALID_PATH` | Specified item does not map to a local file (e.g. network stream or `cdda://` track); `paths[i]` reflects the index in the input array |
| `PERMISSION_DENIED` | Specified item rejected by path security policy; `paths[i]` reflects the index in the normalized, deduplicated list |
| `OPERATION_FAILED` | A token could not be created |

Error messages never contain a path.

### applyDragToken(dataTransfer, token)

Signature: `fb.dnd.applyDragToken(dataTransfer: DataTransfer, token: string): void`

Writes the token into a `dragstart` event so the host attaches the files. Must be
called synchronously inside the `dragstart` handler. It does two things, and both
are required:

1. Sets `text/plain` to the token payload (`fb2k-dnd-token/1:` followed by token), overriding existing text content so the host recognizes the gesture as a file drag-out
2. Sets `effectAllowed` to `'copy'`

::: danger `effectAllowed` must be exactly `'copy'`
After files are attached, the drop action is executed directly by the destination application. When destination and source reside on the same drive partition, Windows File Explorer defaults to moving files. To prevent source files from being deleted by external targets, the host strictly requires copy-only gestures: if `effectAllowed` permits wider operations than copy, the host refuses to attach files (emitting `dnd:dragEnded` with `INVALID_PARAMS`) and falls back to a standard web drag. Omitting `effectAllowed` implies `copy | move | link` and likewise prevents file attachment. Do not modify `effectAllowed` after calling `applyDragToken`.
:::

The `text/plain` slot is not available for page text during a drag-out. The host
blanks it before the drag leaves, so a text editor as the drop target receives an
empty string, never the token.

```javascript
let token = null;

trackEl.addEventListener('pointerdown', async () => {
    token = null;
    const r = await fb.dnd.prepareDrag([trackPath]);
    if (r.success) token = r.token;
});

trackEl.addEventListener('dragstart', (e) => {
    if (token) fb.dnd.applyDragToken(e.dataTransfer, token);
    // no token yet: an ordinary drag of whatever the page set
});

trackEl.addEventListener('dragend', (e) => {
    // 'copy' — a target took the files; 'none' — cancelled or refused
    console.log(e.dataTransfer.dropEffect);
});
```

### onDragEnded(handler)

Signature: `fb.dnd.onDragEnded(handler: (payload: DndDragEndedPayload) => void): () => void`

Subscribes to `dnd:dragEnded`, which fires **only when the host refuses** to
attach files to a drag-out, at the moment the drag starts. The drag is not
cancelled: it goes on as an ordinary page drag without files, so a drop inside the
page still works. Only in the rare case that the token text cannot be blanked is
the drag cancelled. Returns an unsubscribe function.

| Field | Type | Description |
| --- | --- | --- |
| `result` | `'failed'` | Always `'failed'`; the host reports refusals only |
| `code` | `string` | `PERMISSION_DENIED` (token unknown, expired, spent, superseded or from another window), `INVALID_PARAMS` (`effectAllowed` was not `'copy'`), `OPERATION_FAILED` (the file list could not be attached) |
| `error` | `string` | Human-readable reason. Never contains a path |

A valid token is consumed before checking `effectAllowed` or attaching the file list. After `INVALID_PARAMS` or `OPERATION_FAILED`, request a fresh token before retrying.

::: warning This is not a completion notice
No dedicated completion event is emitted when the host accepts the token. Drag interactions follow standard web drag lifecycles, with outcomes reported in the element native `dragend` handler: `dataTransfer.dropEffect` of `'copy'` indicates the target accepted files, while `'none'` indicates cancellation or refusal. Place cleanup logic inside `dragend`; `dnd:dragEnded` serves only to diagnose why files failed to attach during gesture initiation.
:::

```javascript
const off = fb.dnd.onDragEnded(({ code, error }) => {
    if (code === 'PERMISSION_DENIED') token = null; // mint a fresh one next time
    console.warn('drag-out refused:', code, error);
});
```

### What to expect during a drag-out

- Drag gestures with valid tokens attach physical files if accepted by the host; if attachment fails, they fall back to standard web drags without external files, while internal page drop handlers remain functional. Dropping into File Explorer produces neither files nor raw token strings. If token text cannot be sanitized, the gesture is cancelled.
- Drag gestures without tokens (text, images, hyperlinks) are unaffected and do not emit `dnd:dragEnded`.
- During drag interactions, the host window waits synchronously for destination responses, matching native WebView2 behavior. External application delays cannot be bypassed by the page.
- Inside an `<iframe>`, token exchanges use the standard `invoke` channel. Do not rely on framing as a security boundary; the host's origin check is the boundary.

## Not supported

### startDrag(type?)

Signature: `fb.dnd.startDrag(type?: string): Promise<DndStartDragResponse>`

This endpoint does not start a drag and ignores its argument. The host handler
**resolves** with `{ success: false, code: 'NOT_SUPPORTED' }` rather than rejecting.
Test `success`; transport failures can still reject. Use
[`prepareDrag`](#preparedrag-paths) with
[`applyDragToken`](#applydragtoken-datatransfer-token) instead.

```javascript
const r = await fb.dnd.startDrag('files');
if (r.success === false) console.log(r.code); // 'NOT_SUPPORTED'
```
