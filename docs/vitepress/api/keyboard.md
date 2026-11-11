# Keyboard API

The `keyboard` namespace provides methods for registering global hotkeys and local shortcuts.

## keyboard

### keyboard.getRegisteredHotkeys

<!-- api-schema:begin keyboard.getRegisteredHotkeys -->
List the registered hotkeys followed by the shortcuts. Shortcut entries go away with `registerShortcut` in 3.0.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `hotkeys` | `KeyboardHotkey[]` | Hotkeys first, then shortcuts. |
| `hotkeys[].id` | `integer` | Hotkey id; `0` for a shortcut. |
| `hotkeys[].key` | `string` | The combination as registered. |
| `hotkeys[].action` | `string` | The action name. |
| `hotkeys[].global` | `boolean` | The `global` flag given at registration; `false` for a shortcut. |
| `hotkeys[].windowId` | `string` | Id of the window that registered the hotkey, the one its presses go to. Absent for a shortcut, and for a hotkey whose caller could not be matched to a window. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('keyboard.getRegisteredHotkeys');
```

### keyboard.registerHotkey

<!-- api-schema:begin keyboard.registerHotkey -->
Register a system-wide hotkey. When it is pressed the registering window receives `keyboard:hotkey` with `{ id, key, action }`, whichever program has the focus. The combination must hold Ctrl, Alt or Win, except a media key alone; Ctrl+A, C, V, X, Y, Z, Ctrl+Shift+Z, Alt+F4, Alt+Tab, Alt+Space and Ctrl+Esc are refused too, all with `INVALID_PARAMS`. A combination a window of this component already holds fails with `OPERATION_FAILED` naming that window; one Windows refuses, because another program or a global foobar2000 shortcut holds it, fails with `OPERATION_FAILED` and the Windows error in `details.win32Error`. The hotkey is released when the registering page navigates or reloads and when its window closes.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | The combination to register. Must not be empty. |
| `action` | `string` | Yes | Action name reported back in the `keyboard:hotkey` payload. Must not be empty. |
| `global` | `boolean` | No | Recorded on the entry and reported by `getRegisteredHotkeys`; the hotkey is registered system-wide either way. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `id` | `integer` | Id of the hotkey, from `1`; the key `unregisterHotkey` takes. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('keyboard.registerHotkey', {
    key: 'Ctrl+Alt+P',
    action: 'togglePause',
});
if (res.success === false) throw new Error(res.error);
const { id } = res;
```

### keyboard.registerShortcut

<!-- api-schema:begin keyboard.registerShortcut -->
Deprecated: Has no effect and will be removed in 3.0; listen for `keydown` in the page instead.

Store a key combination under the given action name. Nothing matches key presses against stored shortcuts. It is listed by `getRegisteredHotkeys` with `id` `0` and removed by `unregisterHotkey` with the key.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | The combination to store. Must not be empty. |
| `action` | `string` | Yes | Action name stored with the shortcut. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('keyboard.registerShortcut', { key: 'Ctrl+Shift+L', action: 'focusSearch' });
```

### keyboard.unregisterHotkey

<!-- api-schema:begin keyboard.unregisterHotkey -->
Remove a hotkey by its id, or a hotkey or shortcut by its key string; exactly one of the two must be given. Removing a shortcut goes away with `registerShortcut` in 3.0.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `integer` | No | Id from `registerHotkey`. At least `1`. |
| `key` | `string` | No | The key string the hotkey or shortcut was registered with. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Pass the ID returned by keyboard.registerHotkey, or the original key combination string
await fb2k.invoke('keyboard.unregisterHotkey', { id: 1 });
```

### keyboard.setReservedKeys

<!-- api-schema:begin keyboard.setReservedKeys -->
Replace the calling page's reserved keys. While the page has the focus, the keys selected in the component preferences run the foobar2000 shortcuts bound to them; a reserved combination stays with the page even when foobar2000 has a shortcut for it. The list is cleared when the page navigates or reloads and when its window closes. A key string that does not parse fails with `INVALID_PARAMS` and leaves the list unchanged; passing an empty list releases all reserved keys. With `all`, every key stays with the page, for example while the page records a shortcut; F12 in development server mode and system-wide hotkeys are excluded, and F5 reloads the page unless a `keydown` listener calls `preventDefault()`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `keys` | `string[]` | Yes | Combinations to keep for the page, written like `registerHotkey` keys; duplicates are fine. |
| `all` | `boolean` | No | Keep every key for the page, not only `keys`. A later call without it keeps only `keys` again. Default: `false`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// The page handles Ctrl+F and Ctrl+S itself, even when foobar2000 binds them
await fb2k.invoke('keyboard.setReservedKeys', { keys: ['Ctrl+F', 'Ctrl+S'] });
// Release them all
await fb2k.invoke('keyboard.setReservedKeys', { keys: [] });
// While recording a shortcut, keep every key for the page; call again without `all` afterwards
await fb2k.invoke('keyboard.setReservedKeys', { keys: ['Ctrl+F', 'Ctrl+S'], all: true });
```

## Keyboard shortcuts inside the WebView

While a page has the focus, key presses go to the shortcuts set under foobar2000's Preferences > Keyboard Shortcuts: main menu and context menu commands the user bound there work as usual, and a key with no shortcut still reaches the page. The user decides which keys take part under the component's preferences, WebView2 UI > Window > Keyboard shortcuts; by default they are combinations with Ctrl, Alt or Win and F1 to F24. Themes can neither read nor change this choice.

While the focus is in a text box (an input, textarea, select or editable content), the keys for typing and moving the cursor stay with the page: letters, digits, Space, punctuation, the arrow keys, Home and End without Alt or Win, and PageUp and PageDown. With the focus anywhere else, these keys go to the shortcuts as well. Those pressed alone or with Shift belong to "Other keys" in the preferences, which is off by default; Ctrl with an arrow key, Home or End belongs to "Ctrl combinations". When a shortcut runs, the key's default action, such as scrolling the page or activating the focused button, is cancelled.

These keys always stay with the page: Ctrl+A, C, V, X, Y, Z, Ctrl+Shift+Z, Ctrl+Insert, Backspace, Delete, Esc, Tab, Enter without Alt or Win, and F12 in development server mode. F5 never goes to a foobar2000 shortcut: it reloads the page unless a `keydown` listener of the page calls `preventDefault()`. While the page is loading, after it failed to load, and on a page whose origin is not trusted, F5 reloads at once and the page does not receive the key press. Context menu commands act on the selection the theme handed to the host through `selection`.

When a theme's own shortcut collides with a foobar2000 binding, keep the combination for the page with [`keyboard.setReservedKeys`](#keyboard-setreservedkeys). Typing and cursor keys pressed outside a text box go to the shortcuts only after every `keydown` listener of the page has run, those on `window` included, so calling `preventDefault()` keeps such a key for the page. Other combinations reach the shortcuts before the page receives them, and reserving them is the only way to keep them.

## Interaction delivery and limitations

`keyboard.registerHotkey` registers a Windows system hotkey, dispatches `keyboard:hotkey` to the registering window, and releases the hotkey when that page navigates or reloads and when its window closes. `keyboard.registerShortcut` is deprecated: it stores the combination without matching key presses, and will be removed in 3.0. Both registration methods require non-empty `key` and `action` parameters; `keyboard.unregisterHotkey` takes exactly one of the numeric `id` and the original `key` string.
