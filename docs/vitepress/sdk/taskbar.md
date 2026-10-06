# fb.taskbar Windows Taskbar

`fb.taskbar` controls the main window's taskbar button: thumbnail-toolbar buttons, progress, the overlay icon and flashing. Called from a Default UI or Columns UI panel, every method fails with `PANEL_MODE_UNSUPPORTED`.

## flash(options?)

Signature: `fb.taskbar.flash(options?: TaskbarFlashParams): Promise<TaskbarFlashResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| options.count | number | No | Number of flashes. Defaults to 3 |
| options.interval | number | No | Milliseconds between flashes; omitted or `0` uses the system cursor blink rate |

Flashes the main window's taskbar button. Fails with `OPERATION_FAILED` when that button has not been created yet or the main window no longer exists.

```javascript
const result = await fb.taskbar.flash({ count: 3 });
```

## setOverlayIcon(options?)

Signature: `fb.taskbar.setOverlayIcon(options?: TaskbarSetOverlayIconParams): Promise<TaskbarSetOverlayIconResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| options.icon | string \| null | No | Raw Base64-encoded `.ico` file bytes; no `data:` or `base64:` prefix. Omit or pass `null` to clear the overlay |
| options.description | string | No | Accessible icon description |

Lays a small icon over the taskbar button, typically to show the playback state; calling it without arguments clears the overlay. Fails with `OPERATION_FAILED` when the taskbar button does not exist yet or the system rejects the call.

```javascript
// Raw Base64 .ico bytes the theme stored earlier; null clears the overlay.
const icon = localStorage.getItem('pausedOverlayIcon');
const result = await fb.taskbar.setOverlayIcon({ icon, description: 'Paused' });
```

## setThumbnailButtons(buttons)

Signature: `fb.taskbar.setThumbnailButtons(buttons: ThumbnailButton[]): Promise<TaskbarSetThumbnailButtonsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| buttons | ThumbnailButton[] | Yes | Up to seven buttons with `id`, `tooltip`, optional `icon`, and optional state flags |

Every button `icon` uses raw Base64-encoded `.ico` file bytes. PNG, JPEG, SVG,
Data URLs, and the `file.write`-specific `base64:` marker are not accepted icon
representations. Invalid values may fall back to a default icon.

Installs the thumbnail toolbar once for the window. Use `updateButton(options)` for later state changes; it cannot add or remove buttons. Button clicks emit `taskbar:buttonClicked` with `{ id }`. `setProgress({ state, value? })` accepts `none`, `indeterminate`, `normal`, `error`, or `paused`; `value` is a 0-1 fraction for determinate states.

```javascript
const result = await fb.taskbar.setThumbnailButtons([
    { id: 'play-pause', tooltip: 'Play or pause' }
]);
```

Update an existing thumbnail-toolbar button without reinstalling the toolbar:

```javascript
await fb.taskbar.updateButton({ id: 'play', tooltip: 'Pause', enabled: true });
```
