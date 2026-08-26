# fb.taskbar Windows Taskbar

`fb.taskbar` controls thumbnail-toolbar buttons, progress, overlay icons, and taskbar flashing for the current window.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### flash()

Signature: `fb.taskbar.flash(options?: { count?: number; interval?: number }): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| options.count | number | No | Number of flashes. Defaults to 3 |
| options.interval | number | No | Flash interval in milliseconds; omission uses the system default |

Returns the result of the underlying `taskbar.flash` call.

```javascript
const result = await fb.taskbar.flash({ count: 3 });
```

### setOverlayIcon()

Signature: `fb.taskbar.setOverlayIcon(options?: { icon?: string | null; description?: string }): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| options.icon | string \| null | No | Raw Base64-encoded `.ico` file bytes; no `data:` or `base64:` prefix. Omit or pass `null` to clear the overlay |
| options.description | string | No | Accessible icon description |

Returns the result of the underlying `taskbar.setOverlayIcon` call.

```javascript
const result = await fb.taskbar.setOverlayIcon({ icon, description: 'Paused' });
```

### setThumbnailButtons()

Signature: `fb.taskbar.setThumbnailButtons(buttons: ThumbnailButton[]): Promise<BaseResponse>`

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

<!-- END AUTO-GENERATED SDK STUBS -->
