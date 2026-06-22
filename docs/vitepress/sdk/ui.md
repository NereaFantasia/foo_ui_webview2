# fb.ui 窗口控制 

`fb.ui` 封装了 `window.*` 和 `ui.*` 底层 API，提供窗口管理的完整能力。

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

切换最大化状态。

```javascript
await fb.ui.toggleMaximize();
```

### setTitle(title) 

设置窗口标题。

```javascript
await fb.ui.setTitle('Now Playing: Let It Be');
```

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

开始调整窗口大小。`edge` 为调整方向。

## 状态查询 

### getState() 

获取窗口完整状态。

```javascript
const state = await fb.ui.getState();
// {maximized, minimized, fullscreen, alwaysOnTop, focused, width, height, x, y}
```

### isMaximized() 

```javascript
const r = await fb.ui.isMaximized(); // {isMaximized: true}
```

### isMinimized() 

```javascript
const r = await fb.ui.isMinimized(); // {isMinimized: false}
```

### isFullscreen() 

```javascript
const r = await fb.ui.isFullscreen(); // {isFullscreen: false}
```

### isAlwaysOnTop() 

```javascript
const r = await fb.ui.isAlwaysOnTop(); // {isAlwaysOnTop: false}
```

### isResizable() 

```javascript
const r = await fb.ui.isResizable(); // {isResizable: true}
```

### getTitle() 

获取当前窗口标题。

### getMode() 

获取窗口模式（如 `'normal'`、`'popup'` 等）。

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

### setBounds(opts) 

一次性设置窗口位置和大小。

```javascript
await fb.ui.setBounds({ x: 100, y: 100, width: 1280, height: 720 });
```

### center() 

将窗口居中到屏幕。

### hasSavedBounds() 

检查是否有保存的窗口位置信息。

## 尺寸约束 

### setMinSize(width, height) / getMinSize() 

设置/获取窗口最小尺寸。

```javascript
await fb.ui.setMinSize(400, 300);
const min = await fb.ui.getMinSize(); // {width, height}
```

### setMaxSize(width, height) / getMaxSize() 

设置/获取窗口最大尺寸。

### setResizable(resizable) 

设置窗口是否可调整大小。

## 置顶 

### setAlwaysOnTop(enabled) 

设置窗口置顶。

```javascript
await fb.ui.setAlwaysOnTop(true);
```

### toggleAlwaysOnTop() 

切换置顶状态。

## 全屏 

### toggleFullscreen() 

切换全屏模式。

### enterFullscreen() / exitFullscreen() 

进入/退出全屏。

### setFullscreen(enabled) 

设置全屏状态。

```javascript
await fb.ui.setFullscreen(true);
```

## 焦点与通知 

### focus() / blur() 

聚焦/失焦窗口。

### flash(opts) 

闪烁窗口。

### flashTaskbar(count?) 

闪烁任务栏按钮。`count` 可选，指定闪烁次数。

```javascript
await fb.ui.flashTaskbar();
await fb.ui.flashTaskbar(3); // 闪烁 3 次
```

### showSystemMenu(x, y, w?, h?) 

在指定位置显示系统菜单。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| x | number | ✓ | 屏幕 X 坐标 |
| y | number | ✓ | 屏幕 Y 坐标 |
| w | number | ✗ | 标题栏区域宽度 |
| h | number | ✗ | 标题栏区域高度 |

### showContextMenu(x?, y?) 

显示右键上下文菜单。不传参数时在鼠标位置弹出。

## DPI / 缩放 

### getDpiScale() 

获取 DPI 缩放信息。

```javascript
const { dpi, scale } = await fb.ui.getDpiScale();
```

### setZoom(zoom) / getZoom() / resetZoom() 

设置/获取/重置页面缩放级别。

```javascript
await fb.ui.setZoom(1.5); // 150%
const { zoom } = await fb.ui.getZoom();
await fb.ui.resetZoom();
```

### setZoomForDpi(dpi?) 

根据 DPI 自动设置缩放。

### setMica(opts) / setMicaEffect(opts) 

启用 Mica 材质效果。

```javascript
await fb.ui.setMica({ enabled: true });
await fb.ui.setMicaEffect({ type: 'mica-alt' });
```

### setAcrylic(opts) 

启用 Acrylic 亚克力效果。

```javascript
await fb.ui.setAcrylic({ enabled: true, tintColor: '#000000', tintOpacity: 0.5 });
```

### setBlur(opts) 

启用模糊效果。

### setDarkMode(enabled) 

设置深色模式。

```javascript
await fb.ui.setDarkMode(true);
```

### setBackgroundTransparency(opts) 

设置背景透明度。

### refreshWebView() 

刷新 WebView 渲染（DWM 特效切换后可能需要）。

### setCornerPreference(mode) / getCornerPreference() 

设置/获取窗口圆角模式（Windows 11）。

## 标题栏 

### getTitlebarHeight() / setTitlebarHeight(height) 

获取/设置自定义标题栏高度。

```javascript
const { height } = await fb.ui.getTitlebarHeight();
await fb.ui.setTitlebarHeight(32);
```

### getCaptionButtonsWidth() 

获取标题栏按钮（最小化/最大化/关闭）的总宽度。

### getTitlebarInfo() 

获取标题栏完整信息。

### setDragRegions(regions) / clearDragRegions() 

设置/清除可拖拽区域（用于无边框窗口的自定义标题栏）。

```javascript
await fb.ui.setDragRegions([
    { x: 0, y: 0, width: 800, height: 32 }
]);
await fb.ui.clearDragRegions();
```

### setNoDragRegions(regions) / clearNoDragRegions() 

设置/清除不可拖拽区域（在拖拽区域内排除某些元素）。

### setFrameless(frameless) 

设置无边框模式。

## 多窗口管理 

`fb.ui` 已完整封装多窗口 API，无需直接调用底层 `window.*` 方法。

### createPopup(opts) 

创建弹出窗口。每个 popup 拥有独立的 BridgeCore + WebView2 实例。

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
        // 覆盖 desktopLyrics 默认中需要调整的字段
        keepVisibleOnShowDesktop: true,
        allowMinimize: false,
    },
    backdropPolicy: {
        activeEffect: 'none',
        inactiveEffect: 'none',
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

关闭指定弹出窗口或所有弹出窗口。

### getAllWindows() 

获取所有窗口列表。

### getCurrentWindowId() 

获取当前窗口 ID。

### getPopupBehavior(windowId?) / setPopupBehavior(opts) 

获取/设置弹出窗口行为。

### getBackdropPolicy(windowId?) / setBackdropPolicy(opts) 

获取/设置窗口背景策略。

### setClickThrough(opts) / isClickThrough(windowId?) 

设置/查询窗口点击穿透。

### sendMessage(targetWindowId, message) 

向指定窗口发送消息。

```javascript
await fb.ui.sendMessage('main', { type: 'theme-changed', dark: true });
```

### broadcast(message) 

向所有窗口广播消息。

```javascript
await fb.ui.broadcast({ type: 'config-updated' });
```

### cancelClose() / confirmClose() 

异步关闭确认。在 `window:beforeClose` 事件中使用。

```javascript
fb2k.on('window:beforeClose', async () => {
    const save = confirm('是否保存更改？');
    if (save) {
        await saveChanges();
        fb.ui.confirmClose();
    } else {
        fb.ui.cancelClose();
    }
});
```

## 开发服务器 

### getDevServerConfig() / setDevServerConfig(opts) 

获取/设置开发服务器配置（仅开发模式）。

## 补全方法参考

本节补齐 `fb.ui` 中此前仅在分组标题或正文中提到、但缺少独立 SDK 示例调用的方法。`fb.ui` 是 SDK 视角的窗口门面，内部多数方法调用底层 `window.*` API。

### blur()

签名：`fb.ui.blur(): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：窗口失焦操作结果。

```javascript
await fb.ui.blur();
```

### center()

签名：`fb.ui.center(): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：窗口居中操作结果。

```javascript
await fb.ui.center();
```

### clearClickThroughExcludeRegions(windowId?)

签名：`fb.ui.clearClickThroughExcludeRegions(windowId?: string): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | 目标窗口 ID；省略时作用于当前窗口 |

返回值：清除点击穿透排除区域的操作结果。

```javascript
await fb.ui.clearClickThroughExcludeRegions();
```

### clearNoDragRegions()

签名：`fb.ui.clearNoDragRegions(): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：清除不可拖拽区域的操作结果。

```javascript
await fb.ui.clearNoDragRegions();
```

### closeAllPopups()

签名：`fb.ui.closeAllPopups(): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：关闭所有 popup 的操作结果。

```javascript
await fb.ui.closeAllPopups();
```

### closePopup(windowId)

签名：`fb.ui.closePopup(windowId: string): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 是 | 要关闭的 popup 窗口 ID |

返回值：关闭指定 popup 的操作结果。

```javascript
await fb.ui.closePopup('popup-settings');
```

### enterFullscreen()

签名：`fb.ui.enterFullscreen(): Promise<BaseResponse & { isFullscreen?: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：进入全屏后的操作结果，可能包含 `isFullscreen`。

```javascript
await fb.ui.enterFullscreen();
```

### exitFullscreen()

签名：`fb.ui.exitFullscreen(): Promise<BaseResponse & { isFullscreen?: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：退出全屏后的操作结果，可能包含 `isFullscreen`。

```javascript
await fb.ui.exitFullscreen();
```

### flash(opts)

签名：`fb.ui.flash(opts: WindowFlashParams): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowFlashParams | 是 | 闪烁窗口的选项，如次数、间隔或目标窗口 |

返回值：窗口闪烁操作结果。

```javascript
await fb.ui.flash({ count: 3 });
```

### focus(windowId?)

签名：`fb.ui.focus(windowId?: string): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | 目标窗口 ID；省略时聚焦当前窗口 |

返回值：窗口聚焦操作结果。

```javascript
await fb.ui.focus();
```

### getAllWindows()

签名：`fb.ui.getAllWindows(): Promise<WindowListResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：所有已知窗口的列表。

```javascript
const windows = await fb.ui.getAllWindows();
```

### getBackdropPolicy(windowId?)

签名：`fb.ui.getBackdropPolicy(windowId?: string): Promise<WindowBackdropPolicyState>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | 目标窗口 ID；省略时查询当前窗口 |

返回值：窗口背景策略状态。

```javascript
const policy = await fb.ui.getBackdropPolicy();
```

### getBounds()

签名：`fb.ui.getBounds(): Promise<WindowBounds>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：窗口边界 `{ x, y, width, height }`。

```javascript
const bounds = await fb.ui.getBounds();
```

### getCaptionButtonsWidth()

签名：`fb.ui.getCaptionButtonsWidth(): Promise<{ width: number }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：标题栏系统按钮区域宽度。

```javascript
const { width } = await fb.ui.getCaptionButtonsWidth();
```

### getCornerPreference()

签名：`fb.ui.getCornerPreference(): Promise<{ mode: string }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：当前窗口圆角偏好模式。

```javascript
const { mode } = await fb.ui.getCornerPreference();
```

### getCurrentWindowId()

签名：`fb.ui.getCurrentWindowId(): Promise<{ windowId: string }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：当前窗口 ID。

```javascript
const { windowId } = await fb.ui.getCurrentWindowId();
```

### getDevServerConfig()

签名：`fb.ui.getDevServerConfig(): Promise<WindowDevServerConfig>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：开发服务器配置。

```javascript
const devServer = await fb.ui.getDevServerConfig();
```

### getMaxSize()

签名：`fb.ui.getMaxSize(): Promise<{ width: number; height: number }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：窗口最大尺寸约束。

```javascript
const maxSize = await fb.ui.getMaxSize();
```

### getMode()

签名：`fb.ui.getMode(): Promise<{ mode: string }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：当前窗口模式。

```javascript
const { mode } = await fb.ui.getMode();
```

### getPopupBehavior(windowId?)

签名：`fb.ui.getPopupBehavior(windowId?: string): Promise<WindowPopupBehaviorState>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | popup 窗口 ID；省略时查询当前窗口 |

返回值：popup 行为策略。

```javascript
const behavior = await fb.ui.getPopupBehavior('popup-settings');
```

### getTitle()

签名：`fb.ui.getTitle(): Promise<{ title: string }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：当前窗口标题。

```javascript
const { title } = await fb.ui.getTitle();
```

### getTitlebarInfo()

签名：`fb.ui.getTitlebarInfo(): Promise<WindowTitlebarInfo>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：标题栏高度、按钮宽度和相关布局信息。

```javascript
const titlebar = await fb.ui.getTitlebarInfo();
```

### hasSavedBounds()

签名：`fb.ui.hasSavedBounds(): Promise<{ hasSavedBounds: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：是否存在已保存窗口位置。

```javascript
const { hasSavedBounds } = await fb.ui.hasSavedBounds();
```

### isClickThrough(windowId?)

签名：`fb.ui.isClickThrough(windowId?: string): Promise<{ clickThrough: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | 目标窗口 ID；省略时查询当前窗口 |

返回值：窗口是否处于点击穿透状态。

```javascript
const { clickThrough } = await fb.ui.isClickThrough();
```

### refreshWebView()

签名：`fb.ui.refreshWebView(): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：刷新 WebView 的操作结果。

```javascript
await fb.ui.refreshWebView();
```

### setBackgroundTransparency(opts)

签名：`fb.ui.setBackgroundTransparency(opts: WindowSetBackgroundTransparencyParams): Promise<BaseResponse & { description?: string }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetBackgroundTransparencyParams | 是 | 背景透明度配置 |

返回值：设置结果，可能包含效果说明。

```javascript
await fb.ui.setBackgroundTransparency({ enabled: true, opacity: 0.85 });
```

### setBlur(opts)

签名：`fb.ui.setBlur(opts: WindowSetBlurParams): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetBlurParams | 是 | Blur 效果配置 |

返回值：设置 Blur 的操作结果。

```javascript
await fb.ui.setBlur({ enabled: true });
```

### setClickThrough(opts)

签名：`fb.ui.setClickThrough(opts: WindowSetClickThroughParams): Promise<BaseResponse & { clickThrough?: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetClickThroughParams | 是 | 点击穿透配置 |

返回值：设置结果，可能包含当前点击穿透状态。

```javascript
await fb.ui.setClickThrough({ enabled: true });
```

### setClickThroughExcludeRegions(opts)

签名：`fb.ui.setClickThroughExcludeRegions(opts: WindowSetClickThroughExcludeRegionsParams): Promise<BaseResponse & { count?: number; dpiScale?: number; warning?: string }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetClickThroughExcludeRegionsParams | 是 | 点击穿透排除区域配置 |

返回值：设置结果，可能包含区域数量、DPI 缩放或警告。

```javascript
await fb.ui.setClickThroughExcludeRegions({
    regions: [{ x: 0, y: 0, width: 320, height: 80 }],
});
```

### setCornerPreference(mode)

签名：`fb.ui.setCornerPreference(mode: string): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| mode | string | 是 | Windows 11 圆角偏好，如 `default`、`round`、`square` |

返回值：设置圆角偏好的操作结果。

```javascript
await fb.ui.setCornerPreference('round');
```

### setDevServerConfig(opts)

签名：`fb.ui.setDevServerConfig(opts: WindowSetDevServerConfigParams): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| opts | WindowSetDevServerConfigParams | 是 | 开发服务器配置 |

返回值：保存开发服务器配置的操作结果。

```javascript
await fb.ui.setDevServerConfig({ enabled: true, url: 'http://localhost:5173' });
```

### setFrameless(frameless)

签名：`fb.ui.setFrameless(frameless: boolean): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| frameless | boolean | 是 | 是否启用无边框窗口 |

返回值：设置无边框模式的操作结果。

```javascript
await fb.ui.setFrameless(true);
```

### setMaxSize(width, height)

签名：`fb.ui.setMaxSize(width: number, height: number): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| width | number | 是 | 最大宽度 |
| height | number | 是 | 最大高度 |

返回值：设置最大尺寸的操作结果。

```javascript
await fb.ui.setMaxSize(1920, 1080);
```

### setNoDragRegions(regions)

签名：`fb.ui.setNoDragRegions(regions: unknown[]): Promise<BaseResponse & { count?: number; dpiScale?: number }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| regions | unknown[] | 是 | 不可拖拽区域数组 |

返回值：设置结果，可能包含区域数量和 DPI 缩放。

```javascript
await fb.ui.setNoDragRegions([{ x: 720, y: 0, width: 80, height: 32 }]);
```

### setResizable(resizable)

签名：`fb.ui.setResizable(resizable: boolean): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| resizable | boolean | 是 | 是否允许用户调整窗口大小 |

返回值：设置可调整大小状态的操作结果。

```javascript
await fb.ui.setResizable(false);
```

### setZoomForDpi(dpi?)

签名：`fb.ui.setZoomForDpi(dpi?: number): Promise<BaseResponse & { zoom?: number }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| dpi | number | 否 | 指定 DPI；省略时使用当前窗口 DPI |

返回值：设置结果，可能包含最终 zoom。

```javascript
await fb.ui.setZoomForDpi();
```

### showContextMenu(x?, y?)

签名：`fb.ui.showContextMenu(x?: number, y?: number): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| x | number | 否 | 屏幕 X 坐标 |
| y | number | 否 | 屏幕 Y 坐标 |

返回值：显示上下文菜单的操作结果。

```javascript
await fb.ui.showContextMenu();
```

### showSystemMenu(x, y, w?, h?)

签名：`fb.ui.showSystemMenu(x: number, y: number, w?: number, h?: number): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| x | number | 是 | 屏幕 X 坐标 |
| y | number | 是 | 屏幕 Y 坐标 |
| w | number | 否 | 标题栏区域宽度 |
| h | number | 否 | 标题栏区域高度 |

返回值：显示系统菜单的操作结果。

```javascript
await fb.ui.showSystemMenu(100, 32);
```

### startResize(edge)

签名：`fb.ui.startResize(edge: string): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| edge | string | 是 | 调整方向，如 `left`、`right`、`top`、`bottom` |

返回值：开始系统级窗口 resize 的操作结果。

```javascript
await fb.ui.startResize('right');
```

### toggleAlwaysOnTop()

签名：`fb.ui.toggleAlwaysOnTop(): Promise<BaseResponse & { enabled?: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：切换置顶后的操作结果，可能包含 `enabled`。

```javascript
await fb.ui.toggleAlwaysOnTop();
```

### toggleFullscreen()

签名：`fb.ui.toggleFullscreen(): Promise<BaseResponse & { fullscreen?: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：切换全屏后的操作结果，可能包含 `fullscreen`。

```javascript
await fb.ui.toggleFullscreen();
```
