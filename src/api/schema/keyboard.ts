import type { Int } from './common.js';

export interface Api {
  /**
   * Register a system-wide hotkey on the main window. When it is pressed the registering
   * window receives `keyboard:hotkey` with `{ id, key, action }`. Windows refuses a combination
   * another program already holds.
   * @zh 在主窗口上注册全局热键。按下时注册它的窗口收到 `keyboard:hotkey`（`{ id, key, action }`）。别的程序已占用的组合 Windows 会拒绝。
   */
  registerHotkey(params: RegisterHotkeyParams): RegisterHotkeyResult;

  /**
   * Store a key combination as a WebView-local shortcut under the given action name. It is
   * listed by `getRegisteredHotkeys` with `id` `0` and removed by `unregisterHotkey` with the
   * key.
   * @zh 把一个按键组合登记为 WebView 内的快捷键，挂在给定的动作名下。`getRegisteredHotkeys` 以 `id` 为 `0` 列出它，`unregisterHotkey` 按 key 移除。
   */
  registerShortcut(params: RegisterShortcutParams): void;

  /**
   * Remove a hotkey by its id, or a hotkey or shortcut by its key string; exactly one of the
   * two must be given.
   * @zh 按 id 移除热键，或按 key 字符串移除热键或快捷键；两者必须恰好给一个。
   */
  unregisterHotkey(params: UnregisterHotkeyParams): void;

  /**
   * List the registered hotkeys followed by the shortcuts.
   * @zh 列出已注册的热键，随后是快捷键。
   */
  getRegisteredHotkeys(): GetRegisteredHotkeysResult;
}

export interface Events {
  /**
   * A hotkey registered with `keyboard.registerHotkey` was pressed. It goes to the window that
   * registered it. A hotkey stays registered after that window closes; its presses then go to
   * the main window's page.
   * @zh 用 `keyboard.registerHotkey` 注册的热键被按下，发给注册它的窗口。该窗口关闭后热键仍然有效，之后的按键改发给主窗口的页面。
   * @delivery owner
   */
  hotkey: HotkeyPayload;
}

interface HotkeyPayload {
  /**
   * The id `keyboard.registerHotkey` returned.
   * @zh `keyboard.registerHotkey` 返回的 id。
   */
  id: Int;
  /**
   * The combination as it was registered.
   * @zh 注册时给的组合。
   */
  key: string;
  /**
   * The action name given at registration.
   * @zh 注册时给的动作名。
   */
  action: string;
}

/**
 * A key combination: modifiers `Ctrl` / `Control`, `Alt`, `Shift`, `Win` joined with `+` to a
 * key name (a letter, a digit, `F1` to `F12`, `Space`, `Enter`, `Tab`, `Escape`, `Backspace`,
 * `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, the arrow keys, the media keys
 * `PlayPause` / `MediaStop` / `NextTrack` / `PrevTrack` / `VolumeUp` / `VolumeDown` /
 * `VolumeMute`, or one of the punctuation keys). Case does not matter.
 * @zh 按键组合：修饰键 `Ctrl` / `Control`、`Alt`、`Shift`、`Win` 用 `+` 连到键名（字母、数字、`F1` 到 `F12`、`Space`、`Enter`、`Tab`、`Escape`、`Backspace`、`Delete`、`Insert`、`Home`、`End`、`PageUp`、`PageDown`、方向键、媒体键 `PlayPause` / `MediaStop` / `NextTrack` / `PrevTrack` / `VolumeUp` / `VolumeDown` / `VolumeMute`，或标点键）。不区分大小写。
 */
type KeyCombination = string;

interface RegisterHotkeyParams {
  /**
   * The combination to register.
   * @zh 要注册的组合。
   * @minLength 1
   */
  key: KeyCombination;
  /**
   * Action name reported back in the `keyboard:hotkey` payload.
   * @zh 随 `keyboard:hotkey` 载荷报回的动作名。
   * @minLength 1
   */
  action: string;
  /**
   * Recorded on the entry and reported by `getRegisteredHotkeys`; the hotkey is registered
   * system-wide either way.
   * @zh 记录在条目上并由 `getRegisteredHotkeys` 报出；无论取值热键都是全局注册的。
   * @default true
   */
  global?: boolean;
}

interface RegisterHotkeyResult {
  /**
   * Id of the hotkey, from `1`; the key `unregisterHotkey` takes.
   * @zh 热键 id，从 `1` 起；`unregisterHotkey` 用它。
   */
  id: Int;
}

interface RegisterShortcutParams {
  /**
   * The combination to store.
   * @zh 要登记的组合。
   * @minLength 1
   */
  key: KeyCombination;
  /**
   * Action name stored with the shortcut.
   * @zh 随快捷键存储的动作名。
   * @minLength 1
   */
  action: string;
}

interface UnregisterHotkeyParams {
  /**
   * Id from `registerHotkey`.
   * @zh `registerHotkey` 返回的 id。
   * @minimum 1
   */
  id?: Int;
  /**
   * The key string the hotkey or shortcut was registered with.
   * @zh 注册热键或快捷键时用的 key 字符串。
   */
  key?: KeyCombination;
}

/**
 * One registered entry.
 * @zh 一条已注册的条目。
 */
interface KeyboardHotkey {
  /**
   * Hotkey id; `0` for a shortcut.
   * @zh 热键 id；快捷键为 `0`。
   */
  id: Int;
  /**
   * The combination as registered.
   * @zh 注册时的组合。
   */
  key: string;
  /**
   * The action name.
   * @zh 动作名。
   */
  action: string;
  /**
   * The `global` flag given at registration; `false` for a shortcut.
   * @zh 注册时给的 `global`；快捷键为 `false`。
   */
  global: boolean;
}

interface GetRegisteredHotkeysResult {
  /**
   * Hotkeys first, then shortcuts.
   * @zh 先热键，后快捷键。
   */
  hotkeys: KeyboardHotkey[];
}
