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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 | 目标窗口 id。 |

- **返回值**: `{ "fullscreen": false, "isFullscreen": false }`

## window.setFullscreen


设置全屏模式。采用 Chromium 风格的全屏实现，保存/恢复窗口状态。

主窗口与 popup 进入/退出 fullscreen 后，都会重新通过统一的 window chrome resolver/applier 应用当前 `backdropPolicy` / frameless / darkMode 状态。默认作用于当前调用窗口；如需显式指定目标窗口，可传 `windowId`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `enabled` | `boolean` | 否 | `true` |  |

- **返回值**: `{ "success": true, "fullscreen": true }`

```javascript
await fb2k.invoke('window.setFullscreen', { enabled: true });
// 退出全屏
await fb2k.invoke('window.setFullscreen', { enabled: false });
```

## window.setBounds


设置窗口边界。所有参数可选，未传递的保持不变。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `x` | `integer` | 否 | 当前值 |  |
| `y` | `integer` | 否 | 当前值 |  |
| `width` | `integer` | 否 | 当前值 |  |
| `height` | `integer` | 否 | 当前值 |  |

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 | `main` 或 popup id。 |

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `x` | `integer` | 否 | `0` | 排除区域左上角 X；未提供 `w`/`h` 时 `x`/`y` 直接作为菜单弹出坐标。 |
| `y` | `integer` | 否 | `0` | 排除区域左上角 Y。 |
| `w` | `integer` | 否 | `0` | 排除区域宽度。 |
| `h` | `integer` | 否 | `0` | 排除区域高度。 |

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `height` | `integer` | 否 | `32` | 有效范围 24–100。 |

- **返回值**: `{ "success": true, "height": 32 }`

::: warning
高度必须在 24-100 之间，超出范围返回错误。
:::

::: tip
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
| `regions` | `array` | 否 | CSS 像素矩形 `{ x, y, width, height }` 数组。 |

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方 popup | 仅 popup。 |

**返回值**:

```json
{
    "success": true,
    "windowId": "popup-1",
    "profile": "standard",
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

- `profile` — 当前 popup 应用的预设档，取值只有 `standard`、`miniPlayer`、`desktopLyrics` 三种
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


运行时更新 popup 窗口的行为策略。可一次切换 profile，也可通过 `behavior` 做字段级覆盖。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方 popup | 目标 popup id。 |
| `profile` | `string` | 否 | 当前 profile | `standard`、`miniPlayer` 或 `desktopLyrics`。 |
| `behavior` | `object` | 否 | — | 字段级覆盖；仅在提供时应用。 |

字段优先级：`behavior.*` 字段覆盖 > `profile` 默认。


::: warning 仅 popup
显式传入 `windowId: "main"` 会返回 `{ success: false, error: "window.setPopupBehavior does not support main window" }`。省略 `windowId` 时作用于调用方 popup；若调用方不是 popup（例如从主窗口调用），返回的是 `{ success: false, error: "Window not found" }`，而非上面那条消息。
:::

**返回值**: `{ "success": true, "windowId": "...", "profile": "...", "behavior": { ... }, "resolvedBehavior": { ... } }`

`resolvedBehavior` 是 profile 默认值与你的覆盖合并后的最终生效策略。`profile` 与 `behavior` 相互独立：只传其中一个不会重置另一个；`behavior` 内某字段传 `null` 表示**擦除**该项覆盖，而非存入 null。

`profile` 匹配不区分大小写，且接受连字符与下划线写法，因此 `miniPlayer`、`miniplayer`、`mini-player`、`mini_player` 等价。返回的 `profile` 始终是上述三个规范名称之一。

```javascript
// 仅切换 profile
await fb2k.invoke('window.setPopupBehavior', { profile: 'miniPlayer' });

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


获取窗口的 DWM 背景效果策略。支持 main 与 popup。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |

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


运行时更新 DWM 背景策略。可单独覆盖 `activeEffect` / `inactiveEffect` 等字段；传入 `null` 表示清空字段恢复默认。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `backdropPolicy` | `object` | 是 | — | 字段级补丁；字段传 `null` 表示恢复默认。 |

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

## 其他公开 API


### window.broadcast


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `json` | 是 | 广播到除发送者外所有窗口的 JSON 消息体（经 `window:message` 事件送达）。 |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.broadcast', { message: { type: 'themeChanged', theme: 'dark' } });
```


### window.cancelClose


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.cancelClose');
```


### window.center


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.center');
```


### window.clearClickThroughExcludeRegions


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 仅 popup；从目标 popup 自身调用时可省略。 |

**返回值**: `{"error":"...","success":true,"windowId":"..."}`

```js
// 仅作用于 popup；只有从该 popup 自身调用时才可省略 windowId
await fb2k.invoke('window.clearClickThroughExcludeRegions', { windowId: 'popup-1' });
```


### window.clearNoDragRegions


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.clearNoDragRegions');
```


### window.close


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('window.close');
```


### window.closeAllPopups


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('window.closeAllPopups');
```


### window.closePopup


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 是 | 要关闭的 popup id；不回退到调用方窗口。 |

**返回值**: `{"error":"...","success":true}`

```js
// 此处 windowId 为必填，不会回退到调用方窗口
await fb2k.invoke('window.closePopup', { windowId: 'popup-1' });
```


### window.confirmClose


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.confirmClose');
```


### window.createPopup


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `url` | `string` | 否 | — | 弹窗加载的页面地址。 |
| `title` | `string` | 否 | — |  |
| `x` | `integer` | 否 | 系统默认 | 省略时由系统定位（CW_USEDEFAULT）。 |
| `y` | `integer` | 否 | 系统默认 |  |
| `width` | `integer` | 否 | `400` |  |
| `height` | `integer` | 否 | `300` |  |
| `minWidth` | `integer` | 否 | `200` |  |
| `minHeight` | `integer` | 否 | `150` |  |
| `maxWidth` | `integer` | 否 | `0` | `0` 表示无上限。 |
| `maxHeight` | `integer` | 否 | `0` | `0` 表示无上限。 |
| `resizable` | `boolean` | 否 | `true` |  |
| `frame` | `boolean` | 否 | `true` | `false` 为无边框。 |
| `transparent` | `boolean` | 否 | `false` | 背景透明。 |
| `alwaysOnTop` | `boolean` | 否 | `false` |  |
| `showInTaskbar` | `boolean` | 否 | `false` |  |
| `clickThrough` | `boolean` | 否 | `false` | 鼠标穿透。 |
| `beforeClose` | `boolean` | 否 | `false` | 关闭前发送 `window:beforeClose` 供确认。 |
| `profile` | `string` | 否 | — | 行为预设：`standard` / `miniPlayer` / `desktopLyrics`。 |
| `behavior` | `object` | 否 | — | 行为字段覆盖（见 `setPopupBehavior`）。 |
| `backdropPolicy` | `object` | 否 | — | 背景策略（见 `setBackdropPolicy`）。 |

**返回值**: `{"error":"...","success":true,"windowId":"..."}`

```js
const { windowId } = await fb2k.invoke('window.createPopup', {
    url: 'popup.html',
    width: 480,
    height: 320,
    title: 'Now Playing',
});
```


### window.enterFullscreen


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |

**返回值**: `{"error":"...","isFullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.enterFullscreen');
```


### window.exitFullscreen


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |

**返回值**: `{"error":"...","isFullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.exitFullscreen');
```


### window.flash


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `count` | `integer` | 否 | `3` | 闪烁次数。 |
| `enabled` | `boolean` | 否 | `true` |  |

**返回值**: `{"error":"...","success":true}`

```js
// 省略 count 与 enabled 即闪烁 3 次
await fb2k.invoke('window.flash');
```


### window.flashTaskbar


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `count` | `integer` | 否 | `3` | 闪烁次数。 |

**返回值**: `{"success":true}`

```js
// 省略 count 即闪烁 3 次
await fb2k.invoke('window.flashTaskbar');
```


### window.getAllWindows


_无参数。_

**返回值**: `{"items":"...","success":true}`

```js
const result = await fb2k.invoke('window.getAllWindows');
```


### window.getBounds


_无参数。_

**返回值**: `{"height":"...","width":"...","x":"...","y":"..."}`

```js
const result = await fb2k.invoke('window.getBounds');
```


### window.getCornerPreference

返回主窗口的 Windows 11 圆角偏好；`mode` 与 `preference` 是同一个值的两个名字。仅作用于主窗口：不接受 `windowId`，也忽略调用方窗口。popup 不暴露该设置——它自行管理圆角（无边框时圆角，否则用系统默认），并在能力中声明 `supportsCornerPreference: false`。

_无参数。_

**返回值**: `{"mode":"...","preference":"..."}`

无主窗口时返回 `"default"` 而非错误。

```js
const result = await fb2k.invoke('window.getCornerPreference');
```


### window.getCurrentWindowId


_无参数。_

**返回值**: `{"success":true,"windowId":"..."}`

```js
const result = await fb2k.invoke('window.getCurrentWindowId');
```


### window.getDevServerConfig


_无参数。_

**返回值**: `{"devServerUrl":"...","success":true,"useDevServer":"..."}`

```js
const result = await fb2k.invoke('window.getDevServerConfig');
```


### window.getMaxSize

读取窗口的最大尺寸约束。解析取显式 `windowId` 或调用方窗口，**不回退主窗口**——无法解析调用上下文时直接失败，而不是静默返回另一个窗口的约束。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`，因为面板不是窗口 shell。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |

**返回值**: `{"height":"...","success":true,"width":"...","windowId":"..."}`。无法解析目标窗口时返回 `{ "success": false, "error": "..." }`。

数值单位为物理像素；宿主以 DIP 存储约束，并按**目标窗口**的 DPI 换算。`0` 表示无上限，且换算前后精确不变。

返回的是**请求值**，不是当前窗口尺寸。「物理 → DIP → 物理」的往返会量化到整数 DIP，故在非 100% 缩放下 `get` 与传给 `set` 的值每轴最多相差 1px（例如 125% 下 `202px` 读回为 `203px`）。请把 getter 理解为「在 ±1px 内复述你设置的约束」，而非精确相等。`0` 不受此影响。

```js
const result = await fb2k.invoke('window.getMaxSize');
```


### window.getMinSize

读取窗口的最小尺寸约束。解析取显式 `windowId` 或调用方窗口，**不回退主窗口**——无法解析调用上下文时直接失败，而不是静默返回另一个窗口的约束。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`，因为面板不是窗口 shell。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |

**返回值**: `{"height":"...","success":true,"width":"...","windowId":"..."}`。无法解析目标窗口时返回 `{ "success": false, "error": "..." }`。

数值单位为物理像素；宿主以 DIP 存储约束，并按**目标窗口**的 DPI 换算。

返回的是**请求值**，不是当前窗口尺寸。「物理 → DIP → 物理」的往返会量化到整数 DIP，故在非 100% 缩放下 `get` 与传给 `set` 的值每轴最多相差 1px（例如 125% 下 `202px` 读回为 `203px`）。请把 getter 理解为「在 ±1px 内复述你设置的约束」，而非精确相等。

```js
const result = await fb2k.invoke('window.getMinSize');
```


### window.getMode


_无参数。_

**返回值**: `{"mode":"...","panel":"...","panelMode":"...","windowId":"..."}`

```js
const result = await fb2k.invoke('window.getMode');
```


### window.getTitlebarHeight


_无参数。_

**返回值**: `{"height":"..."}`

```js
const result = await fb2k.invoke('window.getTitlebarHeight');
```


### window.getZoom


_无参数。_

**返回值**: `{"dpi":"...","dpiScale":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.getZoom');
```


### window.isAlwaysOnTop


_无参数。_

**返回值**: `{"enabled":"...","isAlwaysOnTop":"..."}`

```js
const result = await fb2k.invoke('window.isAlwaysOnTop');
```


### window.isClickThrough


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 | 仅 popup。 |

**返回值**: `{"clickThrough":"...","error":"...","success":true}`

```js
const { clickThrough } = await fb2k.invoke('window.isClickThrough', { windowId: 'popup-1' });
```


### window.isResizable

返回窗口请求态的可调整性。解析取显式 `windowId` 或调用方窗口，**不回退主窗口**。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |

**返回值**: `{"resizable":"...","success":true,"windowId":"..."}`。无法解析目标窗口时返回 `{ "success": false, "error": "..." }`。

所有窗口 shell（含完全无边框的 popup）都支持通过 [`window.setResizable`](#window-setresizable) 在运行时改变它。

```js
const result = await fb2k.invoke('window.isResizable');
```


### window.maximize


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.maximize');
```


### window.minimize


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.minimize');
```


### window.refreshWebView


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.refreshWebView');
```


### window.reload


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.reload');
```


### window.resetZoom


_无参数。_

**返回值**: `{"error":"...","success":true,"zoom":"..."}`

```js
const result = await fb2k.invoke('window.resetZoom');
```


### window.restore


_无参数。_

**返回值**: `{"error":"...","success":true}`

```js
const result = await fb2k.invoke('window.restore');
```


### window.sendMessage


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `targetWindowId` | `string` | 是 | 目标窗口 id（`main` 或 popup id）。 |
| `message` | `json` | 是 | 任意 JSON 消息体，经 `window:message` 事件送达。 |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.sendMessage', {
    targetWindowId: 'popup-1',
    message: { type: 'seek', position: 42 },
});
```


### window.setAcrylic

应用或清除亚克力背景材质。panel 模式下不支持。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `enabled` | `boolean` | 否 | `true` |  |
| `darkMode` | `boolean` | 否 | 当前模式 |  |

**返回值**: `{ "success": true, "enabled": true }`；仅当你传入 `darkMode` 时才回显该字段。

`success` 反映材质是否真正应用成功——即使窗口有效，平台拒绝该效果时也可能为 `false`。

```js
await fb2k.invoke('window.setAcrylic', { enabled: true, darkMode: true });
```


### window.setAlwaysOnTop


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `enabled` | `boolean` | 否 | `true` |  |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.setAlwaysOnTop', { enabled: true });
```


### window.setBackgroundTransparency


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `transparent` | `boolean` | 否 | `true` |  |

**返回值**: `{"description":"...","error":"...","success":true,"transparent":"..."}`

```js
await fb2k.invoke('window.setBackgroundTransparency', { transparent: true });
```


### window.setBlur


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `enabled` | `boolean` | 否 | `true` |  |

**返回值**: `{"enabled":"...","success":true}`

```js
await fb2k.invoke('window.setBlur', { enabled: true });
```


### window.setClickThrough


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 | 仅 popup。 |
| `enabled` | `boolean` | 否 | `true` |  |

**返回值**: `{"clickThrough":"...","error":"...","success":true}`

```js
await fb2k.invoke('window.setClickThrough', { enabled: true });
```


### window.setClickThroughExcludeRegions


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方 popup | 仅 popup。 |
| `regions` | `array` | 否 | — | CSS 像素矩形 `{ x, y, width, height }` 数组。 |

**返回值**: `{"count":0,"dpiScale":"...","success":true,"warning":"...","windowId":"..."}`

```js
await fb2k.invoke('window.setClickThroughExcludeRegions', {
    regions: [{ x: 12, y: 12, width: 160, height: 40 }],
});
```


### window.setCornerPreference

设置主窗口的 Windows 11 圆角偏好。仅作用于主窗口：不接受 `windowId`，也忽略调用方窗口。popup 不接受该设置——它自行管理圆角并声明 `supportsCornerPreference: false`。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `mode` | `string` | 否 | `default` | 可取 `default` / `none` / `round` / `small`。 |

**返回值**: `{"error":"...","success":true}`

`"default"` 映射为圆角，因为无边框窗口没有标准非客户区框架可供系统默认值生效。

```js
await fb2k.invoke('window.setCornerPreference', { mode: 'round' });
```


### window.setDarkMode


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `enabled` | `boolean` | 否 | `true` |  |

**返回值**: `{"enabled":"...","success":true}`

```js
await fb2k.invoke('window.setDarkMode', { enabled: true });
```


### window.setDevServerConfig


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `useDevServer` | `boolean` | 否 | `false` |  |
| `devServerUrl` | `string` | 否 | — | 开发服务器地址，如 `http://localhost:5173`。 |

**返回值**: `{"devServerUrl":"...","success":true,"useDevServer":"..."}`

```js
await fb2k.invoke('window.setDevServerConfig', {
    useDevServer: true,
    devServerUrl: 'http://localhost:5173',
});
```


### window.setFrameless


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `frameless` | `boolean` | 否 | `true` |  |

**返回值**: `{"error":"...","frameless":"...","success":true}`

```js
await fb2k.invoke('window.setFrameless', { frameless: true });
```


### window.setMaxSize

设置窗口的最大尺寸。解析取显式 `windowId` 或调用方窗口，**绝不回退主窗口**——无法解析时直接失败，而不是改到非预期的窗口。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `width` | `integer` | 否 | `0` |  |
| `height` | `integer` | 否 | `0` |  |

**返回值**: `{"error":"...","success":true,"windowId":"..."}`

数值单位为物理像素，按**目标窗口**的 DPI 换算为宿主的 DIP 存储；`0`（或负值）表示清除该上限。施加约束后会立即校正当前窗口尺寸，故已超出新上限的窗口会被收缩，而非等到下次用户拖拽。

```js
await fb2k.invoke('window.setMaxSize', { width: 1920, height: 1080 });
```


### window.setMica

应用或清除 Mica 背景材质。panel 模式下不支持。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `enabled` | `boolean` | 否 | `true` |  |
| `variant` | `string` | 否 | `mica` | `mica` 或 `mica-alt`。 |
| `darkMode` | `boolean` | 否 | 当前模式 |  |

**返回值**: `{ "success": true, "enabled": true, "variant": "mica" }`；仅当你传入 `darkMode` 时才回显该字段。

只有 `mica-alt` 会选择备用变体；其余取值（包括无法识别的值）都会被**静默归一化**为 `mica` 而不报错，因此如需确认实际生效值请读取返回的 `variant`。`success` 反映材质是否真正应用成功——即使窗口有效，平台拒绝该效果时也可能为 `false`。

```js
await fb2k.invoke('window.setMica', { enabled: true, variant: 'mica-alt' });
```


### window.setMicaEffect

[`window.setMica`](#window-setmica) 的兼容别名：参数、行为与返回结构完全一致。新代码建议直接用 `window.setMica`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `enabled` | `boolean` | 否 | `true` |  |
| `variant` | `string` | 否 | `mica` | `mica` 或 `mica-alt`。 |
| `darkMode` | `boolean` | 否 | 当前模式 |  |

**返回值**: `{ "success": true, "enabled": true, "variant": "mica" }`；仅当你传入 `darkMode` 时才回显该字段。

```js
await fb2k.invoke('window.setMicaEffect', { enabled: true });
```


### window.setMinSize

设置窗口的最小尺寸。解析取显式 `windowId` 或调用方窗口，**绝不回退主窗口**——无法解析时直接失败，而不是改到非预期的窗口。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `width` | `integer` | 否 | `0` |  |
| `height` | `integer` | 否 | `0` |  |

**返回值**: `{"error":"...","success":true,"windowId":"..."}`

数值单位为物理像素，按**目标窗口**的 DPI 换算为宿主的 DIP 存储，使约束在 DPI 变化后仍然稳定。非正值会归一为 1px 下限。施加约束后会立即校正当前窗口尺寸，故已小于新下限的窗口会被放大，而非等到下次用户拖拽。

```js
await fb2k.invoke('window.setMinSize', { width: 480, height: 320 });
```


### window.setNoDragRegions


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `regions` | `array` | 否 | CSS 像素矩形 `{ x, y, width, height }` 数组。 |

**返回值**: `{"count":"...","dpiScale":"...","error":"...","success":true}`

```js
await fb2k.invoke('window.setNoDragRegions', {
    regions: [{ x: 690, y: 0, width: 110, height: 32 }],
});
```


### window.setPosition


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `x` | `integer` | 否 | `0` |  |
| `y` | `integer` | 否 | `0` |  |

**返回值**: `{"success":true}`

```js
await fb2k.invoke('window.setPosition', { x: 100, y: 100 });
```


### window.setResizable

设置窗口是否可由用户调整大小。解析取显式 `windowId` 或调用方窗口，**绝不回退主窗口**。面板（DUI/CUI）调用方会被拒绝并返回 `panelMode: true`。设为与当前相同的值视为成功——幂等调用不算失败。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |
| `resizable` | `boolean` | 否 | `true` |  |

**返回值**: `{"error":"...","success":true,"windowId":"..."}`

所有窗口 shell 都支持运行时切换，包括完全无边框的 popup（`frame: false` 且 `transparent: true` 且无背景效果）：这类窗口会把整个非客户区收掉，故新增尺寸边框只改变命中测试，不改变外观。因此 `success: false` 表示 Win32 调用**真实失败**（样式未写入或帧刷新失败），而非「窗口形态不支持」；此时请求态不会被提交。

::: warning 行为变更
此前该调用无论来自哪个窗口都硬改主窗口，故从 popup 调用会改错窗口。
:::

```js
await fb2k.invoke('window.setResizable', { resizable: false });
```


### window.setSize


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `width` | `integer` | 否 | `800` |  |
| `height` | `integer` | 否 | `600` |  |

**返回值**: `{"success":true}`

```js
await fb2k.invoke('window.setSize', { width: 1024, height: 640 });
```


### window.setTitle


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `title` | `string` | 否 | `foobar2000` |  |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.setTitle', { title: 'Now Playing' });
```


### window.setZoom


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `zoom` | `number` | 否 | `1` | 缩放倍率，如 `1.25`。 |

**返回值**: `{"error":"...","success":true,"zoom":"..."}`

```js
await fb2k.invoke('window.setZoom', { zoom: 1.25 });
```


### window.setZoomForDpi


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `dpi` | `integer` | 否 | `0` | 省略或 `0` 时按调用方窗口当前 DPI 推导。 |

**返回值**: `{"dpi":"...","error":"...","success":true,"zoom":"..."}`

```js
// 省略 dpi 时按调用方窗口当前 DPI 推导缩放
const { zoom } = await fb2k.invoke('window.setZoomForDpi');
```


### window.startResize


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `edge` | `string` | 否 | `bottomright` | 拖拽的边或角。 |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('window.startResize', { edge: 'bottomright' });
```


### window.toggleAlwaysOnTop


_无参数。_

**返回值**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleAlwaysOnTop');
```


### window.toggleFullscreen


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `windowId` | `string` | 否 | 调用方窗口 |  |

**返回值**: `{"error":"...","fullscreen":"...","success":true}`

```js
const result = await fb2k.invoke('window.toggleFullscreen');
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
`window:alwaysOnTopChanged`。事件 payload 是运行时数据，调用方应兼容 shell 后续
新增的字段。
