# fb.keyboard 热键与快捷键

`fb.keyboard` 注册热键和快捷键，并列出当前已注册的条目。热键是全局注册的，按下时通过 `keyboard:hotkey` 报告；快捷键是登记在 WebView 内、挂在某个动作名下的按键组合。

## getRegisteredHotkeys()

签名：`fb.keyboard.getRegisteredHotkeys(): Promise<KeyboardGetRegisteredHotkeysResponse>`

返回 `{ success, hotkeys }`。每个 `HotkeyInfo` 包含 `id`、`key`、`action` 与 `global`；热键条目另有 `windowId`，即注册它并接收按键事件的窗口。返回列表中热键排在前面，快捷键排在后面；快捷键条目的 `id` 为 `0`，`global` 为 `false`。快捷键条目将在 3.0 版本中随 `registerShortcut()` 一并移除。

```javascript
const res = await fb.keyboard.getRegisteredHotkeys();
if (res.success === false) throw new Error(res.error);
const { hotkeys } = res;
```

## registerShortcut(key, action)

签名：`fb.keyboard.registerShortcut(key: string, action: string): Promise<KeyboardRegisterShortcutResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 快捷键的按键组合 |
| `action` | `string` | 是 | 随快捷键存储的动作名 |

::: warning 已废弃
`registerShortcut()` 没有任何效果，3.0 移除。页面里的快捷键请直接监听 `keydown`。
:::

将按键组合记录在指定动作名下，但宿主不执行按键匹配逻辑。与 `registerHotkey()` 不同，该方法不接受选项对象，也不进行全局系统注册。`getRegisteredHotkeys()` 返回的条目中该快捷键 `id` 为 `0`，可通过 `unregisterHotkey({ key })` 移除。

```javascript
const result = await fb.keyboard.registerShortcut('Space', 'toggle');
```

## unregisterHotkey(options)

签名：`fb.keyboard.unregisterHotkey(options: KeyboardUnregisterHotkeyParams): Promise<KeyboardUnregisterHotkeyResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `options.key` | `string` | 否 | 注册时用的按键组合 |
| `options.id` | `number` | 否 | 注册时返回的数字 id |

移除热键或快捷键。`id` 与 `key` 二者必选其一且互斥：指定 `id` 时根据 `registerHotkey()` 返回的数值注销热键；指定 `key` 时根据注册组合键字符串注销热键或快捷键。通过 `key` 注销快捷键的功能将在 3.0 版本中随 `registerShortcut()` 一并移除。

```javascript
const result = await fb.keyboard.unregisterHotkey({
	key: 'Ctrl+Shift+P',
});
```

## setReservedKeys(keys)

签名：`fb.keyboard.setReservedKeys(keys: string[], options?: { all?: boolean }): Promise<KeyboardSetReservedKeysResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `keys` | `string[]` | 是 | 留给页面的按键组合，写法与 `registerHotkey()` 的 `key` 相同 |
| `options.all` | `boolean` | 否 | 是否把全部按键都留给页面，而不只是 `keys`；默认 `false` |

替换当前页面的保留按键列表。页面聚焦时，组件首选项选定的按键执行 foobar2000「首选项 > 键盘快捷键」中为其绑定的快捷键；列表中指定的保留键始终优先由页面 `keydown` 事件接收。列表在页面导航、重载或窗口关闭时重置；传入空数组可清空保留键。若按键字符串无法解析，方法以 `INVALID_PARAMS` 失败且保留键列表保持不变。设置 `{ all: true }` 时所有按键均由页面接收（如快捷键录制期间），再次调用传入普通 `keys` 即可恢复选择性保留。系统级热键与开发服务器模式下的 F12 快捷键除外。页面的 `keydown` 监听没有调用 `preventDefault()` 时，F5 重载页面。转发规则见 [WebView 内的快捷键](../api/keyboard.md#webview-内的快捷键)。

```javascript
await fb.keyboard.setReservedKeys(['Ctrl+F', 'Ctrl+S']);
// 录制快捷键期间
await fb.keyboard.setReservedKeys(['Ctrl+F', 'Ctrl+S'], { all: true });
```

## 注册热键

签名：`fb.keyboard.registerHotkey(key: string, action: string, options?: Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>): Promise<KeyboardRegisterHotkeyResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要注册的按键组合 |
| `action` | `string` | 是 | 随 `keyboard:hotkey` 载荷报回的动作名 |
| `options.global` | `boolean` | 否 | 记录在条目上，由 `getRegisteredHotkeys()` 报出；默认 `true` |

`fb.keyboard.registerHotkey(key, action, options?)` 调用 `keyboard.registerHotkey` 注册全局热键。`options` 的类型是 `Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>`，可设置 `global`。注册成功时返回自增数字 `id`（从 `1` 开始）；后续可通过 `unregisterHotkey({ id })` 注销，该值亦对应 `getRegisteredHotkeys()` 中返回的 `HotkeyInfo.id`。

热键由组件独立消息窗口注册，无论 `global` 为何值均属于系统级全局热键。按键组合须包含 `Ctrl`、`Alt` 或 `Win`（独立媒体键除外）。系统保留键组合（如 `Ctrl+A/C/V/X/Y/Z`、`Ctrl+Shift+Z`、`Alt+F4`、`Alt+Tab`、`Alt+Space` 及 `Ctrl+Esc`）均被拒绝并返回 `INVALID_PARAMS`。若该组合已被本组件的某个窗口、其他程序或 foobar2000 的全局快捷键占用，调用返回 `OPERATION_FAILED`；被其他程序或 foobar2000 全局快捷键占用时，`details.win32Error` 包含对应的 Windows 错误码。

页面导航、重载或窗口关闭时，其注册的热键将自动注销；页面加载后须重新注册。

按键组合通过 `+` 连接修饰键（`Ctrl` 或 `Control`、`Alt`、`Shift`、`Win`）与基础键名。基础键名可为字母、数字、功能键（`F1`–`F12`）、`Space`、`Enter`、`Tab`、`Escape`、`Backspace`、`Delete`、`Insert`、`Home`、`End`、`PageUp`、`PageDown`、方向键、媒体键（`PlayPause`、`MediaStop`、`NextTrack`、`PrevTrack`、`VolumeUp`、`VolumeDown`、`VolumeMute`）或标点符号键。键名匹配不区分大小写。

```javascript
const result = await fb.keyboard.registerHotkey(
	'Ctrl+Shift+P',
	'playPause',
	{ global: true },
);
```

## keyboard:hotkey

已注册热键触发时，`keyboard:hotkey` 携带含 `id`、`key` 与 `action` 的 `KeyboardHotkeyPayload`。

事件发给注册热键的窗口。

```javascript
const off = fb.on('keyboard:hotkey', ({ id, key, action }) => {
	console.log(id, key, action);
});

off();
```
