import type { Int } from './common.js';

export interface Api {
  /**
   * Register a system-wide hotkey on the main window. When it is pressed the registering
   * window receives `keyboard:hotkey` with `{ id, key, action }`. Windows refuses a combination
   * another program already holds.
   * @zh 在主窗口句柄上注册系统级全局热键；按下该热键时向发起注册的窗口派发 `keyboard:hotkey` 事件（载荷为 `{ id, key, action }`）；若按键组合已被系统中其他程序占用，Windows 系统将拒绝注册
   */
  registerHotkey(params: RegisterHotkeyParams): RegisterHotkeyResult;

  /**
   * Store a key combination as a WebView-local shortcut under the given action name. It is
   * listed by `getRegisteredHotkeys` with `id` `0` and removed by `unregisterHotkey` with the
   * key.
   * @zh 登记 WebView 页面作用域内的应用内快捷键（绑定至指定动作名；通过 `getRegisteredHotkeys` 列出时 `id` 固定为 `0`，注销时通过 `unregisterHotkey` 传入 key 字符串移除）
   */
  registerShortcut(params: RegisterShortcutParams): void;

  /**
   * Remove a hotkey by its id, or a hotkey or shortcut by its key string; exactly one of the
   * two must be given.
   * @zh 注销已注册的热键或应用内快捷键（支持通过数字 `id` 注销热键，或通过按键组合 `key` 字符串注销热键或快捷键；两者必须恰好提供一项）
   */
  unregisterHotkey(params: UnregisterHotkeyParams): void;

  /**
   * List the registered hotkeys followed by the shortcuts.
   * @zh 获取已注册的全部全局热键及应用内快捷键列表（先列出系统热键，后列出应用内快捷键）
   */
  getRegisteredHotkeys(): GetRegisteredHotkeysResult;
}

export interface Events {
  /**
   * A hotkey registered with `keyboard.registerHotkey` was pressed. It goes to the window that
   * registered it. A hotkey stays registered after that window closes; its presses then go to
   * the main window's page.
   * @zh 全局热键触发事件；定向投递给发起注册该热键的窗口页面（若注册窗口已关闭，热键仍然保持注册有效，后续触发将重定向投递至主窗口页面）
   * @delivery owner
   */
  hotkey: HotkeyPayload;
}

interface HotkeyPayload {
  /**
   * The id `keyboard.registerHotkey` returned.
   * @zh `keyboard.registerHotkey` 注册成功时返回的热键数值标识符 id
   */
  id: Int;
  /**
   * The combination as it was registered.
   * @zh 注册时传入的按键组合字符串 key
   */
  key: string;
  /**
   * The action name given at registration.
   * @zh 注册时绑定的动作名称 action
   */
  action: string;
}

/**
 * A key combination: modifiers `Ctrl` / `Control`, `Alt`, `Shift`, `Win` joined with `+` to a
 * key name (a letter, a digit, `F1` to `F12`, `Space`, `Enter`, `Tab`, `Escape`, `Backspace`,
 * `Delete`, `Insert`, `Home`, `End`, `PageUp`, `PageDown`, the arrow keys, the media keys
 * `PlayPause` / `MediaStop` / `NextTrack` / `PrevTrack` / `VolumeUp` / `VolumeDown` /
 * `VolumeMute`, or one of the punctuation keys). Case does not matter.
 * @zh 按键组合定义：支持修饰键（`Ctrl` / `Control`、`Alt`、`Shift`、`Win`）以 `+` 连接键名（单字母、数字、`F1` 到 `F12`、`Space`、`Enter`、`Tab`、`Escape`、`Backspace`、`Delete`、`Insert`、`Home`、`End`、`PageUp`、`PageDown`、方向键、媒体控制键 `PlayPause` / `MediaStop` / `NextTrack` / `PrevTrack` / `VolumeUp` / `VolumeDown` / `VolumeMute`，或标点符号键；大小写不敏感）
 */
type KeyCombination = string;

interface RegisterHotkeyParams {
  /**
   * The combination to register.
   * @zh 待注册的按键组合字符串
   * @minLength 1
   */
  key: KeyCombination;
  /**
   * Action name reported back in the `keyboard:hotkey` payload.
   * @zh 触发热键时随 `keyboard:hotkey` 事件载荷回传的动作名称
   * @minLength 1
   */
  action: string;
  /**
   * Recorded on the entry and reported by `getRegisteredHotkeys`; the hotkey is registered
   * system-wide either way.
   * @zh 是否标记为全局热键（记录于条目属性中供 `getRegisteredHotkeys` 查询，实际均注册为系统级热键）
   * @default true
   */
  global?: boolean;
}

interface RegisterHotkeyResult {
  /**
   * Id of the hotkey, from `1`; the key `unregisterHotkey` takes.
   * @zh 分配的热键唯一标识符（自 `1` 开始递增，注销时可传给 `unregisterHotkey`）
   */
  id: Int;
}

interface RegisterShortcutParams {
  /**
   * The combination to store.
   * @zh 待登记的按键组合字符串
   * @minLength 1
   */
  key: KeyCombination;
  /**
   * Action name stored with the shortcut.
   * @zh 与快捷键关联绑定的动作名称
   * @minLength 1
   */
  action: string;
}

interface UnregisterHotkeyParams {
  /**
   * Id from `registerHotkey`.
   * @zh 待注销热键的数值标识符 id（取自 `registerHotkey` 的返回值）
   * @minimum 1
   */
  id?: Int;
  /**
   * The key string the hotkey or shortcut was registered with.
   * @zh 待注销热键或快捷键的原始按键组合字符串 key
   */
  key?: KeyCombination;
}

/**
 * One registered entry.
 * @zh 已注册的热键或应用内快捷键条目数据结构
 */
interface KeyboardHotkey {
  /**
   * Hotkey id; `0` for a shortcut.
   * @zh 热键标识符 id（系统级全局热键大于等于 1，应用内快捷键固定为 `0`）
   */
  id: Int;
  /**
   * The combination as registered.
   * @zh 注册时配置的按键组合字符串
   */
  key: string;
  /**
   * The action name.
   * @zh 关联绑定的动作名称
   */
  action: string;
  /**
   * The `global` flag given at registration; `false` for a shortcut.
   * @zh 是否为全局热键（全局热键为 true，应用内快捷键为 false）
   */
  global: boolean;
}

interface GetRegisteredHotkeysResult {
  /**
   * Hotkeys first, then shortcuts.
   * @zh 已注册条目列表（系统级全局热键在前，应用内快捷键在后）
   */
  hotkeys: KeyboardHotkey[];
}
