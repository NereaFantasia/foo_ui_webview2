# Window 窗口

## window.toggleMaximize

<!-- api-schema:begin window.toggleMaximize -->
最大化调用方窗口，已最大化时还原。全屏的窗口改为退出全屏。面板模式下失败；没有窗口时失败并带 `maximized: false`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `maximized` | `boolean` | 投递的请求生效后窗口是否最大化。若是退出全屏，则是窗口回到的状态。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('window.toggleMaximize');
if (result.success === false) throw new Error(result.error);
console.log(result.maximized ? '已最大化' : '已还原');
```

## window.getState

<!-- api-schema:begin window.getState -->
报告调用方窗口的状态标志与矩形。每个标志都发两遍，一遍不带前缀、一遍带 `is` 前缀。没有窗口时标志全为 `false`，矩形全为 0。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `maximized` | `boolean` | 窗口是否最大化。 |
| `minimized` | `boolean` | 窗口是否最小化。 |
| `fullscreen` | `boolean` | 窗口是否全屏。 |
| `alwaysOnTop` | `boolean` | 窗口是否保持在其他窗口之上。 |
| `focused` | `boolean` | 窗口是否为前台窗口。 |
| `isMaximized` | `boolean` | 与 `maximized` 相同。 |
| `isMinimized` | `boolean` | 与 `minimized` 相同。 |
| `isFullscreen` | `boolean` | 与 `fullscreen` 相同。 |
| `isAlwaysOnTop` | `boolean` | 与 `alwaysOnTop` 相同。 |
| `isFocused` | `boolean` | 与 `focused` 相同。 |
| `width` | `integer` | 宽度，物理像素，含边框。 |
| `height` | `integer` | 高度，物理像素，含边框。 |
| `x` | `integer` | 左边缘的屏幕坐标。 |
| `y` | `integer` | 上边缘的屏幕坐标。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const state = await fb2k.invoke('window.getState');
if (state.success === false) throw new Error(state.error);
if (state.isMaximized) {
    console.log(`窗口已最大化，尺寸: ${state.width}x${state.height}`);
}
```

## window.isMaximized

<!-- api-schema:begin window.isMaximized -->
报告调用方窗口是否最大化；没有窗口时为 `false`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `maximized` | `boolean` | 窗口是否最大化。 |
| `isMaximized` | `boolean` | 与 `maximized` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.isMinimized

<!-- api-schema:begin window.isMinimized -->
报告调用方窗口是否最小化；没有窗口时为 `false`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `minimized` | `boolean` | 窗口是否最小化。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.isFullscreen

<!-- api-schema:begin window.isFullscreen -->
报告窗口是否全屏。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fullscreen` | `boolean` | 窗口是否全屏。 |
| `isFullscreen` | `boolean` | 与 `fullscreen` 相同。 |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.setFullscreen

<!-- api-schema:begin window.setFullscreen -->
进入或退出全屏。设为窗口已有的状态照常成功。退出全屏时恢复进入前的矩形、最大化状态与置顶状态。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `enabled` | `boolean` | 否 | `true` 进入全屏，`false` 退出全屏。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fullscreen` | `boolean` | 窗口现在是否全屏。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

主窗口与 popup 进入/退出 fullscreen 后，都会重新通过统一的 window chrome resolver/applier 应用当前 `backdropPolicy` / frameless / darkMode 状态。

```javascript
await fb2k.invoke('window.setFullscreen', { enabled: true });
// 退出全屏
await fb2k.invoke('window.setFullscreen', { enabled: false });
```

## window.setBounds

<!-- api-schema:begin window.setBounds -->
一次移动或缩放调用方窗口；省略的键保持当前值，小数部分舍去。面板模式与没有窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `x` | `number` | 否 | 新的左边缘屏幕坐标。 |
| `y` | `number` | 否 | 新的上边缘屏幕坐标。 |
| `width` | `number` | 否 | 新的宽度，物理像素，含边框。 |
| `height` | `number` | 否 | 新的高度，物理像素，含边框。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 只改大小不改位置
await fb2k.invoke('window.setBounds', { width: 1920, height: 1080 });
// 只改位置不改大小
await fb2k.invoke('window.setBounds', { x: 0, y: 0 });
```

## window.hasSavedBounds

<!-- api-schema:begin window.hasSavedBounds -->
报告此前的会话是否保存过主窗口的位置，页面可据此决定首次启动时是否设置默认大小。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hasSavedBounds` | `boolean` | 是否保存有此前会话的位置。 |
| `description` | `string` | 复述 `hasSavedBounds` 的一句英文，供日志使用。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.getDpiScale

<!-- api-schema:begin window.getDpiScale -->
报告调用方窗口设备上下文的 DPI 及其与 96 的比值；没有窗口时为 `96`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dpi` | `integer` | DPI；`96` 即 100 %。 |
| `scale` | `number` | `dpi / 96`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const dpi = await fb2k.invoke('window.getDpiScale');
if (dpi.success === false) throw new Error(dpi.error);
console.log(`DPI: ${dpi.dpi}, 缩放: ${dpi.scale}x`);
```

## window.focus

<!-- api-schema:begin window.focus -->
把窗口带到前台：最小化的先还原，隐藏的先显示。没有这个窗口时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 聚焦调用者自身窗口
await fb2k.invoke('window.focus');

// 从弹窗聚焦主窗口
await fb2k.invoke('window.focus', { windowId: 'main' });

// 从主窗口聚焦指定弹窗
await fb2k.invoke('window.focus', { windowId: 'my-popup-id' });
```

## window.blur

<!-- api-schema:begin window.blur -->
把前台交给 Z 序里位于调用方窗口之下的那个窗口。没有窗口时失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.getTitle

<!-- api-schema:begin window.getTitle -->
报告调用方窗口的标题，最多 255 个字符；没有窗口时为空串。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `title` | `string` | 窗口标题。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('window.getTitle');
if (result.success === false) throw new Error(result.error);
console.log('窗口标题:', result.title);
```

## window.showSystemMenu

<!-- api-schema:begin window.showSystemMenu -->
打开调用方窗口的系统菜单（还原、移动、大小、最小化、最大化、关闭）并执行选中的命令。坐标是屏幕像素，小数部分舍去。`w` 与 `h` 都为正时菜单在矩形 `x`、`y`、`w`、`h` 下方弹出并避开它，否则在 `x`、`y` 处弹出。面板模式、没有窗口时失败；窗口没有系统菜单时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `x` | `number` | 否 | 要避开的矩形的左边缘；`w` 或 `h` 不为正时是菜单的位置。默认 `0`。 |
| `y` | `number` | 否 | 要避开的矩形的上边缘；`w` 或 `h` 不为正时是菜单的位置。默认 `0`。 |
| `w` | `number` | 否 | 要避开的矩形的宽度。默认 `0`。 |
| `h` | `number` | 否 | 要避开的矩形的高度。默认 `0`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

<!-- api-schema:begin window.startDrag -->
开始用鼠标拖动调用方窗口，效果与按住标题栏相同。在 `mousedown` 处理函数里、按键仍按着时调用。面板模式与没有窗口时失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
titlebar.addEventListener('mousedown', async () => {
    await fb2k.invoke('window.startDrag');
});
```

## window.setTitlebarHeight

<!-- api-schema:begin window.setTitlebarHeight -->
设置调用方窗口（主窗口或 popup）的标题栏高度；popup 的高度不影响主窗口。高度须在 24 到 100 之间，否则以 `INVALID_PARAMS` 失败。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `height` | `number` | 否 | 标题栏高度，物理像素，24 到 100；小数部分舍去。默认 `32`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `height` | `integer` | 设置后的高度。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.getTitlebarInfo

<!-- api-schema:begin window.getTitlebarInfo -->
一并报告主窗口的标题栏高度、标题栏按钮宽度与最大化状态，物理像素。只看主窗口；没有主窗口时报 `32`、`138`、`46` 与 `false`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `height` | `integer` | 标题栏高度，物理像素。 |
| `captionButtonsWidth` | `integer` | 三个标题栏按钮的总宽度。 |
| `captionButtonWidth` | `integer` | 单个标题栏按钮的宽度。 |
| `isMaximized` | `boolean` | 主窗口是否最大化。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.getCaptionButtonsWidth

<!-- api-schema:begin window.getCaptionButtonsWidth -->
报告主窗口标题栏按钮（最小化、最大化、关闭）的宽度，按其 DPI 换算的物理像素。只看主窗口；没有主窗口时报 `138` 与 `46`，不失败。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `width` | `integer` | 三个按钮的总宽度。 |
| `buttonWidth` | `integer` | 单个按钮的宽度。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.setDragRegions

<!-- api-schema:begin window.setDragRegions -->
设置拖动调用方窗口（主窗口或 popup）的矩形，替换之前的设置。矩形是页面的 CSS 像素，按窗口 DPI 缩放；宽或高不为正的矩形被跳过。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `regions` | `WindowRegion[]` | 否 | 拖动矩形；省略时为空。 |
| `regions[].x` | `number` | 否 | 左边缘。默认 `0`。 |
| `regions[].y` | `number` | 否 | 上边缘。默认 `0`。 |
| `regions[].width` | `number` | 否 | 宽度。默认 `0`。 |
| `regions[].height` | `number` | 否 | 高度。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 保留下来的矩形个数。 |
| `dpiScale` | `number` | 矩形所乘的缩放比例，即窗口 DPI 除以 96。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('window.setDragRegions', {
    regions: [
        { x: 0, y: 0, width: 800, height: 32 }
    ]
});
```

## window.clearDragRegions

<!-- api-schema:begin window.clearDragRegions -->
清除调用方窗口的拖动矩形。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## window.getPopupBehavior

<!-- api-schema:begin window.getPopupBehavior -->
报告 popup 的行为预设、在它上面设置的覆盖项与实际生效的行为。只适用于 popup：传 `main` 以 `NOT_SUPPORTED` 失败；不传 `windowId` 时取调用方 popup，调用方不是 popup 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | popup 的 id；省略或为空串时为调用方 popup。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
| `profile` | `"legacy" \| "standard" \| "miniPlayer" \| "desktopLyrics"` | 行为预设；创建时没给预设为 `legacy`。 |
| `behavior` | `Record<string, any>` | 在 popup 上设置的行为覆盖。 |
| `resolvedBehavior` | `WindowPopupBehaviorState` | 实际生效的行为。 |
| `resolvedBehavior.showInTaskbar` | `boolean` | 是否出现在任务栏上。 |
| `resolvedBehavior.showInAltTab` | `boolean` | 是否出现在 Alt+Tab 里。 |
| `resolvedBehavior.keepVisibleOnShowDesktop` | `boolean` | 显示桌面时是否仍然可见。 |
| `resolvedBehavior.allowMinimize` | `boolean` | 能否最小化。 |
| `resolvedBehavior.owner` | `"none" \| "main"` | `main` 让 popup 位于主窗口之上并随主窗口最小化；`none` 让它独立。 |
| `resolvedBehavior.noActivate` | `boolean` | 显示或点击 popup 时是否不抢焦点。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const info = await fb2k.invoke('window.getPopupBehavior');
if (info.success === false) throw new Error(info.error);
console.log(info.resolvedBehavior.showInTaskbar);
```

## window.setPopupBehavior

<!-- api-schema:begin window.setPopupBehavior -->
运行时修改 popup 的行为预设或逐字段覆盖，然后以 `window:behaviorChanged` 通告结果。`profile` 与 `behavior` 互相独立：只传一个不影响另一个。目标的选取同 `window.getPopupBehavior`；认不出的预设以 `INVALID_PARAMS` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | popup 的 id；省略或为空串时为调用方 popup。 |
| `profile` | `string` | 否 | 新的预设：`standard`、`miniPlayer` 或 `desktopLyrics`，不区分大小写，可写 `-` 或 `_`（`mini-player`）。 |
| `behavior` | `Record<string, any>` | 否 | 合并进现有覆盖的新覆盖；值为 `null` 的键删除该覆盖。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
| `profile` | `"legacy" \| "standard" \| "miniPlayer" \| "desktopLyrics"` | 行为预设。 |
| `behavior` | `Record<string, any>` | 合并之后的行为覆盖。 |
| `resolvedBehavior` | `WindowPopupBehaviorState` | 实际生效的行为。 |
| `resolvedBehavior.showInTaskbar` | `boolean` | 是否出现在任务栏上。 |
| `resolvedBehavior.showInAltTab` | `boolean` | 是否出现在 Alt+Tab 里。 |
| `resolvedBehavior.keepVisibleOnShowDesktop` | `boolean` | 显示桌面时是否仍然可见。 |
| `resolvedBehavior.allowMinimize` | `boolean` | 能否最小化。 |
| `resolvedBehavior.owner` | `"none" \| "main"` | `main` 让 popup 位于主窗口之上并随主窗口最小化；`none` 让它独立。 |
| `resolvedBehavior.noActivate` | `boolean` | 显示或点击 popup 时是否不抢焦点。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

字段优先级：`behavior.*` 字段覆盖 > `profile` 默认。

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

<!-- api-schema:begin window.getBackdropPolicy -->
报告窗口的背景策略：在它上面设置的覆盖项与实际生效的策略。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |
| `backdropPolicy` | `Record<string, any>` | 在窗口上设置的背景策略覆盖。 |
| `resolvedBackdropPolicy` | `WindowBackdropPolicyState` | 实际生效的背景策略。 |
| `resolvedBackdropPolicy.activeEffect` | `"inherit" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | 窗口激活时的背景；`inherit` 跟随 foobar2000 首选项。 |
| `resolvedBackdropPolicy.inactiveEffect` | `"inherit" \| "system" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | 窗口失焦时的背景：`inherit` 沿用激活时的效果、由 Windows 调暗，`system` 交还平台背景。 |
| `resolvedBackdropPolicy.darkMode` | `boolean` | 背景是否用深色变体。 |
| `resolvedBackdropPolicy.reapplyOnActivate` | `boolean` | 每次激活时是否重新写入背景。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const info = await fb2k.invoke('window.getBackdropPolicy');
if (info.success === false) throw new Error(info.error);
console.log(info.resolvedBackdropPolicy.activeEffect); // 'mica'
```

## window.setBackdropPolicy

<!-- api-schema:begin window.setBackdropPolicy -->
把逐字段覆盖合并进窗口的背景策略；值为 `null` 的键删除该覆盖。即使窗口没能立即画出结果（例如启动时仍隐藏），覆盖也会保存；此时调用以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `backdropPolicy` | `Record<string, any>` | 是 | 以 `resolvedBackdropPolicy` 的键为键的覆盖，合并进现有覆盖；值为 `null` 的键删除该覆盖。其他键照存照回显但不起作用，取值表之外的效果名不改变正在使用的效果。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |
| `backdropPolicy` | `Record<string, any>` | 合并之后的背景策略覆盖。 |
| `resolvedBackdropPolicy` | `WindowBackdropPolicyState` | 实际生效的背景策略。 |
| `resolvedBackdropPolicy.activeEffect` | `"inherit" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | 窗口激活时的背景；`inherit` 跟随 foobar2000 首选项。 |
| `resolvedBackdropPolicy.inactiveEffect` | `"inherit" \| "system" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | 窗口失焦时的背景：`inherit` 沿用激活时的效果、由 Windows 调暗，`system` 交还平台背景。 |
| `resolvedBackdropPolicy.darkMode` | `boolean` | 背景是否用深色变体。 |
| `resolvedBackdropPolicy.reapplyOnActivate` | `boolean` | 每次激活时是否重新写入背景。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

// 失焦沿用已解析的 activeEffect，由 DWM 做失焦变暗
await fb2k.invoke('window.setBackdropPolicy', {
    backdropPolicy: {
        activeEffect: 'mica',
        inactiveEffect: 'inherit'
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

<!-- api-schema:begin window.broadcast -->
给除调用方以外的每个窗口发消息，以 `window:message`（`{ sourceWindowId, message }`）送达。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `any` | 是 | 消息，作为事件的 `message` 送达。`null` 按没传处理。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.broadcast', { message: { type: 'themeChanged', theme: 'dark' } });
```


### window.cancelClose

<!-- api-schema:begin window.cancelClose -->
收到 `window:beforeClose` 后让调用方 popup 保持打开；没有待定的关闭时什么也不做。调用方不是 popup 时以 `NOT_FOUND` 失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.cancelClose');
```


### window.center

<!-- api-schema:begin window.center -->
把调用方窗口移到所在显示器工作区的中央，大小不变。面板模式与没有窗口时失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.center');
```


### window.clearClickThroughExcludeRegions

<!-- api-schema:begin window.clearClickThroughExcludeRegions -->
清除 popup 的鼠标穿透排除矩形。目标的选取同 `window.setClickThrough`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | popup 的 id；省略或为空串时为调用方窗口。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
// 仅作用于 popup；只有从该 popup 自身调用时才可省略 windowId
await fb2k.invoke('window.clearClickThroughExcludeRegions', { windowId: 'popup-1' });
```


### window.clearNoDragRegions

<!-- api-schema:begin window.clearNoDragRegions -->
清除调用方窗口的不可拖动矩形。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.clearNoDragRegions');
```


### window.close

<!-- api-schema:begin window.close -->
像系统关闭命令那样关闭调用方窗口。没有窗口时什么也不做并照常成功。面板模式下失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.close');
```


### window.closeAllPopups

<!-- api-schema:begin window.closeAllPopups -->
像点各自的关闭按钮那样关闭所有 popup。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.closeAllPopups');
```


### window.closePopup

<!-- api-schema:begin window.closePopup -->
像点关闭按钮那样关闭一个 popup，所以以 `beforeClose` 创建的 popup 会先收到 `window:beforeClose`。传 `main` 以 `INVALID_PARAMS` 失败，没有 popup 是该 id 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 是 | 要关闭的 popup 的 id。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
// 此处 windowId 为必填，不会回退到调用方窗口
await fb2k.invoke('window.closePopup', { windowId: 'popup-1' });
```


### window.confirmClose

<!-- api-schema:begin window.confirmClose -->
收到 `window:beforeClose` 后让调用方 popup 关闭；没有待定的关闭时什么也不做。调用方不是 popup 时以 `NOT_FOUND` 失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.confirmClose');
```


### window.createPopup

<!-- api-schema:begin window.createPopup -->
打开一个拥有独立 WebView 与桥的 popup 窗口；最多同时打开 8 个。从 DUI/CUI 面板也能调用。已有 8 个 popup 或窗口创建失败时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 否 | 要加载的页面。`http://`、`https://`、`file:///` 或 `data:` 地址原样加载；含 `.html` 的路径从主题（或开发服务器）加载；其他值（含空串）加载主题的 `index.html`，并把该值作为 `route` 查询参数。除 `data:` 外每个地址都会加上 `windowId` 查询参数。`http://` 或 `https://` 地址的页面只有在调用方页面已经信任该来源时才能使用桥，其他绝对地址加载的页面都不能。这样的页面照常显示，但调用以 `ORIGIN_DENIED` 失败，也收不到任何事件。默认 `""`。 |
| `title` | `string` | 否 | 窗口标题。默认 `""`。 |
| `x` | `number` | 否 | 左边缘的屏幕坐标；省略时由 Windows 决定位置。小数部分舍去。 |
| `y` | `number` | 否 | 上边缘的屏幕坐标；省略时由 Windows 决定位置。小数部分舍去。 |
| `width` | `number` | 否 | 宽度，物理像素；小数部分舍去。默认 `400`。 |
| `height` | `number` | 否 | 高度，物理像素；小数部分舍去。默认 `300`。 |
| `minWidth` | `number` | 否 | 调整大小时的最小宽度，物理像素。默认 `200`。 |
| `minHeight` | `number` | 否 | 调整大小时的最小高度，物理像素。默认 `150`。 |
| `maxWidth` | `number` | 否 | 调整大小时的最大宽度，物理像素；`0` 表示不设上限。默认 `0`。 |
| `maxHeight` | `number` | 否 | 调整大小时的最大高度，物理像素；`0` 表示不设上限。默认 `0`。 |
| `resizable` | `boolean` | 否 | 用户能否调整 popup 的大小。默认 `true`。 |
| `frame` | `boolean` | 否 | 是否画原生边框与标题栏；`false` 创建无边框 popup。默认 `true`。 |
| `transparent` | `boolean` | 否 | 背景是否透明。默认 `false`。 |
| `alwaysOnTop` | `boolean` | 否 | 让 popup 保持在所有其他窗口之上。只想让它位于主窗口之上时，改用 `standard` 预设或 `behavior.owner: "main"`。默认 `false`。 |
| `showInTaskbar` | `boolean` | 否 | popup 是否出现在任务栏与 Alt+Tab 里；省略时由预设决定。 |
| `clickThrough` | `boolean` | 否 | 让鼠标输入穿过 popup。默认 `false`。 |
| `beforeClose` | `boolean` | 否 | 关闭前先发 `window:beforeClose`，等待 `window.confirmClose` 或 `window.cancelClose`。默认 `false`。 |
| `profile` | `string` | 否 | 行为预设：`standard`、`miniPlayer` 或 `desktopLyrics`，不区分大小写，可写 `-` 或 `_`（`mini-player`）；认不出的值按 `standard`。省略时不用预设。 |
| `behavior` | `Record<string, any>` | 否 | 叠加在预设之上的行为覆盖，键同 `window.getPopupBehavior` 的 `resolvedBehavior`。 |
| `backdropPolicy` | `Record<string, any>` | 否 | 叠加在预设之上的背景策略覆盖，键同 `window.getBackdropPolicy` 的 `resolvedBackdropPolicy`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 新 popup 的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
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
进入全屏。窗口已经全屏时以 `OPERATION_FAILED` 失败。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isFullscreen` | `boolean` | 恒为 `true`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.enterFullscreen');
```


### window.exitFullscreen

<!-- api-schema:begin window.exitFullscreen -->
退出全屏并恢复进入前的状态。窗口不在全屏时以 `OPERATION_FAILED` 失败。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isFullscreen` | `boolean` | 恒为 `false`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.exitFullscreen');
```


### window.flash

<!-- api-schema:begin window.flash -->
闪烁调用方窗口的标题栏与任务栏按钮以吸引注意，或停止闪烁。面板模式与没有窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 否 | `true` 开始闪烁，`false` 停止。默认 `true`。 |
| `count` | `integer` | 否 | 闪烁次数。默认 `3`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
// 省略 count 与 enabled 即闪烁 3 次
await fb2k.invoke('window.flash');
```


### window.flashTaskbar

<!-- api-schema:begin window.flashTaskbar -->
让调用方窗口的任务栏按钮与标题栏闪烁若干次。面板模式与没有窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `count` | `integer` | 否 | 闪烁次数。默认 `3`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
// 省略 count 即闪烁 3 次
await fb2k.invoke('window.flashTaskbar');
```


### window.getAllWindows

<!-- api-schema:begin window.getAllWindows -->
列出主窗口与每个 popup，连同它们的策略、能力与矩形。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `WindowInfo[]` | 主窗口在前，其后是各 popup。 |
| `items[].windowId` | `string` | `main` 或 popup 的 id。 |
| `items[].isMain` | `boolean` | 是否为主窗口。 |
| `items[].title` | `string` | 窗口标题。 |
| `items[].url` | `string` | 仅 popup：创建 popup 时给的 `url`。 |
| `items[].profile` | `"legacy" \| "standard" \| "miniPlayer" \| "desktopLyrics"` | 仅 popup：行为预设；创建时没给预设为 `legacy`。 |
| `items[].behavior` | `Record<string, any>` | 仅 popup：在 popup 上设置的行为覆盖。 |
| `items[].resolvedBehavior` | `WindowPopupBehaviorState` | 仅 popup：实际生效的行为。 |
| `items[].resolvedBehavior.showInTaskbar` | `boolean` | 是否出现在任务栏上。 |
| `items[].resolvedBehavior.showInAltTab` | `boolean` | 是否出现在 Alt+Tab 里。 |
| `items[].resolvedBehavior.keepVisibleOnShowDesktop` | `boolean` | 显示桌面时是否仍然可见。 |
| `items[].resolvedBehavior.allowMinimize` | `boolean` | 能否最小化。 |
| `items[].resolvedBehavior.owner` | `"none" \| "main"` | `main` 让 popup 位于主窗口之上并随主窗口最小化；`none` 让它独立。 |
| `items[].resolvedBehavior.noActivate` | `boolean` | 显示或点击 popup 时是否不抢焦点。 |
| `items[].backdropPolicy` | `Record<string, any>` | 在窗口上设置的背景策略覆盖。 |
| `items[].resolvedBackdropPolicy` | `WindowBackdropPolicyState` | 实际生效的背景策略。 |
| `items[].resolvedBackdropPolicy.activeEffect` | `"inherit" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | 窗口激活时的背景；`inherit` 跟随 foobar2000 首选项。 |
| `items[].resolvedBackdropPolicy.inactiveEffect` | `"inherit" \| "system" \| "none" \| "mica" \| "mica-alt" \| "acrylic"` | 窗口失焦时的背景：`inherit` 沿用激活时的效果、由 Windows 调暗，`system` 交还平台背景。 |
| `items[].resolvedBackdropPolicy.darkMode` | `boolean` | 背景是否用深色变体。 |
| `items[].resolvedBackdropPolicy.reapplyOnActivate` | `boolean` | 每次激活时是否重新写入背景。 |
| `items[].capabilities` | `WindowObservationCapabilities` | 窗口支持的功能。 |
| `items[].capabilities.supportsBackdropPolicy` | `boolean` | `window.setBackdropPolicy` 是否生效。 |
| `items[].capabilities.supportsFrameless` | `boolean` | `window.setFrameless` 是否生效。 |
| `items[].capabilities.supportsCornerPreference` | `boolean` | `window.setCornerPreference` 是否生效。 |
| `items[].capabilities.supportsPopupBehavior` | `boolean` | `window.setPopupBehavior` 是否生效。 |
| `items[].capabilities.supportsMicaAlt` | `boolean` | 能否画 Mica Alt 背景。 |
| `items[].capabilities.supportsFullscreen` | `boolean` | 能否进入全屏。 |
| `items[].capabilities.supportsOwnerPolicy` | `boolean` | `behavior.owner` 是否生效。 |
| `items[].capabilities.supportsNoActivate` | `boolean` | `behavior.noActivate` 是否生效。 |
| `items[].capabilities.supportsBeforeClose` | `boolean` | `beforeClose` 是否生效。 |
| `items[].bounds` | `WindowBounds` | 窗口矩形。 |
| `items[].bounds.x` | `integer` | 左边缘。 |
| `items[].bounds.y` | `integer` | 上边缘。 |
| `items[].bounds.width` | `integer` | 宽度。 |
| `items[].bounds.height` | `integer` | 高度。 |
| `items[].shell` | `any` | 窗口外壳的诊断快照（生命周期与启动状态）；形状可能随版本变化。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getAllWindows');
```


### window.getBounds

<!-- api-schema:begin window.getBounds -->
报告调用方窗口的矩形：屏幕坐标，物理像素，含边框。最小化时报的是 Windows 停放窗口的位置，不是还原后的几何。没有窗口时矩形全为 0。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `x` | `integer` | 左边缘的屏幕坐标。 |
| `y` | `integer` | 上边缘的屏幕坐标。 |
| `width` | `integer` | 宽度，物理像素，含边框。 |
| `height` | `integer` | 高度，物理像素，含边框。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getBounds');
```


### window.getCornerPreference

<!-- api-schema:begin window.getCornerPreference -->
报告主窗口最后一次设置的圆角；没有主窗口时为 `default`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `string` | 最后一次设置的圆角。 |
| `preference` | `string` | 与 `mode` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

无主窗口时返回 `"default"` 而非错误。

```js
const result = await fb2k.invoke('window.getCornerPreference');
```


### window.getCurrentWindowId

<!-- api-schema:begin window.getCurrentWindowId -->
报告调用方窗口的 id：`main`、popup 的 id 或面板的 id。无法匹配的调用方得到 `main`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 调用方窗口的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getCurrentWindowId');
```


### window.getDevServerConfig

<!-- api-schema:begin window.getDevServerConfig -->
报告页面是否从开发服务器而不是已安装的文件加载，以及开发服务器的地址。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `useDevServer` | `boolean` | 页面是否从开发服务器加载。 |
| `devServerUrl` | `string` | 开发服务器的地址。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getDevServerConfig');
```


### window.getMaxSize

<!-- api-schema:begin window.getMaxSize -->
报告窗口请求的最大尺寸；`0` 表示不设上限。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `width` | `integer` | 最大宽度，按窗口当前 DPI 换算的物理像素；`0` 表示不设上限。 |
| `height` | `integer` | 最大高度，按窗口当前 DPI 换算的物理像素；`0` 表示不设上限。 |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

数值单位为物理像素；宿主以 DIP 存储约束，并按**目标窗口**的 DPI 换算。`0` 表示无上限，且换算前后精确不变。

返回的是**请求值**，不是当前窗口尺寸。「物理 → DIP → 物理」的往返会量化到整数 DIP，故在非 100% 缩放下 `get` 与传给 `set` 的值每轴最多相差 1px（例如 125% 下 `202px` 读回为 `203px`）。请把 getter 理解为「在 ±1px 内复述你设置的约束」，而非精确相等。`0` 不受此影响。

```js
const result = await fb2k.invoke('window.getMaxSize');
```


### window.getMinSize

<!-- api-schema:begin window.getMinSize -->
报告窗口请求的最小尺寸，即最后一次设置的值，而不是窗口实际的尺寸。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `width` | `integer` | 最小宽度，按窗口当前 DPI 换算的物理像素。 |
| `height` | `integer` | 最小高度，按窗口当前 DPI 换算的物理像素。 |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

数值单位为物理像素；宿主以 DIP 存储约束，并按**目标窗口**的 DPI 换算。

返回的是**请求值**，不是当前窗口尺寸。「物理 → DIP → 物理」的往返会量化到整数 DIP，故在非 100% 缩放下 `get` 与传给 `set` 的值每轴最多相差 1px（例如 125% 下 `202px` 读回为 `203px`）。请把 getter 理解为「在 ±1px 内复述你设置的约束」，而非精确相等。

```js
const result = await fb2k.invoke('window.getMinSize');
```


### window.getMode

<!-- api-schema:begin window.getMode -->
报告调用方页面的宿主模式和窗口 id。主窗口与 popup 都返回 `standalone` 和 `panelMode: false`，从面板打开的 popup 也如此。已注册且有 id 的窗口与 `window.getCurrentWindowId` 返回相同的 id。需要适配面板模式的页面在启动时读取它；DUI/CUI 面板还会以 `panel:initialized` 事件通告同样的字段。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"standalone" \| "dui" \| "cui" \| "panel" \| "unknown"` | `standalone`、`dui` 或 `cui`；页面在面板里但分辨不出类型时为 `panel`，面板类型未知时为 `unknown`。 |
| `panelMode` | `boolean` | 页面是否在 DUI 或 CUI 面板里；主窗口与 popup 为 `false`。 |
| `windowId` | `string` | 调用方窗口的 id：主窗口为 `main`，popup 为实际 popup id，面板为面板 id。面板没有 id 时为 `panel`；无法匹配的调用方为 `main`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getMode');
```


### window.getTitlebarHeight

<!-- api-schema:begin window.getTitlebarHeight -->
报告调用方窗口（主窗口或 popup）的标题栏高度，物理像素。两者都不是的调用方得到默认值 `32`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `height` | `integer` | 标题栏高度，物理像素。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getTitlebarHeight');
```


### window.getZoom

<!-- api-schema:begin window.getZoom -->
报告调用方窗口 WebView 的缩放倍数与窗口的 DPI。调用方窗口自己没有 WebView 时报缩放 `1`，并省略 `dpi` 与 `dpiScale`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `zoom` | `number` | 缩放倍数；`1` 即 100 %。 |
| `dpi` | `integer` | 窗口的 DPI；没有 WebView 时省略。 |
| `dpiScale` | `number` | `dpi / 96`；没有 WebView 时省略。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.getZoom');
```


### window.isAlwaysOnTop

<!-- api-schema:begin window.isAlwaysOnTop -->
报告调用方窗口是否保持在其他窗口之上；没有窗口时为 `false`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 窗口是否保持在其他窗口之上。 |
| `isAlwaysOnTop` | `boolean` | 与 `enabled` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.isAlwaysOnTop');
```


### window.isClickThrough

<!-- api-schema:begin window.isClickThrough -->
报告鼠标输入是否穿过 popup。目标的选取同 `window.setClickThrough`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | popup 的 id；省略或为空串时为调用方窗口。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clickThrough` | `boolean` | 鼠标输入是否穿过。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('window.isClickThrough', { windowId: 'popup-1' });
if (res.success === false) throw new Error(res.error);
const { clickThrough } = res;
```


### window.isResizable

<!-- api-schema:begin window.isResizable -->
报告用户能否拖动边框调整窗口大小。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `resizable` | `boolean` | 用户能否拖动边框调整窗口大小。 |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

所有窗口 shell（含完全无边框的 popup）都支持通过 [`window.setResizable`](#window-setresizable) 在运行时改变它。

```js
const result = await fb2k.invoke('window.isResizable');
```


### window.maximize

<!-- api-schema:begin window.maximize -->
以系统动画最大化调用方窗口；窗口处于全屏时先退出全屏。请求是投递的。面板模式与没有窗口时失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.maximize');
```


### window.minimize

<!-- api-schema:begin window.minimize -->
以系统动画最小化调用方窗口。请求是投递的，窗口在调用返回之后才变状态。面板模式与没有窗口时失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.minimize');
```


### window.refreshWebView

<!-- api-schema:begin window.refreshWebView -->
重绘调用方窗口的 WebView，清除背景效果变化后残留的画面。调用方窗口自己没有 WebView 时（例如 DUI/CUI 面板）以 `NOT_FOUND` 失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.refreshWebView');
```


### window.reload

<!-- api-schema:begin window.reload -->
重新加载调用方窗口的页面。调用方窗口自己没有 WebView 时（例如 DUI/CUI 面板）以 `NOT_FOUND` 失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.reload');
```


### window.resetZoom

<!-- api-schema:begin window.resetZoom -->
把调用方窗口 WebView 的缩放倍数恢复为 `1`；与 `window.setZoom` 一样，此后首选项的默认缩放不再作用于它。调用方窗口自己没有 WebView 时以 `NOT_FOUND` 失败，WebView2 拒绝该倍数时以 `OPERATION_FAILED` 失败。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `zoom` | `number` | 恒为 `1`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.resetZoom');
```


### window.restore

<!-- api-schema:begin window.restore -->
以系统动画把调用方窗口从最小化或最大化还原。全屏的窗口改为退出全屏，回到全屏前的状态。面板模式与没有窗口时失败。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.restore');
```


### window.sendMessage

<!-- api-schema:begin window.sendMessage -->
给一个窗口发消息，以 `window:message`（`{ sourceWindowId, message }`）送达。没有窗口是该 id 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `targetWindowId` | `string` | 是 | 接收窗口的 id。不能为空。 |
| `message` | `any` | 是 | 消息，作为事件的 `message` 送达。`null` 按没传处理。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.sendMessage', {
    targetWindowId: 'popup-1',
    message: { type: 'seek', position: 42 },
});
```


### window.setAcrylic

<!-- api-schema:begin window.setAcrylic -->
打开或关闭窗口的亚克力背景。即使窗口没能立即画出，设置也会保存；此时调用以 `OPERATION_FAILED` 失败，失败里带返回值的字段。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `enabled` | `boolean` | 否 | `true` 显示亚克力；`false` 去掉经 `setMica` 或 `setAcrylic` 设置的背景。默认 `true`。 |
| `darkMode` | `boolean` | 否 | 深色（`true`）或浅色（`false`）背景；省略时不变。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 回显 `enabled`。 |
| `darkMode` | `boolean` | 回显 `darkMode`；传了才有。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`success` 反映材质是否真正应用成功——即使窗口有效，平台拒绝该效果时也可能为 `false`。

```js
await fb2k.invoke('window.setAcrylic', { enabled: true, darkMode: true });
```


### window.setAlwaysOnTop

<!-- api-schema:begin window.setAlwaysOnTop -->
让调用方窗口保持在其他窗口之上，或取消。面板模式与没有窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 否 | 是否让窗口保持在其他窗口之上。默认 `true`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setAlwaysOnTop', { enabled: true });
```


### window.setBackgroundTransparency

<!-- api-schema:begin window.setBackgroundTransparency -->
让窗口的 WebView 背景透明（背景效果得以透出）或不透明。面板模式下失败；窗口与其 WebView 都没接受时以 `OPERATION_FAILED` 失败，窗口有 WebView 时失败里带返回值的字段。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `transparent` | `boolean` | 否 | `true` 为透明背景，`false` 为不透明。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `transparent` | `boolean` | 回显 `transparent`。 |
| `description` | `string` | 描述新状态的一句英文，供日志使用。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setBackgroundTransparency', { transparent: true });
```


### window.setBlur

<!-- api-schema:begin window.setBlur -->
打开或关闭窗口的背景模糊。即使窗口没能立即画出，设置也会保存；此时调用以 `OPERATION_FAILED` 失败，失败里带 `enabled`。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `enabled` | `boolean` | 否 | 是否模糊窗口后面的内容。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 回显 `enabled`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setBlur', { enabled: true });
```


### window.setClickThrough

<!-- api-schema:begin window.setClickThrough -->
让鼠标输入穿过 popup 落到下面的窗口，经 `window.setClickThroughExcludeRegions` 设置的矩形除外。只适用于 popup：不传 `windowId` 时取调用方窗口，它不是 popup 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | popup 的 id；省略或为空串时为调用方窗口。 |
| `enabled` | `boolean` | 否 | 鼠标输入是否穿过。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clickThrough` | `boolean` | 现在鼠标输入是否穿过。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setClickThrough', { enabled: true });
```


### window.setClickThroughExcludeRegions

<!-- api-schema:begin window.setClickThroughExcludeRegions -->
设置 popup 在鼠标穿透时仍接收鼠标输入的矩形，替换之前的设置。矩形是页面的 CSS 像素，按 popup 的 DPI 缩放；宽或高不为正的矩形被跳过，最多保留 32 个。目标的选取同 `window.setClickThrough`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | popup 的 id；省略或为空串时为调用方窗口。 |
| `regions` | `WindowRegion[]` | 否 | 矩形；省略时为空。 |
| `regions[].x` | `number` | 否 | 左边缘。默认 `0`。 |
| `regions[].y` | `number` | 否 | 上边缘。默认 `0`。 |
| `regions[].width` | `number` | 否 | 宽度。默认 `0`。 |
| `regions[].height` | `number` | 否 | 高度。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | popup 的 id。 |
| `count` | `integer` | 保留下来的矩形个数。 |
| `dpiScale` | `number` | 矩形所乘的缩放比例，即 popup 的 DPI 除以 96。 |
| `warning` | `string` | 给出的矩形超过 32 个时为 `regions truncated to 32`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setClickThroughExcludeRegions', {
    regions: [{ x: 12, y: 12, width: 160, height: 40 }],
});
```


### window.setCornerPreference

<!-- api-schema:begin window.setCornerPreference -->
设置主窗口的 Windows 11 圆角。不论哪个窗口调用都只作用于主窗口；popup 自行管理圆角。面板模式与没有主窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `string` | 否 | `default` 或 `round` 为圆角，`small` 为小圆角，`none` 为直角。其他值按圆角处理，并原样报回。默认 `"default"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`"default"` 映射为圆角，因为无边框窗口没有标准非客户区框架可供系统默认值生效。

```js
await fb2k.invoke('window.setCornerPreference', { mode: 'round' });
```


### window.setDarkMode

<!-- api-schema:begin window.setDarkMode -->
在深色与浅色之间切换窗口的背景。即使窗口没能立即画出，设置也会保存；此时调用以 `OPERATION_FAILED` 失败，失败里带 `enabled`。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `enabled` | `boolean` | 否 | `true` 为深色，`false` 为浅色。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 回显 `enabled`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setDarkMode', { enabled: true });
```


### window.setDevServerConfig

<!-- api-schema:begin window.setDevServerConfig -->
修改开发服务器设置；省略的键保持原值。下次加载页面时生效。响应报告两项设置保存后的值。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `useDevServer` | `boolean` | 否 | 页面是否从开发服务器加载。 |
| `devServerUrl` | `string` | 否 | 开发服务器的地址，例如 `http://localhost:5173`；原样保存。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `useDevServer` | `boolean` | 保存后的「页面是否从开发服务器加载」。 |
| `devServerUrl` | `string` | 保存后的开发服务器地址。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setDevServerConfig', {
    useDevServer: true,
    devServerUrl: 'http://localhost:5173',
});
```


### window.setFrameless

<!-- api-schema:begin window.setFrameless -->
去掉或恢复窗口的原生边框与标题栏。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `frameless` | `boolean` | 否 | `true` 去掉边框，`false` 恢复。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `frameless` | `boolean` | 窗口现在是否无边框。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setFrameless', { frameless: true });
```


### window.setMaximizeButtonRegion

<!-- api-schema:begin window.setMaximizeButtonRegion -->
告诉宿主页面把主窗口的最大化键画在哪里，这样在 Windows 11 上悬停它会像标准窗口的按钮一样弹出贴靠布局。矩形是页面的 CSS 像素，按窗口 DPI 与页面缩放换算，与 `devicePixelRatio` 同一个系数，替换之前的矩形；省略 `region`，或宽高不为正，则移除。窗口无标题栏、可调整大小且不在全屏时，宿主把这个矩形当作窗口的最大化键回答，并把在这里收到的鼠标输入转给页面，所以按钮照常有悬停与按下样式，照常由它自己的点击处理；在 Windows 10 上和其他状态下，这个矩形仍是普通的页面内容。布局挪动按钮后要重新设置；页面导航或重载时它被移除。只对主窗口：从 popup 调用以 `NOT_SUPPORTED` 失败，面板模式下失败，调用方不是窗口时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `region` | `WindowRegion` | 否 | 页面画最大化键的位置；省略时宿主忘掉这个按钮。 |
| `region.x` | `number` | 否 | 左边缘。默认 `0`。 |
| `region.y` | `number` | 否 | 上边缘。默认 `0`。 |
| `region.width` | `number` | 否 | 宽度。默认 `0`。 |
| `region.height` | `number` | 否 | 高度。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hasRegion` | `boolean` | 现在是否设着矩形：移除之后，或矩形宽高不为正时为 `false`。 |
| `snapLayouts` | `boolean` | 系统会为最大化键提供贴靠布局，即 Windows 11 或更新。为 `false` 时宿主从不把这个矩形当作最大化键。为 `true` 时，页面悬停时仍会显示按钮的 `title` 提示，它会挡住贴靠布局浮层，所以不要设 title，改用 `aria-label` 给按钮命名。 |
| `scale` | `number` | 矩形所乘的缩放比例：窗口 DPI 除以 96，再乘页面缩放。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

Windows 11 只在窗口的最大化键上弹出贴靠布局，页面自绘的按钮要由宿主把那块矩形
当作按钮回答。这样按钮就不再直接从 Windows 收到鼠标，由宿主转交，所以 `:hover`、
`:active` 与 `click` 照常有效，最大化仍由页面自己的点击处理完成。矩形要保持最新：
首次布局后设置一次，按钮挪动时（窗口缩放、最大化与还原、DPI 或缩放变化）再设置。
`snapLayouts` 为 `true` 时去掉按钮的 `title`，改用 `aria-label` 命名，否则提示会挡住
浮层。

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
设置窗口的最大尺寸；`0` 表示不设上限，小数部分舍去。宿主按目标窗口的 DIP 保存，所以读回时可能差 1 px（`0` 读回仍是 `0`）。窗口比新的上限大时立即缩小；最大化、全屏、最小化时不动。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `width` | `number` | 否 | 最大宽度，物理像素；`0` 表示不设上限。默认 `0`。 |
| `height` | `number` | 否 | 最大高度，物理像素；`0` 表示不设上限。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

数值单位为物理像素，按**目标窗口**的 DPI 换算为宿主的 DIP 存储；`0`（或负值）表示清除该上限。施加约束后会立即校正当前窗口尺寸，故已超出新上限的窗口会被收缩，而非等到下次用户拖拽。

```js
await fb2k.invoke('window.setMaxSize', { width: 1920, height: 1080 });
```


### window.setMica

<!-- api-schema:begin window.setMica -->
打开或关闭窗口的 Mica 背景。即使窗口没能立即画出（例如启动时仍隐藏），设置也会保存；此时调用以 `OPERATION_FAILED` 失败，失败里带返回值的字段。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `enabled` | `boolean` | 否 | `true` 显示 Mica；`false` 去掉经 `setMica` 或 `setAcrylic` 设置的背景。默认 `true`。 |
| `variant` | `string` | 否 | `mica-alt` 为标签页变体；其他值都按 `mica`。默认 `"mica"`。 |
| `darkMode` | `boolean` | 否 | 深色（`true`）或浅色（`false`）背景；省略时不变。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 回显 `enabled`。 |
| `variant` | `string` | 实际使用的变体，`mica` 或 `mica-alt`。 |
| `darkMode` | `boolean` | 回显 `darkMode`；传了才有。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

只有 `mica-alt` 会选择备用变体；其余取值（包括无法识别的值）都会被**静默归一化**为 `mica` 而不报错。返回的 `variant` 是归一化后的请求值，不是屏幕上实际的效果：popup 不支持 Mica Alt、会被降级，所以对 popup 可能回 `variant: "mica-alt"` 而实际得到别的背景。`success` 反映材质是否真正应用成功——即使窗口有效，平台拒绝该效果时也可能为 `false`。

```js
await fb2k.invoke('window.setMica', { enabled: true, variant: 'mica-alt' });
```


### window.setMicaEffect

<!-- api-schema:begin window.setMicaEffect -->
与 `window.setMica` 相同。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `enabled` | `boolean` | 否 | `true` 显示 Mica；`false` 去掉经 `setMica` 或 `setAcrylic` 设置的背景。默认 `true`。 |
| `variant` | `string` | 否 | `mica-alt` 为标签页变体；其他值都按 `mica`。默认 `"mica"`。 |
| `darkMode` | `boolean` | 否 | 深色（`true`）或浅色（`false`）背景；省略时不变。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 回显 `enabled`。 |
| `variant` | `string` | 实际使用的变体，`mica` 或 `mica-alt`。 |
| `darkMode` | `boolean` | 回显 `darkMode`；传了才有。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setMicaEffect', { enabled: true });
```


### window.setMinSize

<!-- api-schema:begin window.setMinSize -->
设置窗口的最小尺寸，小数部分舍去。宿主按目标窗口的 DIP 保存，所以读回时可能差 1 px。窗口比新的下限小时立即长大；最大化、全屏时不动，最小化的窗口在还原时再长大。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `width` | `number` | 否 | 最小宽度，物理像素；`0` 及以下降到 1 DIP。默认 `0`。 |
| `height` | `number` | 否 | 最小高度，物理像素；`0` 及以下降到 1 DIP。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

数值单位为物理像素，按**目标窗口**的 DPI 换算为宿主的 DIP 存储，使约束在 DPI 变化后仍然稳定。非正值会归一为 1px 下限。施加约束后会立即校正当前窗口尺寸，故已小于新下限的窗口会被放大，而非等到下次用户拖拽。

```js
await fb2k.invoke('window.setMinSize', { width: 480, height: 320 });
```


### window.setNoDragRegions

<!-- api-schema:begin window.setNoDragRegions -->
设置永远不拖动调用方窗口的矩形（例如拖动区域里的按钮），替换之前的设置。矩形是页面的 CSS 像素，按窗口 DPI 缩放；宽或高不为正的矩形被跳过。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `regions` | `WindowRegion[]` | 否 | 不可拖动矩形；省略时为空。 |
| `regions[].x` | `number` | 否 | 左边缘。默认 `0`。 |
| `regions[].y` | `number` | 否 | 上边缘。默认 `0`。 |
| `regions[].width` | `number` | 否 | 宽度。默认 `0`。 |
| `regions[].height` | `number` | 否 | 高度。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 保留下来的矩形个数。 |
| `dpiScale` | `number` | 矩形所乘的缩放比例，即窗口 DPI 除以 96。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setNoDragRegions', {
    regions: [{ x: 690, y: 0, width: 110, height: 32 }],
});
```


### window.setPosition

<!-- api-schema:begin window.setPosition -->
移动调用方窗口的左上角，大小不变，小数部分舍去。面板模式与没有窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `x` | `number` | 否 | 新的左边缘屏幕坐标。默认 `0`。 |
| `y` | `number` | 否 | 新的上边缘屏幕坐标。默认 `0`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setPosition', { x: 100, y: 100 });
```


### window.setResizable

<!-- api-schema:begin window.setResizable -->
设置用户能否拖动边框调整窗口大小。设为窗口已有的值照常成功。面板模式下失败；Windows 拒绝新样式时以 `OPERATION_FAILED` 失败，失败里带 `windowId`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |
| `resizable` | `boolean` | 否 | 用户能否拖动边框调整窗口大小。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `windowId` | `string` | 调用实际作用的窗口的 id。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

所有窗口 shell 都支持运行时切换，包括完全无边框的 popup（`frame: false` 且 `transparent: true` 且无背景效果）：这类窗口会把整个非客户区收掉，故新增尺寸边框只改变命中测试，不改变外观。因此 `success: false` 表示 Win32 调用**真实失败**（样式未写入或帧刷新失败），而非「窗口形态不支持」；此时请求态不会被提交。

::: warning 行为变更
此前该调用无论来自哪个窗口都硬改主窗口，故从 popup 调用会改错窗口。
:::

```js
await fb2k.invoke('window.setResizable', { resizable: false });
```


### window.setSize

<!-- api-schema:begin window.setSize -->
调整调用方窗口的大小，位置不变，小数部分舍去。尺寸受窗口的最小、最大尺寸约束，响应不说明是否被约束过。面板模式与没有窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `width` | `number` | 否 | 新的宽度，物理像素，含边框。默认 `800`。 |
| `height` | `number` | 否 | 新的高度，物理像素，含边框。默认 `600`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setSize', { width: 1024, height: 640 });
```


### window.setTitle

<!-- api-schema:begin window.setTitle -->
设置调用方窗口的标题，显示在标题栏与任务栏上。面板模式与没有窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 新标题。默认 `"foobar2000"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setTitle', { title: 'Now Playing' });
```


### window.setZoom

<!-- api-schema:begin window.setZoom -->
设置调用方窗口 WebView 的缩放倍数。此后在本次会话里，首选项的默认缩放不再作用于这个 WebView。调用方窗口自己没有 WebView 时以 `NOT_FOUND` 失败；WebView2 拒绝该倍数时以 `OPERATION_FAILED` 失败，失败里带当前生效的 `zoom`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `zoom` | `number` | 否 | 缩放倍数；`1` 即 100 %。默认 `1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `zoom` | `number` | 调用之后生效的缩放倍数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.setZoom', { zoom: 1.25 });
```


### window.setZoomForDpi

<!-- api-schema:begin window.setZoomForDpi -->
把调用方窗口 WebView 的缩放倍数设为 `dpi / 96`；与 `window.setZoom` 一样，此后首选项的默认缩放不再作用于它。调用方窗口自己没有 WebView 时以 `NOT_FOUND` 失败；WebView2 拒绝该倍数时以 `OPERATION_FAILED` 失败，失败里带返回值的字段。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `dpi` | `integer` | 否 | 要匹配的 DPI；`0` 及以下用窗口的 DPI，未知时用 `96`。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `dpi` | `integer` | 实际使用的 DPI。 |
| `zoom` | `number` | 调用之后生效的缩放倍数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
// 省略 dpi 时按调用方窗口当前 DPI 推导缩放
const res = await fb2k.invoke('window.setZoomForDpi');
if (res.success === false) throw new Error(res.error);
const { zoom } = res;
```


### window.startResize

<!-- api-schema:begin window.startResize -->
开始用鼠标从某条边或某个角调整调用方窗口的大小。在 `mousedown` 处理函数里、按键仍按着时调用。面板模式与没有窗口时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `edge` | `string` | 否 | 要拖动的边或角：`left`、`right`、`top`、`bottom`、`topleft`、`topright`、`bottomleft` 或 `bottomright`。其他值都拖右下角。默认 `"bottomright"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('window.startResize', { edge: 'bottomright' });
```


### window.toggleAlwaysOnTop

<!-- api-schema:begin window.toggleAlwaysOnTop -->
切换调用方窗口是否保持在其他窗口之上。面板模式下失败；没有窗口时失败并带 `enabled: false`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 窗口现在是否保持在其他窗口之上。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('window.toggleAlwaysOnTop');
```


### window.toggleFullscreen

<!-- api-schema:begin window.toggleFullscreen -->
进入全屏，窗口已全屏时退出。面板模式下失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `windowId` | `string` | 否 | 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fullscreen` | `boolean` | 窗口现在是否全屏。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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
