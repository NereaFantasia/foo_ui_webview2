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
| buttons | array | ✓ | 按钮定义数组（最多 7 个） |

**按钮结构**：

| 字段 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| id | string | ✓ | 按钮唯一标识，用于事件回调 |
| tooltip | string | ✓ | 悬停提示文本 |
| icon | string | ✗ | base64 编码的 ICO 图像；省略则使用 foobar2000 主图标 |
| enabled | boolean | ✗ | 是否可点击（默认 `true`） |
| visible | boolean | ✗ | 是否显示（默认 `true`） |
| dismissOnClick | boolean | ✗ | 点击后关闭缩略图预览（默认 `false`） |

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
| id | string | ✓ | 要更新的按钮 ID |
| tooltip | string | ✗ | 新的提示文本 |
| icon | string | ✗ | 新的 base64 ICO 图像 |
| enabled | boolean | ✗ | 启用/禁用 |
| visible | boolean | ✗ | 显示/隐藏 |

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
| state | string | ✓ | `none` / `indeterminate` / `normal` / `error` / `paused` |
| value | number | ✗ | 进度值 0–1（`normal`/`error`/`paused` 时有效） |

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
| icon | string | ✗ | base64 编码的 ICO；传 `null` 或省略则清除叠加图标 |
| description | string | ✗ | 无障碍描述文本 |

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
| count | number | ✗ | 闪烁次数（默认 3） |
| interval | number | ✗ | 闪烁间隔 ms（默认 0，使用系统默认） |

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
| icon | string | ✗ | base64 ICO；省略则使用 foobar2000 主图标 |
| tooltip | string | ✗ | 鼠标悬停提示（默认 `"foobar2000"`） |

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
| icon | string | ✗ | base64 ICO；省略 / 传 `null` 则回退到 foobar2000 主图标 |

---

### tray.setTooltip

更新托盘图标的悬停提示文本。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| tooltip | string | ✓ | 提示文本（最长 128 个字符） |

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
| title | string | ✓ | 通知标题 |
| message | string | ✓ | 通知内容 |
| icon | string | ✗ | `info`（默认）/ `warning` / `error` |

---

### tray.setContextMenu

设置右键菜单。菜单项点击后触发 `tray:menuItemClicked` 事件。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| items | array | ✓ | 菜单项数组 |
| config | object | ✗ | 菜单配置（见下方 TrayMenuConfig） |

**菜单项结构**：

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| id | string | 标识符（`separator` 可省略） |
| label | string | 显示文本 |
| type | string | `normal`（默认）/ `separator` / `submenu`；富类型 `nowplaying` / `rating` / `slider` / `segmented`（仅 `render: 'webview'` 完整渲染，`native` 降级，见下方「富菜单项字段」） |
| enabled | boolean | 是否可用（默认 `true`） |
| visible | boolean | 是否显示（默认 `true`） |
| checked | boolean | 勾选状态（默认 `false`；勾选项设 `checked: true` 即可，无需特殊 `type`） |
| icon | string | base64 ICO 图标（当前预留，native 与 webview 均暂不渲染） |
| iconSvg | `{ viewBox, content }` | 内联单色 SVG 图标，**仅 `render: 'webview'` 渲染**（native 忽略）。`content` 为 SVG 内部标记（如 `<path .../>`），以 `<svg viewBox=...>content</svg>` + `fill: currentColor` 绘制，跟随菜单文字色；同层只要有一项带图标，所有普通/子菜单项就预留 16px 图标列以左对齐文字。`content` 原样注入，请仅用受信的形状标记；来源不可信时前端需自行 sanitize（去除 `<script>`/`on*`） |
| submenu | array | 子菜单项（需配合 `type: 'submenu'`） |

**富菜单项字段（`nowplaying` / `rating` / `slider` / `segmented`，仅 `render: 'webview'` 完整渲染）**

富类型由自绘菜单（`render: 'webview'`）完整渲染；`native`（Win32 `TrackPopupMenu`）会降级：`nowplaying` → 置灰标题行、`rating` → 「★1–5」子菜单、`slider` → 分级子菜单、`segmented` → 纯文字项。

| 字段 | 适用类型 | 类型 | 描述 |
| --- | --- | --- | --- |
| cover | nowplaying | string | 专辑封面，渲染为 40x40 缩略图。支持 `data:image/...;base64,...` / `http(s)://...` / 裸 base64 三种形态。省略且 `config.autoNowPlaying` 开启时，`webview` 后端自动取当前曲目封面 |
| title | nowplaying | string | 主标题（曲名）。省略时回退 `label`；`autoNowPlaying` 时自动取 `%title%`（自动回退文件名） |
| subtitle | nowplaying | string | 副标题（艺术家/专辑）。`autoNowPlaying` 时自动取 `%artist%` |
| value | rating / slider / segmented | number | `rating`：星级 `0..5`；`slider`：`[min, max]` 内整数；`segmented`：选中段的零基索引 |
| min | slider | number | 滑块最小值（默认 `0`） |
| max | slider | number | 滑块最大值（默认 `100`） |
| segments | segmented | array | 分段单选的各段 `{ label?, iconSvg?, enabled? }`（一行互斥选项）。选中段 = `value`（索引）；某段 `enabled: false` 置灰不可选 |

> `rating` / `slider` / `segmented` 的值变化通过 `tray:menuItemClicked` 携带 `{ id, value }` 上报，且**不关闭菜单**（`segmented` 的 `value` = 选中段索引，点击段或键盘 Left/Right 切换；index→业务语义由前端映射，契约保持通用）；`nowplaying` 点击与普通项一样发 `{ id }` 并关闭。值控件不参与 `autoNowPlaying` 兜底（仅 `nowplaying`）。

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

**自绘托盘菜单（`config.render: 'webview'`）**：将 `config.render` 设为 `'webview'`，右键托盘改用 WebView2 自绘菜单渲染同一份三区菜单（`showPlaybackControls` / `showSystemItems` / `customPosition` 行为不变）。**前端零改动**——选中仍走 `tray:menuItemClicked`，`tray:beforeContextMenu` 语义不变（弹出前发射）；自绘托盘菜单**不**发射 `menu:select` / `menu:dismiss`（这两个属 `menu.*` 命名空间）。默认 `'native'` 与原生托盘菜单完全一致。自绘菜单使用**内容尺寸窗**——按菜单内容大小定位、**浮于任务栏之上且不压暗任务栏**，子菜单支持 1 层展开。

```javascript
await fb2k.invoke('tray.setContextMenu', {
    items: [{ id: 'about', label: '关于' }],
    config: { render: 'webview' },
});
```

**菜单项图标（`iconSvg`，仅 `render: 'webview'`）**：普通/子菜单项可带内联单色 SVG 图标，绘制在文字左侧、固定 8px 间距。前端从自己的 iconMap 取出 `{ viewBox, content }` 填入；图标用 `currentColor` 跟随菜单文字色（hover 时自动变白）。同一层只要有一项带图标，其余无图标项也会预留图标列，保证文字左缘对齐。

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
    showSystemItems?: boolean;        // 默认 true，自动注入「Exit foobar2000」到 bottom 分区
    customPosition?: 'top' | 'playback' | 'bottom';  // 默认 'top'，setContextMenu 的 items 写入哪个分区
    render?: 'native' | 'webview';    // 默认 'native'（Win32 菜单）；'webview' 改用 WebView2 自绘菜单
    autoNowPlaying?: boolean;         // 默认 false；nowplaying 项的空字段(cover/title/subtitle)右键时按「前端优先、后端兜底」自动填当前曲目（cover 仅 webview）
    css?: string;                     // 仅 render:'webview'；前端样式字符串，注入自绘菜单专用 <style> 层（每次右键覆盖）
    cssReplace?: boolean;             // 默认 false=override 叠加在内置样式之上；true=replace（禁用内置样式，仅留前端 css + 受保护结构层）
    backdrop?: 'acrylic' | 'mica' | 'mica-alt' | 'none';  // 仅 render:'webview'；DWM 系统背景材质，默认 'acrylic'（瞬态菜单语义正确）；每次右键应用
    backdropDarkMode?: boolean;       // 仅 render:'webview'；背景暗色调，默认 true，前端可据主题传 false 跟随浅色
    closeAnimationMs?: number;        // 仅 render:'webview'；默认 0=立即关闭；>0=关闭前播退场动画的毫秒数（应≈你的 #menu.out transition 时长）。退场态 class=#menu.out，可经 css 自定义；replaced/超时仍立即关闭
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

**nowplaying 智能兜底（`autoNowPlaying`）**：开启后，`type: 'nowplaying'` 项中**前端没传的字段**会在右键弹出时由后端用当前曲目自动补全（前端传了就用前端的，**前端优先**）。`cover` 自动补全仅 `render: 'webview'`，取当前曲目内嵌/本地封面并缩略为缩略图；对 foobar2000 取不到封面的来源（如多数流媒体）请前端自行传 `cover` —— 支持 `http(s)://` / `data:` / 裸 base64 三种形态。`title` 走 `%title%`（自动回退文件名），`subtitle` 走 `%artist%`，兼容流媒体动态标题。

```javascript
// 纯本地：只声明空 nowplaying，cover/title/subtitle 全自动
await fb2k.invoke('tray.setContextMenu', {
    items: [{ type: 'nowplaying', id: 'np' }],
    config: { render: 'webview', autoNowPlaying: true },
});

// 流媒体 / 混合：前端传 http 封面与文本（优先于后端兜底）
await fb2k.invoke('tray.setContextMenu', {
    items: [{ type: 'nowplaying', id: 'np', cover: 'https://example.com/art.jpg', title: '歌名', subtitle: '歌手' }],
    config: { render: 'webview', autoNowPlaying: true },
});
```

**前端样式接管（`css` / `cssReplace`，仅 `render: 'webview'`）**：通过 `config.css` 把一段 CSS 字符串注入自绘菜单专用的 `<style>` 层，每次右键弹出时应用，**完全由前端决定菜单视觉**（颜色 / 字体 / 留白 / 圆角 / 阴影 / 深浅色 / 动效等）。

- 默认 **override 叠加** 模式：你的规则叠加在内置样式之上，按菜单的**稳定 class 名**编写并靠源序或 `!important` 取胜。可用的稳定 class（自绘菜单 overlay 是独立顶层 document，宿主页的 `::part()` 无法跨 document 触达，故钩子 = class 名）：容器 `.fb-menu`；菜单项 `.fb-item`（+ `.nrm` / `.disabled` / `.active` / `.checked` / `.has-sub`）、图标列 `.fb-item-ico`、子菜单箭头 `.fb-arrow`、分隔线 `.fb-sep`；nowplaying `.fb-np` / `.fb-np-cover` / `.fb-np-text` / `.fb-np-title` / `.fb-np-sub`；rating `.fb-rating` / `.fb-stars` / `.fb-star`（+ `.on`）；slider `.fb-slider` / `.fb-slider-track` / `.fb-slider-fill` / `.fb-slider-thumb` / `.fb-slider-val`。
- `cssReplace: true` 切 **replace** 模式：禁用全部内置默认样式，整张菜单（含入场动画）以你的 `css` 为准，仅保留一层**受保护结构层**（根容器盒模型 / 溢出 / 定位 / 根显隐）以保证内容尺寸窗测量稳定，不可被覆盖。
- `native` 后端忽略 `css` / `cssReplace`。

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

**DWM 背景效果（`backdrop` / `backdropDarkMode`，仅 `render: 'webview'`）**：自绘托盘菜单的系统级背景材质，词表与主窗口一致：`'acrylic'`（默认，瞬态面 = 菜单语义正确）/ `'mica'` / `'mica-alt'` / `'none'`，每次右键弹出时应用、可随主题切换。`backdropDarkMode`（默认 `true`）控制背景暗色调，前端可据主题传 `false` 跟随浅色。注意：自绘菜单 `.fb-menu` 默认背景**不透明会遮住背景效果**——需前端经 `css` 把 `.fb-menu` 背景改半透明才能透出亚克力/云母。`native` 后端忽略。

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

> ⚠️ **DWM 背景（亚克力/云母）与淡入淡出动画冲突**：DWM 背景是**窗口级二元效果**，随 Win32 窗口瞬间显示/隐藏，**不随 CSS 动画淡入淡出**（`closeAnimationMs` 只动画 web 内容）。两者同用时背景会"啪"地出现/消失而内容在淡——过场不同步。想要全程平滑的开关动画，请改用 **CSS 半透明背景**（`backdrop: 'none'` + `.fb-menu{background:rgba(...)}`）代替 DWM 背景。**真·桌面模糊** 与 **可动画过场** 二选一（Windows 合成架构所限，非实现 bug；overlay 为 WebView2 Visual Hosting/DirectComposition 透明窗，无法用 `WS_EX_LAYERED` 淡整窗 alpha）。

---

### tray.setMinimizeToTray

窗口最小化时隐藏到托盘而非任务栏。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| enabled | boolean | ✓ | 是否启用 |

---

### tray.setCloseToTray

关闭窗口时隐藏到托盘而非退出。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| enabled | boolean | ✓ | 是否启用 |

---

### tray.isVisible

查询托盘图标是否存在。

**返回值**：`{ "success": true, "visible": boolean }`

---

### tray.appendMenuItems

增量追加菜单项到指定分区。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| items | TrayMenuItem[] | ✓ | 要追加的菜单项数组 |
| position | `'top' \| 'playback' \| 'bottom'` | ✗ | 目标分区，默认 `'top'` |

**返回值**：`{ "success": true }`

---

### tray.removeMenuItems

按 id 跨分区移除菜单项。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| ids | string[] | ✓ | 要移除的菜单项 id 数组 |

**返回值**：`{ "success": true, "removed": number }` — `removed` 为实际删除数量

---

### tray.clearMenuItems

清空指定分区菜单。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| position | `'top' \| 'playback' \| 'bottom'` | ✗ | 目标分区；省略时清空全部三个分区 |

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
| id | string | ✓ | 目标菜单项 id（跨三个分区递归查找，含子菜单） |
| checked | boolean | 可选 | 新的勾选状态（与 enabled 至少提供一个） |
| enabled | boolean | 可选 | 新的启用状态（与 checked 至少提供一个） |

**返回值**：`{ "success": true, "found": true }`（`found` 表示是否找到该 id 的菜单项）

---

### 事件（Tray）

| 事件 | 数据 | 描述 |
| --- | --- | --- |
| `tray:click` | `{ button: number, x: number, y: number }` | 托盘图标被单击（button: 0=左键） |
| `tray:doubleClick` | `{ x: number, y: number }` | 托盘图标被双击 |
| `tray:menuItemClicked` | `{ id: string }` | 右键菜单项被点击 |
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
        showSystemItems: true,         // 自动注入 bottom 分区 Exit foobar2000
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
