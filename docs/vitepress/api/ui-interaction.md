# Ui Interaction API

English API reference for the `dnd`, `keyboard`, `ui` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## dnd

External file drop. Windows hands the dropped file list to the native window
rather than the page, and the HTML5 `File` object hides its filesystem path, so
this namespace acts as a side channel for the host's own view of a drag. It does
not replace HTML5 drag and drop: `dragenter` / `dragover` / `drop` keep firing.

Events: `dnd:enter`, `dnd:leave`, `dnd:drop`, `dnd:capabilitiesChanged`, and
`dnd:dragEnded` for the outbound direction (see `dnd.prepareDrag`). There is
deliberately **no** `dnd:over` — a single drag produces hundreds of `DragOver`
calls, so pages track the cursor with the HTML5 `dragover` event instead.

The reverse direction — dragging tracks *out of* the window as real files — goes
through `dnd.prepareDrag`.

### dnd.getCapabilities


_No parameters._

**Returns**: `{"html5":true,"hosting":"visual","paths":true,"dragOut":true,"pathsUnavailableReason":"...","dragOutUnavailableReason":"...","success":true}`

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sessionId` | `string` | No | Session to query, from a `dnd:*` payload. Omit to query the session that is active or most recently ended for this window. |

**Returns**: `{"paths":"...","resolvedPaths":"...","sessionId":"...","success":true}`

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
const { paths } = await fb2k.invoke('dnd.getPathsAsync');
```

### dnd.prepareDrag


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Locations to drag out, at least one. Accepts native paths, `file://` URLs, `file-relative://` URLs (a portable install's form for media on the program's volume, resolved by foobar2000), `archive://` / `unpack://` entries, and the `path\|subsong:N` form (the suffix is dropped and the container is dragged). A track's `path` and its `absolutePath` are both accepted. File existence is not checked. |

**Returns**: `{"success":true,"token":"..."}`

Exchanges a list of tracks for a one-shot **token** that lets the next drag out of
this window carry those files. The host checks the document origin first, then
normalises the paths to native paths and validates them the same way as
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
   when the destination is on the source's drive), so the host refuses any drag
   whose allowed effects are wider than copy. Leaving it unset means
   `copy | move | link`, which is refused.

**A refusal at drag start is reported on `dnd:dragEnded`** with
`{ result: 'failed', code, error }`: `PERMISSION_DENIED` (token unknown, expired,
spent, superseded or from another window), `INVALID_PARAMS` (`effectAllowed` was
not `'copy'`), `OPERATION_FAILED` (the file list could not be attached). It is
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
`ORIGIN_DENIED` (origin not trusted; decided before any path is looked at),
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


_No parameters._

**Returns**: always a `NOT_SUPPORTED` error envelope.

This endpoint does not start a drag. The host handler resolves with
`{ success: false, code: 'NOT_SUPPORTED' }`; test `success`, since this error
does not reject the promise. Transport failures can still reject. Use `dnd.prepareDrag`.

```js
const r = await fb2k.invoke('dnd.startDrag'); // resolves: { success: false, code: 'NOT_SUPPORTED' }
```

## keyboard

### keyboard.getRegisteredHotkeys


_No parameters._

**Returns**: `{"hotkeys":"...","success":true}`

```js
const result = await fb2k.invoke('keyboard.getRegisteredHotkeys');
```

### keyboard.registerHotkey


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `key` | `string` | Yes | — | Key combination such as `Ctrl+Alt+P`. |
| `action` | `string` | Yes | — | Action name echoed back in the `keyboard:hotkey` payload. |
| `global` | `boolean` | No | `true` |  |

**Returns**: `{"error":"...","id":"...","success":true}`

```js
const { id } = await fb2k.invoke('keyboard.registerHotkey', {
    key: 'Ctrl+Alt+P',
    action: 'togglePause',
});
```

### keyboard.registerShortcut


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `action` | `string` | Yes | Action name stored with the shortcut. |
| `key` | `string` | Yes | Key combination such as `Ctrl+Shift+L`. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('keyboard.registerShortcut', { key: 'Ctrl+Shift+L', action: 'focusSearch' });
```

### keyboard.unregisterHotkey


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `string` | No | Numeric id returned by `keyboard.registerHotkey`. |
| `key` | `string` | No | The original key string. |

**Returns**: `{"error":"...","success":true}`

```js
// pass the id returned by keyboard.registerHotkey, or the original key string
await fb2k.invoke('keyboard.unregisterHotkey', { id: 1 });
```

## ui

### ui.hideNotification


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('ui.hideNotification');
```

### ui.showContextMenu


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `x` | `integer` | No | `-1` | Omit or `-1` to use the current cursor position. |
| `y` | `integer` | No | `-1` | Omit or `-1` to use the current cursor position. |

**Returns**: `{"error":"...","success":true}`

```js
// omit x and y to open at the current cursor position
await fb2k.invoke('ui.showContextMenu');
```

### ui.showCustomMenu


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `items` | `array` | Yes | — | Menu item array. |
| `x` | `integer` | No | `0` | Currently ignored; the menu opens at the system cursor position. |
| `y` | `integer` | No | `0` | Currently ignored. |
| `w` | `integer` | No | `0` |  |
| `h` | `integer` | No | `0` |  |
| `suppressDefault` | `boolean` | No | `false` |  |

**Returns**: `{"error":"...","selectedId":"...","success":true}`

```js
const { selectedId } = await fb2k.invoke('ui.showCustomMenu', {
    items: [
        { id: 'play', label: 'Play' },
        { type: 'separator' },
        { id: 'remove', label: 'Remove', enabled: false },
    ],
});
```

### ui.showNotification


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `title` | `string` | No | — | Notification title. |
| `body` | `string` | No | — | Notification body. |
| `timeout` | `integer` | No | `5000` | Display time in milliseconds. |
| `silent` | `boolean` | No | `false` |  |

**Returns**: `{"error":"...","id":"...","success":true}`

```js
await fb2k.invoke('ui.showNotification', {
    title: 'Now Playing',
    body: 'Daft Punk - Digital Love',
});
```

### ui.showToast


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `message` | `string` | Yes | — | Toast text. |
| `type` | `string` | No | `info` | `info`, `success`, `warning`, or `error`. |
| `duration` | `integer` | No | `3000` | Display time in milliseconds. |
| `position` | `string` | No | `bottom-right` |  |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('ui.showToast', { message: 'Playlist saved', type: 'success' });
```

## Interaction delivery and limitations

`ui.showCustomMenu` uses the current cursor position for native placement and
routes `ui:menuItemClicked` only to the caller. A dismissed menu returns a
successful result with `selectedId: null`. `ui.showToast` does not paint UI in
native code; it emits `ui:toast` to the caller, so the theme owns rendering.

`keyboard.registerHotkey` registers a Windows hotkey and later routes
`keyboard:hotkey` to the window that registered it. `registerShortcut` only
stores an application-local shortcut. Both registration methods require a
non-empty `key` and `action`; `unregisterHotkey` accepts either the numeric
`id` or the original key string.

`dnd.getPathsAsync` and `dnd.getCapabilities` both resolve the calling window
from the message's own HWND, so a page cannot read another window's drag session.
Paths are withheld from untrusted origins while HTML5 drag events keep working, so
branch on `paths` and `html5` independently rather than hiding all drop
affordances at once. `dnd.startDrag` reports the current native limitation for
dragging content out of the window rather than implementing an OLE drag source.
