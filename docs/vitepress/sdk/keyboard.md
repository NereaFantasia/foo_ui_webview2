# fb.keyboard Hotkeys and Shortcuts

`fb.keyboard` registers hotkeys and shortcuts and exposes the current hotkey inventory. A hotkey is registered system-wide and reports presses through `keyboard:hotkey`; a shortcut is a key combination stored inside the WebView under an action name.

## getRegisteredHotkeys()

Signature: `fb.keyboard.getRegisteredHotkeys(): Promise<KeyboardGetRegisteredHotkeysResponse>`

Returns `{ success, hotkeys }`. Each `HotkeyInfo` contains `id`, `key`, `action`, and `global`. Hotkeys come first, then shortcuts; a shortcut has `id` `0` and `global` `false`.

```javascript
const res = await fb.keyboard.getRegisteredHotkeys();
if (res.success === false) throw new Error(res.error);
const { hotkeys } = res;
```

## registerShortcut(key, action)

Signature: `fb.keyboard.registerShortcut(key: string, action: string): Promise<KeyboardRegisterShortcutResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Shortcut key expression. |
| `action` | `string` | Yes | Action name stored with the shortcut. |

Stores the key combination as a WebView-local shortcut under the given action name. Unlike `registerHotkey()`, it takes no options object and registers nothing system-wide. `getRegisteredHotkeys()` lists the shortcut with `id` `0`, and `unregisterHotkey({ key })` removes it.

```javascript
const result = await fb.keyboard.registerShortcut('Space', 'toggle');
```

## unregisterHotkey(options)

Signature: `fb.keyboard.unregisterHotkey(options: KeyboardUnregisterHotkeyParams): Promise<KeyboardUnregisterHotkeyResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.key` | `string` | No | Registered key expression. |
| `options.id` | `number` | No | Numeric registration ID. |

Removes a hotkey or a shortcut. Give exactly one of `id` and `key`: `id` removes a hotkey by the number `registerHotkey()` returned, and `key` removes a hotkey or a shortcut by the string it was registered with.

```javascript
const result = await fb.keyboard.unregisterHotkey({
	key: 'Ctrl+Shift+P',
});
```

## Register a Hotkey

Signature: `fb.keyboard.registerHotkey(key: string, action: string, options?: Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>): Promise<KeyboardRegisterHotkeyResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Key combination to register. |
| `action` | `string` | Yes | Action name reported back in the `keyboard:hotkey` payload. |
| `options.global` | `boolean` | No | Recorded on the entry and reported by `getRegisteredHotkeys()`; defaults to `true`. |

`fb.keyboard.registerHotkey(key, action, options?)` registers a system-wide hotkey through `keyboard.registerHotkey`. `options` is `Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>` and can set `global`. On success the response carries `id`, a number from `1`; `unregisterHotkey({ id })` takes it, and `getRegisteredHotkeys()` reports it as `HotkeyInfo.id`.

The hotkey is registered on the main window and is system-wide whatever `global` says. Windows refuses a combination another program already holds.

A key combination joins the modifiers `Ctrl` (or `Control`), `Alt`, `Shift`, and `Win` with `+` to a key name: a letter, a digit, `F1` to `F12`, `Space`, `Enter`, `Tab`, `Escape`, `Backspace`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, an arrow key, one of the media keys `PlayPause`, `MediaStop`, `NextTrack`, `PrevTrack`, `VolumeUp`, `VolumeDown`, `VolumeMute`, or a punctuation key. Case does not matter.

```javascript
const result = await fb.keyboard.registerHotkey(
	'Ctrl+Shift+P',
	'playPause',
	{ global: true },
);
```

## keyboard:hotkey

When a registered hotkey fires, `keyboard:hotkey` carries `KeyboardHotkeyPayload` with `id`, `key`, and `action`.

The event goes to the window that registered the hotkey. The hotkey stays registered after that window closes, and its presses then go to the main window's page.

```javascript
const off = fb.on('keyboard:hotkey', ({ id, key, action }) => {
	console.log(id, key, action);
});

off();
```
