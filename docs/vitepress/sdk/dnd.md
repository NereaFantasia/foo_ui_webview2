# fb.dnd Drag and Drop

`fb.dnd` exposes the host's view of an external file drag so a page can obtain
real filesystem paths, which the HTML5 `File` object deliberately withholds.

## How it works

Windows delivers a dropped file list to the native window, not to the page, and
`File.path` is always `null` in a browser engine. This namespace does not replace
HTML5 drag and drop: the standard `dragenter` / `dragover` / `drop` events keep
firing exactly as before, and `fb.dnd` runs alongside them as a side channel that
answers the one question HTML5 cannot — where the files actually live on disk.

The host tracks each drag as a *session* with an id, and publishes it to the
top-level document as `window.__fbDndSession` before any `dnd:*` listener runs.

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
    const { paths } = await fb.dnd.getPathsAsync();
    if (paths.length) {
        await fb.playlist.addPaths(paths);
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

`dnd:enter` carries `sessionId`, `paths`, `resolvedPaths`, `hasFiles` and the same
cursor fields; `dnd:leave` carries only `sessionId`.

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
await fb.playlist.addPaths(playable);
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

Signature: `fb.dnd.getCapabilities(): Promise<DndCapabilities>`

What this window's integration can currently deliver. Not constant for the
window's lifetime: navigating to a different origin can withdraw path access while
leaving HTML5 drag events intact.

| Field | Type | Description |
| --- | --- | --- |
| `html5` | `boolean` | Page still receives standard HTML5 drag events. |
| `paths` | `boolean` | Real filesystem paths are obtainable. |
| `hosting` | `'visual' \| 'standard'` | How the window hosts its WebView. |
| `pathsUnavailableReason` | `string` | Present only when `paths` is `false`. |

```javascript
const caps = await fb.dnd.getCapabilities();
if (!caps.paths) {
    console.warn('paths unavailable:', caps.pathsUnavailableReason);
}
```

`pathsUnavailableReason` is one of `origin-untrusted`, `inner-target-not-found`,
`forward-unavailable`, `chain-failed`, `displaced`, or `register-failed`.

Subscribe to `dnd:capabilitiesChanged` to react to a change. Its payload carries
the same `html5`, `paths`, `hosting` and `pathsUnavailableReason` fields.

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

## Not supported

### startDrag(type?)

Signature: `fb.dnd.startDrag(type?: string): Promise<DndStartDragResponse>`

Dragging tracks *out of* the window into other applications. Not implemented: it
requires a native `IDropSource`, which this component does not provide. The
promise **resolves** with `{ success: false, code: 'NOT_SUPPORTED' }` rather than
rejecting, because the host delivers handler-returned error envelopes as a normal
result — test `success` instead of relying on `catch`. The argument is accepted
and ignored so old call sites still compile.

```javascript
const r = await fb.dnd.startDrag('files');
console.log(r.success, r.code); // false 'NOT_SUPPORTED'
```
