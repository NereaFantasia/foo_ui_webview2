# fb.dnd Drag and Drop

`fb.dnd` exposes the host's view of an external file drag so a page can obtain
real filesystem paths, which the HTML5 `File` object deliberately withholds.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->
<!-- END AUTO-GENERATED SDK STUBS -->

## How it works

Windows delivers a dropped file list to the native window, not to the page, and
`File.path` is always `null` in a browser engine. This namespace does not replace
HTML5 drag and drop: the standard `dragenter` / `dragover` / `drop` events keep
firing exactly as before, and `fb.dnd` runs alongside them as a side channel that
answers the one question HTML5 cannot — where the files actually live on disk.

The host tracks each drag as a *session* with an id, and publishes it to the page
as `window.__fbDndSession` before any `dnd:*` listener runs.

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

Returns `{ sessionId, paths }`. `paths` is empty when the session expired, carried
no file list, or the origin is not trusted with paths.

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
