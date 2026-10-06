# Drag and Drop API

Methods of the `dnd` namespace.

## dnd

External file drop. Windows hands the dropped file list to the native window
rather than the page, and the HTML5 `File` object hides its filesystem path, so
this namespace acts as a side channel for the host's own view of a drag. It does
not replace HTML5 drag and drop: `dragenter` / `dragover` / `drop` keep firing.

Events: `dnd:enter`, `dnd:leave`, `dnd:drop`, `dnd:capabilitiesChanged`, and
`dnd:dragEnded` for the outbound direction (see `dnd.prepareDrag`). There is
deliberately **no** `dnd:over` — a single drag produces hundreds of `DragOver`
calls, so pages track the cursor with the HTML5 `dragover` event instead.

**A page accepts a drop the HTML5 way**, by calling `preventDefault()` in `dragover`
over the element that takes it. Where no handler does, the host refuses the drop on
the page's behalf: the cursor shows "forbidden", releasing there drops nothing and
sends no `dnd:drop`, and the browser does not open the dropped file by itself. Text
fields keep their default text drop. For the first half second after a drag enters
the window, before the page has answered, the cursor shows "copy", so it does not
flicker on the way in; a drop released in that time is delivered.

`dnd:enter`, `dnd:drop` and `dnd.getPathsAsync` report the drag's `source`:
`self` when it started in this window's page, `other-window` when it started in
another window of the same foobar2000, `external` otherwise. A page that already
handles its own drag through HTML5 events can use it to ignore the host's copy of
the same files. The host reads it from a marker it adds to drags that start in a
window it hosts; any program can imitate the marker, so it is a hint, not a
security check.

The reverse direction — dragging tracks *out of* the window as real files — goes
through `dnd.prepareDrag`.

### dnd.getCapabilities

<!-- api-schema:begin dnd.getCapabilities -->
Report what this window's drag-drop integration can currently deliver. Fails with `NOT_FOUND` when the calling window has no drag-drop registration.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `html5` | `boolean` | The page receives standard HTML5 drag events; `false` means the window has no drag-drop support at all. |
| `paths` | `boolean` | Real filesystem paths are obtainable through `dnd.getPathsAsync` and the `dnd:*` events. Not fixed for the window's lifetime: a navigation or Chromium re-registering its own drop target can withdraw it, announced by `dnd:capabilitiesChanged`. |
| `hosting` | `"visual" \| "standard"` | How the window hosts its WebView: `visual` for the main and popup windows, `standard` for a DUI or CUI panel. |
| `pathsUnavailableReason` | `"register-failed" \| "forward-unavailable" \| "inner-target-not-found" \| "chain-failed" \| "displaced" \| "origin-untrusted"` | Why `paths` is `false`; its presence always means `paths` is `false`. |
| `dragOut` | `boolean` | Files can be dragged out of the window through `dnd.prepareDrag`. Independent of `paths`. |
| `dragOutUnavailableReason` | `"not-visual-hosting" \| "runtime-too-old" \| "register-failed"` | Why `dragOut` is `false`; its presence always means `dragOut` is `false`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`html5`, `paths` and `dragOut` are independent: a panel-mode host can lose the
path side channel while HTML5 drag events keep working, because Chromium handles
those itself, and `dragOut` additionally depends on the WebView2 Runtime. `hosting`
is `"visual"` (main or popup window) or `"standard"` (DUI / CUI panel, where paths
are unavailable). `pathsUnavailableReason` appears only when `paths` is `false`,
and is one of `origin-untrusted`, `inner-target-not-found`, `forward-unavailable`,
`chain-failed`, `displaced`, `register-failed`.

`dragOut` is `true` when the page can drag files out through `dnd.prepareDrag`.
`dragOutUnavailableReason` appears only when it is `false`: `not-visual-hosting`
(panel mode, Chromium owns the drag source), `runtime-too-old` (the WebView2
Runtime predates the drag-start event; Edge 144 or newer is required), or
`register-failed`.

Capabilities are not constant for the window's lifetime; navigating to a different
origin withdraws path access and emits `dnd:capabilitiesChanged` with the same
fields.

```js
const caps = await fb2k.invoke('dnd.getCapabilities');
```

### dnd.getPathsAsync

<!-- api-schema:begin dnd.getPathsAsync -->
Read the real filesystem paths of a drag session from host memory, so the answer does not depend on the delivery order of `dnd:*` messages and stays valid after an `await`. Fails with `NOT_FOUND` when the calling window has no drag-drop registration.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | No | Session to query, from a `dnd:*` payload. Omit it, or pass an empty string, for the session that is active or most recently ended for this window. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `sessionId` | `string` | Session the paths belong to; an empty string when no session was found, so a caller can tell "nothing to report" from "a session with no files". |
| `paths` | `string[]` | Real paths in `DataTransfer.files` order; empty when the session expired, carried no file list, or the document origin is not trusted with paths. |
| `resolvedPaths` | `(string \| null)[]` | Target of the `.lnk` shortcut at the same index of `paths`, or `null` when the entry is not a shortcut, points at a shell object rather than a file, has a target too long to read back intact, or could not be resolved; never an empty string. A broken shortcut still reports the path it recorded. Always the same length as `paths`. |
| `source` | `"self" \| "other-window" \| "external"` | Where the session's drag came from, as in `dnd:enter`; absent when no session was found. Reported even when the paths are withheld, since it reveals no path. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The reliable way to obtain paths inside a HTML5 `drop` handler: it reads host
session state rather than a snapshot pushed to the page, so it does not depend on
message delivery order. Paths come back in the same order as
`DataTransfer.files`, so a page can pair them by index.

`paths` is an empty array when the session is unknown, expired, carried no file
list, or the origin is not trusted with paths. Sessions are stored per window, so
a session id alone cannot read another window's paths.

`resolvedPaths` carries shortcut (`.lnk`) targets and is **always the same length
as `paths`**, so an index valid for one is valid for the other; entries that are
not shortcuts are `null`. It is emptied together with `paths`, never separately.

```js
const res = await fb2k.invoke('dnd.getPathsAsync');
if (res.success === false) throw new Error(res.error);
const { paths } = res;
```

### dnd.prepareDrag

<!-- api-schema:begin dnd.prepareDrag -->
Exchange paths for a one-shot token that lets the next drag out of this window carry those files. Parameter shape errors are reported first; then the document origin is checked (`ORIGIN_DENIED`) before any path is resolved or validated, then the window's `dragOut` capability (`NOT_SUPPORTED`), then each path must resolve to a local file (`INVALID_PATH`) and pass path security (`PERMISSION_DENIED`).

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Locations to drag out: native paths, `file://` and `file-relative://` URLs, `archive://` / `unpack://` entries, and `path\|subsong:N`. File existence is not checked. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `token` | `string` | Opaque one-shot token of 32 lowercase hex characters, valid for 30 seconds, bound to the requesting window and superseded by the next successful call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Exchanges a list of tracks for a one-shot **token** that lets the next drag out of
this window carry those files. The host checks the parameter shape, then the
document origin, then normalises the paths to native paths and validates them the same way as
`queue.addPaths`; only then is a token minted. When the drag actually starts the
host swaps the token for that already-validated list — **no path travels through
the page at drag time**.

**Get the token before `dragstart`.** The drag data store is writable only while
the `dragstart` handler runs synchronously; awaiting this call inside it makes the
later `setData` fail silently and the drag goes out empty. Request it on
`pointerdown`, keep it, and use it synchronously in `dragstart`. A very fast
press-and-move can still outrun the request; without a token the drag is an
ordinary page drag.

The token is valid for 30 seconds, spent by the first drag that uses it, bound to
the requesting window, and superseded by the next successful call from the same window even
if it has not expired.

**Two things must be written in `dragstart`:**

1. `e.dataTransfer.setData('text/plain', 'fb2k-dnd-token/1:' + token)` — the host
   recognises a drag-out by this prefix. The `text/plain` slot is therefore taken;
   the host blanks it before the drag leaves, so text targets receive an empty
   string rather than the token.
2. `e.dataTransfer.effectAllowed = 'copy'` — **required**. Once files are
   attached, a *move* is carried out by the drop target (Explorer moves by default
   when the destination is on the source's drive), so the host attaches no files
   to a drag whose allowed effects are wider than copy. Leaving it unset means
   `copy | move | link`, which gets no files.

**A refusal at drag start is reported on `dnd:dragEnded`** with
`{ result: 'failed', code, error }`: `PERMISSION_DENIED` (token unknown, expired,
spent, superseded or from another window), `INVALID_PARAMS` (`effectAllowed` was
not `'copy'`), `OPERATION_FAILED` (the file list could not be attached). A refusal
does not cancel the drag: the host blanks the token text and lets the drag go on as
an ordinary page drag without files, so a drop inside the page, such as an album
onto a playlist, still works. Only if the token text cannot be blanked is the drag
cancelled. The event is
**not** a completion notice: when the host accepts the token no event follows, the
drag runs like any page drag with the browser's drag image, and the outcome is in
the element's own `dragend` (`dataTransfer.dropEffect` is `'copy'` when a target
took the files, `'none'` when cancelled or refused). While the drag is in progress
the host window waits for the drop target exactly as it does for a plain HTML5
drag. Drags without the token prefix are untouched and never produce
`dnd:dragEnded`.

What lands is always a physical file: a track inside a cue sheet or multi-track
container drags the whole container, an `archive://` / `unpack://` entry drags the
whole archive, and several requested entries can collapse into one file.

Error codes: `NOT_FOUND` (calling window has no drag-drop registration),
`ORIGIN_DENIED` (origin not trusted; reported after shape errors and before any path is looked at),
`NOT_SUPPORTED` (`dragOut` is `false`), `INVALID_PARAMS` (`paths` missing, empty
or not all strings), `INVALID_PATH` (an entry has no local file behind it;
`paths[i]` indexes the array you passed), `PERMISSION_DENIED` (an entry refused by
path security; `paths[i]` indexes the normalised, de-duplicated list),
`OPERATION_FAILED` (a token could not be created). Messages never contain a path.
Handler failures resolve with an error envelope; transport failures can still reject.
A valid token is consumed before checking the allowed effects or attaching files,
so a drag-start failure also requires a fresh token before retrying.

```js
let token = null;
el.addEventListener('pointerdown', async () => {
    token = null;
    const r = await fb2k.invoke('dnd.prepareDrag', { paths: [trackPath] });
    if (r.success) token = r.token;
});
el.addEventListener('dragstart', (e) => {
    if (!token) return;
    e.dataTransfer.setData('text/plain', 'fb2k-dnd-token/1:' + token);
    e.dataTransfer.effectAllowed = 'copy';
});
el.addEventListener('dragend', (e) => console.log(e.dataTransfer.dropEffect));
fb2k.on('dnd:dragEnded', (p) => console.warn('drag-out refused:', p.code, p.error));
```

### dnd.startDrag

<!-- api-schema:begin dnd.startDrag -->
Not implemented: always fails with `NOT_SUPPORTED`. Use `dnd.prepareDrag` instead.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

This endpoint does not start a drag. The host handler resolves with
`{ success: false, code: 'NOT_SUPPORTED' }`; test `success`, since this error
does not reject the promise. Transport failures can still reject. Use `dnd.prepareDrag`.

```js
const r = await fb2k.invoke('dnd.startDrag'); // resolves: { success: false, code: 'NOT_SUPPORTED' }
```

## Interaction delivery and limitations

`dnd.getPathsAsync` and `dnd.getCapabilities` both resolve the calling window
from the message's own HWND, so a page cannot read another window's drag session.
Paths are withheld from untrusted origins while HTML5 drag events keep working, so
branch on `paths` and `html5` independently rather than hiding all drop
affordances at once. `dnd.startDrag` reports the current native limitation for
dragging content out of the window rather than implementing an OLE drag source.
