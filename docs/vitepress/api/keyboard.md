# Keyboard API

Methods of the `keyboard` namespace.

## keyboard

### keyboard.getRegisteredHotkeys

<!-- api-schema:begin keyboard.getRegisteredHotkeys -->
List the registered hotkeys followed by the shortcuts.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `hotkeys` | `KeyboardHotkey[]` | Hotkeys first, then shortcuts. |
| `hotkeys[].id` | `integer` | Hotkey id; `0` for a shortcut. |
| `hotkeys[].key` | `string` | The combination as registered. |
| `hotkeys[].action` | `string` | The action name. |
| `hotkeys[].global` | `boolean` | The `global` flag given at registration; `false` for a shortcut. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('keyboard.getRegisteredHotkeys');
```

### keyboard.registerHotkey

<!-- api-schema:begin keyboard.registerHotkey -->
Register a system-wide hotkey on the main window. When it is pressed the registering window receives `keyboard:hotkey` with `{ id, key, action }`. Windows refuses a combination another program already holds.

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
Store a key combination as a WebView-local shortcut under the given action name. It is listed by `getRegisteredHotkeys` with `id` `0` and removed by `unregisterHotkey` with the key.

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
Remove a hotkey by its id, or a hotkey or shortcut by its key string; exactly one of the two must be given.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `integer` | No | Id from `registerHotkey`. At least `1`. |
| `key` | `string` | No | The key string the hotkey or shortcut was registered with. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// pass the id returned by keyboard.registerHotkey, or the original key string
await fb2k.invoke('keyboard.unregisterHotkey', { id: 1 });
```

## Interaction delivery and limitations

`keyboard.registerHotkey` registers a Windows hotkey and later routes
`keyboard:hotkey` to the window that registered it. `registerShortcut` only
stores an application-local shortcut. Both registration methods require a
non-empty `key` and `action`; `unregisterHotkey` takes exactly one of the numeric
`id` and the original key string.
