# fb.keyboard keyboard

本页是 `fb.keyboard` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### getRegisteredHotkeys()

封装 `keyboard.getRegisteredHotkeys`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.keyboard.getRegisteredHotkeys(/* 参数见 TypeScript 声明 */);
```

### registerShortcut()

封装 `keyboard.registerShortcut`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.keyboard.registerShortcut(/* 参数见 TypeScript 声明 */);
```

### unregisterHotkey()

封装 `keyboard.unregisterHotkey`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.keyboard.unregisterHotkey(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 注册热键

`fb.keyboard.registerHotkey(key, action, options?)` 调用 `keyboard.registerHotkey`。`options` 的类型是 `Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>`，可设置 `global`。当前 facade 将可选响应字段 `id` 标为 `string`，而 `getRegisteredHotkeys()` 中每个 `HotkeyInfo.id` 的类型是 `number`。

```javascript
const result = await fb.keyboard.registerHotkey(
	'Ctrl+Shift+P',
	'playPause',
	{ global: true },
);
```

## keyboard:hotkey

已注册热键触发时，`keyboard:hotkey` 携带含 `id`、`key` 与 `action` 的 `KeyboardHotkeyPayload`。

```javascript
const off = fb.on('keyboard:hotkey', ({ id, key, action }) => {
	console.log(id, key, action);
});

off();
```
