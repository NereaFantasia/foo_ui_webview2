# fb.menu menu

本页是 `fb.menu` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### getContextMenu()

签名：`fb.menu.getContextMenu(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `menu.getContextMenu` 调用结果。

```javascript
const result = await fb.menu.getContextMenu();
```

### getMainMenu()

签名：`fb.menu.getMainMenu(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `menu.getMainMenu` 调用结果。

```javascript
const result = await fb.menu.getMainMenu();
```

### runContextCommand()

签名：`fb.menu.runContextCommand(command: string, options?: Omit<MenuRunContextCommandParams, 'command'>): Promise<MenuRunContextCommandResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 右键菜单命令路径、名称或 GUID |
| `options.subGuid` | `string` | 否 | 动态生成子项的节点 GUID。不传则命中其父容器，等于什么都不执行 |

返回值：底层 `menu.runContextCommand` 调用结果，可能含 `guid`、`itemCount` 与 `executionConfirmed`——后者为 `false` 表示命令走的是不返回结果的入口，无法观测是否真的执行。

```javascript
const result = await fb.menu.runContextCommand('Properties');
```

### runMainMenuCommand()

签名：`fb.menu.runMainMenuCommand(command: string, options?: Omit<MenuRunMainMenuCommandParams, 'command'>): Promise<MenuRunMainMenuCommandResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `command` | `string` | 是 | 命令 GUID、叶子命令名或斜杠分隔的路径 |
| `options.subGuid` | `string` | 否 | 动态子命令的子 GUID |

返回值：底层 `menu.runMainMenuCommand` 调用结果，可能带上解析出的 `guid`。

**推荐用 GUID 形式**：它是唯一跨宿主稳定的寻址方式。汉化版 foobar2000 上报的是中文命令名，
英文名或英文路径在该宿主上解析不到。GUID 可从 `discovery.getMainMenuCommands()` 或
`menu.getMainMenu()` 叶子节点的 `guid` 字段取得。

失败以 `success: false` 加 `code` 返回：`MENU_ITEM_DISABLED`、
`MENU_MATCH_AMBIGUOUS`（详见 `candidates`）、`MENU_COMMAND_NOT_FOUND`。

```javascript
// 推荐：按 GUID 寻址
const result = await fb.menu.runMainMenuCommand(
    '{11213A01-9F36-4E69-A1BB-7A72F418DE3A}',
);

// 动态子命令需要「所属命令 GUID + subGuid」
await fb.menu.runMainMenuCommand('{41D98AF1-8C4F-4F0E-8B7A-1A4B0F7B1234}', {
    subGuid: '{A222D5A9-2903-AA8C-EEAE-4B9230558B55}',
});
```

### showNativePopup()

签名：`fb.menu.showNativePopup(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `menu.showNativePopup` 调用结果。

```javascript
const result = await fb.menu.showNativePopup();
```

<!-- END AUTO-GENERATED SDK STUBS -->

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
const { menuId } = await fb.menu.show([{ id: 'a', label: 'A' }]);
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

动画与材质之间存在取舍，因为 DWM 背景是窗口级、全有或全无的效果：它随窗口显示 / 隐藏瞬间出现或消失，无法跟 CSS 动画一起淡入淡出。因此 `closeAnimationMs` 只动画 web 内容，开着 `acrylic` / `mica` 时背景会「瞬灭」而内容还在淡出，过场不同步。要全程平滑，请改用 `backdrop: 'none'` 加 `.fb-menu` 的 CSS 半透明背景，代价是 CSS 半透明没有真实模糊。关闭时渲染器把根菜单的 class 从 `#menu.in` 切到 `#menu.out`，内置的 `#menu.out` 规则可经 `css` 覆盖；`replaced`（新菜单顶掉旧菜单）与内部超时路径始终立即隐藏。

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

`iconSvg` 由 `DOMParser` 解析，只把白名单内的图形元素与属性克隆进实时文档，因此不存在裸标记注入。非法或超限图标被静默丢弃，该行照常绘制、只是没有图标。

### 事件

| 事件 | payload | 时机 |
| --- | --- | --- |
| `menu:show` | `{ menuId }` | 菜单显示 |
| `menu:select` | `{ menuId, itemId }` | 选中某项后触发，随后自动关闭 |
| `menu:valueChanged` | `{ menuId, itemId, value }` | rating / slider / segmented 值变更，菜单保持打开 |
| `menu:dismiss` | `{ menuId, reason }` | 关闭（reason：outside / escape / select / api / timeout / blur） |

### 既有菜单方法（主菜单 / 上下文菜单查询与执行）

```javascript
await fb.menu.getMainMenu('View');
await fb.menu.getContextMenu({ mode: 'auto' });
await fb.menu.runMainMenuCommand('View/Console');
await fb.menu.runContextCommand('Properties');
await fb.menu.runContextCommandById(3, { mode: 'selection' });
await fb.menu.showNativePopup({ mode: 'selection' });
```
