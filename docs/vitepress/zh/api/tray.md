# Tray 托盘 API

提供 Windows 系统托盘图标管理、托盘上下文菜单、气泡通知以及最小化/关闭到托盘集成能力。

## Tray API — 系统托盘（16 个 API） {#tray-api-—-系统托盘-15-个-api}

### tray.create

<!-- api-schema:begin tray.create -->
创建系统托盘图标（图标成功创建后方可接收用户输入与展示气泡通知；重复调用将保留此前通过 setClickAction 设置的点击动作；面板模式下、主窗口句柄缺失或 Shell 拒绝注册时调用失败）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `icon` | `string` | 否 | 托盘图标（.ico 文件原始 Base64 编码字符串；省略时使用默认图标） |
| `tooltip` | `string` | 否 | 鼠标悬停时的提示文本默认 `"foobar2000"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

在前端初始化时调用 `tray.create`，注册图标后才能接收点击或显示通知。`setClickAction` 可在创建前配置；没有图标时，`setIcon`、`setTooltip` 和 `showBalloon` 会失败，`tray.isVisible` 返回 `false`。

```javascript
await fb2k.invoke('tray.create', { tooltip: 'foobar2000 - 已停止' });
```

---

### tray.destroy

<!-- api-schema:begin tray.destroy -->
销毁系统托盘图标（取消挂起的待处理点击，并将左键点击动作重置为 event；若主窗口当前处于隐藏到托盘状态，将自动恢复显示以防界面无法唤回）

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.setIcon

<!-- api-schema:begin tray.setIcon -->
更换系统托盘图标图像（若尚未通过 tray.create 创建图标则调用失败）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `icon` | `string` | 否 | 待更新的托盘图标（.ico 文件原始 Base64 编码字符串） |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.setTooltip

<!-- api-schema:begin tray.setTooltip -->
更新系统托盘图标的悬停提示文本（若尚未通过 tray.create 创建图标则调用失败）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `tooltip` | `string` | 否 | 待更新的悬停提示文本（传入空字符串表示清除提示）默认 `""`。 |

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
通过系统托盘图标弹出气泡通知（若尚未通过 tray.create 创建图标则调用失败）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `title` | `string` | 否 | 气泡通知标题文本默认 `""`。 |
| `message` | `string` | 否 | 气泡通知正文文本默认 `""`。 |
| `icon` | `"info" \| "warning" \| "error"` | 否 | 气泡通知图标类型（info 信息、warning 警告、error 错误）默认 `"info"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.setContextMenu

<!-- api-schema:begin tray.setContextMenu -->
整体替换指定分区（由 config.customPosition 指定，默认为 top）的自定义菜单项（其余分区保持不变；任一菜单项校验失败或超出资源配额时整体放弃更新；普通菜单项点击时通过 tray:menuItemClicked 派发事件，内置播放/系统项与声明了 playbackAction 的项由宿主原生直接执行而不派发事件；如需单次原子更新全部三个分区请使用 setMenuZones）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `TrayMenuItem[]` | 是 | 写入目标分区的自定义菜单项列表 |
| `items[].id` | `string` | 否 | 菜单项唯一标识符（由 tray:menuItemClicked 事件上报；分隔线可省略；预留的 _sys_show 与 _sys_exit 将绑定原生显示主窗口与退出行为并抑制默认系统项注入） |
| `items[].label` | `string` | 否 | 菜单项显示文本 |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 菜单项展现类型（checkbox 为可勾选 normal 项的历史兼容写法，建议统一通过 checked 属性控制勾选状态）默认 `"normal"`。 |
| `items[].enabled` | `boolean` | 否 | 菜单项是否处于启用可交互状态默认 `true`。 |
| `items[].visible` | `boolean` | 否 | 菜单项是否可见默认 `true`。 |
| `items[].checked` | `boolean` | 否 | 菜单项勾选状态（传入该属性包括 false 将使菜单项转为可勾选模式，webview 后端映射为 menuitemcheckbox 并支持通过 getMenuItems 读取；缺省时表示不可勾选普通菜单项） |
| `items[].icon` | `string` | 否 | 保留字段（双后端均不绘制，图标定义请使用 iconSvg） |
| `items[].iconSvg` | `TrayIconSvg` | 否 | 菜单项前置矢量图标（仅 webview 后端绘制；同一层级内只要有一项配置了图标，同级全部 normal 与 submenu 项均会自动对齐预留图标列宽度） |
| `items[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `items[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `items[].cover` | `string` | 否 | nowplaying 正在播放卡片封面（支持 data: URL、http(s):// 链接或原始 Base64 JPEG；开启 config.autoNowPlaying 时缺省将自动拉取当前曲目前置封面；仅 webview 后端生效） |
| `items[].title` | `string` | 否 | nowplaying 正在播放卡片主标题（通常为曲目名称；缺省时回退使用 label） |
| `items[].subtitle` | `string` | 否 | nowplaying 正在播放卡片副标题（通常为艺术家或专辑信息） |
| `items[].value` | `integer` | 否 | 控件当前数值（rating 支持 0 至 5 星；slider 支持 min 至 max 范围内的数值，超出范围自动夹取；segmented 为当前选中分段索引；仅在此三类控件上生效并回传） |
| `items[].min` | `integer` | 否 | slider 滑块数值下限（若大于 max 将自动与 max 互换） |
| `items[].max` | `integer` | 否 | slider 滑块数值上限 |
| `items[].orientation` | `"horizontal" \| "vertical"` | 否 | slider 滑块布局方向（仅 webview 后端支持；缺省为水平方向，垂直方向时 min 位于底部） |
| `items[].segments` | `TraySegment[]` | 否 | segmented 分段选项列表（单行互斥选择项，value 为当前选中项索引；点击分段上报 { id, value } 且菜单保持展开） |
| `items[].segments[].label` | `string` | 否 | 分段文本标签（未提供图标时展示） |
| `items[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段矢量图标（优先级高于文本标签） |
| `items[].segments[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `items[].segments[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `items[].segments[].enabled` | `boolean` | 否 | 分段是否可用（为 false 时置灰且不可点击）默认 `true`。 |
| `items[].submenu` | `TrayMenuItem[]` | 否 | submenu 子菜单包含的子菜单项列表 |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 声明由宿主原生直接执行的播放命令（支持在主页面挂起、最小化或锁屏状态下稳定响应；配置后点击该项不触发 tray:menuItemClicked；仅允许配置在 normal 叶子节点上） |
| `config` | `TrayMenuConfig` | 否 | 待更新的托盘菜单全局配置选项 |
| `config.showPlaybackControls` | `boolean` | 否 | 是否在 playback 分区自动注入上一首、播放/暂停、下一首与停止等原生播放控制项（缺省为 true） |
| `config.showSystemItems` | `boolean` | 否 | 是否在 bottom 分区自动注入显示主窗口与退出等原生系统控制项（支持在页面深度挂起状态下可靠响应；缺省为 true） |
| `config.customPosition` | `"top" \| "playback" \| "bottom"` | 否 | setContextMenu 默认写入的目标菜单分区（缺省为 top） |
| `config.render` | `"native" \| "webview"` | 否 | 菜单渲染后端（native 为 Win32 原生菜单；webview 为基于 WebView2 的自绘现代浮层，支持富控件与自定义 CSS 样式；缺省为 native） |
| `config.autoNowPlaying` | `boolean` | 否 | 展开菜单时是否自动使用当前播放曲目填充 nowplaying 卡片中留空的 cover、title 与 subtitle 字段（调用方显式指定的字段优先；封面图像自动下采样至 64 像素且限制最大 256 KiB；仅 webview 后端生效；缺省为 false） |
| `config.css` | `string` | 否 | 注入 webview 自绘菜单的自定义 CSS 样式代码（叠加在内置样式之上，可通过 .fb-menu、.fb-item、.fb-sep 等稳定类名定制视觉样式） |
| `config.cssReplace` | `boolean` | 否 | 是否完全停用内置 CSS 样式（仅保留受保护的 DOM 结构层与用户自定义 css 规则；仅 webview 后端生效；缺省为 false） |
| `config.backdrop` | `"acrylic" \| "mica" \| "mica-alt" \| "none"` | 否 | webview 自绘菜单的 Windows DWM 材质背景效果（acrylic 亚克力、mica、mica-alt 或 none；材质紧贴窗口即时切换，不支持 CSS 透明度渐变过渡；缺省为 acrylic） |
| `config.backdropDarkMode` | `boolean` | 否 | DWM 背景材质是否强制启用深色调（仅 webview 后端生效；设为 false 时跟随浅色视觉风格；缺省为 true） |
| `config.closeAnimationMs` | `integer` | 否 | webview 自绘菜单在用户主动关闭时的退场过渡动画时长（单位毫秒，取值范围 0 至 1000；传入 0 表示立即关闭窗口；可配合 #menu.out CSS 选择器定制动画；缺省为 0） |
| `config.layoutMode` | `"flat" \| "zones"` | 否 | webview 自绘菜单的 DOM 容器布局模式（flat 表示所有菜单项平铺于根容器下；zones 表示将非空分区分别包裹于 .fb-zone[data-zone] 容器内；缺省为 flat） |

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
单次原子替换全部三个分区的自定义菜单项（未传入的分区将被清空；菜单项校验规则与 config 语义同 setContextMenu，任一菜单项校验失败或超限时整体放弃保存且不修改配置；整份菜单更新时建议优先使用本方法，避免通过 clearMenuItems 与 appendMenuItems 组合调用导致中间状态闪烁或并发交错重复）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `top` | `TrayMenuItem[]` | 否 | top 分区菜单项列表（省略时清空该分区） |
| `top[].id` | `string` | 否 | 菜单项唯一标识符（由 tray:menuItemClicked 事件上报；分隔线可省略；预留的 _sys_show 与 _sys_exit 将绑定原生显示主窗口与退出行为并抑制默认系统项注入） |
| `top[].label` | `string` | 否 | 菜单项显示文本 |
| `top[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 菜单项展现类型（checkbox 为可勾选 normal 项的历史兼容写法，建议统一通过 checked 属性控制勾选状态）默认 `"normal"`。 |
| `top[].enabled` | `boolean` | 否 | 菜单项是否处于启用可交互状态默认 `true`。 |
| `top[].visible` | `boolean` | 否 | 菜单项是否可见默认 `true`。 |
| `top[].checked` | `boolean` | 否 | 菜单项勾选状态（传入该属性包括 false 将使菜单项转为可勾选模式，webview 后端映射为 menuitemcheckbox 并支持通过 getMenuItems 读取；缺省时表示不可勾选普通菜单项） |
| `top[].icon` | `string` | 否 | 保留字段（双后端均不绘制，图标定义请使用 iconSvg） |
| `top[].iconSvg` | `TrayIconSvg` | 否 | 菜单项前置矢量图标（仅 webview 后端绘制；同一层级内只要有一项配置了图标，同级全部 normal 与 submenu 项均会自动对齐预留图标列宽度） |
| `top[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `top[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `top[].cover` | `string` | 否 | nowplaying 正在播放卡片封面（支持 data: URL、http(s):// 链接或原始 Base64 JPEG；开启 config.autoNowPlaying 时缺省将自动拉取当前曲目前置封面；仅 webview 后端生效） |
| `top[].title` | `string` | 否 | nowplaying 正在播放卡片主标题（通常为曲目名称；缺省时回退使用 label） |
| `top[].subtitle` | `string` | 否 | nowplaying 正在播放卡片副标题（通常为艺术家或专辑信息） |
| `top[].value` | `integer` | 否 | 控件当前数值（rating 支持 0 至 5 星；slider 支持 min 至 max 范围内的数值，超出范围自动夹取；segmented 为当前选中分段索引；仅在此三类控件上生效并回传） |
| `top[].min` | `integer` | 否 | slider 滑块数值下限（若大于 max 将自动与 max 互换） |
| `top[].max` | `integer` | 否 | slider 滑块数值上限 |
| `top[].orientation` | `"horizontal" \| "vertical"` | 否 | slider 滑块布局方向（仅 webview 后端支持；缺省为水平方向，垂直方向时 min 位于底部） |
| `top[].segments` | `TraySegment[]` | 否 | segmented 分段选项列表（单行互斥选择项，value 为当前选中项索引；点击分段上报 { id, value } 且菜单保持展开） |
| `top[].segments[].label` | `string` | 否 | 分段文本标签（未提供图标时展示） |
| `top[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段矢量图标（优先级高于文本标签） |
| `top[].segments[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `top[].segments[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `top[].segments[].enabled` | `boolean` | 否 | 分段是否可用（为 false 时置灰且不可点击）默认 `true`。 |
| `top[].submenu` | `TrayMenuItem[]` | 否 | submenu 子菜单包含的子菜单项列表 |
| `top[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 声明由宿主原生直接执行的播放命令（支持在主页面挂起、最小化或锁屏状态下稳定响应；配置后点击该项不触发 tray:menuItemClicked；仅允许配置在 normal 叶子节点上） |
| `playback` | `TrayMenuItem[]` | 否 | playback 分区菜单项列表（省略时清空该分区；showPlaybackControls 注入的原生项独立维护，不受此清空影响） |
| `playback[].id` | `string` | 否 | 菜单项唯一标识符（由 tray:menuItemClicked 事件上报；分隔线可省略；预留的 _sys_show 与 _sys_exit 将绑定原生显示主窗口与退出行为并抑制默认系统项注入） |
| `playback[].label` | `string` | 否 | 菜单项显示文本 |
| `playback[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 菜单项展现类型（checkbox 为可勾选 normal 项的历史兼容写法，建议统一通过 checked 属性控制勾选状态）默认 `"normal"`。 |
| `playback[].enabled` | `boolean` | 否 | 菜单项是否处于启用可交互状态默认 `true`。 |
| `playback[].visible` | `boolean` | 否 | 菜单项是否可见默认 `true`。 |
| `playback[].checked` | `boolean` | 否 | 菜单项勾选状态（传入该属性包括 false 将使菜单项转为可勾选模式，webview 后端映射为 menuitemcheckbox 并支持通过 getMenuItems 读取；缺省时表示不可勾选普通菜单项） |
| `playback[].icon` | `string` | 否 | 保留字段（双后端均不绘制，图标定义请使用 iconSvg） |
| `playback[].iconSvg` | `TrayIconSvg` | 否 | 菜单项前置矢量图标（仅 webview 后端绘制；同一层级内只要有一项配置了图标，同级全部 normal 与 submenu 项均会自动对齐预留图标列宽度） |
| `playback[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `playback[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `playback[].cover` | `string` | 否 | nowplaying 正在播放卡片封面（支持 data: URL、http(s):// 链接或原始 Base64 JPEG；开启 config.autoNowPlaying 时缺省将自动拉取当前曲目前置封面；仅 webview 后端生效） |
| `playback[].title` | `string` | 否 | nowplaying 正在播放卡片主标题（通常为曲目名称；缺省时回退使用 label） |
| `playback[].subtitle` | `string` | 否 | nowplaying 正在播放卡片副标题（通常为艺术家或专辑信息） |
| `playback[].value` | `integer` | 否 | 控件当前数值（rating 支持 0 至 5 星；slider 支持 min 至 max 范围内的数值，超出范围自动夹取；segmented 为当前选中分段索引；仅在此三类控件上生效并回传） |
| `playback[].min` | `integer` | 否 | slider 滑块数值下限（若大于 max 将自动与 max 互换） |
| `playback[].max` | `integer` | 否 | slider 滑块数值上限 |
| `playback[].orientation` | `"horizontal" \| "vertical"` | 否 | slider 滑块布局方向（仅 webview 后端支持；缺省为水平方向，垂直方向时 min 位于底部） |
| `playback[].segments` | `TraySegment[]` | 否 | segmented 分段选项列表（单行互斥选择项，value 为当前选中项索引；点击分段上报 { id, value } 且菜单保持展开） |
| `playback[].segments[].label` | `string` | 否 | 分段文本标签（未提供图标时展示） |
| `playback[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段矢量图标（优先级高于文本标签） |
| `playback[].segments[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `playback[].segments[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `playback[].segments[].enabled` | `boolean` | 否 | 分段是否可用（为 false 时置灰且不可点击）默认 `true`。 |
| `playback[].submenu` | `TrayMenuItem[]` | 否 | submenu 子菜单包含的子菜单项列表 |
| `playback[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 声明由宿主原生直接执行的播放命令（支持在主页面挂起、最小化或锁屏状态下稳定响应；配置后点击该项不触发 tray:menuItemClicked；仅允许配置在 normal 叶子节点上） |
| `bottom` | `TrayMenuItem[]` | 否 | bottom 分区菜单项列表（省略时清空该分区；showSystemItems 注入的原生项独立维护，不受此清空影响） |
| `bottom[].id` | `string` | 否 | 菜单项唯一标识符（由 tray:menuItemClicked 事件上报；分隔线可省略；预留的 _sys_show 与 _sys_exit 将绑定原生显示主窗口与退出行为并抑制默认系统项注入） |
| `bottom[].label` | `string` | 否 | 菜单项显示文本 |
| `bottom[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 菜单项展现类型（checkbox 为可勾选 normal 项的历史兼容写法，建议统一通过 checked 属性控制勾选状态）默认 `"normal"`。 |
| `bottom[].enabled` | `boolean` | 否 | 菜单项是否处于启用可交互状态默认 `true`。 |
| `bottom[].visible` | `boolean` | 否 | 菜单项是否可见默认 `true`。 |
| `bottom[].checked` | `boolean` | 否 | 菜单项勾选状态（传入该属性包括 false 将使菜单项转为可勾选模式，webview 后端映射为 menuitemcheckbox 并支持通过 getMenuItems 读取；缺省时表示不可勾选普通菜单项） |
| `bottom[].icon` | `string` | 否 | 保留字段（双后端均不绘制，图标定义请使用 iconSvg） |
| `bottom[].iconSvg` | `TrayIconSvg` | 否 | 菜单项前置矢量图标（仅 webview 后端绘制；同一层级内只要有一项配置了图标，同级全部 normal 与 submenu 项均会自动对齐预留图标列宽度） |
| `bottom[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `bottom[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `bottom[].cover` | `string` | 否 | nowplaying 正在播放卡片封面（支持 data: URL、http(s):// 链接或原始 Base64 JPEG；开启 config.autoNowPlaying 时缺省将自动拉取当前曲目前置封面；仅 webview 后端生效） |
| `bottom[].title` | `string` | 否 | nowplaying 正在播放卡片主标题（通常为曲目名称；缺省时回退使用 label） |
| `bottom[].subtitle` | `string` | 否 | nowplaying 正在播放卡片副标题（通常为艺术家或专辑信息） |
| `bottom[].value` | `integer` | 否 | 控件当前数值（rating 支持 0 至 5 星；slider 支持 min 至 max 范围内的数值，超出范围自动夹取；segmented 为当前选中分段索引；仅在此三类控件上生效并回传） |
| `bottom[].min` | `integer` | 否 | slider 滑块数值下限（若大于 max 将自动与 max 互换） |
| `bottom[].max` | `integer` | 否 | slider 滑块数值上限 |
| `bottom[].orientation` | `"horizontal" \| "vertical"` | 否 | slider 滑块布局方向（仅 webview 后端支持；缺省为水平方向，垂直方向时 min 位于底部） |
| `bottom[].segments` | `TraySegment[]` | 否 | segmented 分段选项列表（单行互斥选择项，value 为当前选中项索引；点击分段上报 { id, value } 且菜单保持展开） |
| `bottom[].segments[].label` | `string` | 否 | 分段文本标签（未提供图标时展示） |
| `bottom[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段矢量图标（优先级高于文本标签） |
| `bottom[].segments[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `bottom[].segments[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `bottom[].segments[].enabled` | `boolean` | 否 | 分段是否可用（为 false 时置灰且不可点击）默认 `true`。 |
| `bottom[].submenu` | `TrayMenuItem[]` | 否 | submenu 子菜单包含的子菜单项列表 |
| `bottom[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 声明由宿主原生直接执行的播放命令（支持在主页面挂起、最小化或锁屏状态下稳定响应；配置后点击该项不触发 tray:menuItemClicked；仅允许配置在 normal 叶子节点上） |
| `config` | `TrayMenuConfig` | 否 | 待更新的托盘菜单全局配置选项（传入的 customPosition 将保存供后续 setContextMenu 调用使用） |
| `config.showPlaybackControls` | `boolean` | 否 | 是否在 playback 分区自动注入上一首、播放/暂停、下一首与停止等原生播放控制项（缺省为 true） |
| `config.showSystemItems` | `boolean` | 否 | 是否在 bottom 分区自动注入显示主窗口与退出等原生系统控制项（支持在页面深度挂起状态下可靠响应；缺省为 true） |
| `config.customPosition` | `"top" \| "playback" \| "bottom"` | 否 | setContextMenu 默认写入的目标菜单分区（缺省为 top） |
| `config.render` | `"native" \| "webview"` | 否 | 菜单渲染后端（native 为 Win32 原生菜单；webview 为基于 WebView2 的自绘现代浮层，支持富控件与自定义 CSS 样式；缺省为 native） |
| `config.autoNowPlaying` | `boolean` | 否 | 展开菜单时是否自动使用当前播放曲目填充 nowplaying 卡片中留空的 cover、title 与 subtitle 字段（调用方显式指定的字段优先；封面图像自动下采样至 64 像素且限制最大 256 KiB；仅 webview 后端生效；缺省为 false） |
| `config.css` | `string` | 否 | 注入 webview 自绘菜单的自定义 CSS 样式代码（叠加在内置样式之上，可通过 .fb-menu、.fb-item、.fb-sep 等稳定类名定制视觉样式） |
| `config.cssReplace` | `boolean` | 否 | 是否完全停用内置 CSS 样式（仅保留受保护的 DOM 结构层与用户自定义 css 规则；仅 webview 后端生效；缺省为 false） |
| `config.backdrop` | `"acrylic" \| "mica" \| "mica-alt" \| "none"` | 否 | webview 自绘菜单的 Windows DWM 材质背景效果（acrylic 亚克力、mica、mica-alt 或 none；材质紧贴窗口即时切换，不支持 CSS 透明度渐变过渡；缺省为 acrylic） |
| `config.backdropDarkMode` | `boolean` | 否 | DWM 背景材质是否强制启用深色调（仅 webview 后端生效；设为 false 时跟随浅色视觉风格；缺省为 true） |
| `config.closeAnimationMs` | `integer` | 否 | webview 自绘菜单在用户主动关闭时的退场过渡动画时长（单位毫秒，取值范围 0 至 1000；传入 0 表示立即关闭窗口；可配合 #menu.out CSS 选择器定制动画；缺省为 0） |
| `config.layoutMode` | `"flat" \| "zones"` | 否 | webview 自绘菜单的 DOM 容器布局模式（flat 表示所有菜单项平铺于根容器下；zones 表示将非空分区分别包裹于 .fb-zone[data-zone] 容器内；缺省为 flat） |

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
配置主窗口最小化时是否自动隐藏至系统托盘（未通过 create 创建托盘图标时维持系统默认最小化行为；tray.create 会根据首选项重置该状态，故须在 create 之后调用）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | 是否启用最小化时隐藏至系统托盘 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.setCloseToTray

<!-- api-schema:begin tray.setCloseToTray -->
配置主窗口关闭时是否拦截退出并隐藏至系统托盘（未通过 create 创建托盘图标时将降级为最小化窗口以防失去窗口句柄；tray.create 会根据首选项重置该状态，故须在 create 之后调用）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | 是否启用关闭窗口时隐藏至系统托盘 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.isVisible

<!-- api-schema:begin tray.isVisible -->
查询当前系统托盘图标是否存在并处于激活状态

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `visible` | `boolean` | 托盘图标当前是否存在且处于显示状态 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.appendMenuItems

<!-- api-schema:begin tray.appendMenuItems -->
向指定菜单分区追加菜单项（参数校验规则与 setContextMenu 保持一致）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `TrayMenuItem[]` | 是 | 待追加的菜单项列表 |
| `items[].id` | `string` | 否 | 菜单项唯一标识符（由 tray:menuItemClicked 事件上报；分隔线可省略；预留的 _sys_show 与 _sys_exit 将绑定原生显示主窗口与退出行为并抑制默认系统项注入） |
| `items[].label` | `string` | 否 | 菜单项显示文本 |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 否 | 菜单项展现类型（checkbox 为可勾选 normal 项的历史兼容写法，建议统一通过 checked 属性控制勾选状态）默认 `"normal"`。 |
| `items[].enabled` | `boolean` | 否 | 菜单项是否处于启用可交互状态默认 `true`。 |
| `items[].visible` | `boolean` | 否 | 菜单项是否可见默认 `true`。 |
| `items[].checked` | `boolean` | 否 | 菜单项勾选状态（传入该属性包括 false 将使菜单项转为可勾选模式，webview 后端映射为 menuitemcheckbox 并支持通过 getMenuItems 读取；缺省时表示不可勾选普通菜单项） |
| `items[].icon` | `string` | 否 | 保留字段（双后端均不绘制，图标定义请使用 iconSvg） |
| `items[].iconSvg` | `TrayIconSvg` | 否 | 菜单项前置矢量图标（仅 webview 后端绘制；同一层级内只要有一项配置了图标，同级全部 normal 与 submenu 项均会自动对齐预留图标列宽度） |
| `items[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `items[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `items[].cover` | `string` | 否 | nowplaying 正在播放卡片封面（支持 data: URL、http(s):// 链接或原始 Base64 JPEG；开启 config.autoNowPlaying 时缺省将自动拉取当前曲目前置封面；仅 webview 后端生效） |
| `items[].title` | `string` | 否 | nowplaying 正在播放卡片主标题（通常为曲目名称；缺省时回退使用 label） |
| `items[].subtitle` | `string` | 否 | nowplaying 正在播放卡片副标题（通常为艺术家或专辑信息） |
| `items[].value` | `integer` | 否 | 控件当前数值（rating 支持 0 至 5 星；slider 支持 min 至 max 范围内的数值，超出范围自动夹取；segmented 为当前选中分段索引；仅在此三类控件上生效并回传） |
| `items[].min` | `integer` | 否 | slider 滑块数值下限（若大于 max 将自动与 max 互换） |
| `items[].max` | `integer` | 否 | slider 滑块数值上限 |
| `items[].orientation` | `"horizontal" \| "vertical"` | 否 | slider 滑块布局方向（仅 webview 后端支持；缺省为水平方向，垂直方向时 min 位于底部） |
| `items[].segments` | `TraySegment[]` | 否 | segmented 分段选项列表（单行互斥选择项，value 为当前选中项索引；点击分段上报 { id, value } 且菜单保持展开） |
| `items[].segments[].label` | `string` | 否 | 分段文本标签（未提供图标时展示） |
| `items[].segments[].iconSvg` | `TrayIconSvg` | 否 | 分段矢量图标（优先级高于文本标签） |
| `items[].segments[].iconSvg.viewBox` | `string` | 是 | SVG viewBox 视口属性 |
| `items[].segments[].iconSvg.content` | `string` | 是 | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `items[].segments[].enabled` | `boolean` | 否 | 分段是否可用（为 false 时置灰且不可点击）默认 `true`。 |
| `items[].submenu` | `TrayMenuItem[]` | 否 | submenu 子菜单包含的子菜单项列表 |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 否 | 声明由宿主原生直接执行的播放命令（支持在主页面挂起、最小化或锁屏状态下稳定响应；配置后点击该项不触发 tray:menuItemClicked；仅允许配置在 normal 叶子节点上） |
| `position` | `"top" \| "playback" \| "bottom"` | 否 | 目标追加分区默认 `"top"`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.removeMenuItems

<!-- api-schema:begin tray.removeMenuItems -->
按菜单项唯一标识符 id 从所有分区（递归包含子菜单）中移除匹配的菜单项

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `ids` | `string[]` | 是 | 待移除菜单项的标识符 id 列表 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `removed` | `integer` | 实际成功移除的菜单项总数（若部分 id 未匹配则小于传入 id 数量） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.clearMenuItems

<!-- api-schema:begin tray.clearMenuItems -->
清空指定菜单分区的自定义菜单项（省略 position 时同时清空全部三个分区）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `position` | `"top" \| "playback" \| "bottom"` | 否 | 待清空的目标菜单分区（省略时清空全部三个分区） |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

---

### tray.getMenuItems

<!-- api-schema:begin tray.getMenuItems -->
平铺获取全部存储的自定义菜单项列表（按 top、playback、bottom 分区顺序排列；运行时自动注入的 showPlaybackControls 与 showSystemItems 菜单项不包含在内）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `TrayMenuItem[]` | 当前存储的全部自定义菜单项列表（按 top、playback、bottom 分区顺序平铺展示） |
| `items[].id` | `string` | 菜单项唯一标识符（由 tray:menuItemClicked 事件上报；分隔线可省略；预留的 _sys_show 与 _sys_exit 将绑定原生显示主窗口与退出行为并抑制默认系统项注入） |
| `items[].label` | `string` | 菜单项显示文本 |
| `items[].type` | `"normal" \| "separator" \| "checkbox" \| "submenu" \| "nowplaying" \| "rating" \| "slider" \| "segmented"` | 菜单项展现类型（checkbox 为可勾选 normal 项的历史兼容写法，建议统一通过 checked 属性控制勾选状态） |
| `items[].enabled` | `boolean` | 菜单项是否处于启用可交互状态 |
| `items[].visible` | `boolean` | 菜单项是否可见 |
| `items[].checked` | `boolean` | 菜单项勾选状态（传入该属性包括 false 将使菜单项转为可勾选模式，webview 后端映射为 menuitemcheckbox 并支持通过 getMenuItems 读取；缺省时表示不可勾选普通菜单项） |
| `items[].icon` | `string` | 保留字段（双后端均不绘制，图标定义请使用 iconSvg） |
| `items[].iconSvg` | `TrayIconSvg` | 菜单项前置矢量图标（仅 webview 后端绘制；同一层级内只要有一项配置了图标，同级全部 normal 与 submenu 项均会自动对齐预留图标列宽度） |
| `items[].iconSvg.viewBox` | `string` | SVG viewBox 视口属性 |
| `items[].iconSvg.content` | `string` | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `items[].cover` | `string` | nowplaying 正在播放卡片封面（支持 data: URL、http(s):// 链接或原始 Base64 JPEG；开启 config.autoNowPlaying 时缺省将自动拉取当前曲目前置封面；仅 webview 后端生效） |
| `items[].title` | `string` | nowplaying 正在播放卡片主标题（通常为曲目名称；缺省时回退使用 label） |
| `items[].subtitle` | `string` | nowplaying 正在播放卡片副标题（通常为艺术家或专辑信息） |
| `items[].value` | `integer` | 控件当前数值（rating 支持 0 至 5 星；slider 支持 min 至 max 范围内的数值，超出范围自动夹取；segmented 为当前选中分段索引；仅在此三类控件上生效并回传） |
| `items[].min` | `integer` | slider 滑块数值下限（若大于 max 将自动与 max 互换） |
| `items[].max` | `integer` | slider 滑块数值上限 |
| `items[].orientation` | `"horizontal" \| "vertical"` | slider 滑块布局方向（仅 webview 后端支持；缺省为水平方向，垂直方向时 min 位于底部） |
| `items[].segments` | `TraySegment[]` | segmented 分段选项列表（单行互斥选择项，value 为当前选中项索引；点击分段上报 { id, value } 且菜单保持展开） |
| `items[].segments[].label` | `string` | 分段文本标签（未提供图标时展示） |
| `items[].segments[].iconSvg` | `TrayIconSvg` | 分段矢量图标（优先级高于文本标签） |
| `items[].segments[].iconSvg.viewBox` | `string` | SVG viewBox 视口属性 |
| `items[].segments[].iconSvg.content` | `string` | SVG 内部矢量图形标记（例如 <path d="..."/>） |
| `items[].segments[].enabled` | `boolean` | 分段是否可用（为 false 时置灰且不可点击） |
| `items[].submenu` | `TrayMenuItem[]` | submenu 子菜单包含的子菜单项列表 |
| `items[].playbackAction` | `"play-pause" \| "previous" \| "next" \| "stop"` | 声明由宿主原生直接执行的播放命令（支持在主页面挂起、最小化或锁屏状态下稳定响应；配置后点击该项不触发 tray:menuItemClicked；仅允许配置在 normal 叶子节点上） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 仅返回用户通过 `setContextMenu` / `appendMenuItems` 添加的项；由 `showPlaybackControls` / `showSystemItems` 自动注入的内置项不在返回值内。内置项使用 `_pb_` 与 `_sys_` 前缀的保留命名空间，用户自定义菜单项应避免使用这些前缀。

---

### tray.setMenuItemState

<!-- api-schema:begin tray.setMenuItemState -->
原地更新指定菜单项的 checked 或 enabled 状态（跨全部三个分区递归检索子菜单匹配 id；传入 checked 参数（含 false）将自动使该项具备可勾选属性；原生菜单在每次弹出时动态重建，状态变更于下一次展开菜单时生效）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `string` | 是 | 待修改菜单项的唯一标识符 id不能为空。 |
| `checked` | `boolean` | 否 | 新的勾选状态（传入布尔值时自动确保菜单项支持勾选交互） |
| `enabled` | `boolean` | 否 | 新的启用状态 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 是否成功定位并更新了指定 id 的菜单项 |

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
| 执行 | 组装阶段翻译为匹配的内置命令，由插件原生直接执行 |
| 后台可靠 | 窗口最小化、关闭到托盘或会话锁屏（主页面深度挂起）时仍可靠执行 |
| 事件 | 声明项**不发** `tray:menuItemClicked`，按钮状态从 `playback:*` 事件反映 |
| 外观 | 调用方完整控制 `label`、`icon` 与 `id`，仅底层路由改变 |
| 校验 | 严格校验：未知 token，或写在 separator、submenu、富控件上时，整次调用返回 `INVALID_PARAMS` |
| `'exit'` / `'show-main-window'` | 不接受：系统动作仍走精确保留标识符 `_sys_exit` 与 `_sys_show` |
| 作用域 | 仅适用于系统托盘菜单，对通用弹出菜单 `menu.show` 无效 |
| Round-trip | `getMenuItems()` 完整回传已声明的 `playbackAction` |

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

## tray.setClickAction

<!-- api-schema:begin tray.setClickAction -->
配置系统托盘图标进程级左键点击行为（支持在 create 前预先设置；默认 event 模式派发常规点击事件；toggleMainWindow 模式由宿主原生切换独立主窗口的显隐与恢复聚焦状态，无需唤醒页面脚本亦不派发点击事件；双击仅触发单次状态切换；调用时自动取消挂起的待处理点击，以最后一次成功设置的值为准；Shell 重建时保留此配置，destroy 时重置为 event；面板模式下调用返回 PANEL_MODE_UNSUPPORTED；启用原生切换前应移除前端已注册的窗口控制监听器，系统可能因焦点策略拒绝前台激活）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `action` | `"event" \| "toggleMainWindow"` | 是 | 左键点击行为模式（event 向页面派发点击事件；toggleMainWindow 触发原生窗口显隐切换） |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

旧主题默认仍使用点击事件。新主题应在初始化时移除旧点击回调中的窗口命令，然后选择原生模式；两个托盘开关的设置不受影响。
