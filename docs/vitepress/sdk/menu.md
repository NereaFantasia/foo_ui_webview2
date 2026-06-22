# fb.menu menu

本页是 `fb.menu` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

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

签名：`fb.menu.runContextCommand(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `menu.runContextCommand`, `menu.runContextCommandById` 调用结果。

```javascript
const result = await fb.menu.runContextCommand();
```

### runMainMenuCommand()

签名：`fb.menu.runMainMenuCommand(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `menu.runMainMenuCommand` 调用结果。

```javascript
const result = await fb.menu.runMainMenuCommand();
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
await fb.menu.runContextCommandById(3, { mode: 'playlist' });
await fb.menu.showNativePopup({ mode: 'playlist' });
```
