# UI API

Methods of the `ui` namespace.

## ui

### ui.hideNotification

<!-- api-schema:begin ui.hideNotification -->
Hide the balloon shown by `showNotification`; nothing to do when none was shown.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('ui.hideNotification');
```

### ui.showContextMenu

<!-- api-schema:begin ui.showContextMenu -->
Open the main window's own context menu. Coordinates that are omitted, not positive, or more than 50 px away from the real cursor are replaced by the cursor position.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `integer` | No | Screen x in pixels. Default: `-1`. |
| `y` | `integer` | No | Screen y in pixels. Default: `-1`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// omit x and y to open at the current cursor position
await fb2k.invoke('ui.showContextMenu');
```

### ui.showCustomMenu

<!-- api-schema:begin ui.showCustomMenu -->
Show a native popup menu at the system cursor position and wait for the choice. The chosen row is reported as `selectedId` and also announced to the calling window as `ui:menuItemClicked` with `{ id, label }`; dismissing the menu reports `selectedId: null`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `UiMenuItem[]` | Yes | The rows. |
| `items[].id` | `string` | No | Identifier reported as `selectedId` and in `ui:menuItemClicked`. |
| `items[].label` | `string` | No | Display text. |
| `items[].type` | `string` | No | `separator` draws a separator; any other value is an ordinary row. Default: `"item"`. |
| `items[].enabled` | `boolean` | No | Whether the row can be picked. Default: `true`. |
| `items[].checked` | `boolean` | No | Whether the row shows a checkmark. Default: `false`. |
| `items[].shortcut` | `string` | No | Shortcut hint drawn right-aligned after the label. |
| `items[].submenu` | `UiMenuItem[]` | No | Child rows; a row with this key opens a submenu instead of being picked. |
| `x` | `integer` | No | Accepted for compatibility; the menu opens at the system cursor. Default: `0`. |
| `y` | `integer` | No | Accepted for compatibility; the menu opens at the system cursor. Default: `0`. |
| `w` | `integer` | No | With `h`, the size of a rectangle below the cursor the menu must not cover; the menu then opens below it. Default: `0`. |
| `h` | `integer` | No | See `w`. Default: `0`. |
| `suppressDefault` | `boolean` | No | Accepted for compatibility; has no effect. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `selectedId` | `string \| null` | Id of the chosen row; `null` when the menu was dismissed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('ui.showCustomMenu', {
    items: [
        { id: 'play', label: 'Play' },
        { type: 'separator' },
        { id: 'remove', label: 'Remove', enabled: false },
    ],
});
if (res.success === false) throw new Error(res.error);
const { selectedId } = res;
```

### ui.showNotification

<!-- api-schema:begin ui.showNotification -->
Show a Windows balloon notification from the plugin's notification icon, creating the icon on first use. At least one of `title` and `body` must be given.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Notification title. Default: `""`. |
| `body` | `string` | No | Notification body. Default: `""`. |
| `silent` | `boolean` | No | `true` shows it without the notification sound. Default: `false`. |
| `timeout` | `integer` | No | Display time in milliseconds, as a hint to the shell. Default: `5000`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `id` | `integer` | Sequence number of the notification, from `1`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('ui.showNotification', {
    title: 'Now Playing',
    body: 'Daft Punk - Digital Love',
});
```

### ui.showToast

<!-- api-schema:begin ui.showToast -->
Ask the calling window's page to show a toast: nothing is painted natively, the payload is delivered to that window as `ui:toast` with `{ message, duration, type, position }`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `string` | Yes | Toast text. Must not be empty. |
| `duration` | `integer` | No | Display time in milliseconds. Default: `3000`. |
| `type` | `"info" \| "success" \| "warning" \| "error"` | No | Toast kind, passed through to the page. Default: `"info"`. |
| `position` | `string` | No | Screen corner, passed through to the page. Default: `"bottom-right"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('ui.showToast', { message: 'Playlist saved', type: 'success' });
```

## Interaction delivery and limitations

`ui.showCustomMenu` uses the current cursor position for native placement and
routes `ui:menuItemClicked` only to the caller. A dismissed menu returns a
successful result with `selectedId: null`. `ui.showToast` does not paint UI in
native code; it emits `ui:toast` to the caller, so the theme owns rendering.
