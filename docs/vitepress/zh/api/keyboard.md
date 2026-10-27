# Keyboard 键盘 API

提供 Windows 系统级全局热键与 WebView 应用内快捷键的注册、管理与事件监听能力。

## Keyboard API - 快捷键 (4 个 API)

### keyboard.registerHotkey

<!-- api-schema:begin keyboard.registerHotkey -->
在主窗口句柄上注册系统级全局热键；按下该热键时向发起注册的窗口派发 `keyboard:hotkey` 事件（载荷为 `{ id, key, action }`）；若按键组合已被系统中其他程序占用，Windows 系统将拒绝注册

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待注册的按键组合字符串不能为空。 |
| `action` | `string` | 是 | 触发热键时随 `keyboard:hotkey` 事件载荷回传的动作名称不能为空。 |
| `global` | `boolean` | 否 | 是否标记为全局热键（记录于条目属性中供 `getRegisteredHotkeys` 查询，实际均注册为系统级热键）默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `integer` | 分配的热键唯一标识符（自 `1` 开始递增，注销时可传给 `unregisterHotkey`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('keyboard.registerHotkey', {
    key: 'Ctrl+Alt+Space',
    action: 'play_pause',
    global: true
});
// result.id 可用于后续 unregisterHotkey

fb2k.on('keyboard:hotkey', (data) => {
    if (data.action === 'play_pause') fb2k.invoke('playback.playOrPause');
});
```

### keyboard.registerShortcut

<!-- api-schema:begin keyboard.registerShortcut -->
登记 WebView 页面作用域内的应用内快捷键（绑定至指定动作名；通过 `getRegisteredHotkeys` 列出时 `id` 固定为 `0`，注销时通过 `unregisterHotkey` 传入 key 字符串移除）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 待登记的按键组合字符串不能为空。 |
| `action` | `string` | 是 | 与快捷键关联绑定的动作名称不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### keyboard.unregisterHotkey

<!-- api-schema:begin keyboard.unregisterHotkey -->
注销已注册的热键或应用内快捷键（支持通过数字 `id` 注销热键，或通过按键组合 `key` 字符串注销热键或快捷键；两者必须恰好提供一项）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `integer` | 否 | 待注销热键的数值标识符 id（取自 `registerHotkey` 的返回值）不小于 `1`。 |
| `key` | `string` | 否 | 待注销热键或快捷键的原始按键组合字符串 key |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('keyboard.unregisterHotkey', { id: result.id });
// 或
await fb2k.invoke('keyboard.unregisterHotkey', { key: 'Ctrl+Alt+Space' });
```

### keyboard.getRegisteredHotkeys

<!-- api-schema:begin keyboard.getRegisteredHotkeys -->
获取已注册的全部全局热键及应用内快捷键列表（先列出系统热键，后列出应用内快捷键）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hotkeys` | `KeyboardHotkey[]` | 已注册条目列表（系统级全局热键在前，应用内快捷键在后） |
| `hotkeys[].id` | `integer` | 热键标识符 id（系统级全局热键大于等于 1，应用内快捷键固定为 `0`） |
| `hotkeys[].key` | `string` | 注册时配置的按键组合字符串 |
| `hotkeys[].action` | `string` | 关联绑定的动作名称 |
| `hotkeys[].global` | `boolean` | 是否为全局热键（全局热键为 true，应用内快捷键为 false） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```json
{
    "success": true,
    "hotkeys": [
        { "id": 1, "key": "Ctrl+Alt+Space", "action": "play_pause", "global": true }
    ]
}
```

## 交互投递与限制

`keyboard.registerHotkey` 向 Windows 注册系统级全局热键，触发时将 `keyboard:hotkey` 事件定向派发给发起注册的窗口；若注册窗口已销毁，后续触发自动转派给主窗口。`keyboard.registerShortcut` 仅登记应用内快捷键。两类注册方法均要求传入非空的 `key` 与 `action`；`keyboard.unregisterHotkey` 必须在数字 `id` 与原始 `key` 按键组合字符串中恰好指定一项。
