# Tray 托盘 API

Windows 系统托盘图标、托盘菜单与气泡通知，以及最小化或关闭到托盘。

## Tray API — 系统托盘（15 个 API）

### tray.create

<!-- api-schema:begin tray.create -->
创建托盘图标。其他 `tray.*` 方法之前先调用一次：没创建时它们照样成功返回，但图标不显示、托盘事件不触发、`isVisible` 报 `false`。面板模式、主窗口没有句柄、shell 拒绝注册图标时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `icon` | `string` | 否 | 要显示的图标。 |
| `tooltip` | `string` | 否 | 悬停提示文本。默认 `"foobar2000"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> ⚠️ **必须在前端生命周期初始化阶段显式调用一次**（推荐放在 Vue/React 应用启动钩子或脚本入口）。其他所有 `tray.*` API（`setIcon` / `setTooltip` / `setContextMenu` / `showBalloon` / `appendMenuItems` 等）依赖 `tray.create` 建立的 Shell_NotifyIcon 注册；如果跳过，调用本身会成功返回但**托盘图标不显示、事件不触发、`tray.isVisible` 返回 false**。

```javascript
await fb2k.invoke('tray.create', { tooltip: 'foobar2000 - 已停止' });
```

---

### tray.destroy

<!-- api-schema:begin tray.destroy -->
移除托盘图标。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.setIcon

<!-- api-schema:begin tray.setIcon -->
更换托盘图标图像；图标未创建时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `icon` | `string` | 否 | 要显示的图标。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.setTooltip

<!-- api-schema:begin tray.setTooltip -->
更新托盘图标的悬停提示文本；图标未创建时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `tooltip` | `string` | 否 | 悬停提示文本；空串即清除。默认 `""`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
fb2k.on('playback:trackChanged', async (track) => {
    await fb2k.invoke('tray.setTooltip', {
        tooltip: `♪ ${track.artist ?? ''} - ${track.title ?? ''}`,
    });
});
```

---

### tray.showBalloon

<!-- api-schema:begin tray.showBalloon -->
从托盘图标显示气泡通知；图标未创建时失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 通知标题。默认 `""`。 |
| `message` | `string` | 否 | 通知正文。默认 `""`。 |
| `icon` | `"info" \| "warning" \| "error"` | 否 | 气泡里的图标。默认 `"info"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.setContextMenu

<!-- api-schema:begin tray.setContextMenu -->
整体替换某一区（`config.customPosition`，默认 `top`）的用户行，其余区不动。任一行被拒或超出资源上限时什么都不存。普通行点击经 `tray:menuItemClicked` 上报；内置播放/系统行与声明了 `playbackAction` 的行由插件原生执行，不发该事件。要一次替换全部分区，用 `setMenuZones`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `TrayMenuItem[]` | 是 | 该区的行。 |
| `items[].id` | `string` | 否 | `tray:menuItemClicked` 上报的标识；分隔线可省略。精确的 `_sys_show` 与 `_sys_exit` 是原生的「显示主窗口」与「退出」行：保留调用方的文字，并跳过对应的内置注入。 |
| `items[].label` | `string` | 否 | 显示文本。 |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 行类型。`checkbox` 是「可勾选的 normal 行」的旧写法；勾选状态本身由 `checked` 决定。默认 `"normal"`。 |
| `items[].enabled` | `boolean` | 否 | 是否可点击。默认 `true`。 |
| `items[].visible` | `boolean` | 否 | 是否显示。默认 `true`。 |
| `items[].checked` | `boolean` | 否 | 勾选状态。给了这个键（含 `false`）该行就是可勾选的：`webview` 后端映射为 `menuitemcheckbox`，`getMenuItems` 会带回这个键。省略则不可勾选。 |
| `items[].icon` | `string` | 否 | 保留字段，两个后端都不绘制；行图标用 `iconSvg`。 |
| `items[].iconSvg` | `TrayIconSvg` | 否 | `webview` 后端画在文字前的图标。同一层只要有一行带可渲染图标，该层所有 `normal` 与 `submenu` 行都预留图标列。 |
| `items[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `items[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `items[].cover` | `string` | 否 | `nowplaying` 行的封面：`data:` URL、`http(s)://` URL 或裸 Base64 JPEG。开了 `config.autoNowPlaying` 时留空由正在播放曲目的封面补上（仅 `webview`）。 |
| `items[].title` | `string` | 否 | `nowplaying` 行的第一行，通常是曲名；缺省用 `label`。 |
| `items[].subtitle` | `string` | 否 | `nowplaying` 行的第二行，通常是艺术家或专辑。 |
| `items[].value` | `integer` | 否 | 当前值：`rating` 是 `0` 到 `5` 星，`slider` 是 `min` 到 `max` 内的值（会被夹取），`segmented` 是选中段的索引。只对这三种类型回报。 |
| `items[].min` | `integer` | 否 | `slider` 的下限；大于 `max` 时两者互换。 |
| `items[].max` | `integer` | 否 | `slider` 的上限。 |
| `items[].orientation` | `"horizontal" \| "vertical"` | 否 | `slider` 的方向（仅 `webview`）；省略即水平。纵向时 `min` 在底部。 |
| `items[].segments` | `TraySegment[]` | 否 | `segmented` 的分段，一行互斥选项；`value` 是选中段的索引。点选上报 `{ id, value }` 且菜单保持打开。 |
| `items[].segments[].label` | `string` | 否 | 分段文字；没有图标时显示。 |
| `items[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段图标；优先于文字。 |
| `items[].segments[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `items[].segments[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `items[].segments[].enabled` | `boolean` | 否 | `false` 置灰，不可选。默认 `true`。 |
| `items[].submenu` | `TrayMenuItem[]` | 否 | `submenu` 行的子行。 |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 该行被选中时由插件原生执行的播放命令，页面挂起（最小化、藏到托盘、锁屏）时仍可用；这样的行不发 `tray:menuItemClicked`。只能写在 `normal` 叶子上，写在别处整次调用失败。 |
| `config` | `TrayMenuConfig` | 否 | 要改的菜单级选项。 |
| `config.showPlaybackControls` | `boolean` | 否 | 向 `playback` 区注入内置的上一首、播放/暂停、下一首、停止行。默认 `true`。 |
| `config.showSystemItems` | `boolean` | 否 | 向 `bottom` 区注入原生的「显示主窗口」与「退出」行；页面挂起时两者仍可用。默认 `true`。 |
| `config.customPosition` | `"top" \| "playback" \| "bottom"` | 否 | `setContextMenu` 把行写进哪个区。默认 `top`。 |
| `config.render` | `"native" \| "webview"` | 否 | 菜单后端：Win32 菜单，或自绘的 WebView2 浮层（渲染富类型行与下面的样式选项）。默认 `native`。 |
| `config.autoNowPlaying` | `boolean` | 否 | 菜单打开时用正在播放的曲目补 `nowplaying` 行留空的 `cover`、`title`、`subtitle`；调用方给的值优先。封面缩到 64 px，超过 256 KiB 省略；`cover` 只在 `webview` 下补。默认 `false`。 |
| `config.css` | `string` | 否 | 每次打开 `webview` 菜单时注入的样式表，叠加在内置样式之上；按菜单的稳定 class 名（`.fb-menu`、`.fb-item`、`.fb-sep` 等）编写。 |
| `config.cssReplace` | `boolean` | 否 | `true` 时停用内置样式，只留 `css` 与受保护的结构层（仅 `webview`）。默认 `false`。 |
| `config.backdrop` | `"acrylic" \| "mica" \| "mica-alt" \| "none"` | 否 | `webview` 菜单的 DWM 背景材质，取值与窗口相同。随窗口瞬间出现/消失，不能用 CSS 淡入淡出。默认 `acrylic`。 |
| `config.backdropDarkMode` | `boolean` | 否 | 背景材质的暗色调（仅 `webview`）；`false` 跟随浅色主题。默认 `true`。 |
| `config.closeAnimationMs` | `integer` | 否 | `webview` 菜单在用户关闭时先播放退场过渡（`#menu.out`）的毫秒数，夹到 `0` 到 `1000`；`0` 立即隐藏。默认 `0`。 |
| `config.layoutMode` | `"flat" \| "zones"` | 否 | `webview` 菜单的 DOM 布局：`flat` 让行直接挂在根下，`zones` 把每个非空区包进 `.fb-zone[data-zone]`。默认 `flat`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

**nowplaying 自动补全（`autoNowPlaying`）**：开启后，`type: 'nowplaying'` 项中**前端没传的字段**会在右键弹出时由后端用当前曲目自动补全（前端传了就用前端的，**前端优先**）。`title` 走 `%title%`（自动回退文件名），`subtitle` 走 `%artist%`，兼容流媒体动态标题。

`cover` 自动补全仅适用于 `render: 'webview'`，只读 now-playing 的**内存封面缓存**，不做磁盘回退。缓存无封面时字段留空，前端可自行传入 `cover`，支持 `http(s)://`、`data:` 和裸 base64。最长边超过 **64 px** 时尝试缩到 64 px 并重编码为 JPEG；原图不超过 64 px，或缩图失败时，沿用原始字节与格式。所选图像字节超过 **256 KiB** 时省略封面，此检查发生在 base64 编码之前。默认样式表以 40×40 CSS px 绘制；64 px 缩略图在 125% 显示缩放下绘制到约 51 CSS px 以上时会放大采样。

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

- 默认 **override 叠加** 模式：你的规则叠加在内置样式之上，按菜单的**稳定 class 名**编写并靠源序或 `!important` 取胜。可用的稳定 class（自绘菜单 overlay 是独立顶层 document，宿主页的 `::part()` 无法跨 document 触达，故钩子 = class 名）：容器 `.fb-menu`；菜单项 `.fb-item`（+ `.nrm` / `.disabled` / `.active` / `.checked` / `.has-sub`）、图标列 `.fb-item-ico`、子菜单箭头 `.fb-arrow`、分隔线 `.fb-sep`；nowplaying `.fb-np` / `.fb-np-cover` / `.fb-np-text` / `.fb-np-title` / `.fb-np-sub`；rating `.fb-rating` / `.fb-stars` / `.fb-star`（+ `.on`）/ `.fb-rating-control`；slider `.fb-slider` / `.fb-slider-track` / `.fb-slider-fill` / `.fb-slider-thumb` / `.fb-slider-val` / `.fb-slider-control`；segmented `.fb-seg` / `.fb-seg-btn`。其中 `.fb-rating-control`、`.fb-slider-control` 与选中的 `.fb-seg-btn` 是仅有的三个会拿到真实 DOM 焦点的元素，编辑态的浏览器默认焦点环画在它们身上；内置样式与受保护层都不给这个焦点环写规则，`cssReplace` 模式下请针对这三个选择器设置或重置。
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
if (ver.success === false) throw new Error(ver.error);
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

### tray.setMenuZones

<!-- api-schema:begin tray.setMenuZones -->
一次替换三个区的全部用户行，没给出的区清空。行与 `config` 的规则同 `setContextMenu`；任一行被拒或超出资源上限时什么都不存，`config` 也不改。整份菜单要变时用它，不要先 `clearMenuItems` 再 `appendMenuItems`：分开的几次调用之间菜单可能被打开，只显示更新的一部分；两次这样的更新交错时，行会被加两遍。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `top` | `TrayMenuItem[]` | 否 | `top` 区的行；省略即清空该区。 |
| `top[].id` | `string` | 否 | `tray:menuItemClicked` 上报的标识；分隔线可省略。精确的 `_sys_show` 与 `_sys_exit` 是原生的「显示主窗口」与「退出」行：保留调用方的文字，并跳过对应的内置注入。 |
| `top[].label` | `string` | 否 | 显示文本。 |
| `top[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 行类型。`checkbox` 是「可勾选的 normal 行」的旧写法；勾选状态本身由 `checked` 决定。默认 `"normal"`。 |
| `top[].enabled` | `boolean` | 否 | 是否可点击。默认 `true`。 |
| `top[].visible` | `boolean` | 否 | 是否显示。默认 `true`。 |
| `top[].checked` | `boolean` | 否 | 勾选状态。给了这个键（含 `false`）该行就是可勾选的：`webview` 后端映射为 `menuitemcheckbox`，`getMenuItems` 会带回这个键。省略则不可勾选。 |
| `top[].icon` | `string` | 否 | 保留字段，两个后端都不绘制；行图标用 `iconSvg`。 |
| `top[].iconSvg` | `TrayIconSvg` | 否 | `webview` 后端画在文字前的图标。同一层只要有一行带可渲染图标，该层所有 `normal` 与 `submenu` 行都预留图标列。 |
| `top[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `top[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `top[].cover` | `string` | 否 | `nowplaying` 行的封面：`data:` URL、`http(s)://` URL 或裸 Base64 JPEG。开了 `config.autoNowPlaying` 时留空由正在播放曲目的封面补上（仅 `webview`）。 |
| `top[].title` | `string` | 否 | `nowplaying` 行的第一行，通常是曲名；缺省用 `label`。 |
| `top[].subtitle` | `string` | 否 | `nowplaying` 行的第二行，通常是艺术家或专辑。 |
| `top[].value` | `integer` | 否 | 当前值：`rating` 是 `0` 到 `5` 星，`slider` 是 `min` 到 `max` 内的值（会被夹取），`segmented` 是选中段的索引。只对这三种类型回报。 |
| `top[].min` | `integer` | 否 | `slider` 的下限；大于 `max` 时两者互换。 |
| `top[].max` | `integer` | 否 | `slider` 的上限。 |
| `top[].orientation` | `"horizontal" \| "vertical"` | 否 | `slider` 的方向（仅 `webview`）；省略即水平。纵向时 `min` 在底部。 |
| `top[].segments` | `TraySegment[]` | 否 | `segmented` 的分段，一行互斥选项；`value` 是选中段的索引。点选上报 `{ id, value }` 且菜单保持打开。 |
| `top[].segments[].label` | `string` | 否 | 分段文字；没有图标时显示。 |
| `top[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段图标；优先于文字。 |
| `top[].segments[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `top[].segments[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `top[].segments[].enabled` | `boolean` | 否 | `false` 置灰，不可选。默认 `true`。 |
| `top[].submenu` | `TrayMenuItem[]` | 否 | `submenu` 行的子行。 |
| `top[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 该行被选中时由插件原生执行的播放命令，页面挂起（最小化、藏到托盘、锁屏）时仍可用；这样的行不发 `tray:menuItemClicked`。只能写在 `normal` 叶子上，写在别处整次调用失败。 |
| `playback` | `TrayMenuItem[]` | 否 | `playback` 区的行；省略即清空该区。`config.showPlaybackControls` 注入的内置行不存在区里，清空该区不会去掉它们。 |
| `playback[].id` | `string` | 否 | `tray:menuItemClicked` 上报的标识；分隔线可省略。精确的 `_sys_show` 与 `_sys_exit` 是原生的「显示主窗口」与「退出」行：保留调用方的文字，并跳过对应的内置注入。 |
| `playback[].label` | `string` | 否 | 显示文本。 |
| `playback[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 行类型。`checkbox` 是「可勾选的 normal 行」的旧写法；勾选状态本身由 `checked` 决定。默认 `"normal"`。 |
| `playback[].enabled` | `boolean` | 否 | 是否可点击。默认 `true`。 |
| `playback[].visible` | `boolean` | 否 | 是否显示。默认 `true`。 |
| `playback[].checked` | `boolean` | 否 | 勾选状态。给了这个键（含 `false`）该行就是可勾选的：`webview` 后端映射为 `menuitemcheckbox`，`getMenuItems` 会带回这个键。省略则不可勾选。 |
| `playback[].icon` | `string` | 否 | 保留字段，两个后端都不绘制；行图标用 `iconSvg`。 |
| `playback[].iconSvg` | `TrayIconSvg` | 否 | `webview` 后端画在文字前的图标。同一层只要有一行带可渲染图标，该层所有 `normal` 与 `submenu` 行都预留图标列。 |
| `playback[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `playback[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `playback[].cover` | `string` | 否 | `nowplaying` 行的封面：`data:` URL、`http(s)://` URL 或裸 Base64 JPEG。开了 `config.autoNowPlaying` 时留空由正在播放曲目的封面补上（仅 `webview`）。 |
| `playback[].title` | `string` | 否 | `nowplaying` 行的第一行，通常是曲名；缺省用 `label`。 |
| `playback[].subtitle` | `string` | 否 | `nowplaying` 行的第二行，通常是艺术家或专辑。 |
| `playback[].value` | `integer` | 否 | 当前值：`rating` 是 `0` 到 `5` 星，`slider` 是 `min` 到 `max` 内的值（会被夹取），`segmented` 是选中段的索引。只对这三种类型回报。 |
| `playback[].min` | `integer` | 否 | `slider` 的下限；大于 `max` 时两者互换。 |
| `playback[].max` | `integer` | 否 | `slider` 的上限。 |
| `playback[].orientation` | `"horizontal" \| "vertical"` | 否 | `slider` 的方向（仅 `webview`）；省略即水平。纵向时 `min` 在底部。 |
| `playback[].segments` | `TraySegment[]` | 否 | `segmented` 的分段，一行互斥选项；`value` 是选中段的索引。点选上报 `{ id, value }` 且菜单保持打开。 |
| `playback[].segments[].label` | `string` | 否 | 分段文字；没有图标时显示。 |
| `playback[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段图标；优先于文字。 |
| `playback[].segments[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `playback[].segments[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `playback[].segments[].enabled` | `boolean` | 否 | `false` 置灰，不可选。默认 `true`。 |
| `playback[].submenu` | `TrayMenuItem[]` | 否 | `submenu` 行的子行。 |
| `playback[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 该行被选中时由插件原生执行的播放命令，页面挂起（最小化、藏到托盘、锁屏）时仍可用；这样的行不发 `tray:menuItemClicked`。只能写在 `normal` 叶子上，写在别处整次调用失败。 |
| `bottom` | `TrayMenuItem[]` | 否 | `bottom` 区的行；省略即清空该区。`config.showSystemItems` 注入的内置行不存在区里，清空该区不会去掉它们。 |
| `bottom[].id` | `string` | 否 | `tray:menuItemClicked` 上报的标识；分隔线可省略。精确的 `_sys_show` 与 `_sys_exit` 是原生的「显示主窗口」与「退出」行：保留调用方的文字，并跳过对应的内置注入。 |
| `bottom[].label` | `string` | 否 | 显示文本。 |
| `bottom[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 行类型。`checkbox` 是「可勾选的 normal 行」的旧写法；勾选状态本身由 `checked` 决定。默认 `"normal"`。 |
| `bottom[].enabled` | `boolean` | 否 | 是否可点击。默认 `true`。 |
| `bottom[].visible` | `boolean` | 否 | 是否显示。默认 `true`。 |
| `bottom[].checked` | `boolean` | 否 | 勾选状态。给了这个键（含 `false`）该行就是可勾选的：`webview` 后端映射为 `menuitemcheckbox`，`getMenuItems` 会带回这个键。省略则不可勾选。 |
| `bottom[].icon` | `string` | 否 | 保留字段，两个后端都不绘制；行图标用 `iconSvg`。 |
| `bottom[].iconSvg` | `TrayIconSvg` | 否 | `webview` 后端画在文字前的图标。同一层只要有一行带可渲染图标，该层所有 `normal` 与 `submenu` 行都预留图标列。 |
| `bottom[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `bottom[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `bottom[].cover` | `string` | 否 | `nowplaying` 行的封面：`data:` URL、`http(s)://` URL 或裸 Base64 JPEG。开了 `config.autoNowPlaying` 时留空由正在播放曲目的封面补上（仅 `webview`）。 |
| `bottom[].title` | `string` | 否 | `nowplaying` 行的第一行，通常是曲名；缺省用 `label`。 |
| `bottom[].subtitle` | `string` | 否 | `nowplaying` 行的第二行，通常是艺术家或专辑。 |
| `bottom[].value` | `integer` | 否 | 当前值：`rating` 是 `0` 到 `5` 星，`slider` 是 `min` 到 `max` 内的值（会被夹取），`segmented` 是选中段的索引。只对这三种类型回报。 |
| `bottom[].min` | `integer` | 否 | `slider` 的下限；大于 `max` 时两者互换。 |
| `bottom[].max` | `integer` | 否 | `slider` 的上限。 |
| `bottom[].orientation` | `"horizontal" \| "vertical"` | 否 | `slider` 的方向（仅 `webview`）；省略即水平。纵向时 `min` 在底部。 |
| `bottom[].segments` | `TraySegment[]` | 否 | `segmented` 的分段，一行互斥选项；`value` 是选中段的索引。点选上报 `{ id, value }` 且菜单保持打开。 |
| `bottom[].segments[].label` | `string` | 否 | 分段文字；没有图标时显示。 |
| `bottom[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段图标；优先于文字。 |
| `bottom[].segments[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `bottom[].segments[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `bottom[].segments[].enabled` | `boolean` | 否 | `false` 置灰，不可选。默认 `true`。 |
| `bottom[].submenu` | `TrayMenuItem[]` | 否 | `submenu` 行的子行。 |
| `bottom[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 该行被选中时由插件原生执行的播放命令，页面挂起（最小化、藏到托盘、锁屏）时仍可用；这样的行不发 `tray:menuItemClicked`。只能写在 `normal` 叶子上，写在别处整次调用失败。 |
| `config` | `TrayMenuConfig` | 否 | 要改的菜单级选项。`customPosition` 存下来供之后的 `setContextMenu` 使用，对本次调用不起作用。 |
| `config.showPlaybackControls` | `boolean` | 否 | 向 `playback` 区注入内置的上一首、播放/暂停、下一首、停止行。默认 `true`。 |
| `config.showSystemItems` | `boolean` | 否 | 向 `bottom` 区注入原生的「显示主窗口」与「退出」行；页面挂起时两者仍可用。默认 `true`。 |
| `config.customPosition` | `"top" \| "playback" \| "bottom"` | 否 | `setContextMenu` 把行写进哪个区。默认 `top`。 |
| `config.render` | `"native" \| "webview"` | 否 | 菜单后端：Win32 菜单，或自绘的 WebView2 浮层（渲染富类型行与下面的样式选项）。默认 `native`。 |
| `config.autoNowPlaying` | `boolean` | 否 | 菜单打开时用正在播放的曲目补 `nowplaying` 行留空的 `cover`、`title`、`subtitle`；调用方给的值优先。封面缩到 64 px，超过 256 KiB 省略；`cover` 只在 `webview` 下补。默认 `false`。 |
| `config.css` | `string` | 否 | 每次打开 `webview` 菜单时注入的样式表，叠加在内置样式之上；按菜单的稳定 class 名（`.fb-menu`、`.fb-item`、`.fb-sep` 等）编写。 |
| `config.cssReplace` | `boolean` | 否 | `true` 时停用内置样式，只留 `css` 与受保护的结构层（仅 `webview`）。默认 `false`。 |
| `config.backdrop` | `"acrylic" \| "mica" \| "mica-alt" \| "none"` | 否 | `webview` 菜单的 DWM 背景材质，取值与窗口相同。随窗口瞬间出现/消失，不能用 CSS 淡入淡出。默认 `acrylic`。 |
| `config.backdropDarkMode` | `boolean` | 否 | 背景材质的暗色调（仅 `webview`）；`false` 跟随浅色主题。默认 `true`。 |
| `config.closeAnimationMs` | `integer` | 否 | `webview` 菜单在用户关闭时先播放退场过渡（`#menu.out`）的毫秒数，夹到 `0` 到 `1000`；`0` 立即隐藏。默认 `0`。 |
| `config.layoutMode` | `"flat" \| "zones"` | 否 | `webview` 菜单的 DOM 布局：`flat` 让行直接挂在根下，`zones` 把每个非空区包进 `.fb-zone[data-zone]`。默认 `flat`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

宿主在主线程上一步做完整次替换，菜单也在主线程上构建，所以右键看到的要么是旧菜单、要么是新菜单，不会是两者拼起来的。同一页面的调用按发出顺序执行：连发两次更新，留下的是后一次。

```javascript
await fb2k.invoke('tray.setMenuZones', {
    top: [
        { id: 'nowplaying', type: 'nowplaying', title: '曲名', subtitle: '艺术家' },
        { type: 'separator' },
    ],
    bottom: [{ id: 'settings', label: '设置' }],
    config: { showPlaybackControls: true },
});
```

---

### tray.setMinimizeToTray

<!-- api-schema:begin tray.setMinimizeToTray -->
窗口最小化时隐藏到托盘而不是任务栏。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | `true` 即隐藏到托盘。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.setCloseToTray

<!-- api-schema:begin tray.setCloseToTray -->
关闭窗口时隐藏到托盘而不是退出。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | `true` 即隐藏到托盘。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.isVisible

<!-- api-schema:begin tray.isVisible -->
报告托盘图标是否存在。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `visible` | `boolean` | 图标是否存在。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.appendMenuItems

<!-- api-schema:begin tray.appendMenuItems -->
向某一区追加行；校验规则同 `setContextMenu`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `TrayMenuItem[]` | 是 | 要追加的行。 |
| `items[].id` | `string` | 否 | `tray:menuItemClicked` 上报的标识；分隔线可省略。精确的 `_sys_show` 与 `_sys_exit` 是原生的「显示主窗口」与「退出」行：保留调用方的文字，并跳过对应的内置注入。 |
| `items[].label` | `string` | 否 | 显示文本。 |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 行类型。`checkbox` 是「可勾选的 normal 行」的旧写法；勾选状态本身由 `checked` 决定。默认 `"normal"`。 |
| `items[].enabled` | `boolean` | 否 | 是否可点击。默认 `true`。 |
| `items[].visible` | `boolean` | 否 | 是否显示。默认 `true`。 |
| `items[].checked` | `boolean` | 否 | 勾选状态。给了这个键（含 `false`）该行就是可勾选的：`webview` 后端映射为 `menuitemcheckbox`，`getMenuItems` 会带回这个键。省略则不可勾选。 |
| `items[].icon` | `string` | 否 | 保留字段，两个后端都不绘制；行图标用 `iconSvg`。 |
| `items[].iconSvg` | `TrayIconSvg` | 否 | `webview` 后端画在文字前的图标。同一层只要有一行带可渲染图标，该层所有 `normal` 与 `submenu` 行都预留图标列。 |
| `items[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `items[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `items[].cover` | `string` | 否 | `nowplaying` 行的封面：`data:` URL、`http(s)://` URL 或裸 Base64 JPEG。开了 `config.autoNowPlaying` 时留空由正在播放曲目的封面补上（仅 `webview`）。 |
| `items[].title` | `string` | 否 | `nowplaying` 行的第一行，通常是曲名；缺省用 `label`。 |
| `items[].subtitle` | `string` | 否 | `nowplaying` 行的第二行，通常是艺术家或专辑。 |
| `items[].value` | `integer` | 否 | 当前值：`rating` 是 `0` 到 `5` 星，`slider` 是 `min` 到 `max` 内的值（会被夹取），`segmented` 是选中段的索引。只对这三种类型回报。 |
| `items[].min` | `integer` | 否 | `slider` 的下限；大于 `max` 时两者互换。 |
| `items[].max` | `integer` | 否 | `slider` 的上限。 |
| `items[].orientation` | `"horizontal" \| "vertical"` | 否 | `slider` 的方向（仅 `webview`）；省略即水平。纵向时 `min` 在底部。 |
| `items[].segments` | `TraySegment[]` | 否 | `segmented` 的分段，一行互斥选项；`value` 是选中段的索引。点选上报 `{ id, value }` 且菜单保持打开。 |
| `items[].segments[].label` | `string` | 否 | 分段文字；没有图标时显示。 |
| `items[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段图标；优先于文字。 |
| `items[].segments[].iconSvg.viewBox` | `string` | 是 | SVG 的 `viewBox` 属性。 |
| `items[].segments[].iconSvg.content` | `string` | 是 | SVG 的内部标记。 |
| `items[].segments[].enabled` | `boolean` | 否 | `false` 置灰，不可选。默认 `true`。 |
| `items[].submenu` | `TrayMenuItem[]` | 否 | `submenu` 行的子行。 |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 该行被选中时由插件原生执行的播放命令，页面挂起（最小化、藏到托盘、锁屏）时仍可用；这样的行不发 `tray:menuItemClicked`。只能写在 `normal` 叶子上，写在别处整次调用失败。 |
| `position` | `"top" \| "playback" \| "bottom"` | 否 | 追加到哪个区。默认 `"top"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.removeMenuItems

<!-- api-schema:begin tray.removeMenuItems -->
按 id 从所有区（含子菜单）移除行。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `ids` | `string[]` | 是 | 要移除的行的 id。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `removed` | `integer` | 实际移除的行数；有 id 不存在时少于 id 的个数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.clearMenuItems

<!-- api-schema:begin tray.clearMenuItems -->
清空某一区；省略 `position` 时清空全部三区。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `position` | `"top" \| "playback" \| "bottom"` | 否 | 要清空的区；省略即全部。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.getMenuItems

<!-- api-schema:begin tray.getMenuItems -->
按 `top`、`playback`、`bottom` 的顺序平铺列出各区存储的用户行；运行时为 `showPlaybackControls` / `showSystemItems` 注入的行不在其中。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `TrayMenuItem[]` | 存储的用户行。 |
| `items[].id` | `string` | `tray:menuItemClicked` 上报的标识；分隔线可省略。精确的 `_sys_show` 与 `_sys_exit` 是原生的「显示主窗口」与「退出」行：保留调用方的文字，并跳过对应的内置注入。 |
| `items[].label` | `string` | 显示文本。 |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 行类型。`checkbox` 是「可勾选的 normal 行」的旧写法；勾选状态本身由 `checked` 决定。 |
| `items[].enabled` | `boolean` | 是否可点击。 |
| `items[].visible` | `boolean` | 是否显示。 |
| `items[].checked` | `boolean` | 勾选状态。给了这个键（含 `false`）该行就是可勾选的：`webview` 后端映射为 `menuitemcheckbox`，`getMenuItems` 会带回这个键。省略则不可勾选。 |
| `items[].icon` | `string` | 保留字段，两个后端都不绘制；行图标用 `iconSvg`。 |
| `items[].iconSvg` | `TrayIconSvg` | `webview` 后端画在文字前的图标。同一层只要有一行带可渲染图标，该层所有 `normal` 与 `submenu` 行都预留图标列。 |
| `items[].iconSvg.viewBox` | `string` | SVG 的 `viewBox` 属性。 |
| `items[].iconSvg.content` | `string` | SVG 的内部标记。 |
| `items[].cover` | `string` | `nowplaying` 行的封面：`data:` URL、`http(s)://` URL 或裸 Base64 JPEG。开了 `config.autoNowPlaying` 时留空由正在播放曲目的封面补上（仅 `webview`）。 |
| `items[].title` | `string` | `nowplaying` 行的第一行，通常是曲名；缺省用 `label`。 |
| `items[].subtitle` | `string` | `nowplaying` 行的第二行，通常是艺术家或专辑。 |
| `items[].value` | `integer` | 当前值：`rating` 是 `0` 到 `5` 星，`slider` 是 `min` 到 `max` 内的值（会被夹取），`segmented` 是选中段的索引。只对这三种类型回报。 |
| `items[].min` | `integer` | `slider` 的下限；大于 `max` 时两者互换。 |
| `items[].max` | `integer` | `slider` 的上限。 |
| `items[].orientation` | `"horizontal" \| "vertical"` | `slider` 的方向（仅 `webview`）；省略即水平。纵向时 `min` 在底部。 |
| `items[].segments` | `TraySegment[]` | `segmented` 的分段，一行互斥选项；`value` 是选中段的索引。点选上报 `{ id, value }` 且菜单保持打开。 |
| `items[].segments[].label` | `string` | 分段文字；没有图标时显示。 |
| `items[].segments[].iconSvg` | `TrayIconSvg` | 分段图标；优先于文字。 |
| `items[].segments[].iconSvg.viewBox` | `string` | SVG 的 `viewBox` 属性。 |
| `items[].segments[].iconSvg.content` | `string` | SVG 的内部标记。 |
| `items[].segments[].enabled` | `boolean` | `false` 置灰，不可选。 |
| `items[].submenu` | `TrayMenuItem[]` | `submenu` 行的子行。 |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 该行被选中时由插件原生执行的播放命令，页面挂起（最小化、藏到托盘、锁屏）时仍可用；这样的行不发 `tray:menuItemClicked`。只能写在 `normal` 叶子上，写在别处整次调用失败。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 仅返回用户通过 `setContextMenu` / `appendMenuItems` 添加的项；由 `showPlaybackControls` / `showSystemItems` 自动注入的内置项不在返回值内。内置项使用 `_pb_` 与 `_sys_` 前缀的保留命名空间，用户自定义菜单项应避免使用这些前缀。

---

### tray.setMenuItemState

<!-- api-schema:begin tray.setMenuItemState -->
原地改一行的 `checked` / `enabled`，按 id 跨三区递归（含子菜单）查找。给了 `checked`（哪怕是 `false`）就把该行变成可勾选。原生菜单每次打开时重建，改动在下次打开时可见。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `string` | 是 | 行的 id。不能为空。 |
| `checked` | `boolean` | 否 | 新的勾选状态。 |
| `enabled` | `boolean` | 否 | 新的可用状态。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 是否存在该 id 的行；成功时为 `true`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 原生托盘菜单每次打开时从存储数据重建，因此新状态在**下次打开**时生效（无法刷新已经打开的菜单，Win32 限制）。

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

`tray:beforeContextMenu` 异步事件配合 `setMenuZones` 整份替换，适合「按当前播放状态动态展示菜单项」场景；增量菜单 API（`appendMenuItems` / `removeMenuItems` / `clearMenuItems` / `getMenuItems`）用于局部改动与查询。基础初始化用法参见上方【完整示例】。

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

// 3. 菜单展示前异步通知：按曲目拼出 top 分区的整份行，一次调用替换
fb2k.on('tray:beforeContextMenu', async () => {
    const favorite = { id: 'addFav', label: '☆ 添加到收藏夹' };
    await fb2k.invoke('tray.setMenuZones', {
        top: currentTrack?.path
            ? [
                  favorite,
                  { type: 'separator' },
                  { id: 'revealInExplorer', label: '在文件管理器中显示' },
                  { id: 'copyPath',         label: '复制文件路径' },
              ]
            : [favorite],
    });
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
const res = await fb2k.invoke('tray.getMenuItems');
if (res.success === false) throw new Error(res.error);
const { items } = res;
console.log('当前用户菜单项：', items);

// 6. 按 id 批量移除多个项（跨分区生效）
await fb2k.invoke('tray.removeMenuItems', { ids: ['revealInExplorer', 'copyPath'] });
```

> **语义提示**：`tray:beforeContextMenu` 是异步通知，handler 内的菜单改动只影响下一次菜单弹出，不阻塞本次弹出。若希望首次右键菜单也包含上下文项，应在 `playback:trackChanged` 中同步预填充。两次右键靠得很近时两个 handler 会交错执行：用 `clearMenuItems` 加 `appendMenuItems` 拼菜单会把行加两遍，`setMenuZones` 整份替换则只留下后一次。

## 契约补充

以下补充这些方法的完整参数契约，不改变前文的已有说明。

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

内置 `showPlaybackControls` 注入项是**无状态**的：标签固定为「播放 / 暂停」「上一首」「下一首」「停止」（随 foobar2000 界面语言切换，但**不随播放态**变化），也不带图标。想让标签或图标跟随播放态，请关掉 `showPlaybackControls`、按下例自行声明 `playbackAction` 项，再用 `playback:*` 事件驱动它们的外观。

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

`taskbar.*` 与 `tray.*` 需要 standalone 主窗口。在 panel 中，两者都会失败，`code` 为
`PANEL_MODE_UNSUPPORTED`。先调用 `tray.create`，再使用托盘可见性、回调与
菜单操作。Windows 最多接受七个缩略图按钮；任务栏 COM 尚未初始化时，设置缩略图按钮也会失败。

托盘菜单由 `tray.setContextMenu` 配置，后续可使用 `tray.appendMenuItems`、
`tray.removeMenuItems`、`tray.clearMenuItems` 与 `tray.setMenuItemState` 更新。
整份菜单要换时调用一次 `tray.setMenuZones`，不要把这几个方法串起来调，这样中途打开的菜单不会只显示一半更新。
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

这套提升只认精确 id、**不看 `type`**，所以 `type: 'nowplaying'` 的封面卡也可以挂
`id: '_sys_show'`，点它同样原生恢复主窗口。但该用法**仅在 `render: 'webview'` 下成立**：
原生后端把 `nowplaying` 画成不可点击的灰色标题行，点不动，也不会触发任何路由。

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
