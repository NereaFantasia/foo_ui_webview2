# Taskbar Tray API

English API reference for the `taskbar`, `tray` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## taskbar

### taskbar.flash

Flashes the taskbar button to draw attention.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | integer | No | Number of flashes. Default `3`. |
| `interval` | integer | No | Milliseconds between flashes. Default `0`. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('taskbar.flash', { count: 3 });
```

### taskbar.setOverlayIcon

Draws a small overlay badge on the taskbar button.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `description` | string | No | Accessibility text for the overlay. |
| `icon` | string | No | Raw Base64-encoded `.ico` file bytes, without `data:` or `base64:` prefixes. Empty, `null`, or omitted clears the overlay icon. |

**Returns**: `{"success":true}`

```js
// icoBase64: raw Base64-encoded .ico file bytes
await fb2k.invoke('taskbar.setOverlayIcon', { icon: icoBase64, description: 'Paused' });

// clear the overlay
await fb2k.invoke('taskbar.setOverlayIcon', { icon: '' });
```

### taskbar.setProgress

Sets the taskbar button's progress bar state and fill.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `state` | string | No | One of `none`, `indeterminate`, `normal`, `error`, `paused`. Defaults to `none`, which is also used for any other value. |
| `value` | number | No | Fill fraction. Applied only when it is a number between 0 and 1 inclusive. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('taskbar.setProgress', { state: 'normal', value: 0.42 });
```
## Runtime lifecycle, menu data, and events

The `taskbar.*` and `tray.*` families require a standalone main window. In a
panel, each handler returns `{ success: false, panelMode: true }`. Create the
icon with `tray.create` before relying on tray visibility, callbacks, or menu
operations. Windows accepts no more than seven thumbnail buttons; a thumbnail
install can also fail before the taskbar initializes its COM integration.

`taskbar.setProgress` accepts `none`, `indeterminate`, `normal`, `error`, or
`paused`. A numeric `value` is consumed only when it is within 0–1. Thumbnail
button activation broadcasts `taskbar:buttonClicked` with `{ id }`.

Tray menus are configured by `tray.setContextMenu` and can subsequently be
updated with `tray.appendMenuItems`, `tray.removeMenuItems`,
`tray.clearMenuItems`, and `tray.setMenuItemState`. `tray:menuItemClicked`
normally contains `{ id }`; rating, slider, and segmented controls also supply
`value`. Items executed natively by the plugin do **not** fire this event: the
built-in `showPlaybackControls` / `showSystemItems` injections and any item
declaring `playbackAction` run their command directly. The icon events are
`tray:click` with `{ button, x, y }`, `tray:doubleClick` with `{ x, y }`, and
`tray:beforeContextMenu` with `{ x, y }`. The last event is asynchronous:
changes made by a handler affect a later menu opening rather than the menu
already being constructed.

The menu may use `data:image/...` cover data and optional webview rendering.
For the webview renderer, the configured stylesheet can contain declarations
such as `display:flex`, `flex-direction:column`, and `background:rgba(...)`.
The tray click event for ordinary user items is `tray:menuItemClicked`; it does
not substitute the unrelated `menu:select` or `menu:dismiss` events. Items that
the plugin executes natively — the built-in injections and any item declaring
`playbackAction` — do not emit `tray:menuItemClicked`.

#### Reserved system items

`showSystemItems` (default `true`) injects two natively-executed items into the
bottom zone, in this order:

| Id | Label | Action |
| --- | --- | --- |
| `_sys_show` | Show Main Window | Restores and foregrounds the main window, preserving its maximized / normal placement. |
| `_sys_exit` | Exit foobar2000 | Quits the application, bypassing `setCloseToTray`. |

Both run natively and therefore do **not** fire `tray:menuItemClicked`. This is
load-bearing for `_sys_show`: hiding to the tray applies `put_IsVisible(FALSE)`
plus a deep suspend to the main page, so a `tray:menuItemClicked` handler cannot
run to call `window.focus` itself. A frontend-event route would be dead in
exactly the state the item exists for.

To render your own row instead of the injected one, use the exact,
case-sensitive id `_sys_show` (or `_sys_exit`). It receives the same native
route, your `label` / `icon` are preserved, and the matching injection is
skipped. Lookalike ids such as `_sys_show_alt` or `_SYS_SHOW` stay ordinary user
items and do not suppress the injection.

Top-level taskbar and tray icon fields are not generic image inputs. Non-empty
values must be raw Base64-encoded `.ico` file bytes, without a Data URL header
or `base64:` marker. PNG, JPEG, SVG, and Data URL payloads are not decoded by
the ICO loader. Invalid taskbar button icons may fall back to a default icon;
an invalid overlay may behave like a cleared icon; invalid tray icons fall back
to the foobar2000 main icon.

```js
fb2k.on('taskbar:buttonClicked', ({ id }) => console.log(id));
fb2k.on('tray:click', ({ button, x, y }) => console.log(button, x, y));
fb2k.on('tray:doubleClick', ({ x, y }) => console.log(x, y));
fb2k.on('tray:beforeContextMenu', ({ x, y }) => console.log(x, y));
fb2k.on('tray:menuItemClicked', ({ id, value }) => console.log(id, value));
fb2k.on('playback:stateChanged', () => {});
fb2k.on('playback:time', () => {});
fb2k.on('playback:trackChanged', () => {});
```

### taskbar.setThumbnailButtons

Installs the thumbnail toolbar shown on the taskbar preview.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `buttons` | array | Yes | Up to seven button objects. Each optional `icon` uses raw Base64-encoded `.ico` file bytes without a prefix. |

**Returns**: `{"error":"...","success":true}`

More than seven entries fails the whole call with `too many thumbnail buttons; Windows allows at most 7`; the list is never truncated. A missing or non-array `buttons` fails with `buttons array required`.

```js
await fb2k.invoke('taskbar.setThumbnailButtons', {
    buttons: [
        { id: 'prev', tooltip: 'Previous' },
        { id: 'pp', tooltip: 'Play / Pause' },
        { id: 'next', tooltip: 'Next' },
    ],
});
```

### taskbar.updateButton

Updates one already-installed thumbnail button.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | string | Yes | Id of the button to update, as passed to `taskbar.setThumbnailButtons`. |
| `enabled` | boolean | No | Whether the button accepts clicks. |
| `icon` | string | No | Raw Base64-encoded `.ico` file bytes without a prefix. Empty, `null`, or omitted supplies an empty icon value. |
| `tooltip` | string | No | Hover text. |
| `visible` | boolean | No | Whether the button is shown. |

**Returns**: `{"error":"...","success":true}`

An empty or missing `id` fails with `id required`.

```js
await fb2k.invoke('taskbar.updateButton', { id: 'pp', tooltip: 'Pause' });
```

## tray

### tray.appendMenuItems

Appends rows to an existing tray menu zone.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | array | Yes | Rows to append. A missing or non-array value fails with `items array required`. |
| `position` | string | No | Target zone: `top`, `playback`, or `bottom`. Defaults to `top`, which is also used for any other value. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('tray.appendMenuItems', {
    items: [
        { id: 'rescan', label: 'Rescan library' },
        { type: 'separator' },
    ],
    position: 'bottom',
});
```

### tray.clearMenuItems

Removes user rows from a tray menu zone.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `position` | string | No | Zone to clear: `top`, `playback`, or `bottom`. Omit to clear every zone. |

**Returns**: `{"success":true}`

```js
// clear one zone
await fb2k.invoke('tray.clearMenuItems', { position: 'top' });

// clear all zones
await fb2k.invoke('tray.clearMenuItems');
```

### tray.create

Creates the tray icon. Call this before relying on tray visibility, callbacks, or menu operations.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `icon` | string | No | Raw Base64-encoded `.ico` file bytes without `data:` or `base64:` prefixes; empty, invalid, or omitted falls back to the foobar2000 main icon. |
| `tooltip` | string | No | Hover text. Default `foobar2000`. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('tray.create', { tooltip: 'foobar2000' });
```

### tray.destroy

Removes the tray icon.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('tray.destroy');
```

### tray.getMenuItems

Returns the current tray menu rows, including any declared `playbackAction`.

_No parameters._

**Returns**: `{"items":"...","success":true}`

```js
const result = await fb2k.invoke('tray.getMenuItems');
```

### tray.isVisible

Reports whether the tray icon is currently shown.

_No parameters._

**Returns**: `{"success":true,"visible":"..."}`

```js
const result = await fb2k.invoke('tray.isVisible');
```

### tray.removeMenuItems

Removes specific rows by id.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `ids` | array | Yes | Ids to remove. A missing or non-array value fails with `ids array required`. |

**Returns**: `{"error":"...","removed":"...","success":true}`

`removed` reports how many rows were actually removed, which is not necessarily the number of ids passed.

```js
const { removed } = await fb2k.invoke('tray.removeMenuItems', { ids: ['rescan'] });
```

### tray.setCloseToTray

Makes closing the window hide it to the tray instead of quitting.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | boolean | No | Default `false`. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('tray.setCloseToTray', { enabled: true });
```

### tray.setContextMenu

Replaces the whole tray menu definition.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | array | Yes | Menu rows. A missing or non-array value fails with `items array required`. |
| `config` | object | No | Menu-wide options such as `showPlaybackControls`, `showSystemItems`, and `render`. |

`items[].icon` is a reserved compatibility field and is not rendered by either
the native or WebView menu backend. For WebView-rendered item icons, use
`items[].iconSvg`; the native backend is text-only.

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('tray.setContextMenu', {
    items: [
        { id: 'rescan', label: 'Rescan library' },
        { type: 'separator' },
        { id: 'settings', label: 'Settings', enabled: false },
    ],
    config: { showSystemItems: true },
});
```

### tray.setIcon

Replaces the tray icon image.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `icon` | string | No | Raw Base64-encoded `.ico` file bytes without `data:` or `base64:` prefixes; empty, invalid, or omitted falls back to the foobar2000 main icon. |

**Returns**: `{"success":true}`

```js
// icoBase64: raw Base64-encoded .ico file bytes
await fb2k.invoke('tray.setIcon', { icon: icoBase64 });
```

### tray.setMenuItemState

Toggles the checked or enabled state of one existing row.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | string | Yes | Id of the row to update. |
| `checked` | boolean | No | New checked state. |
| `enabled` | boolean | No | New enabled state. |

**Returns**: `{"error":"...","found":"...","success":true}`

An empty or missing `id` fails with `id required`, and omitting both `checked` and `enabled` fails with `at least one of checked/enabled required`. `found` reports whether a row with that id existed.

```js
await fb2k.invoke('tray.setMenuItemState', { id: 'settings', enabled: true });
```

### tray.setMinimizeToTray

Makes minimizing the window hide it to the tray.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | boolean | No | Default `false`. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('tray.setMinimizeToTray', { enabled: true });
```

### tray.setTooltip

Updates the tray icon's hover text.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `tooltip` | string | No | Hover text. Defaults to an empty string, which clears it. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('tray.setTooltip', { tooltip: 'Artist - Title' });
```

### tray.showBalloon

Shows a balloon notification from the tray icon.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | string | No | Notification title. Defaults to an empty string. |
| `message` | string | No | Notification body. Defaults to an empty string. |
| `icon` | string | No | One of `info`, `warning`, `error`. Defaults to `info`, which is also used for any other value. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('tray.showBalloon', { title: 'Now Playing', message: 'Artist - Title' });
```

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:taskbar.setProgress -->
### Contract supplement: `taskbar.setProgress`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `state` | `string` | No | `none` | One of `none`, `indeterminate`, `normal`, `error`, `paused`; any other value behaves as `none`. |
| `value` | `number` | No | — | Applied only when it is a number between 0 and 1 inclusive; otherwise the fill is left unchanged. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `success` | `boolean` | No |
| `panelMode` | `boolean` | No |

`success` reflects whether the taskbar accepted the change, so it can be `false` even for valid parameters when the COM integration is not yet initialized. In panel mode the call returns `{ success: false, panelMode: true }`.

```js
// indeterminate ignores value
await fb2k.invoke('taskbar.setProgress', { state: 'indeterminate' });
```

<!-- contract-supplement:tray.playbackAction -->
### Contract supplement: `items[].playbackAction`

`playbackAction` declares a native playback action executed by the plugin
instead of the page. It accepts one of `'play-pause' | 'previous' | 'next' |
'stop'` and is valid only on a `type: 'normal'` leaf.

| Aspect | Behavior |
| --- | --- |
| Execution | Translated at composition time to the matching built-in command and run natively by the plugin. |
| Background reliability | Works while the window is minimized, hidden to tray, or the session is locked — states where the page is deep-suspended. |
| Event | A declared item does **not** fire `tray:menuItemClicked`; reflect button state from `playback:*` events. |
| Appearance | The caller keeps full control of `label` / `icon` / `id`; only routing changes. |
| Validation | Fail-loud: an unknown token, or a declaration on a separator / submenu / rich control, rejects the whole `setContextMenu` / `appendMenuItems` call with `INVALID_PARAMS`. |
| `'exit'` / `'show-main-window'` | Not accepted — the system actions stay the reserved `_sys_exit` / `_sys_show` items. |
| Scope | Tray menus only; no effect on `menu.show`. |
| Round-trip | `getMenuItems()` echoes the declared `playbackAction`. |

Without this field, a user item that forwards `tray:menuItemClicked` to
`playback.*` depends on the main WebView's JavaScript and will not run while the
page is deep-suspended (minimize / tray / lock). Use `playbackAction` (or the
built-in `showPlaybackControls` items) for background-reliable tray playback
control. This mirrors the declarative native action pattern of Electron
`MenuItem.role` and Tauri `PredefinedMenuItem`.

```js
// Custom appearance + background-reliable native playback:
await fb2k.invoke('tray.setContextMenu', {
    items: [
        { id: 'prev', label: '⏮', playbackAction: 'previous' },
        { id: 'pp', label: '⏯', playbackAction: 'play-pause' },
        { id: 'next', label: '⏭', playbackAction: 'next' },
    ],
    config: { showPlaybackControls: false, render: 'webview' },
});
```
