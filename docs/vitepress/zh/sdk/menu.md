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

`menu.show` / `menu.close` / `menu.popup` 用 WebView 渲染上下文菜单，替代原生 Win32 `TrackPopupMenu`，支持子菜单、键盘导航与 `::part()` 主题化。

> **与 tray zones 的边界**：public `menu.show` **始终**使用 legacy 直接子 DOM（`#menu > .fb-item`），**不会**出现 `.fb-zone` 容器。分区布局（`layoutMode: 'zones'`）仅属于 `tray.setContextMenu` + `render: 'webview'` 的 opt-in 能力。

### fb.menu.popup(items, position?)

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

### fb.menu.show(items, position?)

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
- **单图 SVG > 32 KiB**：丢弃该图标，不 fail 整菜单。
- 本 API 是通用自绘菜单入口，**不是**托盘分区（top / playback / bottom）配置面；托盘分区请用 `tray.setContextMenu` / `tray.appendMenuItems`。

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
| `type` | `'normal' \| 'separator'` | `separator` 渲染分隔线 |
| `enabled` | `boolean` | 默认 `true`；`false` 灰显不可选 |
| `checked` | `boolean` | 渲染勾选标记 |
| `submenu` | `MenuPopupItem[]` | 子菜单，渲染右展开箭头 |

### 事件

| 事件 | payload | 时机 |
| --- | --- | --- |
| `menu:show` | `{ menuId }` | 菜单显示 |
| `menu:select` | `{ menuId, itemId }` | 选中某项后触发，随后自动关闭 |
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
