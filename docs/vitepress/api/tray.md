# Tray API

Methods of the `tray` namespace.

## tray

### tray.appendMenuItems

<!-- api-schema:begin tray.appendMenuItems -->
Append rows to one menu zone; the same validation as `setContextMenu`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `TrayMenuItem[]` | Yes | Rows to append. |
| `items[].id` | `string` | No | Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the caller's label and skip the matching built-in injection. |
| `items[].label` | `string` | No | Display text. |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | No | Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is otherwise driven by `checked`. Default: `"normal"`. |
| `items[].enabled` | `boolean` | No | Whether the row can be clicked. Default: `true`. |
| `items[].visible` | `boolean` | No | Whether the row is shown. Default: `true`. |
| `items[].checked` | `boolean` | No | Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview` backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the row is not checkable. |
| `items[].icon` | `string` | No | Reserved; neither backend draws it. Use `iconSvg` for a row icon. |
| `items[].iconSvg` | `TrayIconSvg` | No | Icon drawn before the label by the `webview` backend. When any row of a menu level has a renderable icon, every `normal` and `submenu` row of that level reserves the icon column. |
| `items[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `items[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `items[].cover` | `string` | No | `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With `config.autoNowPlaying` an empty value is filled from the playing track's front art (`webview` only). |
| `items[].title` | `string` | No | `nowplaying` first line, usually the track title; falls back to `label`. |
| `items[].subtitle` | `string` | No | `nowplaying` second line, usually artist or album. |
| `items[].value` | `integer` | No | Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider` (clamped), the selected index for `segmented`. Reported back only for those kinds. |
| `items[].min` | `integer` | No | `slider` range minimum; swapped with `max` when larger. |
| `items[].max` | `integer` | No | `slider` range maximum. |
| `items[].orientation` | `"horizontal" \| "vertical"` | No | `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom. |
| `items[].segments` | `TraySegment[]` | No | `segmented` options, one row of mutually exclusive choices; `value` is the selected index. Picking one reports `{ id, value }` and keeps the menu open. |
| `items[].segments[].label` | `string` | No | Text of the segment, shown when it has no icon. |
| `items[].segments[].iconSvg` | `TrayIconSvg` | No | Icon of the segment, preferred over the label. |
| `items[].segments[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `items[].segments[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `items[].segments[].enabled` | `boolean` | No | `false` greys the segment out so it cannot be picked. Default: `true`. |
| `items[].submenu` | `TrayMenuItem[]` | No | Child rows of a `submenu` row. |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | No | A playback command the plugin runs natively when the row is picked, so it keeps working while the page is suspended (minimized, hidden to the tray, session locked). Such a row does not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails the whole call. |
| `position` | `"top" \| "playback" \| "bottom"` | No | Zone to append to. Default: `"top"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

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

<!-- api-schema:begin tray.clearMenuItems -->
Clear one zone, or every zone when `position` is omitted.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `position` | `"top" \| "playback" \| "bottom"` | No | Zone to clear; every zone when omitted. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// clear one zone
await fb2k.invoke('tray.clearMenuItems', { position: 'top' });

// clear all zones
await fb2k.invoke('tray.clearMenuItems');
```

### tray.create

<!-- api-schema:begin tray.create -->
Create the tray icon. Call it once before any other `tray.*` method: without it the other calls succeed but no icon shows, no tray event fires and `isVisible` reports `false`. Fails in panel mode, when the main window has no handle, and when the shell refuses the icon.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `icon` | `string` | No | Icon to show. |
| `tooltip` | `string` | No | Hover text. Default: `"foobar2000"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('tray.create', { tooltip: 'foobar2000' });
```

### tray.destroy

<!-- api-schema:begin tray.destroy -->
Remove the tray icon.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('tray.destroy');
```

### tray.getMenuItems

<!-- api-schema:begin tray.getMenuItems -->
List the user rows of every zone, flattened in `top`, `playback`, `bottom` order, as they were stored. The rows the runtime injects for `showPlaybackControls` and `showSystemItems` are not included.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `items` | `TrayMenuItem[]` | The stored user rows. |
| `items[].id` | `string` | Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the caller's label and skip the matching built-in injection. |
| `items[].label` | `string` | Display text. |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is otherwise driven by `checked`. |
| `items[].enabled` | `boolean` | Whether the row can be clicked. |
| `items[].visible` | `boolean` | Whether the row is shown. |
| `items[].checked` | `boolean` | Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview` backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the row is not checkable. |
| `items[].icon` | `string` | Reserved; neither backend draws it. Use `iconSvg` for a row icon. |
| `items[].iconSvg` | `TrayIconSvg` | Icon drawn before the label by the `webview` backend. When any row of a menu level has a renderable icon, every `normal` and `submenu` row of that level reserves the icon column. |
| `items[].iconSvg.viewBox` | `string` | The SVG `viewBox` attribute. |
| `items[].iconSvg.content` | `string` | The SVG inner markup. |
| `items[].cover` | `string` | `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With `config.autoNowPlaying` an empty value is filled from the playing track's front art (`webview` only). |
| `items[].title` | `string` | `nowplaying` first line, usually the track title; falls back to `label`. |
| `items[].subtitle` | `string` | `nowplaying` second line, usually artist or album. |
| `items[].value` | `integer` | Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider` (clamped), the selected index for `segmented`. Reported back only for those kinds. |
| `items[].min` | `integer` | `slider` range minimum; swapped with `max` when larger. |
| `items[].max` | `integer` | `slider` range maximum. |
| `items[].orientation` | `"horizontal" \| "vertical"` | `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom. |
| `items[].segments` | `TraySegment[]` | `segmented` options, one row of mutually exclusive choices; `value` is the selected index. Picking one reports `{ id, value }` and keeps the menu open. |
| `items[].segments[].label` | `string` | Text of the segment, shown when it has no icon. |
| `items[].segments[].iconSvg` | `TrayIconSvg` | Icon of the segment, preferred over the label. |
| `items[].segments[].iconSvg.viewBox` | `string` | The SVG `viewBox` attribute. |
| `items[].segments[].iconSvg.content` | `string` | The SVG inner markup. |
| `items[].segments[].enabled` | `boolean` | `false` greys the segment out so it cannot be picked. |
| `items[].submenu` | `TrayMenuItem[]` | Child rows of a `submenu` row. |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | A playback command the plugin runs natively when the row is picked, so it keeps working while the page is suspended (minimized, hidden to the tray, session locked). Such a row does not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails the whole call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('tray.getMenuItems');
```

### tray.isVisible

<!-- api-schema:begin tray.isVisible -->
Report whether the tray icon exists.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `visible` | `boolean` | Whether the icon exists. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('tray.isVisible');
```

### tray.removeMenuItems

<!-- api-schema:begin tray.removeMenuItems -->
Remove rows by id from every zone, submenus included.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `ids` | `string[]` | Yes | Ids of the rows to remove. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `removed` | `integer` | Number of rows removed; less than the number of ids when some did not exist. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`removed` reports how many rows were actually removed, which is not necessarily the number of ids passed.

```js
const res = await fb2k.invoke('tray.removeMenuItems', { ids: ['rescan'] });
if (res.success === false) throw new Error(res.error);
const { removed } = res;
```

### tray.setCloseToTray

<!-- api-schema:begin tray.setCloseToTray -->
Hide the window to the tray instead of quitting when it is closed.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | Yes | `true` hides to the tray. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('tray.setCloseToTray', { enabled: true });
```

### tray.setContextMenu

<!-- api-schema:begin tray.setContextMenu -->
Replace the user rows of one menu zone (`config.customPosition`, default `top`); the other zones are left as they are. Nothing is stored when any row is rejected or the resource limits are exceeded. An ordinary row reports its click through `tray:menuItemClicked`; the built-in playback and system rows, and rows declaring `playbackAction`, run natively and do not. To replace every zone at once, use `setMenuZones`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `TrayMenuItem[]` | Yes | The rows of the zone. |
| `items[].id` | `string` | No | Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the caller's label and skip the matching built-in injection. |
| `items[].label` | `string` | No | Display text. |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | No | Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is otherwise driven by `checked`. Default: `"normal"`. |
| `items[].enabled` | `boolean` | No | Whether the row can be clicked. Default: `true`. |
| `items[].visible` | `boolean` | No | Whether the row is shown. Default: `true`. |
| `items[].checked` | `boolean` | No | Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview` backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the row is not checkable. |
| `items[].icon` | `string` | No | Reserved; neither backend draws it. Use `iconSvg` for a row icon. |
| `items[].iconSvg` | `TrayIconSvg` | No | Icon drawn before the label by the `webview` backend. When any row of a menu level has a renderable icon, every `normal` and `submenu` row of that level reserves the icon column. |
| `items[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `items[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `items[].cover` | `string` | No | `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With `config.autoNowPlaying` an empty value is filled from the playing track's front art (`webview` only). |
| `items[].title` | `string` | No | `nowplaying` first line, usually the track title; falls back to `label`. |
| `items[].subtitle` | `string` | No | `nowplaying` second line, usually artist or album. |
| `items[].value` | `integer` | No | Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider` (clamped), the selected index for `segmented`. Reported back only for those kinds. |
| `items[].min` | `integer` | No | `slider` range minimum; swapped with `max` when larger. |
| `items[].max` | `integer` | No | `slider` range maximum. |
| `items[].orientation` | `"horizontal" \| "vertical"` | No | `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom. |
| `items[].segments` | `TraySegment[]` | No | `segmented` options, one row of mutually exclusive choices; `value` is the selected index. Picking one reports `{ id, value }` and keeps the menu open. |
| `items[].segments[].label` | `string` | No | Text of the segment, shown when it has no icon. |
| `items[].segments[].iconSvg` | `TrayIconSvg` | No | Icon of the segment, preferred over the label. |
| `items[].segments[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `items[].segments[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `items[].segments[].enabled` | `boolean` | No | `false` greys the segment out so it cannot be picked. Default: `true`. |
| `items[].submenu` | `TrayMenuItem[]` | No | Child rows of a `submenu` row. |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | No | A playback command the plugin runs natively when the row is picked, so it keeps working while the page is suspended (minimized, hidden to the tray, session locked). Such a row does not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails the whole call. |
| `config` | `TrayMenuConfig` | No | Menu-wide options to change. |
| `config.showPlaybackControls` | `boolean` | No | Inject the built-in previous, play/pause, next and stop rows into the `playback` zone. |
| `config.showSystemItems` | `boolean` | No | Inject the native show-main-window and exit rows into the `bottom` zone; both keep working while the page is suspended. |
| `config.customPosition` | `"top" \| "playback" \| "bottom"` | No | The zone `setContextMenu` writes its rows into. |
| `config.render` | `"native" \| "webview"` | No | Menu backend: the Win32 menu, or the self-drawn WebView2 overlay that renders the rich row kinds and the styling options below. |
| `config.autoNowPlaying` | `boolean` | No | Fill the empty `cover`, `title` and `subtitle` of `nowplaying` rows from the playing track when the menu opens; a value given by the caller always wins. Art is downscaled to 64 px and omitted above 256 KiB; `cover` filling is `webview` only. |
| `config.css` | `string` | No | Stylesheet injected into the `webview` menu on every opening, on top of the built-in styles; target the menu's stable class names (`.fb-menu`, `.fb-item`, `.fb-sep`, ...). |
| `config.cssReplace` | `boolean` | No | `true` disables the built-in styles so only `css` and the protected structural layer apply (`webview` only). |
| `config.backdrop` | `"acrylic" \| "mica" \| "mica-alt" \| "none"` | No | DWM backdrop of the `webview` menu, the same vocabulary as the windows. It snaps in and out with the window and cannot fade with CSS. |
| `config.backdropDarkMode` | `boolean` | No | Dark tint for the backdrop (`webview` only); `false` follows a light theme. |
| `config.closeAnimationMs` | `integer` | No | Milliseconds the `webview` menu plays its exit transition (`#menu.out`) before hiding on a user close; clamped to `0` to `1000`, `0` hides at once. |
| `config.layoutMode` | `"flat" \| "zones"` | No | DOM layout of the `webview` menu: `flat` keeps rows as direct children of the root, `zones` wraps each non-empty zone in `.fb-zone[data-zone]`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`items[].icon` is a reserved compatibility field and is not rendered by either
the native or WebView menu backend. For WebView-rendered item icons, use
`items[].iconSvg`; the native backend is text-only.

With `config.autoNowPlaying` on, any `cover` / `title` / `subtitle` you leave
empty on a `type: 'nowplaying'` item is filled from the current track when the
menu opens; a value you supply always wins. `cover` auto-fill is
`render: 'webview'` only and reads the now-playing in-memory art cache with no
disk fallback, so it stays empty for sources foobar2000 cannot extract art from
(most streams — pass `cover` yourself there). Art whose longest side exceeds
64 px is downscaled to 64 px and re-encoded as JPEG when resizing succeeds.
Smaller originals, or a failed resize, retain the original bytes and format.
The cover is omitted if those image bytes exceed 256 KiB, before base64 encoding.
The default stylesheet draws it at 40x40 CSS px; a 64 px thumbnail rendered above
roughly 51 CSS px at 125% display scaling is upscaled.

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

<!-- api-schema:begin tray.setIcon -->
Replace the tray icon image; fails while no icon is created.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `icon` | `string` | No | Icon to show. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// icoBase64: raw Base64-encoded .ico file bytes
await fb2k.invoke('tray.setIcon', { icon: icoBase64 });
```

### tray.setMenuItemState

<!-- api-schema:begin tray.setMenuItemState -->
Change one row's `checked` or `enabled` state in place, searching every zone and submenu for the id. Giving `checked` (even `false`) makes the row checkable. The native menu is rebuilt each time it opens, so the change shows on the next opening.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `string` | Yes | Id of the row. Must not be empty. |
| `checked` | `boolean` | No | New checkmark state. |
| `enabled` | `boolean` | No | New enabled state. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `found` | `boolean` | Whether a row with the id existed; `true` on success. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

An empty or missing `id` fails with `id required`, and omitting both `checked` and `enabled` fails with `at least one of checked/enabled required`. `found` reports whether a row with that id existed.

```js
await fb2k.invoke('tray.setMenuItemState', { id: 'settings', enabled: true });
```

### tray.setMenuZones

<!-- api-schema:begin tray.setMenuZones -->
Replace the user rows of all three zones in one call; a zone that is not given is cleared. Rows and `config` follow the rules of `setContextMenu`, and nothing is stored, `config` included, when any row is rejected or the resource limits are exceeded. Use it when the whole menu changes, instead of `clearMenuItems` followed by `appendMenuItems`: the menu can open between separate calls and show only part of the update, and two such updates that interleave add their rows twice.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `top` | `TrayMenuItem[]` | No | Rows of the `top` zone; omitted clears the zone. |
| `top[].id` | `string` | No | Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the caller's label and skip the matching built-in injection. |
| `top[].label` | `string` | No | Display text. |
| `top[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | No | Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is otherwise driven by `checked`. Default: `"normal"`. |
| `top[].enabled` | `boolean` | No | Whether the row can be clicked. Default: `true`. |
| `top[].visible` | `boolean` | No | Whether the row is shown. Default: `true`. |
| `top[].checked` | `boolean` | No | Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview` backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the row is not checkable. |
| `top[].icon` | `string` | No | Reserved; neither backend draws it. Use `iconSvg` for a row icon. |
| `top[].iconSvg` | `TrayIconSvg` | No | Icon drawn before the label by the `webview` backend. When any row of a menu level has a renderable icon, every `normal` and `submenu` row of that level reserves the icon column. |
| `top[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `top[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `top[].cover` | `string` | No | `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With `config.autoNowPlaying` an empty value is filled from the playing track's front art (`webview` only). |
| `top[].title` | `string` | No | `nowplaying` first line, usually the track title; falls back to `label`. |
| `top[].subtitle` | `string` | No | `nowplaying` second line, usually artist or album. |
| `top[].value` | `integer` | No | Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider` (clamped), the selected index for `segmented`. Reported back only for those kinds. |
| `top[].min` | `integer` | No | `slider` range minimum; swapped with `max` when larger. |
| `top[].max` | `integer` | No | `slider` range maximum. |
| `top[].orientation` | `"horizontal" \| "vertical"` | No | `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom. |
| `top[].segments` | `TraySegment[]` | No | `segmented` options, one row of mutually exclusive choices; `value` is the selected index. Picking one reports `{ id, value }` and keeps the menu open. |
| `top[].segments[].label` | `string` | No | Text of the segment, shown when it has no icon. |
| `top[].segments[].iconSvg` | `TrayIconSvg` | No | Icon of the segment, preferred over the label. |
| `top[].segments[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `top[].segments[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `top[].segments[].enabled` | `boolean` | No | `false` greys the segment out so it cannot be picked. Default: `true`. |
| `top[].submenu` | `TrayMenuItem[]` | No | Child rows of a `submenu` row. |
| `top[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | No | A playback command the plugin runs natively when the row is picked, so it keeps working while the page is suspended (minimized, hidden to the tray, session locked). Such a row does not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails the whole call. |
| `playback` | `TrayMenuItem[]` | No | Rows of the `playback` zone; omitted clears the zone. The rows `config.showPlaybackControls` injects are not stored in the zone, so clearing it leaves them in place. |
| `playback[].id` | `string` | No | Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the caller's label and skip the matching built-in injection. |
| `playback[].label` | `string` | No | Display text. |
| `playback[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | No | Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is otherwise driven by `checked`. Default: `"normal"`. |
| `playback[].enabled` | `boolean` | No | Whether the row can be clicked. Default: `true`. |
| `playback[].visible` | `boolean` | No | Whether the row is shown. Default: `true`. |
| `playback[].checked` | `boolean` | No | Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview` backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the row is not checkable. |
| `playback[].icon` | `string` | No | Reserved; neither backend draws it. Use `iconSvg` for a row icon. |
| `playback[].iconSvg` | `TrayIconSvg` | No | Icon drawn before the label by the `webview` backend. When any row of a menu level has a renderable icon, every `normal` and `submenu` row of that level reserves the icon column. |
| `playback[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `playback[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `playback[].cover` | `string` | No | `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With `config.autoNowPlaying` an empty value is filled from the playing track's front art (`webview` only). |
| `playback[].title` | `string` | No | `nowplaying` first line, usually the track title; falls back to `label`. |
| `playback[].subtitle` | `string` | No | `nowplaying` second line, usually artist or album. |
| `playback[].value` | `integer` | No | Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider` (clamped), the selected index for `segmented`. Reported back only for those kinds. |
| `playback[].min` | `integer` | No | `slider` range minimum; swapped with `max` when larger. |
| `playback[].max` | `integer` | No | `slider` range maximum. |
| `playback[].orientation` | `"horizontal" \| "vertical"` | No | `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom. |
| `playback[].segments` | `TraySegment[]` | No | `segmented` options, one row of mutually exclusive choices; `value` is the selected index. Picking one reports `{ id, value }` and keeps the menu open. |
| `playback[].segments[].label` | `string` | No | Text of the segment, shown when it has no icon. |
| `playback[].segments[].iconSvg` | `TrayIconSvg` | No | Icon of the segment, preferred over the label. |
| `playback[].segments[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `playback[].segments[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `playback[].segments[].enabled` | `boolean` | No | `false` greys the segment out so it cannot be picked. Default: `true`. |
| `playback[].submenu` | `TrayMenuItem[]` | No | Child rows of a `submenu` row. |
| `playback[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | No | A playback command the plugin runs natively when the row is picked, so it keeps working while the page is suspended (minimized, hidden to the tray, session locked). Such a row does not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails the whole call. |
| `bottom` | `TrayMenuItem[]` | No | Rows of the `bottom` zone; omitted clears the zone. The rows `config.showSystemItems` injects are not stored in the zone, so clearing it leaves them in place. |
| `bottom[].id` | `string` | No | Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the caller's label and skip the matching built-in injection. |
| `bottom[].label` | `string` | No | Display text. |
| `bottom[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | No | Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is otherwise driven by `checked`. Default: `"normal"`. |
| `bottom[].enabled` | `boolean` | No | Whether the row can be clicked. Default: `true`. |
| `bottom[].visible` | `boolean` | No | Whether the row is shown. Default: `true`. |
| `bottom[].checked` | `boolean` | No | Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview` backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the row is not checkable. |
| `bottom[].icon` | `string` | No | Reserved; neither backend draws it. Use `iconSvg` for a row icon. |
| `bottom[].iconSvg` | `TrayIconSvg` | No | Icon drawn before the label by the `webview` backend. When any row of a menu level has a renderable icon, every `normal` and `submenu` row of that level reserves the icon column. |
| `bottom[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `bottom[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `bottom[].cover` | `string` | No | `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With `config.autoNowPlaying` an empty value is filled from the playing track's front art (`webview` only). |
| `bottom[].title` | `string` | No | `nowplaying` first line, usually the track title; falls back to `label`. |
| `bottom[].subtitle` | `string` | No | `nowplaying` second line, usually artist or album. |
| `bottom[].value` | `integer` | No | Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider` (clamped), the selected index for `segmented`. Reported back only for those kinds. |
| `bottom[].min` | `integer` | No | `slider` range minimum; swapped with `max` when larger. |
| `bottom[].max` | `integer` | No | `slider` range maximum. |
| `bottom[].orientation` | `"horizontal" \| "vertical"` | No | `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom. |
| `bottom[].segments` | `TraySegment[]` | No | `segmented` options, one row of mutually exclusive choices; `value` is the selected index. Picking one reports `{ id, value }` and keeps the menu open. |
| `bottom[].segments[].label` | `string` | No | Text of the segment, shown when it has no icon. |
| `bottom[].segments[].iconSvg` | `TrayIconSvg` | No | Icon of the segment, preferred over the label. |
| `bottom[].segments[].iconSvg.viewBox` | `string` | Yes | The SVG `viewBox` attribute. |
| `bottom[].segments[].iconSvg.content` | `string` | Yes | The SVG inner markup. |
| `bottom[].segments[].enabled` | `boolean` | No | `false` greys the segment out so it cannot be picked. Default: `true`. |
| `bottom[].submenu` | `TrayMenuItem[]` | No | Child rows of a `submenu` row. |
| `bottom[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | No | A playback command the plugin runs natively when the row is picked, so it keeps working while the page is suspended (minimized, hidden to the tray, session locked). Such a row does not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails the whole call. |
| `config` | `TrayMenuConfig` | No | Menu-wide options to change. `customPosition` is stored for later `setContextMenu` calls and does not affect this one. |
| `config.showPlaybackControls` | `boolean` | No | Inject the built-in previous, play/pause, next and stop rows into the `playback` zone. |
| `config.showSystemItems` | `boolean` | No | Inject the native show-main-window and exit rows into the `bottom` zone; both keep working while the page is suspended. |
| `config.customPosition` | `"top" \| "playback" \| "bottom"` | No | The zone `setContextMenu` writes its rows into. |
| `config.render` | `"native" \| "webview"` | No | Menu backend: the Win32 menu, or the self-drawn WebView2 overlay that renders the rich row kinds and the styling options below. |
| `config.autoNowPlaying` | `boolean` | No | Fill the empty `cover`, `title` and `subtitle` of `nowplaying` rows from the playing track when the menu opens; a value given by the caller always wins. Art is downscaled to 64 px and omitted above 256 KiB; `cover` filling is `webview` only. |
| `config.css` | `string` | No | Stylesheet injected into the `webview` menu on every opening, on top of the built-in styles; target the menu's stable class names (`.fb-menu`, `.fb-item`, `.fb-sep`, ...). |
| `config.cssReplace` | `boolean` | No | `true` disables the built-in styles so only `css` and the protected structural layer apply (`webview` only). |
| `config.backdrop` | `"acrylic" \| "mica" \| "mica-alt" \| "none"` | No | DWM backdrop of the `webview` menu, the same vocabulary as the windows. It snaps in and out with the window and cannot fade with CSS. |
| `config.backdropDarkMode` | `boolean` | No | Dark tint for the backdrop (`webview` only); `false` follows a light theme. |
| `config.closeAnimationMs` | `integer` | No | Milliseconds the `webview` menu plays its exit transition (`#menu.out`) before hiding on a user close; clamped to `0` to `1000`, `0` hides at once. |
| `config.layoutMode` | `"flat" \| "zones"` | No | DOM layout of the `webview` menu: `flat` keeps rows as direct children of the root, `zones` wraps each non-empty zone in `.fb-zone[data-zone]`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The host applies the whole call in one step on the main thread, where the menu is
also built, so a right-click shows either the old menu or the new one, never a
mix of both. Calls from one page run in the order they were sent: of two updates
sent back to back, the later one is what remains.

```js
await fb2k.invoke('tray.setMenuZones', {
    top: [
        { id: 'nowplaying', type: 'nowplaying', title: 'Song', subtitle: 'Artist' },
        { type: 'separator' },
    ],
    bottom: [{ id: 'settings', label: 'Settings' }],
    config: { showPlaybackControls: true },
});
```

### tray.setMinimizeToTray

<!-- api-schema:begin tray.setMinimizeToTray -->
Hide the window to the tray instead of the taskbar when it is minimized.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | Yes | `true` hides to the tray. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('tray.setMinimizeToTray', { enabled: true });
```

### tray.setTooltip

<!-- api-schema:begin tray.setTooltip -->
Update the icon's hover text; fails while no icon is created.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `tooltip` | `string` | No | Hover text; empty clears it. Default: `""`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('tray.setTooltip', { tooltip: 'Artist - Title' });
```

### tray.showBalloon

<!-- api-schema:begin tray.showBalloon -->
Show a balloon notification from the tray icon; fails while no icon is created.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Notification title. Default: `""`. |
| `message` | `string` | No | Notification body. Default: `""`. |
| `icon` | `"info" \| "warning" \| "error"` | No | Icon shown in the balloon. Default: `"info"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('tray.showBalloon', { title: 'Now Playing', message: 'Artist - Title' });
```

## Runtime lifecycle, menu data, and events

The `taskbar.*` and `tray.*` families require a standalone main window. In a
panel, `taskbar.*` and `tray.*` calls fail with `code: 'PANEL_MODE_UNSUPPORTED'`. Create the icon with `tray.create` before relying on tray visibility, callbacks, or menu
operations. Windows accepts no more than seven thumbnail buttons; a thumbnail
install can also fail before the taskbar initializes its COM integration.

Tray menus are configured by `tray.setContextMenu` and can subsequently be
updated with `tray.appendMenuItems`, `tray.removeMenuItems`,
`tray.clearMenuItems`, and `tray.setMenuItemState`. To replace the whole menu,
call `tray.setMenuZones` once rather than chaining those calls, so a menu opened
in between never shows a partial update. `tray:menuItemClicked`
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

Promotion is keyed on the exact id only, never on `type`, so a
`type: 'nowplaying'` card carrying `id: '_sys_show'` also routes natively —
clicking the cover restores the main window. That works under
`render: 'webview'` only: the native backend draws `nowplaying` as a
non-clickable header line, so there is nothing to click and no route fires.

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

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

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

The built-in `showPlaybackControls` items are stateless: their labels are fixed
(`Play / Pause`, `Previous Track`, `Next Track`, `Stop`, localized to the
foobar2000 UI language but never to the playback state) and they carry no icon.
To reflect playback state in a label or icon, turn `showPlaybackControls` off,
declare your own `playbackAction` items as shown below, and drive their
appearance from `playback:*` events.

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
