# Window API

English API reference for the `window` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## window

### window.blur

Public API method. Runtime authority: `src/api/WindowApi.cpp:2411`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.blur');
```

### window.broadcast


<!-- phase3-major1-review:window.broadcast -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:2245-2256`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `message` | `json` | Yes | none |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: message must be present but may be any JSON value. The handler identifies the caller window, broadcasts to the other managed windows, and returns no recipient count.

<!-- phase3-major1-review-end:window.broadcast -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2468`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | Yes | Required. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.broadcast', { message: /* value */ });
```

### window.cancelClose

Public API method. Runtime authority: `src/api/WindowApi.cpp:2460`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.cancelClose');
```

### window.center

Public API method. Runtime authority: `src/api/WindowApi.cpp:2401`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.center');
```

### window.clearClickThroughExcludeRegions

Public API method. Runtime authority: `src/api/WindowApi.cpp:2466`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.clearClickThroughExcludeRegions', { windowId: /* value */ });
```

### window.clearDragRegions

Public API method. Runtime authority: `src/api/WindowApi.cpp:2420`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.clearDragRegions');
```

### window.clearNoDragRegions

Public API method. Runtime authority: `src/api/WindowApi.cpp:2422`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.clearNoDragRegions');
```

### window.close

Public API method. Runtime authority: `src/api/WindowApi.cpp:2388`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('window.close');
```

### window.closeAllPopups

Public API method. Runtime authority: `src/api/WindowApi.cpp:2453`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('window.closeAllPopups');
```

### window.closePopup

Public API method. Runtime authority: `src/api/WindowApi.cpp:2452`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.closePopup', { windowId: /* value */ });
```

### window.confirmClose

Public API method. Runtime authority: `src/api/WindowApi.cpp:2461`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.confirmClose');
```

### window.createPopup

Public API method. Runtime authority: `src/api/WindowApi.cpp:2451`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `alwaysOnTop` | `boolean` | No | Optional; default false. |
| `backdropPolicy` | `object` | No | Optional; default omitted. |
| `beforeClose` | `boolean` | No | Optional; default false. |
| `behavior` | `object` | No | Optional; default omitted. |
| `clickThrough` | `boolean` | No | Optional; default false. |
| `frame` | `boolean` | No | Optional; default true. |
| `height` | `integer` | No | Optional; default 300. |
| `maxHeight` | `integer` | No | Optional; default 0. |
| `maxWidth` | `integer` | No | Optional; default 0. |
| `minHeight` | `integer` | No | Optional; default 150. |
| `minWidth` | `integer` | No | Optional; default 200. |
| `profile` | `string` | No | Optional; default . |
| `resizable` | `boolean` | No | Optional; default true. |
| `showInTaskbar` | `boolean` | No | Optional; default false. |
| `title` | `string` | No | Optional; default . |
| `transparent` | `boolean` | No | Optional; default false. |
| `url` | `string` | No | Optional; default . |
| `width` | `integer` | No | Optional; default 400. |
| `x` | `integer` | No | Optional; default CW_USEDEFAULT. |
| `y` | `integer` | No | Optional; default CW_USEDEFAULT. |

**Returns**: `{"error":"...","success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.createPopup', { alwaysOnTop: /* value */, backdropPolicy: /* value */, beforeClose: /* value */, behavior: /* value */, clickThrough: /* value */, frame: /* value */, height: /* value */, maxHeight: /* value */, maxWidth: /* value */, minHeight: /* value */, minWidth: /* value */, profile: /* value */, resizable: /* value */, showInTaskbar: /* value */, title: /* value */, transparent: /* value */, url: /* value */, width: /* value */, x: /* value */, y: /* value */ });
```

### window.enterFullscreen


<!-- phase3-major1-review:window.enterFullscreen -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1538-1554`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |

**Return keys (vary by response variant)**: `error`, `isFullscreen`, `success`; `success`; `isFullscreen`, `success`

**Semantics**: The resolver targets windowId when supplied or the caller otherwise. Panel mode and shells without fullscreen capability return errors; already-fullscreen calls return success:false without changing state.

<!-- phase3-major1-review-end:window.enterFullscreen -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2434`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |

**Returns**: `{"error":"...","isFullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.enterFullscreen');
```

### window.exitFullscreen


<!-- phase3-major1-review:window.exitFullscreen -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1554-1570`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |

**Return keys (vary by response variant)**: `error`, `isFullscreen`, `success`; `success`; `isFullscreen`, `success`

**Semantics**: The resolver targets windowId when supplied or the caller otherwise. Panel mode and unsupported shells fail; a non-fullscreen target returns success:false without restoring geometry.

<!-- phase3-major1-review-end:window.exitFullscreen -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2435`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |

**Returns**: `{"error":"...","isFullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.exitFullscreen');
```

### window.flash

Public API method. Runtime authority: `src/api/WindowApi.cpp:2414`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `integer` | No | Optional; default 3. |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.flash', { count: /* value */, enabled: /* value */ });
```

### window.flashTaskbar

Public API method. Runtime authority: `src/api/WindowApi.cpp:2433`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `integer` | No | Optional; default 3. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('window.flashTaskbar', { count: /* value */ });
```

### window.focus

Public API method. Runtime authority: `src/api/WindowApi.cpp:2410`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.focus', { windowId: /* value */ });
```

### window.getAllWindows

Public API method. Runtime authority: `src/api/WindowApi.cpp:2454`.

_No parameters._

**Returns**: `{"items":"...","success":true}`

```js
const result = await fb2k.invoke('window.getAllWindows');
```

### window.getBackdropPolicy


<!-- phase3-major1-review:window.getBackdropPolicy -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:2014-2023`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |

**Return keys (vary by response variant)**: `error`, `success`; `backdropPolicy`, `resolvedBackdropPolicy`, `success`, `windowId`

**Semantics**: Observation resolution selects the explicit windowId or caller. The response is the shell’s backdrop-policy object augmented with success and resolved windowId, including profile/default-resolved policy fields.

<!-- phase3-major1-review-end:window.getBackdropPolicy -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2458`.

_No parameters._

**Returns**: JSON object from the runtime handler.

```js
const result = await fb2k.invoke('window.getBackdropPolicy');
```

### window.getBounds

Public API method. Runtime authority: `src/api/WindowApi.cpp:2399`.

_No parameters._

**Returns**: `{"height":"...","width":"...","x":"...","y":"..."}`

```js
const result = await fb2k.invoke('window.getBounds');
```

### window.getCaptionButtonsWidth


<!-- phase3-major1-review:window.getCaptionButtonsWidth -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1203-1214`.

_No parameters._ This call is **main-window only**: it does not accept `windowId` and ignores the calling window.

**Return keys**: `buttonWidth`, `width`

**Semantics**: Reports the geometry of the main window's custom-drawn caption buttons (minimise / maximise / close), used by frontends that render their own titlebar. Popups have no custom-drawn caption buttons, so there is no popup-scoped value to report and passing `windowId` has no effect. Values are physical pixels and track the main window's DPI. When no main window exists the call returns DIP-baseline defaults (`width: 138`, `buttonWidth: 46`) rather than an error, so callers cannot distinguish "no window" from a genuine 100%-scale measurement.

<!-- phase3-major1-review-end:window.getCaptionButtonsWidth -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2418`.

_No parameters._

**Returns**: `{"buttonWidth":"...","width":"..."}`

```js
const result = await fb2k.invoke('window.getCaptionButtonsWidth');
```

### window.getCornerPreference


<!-- phase3-major1-review:window.getCornerPreference -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1489-1495`.

_No parameters._ This call is **main-window only**: it does not accept `windowId` and ignores the calling window.

**Return keys**: `mode`, `preference`

**Semantics**: Returns the main window's Windows 11 corner-rounding preference; `mode` and `preference` are the same value under two names. Popups do not expose this setting — they manage corner rounding internally (rounded when borderless, system default otherwise) and report `supportsCornerPreference: false` in their capabilities, so this call has no popup-scoped equivalent. When no main window exists the call returns `"default"` rather than an error.

<!-- phase3-major1-review-end:window.getCornerPreference -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2428`.

_No parameters._

**Returns**: `{"mode":"...","preference":"..."}`

```js
const result = await fb2k.invoke('window.getCornerPreference');
```

### window.getCurrentWindowId

Public API method. Runtime authority: `src/api/WindowApi.cpp:2455`.

_No parameters._

**Returns**: `{"success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.getCurrentWindowId');
```

### window.getDevServerConfig

Public API method. Runtime authority: `src/api/WindowApi.cpp:2444`.

_No parameters._

**Returns**: `{"devServerUrl":"...","success":true,"useDevServer":"..."}`

```js
const result = await fb2k.invoke('window.getDevServerConfig');
```

### window.getDpiScale

Public API method. Runtime authority: `src/api/WindowApi.cpp:2432`.

_No parameters._

**Returns**: `{"dpi":"...","scale":"...","success":true}`

```js
const result = await fb2k.invoke('window.getDpiScale');
```

### window.getMaxSize


<!-- phase3-major1-review:window.getMaxSize -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:884-898`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |

**Return keys (vary by response variant)**: `error`, `success`; `height`, `width`, `windowId`

**Semantics**: Observation resolution selects the explicit `windowId` or the calling window; it does **not** fall back to the main window, so a call from an unresolvable context fails instead of silently reporting another window's constraints. Panel (DUI/CUI) callers are rejected with `panelMode: true` because a panel is not a window shell. Values are physical pixels; the host stores constraints in DIPs and converts using the target window's DPI. `0` means "no upper bound" and survives the conversion exactly.

The returned values are the **requested** constraints, not the currently effective window size. Note that a physical → DIP → physical round-trip is quantized to whole DIPs, so at non-100% scaling `get` may differ from the value passed to `set` by up to 1px per axis (for example, `202px` at 125% reads back as `203px`). Treat the getters as reporting the constraint you set to within ±1px rather than byte-for-byte. `0` is exempt.

<!-- phase3-major1-review-end:window.getMaxSize -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2405`.

_No parameters._

**Returns**: `{"height":"...","width":"..."}`

```js
const result = await fb2k.invoke('window.getMaxSize');
```

### window.getMinSize


<!-- phase3-major1-review:window.getMinSize -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:848-864`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |

**Return keys (vary by response variant)**: `error`, `success`; `height`, `width`, `windowId`

**Semantics**: Observation resolution selects the explicit `windowId` or the calling window; it does **not** fall back to the main window, so a call from an unresolvable context fails instead of silently reporting another window's constraints. Panel (DUI/CUI) callers are rejected with `panelMode: true` because a panel is not a window shell. Values are physical pixels; the host stores constraints in DIPs and converts using the target window's DPI.

The returned values are the **requested** constraints, not the currently effective window size. Note that a physical → DIP → physical round-trip is quantized to whole DIPs, so at non-100% scaling `get` may differ from the value passed to `set` by up to 1px per axis (for example, `202px` at 125% reads back as `203px`). Treat the getters as reporting the constraint you set to within ±1px rather than byte-for-byte.

<!-- phase3-major1-review-end:window.getMinSize -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2403`.

_No parameters._

**Returns**: `{"height":"...","width":"..."}`

```js
const result = await fb2k.invoke('window.getMinSize');
```

### window.getMode

Public API method. Runtime authority: `src/api/WindowApi.cpp:2469`.

_No parameters._

**Returns**: `{"mode":"...","panel":"...","panelMode":"...","windowId":"..."}`

```js
const result = await fb2k.invoke('window.getMode');
```

### window.getPopupBehavior

Public API method. Runtime authority: `src/api/WindowApi.cpp:2456`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default . |

**Returns**: `{"success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.getPopupBehavior', { windowId: /* value */ });
```

### window.getState

Public API method. Runtime authority: `src/api/WindowApi.cpp:2393`.

_No parameters._

**Returns**: `{"alwaysOnTop":"...","focused":"...","fullscreen":"...","height":"...","isAlwaysOnTop":"...","isFocused":"...","isFullscreen":"...","isMaximized":"...","isMinimized":"...","maximized":"...","minimized":"...","width":"...","x":"...","y":"..."}`

```js
const result = await fb2k.invoke('window.getState');
```

### window.getTitle

Public API method. Runtime authority: `src/api/WindowApi.cpp:2413`.

_No parameters._

**Returns**: `{"title":"..."}`

```js
const result = await fb2k.invoke('window.getTitle');
```

### window.getTitlebarHeight

Public API method. Runtime authority: `src/api/WindowApi.cpp:2416`.

_No parameters._

**Returns**: `{"height":"..."}`

```js
const result = await fb2k.invoke('window.getTitlebarHeight');
```

### window.getTitlebarInfo


<!-- phase3-major1-review:window.getTitlebarInfo -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1335-1355`.

_No parameters._ This call is **main-window only**: it does not accept `windowId` and ignores the calling window.

**Return keys**: `captionButtonWidth`, `captionButtonsWidth`, `height`, `isMaximized`

**Semantics**: Bundles the main window's titlebar height with its custom-drawn caption-button geometry and maximised state. Three of the four fields have no popup equivalent (popups expose only a titlebar height and have no custom-drawn caption buttons), so the call stays main-scoped rather than returning zeros for the missing fields. Values are physical pixels and track the main window's DPI. When no main window exists the call returns DIP-baseline defaults (`height: 32`, `captionButtonsWidth: 138`, `captionButtonWidth: 46`) rather than an error; those fallbacks are unscaled, so they differ in unit from the normal path on non-100% displays.

<!-- phase3-major1-review-end:window.getTitlebarInfo -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2423`.

_No parameters._

**Returns**: `{"captionButtonWidth":"...","captionButtonsWidth":"...","height":"...","isMaximized":"..."}`

```js
const result = await fb2k.invoke('window.getTitlebarInfo');
```

### window.getZoom

Public API method. Runtime authority: `src/api/WindowApi.cpp:2448`.

_No parameters._

**Returns**: `{"dpi":"...","dpiScale":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.getZoom');
```

### window.hasSavedBounds

Public API method. Runtime authority: `src/api/WindowApi.cpp:2446`.

_No parameters._

**Returns**: `{"Window has saved position from previous session":"...","description":"...","hasSavedBounds":"..."}`

```js
const result = await fb2k.invoke('window.hasSavedBounds');
```

### window.isAlwaysOnTop

Public API method. Runtime authority: `src/api/WindowApi.cpp:2397`.

_No parameters._

**Returns**: `{"enabled":"...","isAlwaysOnTop":"..."}`

```js
const result = await fb2k.invoke('window.isAlwaysOnTop');
```

### window.isClickThrough

Public API method. Runtime authority: `src/api/WindowApi.cpp:2464`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default . |

**Returns**: `{"clickThrough":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('window.isClickThrough', { windowId: /* value */ });
```

### window.isFullscreen

Public API method. Runtime authority: `src/api/WindowApi.cpp:2392`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional target window id resolved by `WindowTargetResolver::ResolveForObservation`; omitted uses the caller window. |

**Returns**: `{"fullscreen":"...","isFullscreen":"..."}`

```js
const result = await fb2k.invoke('window.isFullscreen', { windowId: /* value */ });
```

### window.isMaximized

Public API method. Runtime authority: `src/api/WindowApi.cpp:2390`.

_No parameters._

**Returns**: `{"isMaximized":"...","maximized":"..."}`

```js
const result = await fb2k.invoke('window.isMaximized');
```

### window.isMinimized

Public API method. Runtime authority: `src/api/WindowApi.cpp:2391`.

_No parameters._

**Returns**: `{"minimized":"..."}`

```js
const result = await fb2k.invoke('window.isMinimized');
```

### window.isResizable


<!-- phase3-major1-review:window.isResizable -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:924-933`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |

**Return keys (vary by response variant)**: `error`, `success`; `resizable`, `windowId`

**Semantics**: Observation resolution selects the explicit `windowId` or the calling window; it does **not** fall back to the main window. Panel (DUI/CUI) callers are rejected with `panelMode: true`. Reports the requested resizable state; every window shell — including fully borderless popups — supports changing it at runtime via {@link window.setResizable}.

<!-- phase3-major1-review-end:window.isResizable -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2407`.

_No parameters._

**Returns**: `{"resizable":"..."}`

```js
const result = await fb2k.invoke('window.isResizable');
```

### window.maximize

Public API method. Runtime authority: `src/api/WindowApi.cpp:2386`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.maximize');
```

### window.minimize

Public API method. Runtime authority: `src/api/WindowApi.cpp:2385`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.minimize');
```

### window.refreshWebView

Public API method. Runtime authority: `src/api/WindowApi.cpp:2442`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.refreshWebView');
```

### window.reload

Public API method. Runtime authority: `src/api/WindowApi.cpp:2443`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.reload');
```

### window.resetZoom

Public API method. Runtime authority: `src/api/WindowApi.cpp:2449`.

_No parameters._

**Returns**: `{"error":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.resetZoom');
```

### window.restore

Public API method. Runtime authority: `src/api/WindowApi.cpp:2387`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.restore');
```

### window.sendMessage


<!-- phase3-major1-review:window.sendMessage -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:2223-2242`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `targetWindowId` | `string` | Yes | none |
| `message` | `json` | Yes | none |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `error`, `success`; `success`

**Semantics**: Both a non-empty targetWindowId and present message are required. The message is forwarded as arbitrary JSON from the caller identity; a missing recipient returns the target-not-found error.

<!-- phase3-major1-review-end:window.sendMessage -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2467`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `targetWindowId` | `string` | Yes | Required. |
| `message` | `json` | Yes | Required. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.sendMessage', { message: /* value */, targetWindowId: /* value */ });
```

### window.setAcrylic


<!-- phase3-major1-review:window.setAcrylic -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1593-1617`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |
| `enabled` | `boolean` | No | `true` |
| `darkMode` | `boolean` | No | `leave existing mode` |

**Returns**: `{"darkMode":"...","enabled":true,"success":true}`

**Semantics**: The mutation resolver chooses explicit windowId or caller. enabled patches compatibility backdrop to acrylic or clears it; darkMode is only patched when supplied, and panel mode is unsupported.

<!-- phase3-major1-review-end:window.setAcrylic -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2439`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |
| `darkMode` | `boolean` | No | Optional; default leave existing mode. |


```js
const result = await fb2k.invoke('window.setAcrylic', { darkMode: /* value */, enabled: /* value */ });
```

### window.setAlwaysOnTop

Public API method. Runtime authority: `src/api/WindowApi.cpp:2396`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setAlwaysOnTop', { enabled: /* value */ });
```

### window.setBackdropPolicy


<!-- phase3-major1-review:window.setBackdropPolicy -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:2026-2047`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |
| `backdropPolicy` | `object` | Yes | none |

**Return keys (vary by response variant)**: `error`, `success`; `backdropPolicy`, `resolvedBackdropPolicy`, `success`, `windowId`

**Semantics**: backdropPolicy must be present and an object before target resolution. The shell validates/applies the patch; the successful response returns the updated policy info plus success and windowId.

<!-- phase3-major1-review-end:window.setBackdropPolicy -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2459`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `backdropPolicy` | `object` | Yes | Required. |

**Returns**: `{"error":"...","failed to update backdrop policy":"...","success":true}`

```js
const result = await fb2k.invoke('window.setBackdropPolicy', { backdropPolicy: /* value */ });
```

### window.setBackgroundTransparency

Public API method. Runtime authority: `src/api/WindowApi.cpp:2441`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `transparent` | `boolean` | No | Optional; default true. |

**Returns**: `{"WebView background is now transparent - DWM effects will show through":"...","description":"...","error":"...","success":true,"transparent":"..."}`

```js
const result = await fb2k.invoke('window.setBackgroundTransparency', { transparent: /* value */, windowId: /* value */ });
```

### window.setBlur

Public API method. Runtime authority: `src/api/WindowApi.cpp:2438`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('window.setBlur', { enabled: /* value */, windowId: /* value */ });
```

### window.setBounds


<!-- phase3-major1-review:window.setBounds -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:761-789`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `x` | `integer` | No | `current x` |
| `y` | `integer` | No | `current y` |
| `width` | `integer` | No | `current width` |
| `height` | `integer` | No | `current height` |

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: Only supplied fields are read as integers; omitted coordinates and dimensions retain the current window rectangle. The method is unsupported in panel mode and operates only on the caller window.

<!-- phase3-major1-review-end:window.setBounds -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2400`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `integer` | No | Optional; default current x. |
| `y` | `integer` | No | Optional; default current y. |
| `width` | `integer` | No | Optional; default current width. |
| `height` | `integer` | No | Optional; default current height. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setBounds', { height: /* value */, width: /* value */, x: /* value */, y: /* value */ });
```

### window.setClickThrough

Public API method. Runtime authority: `src/api/WindowApi.cpp:2463`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default true. |
| `windowId` | `string` | No | Optional; default . |

**Returns**: `{"clickThrough":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setClickThrough', { enabled: /* value */, windowId: /* value */ });
```

### window.setClickThroughExcludeRegions

Public API method. Runtime authority: `src/api/WindowApi.cpp:2465`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `array` | No | Optional; default omitted. |
| `windowId` | `string` | No | Optional; default . |

**Returns**: `{"count":0,"dpiScale":"...","success":true,"warning":"...","windowId":"..."}`

```js
const result = await fb2k.invoke('window.setClickThroughExcludeRegions', { regions: /* value */, windowId: /* value */ });
```

### window.setCornerPreference


<!-- phase3-major1-review:window.setCornerPreference -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1477-1486`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `mode` | `string` | No | `"default"` |

This call is **main-window only**: it does not accept `windowId` and ignores the calling window.

**Return keys (vary by response variant)**: `error`, `success`; `success`

**Semantics**: Sets the main window's Windows 11 corner-rounding preference. Accepted values are `"default"`, `"none"`, `"round"` and `"small"`; `"default"` maps to rounded corners because a borderless window has no standard non-client frame for the system default to apply to. Popups do not accept this setting — they manage corner rounding internally and report `supportsCornerPreference: false` — so the call has no popup-scoped form. Panel (DUI/CUI) callers are rejected with `panelMode: true`.

<!-- phase3-major1-review-end:window.setCornerPreference -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2427`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | Optional; default default. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setCornerPreference', { mode: /* value */ });
```

### window.setDarkMode

Public API method. Runtime authority: `src/api/WindowApi.cpp:2440`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('window.setDarkMode', { enabled: /* value */, windowId: /* value */ });
```

### window.setDevServerConfig

Public API method. Runtime authority: `src/api/WindowApi.cpp:2445`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `devServerUrl` | `string` | No | Optional; default . |
| `useDevServer` | `boolean` | No | Optional; default false. |

**Returns**: `{"devServerUrl":"...","success":true,"useDevServer":"..."}`

```js
const result = await fb2k.invoke('window.setDevServerConfig', { devServerUrl: /* value */, useDevServer: /* value */ });
```

### window.setDragRegions

Public API method. Runtime authority: `src/api/WindowApi.cpp:2419`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `array` | No | Optional; default omitted. |

**Returns**: `{"count":"...","dpiScale":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setDragRegions', { regions: /* value */ });
```

### window.setFrameless

Public API method. Runtime authority: `src/api/WindowApi.cpp:2462`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `frameless` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","frameless":"...","success":true}`

```js
const result = await fb2k.invoke('window.setFrameless', { frameless: /* value */, windowId: /* value */ });
```

### window.setFullscreen


<!-- phase3-major1-review:window.setFullscreen -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:899-921`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |
| `enabled` | `boolean` | No | `true` |

**Return keys (vary by response variant)**: `error`, `fullscreen`, `success`; `fullscreen`, `success`

**Semantics**: The mutation resolver chooses explicit windowId or caller. enabled enters and false exits fullscreen only on shells advertising fullscreen capability; panel mode is rejected.

<!-- phase3-major1-review-end:window.setFullscreen -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2408`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","fullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.setFullscreen', { enabled: /* value */ });
```

### window.setMaxSize


<!-- phase3-major1-review:window.setMaxSize -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:867-881`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |
| `width` | `integer` | No | `0` |
| `height` | `integer` | No | `0` |

**Return keys (vary by response variant)**: `error`, `success`; `success`, `windowId`

**Semantics**: Mutation resolution selects the explicit `windowId` or the calling window; it never falls back to the main window, so a call from an unresolvable context fails rather than resizing an unintended window. Panel (DUI/CUI) callers are rejected with `panelMode: true`. Values are physical pixels, converted to the host's DIP storage using the target window's DPI; `0` (or negative) clears the bound. Applying a constraint re-validates the current window size immediately, so a window already larger than the new maximum is shrunk rather than waiting for the next user resize.

<!-- phase3-major1-review-end:window.setMaxSize -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2404`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `height` | `integer` | No | Optional; default 0. |
| `width` | `integer` | No | Optional; default 0. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setMaxSize', { height: /* value */, width: /* value */ });
```

### window.setMica


<!-- phase3-major1-review:window.setMica -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1573-1576`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |
| `enabled` | `boolean` | No | `true` |
| `variant` | `string` | No | `mica` |
| `darkMode` | `boolean` | No | `leave existing mode` |

**Returns**: `{"darkMode":"...","enabled":true,"success":true,"variant":"..."}`

**Semantics**: setMica delegates to SetMicaEffectImpl: only mica-alt selects the alternate variant, all other values normalize to mica. It patches the resolved caller/target shell and is unsupported in panel mode.

<!-- phase3-major1-review-end:window.setMica -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2436`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |
| `variant` | `string` | No | Optional; default mica. |
| `darkMode` | `boolean` | No | Optional; default leave existing mode. |


```js
const result = await fb2k.invoke('window.setMica', { enabled: true, variant: 'mica' });
```

### window.setMicaEffect


<!-- phase3-major1-review:window.setMicaEffect -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1579-1582`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |
| `enabled` | `boolean` | No | `true` |
| `variant` | `string` | No | `mica` |
| `darkMode` | `boolean` | No | `leave existing mode` |

**Returns**: `{"darkMode":"...","enabled":true,"success":true,"variant":"..."}`

**Semantics**: setMicaEffect is a direct compatibility alias of setMica and delegates to the same Mica implementation, including variant normalization, target resolution, and panel restriction.

<!-- phase3-major1-review-end:window.setMicaEffect -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2437`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |
| `enabled` | `boolean` | No | Optional; default true. |
| `variant` | `string` | No | Optional; default mica. |
| `darkMode` | `boolean` | No | Optional; default leave existing mode. |


```js
const result = await fb2k.invoke('window.setMicaEffect', { enabled: true, variant: 'mica' });
```

### window.setMinSize


<!-- phase3-major1-review:window.setMinSize -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:831-845`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |
| `width` | `integer` | No | `0` |
| `height` | `integer` | No | `0` |

**Return keys (vary by response variant)**: `error`, `success`; `success`, `windowId`

**Semantics**: Mutation resolution selects the explicit `windowId` or the calling window; it never falls back to the main window, so a call from an unresolvable context fails rather than resizing an unintended window. Panel (DUI/CUI) callers are rejected with `panelMode: true`. Values are physical pixels, converted to the host's DIP storage using the target window's DPI, which keeps the constraint stable across DPI changes. Non-positive values normalise to a 1px floor. Applying a constraint re-validates the current window size immediately, so a window already smaller than the new minimum is grown rather than waiting for the next user resize.

<!-- phase3-major1-review-end:window.setMinSize -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2402`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `height` | `integer` | No | Optional; default 0. |
| `width` | `integer` | No | Optional; default 0. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setMinSize', { height: /* value */, width: /* value */ });
```

### window.setNoDragRegions

Public API method. Runtime authority: `src/api/WindowApi.cpp:2421`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `array` | No | Optional; default omitted. |

**Returns**: `{"count":"...","dpiScale":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setNoDragRegions', { regions: /* value */ });
```

### window.setPopupBehavior


<!-- phase3-major1-review:window.setPopupBehavior -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:1978-2011`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller popup` |
| `profile` | `string` | No | `leave profile unchanged` |
| `behavior` | `object` | No | `no behavior patch` |

**Returns**: `{"error":"...","success":true,"windowId":"..."}`

**Semantics**: Explicit windowId must name a popup rather than main; without it the caller must resolve to a popup. profile and behavior are independently presence-sensitive and are validated by UpdatePopupBehavior.

<!-- phase3-major1-review-end:window.setPopupBehavior -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2457`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller popup. |
| `profile` | `string` | No | Optional; default leave profile unchanged. |
| `behavior` | `object` | No | Optional; default no behavior patch. |


```js
const result = await fb2k.invoke('window.setPopupBehavior', { behavior: /* value */, profile: /* value */, windowId: /* value */ });
```

### window.setPosition

Public API method. Runtime authority: `src/api/WindowApi.cpp:2430`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `integer` | No | Optional; default 0. |
| `y` | `integer` | No | Optional; default 0. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('window.setPosition', { x: /* value */, y: /* value */ });
```

### window.setResizable


<!-- phase3-major1-review:window.setResizable -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:901-921`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |
| `resizable` | `boolean` | No | `true` |

**Return keys (vary by response variant)**: `error`, `success`; `success`, `windowId`

**Semantics**: Mutation resolution selects the explicit `windowId` or the calling window; it never falls back to the main window. Previously this call always targeted the main window regardless of caller, so invoking it from a popup reconfigured the main window instead. Panel (DUI/CUI) callers are rejected with `panelMode: true`. Setting the value a window already has succeeds — idempotent calls are not failures.

Every window shell supports this at runtime, including fully borderless popups (`frame: false` plus `transparent: true` with no backdrop effect): those windows collapse their entire non-client area, so adding a sizing border changes hit-testing without altering appearance. `success: false` therefore indicates a genuine Win32 failure (the style could not be written or the frame could not be refreshed), not an unsupported window shape; the requested state is not committed in that case.

<!-- phase3-major1-review-end:window.setResizable -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2406`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `resizable` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setResizable', { resizable: /* value */ });
```

### window.setSize

Public API method. Runtime authority: `src/api/WindowApi.cpp:2431`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `height` | `integer` | No | Optional; default 600. |
| `width` | `integer` | No | Optional; default 800. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('window.setSize', { height: /* value */, width: /* value */ });
```

### window.setTitle

Public API method. Runtime authority: `src/api/WindowApi.cpp:2412`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `title` | `string` | No | Optional; default foobar2000. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setTitle', { title: /* value */ });
```

### window.setTitlebarHeight

Public API method. Runtime authority: `src/api/WindowApi.cpp:2417`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `height` | `integer` | No | Optional; default 32. |

**Returns**: `{"error":"...","height":"...","success":true}`

```js
const result = await fb2k.invoke('window.setTitlebarHeight', { height: /* value */ });
```

### window.setZoom

Public API method. Runtime authority: `src/api/WindowApi.cpp:2447`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `zoom` | `number` | No | Optional; default 1. |

**Returns**: `{"error":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.setZoom', { zoom: /* value */ });
```

### window.setZoomForDpi

Public API method. Runtime authority: `src/api/WindowApi.cpp:2450`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `dpi` | `integer` | No | Optional; default 0. |

**Returns**: `{"dpi":"...","error":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.setZoomForDpi', { dpi: /* value */ });
```

### window.showSystemMenu

Public API method. Runtime authority: `src/api/WindowApi.cpp:2415`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `h` | `integer` | No | Optional; default 0. |
| `w` | `integer` | No | Optional; default 0. |
| `x` | `integer` | No | Optional; default 0. |
| `y` | `integer` | No | Optional; default 0. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.showSystemMenu', { h: /* value */, w: /* value */, x: /* value */, y: /* value */ });
```

### window.startDrag

Public API method. Runtime authority: `src/api/WindowApi.cpp:2394`.

_No parameters._

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.startDrag');
```

### window.startResize

Public API method. Runtime authority: `src/api/WindowApi.cpp:2395`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `edge` | `string` | No | Optional; default bottomright. |

**Returns**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.startResize', { edge: /* value */ });
```

### window.toggleAlwaysOnTop

Public API method. Runtime authority: `src/api/WindowApi.cpp:2398`.

_No parameters._

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleAlwaysOnTop');
```

### window.toggleFullscreen


<!-- phase3-major1-review:window.toggleFullscreen -->
#### Source-reviewed contract
Authority: `src/api/WindowApi.cpp:921-944`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` |

**Return keys (vary by response variant)**: `error`, `fullscreen`, `success`; `fullscreen`, `success`

**Semantics**: The mutation resolver chooses explicit windowId or caller. The method flips the shell fullscreen state only when capability is available; panel mode and unsupported targets return false/error.

<!-- phase3-major1-review-end:window.toggleFullscreen -->
Public API method. Runtime authority: `src/api/WindowApi.cpp:2409`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Optional; default caller window. |

**Returns**: `{"error":"...","fullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleFullscreen');
```

### window.toggleMaximize

Public API method. Runtime authority: `src/api/WindowApi.cpp:2389`.

_No parameters._

**Returns**: `{"maximized":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleMaximize');
```

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:window.focus -->
### Contract supplement: `window.focus`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:947-1009`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `` | Optional; default . |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.focus', { windowId: /* value */ });
```
<!-- phase3-supplement:window.getBackdropPolicy -->
### Contract supplement: `window.getBackdropPolicy`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:2014-2023`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `backdropPolicy` | `object` | No |
| `resolvedBackdropPolicy` | `object` | No |
| `windowId` | `string` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.getBackdropPolicy', { windowId: /* value */ });
```
<!-- phase3-supplement:window.isFullscreen -->
### Contract supplement: `window.isFullscreen`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:577-584`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `fullscreen` | `json` | No |
| `isFullscreen` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.isFullscreen', { windowId: /* value */ });
```
<!-- phase3-supplement:window.setAcrylic -->
### Contract supplement: `window.setAcrylic`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:1593-1617`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |
| `enabled` | `boolean` | No | `true` | Optional; default true. |
| `darkMode` | `boolean` | No | `leave existing mode` | Optional; default leave existing mode. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.setAcrylic', { windowId: /* value */, enabled: /* value */, darkMode: /* value */ });
```
<!-- phase3-supplement:window.setBackdropPolicy -->
### Contract supplement: `window.setBackdropPolicy`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:2026-2047`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |
| `backdropPolicy` | `object` | Yes | none | Required. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `backdropPolicy` | `object` | No |
| `resolvedBackdropPolicy` | `object` | No |
| `windowId` | `string` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.setBackdropPolicy', { windowId: /* value */, backdropPolicy: /* value */ });
```
<!-- phase3-supplement:window.setBackgroundTransparency -->
### Contract supplement: `window.setBackgroundTransparency`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:1632-1658`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |
| `transparent` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `description` | `json` | No |
| `transparent` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.setBackgroundTransparency', { windowId: /* value */, transparent: /* value */ });
```
<!-- phase3-supplement:window.setBlur -->
### Contract supplement: `window.setBlur`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:1582-1593`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |
| `enabled` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `enabled` | `json` | No |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.setBlur', { windowId: /* value */, enabled: /* value */ });
```
<!-- phase3-supplement:window.setDarkMode -->
### Contract supplement: `window.setDarkMode`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:1617-1627`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |
| `enabled` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `enabled` | `json` | No |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.setDarkMode', { windowId: /* value */, enabled: /* value */ });
```
<!-- phase3-supplement:window.setFrameless -->
### Contract supplement: `window.setFrameless`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:2088-2101`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |
| `frameless` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `frameless` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.setFrameless', { windowId: /* value */, frameless: /* value */ });
```
<!-- phase3-supplement:window.setFullscreen -->
### Contract supplement: `window.setFullscreen`

Verified contract supplement. Runtime authority: `src/api/WindowApi.cpp:899-921`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | No | `caller window` | Optional; default caller window. |
| `enabled` | `boolean` | No | `true` | Optional; default true. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `fullscreen` | `json` | No |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('window.setFullscreen', { windowId: /* value */, enabled: /* value */ });
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
