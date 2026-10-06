# `fb.ui` window management

`fb.ui` is the SDK facade over the native `window.*` APIs plus `ui.showContextMenu`. Methods return promises and operate on the calling window unless a `windowId` parameter is documented.

## Basic window controls

| Method | Behavior |
| --- | --- |
| `minimize()` | Minimizes the current window. |
| `maximize()` | Maximizes the current window. |
| `restore()` | Restores a minimized or maximized window. |
| `close()` | Requests that the current window close. |
| `toggleMaximize()` | Toggles maximize state and may return `maximized`. |
| `reload()` | Reloads the current WebView. |
| `setTitle(title)` | Sets the native window title. |

```javascript
await fb.ui.minimize();
await fb.ui.restore();
await fb.ui.setTitle('Now Playing: Let It Be');
```

### startDrag() / startResize(edge)

Starts native drag or resize handling for a custom titlebar. `edge` is passed to `window.startResize`.

```javascript
document.getElementById('titlebar').addEventListener('mousedown', () => {
    fb.ui.startDrag();
});

await fb.ui.startResize('right');
```

## State queries

| Method | Return type |
| --- | --- |
| `getState()` | `Promise<WindowState>` |
| `isMaximized()` | `Promise<{ isMaximized: boolean }>` |
| `isMinimized()` | `Promise<{ isMinimized: boolean }>` |
| `isFullscreen()` | `Promise<{ isFullscreen: boolean }>` |
| `isAlwaysOnTop()` | `Promise<{ alwaysOnTop: boolean }>` |
| `isResizable()` | `Promise<{ resizable: boolean }>` |
| `getTitle()` | `Promise<{ title: string }>` |
| `getMode()` | `Promise<{ mode: string }>` |

```javascript
const state = await fb.ui.getState();
const { alwaysOnTop } = await fb.ui.isAlwaysOnTop();
const { resizable } = await fb.ui.isResizable();
```

## Position, size, and constraints

| Method | Signature and result |
| --- | --- |
| `setPosition(x, y)` | `(number, number) => Promise<BaseResponse>` |
| `setSize(width, height)` | `(number, number) => Promise<BaseResponse>` |
| `getBounds()` | `() => Promise<WindowBounds>` |
| `setBounds(opts)` | `(Partial<WindowBounds>) => Promise<BaseResponse>` |
| `center()` | `() => Promise<BaseResponse>` |
| `hasSavedBounds()` | `() => Promise<{ hasSavedBounds: boolean }>` |
| `setMinSize(width, height)` | `(number, number) => Promise<BaseResponse>` |
| `getMinSize()` | `() => Promise<{ width: number; height: number }>` |
| `setMaxSize(width, height)` | `(number, number) => Promise<BaseResponse>` |
| `getMaxSize()` | `() => Promise<{ width: number; height: number }>` |
| `setResizable(resizable)` | `(boolean) => Promise<BaseResponse>` |

```javascript
await fb.ui.setBounds({ x: 100, y: 100, width: 1280, height: 720 });
await fb.ui.setMinSize(400, 300);
await fb.ui.setMaxSize(1920, 1080);
```

## Always-on-top and fullscreen

| Method | Signature and result |
| --- | --- |
| `setAlwaysOnTop(enabled)` | `(boolean) => Promise<BaseResponse>` |
| `toggleAlwaysOnTop()` | `() => Promise<BaseResponse & { enabled?: boolean }>` |
| `toggleFullscreen()` | `() => Promise<BaseResponse & { fullscreen?: boolean }>` |
| `enterFullscreen()` | `() => Promise<BaseResponse & { isFullscreen?: boolean }>` |
| `exitFullscreen()` | `() => Promise<BaseResponse & { isFullscreen?: boolean }>` |
| `setFullscreen(enabled)` | `(boolean) => Promise<BaseResponse & { fullscreen?: boolean }>` |

```javascript
await fb.ui.setAlwaysOnTop(true);
await fb.ui.setFullscreen(true);
```

## Focus, flashing, and system menus

### focus(windowId?) / blur()

Focuses a target window, or the calling window when `windowId` is omitted. `blur()` removes focus from the current window.

### flash(opts) / flashTaskbar(count?)

`flash()` accepts `WindowFlashParams`. `flashTaskbar()` optionally passes a flash count.

```javascript
await fb.ui.flash({ count: 3 });
await fb.ui.flashTaskbar(3);
```

### showSystemMenu(x, y, w?, h?)

Shows the native system menu at screen coordinates. When `w` is provided, the wrapper sends both `w` and `h`.

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

Calls `ui.showContextMenu`. Omit coordinates to use the host's current pointer position.

```javascript
await fb.ui.showContextMenu();
```

## DPI and zoom

| Method | Signature and result |
| --- | --- |
| `getDpiScale()` | `() => Promise<DpiScaleResponse>` |
| `setZoom(zoom)` | `(number) => Promise<BaseResponse>` |
| `getZoom()` | `() => Promise<{ zoom: number }>` |
| `resetZoom()` | `() => Promise<BaseResponse & { zoom?: number }>` |
| `setZoomForDpi(dpi?)` | `(number?) => Promise<BaseResponse & { zoom?: number }>` |

```javascript
const { dpi, scale } = await fb.ui.getDpiScale();
await fb.ui.setZoom(1.5);
await fb.ui.setZoomForDpi(dpi);
```

## DWM and WebView effects

| Method | Parameters | Result |
| --- | --- | --- |
| `setMica(opts = {})` | `JsonObject` | Applied Mica state fields when exposed by the host |
| `setMicaEffect(opts = {})` | `JsonObject` | Applied Mica state fields when exposed by the host |
| `setAcrylic(opts)` | `WindowSetAcrylicParams` | `BaseResponse` |
| `setBlur(opts)` | `WindowSetBlurParams` | `BaseResponse` |
| `setDarkMode(enabled)` | `boolean` | `BaseResponse` |
| `setBackgroundTransparency(opts)` | `WindowSetBackgroundTransparencyParams` | `BaseResponse & { description?: string }` |
| `refreshWebView()` | None | `BaseResponse` |
| `setCornerPreference(mode)` | `string` | `BaseResponse` |
| `getCornerPreference()` | None | `{ mode: string }` |

```javascript
await fb.ui.setMica({ enabled: true });
await fb.ui.setAcrylic({ enabled: true, darkMode: true });
await fb.ui.setBackgroundTransparency({ transparent: true });
await fb.ui.setCornerPreference('round');
```

## Custom titlebar

| Method | Signature and result |
| --- | --- |
| `getTitlebarHeight()` | `() => Promise<{ height: number }>` |
| `setTitlebarHeight(height)` | `(number) => Promise<BaseResponse>` |
| `getCaptionButtonsWidth()` | `() => Promise<{ width: number }>` |
| `getTitlebarInfo()` | `() => Promise<WindowTitlebarInfo>` |
| `setDragRegions(regions)` | `(unknown[]) => Promise<BaseResponse & { count?: number; dpiScale?: number }>` |
| `clearDragRegions()` | `() => Promise<BaseResponse>` |
| `setNoDragRegions(regions)` | `(unknown[]) => Promise<BaseResponse & { count?: number; dpiScale?: number }>` |
| `clearNoDragRegions()` | `() => Promise<BaseResponse>` |
| `setFrameless(frameless)` | `(boolean) => Promise<BaseResponse>` |

```javascript
await fb.ui.setDragRegions([{ x: 0, y: 0, width: 800, height: 32 }]);
await fb.ui.setNoDragRegions([{ x: 720, y: 0, width: 80, height: 32 }]);
```

## Popup windows

### createPopup(opts)

Creates a popup with its own WebView2 and bridge context. The parameter type is `WindowCreatePopupParams`; the response is `{ windowId }`.

```javascript
const popup = await fb.ui.createPopup({
    url: 'settings.html',
    width: 600,
    height: 400,
    title: 'Settings',
    frame: false,
    beforeClose: true,
});
```

#### Behavior profiles

`profile` provides a declarative starting point for z-order, taskbar, activation, and backdrop behavior.

| Profile | Default owner | Typical use |
| --- | --- | --- |
| `'standard'` | `'main'` | Settings, properties, and child dialogs |
| `'miniPlayer'` | `'none'` | Independent compact player |
| `'desktopLyrics'` | `'none'` | Desktop lyrics or click-through overlay |

```javascript
await fb.ui.createPopup({
    url: 'settings.html',
    width: 600,
    height: 400,
    profile: 'standard',
});
```

::: tip Owned windows and always-on-top
An owned popup (`profile: 'standard'` or `behavior.owner: 'main'`) stays above the main window without staying above other applications, and follows the owner when it is minimized. Independent profiles can use `alwaysOnTop` without being minimized with the main window. Avoid combining `owner: 'main'` with `alwaysOnTop: true`; the activation chain can bring the main window forward and cause coupled minimize behavior.
:::

Explicit `behavior` and `backdropPolicy` fields override profile defaults:

```javascript
await fb.ui.createPopup({
    url: 'lyrics.html',
    width: 800,
    height: 200,
    profile: 'desktopLyrics',
    behavior: {
        keepVisibleOnShowDesktop: true,
        allowMinimize: false,
    },
    backdropPolicy: {
        activeEffect: 'none',
        inactiveEffect: 'none',
    },
});
```

Resolution precedence is:

1. Explicit `behavior.*` and `backdropPolicy.*` fields
2. Legacy top-level fields such as `showInTaskbar`
3. `profile` defaults
4. Host defaults

See [`WindowPopupBehaviorPatch`](../api/window#window-setpopupbehavior) and [`WindowBackdropPolicyPatch`](../api/window#window-getbackdroppolicy) for detailed fields.

### Popup lifecycle and policy methods

| Method | Signature and result |
| --- | --- |
| `closePopup(windowId)` | `(string) => Promise<BaseResponse>` |
| `closeAllPopups()` | `() => Promise<BaseResponse>` |
| `getAllWindows()` | `() => Promise<WindowListResponse>` |
| `getCurrentWindowId()` | `() => Promise<{ windowId: string }>` |
| `getPopupBehavior(windowId?)` | `(string?) => Promise<WindowPopupBehaviorState>` |
| `setPopupBehavior(opts)` | `(WindowSetPopupBehaviorParams) => Promise<WindowPopupBehaviorResponse>` |
| `getBackdropPolicy(windowId?)` | `(string?) => Promise<WindowBackdropPolicyState>` |
| `setBackdropPolicy(opts)` | `(WindowBackdropPolicyPatch & { windowId?: string }) => Promise<WindowBackdropPolicyResponse>` |

`setBackdropPolicy` takes a flat patch (`activeEffect`, `inactiveEffect`, and the other `WindowBackdropPolicyPatch` fields) plus optional `windowId`. The wrapper sends `{ backdropPolicy }` to the host; do not nest `backdropPolicy` yourself.

`inactiveEffect` values: `inherit` \| `system` \| `none` \| `mica` \| `mica-alt` \| `acrylic` (`WindowInactiveBackdropEffect`). `inherit` is the host default: the unfocused window keeps the resolved `activeEffect` and DWM applies its own inactive dimming. `system` hands the unfocused frame back to the platform backdrop.

### Click-through regions

| Method | Signature and result |
| --- | --- |
| `setClickThrough(opts)` | `(WindowSetClickThroughParams) => Promise<BaseResponse & { clickThrough?: boolean }>` |
| `isClickThrough(windowId?)` | `(string?) => Promise<{ clickThrough: boolean }>` |
| `setClickThroughExcludeRegions(opts)` | `(WindowSetClickThroughExcludeRegionsParams) => Promise<BaseResponse & { count?: number; dpiScale?: number; warning?: string }>` |
| `clearClickThroughExcludeRegions(windowId?)` | `(string?) => Promise<BaseResponse>` |

```javascript
await fb.ui.setClickThrough({ enabled: true });
await fb.ui.setClickThroughExcludeRegions({
    regions: [{ x: 0, y: 0, width: 320, height: 80 }],
});
```

## Cross-window messaging

### sendMessage(targetWindowId, message) / broadcast(message)

Sends to one window or broadcasts to all windows.

```javascript
await fb.ui.sendMessage('main', { type: 'theme-changed', dark: true });
await fb.ui.broadcast({ type: 'config-updated' });
```

Messages arrive through `window:message` with `{ sourceWindowId, message }`.

### cancelClose() / confirmClose()

Completes an asynchronous close decision after `window:beforeClose`.

```javascript
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

`getDevServerConfig()` returns `WindowDevServerConfig`. `setDevServerConfig()` accepts `WindowSetDevServerConfigParams`.

```javascript
const config = await fb.ui.getDevServerConfig();
await fb.ui.setDevServerConfig({
    useDevServer: true,
    devServerUrl: 'http://localhost:5173',
});
```

## Complete wrapper token index

The following calls preserve the exact SDK tokens for methods that are summarized in tables above:

```javascript
await fb.ui.maximize();
await fb.ui.close();
await fb.ui.toggleMaximize();
await fb.ui.reload();
await fb.ui.isMaximized();
await fb.ui.isMinimized();
await fb.ui.isFullscreen();
await fb.ui.getTitle();
await fb.ui.getMode();
await fb.ui.setPosition(100, 100);
await fb.ui.setSize(1280, 720);
await fb.ui.getBounds();
await fb.ui.center();
await fb.ui.hasSavedBounds();
await fb.ui.getMinSize();
await fb.ui.getMaxSize();
await fb.ui.setResizable(true);
await fb.ui.toggleAlwaysOnTop();
await fb.ui.toggleFullscreen();
await fb.ui.enterFullscreen();
await fb.ui.exitFullscreen();
await fb.ui.focus();
await fb.ui.blur();
await fb.ui.getZoom();
await fb.ui.resetZoom();
await fb.ui.setMicaEffect({ enabled: true });
await fb.ui.setBlur({ enabled: true });
await fb.ui.setDarkMode(true);
await fb.ui.refreshWebView();
await fb.ui.getCornerPreference();
await fb.ui.getTitlebarHeight();
await fb.ui.setTitlebarHeight(32);
await fb.ui.getCaptionButtonsWidth();
await fb.ui.getTitlebarInfo();
await fb.ui.clearDragRegions();
await fb.ui.clearNoDragRegions();
await fb.ui.setFrameless(true);
await fb.ui.closePopup(popup.windowId);
await fb.ui.closeAllPopups();
await fb.ui.getAllWindows();
await fb.ui.getCurrentWindowId();
await fb.ui.getPopupBehavior();
await fb.ui.setPopupBehavior({
    windowId: popup.windowId,
    behavior: { owner: 'none' },
});
await fb.ui.getBackdropPolicy();
await fb.ui.setBackdropPolicy({ windowId: popup.windowId, activeEffect: 'none' });
await fb.ui.isClickThrough();
await fb.ui.clearClickThroughExcludeRegions();
```
