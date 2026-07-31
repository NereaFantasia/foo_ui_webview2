# Window 窗口

## window.toggleMaximize

切换最大化状态。
- **参数**: 无

**返回值**:

```json
{ "success": true, "maximized": true }
```

```javascript
const result = await fb2k.invoke('window.toggleMaximize');
console.log(result.maximized ? '已最大化' : '已还原');
```

## window.getState

获取窗口状态。

- **参数**: 无

**返回值**:

```json
{
    "maximized": false,
    "minimized": false,
    "fullscreen": false,
    "alwaysOnTop": false,
    "focused": true,
    "isMaximized": false,
    "isMinimized": false,
    "isFullscreen": false,
    "isAlwaysOnTop": false,
    "isFocused": true,
    "width": 1280,
    "height": 720,
    "x": 100,
    "y": 100
}
```

> 同时返回 `maximized` / `isMaximized` 等两种命名风格，方便前端按习惯选用。

```javascript
const state = await fb2k.invoke('window.getState');
if (state.isMaximized) {
    console.log(`窗口已最大化，尺寸: ${state.width}x${state.height}`);
}
```

## window.isMaximized

获取窗口最大化状态。

- **参数**: 无
- **返回值**: `{ "maximized": false, "isMaximized": false }`

## window.isMinimized

获取窗口最小化状态。

- **参数**: 无
- **返回值**: `{ "minimized": false }`

## window.isFullscreen

获取窗口全屏状态。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选目标窗口 id，由 `WindowTargetResolver::ResolveForObservation` 解析；省略时使用调用方窗口。 |

- **返回值**: `{ "fullscreen": false, "isFullscreen": false }`

## window.setFullscreen


<!-- phase3-major1-review:window.setFullscreen -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:899-921`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |
| `enabled` | `boolean` | 否 | `true` |

**返回字段（按变体取值）**: `error`, `fullscreen`, `success`；`fullscreen`, `success`

**语义**: The mutation resolver chooses explicit windowId or caller. enabled enters and false exits fullscreen only on shells advertising fullscreen capability; panel mode is rejected.

<!-- phase3-major1-review-end:window.setFullscreen --> 设置全屏模式。采用 Chromium 风格的全屏实现，保存/恢复窗口状态。

主窗口与 popup 进入/退出 fullscreen 后，都会重新通过统一的 window chrome resolver/applier 应用当前 `backdropPolicy` / frameless / darkMode 状态。默认作用于当前调用窗口；如需显式指定目标窗口，可传 `windowId`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |

- **返回值**: `{ "success": true, "fullscreen": true }`

```javascript
await fb2k.invoke('window.setFullscreen', { enabled: true });
// 退出全屏
await fb2k.invoke('window.setFullscreen', { enabled: false });
```

## window.setBounds


<!-- phase3-major1-review:window.setBounds -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:761-789`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `x` | `integer` | 否 | `current x` |
| `y` | `integer` | 否 | `current y` |
| `width` | `integer` | 否 | `current width` |
| `height` | `integer` | 否 | `current height` |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: Only supplied fields are read as integers; omitted coordinates and dimensions retain the current window rectangle. The method is unsupported in panel mode and operates only on the caller window.

<!-- phase3-major1-review-end:window.setBounds --> 设置窗口边界。所有参数可选，未传递的保持不变。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `x` | `integer` | 否 | 可选；默认 current x。 |
| `y` | `integer` | 否 | 可选；默认 current y。 |
| `width` | `integer` | 否 | 可选；默认 current width。 |
| `height` | `integer` | 否 | 可选；默认 current height。 |

- **返回值**: `{ "success": true }`

```javascript
// 只改大小不改位置
await fb2k.invoke('window.setBounds', { width: 1920, height: 1080 });
// 只改位置不改大小
await fb2k.invoke('window.setBounds', { x: 0, y: 0 });
```

## window.hasSavedBounds

检查是否有上次会话保存的窗口位置。前端可根据此决定是否设置默认窗口大小。

- **参数**: 无
- **返回值**: `{ "hasSavedBounds": true, "description": "Window has saved position from previous session" }`

## window.getDpiScale

获取 DPI 缩放信息。

- **参数**: 无
**返回值**: `{"dpi":"...","scale":"...","success":true}`

```javascript
const dpi = await fb2k.invoke('window.getDpiScale');
console.log(`DPI: ${dpi.dpi}, 缩放: ${dpi.scale}x`);
```

## window.focus

使窗口获得焦点。支持可选的 `windowId` 参数，可从任意窗口聚焦指定的主窗口或弹窗。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 。 |

- **返回值**: `{ "success": true }`
- **行为**: 若目标窗口处于最小化状态，会先恢复再置顶激活

```javascript
// 聚焦调用者自身窗口
await fb2k.window.focus();

// 从弹窗聚焦主窗口
await fb2k.window.focus('main');

// 从主窗口聚焦指定弹窗
await fb2k.window.focus('my-popup-id');
```

## window.blur

移除窗口焦点（激活下一个窗口）。

- **参数**: 无
- **返回值**: `{ "success": true }`

## window.getTitle

获取窗口标题。

- **参数**: 无
- **返回值**: `{ "title": "foobar2000" }`

```javascript
const result = await fb2k.invoke('window.getTitle');
console.log('窗口标题:', result.title);
```

## window.showSystemMenu
 显示系统菜单（最小化/最大化/关闭等）。支持传入排除区域避免遮挡按钮。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `h` | `integer` | 否 | 可选；默认 0。 |
| `w` | `integer` | 否 | 可选；默认 0。 |
| `x` | `integer` | 否 | 可选；默认 0。 |
| `y` | `integer` | 否 | 可选；默认 0。 |

- **返回值**: `{ "success": true }`

```javascript
// 在自定义标题栏按钮旁显示系统菜单，避免遮挡按钮
const btn = document.querySelector('.title-bar-icon');
const rect = btn.getBoundingClientRect();
await fb2k.invoke('window.showSystemMenu', {
    x: rect.left, y: rect.top,
    w: rect.width, h: rect.height
});
```

## window.startDrag

 开始拖拽窗口。通常在自定义标题栏的 `mousedown` 事件中调用。
- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
titlebar.addEventListener('mousedown', async () => {
    await fb2k.invoke('window.startDrag');
});
```

## window.setTitlebarHeight
 设置**当前调用窗口**（`main` 或 `popup`）的标题栏高度。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `height` | `integer` | 否 | 可选；默认 32。 |

- **返回值**: `{ "success": true, "height": 32 }`

::: warning WARNING
高度必须在 24-100 之间，超出范围返回错误。
:::

::: tip TIP
从 popup 调用仅影响该 popup，不会污染主窗口标题栏配置。
:::

## window.getTitlebarInfo

获取标题栏完整信息（当前为主窗口信息，兼容行为）。

- **参数**: 无

**返回值**:

```json
{
    "height": 32,
    "captionButtonsWidth": 138,
    "captionButtonWidth": 46,
    "isMaximized": false
}
```

## window.getCaptionButtonsWidth

获取系统标题栏按钮宽度（当前为主窗口信息，兼容行为）。

- **参数**: 无
- **返回值**: `{ "width": 138, "buttonWidth": 46 }`

## window.setDragRegions

 设置**当前调用窗口**（`main` 或 `popup`）的可拖拽区域。CSS 像素会自动根据 DPI 缩放转为物理像素。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `regions` | `array` | 否 | 可选；默认 omitted。 |

- **返回值**: `{ "success": true, "count": 2, "dpiScale": 1.5 }`

```javascript
await fb2k.invoke('window.setDragRegions', {
    regions: [
        { x: 0, y: 0, width: 800, height: 32 }
    ]
});
```

## window.clearDragRegions



**返回值**: `{"success":true}`

## window.getPopupBehavior

获取弹出窗口（popup）的行为策略。仅对 popup 窗口生效，不支持 main 窗口。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 。 |

**返回值**:

```json
{
    "success": true,
    "windowId": "popup-1",
    "profile": "floating",
    "behavior": {
        "showInTaskbar": false,
        "showInAltTab": false
    },
    "resolvedBehavior": {
        "showInTaskbar": false,
        "showInAltTab": false
    }
}
```

字段说明：

- `profile` — 当前 popup 应用的预设档（如 `floating` / `tool` / `dialog` 等，详见 `WindowPopupProfile`）
- `behavior` — 通过 `setPopupBehavior` 显式设置的字段（部分字段，未设置的为 undefined）
- `resolvedBehavior` — profile 默认 + behavior 覆盖后的最终生效值

::: warning 仅 popup
对 main 窗口调用会返回 `{ success: false, error: "window.getPopupBehavior does not support main window" }`。
:::

```javascript
const info = await fb2k.invoke('window.getPopupBehavior');
console.log(info.resolvedBehavior.showInTaskbar);
```

## window.setPopupBehavior


<!-- phase3-major1-review:window.setPopupBehavior -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:1978-2011`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller popup` |
| `profile` | `string` | 否 | `leave profile unchanged` |
| `behavior` | `object` | 否 | `no behavior patch` |

**返回值**: `{"error":"...","success":true,"windowId":"..."}`

**语义**: Explicit windowId must name a popup rather than main; without it the caller must resolve to a popup. profile and behavior are independently presence-sensitive and are validated by UpdatePopupBehavior.

<!-- phase3-major1-review-end:window.setPopupBehavior -->
运行时更新 popup 窗口的行为策略。可一次切换 profile，也可通过 `behavior` 做字段级覆盖。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller popup。 |
| `profile` | `string` | 否 | 可选；默认 leave profile unchanged。 |
| `behavior` | `object` | 否 | 可选；默认 no behavior patch。 |

字段优先级：`behavior.*` 字段覆盖 > `profile` 默认。


::: warning 仅 popup
不支持 main 窗口；调用会返回 `{ success: false, error: "window.setPopupBehavior does not support main window" }`。
:::

```javascript
// 仅切换 profile
await fb2k.invoke('window.setPopupBehavior', { profile: 'tool' });

// 字段级覆盖（保留当前 profile）
await fb2k.invoke('window.setPopupBehavior', {
    behavior: { showInTaskbar: true, showInAltTab: true }
});

// 清空字段，恢复 profile 默认
await fb2k.invoke('window.setPopupBehavior', {
    behavior: { showInTaskbar: null }
});
```

## window.getBackdropPolicy


<!-- phase3-major1-review:window.getBackdropPolicy -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:2014-2023`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |

**返回字段（按变体取值）**: `error`, `success`；`backdropPolicy`, `resolvedBackdropPolicy`, `success`, `windowId`

**语义**: Observation resolution selects the explicit windowId or caller. The response is the shell’s backdrop-policy object augmented with success and resolved windowId, including profile/default-resolved policy fields.

<!-- phase3-major1-review-end:window.getBackdropPolicy -->
获取窗口的 DWM 背景效果策略。支持 main 与 popup。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |

**返回值**:

```json
{
    "success": true,
    "windowId": "main",
    "backdropPolicy": {
        "activeEffect": "mica",
        "inactiveEffect": "system"
    },
    "resolvedBackdropPolicy": {
        "activeEffect": "mica",
        "inactiveEffect": "system",
        "darkMode": true,
        "frameless": false
    }
}
```

字段说明：

- `backdropPolicy` — 调用方此前 `setBackdropPolicy` 显式设置过的字段（部分）
- `resolvedBackdropPolicy` — 系统/profile 默认 + 显式覆盖后的最终生效值
- `activeEffect` 取值：`inherit` \| `system` \| `none` \| `mica` \| `acrylic`
- `inactiveEffect` 取值：`system` \| `none` \| `transparent` 等（详见 `WindowInactiveBackdropEffect` 类型）

```javascript
const info = await fb2k.invoke('window.getBackdropPolicy');
console.log(info.resolvedBackdropPolicy.activeEffect); // 'mica'
```

## window.setBackdropPolicy


<!-- phase3-major1-review:window.setBackdropPolicy -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:2026-2047`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |
| `backdropPolicy` | `object` | 是 | 无 |

**返回字段（按变体取值）**: `error`, `success`；`backdropPolicy`, `resolvedBackdropPolicy`, `success`, `windowId`

**语义**: backdropPolicy must be present and an object before target resolution. The shell validates/applies the patch; the successful response returns the updated policy info plus success and windowId.

<!-- phase3-major1-review-end:window.setBackdropPolicy -->
运行时更新 DWM 背景策略。可单独覆盖 `activeEffect` / `inactiveEffect` 等字段；传入 `null` 表示清空字段恢复默认。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `backdropPolicy` | `object` | 是 | 必填。 |

**返回值**: 同 `getBackdropPolicy`，附 `success: true`；失败时返回 `error`。

::: warning 必填字段
`backdropPolicy` 是必填字段且必须为 object，否则返回 `{ success: false, error: "backdropPolicy is required" }` 或 `"backdropPolicy must be an object"`。
:::

```javascript
// 切换主窗口为 acrylic
await fb2k.invoke('window.setBackdropPolicy', {
    backdropPolicy: { activeEffect: 'acrylic' }
});

// 同时改 active 与 inactive
await fb2k.invoke('window.setBackdropPolicy', {
    backdropPolicy: {
        activeEffect: 'mica',
        inactiveEffect: 'system'
    }
});

// 清空 activeEffect 恢复默认
await fb2k.invoke('window.setBackdropPolicy', {
    backdropPolicy: { activeEffect: null }
});
```

相关类型定义见 SDK：[`WindowBackdropPolicyPatch`](../sdk/ui#dwm-backdrop) / [`WindowPopupBehaviorPatch`](../sdk/ui#popup-behavior)。

## 补充的公开 API

以下章节按 `RegisterApi` 动态补齐，参数键来自 C++ handler 静态提取。

### window.broadcast


<!-- phase3-major1-review:window.broadcast -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:2245-2256`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `message` | `json` | 是 | 无 |

**返回字段（按变体取值）**: `error`, `success`；`success`

**语义**: message must be present but may be any JSON value. The handler identifies the caller window, broadcasts to the other managed windows, and returns no recipient count.

<!-- phase3-major1-review-end:window.broadcast -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2468`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `json` | 是 | 必填。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.broadcast', { message: /* value */ });
```


### window.cancelClose

公开 API。运行时权威：`src/api/WindowApi.cpp:2460`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.cancelClose');
```


### window.center

公开 API。运行时权威：`src/api/WindowApi.cpp:2401`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.center');
```


### window.clearClickThroughExcludeRegions

公开 API。运行时权威：`src/api/WindowApi.cpp:2466`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"error":"...","success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.clearClickThroughExcludeRegions', { windowId: /* value */ });
```


### window.clearNoDragRegions

公开 API。运行时权威：`src/api/WindowApi.cpp:2422`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.clearNoDragRegions');
```


### window.close

公开 API。运行时权威：`src/api/WindowApi.cpp:2388`。

_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('window.close');
```


### window.closeAllPopups

公开 API。运行时权威：`src/api/WindowApi.cpp:2453`。

_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('window.closeAllPopups');
```


### window.closePopup

公开 API。运行时权威：`src/api/WindowApi.cpp:2452`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.closePopup', { windowId: /* value */ });
```


### window.confirmClose

公开 API。运行时权威：`src/api/WindowApi.cpp:2461`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.confirmClose');
```


### window.createPopup

公开 API。运行时权威：`src/api/WindowApi.cpp:2451`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `alwaysOnTop` | `boolean` | 否 | 可选；默认 false。 |
| `backdropPolicy` | `object` | 否 | 可选；默认 omitted。 |
| `beforeClose` | `boolean` | 否 | 可选；默认 false。 |
| `behavior` | `object` | 否 | 可选；默认 omitted。 |
| `clickThrough` | `boolean` | 否 | 可选；默认 false。 |
| `frame` | `boolean` | 否 | 可选；默认 true。 |
| `height` | `integer` | 否 | 可选；默认 300。 |
| `maxHeight` | `integer` | 否 | 可选；默认 0。 |
| `maxWidth` | `integer` | 否 | 可选；默认 0。 |
| `minHeight` | `integer` | 否 | 可选；默认 150。 |
| `minWidth` | `integer` | 否 | 可选；默认 200。 |
| `profile` | `string` | 否 | 可选；默认 。 |
| `resizable` | `boolean` | 否 | 可选；默认 true。 |
| `showInTaskbar` | `boolean` | 否 | 可选；默认 false。 |
| `title` | `string` | 否 | 可选；默认 。 |
| `transparent` | `boolean` | 否 | 可选；默认 false。 |
| `url` | `string` | 否 | 可选；默认 。 |
| `width` | `integer` | 否 | 可选；默认 400。 |
| `x` | `integer` | 否 | 可选；默认 CW_USEDEFAULT。 |
| `y` | `integer` | 否 | 可选；默认 CW_USEDEFAULT。 |

**返回值**: `{"error":"...","success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.createPopup', { alwaysOnTop: /* value */, backdropPolicy: /* value */, beforeClose: /* value */, behavior: /* value */, clickThrough: /* value */, frame: /* value */, height: /* value */, maxHeight: /* value */, maxWidth: /* value */, minHeight: /* value */, minWidth: /* value */, profile: /* value */, resizable: /* value */, showInTaskbar: /* value */, title: /* value */, transparent: /* value */, url: /* value */, width: /* value */, x: /* value */, y: /* value */ });
```


### window.enterFullscreen


<!-- phase3-major1-review:window.enterFullscreen -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:1538-1554`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |

**返回字段（按变体取值）**: `error`, `isFullscreen`, `success`；`success`；`isFullscreen`, `success`

**语义**: The resolver targets windowId when supplied or the caller otherwise. Panel mode and shells without fullscreen capability return errors; already-fullscreen calls return success:false without changing state.

<!-- phase3-major1-review-end:window.enterFullscreen -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2434`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |

**返回值**: `{"error":"...","isFullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.enterFullscreen');
```


### window.exitFullscreen


<!-- phase3-major1-review:window.exitFullscreen -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:1554-1570`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |

**返回字段（按变体取值）**: `error`, `isFullscreen`, `success`；`success`；`isFullscreen`, `success`

**语义**: The resolver targets windowId when supplied or the caller otherwise. Panel mode and unsupported shells fail; a non-fullscreen target returns success:false without restoring geometry.

<!-- phase3-major1-review-end:window.exitFullscreen -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2435`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |

**返回值**: `{"error":"...","isFullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.exitFullscreen');
```


### window.flash

公开 API。运行时权威：`src/api/WindowApi.cpp:2414`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `count` | `integer` | 否 | 可选；默认 3。 |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.flash', { count: /* value */, enabled: /* value */ });
```


### window.flashTaskbar

公开 API。运行时权威：`src/api/WindowApi.cpp:2433`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `count` | `integer` | 否 | 可选；默认 3。 |

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('window.flashTaskbar', { count: /* value */ });
```


### window.getAllWindows

公开 API。运行时权威：`src/api/WindowApi.cpp:2454`。

_无参数。_

**返回值**: `{"items":"...","success":true}`

```js
const result = await fb2k.invoke('window.getAllWindows');
```


### window.getBounds

公开 API。运行时权威：`src/api/WindowApi.cpp:2399`。

_无参数。_

**返回值**: `{"height":"...","width":"...","x":"...","y":"..."}`

```js
const result = await fb2k.invoke('window.getBounds');
```


### window.getCornerPreference


<!-- phase3-major1-review:window.getCornerPreference -->
#### 源码复核契约
权威：`src/api/WindowApi.cpp:1489-1495`。

_无参数。_ 此调用**仅作用于主窗口**：不接受 `windowId`，也忽略调用方窗口。

**返回键**：`mode`、`preference`

**语义**：返回主窗口的 Windows 11 圆角偏好；`mode` 与 `preference` 是同一个值的两个名字。popup 不暴露该设置——它自行管理圆角（无边框时圆角，否则用系统默认），并在能力中声明 `supportsCornerPreference: false`，故此调用没有 popup 作用域的等价形式。无主窗口时返回 `"default"` 而非错误。

<!-- phase3-major1-review-end:window.getCornerPreference -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2428`。

_无参数。_

**返回值**: `{"mode":"...","preference":"..."}`

```js
const result = await fb2k.invoke('window.getCornerPreference');
```


### window.getCurrentWindowId

公开 API。运行时权威：`src/api/WindowApi.cpp:2455`。

_无参数。_

**返回值**: `{"success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.getCurrentWindowId');
```


### window.getDevServerConfig

公开 API。运行时权威：`src/api/WindowApi.cpp:2444`。

_无参数。_

**返回值**: `{"devServerUrl":"...","success":true,"useDevServer":"..."}`

```js
const result = await fb2k.invoke('window.getDevServerConfig');
```


### window.getMaxSize


<!-- phase3-major1-review:window.getMaxSize -->
#### 源码复核契约
权威：`src/api/WindowApi.cpp:884-898`。

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `调用方窗口` |

**返回键（随响应变体而异）**：`error`、`success`；`height`、`width`、`windowId`

**语义**：观测型解析取显式 `windowId` 或调用方窗口，**不再回退主窗口**——无法解析调用上下文时直接失败，而不是静默返回另一个窗口的约束。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`，因为面板不是窗口 shell。数值单位为物理像素；宿主以 DIP 存储约束，并按**目标窗口**的 DPI 换算。`0` 表示无上限。返回的是**请求值**而非当前生效值，故 `set` 后 `get` 可无损往返。

<!-- phase3-major1-review-end:window.getMaxSize -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2405`。

_无参数。_

**返回值**: `{"height":"...","width":"..."}`

```js
const result = await fb2k.invoke('window.getMaxSize');
```


### window.getMinSize


<!-- phase3-major1-review:window.getMinSize -->
#### 源码复核契约
权威：`src/api/WindowApi.cpp:848-864`。

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `调用方窗口` |

**返回键（随响应变体而异）**：`error`、`success`；`height`、`width`、`windowId`

**语义**：观测型解析取显式 `windowId` 或调用方窗口，**不再回退主窗口**——无法解析调用上下文时直接失败，而不是静默返回另一个窗口的约束。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`，因为面板不是窗口 shell。数值单位为物理像素；宿主以 DIP 存储约束，并按**目标窗口**的 DPI 换算。返回的是**请求值**而非当前生效值，故 `set` 后 `get` 可无损往返。

<!-- phase3-major1-review-end:window.getMinSize -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2403`。

_无参数。_

**返回值**: `{"height":"...","width":"..."}`

```js
const result = await fb2k.invoke('window.getMinSize');
```


### window.getMode

公开 API。运行时权威：`src/api/WindowApi.cpp:2469`。

_无参数。_

**返回值**: `{"mode":"...","panel":"...","panelMode":"...","windowId":"..."}`

```js
const result = await fb2k.invoke('window.getMode');
```


### window.getTitlebarHeight

公开 API。运行时权威：`src/api/WindowApi.cpp:2416`。

_无参数。_

**返回值**: `{"height":"..."}`

```js
const result = await fb2k.invoke('window.getTitlebarHeight');
```


### window.getZoom

公开 API。运行时权威：`src/api/WindowApi.cpp:2448`。

_无参数。_

**返回值**: `{"dpi":"...","dpiScale":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.getZoom');
```


### window.isAlwaysOnTop

公开 API。运行时权威：`src/api/WindowApi.cpp:2397`。

_无参数。_

**返回值**: `{"enabled":"...","isAlwaysOnTop":"..."}`

```js
const result = await fb2k.invoke('window.isAlwaysOnTop');
```


### window.isClickThrough

公开 API。运行时权威：`src/api/WindowApi.cpp:2464`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"clickThrough":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('window.isClickThrough', { windowId: /* value */ });
```


### window.isResizable


<!-- phase3-major1-review:window.isResizable -->
#### 源码复核契约
权威：`src/api/WindowApi.cpp:924-933`。

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `调用方窗口` |

**返回键（随响应变体而异）**：`error`、`success`；`resizable`、`supportsRuntimeToggle`、`windowId`

**语义**：观测型解析取显式 `windowId` 或调用方窗口，**不再回退主窗口**。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。`supportsRuntimeToggle` 表示 `window.setResizable` 能否在运行时改变该窗口：完全无边框的 popup（`frame: false` 且 `transparent: true` 且无背景效果）没有可增删的可调整边框，故为 `false`。

<!-- phase3-major1-review-end:window.isResizable -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2407`。

_无参数。_

**返回值**: `{"resizable":"..."}`

```js
const result = await fb2k.invoke('window.isResizable');
```


### window.maximize

公开 API。运行时权威：`src/api/WindowApi.cpp:2386`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.maximize');
```


### window.minimize

公开 API。运行时权威：`src/api/WindowApi.cpp:2385`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.minimize');
```


### window.refreshWebView

公开 API。运行时权威：`src/api/WindowApi.cpp:2442`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.refreshWebView');
```


### window.reload

公开 API。运行时权威：`src/api/WindowApi.cpp:2443`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.reload');
```


### window.resetZoom

公开 API。运行时权威：`src/api/WindowApi.cpp:2449`。

_无参数。_

**返回值**: `{"error":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.resetZoom');
```


### window.restore

公开 API。运行时权威：`src/api/WindowApi.cpp:2387`。

_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.restore');
```


### window.sendMessage


<!-- phase3-major1-review:window.sendMessage -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:2223-2242`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `targetWindowId` | `string` | 是 | 无 |
| `message` | `json` | 是 | 无 |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`error`, `success`；`success`

**语义**: Both a non-empty targetWindowId and present message are required. The message is forwarded as arbitrary JSON from the caller identity; a missing recipient returns the target-not-found error.

<!-- phase3-major1-review-end:window.sendMessage -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2467`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `targetWindowId` | `string` | 是 | 必填。 |
| `message` | `json` | 是 | 必填。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.sendMessage', { message: /* value */, targetWindowId: /* value */ });
```


### window.setAcrylic


<!-- phase3-major1-review:window.setAcrylic -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:1593-1617`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |
| `enabled` | `boolean` | 否 | `true` |
| `darkMode` | `boolean` | 否 | `leave existing mode` |

**返回值**: `{"darkMode":"...","enabled":true,"success":true}`

**语义**: The mutation resolver chooses explicit windowId or caller. enabled patches compatibility backdrop to acrylic or clears it; darkMode is only patched when supplied, and panel mode is unsupported.

<!-- phase3-major1-review-end:window.setAcrylic -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2439`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |
| `darkMode` | `boolean` | 否 | 可选；默认 leave existing mode。 |


```js
const result = await fb2k.invoke('window.setAcrylic', { darkMode: /* value */, enabled: /* value */ });
```


### window.setAlwaysOnTop

公开 API。运行时权威：`src/api/WindowApi.cpp:2396`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setAlwaysOnTop', { enabled: /* value */ });
```


### window.setBackgroundTransparency

公开 API。运行时权威：`src/api/WindowApi.cpp:2441`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `transparent` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"WebView background is now transparent - DWM effects will show through":"...","description":"...","error":"...","success":true,"transparent":"..."}`

```js
const result = await fb2k.invoke('window.setBackgroundTransparency', { transparent: /* value */, windowId: /* value */ });
```


### window.setBlur

公开 API。运行时权威：`src/api/WindowApi.cpp:2438`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('window.setBlur', { enabled: /* value */, windowId: /* value */ });
```


### window.setClickThrough

公开 API。运行时权威：`src/api/WindowApi.cpp:2463`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |
| `windowId` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"clickThrough":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setClickThrough', { enabled: /* value */, windowId: /* value */ });
```


### window.setClickThroughExcludeRegions

公开 API。运行时权威：`src/api/WindowApi.cpp:2465`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `regions` | `array` | 否 | 可选；默认 omitted。 |
| `windowId` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"count":0,"dpiScale":"...","success":true,"warning":"...","windowId":"..."}`

```js
const result = await fb2k.invoke('window.setClickThroughExcludeRegions', { regions: /* value */, windowId: /* value */ });
```


### window.setCornerPreference


<!-- phase3-major1-review:window.setCornerPreference -->
#### 源码复核契约
权威：`src/api/WindowApi.cpp:1477-1486`。

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | `"default"` |

此调用**仅作用于主窗口**：不接受 `windowId`，也忽略调用方窗口。

**返回键（随响应变体而异）**：`error`、`success`；`success`

**语义**：设置主窗口的 Windows 11 圆角偏好。可取 `"default"`、`"none"`、`"round"`、`"small"`；其中 `"default"` 映射为圆角，因为无边框窗口没有标准非客户区框架可供系统默认值生效。popup 不接受该设置——它自行管理圆角并声明 `supportsCornerPreference: false`——故此调用没有 popup 作用域的形式。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。

<!-- phase3-major1-review-end:window.setCornerPreference -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2427`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | 可选；默认 default。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setCornerPreference', { mode: /* value */ });
```


### window.setDarkMode

公开 API。运行时权威：`src/api/WindowApi.cpp:2440`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('window.setDarkMode', { enabled: /* value */, windowId: /* value */ });
```


### window.setDevServerConfig

公开 API。运行时权威：`src/api/WindowApi.cpp:2445`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `devServerUrl` | `string` | 否 | 可选；默认 。 |
| `useDevServer` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: `{"devServerUrl":"...","success":true,"useDevServer":"..."}`

```js
const result = await fb2k.invoke('window.setDevServerConfig', { devServerUrl: /* value */, useDevServer: /* value */ });
```


### window.setFrameless

公开 API。运行时权威：`src/api/WindowApi.cpp:2462`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `frameless` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"error":"...","frameless":"...","success":true}`

```js
const result = await fb2k.invoke('window.setFrameless', { frameless: /* value */, windowId: /* value */ });
```


### window.setMaxSize


<!-- phase3-major1-review:window.setMaxSize -->
#### 源码复核契约
权威：`src/api/WindowApi.cpp:867-881`。

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `调用方窗口` |
| `width` | `integer` | 否 | `0` |
| `height` | `integer` | 否 | `0` |

**返回键（随响应变体而异）**：`error`、`success`；`success`、`windowId`

**语义**：变更型解析取显式 `windowId` 或调用方窗口，**绝不回退主窗口**——无法解析时直接失败，而不是改到非预期的窗口。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。数值单位为物理像素，按**目标窗口**的 DPI 换算为宿主的 DIP 存储；`0`（或负值）表示清除该上限。施加约束后会立即校正当前窗口尺寸，故已超出新上限的窗口会被收缩，而非等到下次用户拖拽。

<!-- phase3-major1-review-end:window.setMaxSize -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2404`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `height` | `integer` | 否 | 可选；默认 0。 |
| `width` | `integer` | 否 | 可选；默认 0。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setMaxSize', { height: /* value */, width: /* value */ });
```


### window.setMica


<!-- phase3-major1-review:window.setMica -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:1573-1576`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |
| `enabled` | `boolean` | 否 | `true` |
| `variant` | `string` | 否 | `mica` |
| `darkMode` | `boolean` | 否 | `leave existing mode` |

**返回值**: `{"darkMode":"...","enabled":true,"success":true,"variant":"..."}`

**语义**: setMica delegates to SetMicaEffectImpl: only mica-alt selects the alternate variant, all other values normalize to mica. It patches the resolved caller/target shell and is unsupported in panel mode.

<!-- phase3-major1-review-end:window.setMica -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2436`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |
| `variant` | `string` | 否 | 可选；默认 mica。 |
| `darkMode` | `boolean` | 否 | 可选；默认 leave existing mode。 |


```js
const result = await fb2k.invoke('window.setMica');
```


### window.setMicaEffect


<!-- phase3-major1-review:window.setMicaEffect -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:1579-1582`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |
| `enabled` | `boolean` | 否 | `true` |
| `variant` | `string` | 否 | `mica` |
| `darkMode` | `boolean` | 否 | `leave existing mode` |

**返回值**: `{"darkMode":"...","enabled":true,"success":true,"variant":"..."}`

**语义**: setMicaEffect is a direct compatibility alias of setMica and delegates to the same Mica implementation, including variant normalization, target resolution, and panel restriction.

<!-- phase3-major1-review-end:window.setMicaEffect -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2437`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | 可选；默认 true。 |
| `variant` | `string` | 否 | 可选；默认 mica。 |
| `darkMode` | `boolean` | 否 | 可选；默认 leave existing mode。 |


```js
const result = await fb2k.invoke('window.setMicaEffect');
```


### window.setMinSize


<!-- phase3-major1-review:window.setMinSize -->
#### 源码复核契约
权威：`src/api/WindowApi.cpp:831-845`。

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `调用方窗口` |
| `width` | `integer` | 否 | `0` |
| `height` | `integer` | 否 | `0` |

**返回键（随响应变体而异）**：`error`、`success`；`success`、`windowId`

**语义**：变更型解析取显式 `windowId` 或调用方窗口，**绝不回退主窗口**——无法解析时直接失败，而不是改到非预期的窗口。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。数值单位为物理像素，按**目标窗口**的 DPI 换算为宿主的 DIP 存储，使约束在 DPI 变化后仍然稳定。非正值会归一为 1px 下限。施加约束后会立即校正当前窗口尺寸，故已小于新下限的窗口会被放大，而非等到下次用户拖拽。

<!-- phase3-major1-review-end:window.setMinSize -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2402`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `height` | `integer` | 否 | 可选；默认 0。 |
| `width` | `integer` | 否 | 可选；默认 0。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setMinSize', { height: /* value */, width: /* value */ });
```


### window.setNoDragRegions

公开 API。运行时权威：`src/api/WindowApi.cpp:2421`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `regions` | `array` | 否 | 可选；默认 omitted。 |

**返回值**: `{"count":"...","dpiScale":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setNoDragRegions', { regions: /* value */ });
```


### window.setPosition

公开 API。运行时权威：`src/api/WindowApi.cpp:2430`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `x` | `integer` | 否 | 可选；默认 0。 |
| `y` | `integer` | 否 | 可选；默认 0。 |

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('window.setPosition', { x: /* value */, y: /* value */ });
```


### window.setResizable


<!-- phase3-major1-review:window.setResizable -->
#### 源码复核契约
权威：`src/api/WindowApi.cpp:901-921`。

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `调用方窗口` |
| `resizable` | `boolean` | 否 | `true` |

**返回键（随响应变体而异）**：`error`、`success`；`success`、`windowId`；`error`、`success`、`supported`、`windowId`

**语义**：变更型解析取显式 `windowId` 或调用方窗口，**绝不回退主窗口**。此前该调用无论来自哪个窗口都硬改主窗口，故从 popup 调用会改错窗口。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。设为与当前相同的值视为成功——幂等调用不算失败。仅当目标 shell 没有可切换的可调整边框时才返回 `supported: false`：完全无边框的 popup（`frame: false` 且 `transparent: true` 且无背景效果）创建时就没有尺寸边框，无法在运行时切换。依赖此调用前可先用 `window.isResizable` 查询 `supportsRuntimeToggle`。

<!-- phase3-major1-review-end:window.setResizable -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2406`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `resizable` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setResizable', { resizable: /* value */ });
```


### window.setSize

公开 API。运行时权威：`src/api/WindowApi.cpp:2431`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `height` | `integer` | 否 | 可选；默认 600。 |
| `width` | `integer` | 否 | 可选；默认 800。 |

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('window.setSize', { height: /* value */, width: /* value */ });
```


### window.setTitle

公开 API。运行时权威：`src/api/WindowApi.cpp:2412`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 可选；默认 foobar2000。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.setTitle', { title: /* value */ });
```


### window.setZoom

公开 API。运行时权威：`src/api/WindowApi.cpp:2447`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `zoom` | `number` | 否 | 可选；默认 1。 |

**返回值**: `{"error":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.setZoom', { zoom: /* value */ });
```


### window.setZoomForDpi

公开 API。运行时权威：`src/api/WindowApi.cpp:2450`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `dpi` | `integer` | 否 | 可选；默认 0。 |

**返回值**: `{"dpi":"...","error":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.setZoomForDpi', { dpi: /* value */ });
```


### window.startResize

公开 API。运行时权威：`src/api/WindowApi.cpp:2395`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `edge` | `string` | 否 | 可选；默认 bottomright。 |

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.startResize', { edge: /* value */ });
```


### window.toggleAlwaysOnTop

公开 API。运行时权威：`src/api/WindowApi.cpp:2398`。

_无参数。_

**返回值**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleAlwaysOnTop');
```


### window.toggleFullscreen


<!-- phase3-major1-review:window.toggleFullscreen -->
#### 源码复核 contract
权威源: `src/api/WindowApi.cpp:921-944`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` |

**返回字段（按变体取值）**: `error`, `fullscreen`, `success`；`fullscreen`, `success`

**语义**: The mutation resolver chooses explicit windowId or caller. The method flips the shell fullscreen state only when capability is available; panel mode and unsupported targets return false/error.

<!-- phase3-major1-review-end:window.toggleFullscreen -->
公开 API。运行时权威：`src/api/WindowApi.cpp:2409`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 可选；默认 caller window。 |

**返回值**: `{"error":"...","fullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleFullscreen');
```

## 合同补充

以下章节补齐严格参数审计发现的公开 contract；不会改变前文的已有说明。

<!-- phase3-supplement:window.focus -->
### Contract 补充：`window.focus`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:947-1009`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `` | 可选；默认 。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.focus', { windowId: /* value */ });
```
<!-- phase3-supplement:window.getBackdropPolicy -->
### Contract 补充：`window.getBackdropPolicy`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:2014-2023`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `backdropPolicy` | `object` | 否 |
| `resolvedBackdropPolicy` | `object` | 否 |
| `windowId` | `string` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.getBackdropPolicy', { windowId: /* value */ });
```
<!-- phase3-supplement:window.isFullscreen -->
### Contract 补充：`window.isFullscreen`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:577-584`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `fullscreen` | `json` | 否 |
| `isFullscreen` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.isFullscreen', { windowId: /* value */ });
```
<!-- phase3-supplement:window.setAcrylic -->
### Contract 补充：`window.setAcrylic`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:1593-1617`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | `true` | 可选；默认 true。 |
| `darkMode` | `boolean` | 否 | `leave existing mode` | 可选；默认 leave existing mode。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.setAcrylic', { windowId: /* value */, enabled: /* value */, darkMode: /* value */ });
```
<!-- phase3-supplement:window.setBackdropPolicy -->
### Contract 补充：`window.setBackdropPolicy`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:2026-2047`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |
| `backdropPolicy` | `object` | 是 | 无 | 必填。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `backdropPolicy` | `object` | 否 |
| `resolvedBackdropPolicy` | `object` | 否 |
| `windowId` | `string` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.setBackdropPolicy', { windowId: /* value */, backdropPolicy: /* value */ });
```
<!-- phase3-supplement:window.setBackgroundTransparency -->
### Contract 补充：`window.setBackgroundTransparency`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:1632-1658`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |
| `transparent` | `boolean` | 否 | `true` | 可选；默认 true。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `description` | `json` | 否 |
| `transparent` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.setBackgroundTransparency', { windowId: /* value */, transparent: /* value */ });
```
<!-- phase3-supplement:window.setBlur -->
### Contract 补充：`window.setBlur`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:1582-1593`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | `true` | 可选；默认 true。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `enabled` | `json` | 否 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.setBlur', { windowId: /* value */, enabled: /* value */ });
```
<!-- phase3-supplement:window.setDarkMode -->
### Contract 补充：`window.setDarkMode`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:1617-1627`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | `true` | 可选；默认 true。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `enabled` | `json` | 否 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.setDarkMode', { windowId: /* value */, enabled: /* value */ });
```
<!-- phase3-supplement:window.setFrameless -->
### Contract 补充：`window.setFrameless`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:2088-2101`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |
| `frameless` | `boolean` | 否 | `true` | 可选；默认 true。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `frameless` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.setFrameless', { windowId: /* value */, frameless: /* value */ });
```
<!-- phase3-supplement:window.setFullscreen -->
### Contract 补充：`window.setFullscreen`

经复核的补充 contract。权威源：`src/api/WindowApi.cpp:899-921`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | `caller window` | 可选；默认 caller window。 |
| `enabled` | `boolean` | 否 | `true` | 可选；默认 true。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `fullscreen` | `json` | 否 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('window.setFullscreen', { windowId: /* value */, enabled: /* value */ });
```

## 运行时行为与事件

所有 `window.*` 调用默认在发起调用的 WebView 上下文中执行；接受 `windowId`
的方法可显式指定目标。`main` 表示主窗口，popup ID 由
`window.createPopup` 与 `window.getAllWindows` 返回。面板模式下需要独立窗口
shell 的调用会返回不支持或找不到窗口，而不会静默改为操作其他窗口。

`window.setDragRegions`、`window.setNoDragRegions` 与 click-through 排除区域使用
CSS 像素矩形，native handler 会按目标窗口 DPI 转换。click-through 和关闭确认等
popup 专用操作拒绝主窗口目标。

运行时会在 shell 状态变化时发射 `window:stateChanged`，并将
`window:beforeClose` 路由到请求关闭确认的 popup。popup 生命周期和协同事件包括
`window:popupOpened`、`window:popupClosed`、`window:message`、
`window:behaviorChanged`、`window:backdropStateChanged`、
`window:hoverStateChanged`、`window:minimizeSuppressed` 与
`window:alwaysOnTopChanged`。事件 payload 是运行时数据，调用方应容忍 shell 后续
添加字段。
