# fb.keyboard Hotkeys and Shortcuts

`fb.keyboard` registers hotkeys and shortcuts and exposes the current hotkey inventory. A hotkey is registered system-wide and reports presses through `keyboard:hotkey`; a shortcut is a key combination stored inside the WebView under an action name.

## getRegisteredHotkeys()

Signature: `fb.keyboard.getRegisteredHotkeys(): Promise<KeyboardGetRegisteredHotkeysResponse>`

Returns `{ success, hotkeys }`. Each `HotkeyInfo` contains `id`, `key`, `action`, and `global`; a hotkey entry also has `windowId`, the window that registered it and receives its presses. The returned list orders hotkeys first, followed by shortcuts; shortcut entries have `id` `0` and `global` `false`. Shortcut entries will be removed alongside `registerShortcut()` in 3.0.

```javascript
const res = await fb.keyboard.getRegisteredHotkeys();
if (res.success === false) throw new Error(res.error);
const { hotkeys } = res;
```

## registerShortcut(key, action)

Signature: `fb.keyboard.registerShortcut(key: string, action: string): Promise<KeyboardRegisterShortcutResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Shortcut key expression |
| `action` | `string` | Yes | Action name stored with the shortcut |

::: warning Deprecated
`registerShortcut()` has no effect and will be removed in 3.0. Listen for `keydown` in the page for in-page shortcuts.
:::

Registers the key combination under the specified action name, but no host logic matches key presses against it. Unlike `registerHotkey()`, this method accepts no options object and performs no system-wide registration. `getRegisteredHotkeys()` lists it with `id` `0`, and `unregisterHotkey({ key })` removes it.

```javascript
const result = await fb.keyboard.registerShortcut('Space', 'toggle');
```

## unregisterHotkey(options)

Signature: `fb.keyboard.unregisterHotkey(options: KeyboardUnregisterHotkeyParams): Promise<KeyboardUnregisterHotkeyResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.key` | `string` | No | Registered key expression |
| `options.id` | `number` | No | Numeric registration ID |

Removes a hotkey or shortcut. Exactly one of `id` or `key` must be provided: `id` removes a hotkey by the numeric ID returned by `registerHotkey()`; `key` removes a hotkey or shortcut by its registered key combination string. Removing shortcuts by `key` will be removed alongside `registerShortcut()` in 3.0.

```javascript
const result = await fb.keyboard.unregisterHotkey({
	key: 'Ctrl+Shift+P',
});
```

## setReservedKeys(keys)

Signature: `fb.keyboard.setReservedKeys(keys: string[], options?: { all?: boolean }): Promise<KeyboardSetReservedKeysResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `keys` | `string[]` | Yes | Combinations to keep for the page, written like the `key` of `registerHotkey()` |
| `options.all` | `boolean` | No | Keep every key for the page, not only `keys`; defaults to `false` |

Replaces this page's reserved keys. While the page has focus, the keys selected in the component preferences run the foobar2000 shortcuts bound to them under Preferences > Keyboard Shortcuts; combinations specified in the reserved list are dispatched directly to the page `keydown` event even if foobar2000 defines matching shortcuts. The reserved list resets when navigating, reloading, or closing the window; pass an empty array to clear all reservations. Invalid key strings fail with `INVALID_PARAMS` and leave the list unchanged. Setting `{ all: true }` routes all keys to the page (such as during shortcut recording); a subsequent call with explicit `keys` restores selective forwarding. System-level hotkeys and F12 in development server mode are excluded, and F5 reloads the page unless a `keydown` listener calls `preventDefault()`. For forwarding rules, see [Keyboard shortcuts inside the WebView](../api/keyboard.md#keyboard-shortcuts-inside-the-webview).

```javascript
await fb.keyboard.setReservedKeys(['Ctrl+F', 'Ctrl+S']);
// While recording a shortcut
await fb.keyboard.setReservedKeys(['Ctrl+F', 'Ctrl+S'], { all: true });
```

## Register a Hotkey

Signature: `fb.keyboard.registerHotkey(key: string, action: string, options?: Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>): Promise<KeyboardRegisterHotkeyResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Key combination to register |
| `action` | `string` | Yes | Action name reported back in the `keyboard:hotkey` payload |
| `options.global` | `boolean` | No | Recorded on the entry and reported by `getRegisteredHotkeys()`; defaults to `true` |

`fb.keyboard.registerHotkey(key, action, options?)` registers a system-wide hotkey through `keyboard.registerHotkey`. `options` is `Omit<KeyboardRegisterHotkeyParams, 'key' | 'action'>` and can set `global`. On success, returns a positive integer `id` (starting from `1`), which can be passed to `unregisterHotkey({ id })` and matches `HotkeyInfo.id` in `getRegisteredHotkeys()`.

Hotkeys are registered by the component message window and are system-wide regardless of the `global` value. Combinations must include `Ctrl`, `Alt`, or `Win` (except standalone media keys). Reserved system combinations (such as `Ctrl+A/C/V/X/Y/Z`, `Ctrl+Shift+Z`, `Alt+F4`, `Alt+Tab`, `Alt+Space`, and `Ctrl+Esc`) are rejected with `INVALID_PARAMS`. If a combination is already held by a window of this component, another application, or a foobar2000 global shortcut, the call fails with `OPERATION_FAILED`; when another application or a foobar2000 global shortcut holds it, `details.win32Error` contains the Win32 error code.

Registered hotkeys are automatically released when the page navigates, reloads, or closes; they must be re-registered upon page load.

Key combinations join modifier keys (`Ctrl` or `Control`, `Alt`, `Shift`, `Win`) with a base key name using `+`. Base key names include letters, digits, function keys (`F1`–`F12`), `Space`, `Enter`, `Tab`, `Escape`, `Backspace`, `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, arrow keys, media keys (`PlayPause`, `MediaStop`, `NextTrack`, `PrevTrack`, `VolumeUp`, `VolumeDown`, `VolumeMute`), or punctuation keys. Key names are case-insensitive.

```javascript
const result = await fb.keyboard.registerHotkey(
	'Ctrl+Shift+P',
	'playPause',
	{ global: true },
);
```

## keyboard:hotkey

When a registered hotkey fires, `keyboard:hotkey` carries `KeyboardHotkeyPayload` with `id`, `key`, and `action`.

The event goes to the window that registered the hotkey.

```javascript
const off = fb.on('keyboard:hotkey', ({ id, key, action }) => {
	console.log(id, key, action);
});

off();
```
