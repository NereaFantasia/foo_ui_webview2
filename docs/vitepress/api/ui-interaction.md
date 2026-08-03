# Ui Interaction API

English API reference for the `dnd`, `keyboard`, `ui` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## dnd

### dnd.getDropZones


_No parameters._

**Returns**: `{"count":"...","success":true,"zones":"..."}`

```js
const result = await fb2k.invoke('dnd.getDropZones');
```

### dnd.registerDropZone


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `accept` | `array` | No | Accepted payload kinds. Defaults to `["files"]` when omitted. |
| `event` | `string` | No | Optional; default dnd:drop. |
| `selector` | `string` | Yes | CSS selector of the element that becomes the drop zone. |

**Returns**: `{"accept":"...","error":"...","event":"...","script":"...","selector":"...","success":true,"zoneId":"..."}`

```js
const { zoneId, script } = await fb2k.invoke('dnd.registerDropZone', { selector: '#playlist' });
```

### dnd.startDrag


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `data` | `string` | No | Payload for a `text` drag. Required when `type` is `text`. |
| `paths` | `array` | Yes | Track or file paths. Required when `type` is `tracks` or `files`. |
| `type` | `string` | No | Optional; default text. |

**Returns**: `{"error":"...","note":"...","success":true,"trackCount":"...","type":"..."}`

```js
await fb2k.invoke('dnd.startDrag', { type: 'tracks', paths: ['C:\\Music\\song.flac'] });
```

### dnd.unregisterDropZone


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `zoneId` | `string` | Yes | Zone id returned by `dnd.registerDropZone`. |

**Returns**: `{"error":"...","script":"...","success":true,"zoneId":"..."}`

```js
await fb2k.invoke('dnd.unregisterDropZone', { zoneId: 'dropzone_1' });
```

## keyboard

### keyboard.getRegisteredHotkeys


_No parameters._

**Returns**: `{"hotkeys":"...","success":true}`

```js
const result = await fb2k.invoke('keyboard.getRegisteredHotkeys');
```

### keyboard.registerHotkey


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `action` | `string` | Yes | Action name echoed back in the `keyboard:hotkey` payload. |
| `global` | `boolean` | No | Optional; default true. |
| `key` | `string` | Yes | Key combination such as `Ctrl+Alt+P`. |

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
| `id` | `string` | No | Optional; omitted by default. |
| `key` | `string` | No | Optional. |

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `integer` | No | Optional; default -1. |
| `y` | `integer` | No | Optional; default -1. |

**Returns**: `{"error":"...","success":true}`

```js
// omit x and y to open at the current cursor position
await fb2k.invoke('ui.showContextMenu');
```

### ui.showCustomMenu


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `h` | `integer` | No | Optional; default 0. |
| `items` | `array` | Yes | Required. |
| `suppressDefault` | `boolean` | No | Optional; default false. |
| `w` | `integer` | No | Optional; default 0. |
| `x` | `integer` | No | Optional; default 0. |
| `y` | `integer` | No | Optional; default 0. |

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `body` | `string` | No | Optional. |
| `silent` | `boolean` | No | Optional; default false. |
| `timeout` | `integer` | No | Optional; default 5000. |
| `title` | `string` | No | Optional. |

**Returns**: `{"error":"...","id":"...","success":true}`

```js
await fb2k.invoke('ui.showNotification', {
    title: 'Now Playing',
    body: 'Daft Punk - Digital Love',
});
```

### ui.showToast


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `duration` | `integer` | No | Optional; default 3000. |
| `message` | `string` | Yes | Toast text. |
| `position` | `string` | No | Optional; default bottom-right. |
| `type` | `string` | No | `info` (default), `success`, `warning`, or `error`. |

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

`dnd.registerDropZone` returns a script that the page must apply to its own
DOM. Its default callback is `dnd:drop`; the callback payload contains the
zone ID plus file metadata, plain text, and HTML data. `dnd.startDrag` reports
the current native limitation for track and file drags rather than implementing
an OLE drag source.
