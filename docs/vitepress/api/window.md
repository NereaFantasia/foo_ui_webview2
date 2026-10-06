# Window API

Methods of the `window` namespace.

## window

### window.blur

<!-- api-schema:begin window.blur -->
Hand the foreground to the window below the calling window in the z-order. Fails when there is no window.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.blur');
```

### window.broadcast

<!-- api-schema:begin window.broadcast -->
Send a message to every window except the caller, delivered as `window:message` with `{ sourceWindowId, message }`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `any` | Yes | The message, delivered as `message` of the event. `null` counts as missing. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.broadcast', { message: { type: 'themeChanged', theme: 'dark' } });
```

### window.cancelClose

<!-- api-schema:begin window.cancelClose -->
Keep the calling popup open after it received `window:beforeClose`; nothing happens when no close is pending. Fails with `NOT_FOUND` when the caller is not a popup.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.cancelClose');
```

### window.center

<!-- api-schema:begin window.center -->
Center the calling window on the work area of its monitor, keeping its size. Fails in panel mode and when there is no window.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.center');
```

### window.clearClickThroughExcludeRegions

<!-- api-schema:begin window.clearClickThroughExcludeRegions -->
Remove a popup's click-through exclude rectangles. Targets as `window.setClickThrough` does.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Id of the popup; omitted or empty, the calling window. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// popup-scoped; omit windowId only when calling from the popup itself
await fb2k.invoke('window.clearClickThroughExcludeRegions', { windowId: 'popup-1' });
```

### window.clearDragRegions

<!-- api-schema:begin window.clearDragRegions -->
Remove the calling window's drag rectangles. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.clearDragRegions');
```

### window.clearNoDragRegions

<!-- api-schema:begin window.clearNoDragRegions -->
Remove the calling window's no-drag rectangles. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.clearNoDragRegions');
```

### window.close

<!-- api-schema:begin window.close -->
Close the calling window as the system close command does. Succeeds without doing anything when there is no window. Fails in panel mode.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.close');
```

### window.closeAllPopups

<!-- api-schema:begin window.closeAllPopups -->
Close every popup as their close buttons do.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.closeAllPopups');
```

### window.closePopup

<!-- api-schema:begin window.closePopup -->
Close a popup as its close button does, so a popup created with `beforeClose` first gets `window:beforeClose`. `main` fails with `INVALID_PARAMS`, an id no popup has with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | Yes | Id of the popup to close. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// windowId is required here; there is no fallback to the calling window
await fb2k.invoke('window.closePopup', { windowId: 'popup-1' });
```

### window.confirmClose

<!-- api-schema:begin window.confirmClose -->
Let the calling popup close after it received `window:beforeClose`; nothing happens when no close is pending. Fails with `NOT_FOUND` when the caller is not a popup.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.confirmClose');
```

### window.createPopup

<!-- api-schema:begin window.createPopup -->
Open a popup window with its own WebView and bridge; at most 8 popups can be open. Works from a DUI/CUI panel too. Fails with `OPERATION_FAILED` when 8 popups are already open or the window cannot be created.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | No | Page to load. An `http://`, `https://`, `file:///` or `data:` URL is loaded as it is; a path containing `.html` is loaded from the theme (or the development server); anything else, including empty, loads the theme's `index.html` with this value as the `route` query parameter. Every URL except `data:` gets a `windowId` query parameter. A page loaded from an `http://` or `https://` URL can use the bridge only when the calling page already trusts that origin; any other absolute URL loads a page that cannot. Such a page still shows, but its calls fail with `ORIGIN_DENIED` and no event reaches it. Default: `""`. |
| `title` | `string` | No | Window title. Default: `""`. |
| `x` | `number` | No | Left edge in screen coordinates; omitted, Windows places the window. Fractions are dropped. |
| `y` | `number` | No | Top edge in screen coordinates; omitted, Windows places the window. Fractions are dropped. |
| `width` | `number` | No | Width in physical pixels; fractions are dropped. Default: `400`. |
| `height` | `number` | No | Height in physical pixels; fractions are dropped. Default: `300`. |
| `minWidth` | `number` | No | Minimum width for resizing, in physical pixels. Default: `200`. |
| `minHeight` | `number` | No | Minimum height for resizing, in physical pixels. Default: `150`. |
| `maxWidth` | `number` | No | Maximum width for resizing, in physical pixels; `0` means no bound. Default: `0`. |
| `maxHeight` | `number` | No | Maximum height for resizing, in physical pixels; `0` means no bound. Default: `0`. |
| `resizable` | `boolean` | No | Whether the user can resize the popup. Default: `true`. |
| `frame` | `boolean` | No | Whether to draw the native frame and caption; `false` creates a borderless popup. Default: `true`. |
| `transparent` | `boolean` | No | Whether the background is transparent. Default: `false`. |
| `alwaysOnTop` | `boolean` | No | Keep the popup above every other window. To keep it above the main window only, use the `standard` preset or `behavior.owner: "main"` instead. Default: `false`. |
| `showInTaskbar` | `boolean` | No | Whether the popup shows on the taskbar and in Alt+Tab; omitted, the preset decides. |
| `clickThrough` | `boolean` | No | Let mouse input pass through the popup. Default: `false`. |
| `beforeClose` | `boolean` | No | Send `window:beforeClose` and wait for `window.confirmClose` or `window.cancelClose` before closing. Default: `false`. |
| `profile` | `string` | No | Behavior preset: `standard`, `miniPlayer` or `desktopLyrics`, case-insensitive, with `-` or `_` allowed (`mini-player`); an unknown value is `standard`. Omitted, no preset. |
| `behavior` | `Record<string, any>` | No | Behavior overrides on top of the preset, keyed like `resolvedBehavior` of `window.getPopupBehavior`. |
| `backdropPolicy` | `Record<string, any>` | No | Backdrop policy overrides on top of the preset, keyed like `resolvedBackdropPolicy` of `window.getBackdropPolicy`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the new popup. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('window.createPopup', {
    url: 'popup.html',
    width: 480,
    height: 320,
    title: 'Now Playing',
});
if (res.success === false) throw new Error(res.error);
const { windowId } = res;
```

### window.enterFullscreen

<!-- api-schema:begin window.enterFullscreen -->
Enter fullscreen. A window that is already fullscreen fails with `OPERATION_FAILED`. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `isFullscreen` | `boolean` | Always `true`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.enterFullscreen');
```

### window.exitFullscreen

<!-- api-schema:begin window.exitFullscreen -->
Leave fullscreen and restore the state the window had before. A window that is not fullscreen fails with `OPERATION_FAILED`. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `isFullscreen` | `boolean` | Always `false`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.exitFullscreen');
```

### window.flash

<!-- api-schema:begin window.flash -->
Flash the calling window's caption and taskbar button to draw attention, or stop flashing. Fails in panel mode and when there is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | `true` to flash, `false` to stop. Default: `true`. |
| `count` | `integer` | No | How many times to flash. Default: `3`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// omit count and enabled to start flashing 3 times
await fb2k.invoke('window.flash');
```

### window.flashTaskbar

<!-- api-schema:begin window.flashTaskbar -->
Flash the calling window's taskbar button and caption a number of times. Fails in panel mode and when there is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `integer` | No | How many times to flash. Default: `3`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// omit count to flash 3 times
await fb2k.invoke('window.flashTaskbar');
```

### window.focus

<!-- api-schema:begin window.focus -->
Bring a window to the foreground, restoring it first when minimized and showing it when hidden. Fails with `NOT_FOUND` when there is no such window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// omit windowId to focus the calling window
await fb2k.invoke('window.focus');
```

### window.getAllWindows

<!-- api-schema:begin window.getAllWindows -->
List the main window and every popup with their policies, capabilities and rectangles.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `items` | `WindowInfo[]` | The main window first, then the popups. |
| `items[].windowId` | `string` | `main` or the popup id. |
| `items[].isMain` | `boolean` | Whether this is the main window. |
| `items[].title` | `string` | Window title. |
| `items[].url` | `string` | Popups only: the `url` the popup was created with. |
| `items[].profile` | `"legacy" \| "standard" \| "miniPlayer" \| "desktopLyrics"` | Popups only: the behavior preset; `legacy` when the popup was created without one. |
| `items[].behavior` | `Record<string, any>` | Popups only: the behavior overrides set on the popup. |
| `items[].resolvedBehavior` | `WindowPopupBehaviorState` | Popups only: the behavior in effect. |
| `items[].resolvedBehavior.showInTaskbar` | `boolean` | Whether the popup shows on the taskbar. |
| `items[].resolvedBehavior.showInAltTab` | `boolean` | Whether the popup shows in Alt+Tab. |
| `items[].resolvedBehavior.keepVisibleOnShowDesktop` | `boolean` | Whether the popup stays visible when the desktop is shown. |
| `items[].resolvedBehavior.allowMinimize` | `boolean` | Whether the popup can be minimized. |
| `items[].resolvedBehavior.owner` | `"none" \| "main"` | `main` keeps the popup above the main window and minimizes it with the main window; `none` makes it independent. |
| `items[].resolvedBehavior.noActivate` | `boolean` | Whether showing or clicking the popup leaves the focus where it was. |
| `items[].backdropPolicy` | `Record<string, any>` | The backdrop policy overrides set on the window. |
| `items[].resolvedBackdropPolicy` | `WindowBackdropPolicyState` | The backdrop policy in effect. |
| `items[].resolvedBackdropPolicy.activeEffect` | `"inherit" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | Backdrop while the window is active; `inherit` follows the foobar2000 preferences. |
| `items[].resolvedBackdropPolicy.inactiveEffect` | `"inherit" \| "system" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | Backdrop while the window is inactive: `inherit` keeps the active one and lets Windows dim it, `system` hands the frame back to the platform backdrop. |
| `items[].resolvedBackdropPolicy.darkMode` | `boolean` | Whether the backdrop uses its dark variant. |
| `items[].resolvedBackdropPolicy.reapplyOnActivate` | `boolean` | Whether the backdrop is written again on every activation. |
| `items[].capabilities` | `WindowObservationCapabilities` | Features the window supports. |
| `items[].capabilities.supportsBackdropPolicy` | `boolean` | Whether `window.setBackdropPolicy` applies. |
| `items[].capabilities.supportsFrameless` | `boolean` | Whether `window.setFrameless` applies. |
| `items[].capabilities.supportsCornerPreference` | `boolean` | Whether `window.setCornerPreference` applies. |
| `items[].capabilities.supportsPopupBehavior` | `boolean` | Whether `window.setPopupBehavior` applies. |
| `items[].capabilities.supportsMicaAlt` | `boolean` | Whether the Mica Alt backdrop can be drawn. |
| `items[].capabilities.supportsFullscreen` | `boolean` | Whether the window can go fullscreen. |
| `items[].capabilities.supportsOwnerPolicy` | `boolean` | Whether `behavior.owner` applies. |
| `items[].capabilities.supportsNoActivate` | `boolean` | Whether `behavior.noActivate` applies. |
| `items[].capabilities.supportsBeforeClose` | `boolean` | Whether `beforeClose` applies. |
| `items[].bounds` | `WindowBounds` | The window rectangle. |
| `items[].bounds.x` | `integer` | Left edge. |
| `items[].bounds.y` | `integer` | Top edge. |
| `items[].bounds.width` | `integer` | Width. |
| `items[].bounds.height` | `integer` | Height. |
| `items[].shell` | `any` | Diagnostic snapshot of the window shell (lifecycle and startup state); its shape can change between versions. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getAllWindows');
```

### window.getBackdropPolicy

<!-- api-schema:begin window.getBackdropPolicy -->
Report a window's backdrop policy: the overrides set on it and the policy in effect.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the window the call acted on. |
| `backdropPolicy` | `Record<string, any>` | The backdrop policy overrides set on the window. |
| `resolvedBackdropPolicy` | `WindowBackdropPolicyState` | The backdrop policy in effect. |
| `resolvedBackdropPolicy.activeEffect` | `"inherit" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | Backdrop while the window is active; `inherit` follows the foobar2000 preferences. |
| `resolvedBackdropPolicy.inactiveEffect` | `"inherit" \| "system" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | Backdrop while the window is inactive: `inherit` keeps the active one and lets Windows dim it, `system` hands the frame back to the platform backdrop. |
| `resolvedBackdropPolicy.darkMode` | `boolean` | Whether the backdrop uses its dark variant. |
| `resolvedBackdropPolicy.reapplyOnActivate` | `boolean` | Whether the backdrop is written again on every activation. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`resolvedBackdropPolicy` is the effective policy after profile defaults are applied.

- `activeEffect` values: `inherit` \| `none` \| `mica` \| `mica-alt` \| `acrylic` (the `WindowActiveBackdropEffect` type)
- `inactiveEffect` values: `inherit` \| `system` \| `none` \| `mica` \| `mica-alt` \| `acrylic` (the `WindowInactiveBackdropEffect` type). `inherit` is the host default: the unfocused window keeps the resolved `activeEffect` and DWM applies its own inactive dimming. `system` hands the unfocused frame back to the platform backdrop. A string outside the table is not rejected: it is echoed in `backdropPolicy`, but `resolvedBackdropPolicy` keeps its previous effect.

```js
// omit windowId to read the calling window
const res = await fb2k.invoke('window.getBackdropPolicy');
if (res.success === false) throw new Error(res.error);
const { resolvedBackdropPolicy } = res;
```

### window.getBounds

<!-- api-schema:begin window.getBounds -->
Report the calling window's rectangle in screen coordinates, in physical pixels, frame included. While the window is minimized this is where Windows parks it, not the geometry it restores to. The rectangle is zero when there is no window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `x` | `integer` | Left edge in screen coordinates. |
| `y` | `integer` | Top edge in screen coordinates. |
| `width` | `integer` | Width in physical pixels, frame included. |
| `height` | `integer` | Height in physical pixels, frame included. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getBounds');
```

### window.getCaptionButtonsWidth

<!-- api-schema:begin window.getCaptionButtonsWidth -->
Report the width of the main window's caption buttons (minimize, maximize, close) in physical pixels at its DPI. Main window only; without one the call reports `138` and `46` rather than failing.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `width` | `integer` | Width of the three buttons together. |
| `buttonWidth` | `integer` | Width of one button. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Values are physical pixels and track the main window's DPI. When no main window exists the call returns the DIP-baseline defaults (`width: 138`, `buttonWidth: 46`) rather than an error, so callers cannot distinguish "no window" from a genuine 100%-scale measurement.

```js
const result = await fb2k.invoke('window.getCaptionButtonsWidth');
```

### window.getCornerPreference

<!-- api-schema:begin window.getCornerPreference -->
Report the main window's corner rounding as last set; `default` when there is no main window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `string` | The corner rounding as last set. |
| `preference` | `string` | Same value as `mode`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

When no main window exists the call returns `"default"` rather than an error.

```js
const result = await fb2k.invoke('window.getCornerPreference');
```

### window.getCurrentWindowId

<!-- api-schema:begin window.getCurrentWindowId -->
Report the id of the calling window: `main`, a popup id or a panel's id. A caller that cannot be matched gets `main`.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the calling window. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getCurrentWindowId');
```

### window.getDevServerConfig

<!-- api-schema:begin window.getDevServerConfig -->
Report whether pages load from a development server instead of the installed files, and that server's address.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `useDevServer` | `boolean` | Whether pages load from the development server. |
| `devServerUrl` | `string` | Address of the development server. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getDevServerConfig');
```

### window.getDpiScale

<!-- api-schema:begin window.getDpiScale -->
Report the DPI of the calling window's device context and its ratio to 96; `96` when there is no window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `dpi` | `integer` | DPI; `96` is 100 %. |
| `scale` | `number` | `dpi / 96`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getDpiScale');
```

### window.getMaxSize

<!-- api-schema:begin window.getMaxSize -->
Report a window's requested maximum size; `0` means no bound.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `width` | `integer` | Maximum width in physical pixels at the window's current DPI; `0` means no bound. |
| `height` | `integer` | Maximum height in physical pixels at the window's current DPI; `0` means no bound. |
| `windowId` | `string` | Id of the window the call acted on. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Values are physical pixels; the host stores constraints in DIPs and converts using the target window's DPI. `0` means "no upper bound" and survives the conversion exactly.

The returned values are the **requested** constraints, not the currently effective window size. A physical → DIP → physical round-trip is quantized to whole DIPs, so at non-100% scaling `get` may differ from the value passed to `set` by up to 1px per axis (for example, `202px` at 125% reads back as `203px`). Treat the getters as reporting the constraint you set to within ±1px rather than byte-for-byte. `0` is exempt.

```js
const result = await fb2k.invoke('window.getMaxSize');
```

### window.getMinSize

<!-- api-schema:begin window.getMinSize -->
Report a window's requested minimum size, which is the value last set rather than the effective window size.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `width` | `integer` | Minimum width in physical pixels at the window's current DPI. |
| `height` | `integer` | Minimum height in physical pixels at the window's current DPI. |
| `windowId` | `string` | Id of the window the call acted on. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Values are physical pixels; the host stores constraints in DIPs and converts using the target window's DPI.

The returned values are the **requested** constraints, not the currently effective window size. A physical → DIP → physical round-trip is quantized to whole DIPs, so at non-100% scaling `get` may differ from the value passed to `set` by up to 1px per axis (for example, `202px` at 125% reads back as `203px`). Treat the getters as reporting the constraint you set to within ±1px rather than byte-for-byte.

```js
const result = await fb2k.invoke('window.getMinSize');
```

### window.getMode

<!-- api-schema:begin window.getMode -->
Report the calling page's hosting mode and window id. The main window and popups report `standalone` with `panelMode: false`, including popups opened from a panel. A registered window with an id reports the same id as `window.getCurrentWindowId`. Pages that adapt to panel mode read it on startup; DUI/CUI panels also announce the same fields with `panel:initialized`.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `"standalone" \| "dui" \| "cui" \| "panel" \| "unknown"` | `standalone`, `dui` or `cui`; `panel` when the page is in a panel that could not be told apart, `unknown` for a panel of an unrecognized kind. |
| `panelMode` | `boolean` | Whether the page is in a DUI or CUI panel; `false` for the main window and popups. |
| `windowId` | `string` | Id of the calling window: `main` for the main window, the popup's actual id, or the panel's id. A panel without an id reports `panel`; an unmatched caller reports `main`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getMode');
```

### window.getPopupBehavior

<!-- api-schema:begin window.getPopupBehavior -->
Report a popup's behavior preset, the overrides set on it and the behavior in effect. Popups only: `main` fails with `NOT_SUPPORTED`; without `windowId` the calling popup is used, and a caller that is not a popup fails with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Id of the popup; omitted or empty, the calling popup. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
| `profile` | `"legacy" \| "standard" \| "miniPlayer" \| "desktopLyrics"` | The behavior preset; `legacy` when the popup was created without one. |
| `behavior` | `Record<string, any>` | The behavior overrides set on the popup. |
| `resolvedBehavior` | `WindowPopupBehaviorState` | The behavior in effect. |
| `resolvedBehavior.showInTaskbar` | `boolean` | Whether the popup shows on the taskbar. |
| `resolvedBehavior.showInAltTab` | `boolean` | Whether the popup shows in Alt+Tab. |
| `resolvedBehavior.keepVisibleOnShowDesktop` | `boolean` | Whether the popup stays visible when the desktop is shown. |
| `resolvedBehavior.allowMinimize` | `boolean` | Whether the popup can be minimized. |
| `resolvedBehavior.owner` | `"none" \| "main"` | `main` keeps the popup above the main window and minimizes it with the main window; `none` makes it independent. |
| `resolvedBehavior.noActivate` | `boolean` | Whether showing or clicking the popup leaves the focus where it was. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const info = await fb2k.invoke('window.getPopupBehavior', { windowId: 'popup-1' });
```

### window.getState

<!-- api-schema:begin window.getState -->
Report the calling window's state flags and rectangle. Every flag is sent twice, bare and `is`-prefixed. When there is no window every flag is `false` and the rectangle is zero.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `maximized` | `boolean` | Whether the window is maximized. |
| `minimized` | `boolean` | Whether the window is minimized. |
| `fullscreen` | `boolean` | Whether the window is fullscreen. |
| `alwaysOnTop` | `boolean` | Whether the window is kept above other windows. |
| `focused` | `boolean` | Whether the window is the foreground window. |
| `isMaximized` | `boolean` | Same value as `maximized`. |
| `isMinimized` | `boolean` | Same value as `minimized`. |
| `isFullscreen` | `boolean` | Same value as `fullscreen`. |
| `isAlwaysOnTop` | `boolean` | Same value as `alwaysOnTop`. |
| `isFocused` | `boolean` | Same value as `focused`. |
| `width` | `integer` | Width in physical pixels, frame included. |
| `height` | `integer` | Height in physical pixels, frame included. |
| `x` | `integer` | Left edge in screen coordinates. |
| `y` | `integer` | Top edge in screen coordinates. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getState');
```

### window.getTitle

<!-- api-schema:begin window.getTitle -->
Report the calling window's title, up to 255 characters; empty when there is no window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `title` | `string` | The window title. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getTitle');
```

### window.getTitlebarHeight

<!-- api-schema:begin window.getTitlebarHeight -->
Report the title bar height of the calling window, the main window or a popup, in physical pixels. A caller that is neither gets the default, `32`.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `height` | `integer` | Title bar height in physical pixels. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getTitlebarHeight');
```

### window.getTitlebarInfo

<!-- api-schema:begin window.getTitlebarInfo -->
Report the main window's title bar height, caption button widths and maximized state together, in physical pixels. Main window only; without one the call reports `32`, `138`, `46` and `false`.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `height` | `integer` | Title bar height in physical pixels. |
| `captionButtonsWidth` | `integer` | Width of the three caption buttons together. |
| `captionButtonWidth` | `integer` | Width of one caption button. |
| `isMaximized` | `boolean` | Whether the main window is maximized. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Values are physical pixels and track the main window's DPI. When no main window exists the call returns the DIP-baseline defaults (`height: 32`, `captionButtonsWidth: 138`, `captionButtonWidth: 46`) rather than an error; those fallbacks are unscaled, so on non-100% displays they differ in unit from the normal path.

```js
const result = await fb2k.invoke('window.getTitlebarInfo');
```

### window.getZoom

<!-- api-schema:begin window.getZoom -->
Report the zoom factor of the calling window's WebView and the window's DPI. Without a WebView of its own the call reports zoom `1` and leaves out `dpi` and `dpiScale`.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `zoom` | `number` | The zoom factor; `1` is 100 %. |
| `dpi` | `integer` | The window's DPI; left out without a WebView. |
| `dpiScale` | `number` | `dpi / 96`; left out without a WebView. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getZoom');
```

### window.hasSavedBounds

<!-- api-schema:begin window.hasSavedBounds -->
Report whether an earlier session saved the main window's position, which a page can use to decide whether to apply a default size on first launch.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `hasSavedBounds` | `boolean` | Whether a position from an earlier session is saved. |
| `description` | `string` | An English sentence restating `hasSavedBounds`, for logs. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.hasSavedBounds');
```

### window.isAlwaysOnTop

<!-- api-schema:begin window.isAlwaysOnTop -->
Report whether the calling window is kept above other windows; `false` when there is no window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether the window is kept above other windows. |
| `isAlwaysOnTop` | `boolean` | Same value as `enabled`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.isAlwaysOnTop');
```

### window.isClickThrough

<!-- api-schema:begin window.isClickThrough -->
Report whether mouse input passes through a popup. Targets as `window.setClickThrough` does.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Id of the popup; omitted or empty, the calling window. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `clickThrough` | `boolean` | Whether mouse input passes through. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('window.isClickThrough', { windowId: 'popup-1' });
if (res.success === false) throw new Error(res.error);
const { clickThrough } = res;
```

### window.isFullscreen

<!-- api-schema:begin window.isFullscreen -->
Report whether a window is fullscreen.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `fullscreen` | `boolean` | Whether the window is fullscreen. |
| `isFullscreen` | `boolean` | Same value as `fullscreen`. |
| `windowId` | `string` | Id of the window the call acted on. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// omit windowId to query the calling window
const res = await fb2k.invoke('window.isFullscreen');
if (res.success === false) throw new Error(res.error);
const { isFullscreen } = res;
```

### window.isMaximized

<!-- api-schema:begin window.isMaximized -->
Report whether the calling window is maximized; `false` when there is no window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `maximized` | `boolean` | Whether the window is maximized. |
| `isMaximized` | `boolean` | Same value as `maximized`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.isMaximized');
```

### window.isMinimized

<!-- api-schema:begin window.isMinimized -->
Report whether the calling window is minimized; `false` when there is no window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `minimized` | `boolean` | Whether the window is minimized. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.isMinimized');
```

### window.isResizable

<!-- api-schema:begin window.isResizable -->
Report whether the user can resize a window by its frame.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `resizable` | `boolean` | Whether the user can resize the window by its frame. |
| `windowId` | `string` | Id of the window the call acted on. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Every window shell — including fully borderless popups — supports changing this at runtime via [`window.setResizable`](#window-setresizable).

```js
const result = await fb2k.invoke('window.isResizable');
```

### window.maximize

<!-- api-schema:begin window.maximize -->
Maximize the calling window with the system animation, leaving fullscreen first when it is fullscreen. The request is posted. Fails in panel mode and when there is no window.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.maximize');
```

### window.minimize

<!-- api-schema:begin window.minimize -->
Minimize the calling window with the system animation. The request is posted, so the window changes state after the call returns. Fails in panel mode and when there is no window.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.minimize');
```

### window.refreshWebView

<!-- api-schema:begin window.refreshWebView -->
Redraw the calling window's WebView, which clears rendering left behind by a backdrop change. Fails with `NOT_FOUND` when the calling window has no WebView of its own, as with a DUI/CUI panel.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.refreshWebView');
```

### window.reload

<!-- api-schema:begin window.reload -->
Reload the calling window's page. Fails with `NOT_FOUND` when the calling window has no WebView of its own, as with a DUI/CUI panel.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.reload');
```

### window.resetZoom

<!-- api-schema:begin window.resetZoom -->
Set the zoom factor of the calling window's WebView back to `1`; as with `window.setZoom`, the default zoom in the preferences no longer applies to it afterwards. Fails with `NOT_FOUND` when the calling window has no WebView of its own, and with `OPERATION_FAILED` when WebView2 refuses the factor.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `zoom` | `number` | Always `1`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.resetZoom');
```

### window.restore

<!-- api-schema:begin window.restore -->
Restore the calling window from minimized or maximized with the system animation. A fullscreen window leaves fullscreen instead and returns to the state it had before. Fails in panel mode and when there is no window.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.restore');
```

### window.sendMessage

<!-- api-schema:begin window.sendMessage -->
Send a message to one window, delivered as `window:message` with `{ sourceWindowId, message }`. Fails with `NOT_FOUND` when no window has that id.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `targetWindowId` | `string` | Yes | Id of the receiving window. Must not be empty. |
| `message` | `any` | Yes | The message, delivered as `message` of the event. `null` counts as missing. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.sendMessage', {
    targetWindowId: 'popup-1',
    message: { type: 'seek', position: 42 },
});
```

### window.setAcrylic

<!-- api-schema:begin window.setAcrylic -->
Turn the acrylic backdrop of a window on or off. The setting is stored even when the window does not draw it right away; the call then fails with `OPERATION_FAILED` and the failure carries the result fields. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `enabled` | `boolean` | No | `true` shows acrylic; `false` removes the backdrop set through `setMica` or `setAcrylic`. Default: `true`. |
| `darkMode` | `boolean` | No | Dark (`true`) or light (`false`) backdrop; omitted, unchanged. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Echo of `enabled`. |
| `darkMode` | `boolean` | Echo of `darkMode`; present when it was given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`success` reports whether the backdrop was actually applied, so it can be `false` even for a valid window when the platform refuses the effect.

```js
await fb2k.invoke('window.setAcrylic', { enabled: true, darkMode: true });
```

### window.setAlwaysOnTop

<!-- api-schema:begin window.setAlwaysOnTop -->
Keep the calling window above other windows, or stop doing so. Fails in panel mode and when there is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Whether to keep the window above other windows. Default: `true`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setAlwaysOnTop', { enabled: true });
```

### window.setBackdropPolicy

<!-- api-schema:begin window.setBackdropPolicy -->
Merge per-field overrides into a window's backdrop policy; a `null` value removes that override. The overrides are stored even when the window does not draw the result right away, for example while it is still hidden at startup; the call then fails with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `backdropPolicy` | `Record<string, any>` | Yes | Overrides keyed like `resolvedBackdropPolicy`, merged into the current ones; a `null` value removes that override. Other keys are stored and echoed but have no effect, and an effect name outside the allowed values leaves the effect in use unchanged. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the window the call acted on. |
| `backdropPolicy` | `Record<string, any>` | The backdrop policy overrides after the merge. |
| `resolvedBackdropPolicy` | `WindowBackdropPolicyState` | The backdrop policy in effect. |
| `resolvedBackdropPolicy.activeEffect` | `"inherit" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | Backdrop while the window is active; `inherit` follows the foobar2000 preferences. |
| `resolvedBackdropPolicy.inactiveEffect` | `"inherit" \| "system" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | Backdrop while the window is inactive: `inherit` keeps the active one and lets Windows dim it, `system` hands the frame back to the platform backdrop. |
| `resolvedBackdropPolicy.darkMode` | `boolean` | Whether the backdrop uses its dark variant. |
| `resolvedBackdropPolicy.reapplyOnActivate` | `boolean` | Whether the backdrop is written again on every activation. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Value tables match `window.getBackdropPolicy`:

- `activeEffect` values: `inherit` \| `none` \| `mica` \| `mica-alt` \| `acrylic` (the `WindowActiveBackdropEffect` type)
- `inactiveEffect` values: `inherit` \| `system` \| `none` \| `mica` \| `mica-alt` \| `acrylic` (the `WindowInactiveBackdropEffect` type). `inherit` is the host default: the unfocused window keeps the resolved `activeEffect` and DWM applies its own inactive dimming. `system` hands the unfocused frame back to the platform backdrop. A string outside the table is not rejected: it is echoed in `backdropPolicy`, but `resolvedBackdropPolicy` keeps its previous effect.

```js
await fb2k.invoke('window.setBackdropPolicy', {
    backdropPolicy: { activeEffect: 'acrylic', darkMode: true },
});

// Keep the resolved activeEffect while unfocused; DWM dims the frame itself
await fb2k.invoke('window.setBackdropPolicy', {
    backdropPolicy: { activeEffect: 'mica', inactiveEffect: 'inherit' },
});
```

### window.setBackgroundTransparency

<!-- api-schema:begin window.setBackgroundTransparency -->
Make a window's WebView background transparent, so the backdrop effect shows through, or opaque. Fails in panel mode, and with `OPERATION_FAILED` when neither the window nor its WebView took the change; that failure carries the result fields unless the window has no WebView.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `transparent` | `boolean` | No | `true` for a transparent background, `false` for an opaque one. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `transparent` | `boolean` | Echo of `transparent`. |
| `description` | `string` | An English sentence describing the new state, for logs. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setBackgroundTransparency', { transparent: true });
```

### window.setBlur

<!-- api-schema:begin window.setBlur -->
Turn the blur-behind effect of a window on or off. The setting is stored even when the window does not draw it right away; the call then fails with `OPERATION_FAILED` and the failure carries `enabled`. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `enabled` | `boolean` | No | Whether to blur what is behind the window. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Echo of `enabled`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setBlur', { enabled: true });
```

### window.setBounds

<!-- api-schema:begin window.setBounds -->
Move or resize the calling window in one call; omitted keys keep their current value and fractions are dropped. Fails in panel mode and when there is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `number` | No | New left edge in screen coordinates. |
| `y` | `number` | No | New top edge in screen coordinates. |
| `width` | `number` | No | New width in physical pixels, frame included. |
| `height` | `number` | No | New height in physical pixels, frame included. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setBounds', { x: 100, y: 100, width: 480, height: 320 });
```

### window.setClickThrough

<!-- api-schema:begin window.setClickThrough -->
Let mouse input pass through a popup to the windows below, except over the rectangles set with `window.setClickThroughExcludeRegions`. Popups only: without `windowId` the calling window is used, and a window that is not a popup fails with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Id of the popup; omitted or empty, the calling window. |
| `enabled` | `boolean` | No | Whether mouse input passes through. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `clickThrough` | `boolean` | Whether mouse input passes through now. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setClickThrough', { enabled: true });
```

### window.setClickThroughExcludeRegions

<!-- api-schema:begin window.setClickThroughExcludeRegions -->
Set the rectangles of a popup that keep taking mouse input while it lets input pass through, replacing the earlier ones. Rectangles are CSS pixels of the page, scaled by the popup's DPI; rectangles without a positive width and height are skipped, and at most 32 are kept. Targets as `window.setClickThrough` does.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Id of the popup; omitted or empty, the calling window. |
| `regions` | `WindowRegion[]` | No | The rectangles; omitted, none. |
| `regions[].x` | `number` | No | Left edge. Default: `0`. |
| `regions[].y` | `number` | No | Top edge. Default: `0`. |
| `regions[].width` | `number` | No | Width. Default: `0`. |
| `regions[].height` | `number` | No | Height. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
| `count` | `integer` | How many rectangles were kept. |
| `dpiScale` | `number` | Scale applied to the rectangles, the popup's DPI divided by 96. |
| `warning` | `string` | `regions truncated to 32` when more than 32 rectangles were given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setClickThroughExcludeRegions', {
    regions: [{ x: 12, y: 12, width: 160, height: 40 }],
});
```

### window.setCornerPreference

<!-- api-schema:begin window.setCornerPreference -->
Set the main window's Windows 11 corner rounding. Main window only, whichever window calls; popups manage their corners themselves. Fails in panel mode and when there is no main window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | `default` or `round` rounds the corners, `small` rounds them slightly and `none` keeps them square. Any other value rounds them and is reported back as given. Default: `"default"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`"default"` maps to rounded corners, because a borderless window has no standard non-client frame for the system default to apply to.

```js
await fb2k.invoke('window.setCornerPreference', { mode: 'round' });
```

### window.setDarkMode

<!-- api-schema:begin window.setDarkMode -->
Switch a window's backdrop between its dark and light variant. The setting is stored even when the window does not draw it right away; the call then fails with `OPERATION_FAILED` and the failure carries `enabled`. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `enabled` | `boolean` | No | `true` for the dark variant, `false` for the light one. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Echo of `enabled`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setDarkMode', { enabled: true });
```

### window.setDevServerConfig

<!-- api-schema:begin window.setDevServerConfig -->
Change the development server settings; omitted keys keep their value. They are used the next time a page loads. The response reports both settings as stored.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `useDevServer` | `boolean` | No | Whether pages load from the development server. |
| `devServerUrl` | `string` | No | Address of the development server, such as `http://localhost:5173`; stored as given. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `useDevServer` | `boolean` | Whether pages load from the development server, as stored. |
| `devServerUrl` | `string` | Address of the development server, as stored. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setDevServerConfig', {
    useDevServer: true,
    devServerUrl: 'http://localhost:5173',
});
```

### window.setDragRegions

<!-- api-schema:begin window.setDragRegions -->
Set the rectangles that drag the calling window, the main window or a popup, replacing the earlier ones. Rectangles are CSS pixels of the page, scaled by the window's DPI; rectangles without a positive width and height are skipped. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `WindowRegion[]` | No | The drag rectangles; omitted, none. |
| `regions[].x` | `number` | No | Left edge. Default: `0`. |
| `regions[].y` | `number` | No | Top edge. Default: `0`. |
| `regions[].width` | `number` | No | Width. Default: `0`. |
| `regions[].height` | `number` | No | Height. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | How many rectangles were kept. |
| `dpiScale` | `number` | Scale applied to the rectangles, the window's DPI divided by 96. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setDragRegions', {
    regions: [{ x: 0, y: 0, width: 800, height: 32 }],
});
```

### window.setFrameless

<!-- api-schema:begin window.setFrameless -->
Remove or restore the native frame and caption of a window. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `frameless` | `boolean` | No | `true` removes the frame, `false` restores it. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `frameless` | `boolean` | Whether the window is frameless now. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setFrameless', { frameless: true });
```

### window.setFullscreen

<!-- api-schema:begin window.setFullscreen -->
Enter or leave fullscreen. Setting the state the window already has succeeds. Leaving fullscreen restores the rectangle, the maximized state and the always-on-top state the window had before. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `enabled` | `boolean` | No | `true` to enter fullscreen, `false` to leave it. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `fullscreen` | `boolean` | Whether the window is fullscreen now. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setFullscreen', { enabled: true });
```

### window.setMaximizeButtonRegion

<!-- api-schema:begin window.setMaximizeButtonRegion -->
Tell the host where the page draws the main window's maximize button, so that on Windows 11 hovering it offers Snap layouts as a standard window's button does. The rectangle is CSS pixels of the page, scaled by the window's DPI and the page zoom, the same factor as `devicePixelRatio`, and replaces the earlier one; `region` omitted, or without a positive width and height, removes it. While the window is frameless, resizable and not full screen, the host answers the rectangle as the window's maximize button and passes the mouse input it receives there on to the page, so the button keeps its hover and pressed styles and its own click handler; on Windows 10 and in every other state the rectangle stays ordinary page content. Set it again whenever layout moves the button; a navigation or reload of the page removes it. Main window only: fails with `NOT_SUPPORTED` from a popup, in panel mode, and with `NOT_FOUND` when the caller is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `region` | `WindowRegion` | No | Where the page draws the maximize button; omitted, the host forgets the button. |
| `region.x` | `number` | No | Left edge. Default: `0`. |
| `region.y` | `number` | No | Top edge. Default: `0`. |
| `region.width` | `number` | No | Width. Default: `0`. |
| `region.height` | `number` | No | Height. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `hasRegion` | `boolean` | Whether a rectangle is now set: `false` after removing it, or when the rectangle had no positive width and height. |
| `snapLayouts` | `boolean` | The system offers Snap layouts for a maximize button, which means Windows 11 or later. Where it is `false` the host never answers the rectangle as the maximize button. Where it is `true` the button's `title` tooltip, which the page still shows on hover, would cover the Snap layouts flyout, so leave the title off and name the button with `aria-label`. |
| `scale` | `number` | Scale applied to the rectangle: the window's DPI divided by 96, times the page zoom. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Windows 11 shows Snap layouts only over a window's maximize button, which for a
page-drawn button means the host has to answer that rectangle as the button. The
button then no longer receives the mouse from Windows directly; the host passes the
input on, so `:hover`, `:active` and `click` keep working, and the page's click
handler still does the maximizing. Keep the rectangle current: set it after the
first layout and again whenever the button moves (window resize, maximize and
restore, DPI or zoom changes). Where `snapLayouts` is `true`, drop the button's
`title` and name it with `aria-label` instead, since the tooltip would cover the
flyout.

```js
const button = document.getElementById('maximize');
const report = () => {
    if (!button) return;
    const r = button.getBoundingClientRect();
    void fb2k.invoke('window.setMaximizeButtonRegion', {
        region: { x: r.left, y: r.top, width: r.width, height: r.height },
    });
};
if (button) new ResizeObserver(report).observe(button);
window.addEventListener('resize', report);
```

### window.setMaxSize

<!-- api-schema:begin window.setMaxSize -->
Set a window's maximum size; `0` removes the bound and fractions are dropped. The host keeps it in DIPs of the target window, so it reads back within 1 px (`0` exactly). A window larger than the new maximum shrinks at once, unless it is maximized, fullscreen or minimized. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `width` | `number` | No | Maximum width in physical pixels; `0` removes the bound. Default: `0`. |
| `height` | `number` | No | Maximum height in physical pixels; `0` removes the bound. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the window the call acted on. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Values are physical pixels, converted to the host's DIP storage using the target window's DPI; `0` (or negative) clears the bound. Applying a constraint re-validates the current window size immediately, so a window already larger than the new maximum is shrunk rather than waiting for the next user resize.

```js
await fb2k.invoke('window.setMaxSize', { width: 1920, height: 1080 });
```

### window.setMica

<!-- api-schema:begin window.setMica -->
Turn the Mica backdrop of a window on or off. The setting is stored even when the window does not draw it right away, for example while it is still hidden at startup; the call then fails with `OPERATION_FAILED` and the failure carries the result fields. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `enabled` | `boolean` | No | `true` shows Mica; `false` removes the backdrop set through `setMica` or `setAcrylic`. Default: `true`. |
| `variant` | `string` | No | `mica-alt` for the tabbed variant; any other value is `mica`. Default: `"mica"`. |
| `darkMode` | `boolean` | No | Dark (`true`) or light (`false`) backdrop; omitted, unchanged. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Echo of `enabled`. |
| `variant` | `string` | The variant used, `mica` or `mica-alt`. |
| `darkMode` | `boolean` | Echo of `darkMode`; present when it was given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Only `mica-alt` selects the alternate variant; every other value — including an unrecognized one — is normalized to `mica` rather than rejected. The returned `variant` echoes that normalized request, not the effect that ended up on screen: popups do not support Mica Alt and are downgraded, so `variant: "mica-alt"` can come back for a window that received a different backdrop. `success` reports whether the backdrop was actually applied and can be `false` for a valid window when the platform refuses the effect.

```js
await fb2k.invoke('window.setMica', { enabled: true, variant: 'mica-alt' });
```

### window.setMicaEffect

<!-- api-schema:begin window.setMicaEffect -->
Same as `window.setMica`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `enabled` | `boolean` | No | `true` shows Mica; `false` removes the backdrop set through `setMica` or `setAcrylic`. Default: `true`. |
| `variant` | `string` | No | `mica-alt` for the tabbed variant; any other value is `mica`. Default: `"mica"`. |
| `darkMode` | `boolean` | No | Dark (`true`) or light (`false`) backdrop; omitted, unchanged. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Echo of `enabled`. |
| `variant` | `string` | The variant used, `mica` or `mica-alt`. |
| `darkMode` | `boolean` | Echo of `darkMode`; present when it was given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setMicaEffect', { enabled: true });
```

### window.setMinSize

<!-- api-schema:begin window.setMinSize -->
Set a window's minimum size; fractions are dropped. The host keeps it in DIPs of the target window, so it reads back within 1 px. A window smaller than the new minimum grows at once, unless it is maximized, fullscreen or minimized (a minimized window grows when restored). Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `width` | `number` | No | Minimum width in physical pixels; `0` or less lowers it to 1 DIP. Default: `0`. |
| `height` | `number` | No | Minimum height in physical pixels; `0` or less lowers it to 1 DIP. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the window the call acted on. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Values are physical pixels, converted to the host's DIP storage using the target window's DPI, which keeps the constraint stable across DPI changes. Non-positive values normalise to a 1px floor. Applying a constraint re-validates the current window size immediately, so a window already smaller than the new minimum is grown rather than waiting for the next user resize.

```js
await fb2k.invoke('window.setMinSize', { width: 480, height: 320 });
```

### window.setNoDragRegions

<!-- api-schema:begin window.setNoDragRegions -->
Set the rectangles that never drag the calling window, such as buttons inside a drag area, replacing the earlier ones. Rectangles are CSS pixels of the page, scaled by the window's DPI; rectangles without a positive width and height are skipped. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `WindowRegion[]` | No | The no-drag rectangles; omitted, none. |
| `regions[].x` | `number` | No | Left edge. Default: `0`. |
| `regions[].y` | `number` | No | Top edge. Default: `0`. |
| `regions[].width` | `number` | No | Width. Default: `0`. |
| `regions[].height` | `number` | No | Height. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | How many rectangles were kept. |
| `dpiScale` | `number` | Scale applied to the rectangles, the window's DPI divided by 96. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setNoDragRegions', {
    regions: [{ x: 690, y: 0, width: 110, height: 32 }],
});
```

### window.setPopupBehavior

<!-- api-schema:begin window.setPopupBehavior -->
Change a popup's behavior preset or its per-field overrides at runtime, then announce the result with `window:behaviorChanged`. `profile` and `behavior` are independent: passing one leaves the other alone. Targets as `window.getPopupBehavior` does; an unknown preset fails with `INVALID_PARAMS`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Id of the popup; omitted or empty, the calling popup. |
| `profile` | `string` | No | New preset: `standard`, `miniPlayer` or `desktopLyrics`, case-insensitive, with `-` or `_` allowed (`mini-player`). |
| `behavior` | `Record<string, any>` | No | Overrides merged into the current ones; a `null` value removes that override. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the popup. |
| `profile` | `"legacy" \| "standard" \| "miniPlayer" \| "desktopLyrics"` | The behavior preset. |
| `behavior` | `Record<string, any>` | The behavior overrides after the merge. |
| `resolvedBehavior` | `WindowPopupBehaviorState` | The behavior in effect. |
| `resolvedBehavior.showInTaskbar` | `boolean` | Whether the popup shows on the taskbar. |
| `resolvedBehavior.showInAltTab` | `boolean` | Whether the popup shows in Alt+Tab. |
| `resolvedBehavior.keepVisibleOnShowDesktop` | `boolean` | Whether the popup stays visible when the desktop is shown. |
| `resolvedBehavior.allowMinimize` | `boolean` | Whether the popup can be minimized. |
| `resolvedBehavior.owner` | `"none" \| "main"` | `main` keeps the popup above the main window and minimizes it with the main window; `none` makes it independent. |
| `resolvedBehavior.noActivate` | `boolean` | Whether showing or clicking the popup leaves the focus where it was. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`resolvedBehavior` is the effective policy after the profile defaults and your overrides are merged. `profile` and `behavior` are independent: supplying one does not reset the other, and a `null` value inside `behavior` erases that override rather than storing null.

Profile matching is case-insensitive and also accepts hyphen and underscore spellings, so `miniPlayer`, `miniplayer`, `mini-player`, and `mini_player` are equivalent. A preset that was set is reported under its canonical name; a popup created without one reports `legacy` until a preset is set.

Passing `windowId: "main"` fails with `NOT_SUPPORTED`. Omitting `windowId` requires the caller to itself be a popup, otherwise the call fails with `NOT_FOUND`.

```js
// switch profile only
await fb2k.invoke('window.setPopupBehavior', { profile: 'miniPlayer' });
// field-level override, keeping the current profile
await fb2k.invoke('window.setPopupBehavior', {
    behavior: { showInTaskbar: true, showInAltTab: true }
});
// clear a field, falling back to the profile default
await fb2k.invoke('window.setPopupBehavior', {
    behavior: { showInTaskbar: null }
});
```

### window.setPosition

<!-- api-schema:begin window.setPosition -->
Move the calling window's top-left corner, keeping its size; fractions are dropped. Fails in panel mode and when there is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `number` | No | New left edge in screen coordinates. Default: `0`. |
| `y` | `number` | No | New top edge in screen coordinates. Default: `0`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setPosition', { x: 100, y: 100 });
```

### window.setResizable

<!-- api-schema:begin window.setResizable -->
Set whether the user can resize a window by its frame. Setting the value the window already has succeeds. Fails in panel mode, and with `OPERATION_FAILED` when Windows refuses the new style; that failure carries `windowId`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |
| `resizable` | `boolean` | No | Whether the user can resize the window by its frame. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `windowId` | `string` | Id of the window the call acted on. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Every window shell supports this at runtime, including fully borderless popups (`frame: false` plus `transparent: true` with no backdrop effect): those windows collapse their entire non-client area, so adding a sizing border changes hit-testing without altering appearance. `success: false` therefore indicates a genuine Win32 failure — the style could not be written, or the frame could not be refreshed — not an unsupported window shape; the requested state is not committed in that case.

::: warning Behavior change
This call previously always targeted the main window regardless of caller, so invoking it from a popup reconfigured the main window instead.
:::

```js
await fb2k.invoke('window.setResizable', { resizable: false });
```

### window.setSize

<!-- api-schema:begin window.setSize -->
Resize the calling window, keeping its position; fractions are dropped. The size stays within the window's minimum and maximum, and the response does not say whether it was held back. Fails in panel mode and when there is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `width` | `number` | No | New width in physical pixels, frame included. Default: `800`. |
| `height` | `number` | No | New height in physical pixels, frame included. Default: `600`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setSize', { width: 1024, height: 640 });
```

### window.setTitle

<!-- api-schema:begin window.setTitle -->
Set the calling window's title, shown in the title bar and on the taskbar. Fails in panel mode and when there is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | The new title. Default: `"foobar2000"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setTitle', { title: 'Now Playing' });
```

### window.setTitlebarHeight

<!-- api-schema:begin window.setTitlebarHeight -->
Set the title bar height of the calling window, the main window or a popup; a popup's height leaves the main window alone. The height must be 24 to 100, otherwise the call fails with `INVALID_PARAMS`. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the main window nor a popup.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `height` | `number` | No | Title bar height in physical pixels, 24 to 100; fractions are dropped. Default: `32`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `height` | `integer` | The height that was set. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setTitlebarHeight', { height: 40 });
```

### window.setZoom

<!-- api-schema:begin window.setZoom -->
Set the zoom factor of the calling window's WebView. From then on, for the rest of the session, the default zoom in the preferences no longer applies to this WebView. Fails with `NOT_FOUND` when the calling window has no WebView of its own, and with `OPERATION_FAILED` when WebView2 refuses the factor; that failure carries the `zoom` in effect.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `zoom` | `number` | No | Zoom factor; `1` is 100 %. Default: `1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `zoom` | `number` | The zoom factor in effect after the call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setZoom', { zoom: 1.25 });
```

### window.setZoomForDpi

<!-- api-schema:begin window.setZoomForDpi -->
Set the zoom factor of the calling window's WebView to `dpi / 96`; as with `window.setZoom`, the default zoom in the preferences no longer applies to it afterwards. Fails with `NOT_FOUND` when the calling window has no WebView of its own, and with `OPERATION_FAILED` when WebView2 refuses the factor; that failure carries the result fields.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `dpi` | `integer` | No | DPI to match; `0` or less uses the window's DPI, or `96` when it is unknown. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `dpi` | `integer` | The DPI used. |
| `zoom` | `number` | The zoom factor in effect after the call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// omit dpi to derive the zoom from the calling window's current DPI
const res = await fb2k.invoke('window.setZoomForDpi');
if (res.success === false) throw new Error(res.error);
const { zoom } = res;
```

### window.showSystemMenu

<!-- api-schema:begin window.showSystemMenu -->
Open the calling window's system menu (restore, move, size, minimize, maximize, close) and run the command picked from it. Coordinates are screen pixels; fractions are dropped. With a positive `w` and `h` the menu opens below the rectangle `x`, `y`, `w`, `h` and keeps clear of it; otherwise it opens at `x`, `y`. Fails in panel mode, when there is no window, and with `OPERATION_FAILED` when the window has no system menu.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `number` | No | Left edge of the rectangle to keep clear, or the menu position when `w` or `h` is not positive. Default: `0`. |
| `y` | `number` | No | Top edge of the rectangle to keep clear, or the menu position when `w` or `h` is not positive. Default: `0`. |
| `w` | `number` | No | Width of the rectangle to keep clear. Default: `0`. |
| `h` | `number` | No | Height of the rectangle to keep clear. Default: `0`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// pass w/h to keep the menu clear of the button that opened it
await fb2k.invoke('window.showSystemMenu', { x: 8, y: 0, w: 32, h: 32 });
```

### window.startDrag

<!-- api-schema:begin window.startDrag -->
Start moving the calling window with the mouse, as pressing its title bar does. Call it from a `mousedown` handler while the button is still down. Fails in panel mode and when there is no window.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.startDrag');
```

### window.startResize

<!-- api-schema:begin window.startResize -->
Start resizing the calling window from one edge or corner with the mouse. Call it from a `mousedown` handler while the button is still down. Fails in panel mode and when there is no window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `edge` | `string` | No | Edge or corner to drag: `left`, `right`, `top`, `bottom`, `topleft`, `topright`, `bottomleft` or `bottomright`. Any other value drags the bottom-right corner. Default: `"bottomright"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('window.startResize', { edge: 'bottomright' });
```

### window.toggleAlwaysOnTop

<!-- api-schema:begin window.toggleAlwaysOnTop -->
Flip whether the calling window is kept above other windows. Fails in panel mode, and with `enabled: false` when there is no window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether the window is now kept above other windows. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.toggleAlwaysOnTop');
```

### window.toggleFullscreen

<!-- api-schema:begin window.toggleFullscreen -->
Enter fullscreen, or leave it when the window is fullscreen. Fails in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `fullscreen` | `boolean` | Whether the window is fullscreen now. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.toggleFullscreen');
```

### window.toggleMaximize

<!-- api-schema:begin window.toggleMaximize -->
Maximize the calling window, or restore it when it is maximized. A fullscreen window leaves fullscreen instead. Fails in panel mode, and with `maximized: false` when there is no window.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `maximized` | `boolean` | Whether the window is maximized once the posted request lands. After leaving fullscreen it is the state the window returned to. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.toggleMaximize');
```

## Runtime behavior and events

All `window.*` calls run in the context of the calling WebView unless a method
accepts `windowId`. A value of `main` identifies the main shell; popup IDs are
returned by `window.createPopup` and `window.getAllWindows`. Calls that require
a standalone shell report an unsupported or not-found result in panel mode
instead of silently targeting an unrelated window.

`window.setDragRegions`, `window.setNoDragRegions`, and click-through exclude
regions accept CSS-pixel rectangles. The native handler converts them using the
target window DPI. Popup-only operations such as click-through and close
confirmation reject a main-window target.

The runtime emits `window:stateChanged` when shell state changes and routes
`window:beforeClose` to the popup that requested close confirmation. Popup
lifecycle and coordination events include `window:popupOpened`,
`window:popupClosed`, `window:message`, `window:behaviorChanged`,
`window:backdropStateChanged`, `window:hoverStateChanged`,
`window:minimizeSuppressed`, and `window:alwaysOnTopChanged`. Event payloads
are runtime data; callers should tolerate fields added by the shell.
