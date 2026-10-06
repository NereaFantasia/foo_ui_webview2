# fb.ui 窗口控制

`fb.ui` 封装了 `window.*` 底层 API 与 `ui.showContextMenu`，提供窗口管理的完整能力。方法都返回 Promise；除非条目另有说明，都作用于调用方窗口。

`isFullscreen`、`isResizable`、`getMinSize` / `setMinSize`、`getMaxSize` / `setMaxSize`、`setResizable`、`setDarkMode`、`setFrameless` 与四个全屏方法可以在参数末尾多传一个 `windowId`，指定 `main` 或某个 popup；省略时作用于调用方窗口。

## 基本窗口控制

### minimize()

最小化窗口。

```javascript
await fb.ui.minimize();
```

### maximize()

最大化窗口。

```javascript
await fb.ui.maximize();
```

### restore()

恢复窗口（从最小化或最大化状态）。

```javascript
await fb.ui.restore();
```

### close()

关闭窗口。

```javascript
await fb.ui.close();
```

### toggleMaximize()

切换最大化状态：最大化窗口，已最大化时还原。返回 `maximized`：请求生效后窗口是否最大化。

```javascript
const res = await fb.ui.toggleMaximize();
if (res.success === false) throw new Error(res.error);
const { maximized } = res;
```

### setTitle(title)

设置窗口标题。

```javascript
await fb.ui.setTitle('Now Playing: Let It Be');
```

### reload()

重新加载页面（开发调试用）。

```javascript
await fb.ui.reload();
```

### startDrag()

开始拖拽窗口（用于自定义标题栏）。在 `mousedown` 事件中调用。

```javascript
document.getElementById('titlebar').addEventListener('mousedown', () => {
    fb.ui.startDrag();
});
```

### startResize(edge)

开始调整窗口大小。`edge` 为调整方向。与 `startDrag` 一样在 `mousedown` 事件中调用。

签名：`fb.ui.startResize(edge: string): Promise<WindowStartResizeResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| edge | string | 是 | 要拖动的边或角：`left`、`right`、`top`、`bottom`、`topleft`、`topright`、`bottomleft`、`bottomright`；其他值按右下角处理 |

```javascript
document.getElementById('resize-grip')?.addEventListener('mousedown', () => {
    fb.ui.startResize('bottomright');
});
```

## 状态查询

### getState()

获取窗口完整状态。

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

获取当前窗口标题。

签名：`fb.ui.getTitle(): Promise<WindowGetTitleResponse>`

```javascript
const res = await fb.ui.getTitle();
if (res.success === false) throw new Error(res.error);
const { title } = res;
```

### getMode()

获取调用方页面的宿主方式：`mode` 为 `standalone`、`dui`、`cui`、`panel` 或 `unknown`，另带 `panelMode` 与 `windowId`。

签名：`fb.ui.getMode(): Promise<WindowGetModeResponse>`

```javascript
const res = await fb.ui.getMode();
if (res.success === false) throw new Error(res.error);
const { mode } = res;
```

## 位置 / 尺寸

### setPosition(x, y)

设置窗口位置。

```javascript
await fb.ui.setPosition(100, 100);
```

### setSize(width, height)

设置窗口大小。

```javascript
await fb.ui.setSize(1280, 720);
```

### getBounds()

获取窗口边界 `{x, y, width, height}`。

签名：`fb.ui.getBounds(): Promise<WindowGetBoundsResponse>`

```javascript
const res = await fb.ui.getBounds();
if (res.success === false) throw new Error(res.error);
const { x, y, width, height } = res;
```

### setBounds(opts)

一次性设置窗口位置和大小。只发送 `x`、`y`、`width`、`height` 四个键，所以 `getBounds` 回来的对象改完可以直接传回。

```javascript
await fb.ui.setBounds({ x: 100, y: 100, width: 1280, height: 720 });
```

### center()

把窗口移到所在显示器工作区的中央，大小不变。

签名：`fb.ui.center(): Promise<WindowCenterResponse>`

```javascript
await fb.ui.center();
```

### hasSavedBounds()

检查此前的会话是否保存过主窗口的位置，页面可据此决定首次启动时是否设置默认大小。

签名：`fb.ui.hasSavedBounds(): Promise<WindowHasSavedBoundsResponse>`

```javascript
const res = await fb.ui.hasSavedBounds();
if (res.success === false) throw new Error(res.error);
const { hasSavedBounds } = res;
```

## 尺寸约束

### setMinSize(width, height) / getMinSize()

设置/获取窗口最小尺寸。

```javascript
await fb.ui.setMinSize(400, 300);
const min = await fb.ui.getMinSize(); // {success, width, height, windowId}
```

### setMaxSize(width, height, windowId?) / getMaxSize(windowId?)

设置/获取窗口最大尺寸，单位是物理像素，`0` 表示不设上限。

签名：`fb.ui.setMaxSize(width: number, height: number, windowId?: string): Promise<WindowSetMaxSizeResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| width | number | 是 | 最大宽度 |
| height | number | 是 | 最大高度 |
| windowId | string | 否 | 目标窗口 ID；省略时为调用方窗口 |

签名：`fb.ui.getMaxSize(windowId?: string): Promise<WindowGetMaxSizeResponse>`

```javascript
await fb.ui.setMaxSize(1920, 1080);
const max = await fb.ui.getMaxSize(); // {success, width, height, windowId}
```

### setResizable(resizable, windowId?)

设置窗口是否可调整大小。

签名：`fb.ui.setResizable(resizable: boolean, windowId?: string): Promise<WindowSetResizableResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| resizable | boolean | 是 | 是否允许用户拖动边框调整窗口大小 |
| windowId | string | 否 | 目标窗口 ID；省略时为调用方窗口 |

```javascript
await fb.ui.setResizable(false);
```

## 置顶

### setAlwaysOnTop(enabled)

设置窗口置顶。

```javascript
await fb.ui.setAlwaysOnTop(true);
```

### toggleAlwaysOnTop()

切换置顶状态。

签名：`fb.ui.toggleAlwaysOnTop(): Promise<WindowToggleAlwaysOnTopResponse>`

```javascript
await fb.ui.toggleAlwaysOnTop();
```

## 全屏

### toggleFullscreen(windowId?)

切换全屏模式。

签名：`fb.ui.toggleFullscreen(windowId?: string): Promise<WindowToggleFullscreenResponse>`

```javascript
await fb.ui.toggleFullscreen();
```

### enterFullscreen(windowId?) / exitFullscreen(windowId?)

进入/退出全屏。窗口已经全屏时 `enterFullscreen` 以 `OPERATION_FAILED` 失败，窗口不在全屏时 `exitFullscreen` 以 `OPERATION_FAILED` 失败。

签名：`fb.ui.enterFullscreen(windowId?: string): Promise<WindowEnterFullscreenResponse>`

签名：`fb.ui.exitFullscreen(windowId?: string): Promise<WindowExitFullscreenResponse>`

```javascript
await fb.ui.enterFullscreen();
await fb.ui.exitFullscreen();
```

### setFullscreen(enabled)

设置全屏状态。

```javascript
await fb.ui.setFullscreen(true);
```

## 焦点与通知

### focus(windowId?) / blur()

聚焦/失焦窗口。`focus` 把窗口带到前台，最小化的窗口先还原；`blur` 把前台交给 Z 序里位于调用方窗口之下的窗口。

签名：`fb.ui.focus(windowId?: string): Promise<WindowFocusResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | 目标窗口 ID：`main` 或 popup 的 ID；省略时聚焦调用方窗口 |

签名：`fb.ui.blur(): Promise<WindowBlurResponse>`

```javascript
await fb.ui.focus();
await fb.ui.blur();
```

### flash(opts)

闪烁窗口的标题栏和任务栏按钮，或停止闪烁。

签名：`fb.ui.flash(opts: WindowFlashParams): Promise<WindowFlashResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowFlashParams | 是 | `enabled` 为 `false` 时停止闪烁，默认 `true`；`count` 为闪烁次数，默认 `3` |

```javascript
await fb.ui.flash({ count: 3 });
await fb.ui.flash({ enabled: false }); // 停止闪烁
```

### flashTaskbar(count?)

闪烁任务栏按钮。`count` 可选，指定闪烁次数。

```javascript
await fb.ui.flashTaskbar();
await fb.ui.flashTaskbar(3); // 闪烁 3 次
```

### showSystemMenu(x, y, w?, h?)

在指定位置显示系统菜单，坐标是屏幕像素。`w` 与 `h` 都为正时，菜单在矩形 `x`、`y`、`w`、`h` 下方弹出并避开它，否则在 `x`、`y` 处弹出。传了 `w` 时，封装层会同时发送 `w` 和 `h`。

签名：`fb.ui.showSystemMenu(x: number, y: number, w?: number, h?: number): Promise<WindowShowSystemMenuResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| x | number | 是 | 屏幕 X 坐标 |
| y | number | 是 | 屏幕 Y 坐标 |
| w | number | 否 | 标题栏区域宽度 |
| h | number | 否 | 标题栏区域高度 |

```javascript
await fb.ui.showSystemMenu(100, 32);
```

### showContextMenu(x?, y?)

显示主窗口的右键上下文菜单。不传坐标、坐标不为正或与实际光标相差超过 50 px 时，在光标位置弹出。

签名：`fb.ui.showContextMenu(x?: number, y?: number): Promise<UiShowContextMenuResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| x | number | 否 | 屏幕 X 坐标 |
| y | number | 否 | 屏幕 Y 坐标 |

```javascript
await fb.ui.showContextMenu();
```

## DPI / 缩放

### getDpiScale()

获取 DPI 缩放信息。

```javascript
const res = await fb.ui.getDpiScale();
if (res.success === false) throw new Error(res.error);
const { dpi, scale } = res;
```

### setZoom(zoom) / getZoom() / resetZoom()

设置/获取/重置页面缩放级别。

```javascript
await fb.ui.setZoom(1.5); // 150%
const res = await fb.ui.getZoom();
if (res.success === false) throw new Error(res.error);
const { zoom } = res;
await fb.ui.resetZoom();
```

### setZoomForDpi(dpi?)

根据 DPI 自动设置缩放：缩放倍数设为 `dpi / 96`。

签名：`fb.ui.setZoomForDpi(dpi?: number): Promise<WindowSetZoomForDpiResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| dpi | number | 否 | 要匹配的 DPI；省略时使用当前窗口的 DPI |

```javascript
await fb.ui.setZoomForDpi();
```

### setMica(opts) / setMicaEffect(opts)

启用 Mica 材质效果。

```javascript
await fb.ui.setMica({ enabled: true });
await fb.ui.setMicaEffect({ variant: 'mica-alt' });
```

### setAcrylic(opts)

启用 Acrylic 亚克力效果。

```javascript
await fb.ui.setAcrylic({ enabled: true, darkMode: true });
```

### setBlur(opts)

启用模糊效果。

签名：`fb.ui.setBlur(opts: WindowSetBlurParams): Promise<WindowSetBlurResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetBlurParams | 是 | Blur 效果配置：`enabled` 与可选的 `windowId` |

```javascript
await fb.ui.setBlur({ enabled: true });
```

### setDarkMode(enabled)

设置深色模式。

```javascript
await fb.ui.setDarkMode(true);
```

### setBackgroundTransparency(opts)

设置背景透明度。

签名：`fb.ui.setBackgroundTransparency(opts: WindowSetBackgroundTransparencyParams): Promise<WindowSetBackgroundTransparencyResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetBackgroundTransparencyParams | 是 | 背景透明度配置：`transparent` 与可选的 `windowId` |

```javascript
await fb.ui.setBackgroundTransparency({ transparent: true });
```

`setMica`、`setMicaEffect`、`setAcrylic`、`setBlur` 与 `setDarkMode` 在窗口暂时画不出效果时（例如启动时仍隐藏）也会把设置存下，此时调用以 `OPERATION_FAILED` 失败，失败里带与成功时相同的字段。

### refreshWebView()

刷新 WebView 渲染（DWM 特效切换后可能需要）。

签名：`fb.ui.refreshWebView(): Promise<WindowRefreshWebViewResponse>`

```javascript
await fb.ui.refreshWebView();
```

### setCornerPreference(mode) / getCornerPreference()

设置/获取窗口圆角模式（Windows 11）。不论哪个窗口调用，都只作用于主窗口。

签名：`fb.ui.setCornerPreference(mode: string): Promise<WindowSetCornerPreferenceResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| mode | string | 是 | `default` 或 `round` 为圆角，`small` 为小圆角，`none` 为直角；其他值按圆角处理 |

签名：`fb.ui.getCornerPreference(): Promise<WindowGetCornerPreferenceResponse>`

```javascript
await fb.ui.setCornerPreference('round');
const res = await fb.ui.getCornerPreference();
if (res.success === false) throw new Error(res.error);
const { mode } = res;
```

## 标题栏

### getTitlebarHeight() / setTitlebarHeight(height)

获取/设置自定义标题栏高度。

```javascript
const res = await fb.ui.getTitlebarHeight();
if (res.success === false) throw new Error(res.error);
const { height } = res;
await fb.ui.setTitlebarHeight(32);
```

### getCaptionButtonsWidth()

获取主窗口标题栏按钮（最小化/最大化/关闭）的宽度：`width` 是三个按钮的总宽度，`buttonWidth` 是单个按钮的宽度。

签名：`fb.ui.getCaptionButtonsWidth(): Promise<WindowGetCaptionButtonsWidthResponse>`

```javascript
const res = await fb.ui.getCaptionButtonsWidth();
if (res.success === false) throw new Error(res.error);
const { width, buttonWidth } = res;
```

### getTitlebarInfo()

获取标题栏完整信息。

签名：`fb.ui.getTitlebarInfo(): Promise<WindowGetTitlebarInfoResponse>`

```javascript
const titlebar = await fb.ui.getTitlebarInfo();
```

### setDragRegions(regions) / clearDragRegions()

设置/清除可拖拽区域（用于无边框窗口的自定义标题栏）。`regions` 是 `WindowRegion[]`，每项 `{ x?, y?, width?, height? }`，页面 CSS 像素；带其他键的矩形会被拒绝。

```javascript
await fb.ui.setDragRegions([
    { x: 0, y: 0, width: 800, height: 32 }
]);
await fb.ui.clearDragRegions();
```

### setNoDragRegions(regions) / clearNoDragRegions()

设置/清除不可拖拽区域（在拖拽区域内排除某些元素）。

签名：`fb.ui.setNoDragRegions(regions: WindowRegion[]): Promise<WindowSetNoDragRegionsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| regions | WindowRegion[] | 是 | 不可拖拽区域，格式同 `setDragRegions` |

签名：`fb.ui.clearNoDragRegions(): Promise<WindowClearNoDragRegionsResponse>`

```javascript
await fb.ui.setNoDragRegions([{ x: 720, y: 0, width: 80, height: 32 }]);
await fb.ui.clearNoDragRegions();
```

### setMaximizeButtonRegion(region?)

告诉宿主页面把主窗口的最大化键画在哪里（页面 CSS 像素），这样在 Windows 11 上悬停
它会弹出贴靠布局。按钮上的鼠标输入由宿主转回页面，按钮照常有悬停、按下样式和自己
的点击处理。布局挪动按钮后要重新设置，省略 `region` 则移除。响应里 `snapLayouts`
为 `true` 时，不要给按钮设 `title`，改用 `aria-label` 命名，否则提示会挡住浮层。

签名：`fb.ui.setMaximizeButtonRegion(region?: WindowRegion): Promise<WindowSetMaximizeButtonRegionResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| region | WindowRegion | 否 | 最大化键的矩形，页面 CSS 像素；省略则移除 |

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

设置无边框模式。

签名：`fb.ui.setFrameless(frameless: boolean, windowId?: string): Promise<WindowSetFramelessResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| frameless | boolean | 是 | `true` 去掉原生边框与标题栏，`false` 恢复 |
| windowId | string | 否 | 目标窗口 ID；省略时为调用方窗口 |

```javascript
await fb.ui.setFrameless(true);
```

## 多窗口管理

`fb.ui` 已完整封装多窗口 API，无需直接调用底层 `window.*` 方法。

### createPopup(opts)

创建弹出窗口。每个 popup 拥有独立的 BridgeCore + WebView2 实例。参数类型是 `WindowCreatePopupOptions`：声明的 `WindowCreatePopupParams`，其中 `profile`、`behavior`、`backdropPolicy` 换成了强类型；返回 `WindowCreatePopupResponse`（`{ windowId }`）。

```javascript
const popup = await fb.ui.createPopup({
    url: 'settings.html',
    width: 600, height: 400,
    title: '设置',
    frame: false,
    beforeClose: true
});
// popup.windowId
```

#### 行为预设：`profile`

`profile` 字段是声明式的弹窗类型预设，自动配置 z-order / taskbar / 背景策略：

| profile | owner | taskbar / Alt-Tab | 典型场景 |
| --- | --- | --- | --- |
| 'standard' | 'main' | 显示 | 设置面板、属性编辑器、子对话框 |
| 'miniPlayer' | 'none' | 隐藏 + 抑制 Win+D | 迷你播放器、独立浮窗 |
| 'desktopLyrics' | 'none' | 隐藏 + 抑制 Win+D + noActivate | 桌面歌词、点击穿透浮层 |

```javascript
// 推荐：用 profile 一次配齐
await fb.ui.createPopup({
    url: 'settings.html',
    width: 600, height: 400,
    profile: 'standard',  // 自动 owner=main，跟随主窗口
});
```

#### z-order 策略：`profile` / `behavior.owner` vs `alwaysOnTop`

::: tip 不要混用 owner=main 与 alwaysOnTop

- **`profile: 'standard'`**（或 `behavior: { owner: 'main' }`）：popup 始终在主窗口上方（Win32 owned-window 关系），但**不会盖住其他应用**。主窗口最小化时 popup 跟随最小化。
- **`profile: 'miniPlayer'` / `'desktopLyrics'`**（默认 `owner: 'none'`）：popup 与主窗口独立。配合 `alwaysOnTop: true` 维持 z-order 时，最小化主窗口不会带走 popup。
- **`owner = 'main'` 与 `alwaysOnTop = true` 不应同时使用**：会触发激活链拽起主窗口、最小化连带隐藏的副作用。

:::

#### 局部覆盖：`behavior` / `backdropPolicy`

`behavior` 在 profile 之上做单字段覆盖，`backdropPolicy` 控制 DWM 视觉效果：

```javascript
await fb.ui.createPopup({
    url: 'lyrics.html',
    width: 800, height: 200,
    profile: 'desktopLyrics',
    behavior: {
        // 只覆盖 desktopLyrics 默认中需要调整的字段：
        // 让「显示桌面」（Win+D）也能隐藏歌词，并允许最小化
        keepVisibleOnShowDesktop: false,
        allowMinimize: true,
    },
    backdropPolicy: {
        activeEffect: 'acrylic',
        inactiveEffect: 'acrylic',
    },
});
```

字段解析优先级（高到低）：

1. `behavior.*` / `backdropPolicy.*` 显式覆盖
2. 旧顶层字段（`showInTaskbar` 等）
3. `profile` 预设默认
4. 主程序默认值

`behavior` 完整字段定义见 [`WindowPopupBehaviorPatch`](../api/window#window-setpopupbehavior)；`backdropPolicy` 见 [`WindowBackdropPolicyPatch`](../api/window#window-getbackdroppolicy)。

### closePopup(windowId) / closeAllPopups()

关闭指定弹出窗口或所有弹出窗口。关闭方式与点关闭按钮相同：以 `beforeClose` 创建的 popup 会先收到 `window:beforeClose`。

签名：`fb.ui.closePopup(windowId: string): Promise<WindowClosePopupResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 是 | 要关闭的 popup 窗口 ID |

签名：`fb.ui.closeAllPopups(): Promise<WindowCloseAllPopupsResponse>`

```javascript
const res = await fb.ui.createPopup({ url: 'settings.html', width: 600, height: 400 });
if (res.success === false) throw new Error(res.error);
await fb.ui.closePopup(res.windowId);
await fb.ui.closeAllPopups();
```

### getAllWindows()

获取所有窗口列表。响应的 `items` 里主窗口在前，其后是各 popup。每个条目带 `windowId`、`title`、`bounds` 与 `capabilities`，以及设置过的背景策略覆盖（`backdropPolicy`）与实际生效的策略（`resolvedBackdropPolicy`）。条目按 `isMain` 区分（`MainWindowInfo | PopupWindowInfo`），只有 popup 带 `url`、`profile`、`behavior` 与 `resolvedBehavior`。

签名：`fb.ui.getAllWindows(): Promise<WindowListResponse>`

```javascript
const windows = await fb.ui.getAllWindows();
```

### getCurrentWindowId()

获取调用方窗口的 ID：`main`、popup 的 ID 或面板的 ID。

签名：`fb.ui.getCurrentWindowId(): Promise<WindowGetCurrentWindowIdResponse>`

```javascript
const res = await fb.ui.getCurrentWindowId();
if (res.success === false) throw new Error(res.error);
const { windowId } = res;
```

### getPopupBehavior(windowId?) / setPopupBehavior(opts) {#popup-behavior}

获取/设置弹出窗口行为。只适用于 popup；`getPopupBehavior` 省略 `windowId` 时取调用方 popup。`getPopupBehavior` 返回 popup 的 `windowId`、行为预设（`profile`）、设置过的覆盖项（`behavior`）与实际生效的行为（`resolvedBehavior`）；`setPopupBehavior` 返回修改之后的同样几个字段。

签名：`fb.ui.getPopupBehavior(windowId?: string): Promise<WindowGetPopupBehaviorResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | popup 窗口 ID；省略时查询调用方 popup |

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

获取/设置窗口背景策略。`getBackdropPolicy` 返回 `windowId`、设置过的覆盖项（`backdropPolicy`）与实际生效的策略（`resolvedBackdropPolicy`）。

签名：`fb.ui.getBackdropPolicy(windowId?: string): Promise<WindowGetBackdropPolicyResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | 目标窗口 ID；省略时查询调用方窗口 |

`setBackdropPolicy` 接受平铺字段（`activeEffect`、`inactiveEffect` 以及 `WindowBackdropPolicyPatch` 的其余字段）和可选的 `windowId`。封装层会组装成 `{ backdropPolicy }` 发给宿主，调用方不要再包一层 `backdropPolicy`。值为 `null` 的字段删除该覆盖。

`inactiveEffect` 取值：`inherit` \| `system` \| `none` \| `mica` \| `mica-alt` \| `acrylic`（`WindowInactiveBackdropEffect`）。`inherit` 为宿主默认值，表示失焦时沿用已解析的 `activeEffect`，由 DWM 负责失焦变暗；`system` 表示失焦时交还平台背景。

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

设置/查询窗口点击穿透：开启后鼠标输入穿过 popup，落到下面的窗口。只适用于 popup：省略 `windowId` 时取调用方窗口，它不是 popup 时以 `NOT_FOUND` 失败。

签名：`fb.ui.setClickThrough(opts: WindowSetClickThroughParams): Promise<WindowSetClickThroughResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetClickThroughParams | 是 | 点击穿透配置：`enabled`（默认 `true`）与可选的 `windowId` |

签名：`fb.ui.isClickThrough(windowId?: string): Promise<WindowIsClickThroughResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | popup 窗口 ID；省略时查询调用方窗口 |

```javascript
await fb.ui.setClickThrough({ enabled: true });
const res = await fb.ui.isClickThrough();
if (res.success === false) throw new Error(res.error);
const { clickThrough } = res;
```

### setClickThroughExcludeRegions(opts) / clearClickThroughExcludeRegions(windowId?)

设置/清除点击穿透时仍接收鼠标输入的矩形。`setClickThroughExcludeRegions` 替换之前的设置；矩形是页面 CSS 像素，按 popup 的 DPI 缩放，宽或高不为正的矩形被跳过，最多保留 32 个，超出时响应带 `warning`。目标窗口的选取同 `setClickThrough`。

签名：`fb.ui.setClickThroughExcludeRegions(opts: WindowSetClickThroughExcludeRegionsParams): Promise<WindowSetClickThroughExcludeRegionsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetClickThroughExcludeRegionsParams | 是 | 排除区域配置：`regions`（`WindowRegion[]`）与可选的 `windowId` |

签名：`fb.ui.clearClickThroughExcludeRegions(windowId?: string): Promise<WindowClearClickThroughExcludeRegionsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | popup 窗口 ID；省略时作用于调用方窗口 |

```javascript
const res = await fb.ui.setClickThroughExcludeRegions({
    regions: [{ x: 0, y: 0, width: 320, height: 80 }],
});
if (res.success === false) throw new Error(res.error);
const { count, warning } = res;
await fb.ui.clearClickThroughExcludeRegions();
```

### sendMessage(targetWindowId, message)

向指定窗口发送消息。`message` 可以是除顶层 `null` 以外的任意 JSON 值。

```javascript
await fb.ui.sendMessage('main', { type: 'theme-changed', dark: true });
```

### broadcast(message)

向除调用方以外的所有窗口广播消息。

```javascript
await fb.ui.broadcast({ type: 'config-updated' });
```

定向发送或广播的消息会通过 `window:message` 到达，payload 为 `{ sourceWindowId, message }`。

### cancelClose() / confirmClose()

异步确认或取消关闭，在 `window:beforeClose` 的处理函数里调用。只有以 `beforeClose: true` 创建的 popup 会收到这个事件；它保持打开，直到页面调用两者之一，3 秒内都没调用则照样关闭。

```javascript
// 页面自己的保存逻辑，这里只是占位。
async function saveChanges() {}

fb.on('window:beforeClose', async () => {
    const save = confirm('是否保存更改？');
    if (save) {
        await saveChanges();
        await fb.ui.confirmClose();
    } else {
        await fb.ui.cancelClose();
    }
});
```

## 开发服务器

### getDevServerConfig() / setDevServerConfig(opts)

获取/设置开发服务器配置：页面是否从开发服务器加载（`useDevServer`），以及开发服务器的地址（`devServerUrl`）。`setDevServerConfig` 省略的键保持原值，新设置在下次加载页面时生效。

签名：`fb.ui.getDevServerConfig(): Promise<WindowGetDevServerConfigResponse>`

签名：`fb.ui.setDevServerConfig(opts: WindowSetDevServerConfigParams): Promise<WindowSetDevServerConfigResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetDevServerConfigParams | 是 | 开发服务器配置：`useDevServer` 与 `devServerUrl`，都可省略 |

```javascript
const res = await fb.ui.getDevServerConfig();
if (res.success === false) throw new Error(res.error);
const { useDevServer, devServerUrl } = res;
await fb.ui.setDevServerConfig({ useDevServer: true, devServerUrl: 'http://localhost:5173' });
```
