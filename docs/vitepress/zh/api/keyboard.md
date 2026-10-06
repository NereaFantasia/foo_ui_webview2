# Keyboard 键盘 API

注册全局热键与 WebView 内快捷键。

## Keyboard API - 快捷键 (4 个 API)

### keyboard.registerHotkey

<!-- api-schema:begin keyboard.registerHotkey -->
在主窗口上注册全局热键。按下时注册它的窗口收到 `keyboard:hotkey`（`{ id, key, action }`）。别的程序已占用的组合 Windows 会拒绝。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要注册的组合。不能为空。 |
| `action` | `string` | 是 | 随 `keyboard:hotkey` 载荷报回的动作名。不能为空。 |
| `global` | `boolean` | 否 | 记录在条目上并由 `getRegisteredHotkeys` 报出；无论取值热键都是全局注册的。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `integer` | 热键 id，从 `1` 起；`unregisterHotkey` 用它。 |

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
把一个按键组合登记为 WebView 内的快捷键，挂在给定的动作名下。`getRegisteredHotkeys` 以 `id` 为 `0` 列出它，`unregisterHotkey` 按 key 移除。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要登记的组合。不能为空。 |
| `action` | `string` | 是 | 随快捷键存储的动作名。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### keyboard.unregisterHotkey

<!-- api-schema:begin keyboard.unregisterHotkey -->
按 id 移除热键，或按 key 字符串移除热键或快捷键；两者必须恰好给一个。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `id` | `integer` | 否 | `registerHotkey` 返回的 id。不小于 `1`。 |
| `key` | `string` | 否 | 注册热键或快捷键时用的 key 字符串。 |

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
列出已注册的热键，随后是快捷键。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `hotkeys` | `KeyboardHotkey[]` | 先热键，后快捷键。 |
| `hotkeys[].id` | `integer` | 热键 id；快捷键为 `0`。 |
| `hotkeys[].key` | `string` | 注册时的组合。 |
| `hotkeys[].action` | `string` | 动作名。 |
| `hotkeys[].global` | `boolean` | 注册时给的 `global`；快捷键为 `false`。 |

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

`keyboard.registerHotkey` 注册 Windows 热键，随后将 `keyboard:hotkey` 路由到
注册它的窗口。`registerShortcut` 只保存应用内快捷键。两个注册方法都要求非空
`key` 和 `action`；`unregisterHotkey` 只认数字 `id` 与原始 key 字符串二者之一。
