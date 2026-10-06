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
page. The cursor shows "forbidden", releasing there drops nothing and sends no
`dnd:drop`, and the browser does not open the dropped file by itself. Text fields
keep their default text drop.

For the first half second after a drag enters the window the page has not answered
yet, so the cursor shows "copy" rather than flickering to "forbidden" on the way in;
a drop released in that time is still delivered. After that the cursor follows the
page's answer as the pointer moves.

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
| `sessionId` | `string` | No | Session to query, from a `dnd:*` payload. Omit to query the session that is active or most recently ended for this window. |

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
| `sessionId` | `string` | Correlates `dnd:enter`, `dnd:leave` and `dnd:drop` for one drag gesture. |
| `paths` | `string[]` | Absolute filesystem paths, in `DataTransfer.files` order; empty when withheld. |
| `resolvedPaths` | `(string \| null)[]` | Shortcut target per index, or `null`. Always the same length as `paths`. See [Shortcut targets](#shortcut-targets). |
| `x`, `y` | `number` | Cursor position in client-area physical pixels — divide by `devicePixelRatio` for CSS pixels. |
| `keyState` | `number` | Win32 `MK_*` modifier / mouse-button mask at drop time. |
| `source` | `'self' \| 'other-window' \| 'external'` | Where the drag came from. See [Where a drag came from](#where-a-drag-came-from). |

`dnd:enter` carries `sessionId`, `paths`, `resolvedPaths`, `hasFiles`, `source` and
the same cursor fields; `dnd:leave` carries only `sessionId`.

### Where a drag came from

`source` on `dnd:enter`, `dnd:drop` and `getPathsAsync` tells a drag the page
started itself apart from files dragged in from outside, so a page that handles
its own drag through HTML5 events can ignore the host's copy of the same files:

| Value | The drag started in |
| --- | --- |
| `'self'` | this window's page, with or without a drag token |
| `'other-window'` | another window of the same foobar2000: the main window or a popup |
| `'external'` | anywhere else: Explorer, another application, another foobar2000 process, or a Default UI / Columns UI panel |

The host marks every drag that starts in a window it hosts and reads the mark back
when the drag enters a window. Any program can imitate the mark, so use `source` to
decide how to handle a drop, not as a security check.

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
drag can reach the page's `drop` handler before publication completes. Keep this
for optimistic UI and use `getPathsAsync` as the real source.

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
foobar2000 cannot play that. So every path source above is joined by a parallel
array of shortcut targets: `resolvedPaths` on the `dnd:enter` / `dnd:drop`
payloads and on `getPathsAsync`, and `getResolvedPaths()` for the snapshot.

`paths` is never rewritten. The index correspondence with `DataTransfer.files` is
a fixed contract, so the target appears beside the original entry rather than in
place of it, and a page decides for itself which one to use.

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

::: warning A target is where the shortcut points, not proof the file is there
A **broken** shortcut does *not* report `null`. Windows returns the target path
the `.lnk` recorded whether or not anything still exists at it, so a non-null
entry means "this is where the shortcut points" and nothing more.

The host deliberately does not check: it reads shortcuts on the thread the
dragging application is blocked on, so a filesystem probe per entry is exactly
what must not happen there. Handle the missing-file case in the page — the
cheapest form is to let the call you pass the path to report its own failure
(`playlist.addPaths` comes back with `addedCount` and `invalidCount`, so a
vanished target shows up as a shortfall rather than as a thrown error).
:::

```javascript
const paths = fb.dnd.getPaths();
const targets = fb.dnd.getResolvedPaths();

// A shortcut plays its target; anything else plays itself.
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
| `html5` | `boolean` | Page still receives standard HTML5 drag events. |
| `paths` | `boolean` | Real filesystem paths are obtainable. |
| `hosting` | `'visual' \| 'standard'` | How the window hosts its WebView. |
| `pathsUnavailableReason` | `string` | Present only when `paths` is `false`. |
| `dragOut` | `boolean` | Files can be dragged out through [`prepareDrag`](#preparedrag-paths). |
| `dragOutUnavailableReason` | `string` | Present only when `dragOut` is `false`. |

```javascript
const caps = await fb.dnd.getCapabilities();
if (caps.success === false) throw new Error(caps.error);
if (!caps.paths) {
    console.warn('paths unavailable:', caps.pathsUnavailableReason);
}
```

`pathsUnavailableReason` is one of `origin-untrusted`, `inner-target-not-found`,
`forward-unavailable`, `chain-failed`, `displaced`, or `register-failed`.

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

The reverse direction: a page element the user can drag into Explorer or another
application, where it arrives as real files. The page never handles paths at drag
time. It exchanges them for a one-shot **token** beforehand, writes the token into
the drag when it starts, and the host swaps the token for the already-validated
file list. From there the drag is an ordinary page drag: the browser draws the
drag image, the cursor follows the target, and the page's own `dragend` reports
the outcome.

Check `dragOut` on [`getCapabilities()`](#getcapabilities) before offering the
gesture.

### prepareDrag(paths)

Signature: `fb.dnd.prepareDrag(paths: string[]): Promise<DndPrepareDragResponse>`

Validates the paths and returns a token for the next drag out of this window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Locations to drag out, at least one. Accepts native paths, `file://` URLs, `file-relative://` URLs (a portable install's form for media on the program's volume, resolved by foobar2000), `archive://` / `unpack://` entries, and the `path\|subsong:N` form (the suffix is dropped and the container is dragged). A track's `path` and its `absolutePath` are both accepted. |

Returns `{ success: true, token }`. The token is opaque; the only thing to do with
it is pass it to [`applyDragToken`](#applydragtoken-datatransfer-token).

::: warning Call this before the drag starts, not inside `dragstart`
The drag data store is writable only while the `dragstart` handler runs
synchronously. A token that arrives after an `await` inside that handler cannot be
written any more — `setData` fails silently and the drag goes out empty. Request the
token on `pointerdown` (or `mousedown`), keep it, and use it synchronously in
`dragstart`. A very fast press-and-move can still outrun the request; if the token
has not arrived when `dragstart` fires, let the drag proceed as an ordinary one.
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
A track inside a cue sheet or a multi-track container drags the whole container.
An `archive://` or `unpack://` entry drags the whole archive. Several requested
entries can therefore collapse into one file, and the position inside the
container is not carried across.
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
| `NOT_FOUND` | The calling window has no drag-drop registration. |
| `ORIGIN_DENIED` | The document origin is not trusted to drag files out. Checked before any path is inspected. |
| `NOT_SUPPORTED` | `dragOut` is `false` for this window. |
| `INVALID_PARAMS` | `paths` missing, empty, or not all strings. |
| `INVALID_PATH` | An entry has no local file behind it (a stream, a `cdda://` track). `paths[i]` indexes **the array you passed**. |
| `PERMISSION_DENIED` | An entry was refused by path security. `paths[i]` indexes the **normalised, de-duplicated** list, which can be shorter than yours. |
| `OPERATION_FAILED` | A token could not be created. |

Error messages never contain a path.

### applyDragToken(dataTransfer, token)

Signature: `fb.dnd.applyDragToken(dataTransfer: DataTransfer, token: string): void`

Writes the token into a `dragstart` event so the host attaches the files. Must be
called synchronously inside the `dragstart` handler. It does two things, and both
are required:

1. sets the `text/plain` entry to the token carrier (`fb2k-dnd-token/1:` followed
   by the token), replacing anything the page put there — the host recognises a
   drag-out by that marker
2. sets `effectAllowed` to `'copy'`

::: danger `effectAllowed` must be exactly `'copy'`
Once the file list is attached, a *move* is carried out by the drop target, not by
the page or the host: Explorer moves files by default when the destination is on
the same drive as the source. The host therefore attaches no files to a drag whose
allowed effects are wider than copy (`INVALID_PARAMS` on `dnd:dragEnded`), and only
the page's own drag is left. Leaving `effectAllowed` unset means
`copy | move | link`, which gets no files. Do not change it after `applyDragToken`.
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
| `result` | `'failed'` | Always `'failed'`; the host reports refusals only. |
| `code` | `string` | `PERMISSION_DENIED` (token unknown, expired, spent, superseded or from another window), `INVALID_PARAMS` (`effectAllowed` was not `'copy'`), `OPERATION_FAILED` (the file list could not be attached). |
| `error` | `string` | Human-readable reason. Never contains a path. |

A valid token is consumed before checking `effectAllowed` or attaching the file list. After `INVALID_PARAMS` or `OPERATION_FAILED`, request a fresh token before retrying.

::: warning This is not a completion notice
When the host accepts the token there is **no event**. The drag proceeds like any
page drag and the outcome is in the element's own `dragend`:
`dataTransfer.dropEffect` is `'copy'` when a target took the files and `'none'`
when the drag was cancelled or the target refused. Put your clean-up in `dragend`,
and use `dnd:dragEnded` to tell the user (or yourself) why a drag carried no files.
:::

```javascript
const off = fb.dnd.onDragEnded(({ code, error }) => {
    if (code === 'PERMISSION_DENIED') token = null; // mint a fresh one next time
    console.warn('drag-out refused:', code, error);
});
```

### What to expect during a drag-out

- A drag that carries a token is either handed over with the files attached, or
  falls back to an ordinary page drag without files: a drop inside the page still
  works, and a drop on Explorer produces no file and no token text. If the token
  text cannot be blanked, the drag is cancelled instead. It never drags the token
  text out.
- Drags that do **not** carry a token — the page dragging its own text, an image
  or a link — are untouched and never produce `dnd:dragEnded`.
- While the drag is in progress the host window waits for the drop target to
  respond, exactly as it does for any HTML5 drag out of a WebView. A target that
  is slow to accept holds the window for that long; this is how WebView2 drags
  work and not something the page can shorten.
- Inside an `<iframe>` the token exchange goes through the same `invoke` channel
  as everything else. Do not rely on framing as a security boundary; the host's
  origin check is the boundary.

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
