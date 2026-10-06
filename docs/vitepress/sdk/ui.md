# `fb.ui` window management

`fb.ui` is the SDK facade over the native `window.*` APIs plus `ui.showContextMenu`, and covers window management. Methods return promises and act on the calling window unless their entry says otherwise.

`isFullscreen`, `isResizable`, `getMinSize` / `setMinSize`, `getMaxSize` / `setMaxSize`, `setResizable`, `setDarkMode`, `setFrameless` and the four fullscreen methods take an optional trailing `windowId` that names `main` or a popup. Omitted, they act on the calling window.

## Basic window controls

### minimize()

Minimizes the window.

```javascript
await fb.ui.minimize();
```

### maximize()

Maximizes the window.

```javascript
await fb.ui.maximize();
```

### restore()

Restores the window from minimized or maximized.

```javascript
await fb.ui.restore();
```

### close()

Closes the window.

```javascript
await fb.ui.close();
```

### toggleMaximize()

Maximizes the window, or restores it when it is maximized. Resolves with `maximized`: whether the window is maximized once the request takes effect.

```javascript
const res = await fb.ui.toggleMaximize();
if (res.success === false) throw new Error(res.error);
const { maximized } = res;
```

### setTitle(title)

Sets the window title.

```javascript
await fb.ui.setTitle('Now Playing: Let It Be');
```

### reload()

Reloads the page. Useful while developing.

```javascript
await fb.ui.reload();
```

### startDrag()

Starts dragging the window, for a custom titlebar. Call it from a `mousedown` handler.

```javascript
document.getElementById('titlebar').addEventListener('mousedown', () => {
    fb.ui.startDrag();
});
```

### startResize(edge)

Starts resizing the window. `edge` is the edge or corner to drag. Like `startDrag`, call it from a `mousedown` handler.

Signature: `fb.ui.startResize(edge: string): Promise<WindowStartResizeResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `edge` | `string` | Yes | Edge or corner to drag: `left`, `right`, `top`, `bottom`, `topleft`, `topright`, `bottomleft` or `bottomright`; any other value drags the bottom-right corner |

```javascript
document.getElementById('resize-grip')?.addEventListener('mousedown', () => {
    fb.ui.startResize('bottomright');
});
```

## State queries

### getState()

Gets the full window state.

```javascript
const state = await fb.ui.getState();
// {maximized, minimized, fullscreen, alwaysOnTop, focused, width, height, x, y}
```

### isMaximized()

```javascript
const r = await fb.ui.isMaximized(); // {success: true, maximized: true, isMaximized: true}
```

### isMinimized()

```javascript
const r = await fb.ui.isMinimized(); // {success: true, minimized: false}
```

### isFullscreen()

```javascript
const r = await fb.ui.isFullscreen(); // {success: true, fullscreen: false, isFullscreen: false, windowId: 'main'}
```

### isAlwaysOnTop()

```javascript
const r = await fb.ui.isAlwaysOnTop(); // {success: true, enabled: false, isAlwaysOnTop: false}
```

### isResizable()

```javascript
const r = await fb.ui.isResizable(); // {success: true, resizable: true, windowId: 'main'}
```

### getTitle()

Gets the current window title.

Signature: `fb.ui.getTitle(): Promise<WindowGetTitleResponse>`

```javascript
const res = await fb.ui.getTitle();
if (res.success === false) throw new Error(res.error);
const { title } = res;
```

### getMode()

Gets how the calling page is hosted: `mode` is `standalone`, `dui`, `cui`, `panel` or `unknown`, and the response also carries `panelMode` and `windowId`.

Signature: `fb.ui.getMode(): Promise<WindowGetModeResponse>`

```javascript
const res = await fb.ui.getMode();
if (res.success === false) throw new Error(res.error);
const { mode } = res;
```

## Position and size

### setPosition(x, y)

Sets the window position.

```javascript
await fb.ui.setPosition(100, 100);
```

### setSize(width, height)

Sets the window size.

```javascript
await fb.ui.setSize(1280, 720);
```

### getBounds()

Gets the window bounds, `{x, y, width, height}`.

Signature: `fb.ui.getBounds(): Promise<WindowGetBoundsResponse>`

```javascript
const res = await fb.ui.getBounds();
if (res.success === false) throw new Error(res.error);
const { x, y, width, height } = res;
```

### setBounds(opts)

Sets the window position and size in one call. Only `x`, `y`, `width` and `height` are sent, so the object `getBounds` resolves with can be edited and passed back.

```javascript
await fb.ui.setBounds({ x: 100, y: 100, width: 1280, height: 720 });
```

### center()

Centers the window on the work area of its monitor, keeping its size.

Signature: `fb.ui.center(): Promise<WindowCenterResponse>`

```javascript
await fb.ui.center();
```

### hasSavedBounds()

Checks whether an earlier session saved the main window's position. A page can use it to decide whether to apply a default size on first launch.

Signature: `fb.ui.hasSavedBounds(): Promise<WindowHasSavedBoundsResponse>`

```javascript
const res = await fb.ui.hasSavedBounds();
if (res.success === false) throw new Error(res.error);
const { hasSavedBounds } = res;
```

## Size constraints

### setMinSize(width, height) / getMinSize()

Sets or gets the minimum window size.

```javascript
await fb.ui.setMinSize(400, 300);
const min = await fb.ui.getMinSize(); // {success, width, height, windowId}
```

### setMaxSize(width, height, windowId?) / getMaxSize(windowId?)

Sets or gets the maximum window size, in physical pixels. `0` means no bound.

Signature: `fb.ui.setMaxSize(width: number, height: number, windowId?: string): Promise<WindowSetMaxSizeResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `width` | `number` | Yes | Maximum width |
| `height` | `number` | Yes | Maximum height |
| `windowId` | `string` | No | Target window ID; omitted, the calling window |

Signature: `fb.ui.getMaxSize(windowId?: string): Promise<WindowGetMaxSizeResponse>`

```javascript
await fb.ui.setMaxSize(1920, 1080);
const max = await fb.ui.getMaxSize(); // {success, width, height, windowId}
```

### setResizable(resizable, windowId?)

Sets whether the window can be resized.

Signature: `fb.ui.setResizable(resizable: boolean, windowId?: string): Promise<WindowSetResizableResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `resizable` | `boolean` | Yes | Whether the user can resize the window by dragging its frame |
| `windowId` | `string` | No | Target window ID; omitted, the calling window |

```javascript
await fb.ui.setResizable(false);
```

## Always on top

### setAlwaysOnTop(enabled)

Keeps the window above other windows, or stops doing so.

```javascript
await fb.ui.setAlwaysOnTop(true);
```

### toggleAlwaysOnTop()

Toggles always-on-top.

Signature: `fb.ui.toggleAlwaysOnTop(): Promise<WindowToggleAlwaysOnTopResponse>`

```javascript
await fb.ui.toggleAlwaysOnTop();
```

## Fullscreen

### toggleFullscreen(windowId?)

Toggles fullscreen.

Signature: `fb.ui.toggleFullscreen(windowId?: string): Promise<WindowToggleFullscreenResponse>`

```javascript
await fb.ui.toggleFullscreen();
```

### enterFullscreen(windowId?) / exitFullscreen(windowId?)

Enters or leaves fullscreen. `enterFullscreen` fails with `OPERATION_FAILED` when the window is already fullscreen, and `exitFullscreen` fails with `OPERATION_FAILED` when it is not.

Signature: `fb.ui.enterFullscreen(windowId?: string): Promise<WindowEnterFullscreenResponse>`

Signature: `fb.ui.exitFullscreen(windowId?: string): Promise<WindowExitFullscreenResponse>`

```javascript
await fb.ui.enterFullscreen();
await fb.ui.exitFullscreen();
```

### setFullscreen(enabled)

Sets the fullscreen state.

```javascript
await fb.ui.setFullscreen(true);
```

## Focus and notifications

### focus(windowId?) / blur()

Focuses or unfocuses a window. `focus` brings the window to the foreground and restores it first when it is minimized. `blur` hands the foreground to the window below the calling window in the z-order.

Signature: `fb.ui.focus(windowId?: string): Promise<WindowFocusResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window ID: `main` or a popup ID; omitted, the calling window is focused |

Signature: `fb.ui.blur(): Promise<WindowBlurResponse>`

```javascript
await fb.ui.focus();
await fb.ui.blur();
```

### flash(opts)

Flashes the window's caption and taskbar button, or stops flashing.

Signature: `fb.ui.flash(opts: WindowFlashParams): Promise<WindowFlashResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts` | `WindowFlashParams` | Yes | `enabled: false` stops flashing, default `true`; `count` is how many times to flash, default `3` |

```javascript
await fb.ui.flash({ count: 3 });
await fb.ui.flash({ enabled: false }); // stop flashing
```

### flashTaskbar(count?)

Flashes the taskbar button. The optional `count` sets how many times.

```javascript
await fb.ui.flashTaskbar();
await fb.ui.flashTaskbar(3); // flash 3 times
```

### showSystemMenu(x, y, w?, h?)

Shows the system menu at a given position, in screen pixels. With a positive `w` and `h`, the menu opens below the rectangle `x`, `y`, `w`, `h` and keeps clear of it; otherwise it opens at `x`, `y`. When `w` is provided, the wrapper sends both `w` and `h`.

Signature: `fb.ui.showSystemMenu(x: number, y: number, w?: number, h?: number): Promise<WindowShowSystemMenuResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `number` | Yes | Screen X coordinate |
| `y` | `number` | Yes | Screen Y coordinate |
| `w` | `number` | No | Titlebar region width |
| `h` | `number` | No | Titlebar region height |

```javascript
await fb.ui.showSystemMenu(100, 32);
```

### showContextMenu(x?, y?)

Shows the main window's context menu. Without coordinates, with coordinates that are not positive, or with coordinates more than 50 px away from the real cursor, the menu opens at the cursor.

Signature: `fb.ui.showContextMenu(x?: number, y?: number): Promise<UiShowContextMenuResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `x` | `number` | No | Screen X coordinate |
| `y` | `number` | No | Screen Y coordinate |

```javascript
await fb.ui.showContextMenu();
```

## DPI and zoom

### getDpiScale()

Gets the DPI scaling.

```javascript
const res = await fb.ui.getDpiScale();
if (res.success === false) throw new Error(res.error);
const { dpi, scale } = res;
```

### setZoom(zoom) / getZoom() / resetZoom()

Sets, gets or resets the page zoom.

```javascript
await fb.ui.setZoom(1.5); // 150%
const res = await fb.ui.getZoom();
if (res.success === false) throw new Error(res.error);
const { zoom } = res;
await fb.ui.resetZoom();
```

### setZoomForDpi(dpi?)

Sets the zoom from a DPI: the zoom factor becomes `dpi / 96`.

Signature: `fb.ui.setZoomForDpi(dpi?: number): Promise<WindowSetZoomForDpiResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `dpi` | `number` | No | DPI to match; omitted, the current window's DPI |

```javascript
await fb.ui.setZoomForDpi();
```

### setMica(opts) / setMicaEffect(opts)

Turns on the Mica backdrop.

```javascript
await fb.ui.setMica({ enabled: true });
await fb.ui.setMicaEffect({ variant: 'mica-alt' });
```

### setAcrylic(opts)

Turns on the acrylic backdrop.

```javascript
await fb.ui.setAcrylic({ enabled: true, darkMode: true });
```

### setBlur(opts)

Turns on the blur effect.

Signature: `fb.ui.setBlur(opts: WindowSetBlurParams): Promise<WindowSetBlurResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts` | `WindowSetBlurParams` | Yes | Blur settings: `enabled` and an optional `windowId` |

```javascript
await fb.ui.setBlur({ enabled: true });
```

### setDarkMode(enabled)

Sets dark mode.

```javascript
await fb.ui.setDarkMode(true);
```

### setBackgroundTransparency(opts)

Sets the background transparency.

Signature: `fb.ui.setBackgroundTransparency(opts: WindowSetBackgroundTransparencyParams): Promise<WindowSetBackgroundTransparencyResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts` | `WindowSetBackgroundTransparencyParams` | Yes | Background transparency settings: `transparent` and an optional `windowId` |

```javascript
await fb.ui.setBackgroundTransparency({ transparent: true });
```

`setMica`, `setMicaEffect`, `setAcrylic`, `setBlur` and `setDarkMode` store their setting even when the window cannot draw it yet, for example while it is still hidden at startup. The call then fails with `OPERATION_FAILED`, and the failure carries the same fields as a success.

### refreshWebView()

Redraws the WebView. A DWM effect change may need it.

Signature: `fb.ui.refreshWebView(): Promise<WindowRefreshWebViewResponse>`

```javascript
await fb.ui.refreshWebView();
```

### setCornerPreference(mode) / getCornerPreference()

Sets or gets the window corner rounding (Windows 11). It applies to the main window only, whichever window calls.

Signature: `fb.ui.setCornerPreference(mode: string): Promise<WindowSetCornerPreferenceResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | Yes | `default` or `round` rounds the corners, `small` rounds them slightly, `none` keeps them square; any other value rounds them |

Signature: `fb.ui.getCornerPreference(): Promise<WindowGetCornerPreferenceResponse>`

```javascript
await fb.ui.setCornerPreference('round');
const res = await fb.ui.getCornerPreference();
if (res.success === false) throw new Error(res.error);
const { mode } = res;
```

## Titlebar

### getTitlebarHeight() / setTitlebarHeight(height)

Gets or sets the custom titlebar height.

```javascript
const res = await fb.ui.getTitlebarHeight();
if (res.success === false) throw new Error(res.error);
const { height } = res;
await fb.ui.setTitlebarHeight(32);
```

### getCaptionButtonsWidth()

Gets the width of the main window's caption buttons (minimize, maximize, close): `width` is all three together, `buttonWidth` is one of them.

Signature: `fb.ui.getCaptionButtonsWidth(): Promise<WindowGetCaptionButtonsWidthResponse>`

```javascript
const res = await fb.ui.getCaptionButtonsWidth();
if (res.success === false) throw new Error(res.error);
const { width, buttonWidth } = res;
```

### getTitlebarInfo()

Gets the full titlebar information.

Signature: `fb.ui.getTitlebarInfo(): Promise<WindowGetTitlebarInfoResponse>`

```javascript
const titlebar = await fb.ui.getTitlebarInfo();
```

### setDragRegions(regions) / clearDragRegions()

Sets or clears the drag regions, for the custom titlebar of a frameless window. `regions` is a `WindowRegion[]`; each entry is `{ x?, y?, width?, height? }` in CSS pixels of the page. A rectangle with any other key is refused.

```javascript
await fb.ui.setDragRegions([
    { x: 0, y: 0, width: 800, height: 32 }
]);
await fb.ui.clearDragRegions();
```

### setNoDragRegions(regions) / clearNoDragRegions()

Sets or clears the no-drag regions, which exclude elements inside a drag region.

Signature: `fb.ui.setNoDragRegions(regions: WindowRegion[]): Promise<WindowSetNoDragRegionsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `regions` | `WindowRegion[]` | Yes | No-drag regions, in the same format as `setDragRegions` |

Signature: `fb.ui.clearNoDragRegions(): Promise<WindowClearNoDragRegionsResponse>`

```javascript
await fb.ui.setNoDragRegions([{ x: 720, y: 0, width: 80, height: 32 }]);
await fb.ui.clearNoDragRegions();
```

### setMaximizeButtonRegion(region?)

Tells the host where the page draws the main window's maximize button, in CSS pixels of the page, so that on Windows 11 hovering it offers Snap layouts. The host hands the mouse input on the button back to the page, so the button keeps its hover and pressed styles and its own click handler. Set the rectangle again whenever layout moves the button, and omit `region` to remove it. When the response says `snapLayouts: true`, leave the button's `title` off and name it with `aria-label`, since the tooltip would cover the flyout.

Signature: `fb.ui.setMaximizeButtonRegion(region?: WindowRegion): Promise<WindowSetMaximizeButtonRegionResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `region` | `WindowRegion` | No | Rectangle of the maximize button, in CSS pixels of the page; omitted, the rectangle is removed |

```javascript
const button = document.getElementById('maximize');
if (button) {
    new ResizeObserver(() => {
        const r = button.getBoundingClientRect();
        void fb.ui.setMaximizeButtonRegion({ x: r.left, y: r.top, width: r.width, height: r.height });
    }).observe(button);
}
```

### setFrameless(frameless, windowId?)

Turns frameless mode on or off.

Signature: `fb.ui.setFrameless(frameless: boolean, windowId?: string): Promise<WindowSetFramelessResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `frameless` | `boolean` | Yes | `true` removes the native frame and caption, `false` restores them |
| `windowId` | `string` | No | Target window ID; omitted, the calling window |

```javascript
await fb.ui.setFrameless(true);
```

## Multiple windows

`fb.ui` wraps the whole multi-window API, so there is no need to call the native `window.*` methods.

### createPopup(opts)

Creates a popup window. Each popup has its own BridgeCore and WebView2 instance. The options type is `WindowCreatePopupOptions`: the declared `WindowCreatePopupParams` with `profile`, `behavior` and `backdropPolicy` typed. The response is `WindowCreatePopupResponse` (`{ windowId }`).

```javascript
const popup = await fb.ui.createPopup({
    url: 'settings.html',
    width: 600, height: 400,
    title: 'Settings',
    frame: false,
    beforeClose: true
});
// popup.windowId
```

#### Behavior presets: `profile`

The `profile` field is a declarative popup preset. It sets z-order, taskbar and backdrop policy for you:

| profile | owner | taskbar / Alt-Tab | Typical use |
| --- | --- | --- | --- |
| 'standard' | 'main' | Shown | Settings panels, property editors, child dialogs |
| 'miniPlayer' | 'none' | Hidden; stays visible on Win+D | Mini player, standalone floating window |
| 'desktopLyrics' | 'none' | Hidden; stays visible on Win+D; noActivate | Desktop lyrics, click-through overlay |

```javascript
// Recommended: let the profile configure everything at once
await fb.ui.createPopup({
    url: 'settings.html',
    width: 600, height: 400,
    profile: 'standard',  // owner becomes 'main', so the popup follows the main window
});
```

#### Z-order: `profile` / `behavior.owner` vs `alwaysOnTop`

::: tip Do not combine owner=main with alwaysOnTop

- **`profile: 'standard'`** (or `behavior: { owner: 'main' }`): the popup always stays above the main window (a Win32 owned-window relationship) but **does not cover other applications**. When the main window is minimized, the popup is minimized with it.
- **`profile: 'miniPlayer'` / `'desktopLyrics'`** (default `owner: 'none'`): the popup is independent of the main window. When it uses `alwaysOnTop: true` to hold its z-order, minimizing the main window does not take the popup with it.
- **Do not use `owner = 'main'` together with `alwaysOnTop = true`**: the activation chain then drags the main window forward, and minimizing hides both windows together.

:::

#### Per-field overrides: `behavior` / `backdropPolicy`

`behavior` overrides single fields on top of the profile, and `backdropPolicy` controls the DWM visual effects:

```javascript
await fb.ui.createPopup({
    url: 'lyrics.html',
    width: 800, height: 200,
    profile: 'desktopLyrics',
    behavior: {
        // Override only the desktopLyrics defaults you want to change:
        // let Show desktop (Win+D) hide the lyrics, and allow minimizing them.
        keepVisibleOnShowDesktop: false,
        allowMinimize: true,
    },
    backdropPolicy: {
        activeEffect: 'acrylic',
        inactiveEffect: 'acrylic',
    },
});
```

Field resolution precedence, from highest to lowest:

1. Explicit `behavior.*` / `backdropPolicy.*` overrides
2. Legacy top-level fields (`showInTaskbar` and the like)
3. `profile` preset defaults
4. Host defaults

For every `behavior` field see [`WindowPopupBehaviorPatch`](../api/window#window-setpopupbehavior); for `backdropPolicy` see [`WindowBackdropPolicyPatch`](../api/window#window-getbackdroppolicy).

### closePopup(windowId) / closeAllPopups()

Closes one popup or all popups. They close as their close button would: a popup created with `beforeClose` first gets `window:beforeClose`.

Signature: `fb.ui.closePopup(windowId: string): Promise<WindowClosePopupResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | Yes | ID of the popup to close |

Signature: `fb.ui.closeAllPopups(): Promise<WindowCloseAllPopupsResponse>`

```javascript
const res = await fb.ui.createPopup({ url: 'settings.html', width: 600, height: 400 });
if (res.success === false) throw new Error(res.error);
await fb.ui.closePopup(res.windowId);
await fb.ui.closeAllPopups();
```

### getAllWindows()

Lists every window. The response carries `items`: the main window first, then the popups. Each entry has its `windowId`, `title`, `bounds` and `capabilities`, the backdrop policy overrides that were set (`backdropPolicy`) and the policy in effect (`resolvedBackdropPolicy`). Entries are discriminated by `isMain` (`MainWindowInfo | PopupWindowInfo`); only popups carry `url`, `profile`, `behavior` and `resolvedBehavior`.

Signature: `fb.ui.getAllWindows(): Promise<WindowListResponse>`

```javascript
const windows = await fb.ui.getAllWindows();
```

### getCurrentWindowId()

Gets the ID of the calling window: `main`, a popup ID or a panel ID.

Signature: `fb.ui.getCurrentWindowId(): Promise<WindowGetCurrentWindowIdResponse>`

```javascript
const res = await fb.ui.getCurrentWindowId();
if (res.success === false) throw new Error(res.error);
const { windowId } = res;
```

### getPopupBehavior(windowId?) / setPopupBehavior(opts) {#popup-behavior}

Gets or sets a popup's behavior. Popups only; when `getPopupBehavior` omits `windowId`, it reads the calling popup. `getPopupBehavior` resolves with the popup's `windowId`, its preset (`profile`), the overrides that were set (`behavior`) and the behavior in effect (`resolvedBehavior`). `setPopupBehavior` resolves with the same fields after the change.

Signature: `fb.ui.getPopupBehavior(windowId?: string): Promise<WindowGetPopupBehaviorResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Popup window ID; omitted, the calling popup |

```javascript
const popup = await fb.ui.createPopup({ url: 'settings.html', width: 600, height: 400, profile: 'standard' });
if (popup.success === false) throw new Error(popup.error);
const behavior = await fb.ui.getPopupBehavior(popup.windowId);
await fb.ui.setPopupBehavior({
    windowId: popup.windowId,
    behavior: { owner: 'none' },
});
```

### getBackdropPolicy(windowId?) / setBackdropPolicy(opts) {#dwm-backdrop}

Gets or sets a window's backdrop policy. `getBackdropPolicy` resolves with the `windowId`, the overrides that were set (`backdropPolicy`) and the policy in effect (`resolvedBackdropPolicy`).

Signature: `fb.ui.getBackdropPolicy(windowId?: string): Promise<WindowGetBackdropPolicyResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Target window ID; omitted, the calling window |

`setBackdropPolicy` takes flat fields (`activeEffect`, `inactiveEffect` and the other `WindowBackdropPolicyPatch` fields) plus an optional `windowId`. The wrapper sends them to the host as `{ backdropPolicy }`, so do not nest `backdropPolicy` yourself. A `null` field removes that override.

`inactiveEffect` values: `inherit` \| `system` \| `none` \| `mica` \| `mica-alt` \| `acrylic` (`WindowInactiveBackdropEffect`). `inherit` is the host default: the unfocused window keeps the resolved `activeEffect`, and DWM dims it. `system` hands the unfocused frame back to the platform backdrop.

```javascript
const popup = await fb.ui.createPopup({ url: 'mini.html', width: 320, height: 120, profile: 'miniPlayer' });
if (popup.success === false) throw new Error(popup.error);
const policy = await fb.ui.getBackdropPolicy(popup.windowId);
await fb.ui.setBackdropPolicy({
    windowId: popup.windowId,
    activeEffect: 'none',
});
```

### setClickThrough(opts) / isClickThrough(windowId?)

Sets or queries click-through: when it is on, mouse input passes through the popup to the windows below. Popups only: when `windowId` is omitted, the calling window is used, and if it is not a popup the call fails with `NOT_FOUND`.

Signature: `fb.ui.setClickThrough(opts: WindowSetClickThroughParams): Promise<WindowSetClickThroughResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts` | `WindowSetClickThroughParams` | Yes | Click-through settings: `enabled` (default `true`) and an optional `windowId` |

Signature: `fb.ui.isClickThrough(windowId?: string): Promise<WindowIsClickThroughResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Popup window ID; omitted, the calling window |

```javascript
await fb.ui.setClickThrough({ enabled: true });
const res = await fb.ui.isClickThrough();
if (res.success === false) throw new Error(res.error);
const { clickThrough } = res;
```

### setClickThroughExcludeRegions(opts) / clearClickThroughExcludeRegions(windowId?)

Sets or clears the rectangles that keep taking mouse input while click-through is on. `setClickThroughExcludeRegions` replaces the earlier rectangles. Rectangles are CSS pixels of the page, scaled by the popup's DPI; rectangles without a positive width and height are skipped. At most 32 are kept, and the response carries `warning` when more were given. The target window is chosen as for `setClickThrough`.

Signature: `fb.ui.setClickThroughExcludeRegions(opts: WindowSetClickThroughExcludeRegionsParams): Promise<WindowSetClickThroughExcludeRegionsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts` | `WindowSetClickThroughExcludeRegionsParams` | Yes | Exclude-region settings: `regions` (`WindowRegion[]`) and an optional `windowId` |

Signature: `fb.ui.clearClickThroughExcludeRegions(windowId?: string): Promise<WindowClearClickThroughExcludeRegionsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `windowId` | `string` | No | Popup window ID; omitted, the calling window |

```javascript
const res = await fb.ui.setClickThroughExcludeRegions({
    regions: [{ x: 0, y: 0, width: 320, height: 80 }],
});
if (res.success === false) throw new Error(res.error);
const { count, warning } = res;
await fb.ui.clearClickThroughExcludeRegions();
```

### sendMessage(targetWindowId, message)

Sends a message to one window. `message` can be any JSON value except a top-level `null`.

```javascript
await fb.ui.sendMessage('main', { type: 'theme-changed', dark: true });
```

### broadcast(message)

Broadcasts a message to every window except the caller.

```javascript
await fb.ui.broadcast({ type: 'config-updated' });
```

Directed and broadcast messages arrive through `window:message` with the payload `{ sourceWindowId, message }`.

### cancelClose() / confirmClose()

Confirms or cancels a close asynchronously. Call them from a `window:beforeClose` handler. Only a popup created with `beforeClose: true` gets that event. It stays open until its page calls one of the two, and closes anyway after 3 seconds without either.

```javascript
// Placeholder for the page's own save logic.
async function saveChanges() {}

fb.on('window:beforeClose', async () => {
    const save = confirm('Save changes?');
    if (save) {
        await saveChanges();
        await fb.ui.confirmClose();
    } else {
        await fb.ui.cancelClose();
    }
});
```

## Development server

### getDevServerConfig() / setDevServerConfig(opts)

Gets or sets the development server settings: whether pages load from the development server (`useDevServer`) and the server's address (`devServerUrl`). `setDevServerConfig` keeps the value of every key you omit, and the new settings apply the next time a page loads.

Signature: `fb.ui.getDevServerConfig(): Promise<WindowGetDevServerConfigResponse>`

Signature: `fb.ui.setDevServerConfig(opts: WindowSetDevServerConfigParams): Promise<WindowSetDevServerConfigResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts` | `WindowSetDevServerConfigParams` | Yes | Development server settings: `useDevServer` and `devServerUrl`, both optional |

```javascript
const res = await fb.ui.getDevServerConfig();
if (res.success === false) throw new Error(res.error);
const { useDevServer, devServerUrl } = res;
await fb.ui.setDevServerConfig({ useDevServer: true, devServerUrl: 'http://localhost:5173' });
```
