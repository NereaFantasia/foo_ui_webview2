# Taskbar API

Methods of the `taskbar` namespace.

## taskbar

### taskbar.flash

<!-- api-schema:begin taskbar.flash -->
Flash the taskbar button to draw attention.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `integer` | No | Number of flashes. At least `0`. Default: `3`. |
| `interval` | `integer` | No | Milliseconds between flashes; `0` uses the system cursor blink rate. At least `0`. Default: `0`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('taskbar.flash', { count: 3 });
```

### taskbar.setOverlayIcon

<!-- api-schema:begin taskbar.setOverlayIcon -->
Draw a small overlay badge on the taskbar button, or clear it.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `icon` | `string \| null` | No | Overlay icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted clears the overlay. |
| `description` | `string` | No | Accessibility text for the overlay. Default: `""`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// icoBase64: raw Base64-encoded .ico file bytes
await fb2k.invoke('taskbar.setOverlayIcon', { icon: icoBase64, description: 'Paused' });

// clear the overlay
await fb2k.invoke('taskbar.setOverlayIcon', { icon: '' });
```

### taskbar.setProgress

<!-- api-schema:begin taskbar.setProgress -->
Set the progress bar drawn on the taskbar button. Once a theme sets it, the plugin stops mirroring playback progress there until the next playback state change.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `state` | `"none" \| "indeterminate" \| "normal" \| "error" \| "paused"` | No | Progress bar state. `none` removes the bar. Default: `"none"`. |
| `value` | `number` | No | Fill fraction, meaningful for the `normal`, `error` and `paused` states. Between `0` and `1` inclusive. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('taskbar.setProgress', { state: 'normal', value: 0.42 });
```

### taskbar.setThumbnailButtons

<!-- api-schema:begin taskbar.setThumbnailButtons -->
Install the thumbnail toolbar shown on the taskbar preview. Windows lets a window install it once; change the buttons afterwards with `taskbar.updateButton`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `buttons` | `ThumbnailButton[]` | Yes | Buttons in display order, at most seven. A longer list fails the whole call; it is never truncated. |
| `buttons[].id` | `string` | Yes | Identifier reported by `taskbar:buttonClicked` and used by `taskbar.updateButton`. Must not be empty. |
| `buttons[].icon` | `string \| null` | No | Button icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted uses the foobar2000 main icon. |
| `buttons[].tooltip` | `string` | No | Hover text. Default: `""`. |
| `buttons[].enabled` | `boolean` | No | Whether the button accepts clicks. Default: `true`. |
| `buttons[].visible` | `boolean` | No | Whether the button is shown. Default: `true`. |
| `buttons[].dismissOnClick` | `boolean` | No | Close the thumbnail preview after a click. Default: `false`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

More than seven entries fails the whole call with `too many thumbnail buttons; Windows allows at most 7`; the list is never truncated. A missing `buttons`, a button without `id`, or an undeclared key is rejected with `INVALID_PARAMS`.

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

<!-- api-schema:begin taskbar.updateButton -->
Update one installed thumbnail button in place. Buttons cannot be added or removed.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `string` | Yes | Id of the button, as passed to `taskbar.setThumbnailButtons`. Must not be empty. |
| `icon` | `string \| null` | No | New icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted keeps the current icon. |
| `tooltip` | `string` | No | New hover text. Empty or omitted keeps the current text. Default: `""`. |
| `enabled` | `boolean` | No | Whether the button accepts clicks. Omitted keeps the current state. |
| `visible` | `boolean` | No | Whether the button is shown. Omitted keeps the current state. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

An empty or missing `id` is rejected with `INVALID_PARAMS`.

```js
await fb2k.invoke('taskbar.updateButton', { id: 'pp', tooltip: 'Pause' });
```

## Runtime lifecycle, menu data, and events

`taskbar.setProgress` accepts `none`, `indeterminate`, `normal`, `error`, or
`paused`; any other `state`, or a `value` outside 0–1, is rejected with
`INVALID_PARAMS`. Thumbnail button activation broadcasts `taskbar:buttonClicked`
with `{ id }`.

The parts shared with `tray` (the standalone-window requirement, the thumbnail button limit, the `.ico` format of top-level `icon` fields, and an event-listener example) are in [Runtime lifecycle, menu data, and events](./tray.md#runtime-lifecycle-menu-data-and-events) on the Tray page.

## Contract supplements

<!-- phase3-supplement:taskbar.setProgress -->
### Contract supplement: `taskbar.setProgress`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `state` | `string` | No | `none` | One of `none`, `indeterminate`, `normal`, `error`, `paused`; any other value is rejected with `INVALID_PARAMS`. |
| `value` | `number` | No | — | Fill fraction between 0 and 1 inclusive; other values are rejected with `INVALID_PARAMS`. |

#### Return value

Only the envelope: `{ success: true }` when the taskbar accepted the change. Even with valid parameters the call fails with `code: 'OPERATION_FAILED'` while the taskbar has not initialized its COM integration, and with `code: 'PANEL_MODE_UNSUPPORTED'` when called from a panel.

```js
// indeterminate ignores value
await fb2k.invoke('taskbar.setProgress', { state: 'indeterminate' });
```
