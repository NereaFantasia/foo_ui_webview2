# fb.menu 菜单

`fb.menu` 查询并执行主菜单与右键菜单命令，也提供由 WebView 绘制的弹出菜单。

## 常用命令 GUID {#standard-command-guids}

识别宿主命令时使用命令标识；翻译后的名称和菜单位置不能当标识。下表取自 foobar2000 SDK 的 `standard_commands`，声明与值分别位于 `menu_helpers.h` 和 `guids.cpp`。这些值标识命令，不保证每种宿主配置都会提供或启用它们。

| 命令 | SDK 标识符 | GUID | 执行入口 |
| --- | --- | --- | --- |
| 曲目属性 | `guid_context_file_properties` | `{6F441057-1D18-4A58-9AC4-8F409CDA7DFD}` | `runContextCommand` |
| 打开所在目录 | `guid_context_file_open_directory` | `{EFC1E9C8-EEEF-427A-8F42-E5781605846D}` | `runContextCommand` |
| 复制名称 | `guid_context_copy_names` | `{FFE18008-BCA2-4B29-AB88-8816B492C434}` | `runContextCommand` |
| 发送到播放列表 | `guid_context_send_to_playlist` | `{44B8F02B-5408-4361-8240-18DEC881B95E}` | `runContextCommand` |
| 重读文件信息 | `guid_context_reload_info` | `{8C3BA2CB-BC4D-4752-8282-C6F9AED75A78}` | `runContextCommand` |
| 文件变更时重读信息 | `guid_context_reload_info_if_changed` | `{BD045EA4-E5E9-4206-8FF9-12AD9F5DCDE1}` | `runContextCommand` |
| 偏好设置 | `guid_main_preferences` | `{11213A01-9F36-4E69-A1BB-7A72F418DE3A}` | `runMainMenuCommand` |
| 关于 | `guid_main_about` | `{EDA23441-5D38-4499-A22C-FE0CE0A987D9}` | `runMainMenuCommand` |

“发送到播放列表”的 GUID 标识 SDK 中的这条命令，不标识某张目标播放列表，也不代表其他组件的动态子项；目标选择方式由命令自身决定。

匹配 GUID 时还要区分主菜单与右键菜单，GUID 文本比较不区分字母大小写。动态叶子必须同时匹配 `guid` 和 `subGuid`；只认父 GUID，无法确定评分档位、转换预设或目标。自绘菜单应保留枚举结果中的 `enabled` 和 `hidden` 状态。`executable: false` / `noStableIdentifier` 表示缺少 GUID 地址，不表示位置编号 `commandId` 已经稳定，也不表示所有执行途径都不可用。

本表没有给播放队列命令或第三方评分档位指定通用地址。调用这些能力时，优先使用专用的 [queue](queue.md) 和 [rating](rating.md) API。保留宿主菜单项时，不按译名、子项位置或相同父 GUID 猜测具体含义。只有命令身份、目标、参数和副作用一致，自定义控件才能视为接管了对应菜单动作。

### 操作目标与完成结果 {#command-targets}

- `runContextCommand` 在调用时确定目标：优先正在播放的曲目，没有播放时才使用活动播放列表的选中项。它不接受 `handles` 或 `mode`。先为指定句柄构建菜单，不会把后续 GUID 调用绑定到那些句柄。
- `runContextCommandById` 接受目标模式和句柄，但执行时会重建菜单。即使参数没变，动态菜单仍可能变化，同一个位置可能执行另一条命令。重新读取菜单只能缩短间隔，不能提供原子的身份校验。
- 需要由宿主为指定曲目显示原生菜单时，可用 `showNativePopup({ mode: 'handles', handles })`。当前 API 尚不能在一次调用中同时按稳定地址与显式目标执行自绘菜单的选择。
- 命令调用成功，即使 `executionConfirmed: true`，也不代表对话框或第三方异步操作已经最终完成。它不能说明用户是否取消了对话框，也不报告哪些对象最终发生了变化。超时同样不能证明操作没有执行，因此不要自动重放可能写入或删除数据的命令。

## getContextMenu(options?)

签名：`fb.menu.getContextMenu(options?: MenuGetContextMenuParams): Promise<MenuGetContextMenuResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `options.mode` | `string` | 否 | 上下文模式：`'auto'`、`'selection'`、`'playlist'`、`'nowPlaying'` 或 `'handles'`。 |
| `options.handles` | `JsonValue[]` | 否 | `'handles'` 与 `'auto'` 用的曲目：路径（可带 `\|subsong:N` 后缀）或 `{ path, subsong }` 对象。 |
| `options.locale` | `string` | 否 | 语言选择，默认 `'auto'`。 |
| `options.i18n` | `boolean` | 否 | 启用标签本地化。 |
| `options.withAvailability` | `boolean` | 否 | 附带可用性信息。 |

返回 `MenuGetContextMenuResponse`，含递归的 `items` 菜单树与上下文信息。命令叶节点带 `subGuid` 时 `source` 为 `contextmenu_dynamic`，否则为 `contextmenu_static`。

```javascript
const result = await fb.menu.getContextMenu({ mode: 'nowPlaying' });
```

`selection` 表示活动播放列表中的选中曲目；`playlist` 是播放列表级上下文，可能只包含针对整个播放列表的命令。

```javascript
const result = await fb.menu.getContextMenu({ mode: 'selection' });
```

## getMainMenu(root?, options?)

签名：`fb.menu.getMainMenu(root?: string, options?: Omit<MenuGetMainMenuParams, 'root'>): Promise<MenuGetMainMenuResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `root` | `string` | 否 | 限定返回的子树，例如 `'Main'` 或 `'View'`。 |
| `options.locale` | `string` | 否 | 语言选择，默认 `'auto'`。 |
| `options.i18n` | `boolean` | 否 | 启用标签本地化。 |
| `options.withAvailability` | `boolean` | 否 | 附带可用性信息。 |

返回 `MenuGetMainMenuResponse`，其 `items` 是递归的 `MenuItem[]` 树。命令叶节点的 `source` 由自身地址决定：带 `subGuid` 为 `mainmenu_dynamic`，不带为 `mainmenu_static`；响应带 `source: 'v1-hmenu'` 时所有叶节点为 `hmenu_fallback`。扁平回退（`fallback: 'flat-mainmenu-commands'`，每项带 `fallback: true`）保留 `flags`，没有 `commandId`。

```javascript
const result = await fb.menu.getMainMenu('View');
```

## runContextCommand(command, options?)

签名：`fb.menu.runContextCommand(command: string, options?: Omit<MenuRunContextCommandParams, 'command'>): Promise<MenuRunContextCommandResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 右键菜单命令路径、名称或 GUID |
| `options.subGuid` | `string` | 否 | 动态生成子项的节点 GUID。不传则命中其父容器，等于什么都不执行 |

封装 `menu.runContextCommand`。命令作用于正在播放的曲目，没有播放时作用于活动播放列表的选中项；两者都没有时以 `NO_ACTIVE_ITEM` 失败。名称没匹配到命令时以 `MENU_COMMAND_NOT_FOUND` 失败，没有命令拥有该 GUID 时以 `NOT_FOUND` 失败。

返回值可能含 `guid`、`itemCount` 与 `executionConfirmed`——后者为 `false` 表示命令走的是不返回结果的入口，无法观测是否真的执行。

```javascript
// 打开正在播放的曲目属性；停止播放时使用活动播放列表选区。
const result = await fb.menu.runContextCommand('{6F441057-1D18-4A58-9AC4-8F409CDA7DFD}');

// 仅传入用户从 getContextMenu().items 中选中的动态叶子。
async function runChosenDynamicContextCommand(node) {
    if (node.type !== 'command' || !node.guid || !node.subGuid) return;
    return fb.menu.runContextCommand(node.guid, { subGuid: node.subGuid });
}
```

## runContextCommandById(id, options?)

签名：`fb.menu.runContextCommandById(id: number, options?: Omit<MenuRunContextCommandByIdParams, 'id'>): Promise<MenuRunContextCommandByIdResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `number` | 是 | 右键菜单节点的 `commandId`，取自 `getContextMenu()` 的结果。 |
| `options.mode` | `string` | 否 | 构建菜单时用的 `mode`，默认 `'auto'`。 |
| `options.handles` | `JsonValue[]` | 否 | 构建菜单时用的 `handles`。 |

运行 `getContextMenu()` 报告的 `commandId` 对应的右键菜单项。宿主按 `options.mode` 与 `options.handles` 重建菜单，所以要传那次调用用的值。id 只对产生它的那份菜单有效，不要保存；重建的菜单里没有该 id 时以 `NOT_FOUND` 失败。

菜单变化后，同一个 id 也可能对应另一条命令，执行时并不报错。用于破坏性操作前，请先阅读[操作目标与完成结果](#command-targets)。

```javascript
const opts = { mode: 'selection' };
const menu = await fb.menu.getContextMenu(opts);
if (menu.success === false) throw new Error(menu.error);
const node = menu.items.find((item) => item.type === 'command');
if (node && node.type === 'command' && node.commandId !== undefined) {
    const res = await fb.menu.runContextCommandById(node.commandId, opts);
    if (res.success === false) console.warn(res.code, res.error);
}
```

## runMainMenuCommand(command, options?)

签名：`fb.menu.runMainMenuCommand(command: string, options?: Omit<MenuRunMainMenuCommandParams, 'command'>): Promise<MenuRunMainMenuCommandResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 命令 GUID、叶子命令名或斜杠分隔的路径 |
| `options.subGuid` | `string` | 否 | 动态子命令的子 GUID |

封装 `menu.runMainMenuCommand`。成功时响应带 `guid`，即实际运行的命令的 GUID；菜单树按名称或路径运行命令时没有这个字段。

**推荐用 GUID 形式**：它是唯一跨宿主稳定的寻址方式。汉化版 foobar2000 上报的是中文命令名，
英文名或英文路径在该宿主上解析不到。GUID 可从 `discovery.getMainMenuCommands()` 或
`menu.getMainMenu()` 叶子节点的 `guid` 字段取得。

失败以 `success: false` 加 `code` 返回：`MENU_ITEM_DISABLED`、
`MENU_MATCH_AMBIGUOUS`（详见 `candidates`）、`MENU_COMMAND_NOT_FOUND`，没有命令拥有该 GUID 时为 `NOT_FOUND`。

```javascript
// 偏好设置：SDK 定义的 GUID 不受宿主语言影响。
const result = await fb.menu.runMainMenuCommand(
    '{11213A01-9F36-4E69-A1BB-7A72F418DE3A}',
);

// 仅传入用户从 getMainMenu().items 中选中的动态叶子。
async function runChosenDynamicMainCommand(node) {
    if (node.type !== 'command' || !node.guid || !node.subGuid) return;
    return fb.menu.runMainMenuCommand(node.guid, { subGuid: node.subGuid });
}
```

## showNativePopup(options?)

签名：`fb.menu.showNativePopup(options?: MenuShowNativePopupParams): Promise<MenuShowNativePopupResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `options.mode` | `string` | 否 | 上下文模式，默认 `'auto'`：依次尝试 handles、当前播放曲目、活动播放列表选中项，最后是播放列表级上下文。 |
| `options.handles` | `JsonValue[]` | 否 | `'handles'` 与 `'auto'` 用的曲目，格式同 `getContextMenu()`。 |
| `options.x`、`options.y` | `number` | 否 | 为兼容而接受，没有作用：菜单总在鼠标指针处打开。 |

宿主在调用返回后随即在鼠标指针处弹出原生菜单。

```javascript
const result = await fb.menu.showNativePopup({ mode: 'selection' });
```

## 自绘弹出菜单（Self-drawn popup menu）

`menu.show` / `menu.close` / `menu.popup` 用 WebView 渲染上下文菜单，替代原生 Win32 `TrackPopupMenu`，支持子菜单、键盘导航、内联富控件，以及通过 `options.css` 完全接管样式。

> **与 tray zones 的边界**：public `menu.show` **始终**使用 legacy 直接子 DOM（`#menu > .fb-item`），**不会**出现 `.fb-zone` 容器。分区布局（`layoutMode: 'zones'`）仅属于 `tray.setContextMenu` + `render: 'webview'` 的 opt-in 能力。

### fb.menu.popup(items, position?, options?)

推荐入口：弹出自绘菜单并等待用户选择。选中返回所选项的 `id`，被取消（外点击 / Esc / 其他原因）时返回 `null`。事件按菜单 id 匹配，多个调用不会互相串扰。

```javascript
const id = await fb.menu.popup(
  [
    { id: 'play', label: '播放' },
    { id: 'queue', label: '加入队列', checked: true },
    { type: 'separator' },
    { id: 'more', label: '更多', submenu: [
      { id: 'props', label: '属性' },
      { id: 'del', label: '删除', enabled: false },
    ] },
  ],
  { x: 200, y: 150 }, // 省略 position 用光标位置
);
if (id) console.log('selected', id);
```

第三个参数是[展示配置](#presentation-options)；右键菜单推荐传 `windowModel: 'contentSized'`。

富控件的值变更走 `menu:valueChanged` 且**不关闭菜单**，因此该 Promise 会一直挂起，直到选中普通项或菜单被取消。菜单里含 rating / slider / segmented 行时，请另行订阅该事件。

### fb.menu.show(items, position?, options?)

底层方法：仅弹出菜单并返回 `{ success, menuId }`；用户选择通过 `menu:select` / `menu:dismiss` 事件回传。

```javascript
const res = await fb.menu.show([{ id: 'a', label: 'A' }]);
if (res.success === false) throw new Error(res.error);
const { menuId } = res;
fb.on('menu:select', (e) => { if (e.menuId === menuId) console.log(e.itemId); });
fb.on('menu:dismiss', (e) => { if (e.menuId === menuId) console.log('dismissed', e.reason); });
```

#### 调用方 / 安全 / 资源边界

- **公开入口**：`menu.show` / `menu.close` / `menu.popup` 可由主题页面调用。内部 `menu.__select` / `menu.__valueChanged` / `menu.__dismiss` / `menu.__ready` / `menu.__getMenuState` 仅 overlay 自身窗口可调；越权 IPC（非 overlay caller 或 `menuId` 不匹配）返回 `INVALID_PARAMS` 且不改变菜单状态。
- **资源上限**（打开 overlay 前事务性校验；超限时整次调用的 `success` 为 `false`，`details` 含 `field` / `limit` / `actual`）：
  - 总 item 数 ≤ 512
  - 递归深度 ≤ 8（根为深度 1）
  - 单个 `segmented` 的 option 数 ≤ 64
  - 单次菜单 SVG `content` 总量 ≤ 256 KiB（先丢弃单图 > 32 KiB 的图标，再对剩余求和）
  - `options.css` ≤ 256 KiB（超限按 `css` 字段报 breach）
- **单图 SVG > 32 KiB**：静默丢弃该图标，不 fail 整菜单。
- 本 API 是通用自绘菜单入口，**不是**托盘分区（top / playback / bottom）配置面；托盘分区请用 `tray.setContextMenu` / `tray.appendMenuItems`。

### 展示配置 {#presentation-options}

两个方法都接受可选的第三个参数 `MenuPopupOptions`。所有字段按次生效（每次弹出都重新应用），因此主题可以随自身明暗状态逐次传值；不传的字段保持默认。

| 字段 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `windowModel` | `'fullscreen' \| 'contentSized'` | `'fullscreen'` | `'contentSized'` 把根菜单与一级子菜单分别画在按内容测量的紧凑窗里，每个面板在自己的表面上承载真实 DWM 背景材质并带系统窗口阴影，**推荐用于右键菜单**；`'fullscreen'` 是兼容默认值，即用一个全屏浮层窗口承载菜单 DOM。 |
| `css` | `string` | — | 前端样式接管，最大 256 KiB，注入 overlay 专用样式层，每次弹出都应用。 |
| `cssReplace` | `boolean` | `false` | `true` 把 `css` 从叠加模式切换为替换模式：禁用内置样式，只保留你的 CSS 与受保护结构层，整体外观（含入场动画）完全由你定义。 |
| `backdrop` | `'acrylic' \| 'mica' \| 'mica-alt' \| 'none'` | `'acrylic'` | 菜单窗口的 DWM 系统背景。`'acrylic'` 是瞬态表面材质，对弹出菜单是正确的默认值；`'mica'` / `'mica-alt'` 面向主窗口背景设计，用在瞬态菜单上可能不协调。 |
| `backdropDarkMode` | `boolean` | `true` | 背景材质的暗色调。传 `false` 以跟随浅色主题。 |
| `closeAnimationMs` | `number` | `0` | 退场（淡出）动画时长（毫秒），clamp 到 `0..1000`。`0` 表示立即隐藏。 |

#### 完整示例

```javascript
document.addEventListener('contextmenu', async (e) => {
  e.preventDefault();
  const id = await fb.menu.popup(
    [
      { id: 'play', label: '播放' },
      { id: 'queue', label: '加入队列' },
      { type: 'separator' },
      { id: 'rating', type: 'rating', label: '评分', value: 3 },
      { id: 'volume', type: 'slider', label: '音量', value: 60, min: 0, max: 100 },
      { type: 'separator' },
      { id: 'props', label: '属性' },
    ],
    undefined,
    {
      windowModel: 'contentSized',
      backdrop: 'acrylic',
      css: `
        .fb-menu { background: rgba(32, 32, 32, 0.82); border-radius: 8px; padding: 4px; }
        .fb-item { color: #f2f2f2; border-radius: 4px; }
        .fb-item.active { background: rgba(255, 255, 255, 0.08); }
      `,
    },
  );
  if (id) console.log('selected', id);
});

fb.on('menu:valueChanged', (e) => {
  console.log('value changed', e.itemId, e.value);
});
```

`position` 传 `undefined` 表示锚定到光标位置，这通常正是 `contextmenu` 处理器需要的。

#### 样式指南

overlay 是独立的顶层文档，宿主页面的 `::part()` 选择器够不到它。受支持的样式契约是稳定 class 名：`.fb-menu`、`.fb-item`（带 `.nrm` / `.disabled` / `.active` / `.checked` / `.has-sub`）、`.fb-item-ico`、`.fb-arrow`、`.fb-sep`、now-playing 的 `.fb-np*`、评分的 `.fb-rating*` 与 `.fb-star`、滑块的 `.fb-slider*`、分段控件的 `.fb-seg*`。默认叠加模式下，你的规则靠源码顺序或 `!important` 取胜。一个很小的受保护结构层（`#viewport`、菜单 box-sizing、固定定位、overflow 与隐藏态兜底）始终最后强制应用。

要做半透明菜单，背景 alpha 建议保持在 0.75 到 0.9 之间：既能保住文字对比度，Windows 11 系统菜单本身对「透」也很克制。

动画与材质之间存在取舍，因为 DWM 背景是窗口级效果，只能整窗生效或整窗没有：它随窗口显示 / 隐藏瞬间出现或消失，无法跟 CSS 动画一起淡入淡出。因此 `closeAnimationMs` 只动画 web 内容，开着 `acrylic` / `mica` 时背景会「瞬灭」而内容还在淡出，过场不同步。要全程平滑，请改用 `backdrop: 'none'` 加 `.fb-menu` 的 CSS 半透明背景，代价是 CSS 半透明没有真实模糊。关闭时渲染器把根菜单的 class 从 `#menu.in` 切到 `#menu.out`，内置的 `#menu.out` 规则可经 `css` 覆盖；`replaced`（新菜单顶掉旧菜单）与内部超时路径始终立即隐藏。

#### 平台要求

acrylic 需要 Windows 11 22H2 或更新版本。Windows 10 上背景会降级为系统支持的材质，因此要求各处外观一致的主题应改用 `backdrop: 'none'` 加 CSS 半透明背景。

### fb.menu.close(reason?)

主动关闭当前自绘菜单。

```javascript
await fb.menu.close('api');
```

### 菜单项 MenuPopupItem

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `string` | 选中时通过 `menu:select` 回传 |
| `label` | `string` | 行文本（分隔线可省略） |
| `type` | `'normal' \| 'separator' \| 'nowplaying' \| 'rating' \| 'slider' \| 'segmented'` | `separator` 渲染分隔线；富类型渲染内联控件 |
| `enabled` | `boolean` | 默认 `true`；`false` 灰显不可选 |
| `checked` | `boolean` | 渲染勾选标记 |
| `iconSvg` | `{ viewBox, content }` | 标签前绘制的单色内联 SVG，`content` 为 SVG 内部标记 |
| `cover` | `string` | `'nowplaying'` 封面：data URL、`http(s)` URL，或按 JPEG 解码的裸 base64 |
| `title` | `string` | `'nowplaying'` 主行，缺省回退到 `label` |
| `subtitle` | `string` | `'nowplaying'` 次行 |
| `value` | `number` | 当前值：`'rating'` 为 `0..5` 星，`'slider'` 为 `[min, max]` 内的整数，`'segmented'` 为选中段的从 0 起索引 |
| `min` / `max` | `number` | `'slider'` 范围，默认 `0` 与 `100` |
| `orientation` | `'horizontal' \| 'vertical'` | `'slider'` 轴向，默认水平 |
| `segments` | `{ label?, iconSvg?, enabled? }[]` | `'segmented'` 的各段，渲染为一行互斥单选 |
| `submenu` | `MenuPopupItem[]` | 子菜单，渲染右展开箭头 |

#### 富控件

`'rating'` / `'slider'` / `'segmented'` 是值控件：改变其值会以 `{ menuId, itemId, value }` 经 `menu:valueChanged` 回报，并**保持菜单打开**——索引到业务含义由前端决定，可以在菜单仍显示时就更新 foobar2000。`'nowplaying'` 卡片则属于普通选择：与任意普通行一样经 `menu:select` 回报并关闭菜单。

除点击与方向键外，在值控件行上滚动鼠标滚轮也能调值（向上滚 = 增大）：每个滚轮事件走一步，`'rating'` 一步一星，`'slider'` 一步为量程的二十分之一，`'segmented'` 一步跳到下一个启用段。该行必须处于启用状态，`min` 与 `max` 相等的滑块则永远不动。当**另一行**正处于编辑态时，滚轮不生效，改为滚动菜单。

`iconSvg` 由 `DOMParser` 解析，只把白名单内的图形元素与属性克隆进实时文档，因此不存在裸标记注入。非法或超限图标被静默丢弃，该行照常绘制、只是没有图标。

### 事件

| 事件 | payload | 时机 |
| --- | --- | --- |
| `menu:select` | `{ menuId, itemId }` | 选中某项后触发，随后自动关闭 |
| `menu:valueChanged` | `{ menuId, itemId, value }` | rating / slider / segmented 值变更，菜单保持打开 |
| `menu:dismiss` | `{ menuId, reason }` | 关闭（reason：outside / escape / select / api / timeout / blur） |

这些事件发给调用 `menu.show` 的页面，所以在 popup 或面板页面里调用的 `fb.menu.popup()` 与在主窗口里一样能 resolve。菜单回报时该页面已不在的，发给同一顶层窗口下的页面，再没有就发给主窗口的页面。菜单被别的页面的 `menu.show` 或 `menu.close` 关掉时，`menu:dismiss` 仍发给打开它的页面。

### 既有菜单方法（主菜单 / 上下文菜单查询与执行）

```javascript
await fb.menu.getMainMenu('View');
await fb.menu.getContextMenu({ mode: 'auto' });
await fb.menu.runMainMenuCommand('View/Console');
await fb.menu.runContextCommand('Properties');
await fb.menu.runContextCommandById(3, { mode: 'selection' });
await fb.menu.showNativePopup({ mode: 'selection' });
```

`runContextCommandById(id, options?)` 运行 `getContextMenu` 报告的 `commandId` 对应的右键菜单项，参数与失败码见 [runContextCommandById()](#runcontextcommandbyid-id-options)。
