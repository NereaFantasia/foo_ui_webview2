# Window 窗口

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

- **参数**: `windowId?: string`，省略时表示当前调用窗口
- **返回值**: `{ "fullscreen": false, "isFullscreen": false }`

## window.setSize 

 设置全屏模式。采用 Chromium 风格的全屏实现，保存/恢复窗口状态。

主窗口与 popup 进入/退出 fullscreen 后，都会重新通过统一的 window chrome resolver/applier 应用当前 `backdropPolicy` / frameless / darkMode 状态。默认作用于当前调用窗口；如需显式指定目标窗口，可传 `windowId`。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| windowId | string | ✗ | 目标窗口 ID；省略时表示当前调用窗口 |
| enabled | boolean | ✗ | 是否全屏（默认 true） |

- **返回值**: `{ "success": true, "fullscreen": true }`

```javascript
await fb2k.invoke('window.setFullscreen', { enabled: true });
// 退出全屏
await fb2k.invoke('window.setFullscreen', { enabled: false });
```

## window.enterFullscreen 

 设置窗口边界。所有参数可选，未传递的保持不变。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| x | number | ✗ | X 坐标 |
| y | number | ✗ | Y 坐标 |
| width | number | ✗ | 宽度 |
| height | number | ✗ | 高度 |

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
- **返回值**: `{ "dpi": 144, "scale": 1.5 }`

```javascript
const dpi = await fb2k.invoke('window.getDpiScale');
console.log(`DPI: ${dpi.dpi}, 缩放: ${dpi.scale}x`);
```

## window.focus 

使窗口获得焦点。支持可选的 `windowId` 参数，可从任意窗口聚焦指定的主窗口或弹窗。

- **参数**:

| 名称 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| windowId | string | 否 | 目标窗口 ID。"main" = 主窗口，其他值 = 弹窗 ID。省略时聚焦调用者所在窗口 |
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

## window.flash 

 显示系统菜单（最小化/最大化/关闭等）。支持传入排除区域避免遮挡按钮。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| x | number | ✗ | X 坐标（默认 0） |
| y | number | ✗ | Y 坐标（默认 0） |
| w | number | ✗ | 排除区域宽度 |
| h | number | ✗ | 排除区域高度 |

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

## window.startResize 

 设置**当前调用窗口**（`main` 或 `popup`）的标题栏高度。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| height | number | ✗ | 高度（默认 32，范围 24-100） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| regions | array | ✓ | 区域数组，每个元素含 x, y, width, height（CSS 像素） |

- **返回值**: `{ "success": true, "count": 2, "dpiScale": 1.5 }`

```javascript
await fb2k.invoke('window.setDragRegions', {
    regions: [
        { x: 0, y: 0, width: 800, height: 32 }
    ]
});
```

## window.clearDragRegions


## window.getPopupBehavior

获取弹出窗口（popup）的行为策略。仅对 popup 窗口生效，不支持 main 窗口。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| windowId | string | ✗ | 目标 popup 窗口 ID。省略时使用当前调用窗口（caller HWND 反查 popup） |

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

运行时更新 popup 窗口的行为策略。可一次切换 profile，也可通过 `behavior` 做字段级覆盖。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| windowId | string | ✗ | 目标 popup 窗口 ID（省略 = 当前调用窗口） |
| profile | string | ✗ | 切换到指定 profile（如 `floating` / `tool` / `dialog`），传入即应用 |
| behavior | object | ✗ | 字段级覆盖，结构同 `WindowPopupBehaviorPatch`：`{ showInTaskbar?: boolean\|null, showInAltTab?: boolean\|null, ... }`；传 `null` 清空对应字段，恢复 profile 默认 |

字段优先级：`behavior.*` 字段覆盖 > `profile` 默认。

**返回值**: 同 `getPopupBehavior`，附 `success: true`。

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

获取窗口的 DWM 背景效果策略。支持 main 与 popup。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| windowId | string | ✗ | 目标窗口 ID（省略 = 当前调用窗口） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| windowId | string | ✗ | 目标窗口 ID（省略 = 当前调用窗口） |
| backdropPolicy | object | ✓ | 字段级 patch；结构同 `WindowBackdropPolicyPatch`：`{ activeEffect?: string\|null, inactiveEffect?: string\|null, ... }` |

**返回值**: 同 `getBackdropPolicy`，附 `success: true`。

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
