# Window API

English API reference for the `window` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## window

### window.blur


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.blur');
```

### window.broadcast


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | Yes | Required. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.broadcast', { message: { type: 'themeChanged', theme: 'dark' } });
```

### window.cancelClose


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.cancelClose');
```

### window.center


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.center');
```

### window.clearClickThroughExcludeRegions


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional. |

**Returns**: `{"error":"...","success":true,"windowId":"..."}`

```js
// popup-scoped; omit windowId only when calling from the popup itself
await fb2k.invoke('window.clearClickThroughExcludeRegions', { windowId: 'popup-1' });
```

### window.clearDragRegions


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.clearDragRegions');
```

### window.clearNoDragRegions


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.clearNoDragRegions');
```

### window.close


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('window.close');
```

### window.closeAllPopups


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('window.closeAllPopups');
```

### window.closePopup


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional. |

**Returns**: `{"error":"...","success":true}`

```js
// windowId is required here; there is no fallback to the calling window
await fb2k.invoke('window.closePopup', { windowId: 'popup-1' });
```

### window.confirmClose


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.confirmClose');
```

### window.createPopup


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `alwaysOnTop` | `boolean` | No | Optional; default false. |
| `backdropPolicy` | `object` | No | Optional; omitted by default. |
| `beforeClose` | `boolean` | No | Optional; default false. |
| `behavior` | `object` | No | Optional; omitted by default. |
| `clickThrough` | `boolean` | No | Optional; default false. |
| `frame` | `boolean` | No | Optional; default true. |
| `height` | `integer` | No | Optional; default 300. |
| `maxHeight` | `integer` | No | Optional; default 0. |
| `maxWidth` | `integer` | No | Optional; default 0. |
| `minHeight` | `integer` | No | Optional; default 150. |
| `minWidth` | `integer` | No | Optional; default 200. |
| `profile` | `string` | No | Optional. |
| `resizable` | `boolean` | No | Optional; default true. |
| `showInTaskbar` | `boolean` | No | Optional; default false. |
| `title` | `string` | No | Optional. |
| `transparent` | `boolean` | No | Optional; default false. |
| `url` | `string` | No | Optional. |
| `width` | `integer` | No | Optional; default 400. |
| `x` | `integer` | No | Optional; default CW_USEDEFAULT. |
| `y` | `integer` | No | Optional; default CW_USEDEFAULT. |

**Returns**: `{"error":"...","success":true,"windowId":"..."}`

```js
const { windowId } = await fb2k.invoke('window.createPopup', {
    url: 'popup.html',
    width: 480,
    height: 320,
    title: 'Now Playing',
});
```

### window.enterFullscreen


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |

**Returns**: `{"error":"...","isFullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.enterFullscreen');
```

### window.exitFullscreen


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |

**Returns**: `{"error":"...","isFullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.exitFullscreen');
```

### window.flash


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `integer` | No | Optional; default 3. |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","success":true}`

```js
// omit count and enabled to start flashing 3 times
await fb2k.invoke('window.flash');
```

### window.flashTaskbar


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `integer` | No | Optional; default 3. |

**Returns**: `{"success":true}`

```js
// omit count to flash 3 times
await fb2k.invoke('window.flashTaskbar');
```

### window.focus


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional. |

**Returns**: `{"error":"...","success":true}`

```js
// omit windowId to focus the calling window
await fb2k.invoke('window.focus');
```

### window.getAllWindows


_No parameters._

**Returns**: `{"items":"...","success":true}`

```js
const result = await fb2k.invoke('window.getAllWindows');
```

### window.getBackdropPolicy

Reads a window's DWM backdrop policy. Supports both the main window and popups.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window. Defaults to the calling window. |

**Returns**: `{ "success": true, "windowId": "...", "backdropPolicy": { ... }, "resolvedBackdropPolicy": { ... } }`

`resolvedBackdropPolicy` is the effective policy after profile defaults are applied. A window that cannot be resolved returns `{ "success": false, "error": "..." }`.

```js
// omit windowId to read the calling window
const { resolvedBackdropPolicy } = await fb2k.invoke('window.getBackdropPolicy');
```

### window.getBounds


_No parameters._

**Returns**: `{"height":"...","width":"...","x":"...","y":"..."}`

```js
const result = await fb2k.invoke('window.getBounds');
```

### window.getCaptionButtonsWidth

Reports the geometry of the main window's custom-drawn caption buttons (minimise / maximise / close), for frontends that render their own titlebar. Main window only: it does not accept `windowId` and ignores the calling window. Popups have no custom-drawn caption buttons, so there is no popup-scoped value to report.

_No parameters._

**Returns**: `{"buttonWidth":"...","width":"..."}`

Values are physical pixels and track the main window's DPI. When no main window exists the call returns the DIP-baseline defaults (`width: 138`, `buttonWidth: 46`) rather than an error, so callers cannot distinguish "no window" from a genuine 100%-scale measurement.

```js
const result = await fb2k.invoke('window.getCaptionButtonsWidth');
```

### window.getCornerPreference

Returns the main window's Windows 11 corner-rounding preference; `mode` and `preference` are the same value under two names. Main window only: it does not accept `windowId` and ignores the calling window. Popups do not expose this setting — they manage corner rounding internally (rounded when borderless, system default otherwise) and report `supportsCornerPreference: false` in their capabilities.

_No parameters._

**Returns**: `{"mode":"...","preference":"..."}`

When no main window exists the call returns `"default"` rather than an error.

```js
const result = await fb2k.invoke('window.getCornerPreference');
```

### window.getCurrentWindowId


_No parameters._

**Returns**: `{"success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.getCurrentWindowId');
```

### window.getDevServerConfig


_No parameters._

**Returns**: `{"devServerUrl":"...","success":true,"useDevServer":"..."}`

```js
const result = await fb2k.invoke('window.getDevServerConfig');
```

### window.getDpiScale


_No parameters._

**Returns**: `{"dpi":"...","scale":"...","success":true}`

```js
const result = await fb2k.invoke('window.getDpiScale');
```

### window.getMaxSize

Reads a window's requested maximum size. Resolution selects the explicit `windowId` or the calling window; it does **not** fall back to the main window, so a call from an unresolvable context fails instead of silently reporting another window's constraints. Panel (DUI/CUI) callers are rejected with `panelMode: true`, because a panel is not a window shell.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window. Defaults to the calling window. |

**Returns**: `{"height":"...","success":true,"width":"...","windowId":"..."}`. A window that cannot be resolved returns `{ "success": false, "error": "..." }`.

Values are physical pixels; the host stores constraints in DIPs and converts using the target window's DPI. `0` means "no upper bound" and survives the conversion exactly.

The returned values are the **requested** constraints, not the currently effective window size. A physical → DIP → physical round-trip is quantized to whole DIPs, so at non-100% scaling `get` may differ from the value passed to `set` by up to 1px per axis (for example, `202px` at 125% reads back as `203px`). Treat the getters as reporting the constraint you set to within ±1px rather than byte-for-byte. `0` is exempt.

```js
const result = await fb2k.invoke('window.getMaxSize');
```

### window.getMinSize

Reads a window's requested minimum size. Resolution selects the explicit `windowId` or the calling window; it does **not** fall back to the main window, so a call from an unresolvable context fails instead of silently reporting another window's constraints. Panel (DUI/CUI) callers are rejected with `panelMode: true`, because a panel is not a window shell.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window. Defaults to the calling window. |

**Returns**: `{"height":"...","success":true,"width":"...","windowId":"..."}`. A window that cannot be resolved returns `{ "success": false, "error": "..." }`.

Values are physical pixels; the host stores constraints in DIPs and converts using the target window's DPI.

The returned values are the **requested** constraints, not the currently effective window size. A physical → DIP → physical round-trip is quantized to whole DIPs, so at non-100% scaling `get` may differ from the value passed to `set` by up to 1px per axis (for example, `202px` at 125% reads back as `203px`). Treat the getters as reporting the constraint you set to within ±1px rather than byte-for-byte.

```js
const result = await fb2k.invoke('window.getMinSize');
```

### window.getMode


_No parameters._

**Returns**: `{"mode":"...","panel":"...","panelMode":"...","windowId":"..."}`

```js
const result = await fb2k.invoke('window.getMode');
```

### window.getPopupBehavior


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional. |

**Returns**: `{"success":true,"windowId":"..."}`

```js
const info = await fb2k.invoke('window.getPopupBehavior', { windowId: 'popup-1' });
```

### window.getState


_No parameters._

**Returns**: `{"alwaysOnTop":"...","focused":"...","fullscreen":"...","height":"...","isAlwaysOnTop":"...","isFocused":"...","isFullscreen":"...","isMaximized":"...","isMinimized":"...","maximized":"...","minimized":"...","width":"...","x":"...","y":"..."}`

```js
const result = await fb2k.invoke('window.getState');
```

### window.getTitle


_No parameters._

**Returns**: `{"title":"..."}`

```js
const result = await fb2k.invoke('window.getTitle');
```

### window.getTitlebarHeight


_No parameters._

**Returns**: `{"height":"..."}`

```js
const result = await fb2k.invoke('window.getTitlebarHeight');
```

### window.getTitlebarInfo

Bundles the main window's titlebar height with its custom-drawn caption-button geometry and maximised state. Main window only: it does not accept `windowId` and ignores the calling window. Three of the four fields have no popup equivalent — popups expose only a titlebar height and have no custom-drawn caption buttons — so the call stays main-scoped rather than returning zeros for the missing fields.

_No parameters._

**Returns**: `{"captionButtonWidth":"...","captionButtonsWidth":"...","height":"...","isMaximized":"..."}`

Values are physical pixels and track the main window's DPI. When no main window exists the call returns the DIP-baseline defaults (`height: 32`, `captionButtonsWidth: 138`, `captionButtonWidth: 46`) rather than an error; those fallbacks are unscaled, so on non-100% displays they differ in unit from the normal path.

```js
const result = await fb2k.invoke('window.getTitlebarInfo');
```

### window.getZoom


_No parameters._

**Returns**: `{"dpi":"...","dpiScale":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.getZoom');
```

### window.hasSavedBounds


_No parameters._

**Returns**: `{"description":"...","hasSavedBounds":"..."}`

```js
const result = await fb2k.invoke('window.hasSavedBounds');
```

### window.isAlwaysOnTop


_No parameters._

**Returns**: `{"enabled":"...","isAlwaysOnTop":"..."}`

```js
const result = await fb2k.invoke('window.isAlwaysOnTop');
```

### window.isClickThrough


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional. |

**Returns**: `{"clickThrough":"...","error":"...","success":true}`

```js
const { clickThrough } = await fb2k.invoke('window.isClickThrough', { windowId: 'popup-1' });
```

### window.isFullscreen


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional target window id resolved by `WindowTargetResolver::ResolveForObservation`; omitted uses the caller window. |

**Returns**: `{"fullscreen":"...","isFullscreen":"..."}`

```js
// omit windowId to query the calling window
const { isFullscreen } = await fb2k.invoke('window.isFullscreen');
```

### window.isMaximized


_No parameters._

**Returns**: `{"isMaximized":"...","maximized":"..."}`

```js
const result = await fb2k.invoke('window.isMaximized');
```

### window.isMinimized


_No parameters._

**Returns**: `{"minimized":"..."}`

```js
const result = await fb2k.invoke('window.isMinimized');
```

### window.isResizable

Reports a window's requested resizable state. Resolution selects the explicit `windowId` or the calling window; it does **not** fall back to the main window. Panel (DUI/CUI) callers are rejected with `panelMode: true`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window. Defaults to the calling window. |

**Returns**: `{"resizable":"...","success":true,"windowId":"..."}`. A window that cannot be resolved returns `{ "success": false, "error": "..." }`.

Every window shell — including fully borderless popups — supports changing this at runtime via [`window.setResizable`](#window-setresizable).

```js
const result = await fb2k.invoke('window.isResizable');
```

### window.maximize


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.maximize');
```

### window.minimize


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.minimize');
```

### window.refreshWebView


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.refreshWebView');
```

### window.reload


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.reload');
```

### window.resetZoom


_No parameters._

**Returns**: `{"error":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.resetZoom');
```

### window.restore


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.restore');
```

### window.sendMessage


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `targetWindowId` | `string` | Yes | Required. |
| `message` | `json` | Yes | Required. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.sendMessage', {
    targetWindowId: 'popup-1',
    message: { type: 'seek', position: 42 },
});
```

### window.setAcrylic

Applies or clears the acrylic backdrop. Not supported in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |
| `darkMode` | `boolean` | No | Optional; leaves the existing mode when omitted. |

**Returns**: `{ "success": true, "enabled": true }`, plus `darkMode` echoed back only when you supplied it.

`success` reports whether the backdrop was actually applied, so it can be `false` even for a valid window when the platform refuses the effect.

```js
await fb2k.invoke('window.setAcrylic', { enabled: true, darkMode: true });
```

### window.setAlwaysOnTop


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.setAlwaysOnTop', { enabled: true });
```

### window.setBackdropPolicy


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `backdropPolicy` | `object` | Yes | Required. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.setBackdropPolicy', {
    backdropPolicy: { activeEffect: 'acrylic', darkMode: true },
});
```

### window.setBackgroundTransparency


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `transparent` | `boolean` | No | Optional; default true. |

**Returns**: `{"description":"...","error":"...","success":true,"transparent":"..."}`

```js
await fb2k.invoke('window.setBackgroundTransparency', { transparent: true });
```

### window.setBlur


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"enabled":"...","success":true}`

```js
await fb2k.invoke('window.setBlur', { enabled: true });
```

### window.setBounds


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `integer` | No | Optional; default current x. |
| `y` | `integer` | No | Optional; default current y. |
| `width` | `integer` | No | Optional; default current width. |
| `height` | `integer` | No | Optional; default current height. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.setBounds', { x: 100, y: 100, width: 480, height: 320 });
```

### window.setClickThrough


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default true. |
| `windowId` | `string` | No | Optional. |

**Returns**: `{"clickThrough":"...","error":"...","success":true}`

```js
await fb2k.invoke('window.setClickThrough', { enabled: true });
```

### window.setClickThroughExcludeRegions


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `array` | No | Optional; omitted by default. |
| `windowId` | `string` | No | Optional. |

**Returns**: `{"count":0,"dpiScale":"...","success":true,"warning":"...","windowId":"..."}`

```js
await fb2k.invoke('window.setClickThroughExcludeRegions', {
    regions: [{ x: 12, y: 12, width: 160, height: 40 }],
});
```

### window.setCornerPreference

Sets the main window's Windows 11 corner-rounding preference. Main window only: it does not accept `windowId` and ignores the calling window. Popups do not accept this setting — they manage corner rounding internally and report `supportsCornerPreference: false` — so the call has no popup-scoped form. Panel (DUI/CUI) callers are rejected with `panelMode: true`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | `"default"`, `"none"`, `"round"` or `"small"`. Defaults to `"default"`. |

**Returns**: `{"error":"...","success":true}`

`"default"` maps to rounded corners, because a borderless window has no standard non-client frame for the system default to apply to.

```js
await fb2k.invoke('window.setCornerPreference', { mode: 'round' });
```

### window.setDarkMode


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"enabled":"...","success":true}`

```js
await fb2k.invoke('window.setDarkMode', { enabled: true });
```

### window.setDevServerConfig


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `devServerUrl` | `string` | No | Optional. |
| `useDevServer` | `boolean` | No | Optional; default false. |

**Returns**: `{"devServerUrl":"...","success":true,"useDevServer":"..."}`

```js
await fb2k.invoke('window.setDevServerConfig', {
    useDevServer: true,
    devServerUrl: 'http://localhost:5173',
});
```

### window.setDragRegions


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `array` | No | Optional; omitted by default. |

**Returns**: `{"count":"...","dpiScale":"...","error":"...","success":true}`

```js
await fb2k.invoke('window.setDragRegions', {
    regions: [{ x: 0, y: 0, width: 800, height: 32 }],
});
```

### window.setFrameless


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `frameless` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","frameless":"...","success":true}`

```js
await fb2k.invoke('window.setFrameless', { frameless: true });
```

### window.setFullscreen


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","fullscreen":"...","success":true}`

```js
await fb2k.invoke('window.setFullscreen', { enabled: true });
```

### window.setMaxSize

Sets a window's maximum size. Resolution selects the explicit `windowId` or the calling window; it never falls back to the main window, so a call from an unresolvable context fails rather than resizing an unintended window. Panel (DUI/CUI) callers are rejected with `panelMode: true`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window. Defaults to the calling window. |
| `height` | `integer` | No | Optional; default 0. |
| `width` | `integer` | No | Optional; default 0. |

**Returns**: `{"error":"...","success":true,"windowId":"..."}`

Values are physical pixels, converted to the host's DIP storage using the target window's DPI; `0` (or negative) clears the bound. Applying a constraint re-validates the current window size immediately, so a window already larger than the new maximum is shrunk rather than waiting for the next user resize.

```js
await fb2k.invoke('window.setMaxSize', { width: 1920, height: 1080 });
```

### window.setMica

Applies or clears the Mica backdrop. Not supported in panel mode.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |
| `variant` | `string` | No | `mica` (default) or `mica-alt`. |
| `darkMode` | `boolean` | No | Optional; leaves the existing mode when omitted. |

**Returns**: `{ "success": true, "enabled": true, "variant": "mica" }`, plus `darkMode` echoed back only when you supplied it.

Only `mica-alt` selects the alternate variant; every other value — including an unrecognized one — is normalized to `mica` rather than rejected. The returned `variant` echoes that normalized request, not the effect that ended up on screen: popups do not support Mica Alt and are downgraded, so `variant: "mica-alt"` can come back for a window that received a different backdrop. `success` reports whether the backdrop was actually applied and can be `false` for a valid window when the platform refuses the effect.

```js
await fb2k.invoke('window.setMica', { enabled: true, variant: 'mica-alt' });
```

### window.setMicaEffect

Compatibility alias of [`window.setMica`](#window-setmica). Same parameters, same behavior, same return shape — prefer `window.setMica` in new code. Because both share one implementation, the panel-mode rejection names `window.setMica` even when you called this alias.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |
| `variant` | `string` | No | `mica` (default) or `mica-alt`. |
| `darkMode` | `boolean` | No | Optional; leaves the existing mode when omitted. |

**Returns**: `{ "success": true, "enabled": true, "variant": "mica" }`, plus `darkMode` echoed back only when you supplied it.

```js
await fb2k.invoke('window.setMicaEffect', { enabled: true });
```

### window.setMinSize

Sets a window's minimum size. Resolution selects the explicit `windowId` or the calling window; it never falls back to the main window, so a call from an unresolvable context fails rather than resizing an unintended window. Panel (DUI/CUI) callers are rejected with `panelMode: true`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window. Defaults to the calling window. |
| `height` | `integer` | No | Optional; default 0. |
| `width` | `integer` | No | Optional; default 0. |

**Returns**: `{"error":"...","success":true,"windowId":"..."}`

Values are physical pixels, converted to the host's DIP storage using the target window's DPI, which keeps the constraint stable across DPI changes. Non-positive values normalise to a 1px floor. Applying a constraint re-validates the current window size immediately, so a window already smaller than the new minimum is grown rather than waiting for the next user resize.

```js
await fb2k.invoke('window.setMinSize', { width: 480, height: 320 });
```

### window.setNoDragRegions


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `array` | No | Optional; omitted by default. |

**Returns**: `{"count":"...","dpiScale":"...","error":"...","success":true}`

```js
await fb2k.invoke('window.setNoDragRegions', {
    regions: [{ x: 690, y: 0, width: 110, height: 32 }],
});
```

### window.setPopupBehavior

Updates a popup's behavior policy at runtime. Popups only — the main window is not a valid target.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target popup id. Omit to use the calling popup. |
| `profile` | `string` | No | `standard`, `miniPlayer`, or `desktopLyrics`. Leaves the profile unchanged when omitted. |
| `behavior` | `object` | No | Field-level overrides. Applied only when supplied. |

**Returns**: `{ "success": true, "windowId": "...", "profile": "...", "behavior": { ... }, "resolvedBehavior": { ... } }`

`resolvedBehavior` is the effective policy after the profile defaults and your overrides are merged. `profile` and `behavior` are independent: supplying one does not reset the other, and a `null` value inside `behavior` erases that override rather than storing null.

Profile matching is case-insensitive and also accepts hyphen and underscore spellings, so `miniPlayer`, `miniplayer`, `mini-player`, and `mini_player` are equivalent. The returned `profile` is always one of the three canonical names.

Passing `windowId: "main"` fails with `window.setPopupBehavior does not support main window`. Omitting `windowId` requires the caller to itself be a popup, otherwise the call fails with `Window not found`.

```js
// switch profile only
await fb2k.invoke('window.setPopupBehavior', { profile: 'miniPlayer' });
// field-level override, keeping the current profile
await fb2k.invoke('window.setPopupBehavior', { behavior: { closeOnFocusLoss: true } });
```

### window.setPosition


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `integer` | No | Optional; default 0. |
| `y` | `integer` | No | Optional; default 0. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('window.setPosition', { x: 100, y: 100 });
```

### window.setResizable

Sets whether a window can be resized by the user. Resolution selects the explicit `windowId` or the calling window; it never falls back to the main window. Panel (DUI/CUI) callers are rejected with `panelMode: true`. Setting the value a window already has succeeds — idempotent calls are not failures.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window. Defaults to the calling window. |
| `resizable` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","success":true,"windowId":"..."}`

Every window shell supports this at runtime, including fully borderless popups (`frame: false` plus `transparent: true` with no backdrop effect): those windows collapse their entire non-client area, so adding a sizing border changes hit-testing without altering appearance. `success: false` therefore indicates a genuine Win32 failure — the style could not be written, or the frame could not be refreshed — not an unsupported window shape; the requested state is not committed in that case.

::: warning Behavior change
This call previously always targeted the main window regardless of caller, so invoking it from a popup reconfigured the main window instead.
:::

```js
await fb2k.invoke('window.setResizable', { resizable: false });
```

### window.setSize


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `height` | `integer` | No | Optional; default 600. |
| `width` | `integer` | No | Optional; default 800. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('window.setSize', { width: 1024, height: 640 });
```

### window.setTitle


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Optional; default foobar2000. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.setTitle', { title: 'Now Playing' });
```

### window.setTitlebarHeight


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `height` | `integer` | No | Optional; default 32. |

**Returns**: `{"error":"...","height":"...","success":true}`

```js
await fb2k.invoke('window.setTitlebarHeight', { height: 40 });
```

### window.setZoom


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `zoom` | `number` | No | Optional; default 1. |

**Returns**: `{"error":"...","success":true,"zoom":"..."}`

```js
await fb2k.invoke('window.setZoom', { zoom: 1.25 });
```

### window.setZoomForDpi


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `dpi` | `integer` | No | Optional; default 0. |

**Returns**: `{"dpi":"...","error":"...","success":true,"zoom":"..."}`

```js
// omit dpi to derive the zoom from the calling window's current DPI
const { zoom } = await fb2k.invoke('window.setZoomForDpi');
```

### window.showSystemMenu


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `h` | `integer` | No | Optional; default 0. |
| `w` | `integer` | No | Optional; default 0. |
| `x` | `integer` | No | Optional; default 0. |
| `y` | `integer` | No | Optional; default 0. |

**Returns**: `{"error":"...","success":true}`

```js
// pass w/h to keep the menu clear of the button that opened it
await fb2k.invoke('window.showSystemMenu', { x: 8, y: 0, w: 32, h: 32 });
```

### window.startDrag


_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.startDrag');
```

### window.startResize


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `edge` | `string` | No | Optional; default bottomright. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.startResize', { edge: 'bottomright' });
```

### window.toggleAlwaysOnTop


_No parameters._

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleAlwaysOnTop');
```

### window.toggleFullscreen


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |

**Returns**: `{"error":"...","fullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleFullscreen');
```

### window.toggleMaximize


_No parameters._

**Returns**: `{"maximized":"...","success":true}`

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
