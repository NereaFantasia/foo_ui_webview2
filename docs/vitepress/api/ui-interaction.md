# Ui Interaction API

English API reference for the `dnd`, `keyboard`, `ui` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## dnd

External file drop. Windows hands the dropped file list to the native window
rather than the page, and the HTML5 `File` object hides its filesystem path, so
this namespace acts as a side channel for the host's own view of a drag. It does
not replace HTML5 drag and drop: `dragenter` / `dragover` / `drop` keep firing.

Events: `dnd:enter`, `dnd:leave`, `dnd:drop`, `dnd:capabilitiesChanged`. There is
deliberately **no** `dnd:over` — a single drag produces hundreds of `DragOver`
calls, so pages track the cursor with the HTML5 `dragover` event instead.

### dnd.getCapabilities


_No parameters._

**Returns**: `{"html5":true,"hosting":"visual","paths":true,"pathsUnavailableReason":"...","success":true}`

`html5` and `paths` are independent: a panel-mode host can lose the path side
channel while HTML5 drag events keep working, because Chromium handles those
itself. `hosting` is `"visual"` (main or popup window) or `"standard"` (DUI / CUI
panel, where paths are unavailable). `pathsUnavailableReason` appears only when
`paths` is `false`, and is one of `origin-untrusted`, `inner-target-not-found`,
`forward-unavailable`, `chain-failed`, `displaced`, `register-failed`.

Capabilities are not constant for the window's lifetime; navigating to a different
origin withdraws path access and emits `dnd:capabilitiesChanged` with the same
four fields.

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

### dnd.startDrag


_No parameters._

**Returns**: always a `NOT_SUPPORTED` error envelope.

Dragging content *out of* the window needs an `IDropSource` implementation and a
host-produced data object; neither exists. It fails explicitly rather than
returning a fake `success: true`, so callers cannot build on a false success.

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
