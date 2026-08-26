# Taskbar & Tray API

Windows 任务栏缩略图按钮与系统托盘图标。共 19 个 API、5 个事件。

> **前提**：仅在 standalone 窗口模式下可用。DUI/CUI 面板调用时返回 `{ success: false, panelMode: true }`。

---

## Taskbar API — 任务栏缩略图（5 个 API）

鼠标悬停任务栏图标时出现的预览缩略图工具栏，最多 7 个按钮。

### taskbar.setThumbnailButtons

设置缩略图工具栏按钮。**只能调用一次**（Windows 限制），之后通过 `taskbar.updateButton` 更新状态。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `buttons` | array | 是 | 默认值：`null`。 |

**按钮结构**：

| 属性 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `id` | string | 否 | 默认值：空字符串。作为后续 `taskbar.updateButton` 与点击事件的按钮标识。 |
| `tooltip` | string | 否 | 默认值：空字符串。缩略图按钮的提示文本。 |
| `icon` | string | 否 | 裸 Base64 编码的 `.ico` 文件字节，不带 `data:` 或 `base64:` 前缀；`null` 也按空图标处理。 |
| `enabled` | boolean | 否 | 默认值：`true`。 |
| `visible` | boolean | 否 | 默认值：`true`。 |
| `dismissOnClick` | boolean | 否 | 默认值：`false`。点击后是否关闭缩略图预览。 |

**返回值**：`{ "success": true }`

```javascript
await fb2k.invoke('taskbar.setThumbnailButtons', {
    buttons: [
        { id: 'prev',      tooltip: '上一首' },
        { id: 'playPause', tooltip: '播放' },
        { id: 'next',      tooltip: '下一首' },
    ]
});

fb2k.on('taskbar:buttonClicked', ({ id }) => {
    if (id === 'prev')      fb2k.invoke('playback.previous');
    if (id === 'playPause') fb2k.invoke('playback.playOrPause');
    if (id === 'next')      fb2k.invoke('playback.next');
});
```

---

### taskbar.updateButton

更新单个按钮的状态（不能新增或删除按钮）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `id` | string | 否 | 默认值：空字符串。 |
| `tooltip` | string | 否 | 可省略；handler 仅在提供该字段时读取。 |
| `icon` | string | 否 | 裸 Base64 编码的 `.ico` 文件字节，不带前缀；空、`null` 或省略时向更新逻辑传入空图标值。 |
| `enabled` | boolean | 否 | 可省略；handler 仅在提供该字段时读取。 |
| `visible` | boolean | 否 | 可省略；handler 仅在提供该字段时读取。 |

**返回值**：`{ "success": true }`

```javascript
// 播放状态变化时切换按钮提示
fb2k.on('playback:stateChanged', ({ state }) => {
    fb2k.invoke('taskbar.updateButton', {
        id: 'playPause',
        tooltip: state === 'playing' ? '暂停' : '播放',
    });
});
```

---

### taskbar.setProgress

设置任务栏图标上的进度条。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `state` | string | 否 | 默认值：`none`。 |
| `value` | number | 否 | 可省略；handler 仅在提供该字段时读取。 |

**返回值**：`{ "success": true }`

```javascript
// 缓存最近一次已知时长；playback:time 只携带 position。
let currentDuration = 0;

fb2k.on('playback:stateChanged', ({ state, duration }) => {
    currentDuration = duration || 0;
    if (state === 'paused') {
        fb2k.invoke('taskbar.setProgress', { state: 'paused' });
    } else if (state === 'stopped') {
        fb2k.invoke('taskbar.setProgress', { state: 'none' });
    }
    // playing 时由下面 playback:time 在 position 推进时设置 normal
});

fb2k.on('playback:time', ({ position }) => {
    if (currentDuration > 0) {
        fb2k.invoke('taskbar.setProgress', {
            state: 'normal',
            value: position / currentDuration,
        });
    }
});
```

---

### taskbar.setOverlayIcon

在任务栏图标右下角叠加一个小状态图标（如播放/暂停指示）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `icon` | string | 否 | 裸 Base64 编码的 `.ico` 文件字节，不带前缀；空、`null` 或省略时清除叠加图标。 |
| `description` | string | 否 | 可省略；handler 仅在提供该字段时读取。 |

**返回值**：`{ "success": true }`

```javascript
fb2k.on('playback:stateChanged', ({ state }) => {
    if (state === 'playing') {
        fb2k.invoke('taskbar.setOverlayIcon', { description: '正在播放' });
    } else {
        fb2k.invoke('taskbar.setOverlayIcon', {});  // 清除
    }
});
```

---

### taskbar.flash

闪烁任务栏按钮，吸引用户注意。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `count` | integer | 否 | 默认值：`3`。 |
| `interval` | integer | 否 | 默认值：`0`。 |

**返回值**：`{ "success": true }`

```javascript
// 新曲目开始时闪烁
fb2k.on('playback:trackChanged', () => {
    fb2k.invoke('taskbar.flash', { count: 2 });
});
```

---

### 事件（Taskbar）

| 事件 | 数据 | 描述 |
| --- | --- | --- |
| `taskbar:buttonClicked` | `{ id: string }` | 缩略图按钮被点击 |

---

## Tray API — 系统托盘（14 个 API）

### tray.create

在系统通知区域创建托盘图标。

> ⚠️ **必须在前端生命周期初始化阶段显式调用一次**（推荐放在 Vue/React 应用启动钩子或脚本入口）。其他所有 `tray.*` API（`setIcon` / `setTooltip` / `setContextMenu` / `showBalloon` / `appendMenuItems` 等）依赖 `tray.create` 建立的 Shell_NotifyIcon 注册；如果跳过，调用本身会成功返回但**托盘图标不显示、事件不触发、`tray.isVisible` 返回 false**。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `icon` | string | 否 | 裸 Base64 编码的 `.ico` 文件字节，不带 `data:` 或 `base64:` 前缀；空、无效或省略时回退到 foobar2000 主图标。 |
| `tooltip` | string | 否 | 默认值：`foobar2000`。 |

**返回值**：`{ "success": true }`

```javascript
await fb2k.invoke('tray.create', { tooltip: 'foobar2000 - 已停止' });
```

---

### tray.destroy

移除托盘图标。

**返回值**：`{ "success": true }`

---

### tray.setIcon

更新托盘图标图像。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `icon` | string | 否 | 裸 Base64 编码的 `.ico` 文件字节，不带前缀；空、无效或省略时回退到 foobar2000 主图标。 |

**返回值**：`{ "success": true }`

---

### tray.setTooltip

更新托盘图标的悬停提示文本。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `tooltip` | string | 否 | 默认值：空字符串。 |

**返回值**：`{ "success": true }`

```javascript
fb2k.on('playback:trackChanged', async (track) => {
    await fb2k.invoke('tray.setTooltip', {
        tooltip: `♪ ${track.artist ?? ''} - ${track.title ?? ''}`,
    });
});
```

---

### tray.showBalloon

显示气泡通知。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `title` | string | 否 | 默认值：空字符串。 |
| `message` | string | 否 | 默认值：空字符串。 |
| `icon` | string | 否 | 默认值：`info`。 |

**返回值**：`{ "success": true }`

---

### tray.setContextMenu

设置右键菜单。**普通用户项**点击后触发 `tray:menuItemClicked`；内置 `showPlaybackControls` / `showSystemItems` 注入项，以及声明了 `playbackAction` 的项，由插件原生执行且**不发**该事件。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `items` | array | 是 | 默认值：`null`。 |
| `config` | object | 否 | 可省略；handler 仅在提供该字段时读取。 |

**菜单项结构**：

| 属性 | 类型 | 默认值 | 描述 |
| --- | --- | --- | --- |
| `id` | string | 空字符串 | 菜单项标识。 |
| `label` | string | 空字符串 | 显示文本。 |
| `type` | string | `normal` | 菜单项类型；`checkbox` 也会建立 checkable 身份。 |
| `enabled` | boolean | `true` | 是否可用。 |
| `visible` | boolean | `true` | 是否可见。 |
| `checked` | boolean | 可省略 | **显式提供该属性（含 `checked: false`）**即建立 checkable 身份：WebView 映射为 `menuitemcheckbox`，`getMenuItems()` 会 round-trip 该键；省略则不是 checkbox。`tray.setMenuItemState(id, { checked })` 也会建立 checkable 身份。 |
| `icon` | string | 空字符串 | 保留兼容字段；native 与 WebView 两种后端当前都不渲染。WebView 菜单项图标请使用 `iconSvg`。 |
| `iconSvg` | object | 可省略 | `{ viewBox, content }` 内联单色 SVG 图标，**仅 `render: 'webview'` 渲染**（native 忽略）。运行时经 `DOMParser` 与元素/属性白名单克隆进 DOM；非法或单图大于 32 KiB 时丢弃图标但继续显示菜单。 |
| `submenu` | array | 可省略 | 递归菜单项数组。 |
| `cover` | string | 空字符串 | `nowplaying` 项的封面数据。 |
| `title` | string | 空字符串 | `nowplaying` 项标题。 |
| `subtitle` | string | 空字符串 | `nowplaying` 项副标题。 |
| `value` | integer | `0` | `rating`、`slider` 或 `segmented` 项的当前值。 |
| `min` | integer | `0` | `slider` 项最小值。 |
| `max` | integer | `100` | `slider` 项最大值。 |
| `orientation` | string | 可省略 | `slider` 方向；支持 `horizontal` 或 `vertical`。 |
| `playbackAction` | string | 可省略 | 声明式原生播放动作：`'play-pause' \| 'previous' \| 'next' \| 'stop'`。仅合法于 `type:'normal'` 叶子；组装时标记为内置播放路由并原生执行，**不发** `tray:menuItemClicked`。外观/`id`/`label`/`icon` 仍由调用方控制；主窗口最小化/托盘隐藏/锁屏深挂起时仍可靠。非法取值或写在 separator/submenu/富控件上 → 整次调用 fail-loud `INVALID_PARAMS`。不接受 `'exit'` / `'show-main-window'`（系统动作仍走精确 `_sys_exit` / `_sys_show`）。对 `menu.show` 无效。`getMenuItems()` round-trip。 |
| `segments` | array | 可省略 | `segmented` 项的分段数组。 |

> `rating` / `slider` / `segmented` 的值变化通过 `tray:menuItemClicked` 携带 `{ id, value }` 上报，且**不关闭菜单**（`segmented` 的 `value` = 选中段索引，点击段或键盘 Left/Right 切换；index→业务语义由前端映射，契约保持通用）；`nowplaying` 点击与普通项一样发 `{ id }` 并关闭。值控件不参与 `autoNowPlaying` 自动补全（仅 `nowplaying`）。
>
> **声明式原生播放（`playbackAction`）**：自定义外观托盘播放项在后台仍需可靠时，应声明本字段（或使用内置 `showPlaybackControls`），不要只靠 `tray:menuItemClicked` → `playback.*`——主页面深挂起时 JS 事件循环不保证运行。按钮态从 `playback:*` 事件反映。
>
> **Slider 方向（仅 webview）**：水平 min 左 max 右；纵向 min 底 max 顶（`clientY` / height，fill `height` 自 bottom，thumb `bottom`）。键盘 Up/Right 增、Down/Left 减、Home=min、End=max。pointermove 50ms 节流，pointerup 强发终值。常量滑块不发 value。DOM 钩子：`.fb-slider[data-orientation]`。
>
> **可访问性**：两态焦点（导航 roving tabindex / 真实 focus；富控件 Enter/Right 进入编辑态，Escape/Enter 返回）。ARIA：普通/子菜单 `menuitem`，checkable `menuitemcheckbox`，slider/rating 内部 `role=slider`，segmented `radiogroup`/`radio`。默认入退场在 `prefers-reduced-motion: reduce` 下禁用 transform/transition（自定义 CSS 由主题作者同样遵守）。

::: warning 顶层图标格式
`taskbar.*` 与 `tray.create` / `tray.setIcon` 的顶层 `icon` 不是通用图片输入，只接受裸 Base64 `.ico` 文件字节。PNG、JPEG、SVG、Data URL 与 `base64:` wire 字符串都不会按预期解析。解析失败时，任务栏按钮可能回退默认图标，overlay 可能等价于清除，tray 会回退 foobar2000 主图标。
:::

```javascript
await fb2k.invoke('tray.setContextMenu', {
    items: [
        { id: 'play',  label: '▶ 播放' },
        { id: 'next',  label: '⏭ 下一首' },
        { type: 'separator' },
        { id: 'exit',  label: '退出' },
    ]
});

fb2k.on('tray:menuItemClicked', ({ id }) => {
    if (id === 'play')  fb2k.invoke('playback.playOrPause');
    if (id === 'next')  fb2k.invoke('playback.next');
    if (id === 'exit')  fb2k.invoke('misc.exit');   // 真退出用 misc.exit；窗口控制在 window.* 命名空间
});
```

**返回值**：`{ "success": true }`

**自绘托盘菜单（`config.render: 'webview'`）**：将 `config.render` 设为 `'webview'`，右键托盘改用 WebView2 自绘菜单渲染同一份三区菜单（`showPlaybackControls` / `showSystemItems` / `customPosition` 行为不变）。**前端零改动**——普通用户项选中仍走 `tray:menuItemClicked`；内置注入项与声明了 `playbackAction` 的项由插件原生执行且不发该事件。`tray:beforeContextMenu` 语义不变（弹出前发射）；自绘托盘菜单**不**发射 `menu:select` / `menu:dismiss`（这两个属 `menu.*` 命名空间）。默认 `'native'` 与原生托盘菜单完全一致。自绘菜单使用**内容尺寸窗**——按菜单内容大小定位、**浮于任务栏之上且不压暗任务栏**，子菜单支持 1 层展开。

```javascript
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'about', label: '关于' }],
    config: { render: 'webview' },
});
```

**菜单项图标（`iconSvg`，仅 `render: 'webview'`）**：普通/子菜单项可带内联单色 SVG 图标，绘制在文字左侧、固定 8px 间距。前端从自己的 iconMap 取出 `{ viewBox, content }` 填入；图标用 `currentColor` 跟随菜单文字色（hover 时自动变白）。运行时执行 SVG allowlist sanitizer（允许 `path/circle/rect/line/polyline/polygon/ellipse/g` 及白名单属性；拒绝 `script`/`on*`/`href`/`url(...)` 等）；非法单图标降级为无图标。同一层只要有一项带可渲染图标，其余无图标项也会预留图标列，保证文字左缘对齐。

```javascript
await fb2k.invoke('tray.setContextMenu', {
    items: [
        { id: 'play', label: '播放', iconSvg: { viewBox: '0 0 24 24', content: '<path d="M8 5v14l11-7z"/>' } },
        { id: 'next', label: '下一首', iconSvg: { viewBox: '0 0 24 24', content: '<path d="M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z"/>' } },
        { type: 'separator' },
        { id: 'about', label: '关于' },  // 无图标：仍占图标列，文字与上面对齐
    ],
    config: { render: 'webview' },
});
```

#### TrayMenuConfig

`tray.setContextMenu` 的可选 `config` 入参：

```typescript
interface TrayMenuConfig {
    showPlaybackControls?: boolean;   // 默认 true，自动注入 4 项播放控制（上一首 / 播放暂停 / 下一首 / 停止）到 playback 分区
    showSystemItems?: boolean;        // 默认 true，自动注入「显示主窗口」+「Exit foobar2000」到 bottom 分区（均原生执行）
    customPosition?: 'top' | 'playback' | 'bottom';  // 默认 'top'，setContextMenu 的 items 写入哪个分区
    render?: 'native' | 'webview';    // 默认 'native'（Win32 菜单）；'webview' 改用 WebView2 自绘菜单
    autoNowPlaying?: boolean;         // 默认 false；nowplaying 项的空字段(cover/title/subtitle)右键时按「前端优先、后端补全」自动填当前曲目（cover 仅 webview）
    css?: string;                     // 仅 render:'webview'；前端样式字符串，注入自绘菜单专用 <style> 层（每次右键覆盖）
    cssReplace?: boolean;             // 默认 false=override 叠加在内置样式之上；true=replace（禁用内置样式，仅留前端 css + 受保护结构层）
    backdrop?: 'acrylic' | 'mica' | 'mica-alt' | 'none';  // 仅 render:'webview'；DWM 系统背景材质，默认 'acrylic'（瞬态窗口的材质，与菜单语义相符）；每次右键应用
    backdropDarkMode?: boolean;       // 仅 render:'webview'；背景暗色调，默认 true，前端可据主题传 false 跟随浅色
    closeAnimationMs?: number;        // 仅 render:'webview'；默认 0=立即关闭；>0=关闭前播退场动画的毫秒数（应≈你的 #menu.out transition 时长）。退场态 class=#menu.out，可经 css 自定义；replaced/超时仍立即关闭
    layoutMode?: 'flat' | 'zones';    // 仅 render:'webview'；默认 'flat' 保留 #menu > .fb-item；'zones' 为 opt-in zone 容器。native 忽略；旧 runtime 忽略未知字段但不生成 wrapper；menu.show 不受影响
}
```

```javascript
// 关闭内置项，items 直接写到 bottom 分区
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'about', label: '关于' }],
    config: {
        showPlaybackControls: false,
        showSystemItems: false,
        customPosition: 'bottom',
    },
});
```

**nowplaying 自动补全（`autoNowPlaying`）**：开启后，`type: 'nowplaying'` 项中**前端没传的字段**会在右键弹出时由后端用当前曲目自动补全（前端传了就用前端的，**前端优先**）。`cover` 自动补全仅 `render: 'webview'`，取当前曲目内嵌/本地封面并缩略为缩略图；对 foobar2000 取不到封面的来源（如多数流媒体）请前端自行传 `cover` —— 支持 `http(s)://` / `data:` / 裸 base64 三种形态。`title` 走 `%title%`（自动回退文件名），`subtitle` 走 `%artist%`，兼容流媒体动态标题。

```javascript
// 纯本地：只声明空 nowplaying，cover/title/subtitle 全自动
await fb2k.invoke('tray.setContextMenu', {
    items: [{ type: 'nowplaying', id: 'np' }],
    config: { render: 'webview', autoNowPlaying: true },
});

// 流媒体 / 混合：前端传 http 封面与文本（优先于后端补全）
await fb2k.invoke('tray.setContextMenu', {
    items: [{ type: 'nowplaying', id: 'np', cover: 'https://example.com/art.jpg', title: '歌名', subtitle: '歌手' }],
    config: { render: 'webview', autoNowPlaying: true },
});
```

**前端样式接管（`css` / `cssReplace`，仅 `render: 'webview'`）**：通过 `config.css` 把一段 CSS 字符串注入自绘菜单专用的 `<style>` 层，每次右键弹出时应用，**完全由前端决定菜单视觉**（颜色 / 字体 / 留白 / 圆角 / 阴影 / 深浅色 / 动效等）。

- 默认 **override 叠加** 模式：你的规则叠加在内置样式之上，按菜单的**稳定 class 名**编写并靠源序或 `!important` 取胜。可用的稳定 class（自绘菜单 overlay 是独立顶层 document，宿主页的 `::part()` 无法跨 document 触达，故钩子 = class 名）：容器 `.fb-menu`；菜单项 `.fb-item`（+ `.nrm` / `.disabled` / `.active` / `.checked` / `.has-sub`）、图标列 `.fb-item-ico`、子菜单箭头 `.fb-arrow`、分隔线 `.fb-sep`；nowplaying `.fb-np` / `.fb-np-cover` / `.fb-np-text` / `.fb-np-title` / `.fb-np-sub`；rating `.fb-rating` / `.fb-stars` / `.fb-star`（+ `.on`）；slider `.fb-slider` / `.fb-slider-track` / `.fb-slider-fill` / `.fb-slider-thumb` / `.fb-slider-val`。
- `cssReplace: true` 切 **replace** 模式：禁用全部内置默认样式，整张菜单（含入场动画）以你的 `css` 为准，仅保留一层**受保护结构层**（`#viewport` 几何、菜单盒模型 / 固定定位 / 溢出、隐藏态 fallback）以保证内容尺寸窗测量稳定。**可见态 display（block / flex / grid）不再由受保护层强制**，主题可直接布局根菜单；用户 CSS 无法用 `display:* !important` 重新显示已隐藏的菜单。
- `native` 后端忽略 `css` / `cssReplace`。
- **`layoutMode`**：默认 `'flat'`，零配置时 DOM 仍为 `#menu > .fb-item` / separator 直接子结构（旧主题选择器继续成立）。仅显式 `'zones'` 时生成 `.fb-zone[data-zone]`。稳定钩子：`.fb-menu[data-depth]`、`.fb-zone[data-zone]`、`.fb-item[data-item-id][data-kind][data-depth][data-zone]`；`data-item-token` 为内部单次 show 身份，**不是**公共 CSS 契约。zones 自 1.10.0 起提供；需兼容旧版的主题应先用 `config.getVersionInfo().plugin.version` 探测运行时版本。`menu.show` 始终保持 legacy 直接子 DOM，不继承 tray zones。

```javascript
// override：仅微调配色，复用内置布局
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'about', label: '关于' }],
    config: {
        render: 'webview',
        css: '.fb-menu{background:#fff;color:#222;border-color:#ddd;} .fb-item.active{background:#0a84ff;color:#fff;}',
    },
});

// replace：完全自定义（内置样式全停用，仅留受保护结构层）
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'about', label: '关于' }],
    config: {
        render: 'webview',
        cssReplace: true,
        css: '.fb-menu{background:#1e1e2e;border-radius:12px;padding:6px;font-family:"Segoe UI";} .fb-item{padding:8px 16px;border-radius:8px;} .fb-item.active{background:#89b4fa;color:#11111b;}',
    },
});

// layoutMode:'flat'（默认）：根可直接 flex/grid；旧 #menu > .fb-item 继续成立
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'a', label: 'A' }, { type: 'separator' }, { id: 'b', label: 'B' }],
    config: {
        render: 'webview',
        css: '#menu{display:flex;flex-direction:column;gap:2px;} .fb-item[data-zone="playback"]{opacity:.95;}',
    },
});

// layoutMode:'zones'（opt-in）：zones 自 1.10.0 起提供；需兼容旧版时先探测运行时版本
const ver = await fb2k.invoke('config.getVersionInfo', {});
const _pluginVersion = ver && ver.plugin && ver.plugin.version;
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'vol', label: 'Volume', type: 'slider', value: 50, min: 0, max: 100 }],
    config: {
        render: 'webview',
        layoutMode: 'zones',
        css: '.fb-zone[data-zone="playback"]{display:flex;flex-direction:column;gap:4px;} .fb-item[data-item-id="vol"]{padding-inline:12px;}',
    },
});
```

---

**分段单选富项（`type: 'segmented'`，仅 `render: 'webview'`）**：一行互斥选项（分段控件 / 单选组），各段可放内联 SVG 图标或文字。选中段索引 = `value`；点击启用段（或键盘 Left/Right 切换）经 `tray:menuItemClicked` 上报 `{ id, value: 索引 }` 并**保持菜单打开**、即时高亮。某段设 `enabled: false` 置灰不可选。`segmented` 是**通用原语**——index→具体业务（如播放模式）的映射由前端完成，契约不含项目专用字段；`native` 后端降级为纯文字项。

```javascript
// 播放模式分段（顺序 / 随机 / 单曲 / 列表循环），index→业务由前端映射
await fb2k.invoke('tray.setContextMenu', {
    items: [{
        type: 'segmented', id: 'playmode', label: '播放模式', value: 0,
        segments: [
            { iconSvg: { viewBox: '0 0 24 24', content: '<path d="..."/>' } }, // 顺序
            { iconSvg: { viewBox: '0 0 24 24', content: '<path d="..."/>' } }, // 随机
            { iconSvg: { viewBox: '0 0 24 24', content: '<path d="..."/>' } }, // 单曲
            { iconSvg: { viewBox: '0 0 24 24', content: '<path d="..."/>' } }, // 列表循环
        ],
    }],
    config: { render: 'webview' },
});

fb2k.on('tray:menuItemClicked', ({ id, value }) => {
    if (id === 'playmode' && value != null) {
        // value 是段索引；前端映射到实际播放模式（契约保持通用）
    }
});
```

**DWM 背景效果（`backdrop` / `backdropDarkMode`，仅 `render: 'webview'`）**：自绘托盘菜单的系统级背景材质，取值与主窗口一致：`'acrylic'`（默认；亚克力是瞬态窗口的材质，与菜单语义相符）/ `'mica'` / `'mica-alt'` / `'none'`，每次右键弹出时应用、可随主题切换。`backdropDarkMode`（默认 `true`）控制背景暗色调，前端可据主题传 `false` 跟随浅色。ContentSized 会真实测量 root 与一级子菜单；root 和展开的一级子菜单分别使用紧凑的独立 HWND，并各自应用调用方配置的 backdrop。未展开时不存在原生 submenu 预留窗口，因此不会在 root 外绘制空白 DWM 材质；runtime 也不会因存在子菜单而把调用方 backdrop 静默改成 `none`。注意：自绘菜单 `.fb-menu` 默认背景**不透明会遮住背景效果**——需前端经 `css` 把 `.fb-menu` 背景改半透明才能透出亚克力/云母。`native` 后端忽略。

```javascript
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'about', label: '关于' }],
    config: {
        render: 'webview',
        backdrop: 'acrylic',          // 'acrylic' | 'mica' | 'mica-alt' | 'none'
        backdropDarkMode: true,
        css: '.fb-menu{background:rgba(40,40,40,.6);}',  // 半透明才透出背景效果
    },
});
```

> ⚠️ **DWM 背景（亚克力/云母）与淡入淡出动画冲突**：DWM 背景是**窗口级二元效果**，随 Win32 窗口瞬间显示/隐藏，**不随 CSS 动画淡入淡出**（`closeAnimationMs` 只动画 web 内容）。两者同用时背景会"啪"地出现/消失而内容在做淡入淡出——两者不同步。想要全程平滑的开关动画，请改用 **CSS 半透明背景**（`backdrop: 'none'` + `.fb-menu{background:rgba(...)}`）代替 DWM 背景。**真正的桌面模糊** 与 **可动画的过渡** 二选一（Windows 合成架构所限，非实现 bug；overlay 为 WebView2 Visual Hosting/DirectComposition 透明窗，无法用 `WS_EX_LAYERED` 淡整窗 alpha）。

---

### tray.setMinimizeToTray

窗口最小化时隐藏到托盘而非任务栏。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `enabled` | boolean | 否 | 默认值：`false`。 |

**返回值**：`{ "success": true }`

---

### tray.setCloseToTray

关闭窗口时隐藏到托盘而非退出。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `enabled` | boolean | 否 | 默认值：`false`。 |

**返回值**：`{ "success": true }`

---

### tray.isVisible

查询托盘图标是否存在。

**返回值**：`{ "success": true, "visible": boolean }`

---

### tray.appendMenuItems

增量追加菜单项到指定分区。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `items` | array | 是 | 默认值：`null`。 |
| `position` | string | 否 | 目标分区；可省略，省略时使用 `top`。 |

**返回值**：`{ "success": true }`

---

### tray.removeMenuItems

按 id 跨分区移除菜单项。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `ids` | array | 是 | 默认值：`null`。 |

**返回值**：`{ "success": true, "removed": number }` — `removed` 为实际删除数量

---

### tray.clearMenuItems

清空指定分区菜单。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `position` | string | 否 | 目标分区；省略时清空全部三个分区。 |

**返回值**：`{ "success": true }`

---

### tray.getMenuItems

查询当前用户自定义菜单项（扁平化，顺序：`top → playback → bottom`）。

> 仅返回用户通过 `setContextMenu` / `appendMenuItems` 添加的项；由 `showPlaybackControls` / `showSystemItems` 自动注入的内置项不在返回值内。内置项使用 `_pb_` 与 `_sys_` 前缀的保留命名空间，用户自定义菜单项应避免使用这些前缀。

**返回值**：`{ "success": true, "items": TrayMenuItem[] }`

---

### tray.setMenuItemState

原地更新单个菜单项的 `checked` / `enabled` 状态，作为 `setContextMenu`（整分区全量替换）之外的细粒度选项。id 跨三个分区递归查找（含 `submenu`），`checked` / `enabled` 至少提供一个。

> 原生托盘菜单每次打开时从存储数据重建，因此新状态在**下次打开**时生效（无法刷新已经打开的菜单，Win32 限制）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| `id` | string | 否 | 默认值：空字符串。 |
| `checked` | boolean | 否 | 可省略；handler 仅在提供该字段时读取。 |
| `enabled` | boolean | 否 | 可省略；handler 仅在提供该字段时读取。 |

**返回值**：`{ "success": true, "found": true }`（`found` 表示是否找到该 id 的菜单项）

---

### 事件（Tray）

| 事件 | 数据 | 描述 |
| --- | --- | --- |
| `tray:click` | `{ button: number, x: number, y: number }` | 托盘图标被单击（button: 0=左键） |
| `tray:doubleClick` | `{ x: number, y: number }` | 托盘图标被双击 |
| `tray:menuItemClicked` | `{ id: string, value?: number }` | 普通用户项 / 富值控件被操作。内置注入项与声明了 `playbackAction` 的项**不发**此事件 |
| `tray:beforeContextMenu` | `{ x: number, y: number }` | 菜单即将显示的异步通知；handler 内的 append/remove/clear 只影响后续右键菜单，不阻塞本次弹出 |

---

## 完整示例

```javascript
// 初始化托盘
await fb2k.invoke('tray.create', { tooltip: 'foobar2000' });

await fb2k.invoke('tray.setContextMenu', {
    items: [
        { id: 'playPause', label: '▶ 播放' },
        { id: 'next',      label: '⏭ 下一首' },
        { type: 'separator' },
        { id: 'exit',      label: '退出' },
    ]
});

await fb2k.invoke('tray.setMinimizeToTray', { enabled: true });
await fb2k.invoke('tray.setCloseToTray',    { enabled: true });

// 初始化任务栏缩略图按钮
await fb2k.invoke('taskbar.setThumbnailButtons', {
    buttons: [
        { id: 'prev',      tooltip: '上一首' },
        { id: 'playPause', tooltip: '播放' },
        { id: 'next',      tooltip: '下一首' },
    ]
});

// 监听事件
fb2k.on('tray:click',          () => fb2k.invoke('window.focus'));
fb2k.on('tray:menuItemClicked', ({ id }) => {
    if (id === 'playPause') fb2k.invoke('playback.playOrPause');
    if (id === 'next')      fb2k.invoke('playback.next');
    if (id === 'exit')      fb2k.invoke('misc.exit');
});
fb2k.on('taskbar:buttonClicked', ({ id }) => {
    if (id === 'prev')      fb2k.invoke('playback.previous');
    if (id === 'playPause') fb2k.invoke('playback.playOrPause');
    if (id === 'next')      fb2k.invoke('playback.next');
});

let currentDuration = 0;

fb2k.on('playback:stateChanged', ({ state, duration }) => {
    currentDuration = duration || 0;
    const isPlaying = state === 'playing';
    fb2k.invoke('taskbar.updateButton', {
        id: 'playPause',
        tooltip: isPlaying ? '暂停' : '播放',
    });
    fb2k.invoke('tray.setTooltip', {
        tooltip: isPlaying ? '正在播放' : 'foobar2000',
    });
    fb2k.invoke('taskbar.setProgress', {
        state: isPlaying ? 'normal' : state === 'paused' ? 'paused' : 'none',
    });
});

fb2k.on('playback:time', ({ position }) => {
    if (currentDuration > 0) {
        fb2k.invoke('taskbar.setProgress', {
            state: 'normal',
            value: position / currentDuration,
        });
    }
});
```

---

## 动态菜单示例

`tray:beforeContextMenu` 异步事件 + 4 个增量菜单 API（`appendMenuItems` / `removeMenuItems` / `clearMenuItems` / `getMenuItems`）适合「按当前播放状态动态展示菜单项」场景。基础初始化用法参见上方【完整示例】。

```javascript
// 0. 前提：必须先创建托盘图标（仅一次，整个生命周期），否则后续菜单 API 不会有可见效果
await fb2k.invoke('tray.create', { tooltip: 'foobar2000' });

// 1. 初始：注入用户自定义项到 top 分区，同时保留内置播放控制项与 Exit
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'addFav', label: '☆ 添加到收藏夹' }],
    config: {
        showPlaybackControls: true,    // 自动注入 playback 分区（上一首 / 播放暂停 / 下一首 / 停止）
        showSystemItems: true,         // 自动注入 bottom 分区「显示主窗口」+ Exit foobar2000
        customPosition: 'top',         // items 写入 top 分区
    },
});

// 2. 缓存当前曲目（从 playback:trackChanged 同步拿最新 track）
let currentTrack = null;
fb2k.on('playback:trackChanged', (track) => { currentTrack = track; });

// 3. 菜单展示前异步通知：清空 top 分区 → 重新注入静态项 → 按曲目追加上下文项
fb2k.on('tray:beforeContextMenu', async () => {
    await fb2k.invoke('tray.clearMenuItems', { position: 'top' });
    await fb2k.invoke('tray.appendMenuItems', {
        items: [{ id: 'addFav', label: '☆ 添加到收藏夹' }],
        position: 'top',
    });

    if (currentTrack?.path) {
        await fb2k.invoke('tray.appendMenuItems', {
            items: [
                { type: 'separator' },
                { id: 'revealInExplorer', label: '在文件管理器中显示' },
                { id: 'copyPath',         label: '复制文件路径' },
            ],
            position: 'top',
        });
    }
});

// 4. 处理菜单项点击（含动态追加的上下文项）
fb2k.on('tray:menuItemClicked', ({ id }) => {
    if (id === 'addFav') {
        // 收藏当前曲目（应用自定义逻辑）
    }
    if (id === 'revealInExplorer' && currentTrack?.path) {
        fb2k.invoke('shell.showInExplorer', { path: currentTrack.path });
    }
    if (id === 'copyPath' && currentTrack?.path) {
        fb2k.invoke('clipboard.write', { text: currentTrack.path });
    }
});

// 5. 查询当前用户菜单项（不包含内置项）
const { items } = await fb2k.invoke('tray.getMenuItems');
console.log('当前用户菜单项：', items);

// 6. 按 id 批量移除多个项（跨分区生效）
await fb2k.invoke('tray.removeMenuItems', { ids: ['revealInExplorer', 'copyPath'] });
```

> **语义提示**：`tray:beforeContextMenu` 是异步通知，handler 内的 `clearMenuItems` / `appendMenuItems` 只影响下一次菜单弹出，不阻塞本次弹出。若希望首次右键菜单也包含上下文项，应在 `playback:trackChanged` 中同步预填充。

## 契约补充

以下补充这些方法的完整参数契约，不改变前文的已有说明。

<!-- phase3-supplement:taskbar.setProgress -->
### Contract 补充：`taskbar.setProgress`

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `state` | `string` | 否 | `none` | 取 `none`、`indeterminate`、`normal`、`error`、`paused` 之一；其他任何值同样按 `none` 处理。 |
| `value` | `number` | 否 | — | 仅当取值为 0 到 1（含端点）的数字时生效，否则保持原有进度。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `success` | `boolean` | 否 |
| `panelMode` | `boolean` | 否 |

`success` 反映任务栏是否接受了本次变更，因此即使参数合法，COM 集成尚未初始化时也可能为 `false`。panel 模式下返回 `{ success: false, panelMode: true }`。

```js
// indeterminate 下 value 不生效
await fb2k.invoke('taskbar.setProgress', { state: 'indeterminate' });
```

<!-- contract-supplement:tray.playbackAction -->
### Contract 补充：`items[].playbackAction`

`playbackAction` 声明由插件原生执行的播放动作（而非页面 JS）。取值之一：`'play-pause' | 'previous' | 'next' | 'stop'`；仅合法于 `type: 'normal'` 叶子。

| 方面 | 行为 |
| --- | --- |
| 执行 | 组装阶段翻译为匹配的内置命令，由插件原生执行。 |
| 后台可靠 | 窗口最小化、关闭到托盘或会话锁屏（主页面深挂起）时仍可用。 |
| 事件 | 声明项**不发** `tray:menuItemClicked`；按钮态从 `playback:*` 反映。 |
| 外观 | 调用方完整控制 `label` / `icon` / `id`；仅路由改变。 |
| 校验 | fail-loud：未知 token，或写在 separator / submenu / 富控件上，整次 `setContextMenu` / `appendMenuItems` 返回 `INVALID_PARAMS`。 |
| `'exit'` / `'show-main-window'` | 不接受——系统动作仍走精确 `_sys_exit` / `_sys_show`。 |
| 作用域 | 仅 tray 菜单；对 `menu.show` 无效。 |
| Round-trip | `getMenuItems()` 回传已声明的 `playbackAction`。 |

未声明本字段、仅靠 `tray:menuItemClicked` 再 `invoke('playback.*')` 的用户项，在主页面深挂起时不保证执行。后台可靠的托盘播放控制请用 `playbackAction`（或内置 `showPlaybackControls` 项）。这与 Electron `MenuItem.role` / Tauri `PredefinedMenuItem` 的声明式原生动作模式同构。

```js
// 自定义外观 + 后台可靠的原生播放：
await fb2k.invoke('tray.setContextMenu', {
    items: [
        { id: 'prev', label: '⏮', playbackAction: 'previous' },
        { id: 'pp', label: '⏯', playbackAction: 'play-pause' },
        { id: 'next', label: '⏭', playbackAction: 'next' },
    ],
    config: { showPlaybackControls: false, render: 'webview' },
});
```

## 运行时生命周期、菜单数据与事件

`taskbar.*` 与 `tray.*` 需要 standalone 主窗口。在 panel 中，每个 handler 返回
`{ success: false, panelMode: true }`。先调用 `tray.create`，再使用托盘可见性、回调与
菜单操作。Windows 最多接受七个缩略图按钮；任务栏 COM 尚未初始化时，设置缩略图按钮也会失败。

`taskbar.setProgress` 支持 `none`、`indeterminate`、`normal`、`error` 和 `paused`。
只有 0–1 之间的 `value` 才生效。缩略图按钮激活时广播
`taskbar:buttonClicked`，payload 为 `{ id }`。

托盘菜单由 `tray.setContextMenu` 配置，后续可使用 `tray.appendMenuItems`、
`tray.removeMenuItems`、`tray.clearMenuItems` 与 `tray.setMenuItemState` 更新。
`tray:menuItemClicked` 通常携带 `{ id }`；rating、slider 与 segmented 控件还会提供
`value`。由插件原生执行的项**不会**触发该事件：内置 `showPlaybackControls` /
`showSystemItems` 注入项，以及声明了 `playbackAction` 的项，直接执行其命令。
图标事件为带 `{ button, x, y }` 的 `tray:click`、带 `{ x, y }` 的
`tray:doubleClick`，以及带 `{ x, y }` 的 `tray:beforeContextMenu`。最后一个事件是
异步通知：handler 的变更影响下一次打开的菜单，不会阻塞当前正在构建的菜单。

菜单可带 `data:image/...` 封面数据，并可选用 webview 渲染。对于 webview renderer，
配置 stylesheet 可包含 `display:flex`、`flex-direction:column` 与
`background:rgba(...)` 等声明。普通用户项的点击事件是 `tray:menuItemClicked`；它与
`menu:select` / `menu:dismiss` 无关，不会取代二者。由插件原生执行的项——内置注入项以及声明了
`playbackAction` 的项——不发 `tray:menuItemClicked`。

### 保留系统项

`showSystemItems`（默认 `true`）向 bottom 分区注入两个**原生执行**的内置项，顺序固定：

| id | 标签 | 动作 |
| --- | --- | --- |
| `_sys_show` | 显示主窗口 | 恢复并前置主窗口，保留其最大化 / 正常放置状态 |
| `_sys_exit` | Exit foobar2000 | 真正退出应用，绕过 `setCloseToTray` |

两项都原生执行，因此**不发** `tray:menuItemClicked`。这对 `_sys_show` 是必要设计：
隐藏到托盘会对主页面施加 `put_IsVisible(FALSE)` + 深度挂起（`TrySuspend`），renderer
被冻结，`tray:menuItemClicked` 的 handler 根本跑不起来，也就无法自行调用
`window.focus`。偏偏在这个项唯一有用的状态下，前端事件路线是死的。

若想自绘该行而不用注入项，直接使用精确、大小写敏感的 id `_sys_show`（或 `_sys_exit`）：
它获得同样的原生路由，你的 `label` / `icon` 被保留，对应的注入项则自动跳过。形似 id
（如 `_sys_show_alt`、`_SYS_SHOW`）仍是普通用户项，且**不会**抑制注入。

```js
fb2k.on('taskbar:buttonClicked', ({ id }) => console.log(id));
fb2k.on('tray:click', ({ button, x, y }) => console.log(button, x, y));
fb2k.on('tray:doubleClick', ({ x, y }) => console.log(x, y));
fb2k.on('tray:beforeContextMenu', ({ x, y }) => console.log(x, y));
fb2k.on('tray:menuItemClicked', ({ id, value }) => console.log(id, value));
fb2k.on('playback:stateChanged', () => {});
fb2k.on('playback:time', () => {});
fb2k.on('playback:trackChanged', () => {});
```
