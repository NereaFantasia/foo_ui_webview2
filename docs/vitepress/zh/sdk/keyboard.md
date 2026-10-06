# fb.keyboard 热键与快捷键

`fb.keyboard` 注册热键和快捷键，并列出当前已注册的条目。热键是全局注册的，按下时通过 `keyboard:hotkey` 报告；快捷键是登记在 WebView 内、挂在某个动作名下的按键组合。

## getRegisteredHotkeys()

签名：`fb.keyboard.getRegisteredHotkeys(): Promise<KeyboardGetRegisteredHotkeysResponse>`

返回 `{ success, hotkeys }`。每个 `HotkeyInfo` 包含 `id`、`key`、`action` 与 `global`。先列热键，后列快捷键；快捷键的 `id` 为 `0`，`global` 为 `false`。

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

把按键组合登记为 WebView 内的快捷键，挂在给定的动作名下。和 `registerHotkey()` 不同，它不接受选项对象，也不做全局注册。`getRegisteredHotkeys()` 以 `id` 为 `0` 列出它，`unregisterHotkey({ key })` 移除它。

```javascript
const result = await fb.keyboard.registerShortcut('Space', 'toggle');
```

## unregisterHotkey(options)

签名：`fb.keyboard.unregisterHotkey(options: KeyboardUnregisterHotkeyParams): Promise<KeyboardUnregisterHotkeyResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `options.key` | `string` | 否 | 注册时用的按键组合 |
| `options.id` | `number` | 否 | 注册时返回的数字 id |

移除热键或快捷键。`id` 和 `key` 必须恰好给一个：`id` 按 `registerHotkey()` 返回的数字移除热键，`key` 按注册时用的字符串移除热键或快捷键。

```javascript
const result = await fb.keyboard.unregisterHotkey({
	key: 'Ctrl+Shift+P',
});
```

## 注册热键

签名：`fb.keyboard.registerHotkey(key: string, action: string, options?: Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>): Promise<KeyboardRegisterHotkeyResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 是 | 要注册的按键组合 |
| `action` | `string` | 是 | 随 `keyboard:hotkey` 载荷报回的动作名 |
| `options.global` | `boolean` | 否 | 记录在条目上，由 `getRegisteredHotkeys()` 报出；默认 `true` |

`fb.keyboard.registerHotkey(key, action, options?)` 调用 `keyboard.registerHotkey` 注册全局热键。`options` 的类型是 `Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>`，可设置 `global`。成功时响应带 `id`，是从 `1` 起的数字；`unregisterHotkey({ id })` 接收它，`getRegisteredHotkeys()` 里的 `HotkeyInfo.id` 就是它。

热键注册在主窗口上，不管 `global` 取什么值都是全局的。别的程序已占用的组合 Windows 会拒绝。

按键组合由修饰键 `Ctrl`（或 `Control`）、`Alt`、`Shift`、`Win` 用 `+` 连到键名组成。键名可以是字母、数字、`F1` 到 `F12`、`Space`、`Enter`、`Tab`、`Escape`、`Backspace`、`Delete`、`Insert`、`Home`、`End`、`PageUp`、`PageDown`、方向键、媒体键 `PlayPause`、`MediaStop`、`NextTrack`、`PrevTrack`、`VolumeUp`、`VolumeDown`、`VolumeMute`，或标点键。不区分大小写。

```javascript
const result = await fb.keyboard.registerHotkey(
	'Ctrl+Shift+P',
	'playPause',
	{ global: true },
);
```

## keyboard:hotkey

已注册热键触发时，`keyboard:hotkey` 携带含 `id`、`key` 与 `action` 的 `KeyboardHotkeyPayload`。

事件发给注册热键的窗口。该窗口关闭后热键仍然有效，之后的按键改发给主窗口的页面。

```javascript
const off = fb.on('keyboard:hotkey', ({ id, key, action }) => {
	console.log(id, key, action);
});

off();
```
