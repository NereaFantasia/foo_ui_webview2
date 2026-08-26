# fb.keyboard Hotkeys and Shortcuts

`fb.keyboard` registers hotkeys and shortcuts and exposes the current hotkey inventory.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### getRegisteredHotkeys()

Signature: `fb.keyboard.getRegisteredHotkeys(): Promise<KeyboardGetRegisteredHotkeysResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns `{ success, hotkeys }`. Each `HotkeyInfo` contains `id`, `key`, `action`, and `global`.

```javascript
const { hotkeys } = await fb.keyboard.getRegisteredHotkeys();
```

### registerShortcut()

Signature: `fb.keyboard.registerShortcut(key: string, action: string): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Shortcut key expression. |
| `action` | `string` | Yes | Action identifier sent by the host. |

Returns the `keyboard.registerShortcut` response envelope.

```javascript
const result = await fb.keyboard.registerShortcut('Space', 'toggle');
```

### unregisterHotkey()

Signature: `fb.keyboard.unregisterHotkey(options: KeyboardUnregisterHotkeyParams): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.key` | `string` | No | Registered key expression. |
| `options.id` | `number` | No | Numeric registration ID. |

Returns the `keyboard.unregisterHotkey` response envelope.

```javascript
const result = await fb.keyboard.unregisterHotkey({
	key: 'Ctrl+Shift+P',
});
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Register a Hotkey

`fb.keyboard.registerHotkey(key, action, options?)` invokes `keyboard.registerHotkey`. `options` is `Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>` and can set `global`. The facade currently types the optional response `id` as `string`; `getRegisteredHotkeys()` exposes each registered `HotkeyInfo.id` as `number`.

```javascript
const result = await fb.keyboard.registerHotkey(
	'Ctrl+Shift+P',
	'playPause',
	{ global: true },
);
```

## keyboard:hotkey

When a registered hotkey fires, `keyboard:hotkey` carries `KeyboardHotkeyPayload` with `id`, `key`, and `action`.

```javascript
const off = fb.on('keyboard:hotkey', ({ id, key, action }) => {
	console.log(id, key, action);
});

off();
```
