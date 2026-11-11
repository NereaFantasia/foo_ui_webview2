import type { Int } from './common.js';

export interface Api {
  /**
   * Register a system-wide hotkey. When it is pressed the registering window receives
   * `keyboard:hotkey` with `{ id, key, action }`, whichever program has the focus. The
   * combination must hold Ctrl, Alt or Win, except a media key alone; Ctrl+A, C, V, X, Y, Z,
   * Ctrl+Shift+Z, Alt+F4, Alt+Tab, Alt+Space and Ctrl+Esc are refused too, all with
   * `INVALID_PARAMS`. A combination a window of this component already holds fails with
   * `OPERATION_FAILED` naming that window; one Windows refuses, because another program or a
   * global foobar2000 shortcut holds it, fails with `OPERATION_FAILED` and the Windows error in
   * `details.win32Error`. The hotkey is released when the registering page navigates or reloads
   * and when its window closes.
   * @zh 注册系统级全局热键：无论焦点在哪个程序，按下时都向发起注册的窗口派发 `keyboard:hotkey`（载荷为 `{ id, key, action }`）。组合须含 Ctrl、Alt 或 Win，单按媒体键除外；Ctrl+A、C、V、X、Y、Z，Ctrl+Shift+Z，Alt+F4、Alt+Tab、Alt+Space 与 Ctrl+Esc 也会被拒绝，均返回 `INVALID_PARAMS`。本组件的某个窗口已持有该组合时以 `OPERATION_FAILED` 失败，错误信息写明持有的窗口；被其他程序或 foobar2000 的全局快捷键占用、Windows 拒绝注册时同样返回 `OPERATION_FAILED`，`details.win32Error` 为 Windows 错误码。注册页面导航或重载、所在窗口关闭时，热键随之注销
   */
  registerHotkey(params: RegisterHotkeyParams): RegisterHotkeyResult;

  /**
   * Store a key combination under the given action name. Nothing matches key presses against
   * stored shortcuts. It is listed by `getRegisteredHotkeys` with `id` `0` and removed by
   * `unregisterHotkey` with the key.
   * @zh 以指定动作名登记一个按键组合，但不会有任何代码按它匹配按键。通过 `getRegisteredHotkeys` 列出时 `id` 为 `0`，用 `unregisterHotkey` 传入 key 字符串移除
   * @deprecated Has no effect and will be removed in 3.0; listen for `keydown` in the page instead.
   * @deprecatedZh 没有任何效果，3.0 移除；请改为在页面里监听 `keydown`。
   */
  registerShortcut(params: RegisterShortcutParams): void;

  /**
   * Remove a hotkey by its id, or a hotkey or shortcut by its key string; exactly one of the
   * two must be given. Removing a shortcut goes away with `registerShortcut` in 3.0.
   * @zh 注销已注册的热键或应用内快捷键（支持通过数字 `id` 注销热键，或通过按键组合 `key` 字符串注销热键或快捷键；两者必须恰好提供一项）。注销快捷键的用法随 `registerShortcut` 在 3.0 移除
   */
  unregisterHotkey(params: UnregisterHotkeyParams): void;

  /**
   * List the registered hotkeys followed by the shortcuts. Shortcut entries go away with
   * `registerShortcut` in 3.0.
   * @zh 获取已注册的全部全局热键及应用内快捷键列表（先列出系统热键，后列出应用内快捷键）。应用内快捷键条目随 `registerShortcut` 在 3.0 移除
   */
  getRegisteredHotkeys(): GetRegisteredHotkeysResult;

  /**
   * Replace the calling page's reserved keys. While the page has the focus, the keys selected in the
   * component preferences run the foobar2000 shortcuts bound to them; a reserved combination stays
   * with the page even when foobar2000 has a shortcut for it. The list is cleared when the page
   * navigates or reloads and when its window closes. A key string that does not parse fails with
   * `INVALID_PARAMS` and leaves the list unchanged; passing an empty list releases all reserved keys.
   * With `all`, every key stays with the page, for example while the page records a shortcut;
   * F12 in development server mode and system-wide hotkeys are excluded, and F5 reloads the page
   * unless a `keydown` listener calls `preventDefault()`.
   * @zh 全量替换当前页面的保留按键列表。页面聚焦时，组件首选项选定的按键执行 foobar2000「首选项 > 键盘快捷键」中为其绑定的快捷键；列表中指定的保留键始终优先由页面自身接收处理。保留列表在页面导航、重新加载或所在窗口关闭时清空。若存在无法解析的按键字符串，调用以 `INVALID_PARAMS` 失败且现有列表保持不变；传入空数组可释放全部保留键。设置 `all: true` 时所有按键均由页面接收（如快捷键录制场景）；系统级热键与开发服务器模式下的 F12 快捷键除外。页面的 `keydown` 监听没有调用 `preventDefault()` 时，F5 重载页面。
   */
  setReservedKeys(params: SetReservedKeysParams): void;
}

export interface Events {
  /**
   * A hotkey registered with `keyboard.registerHotkey` was pressed. It goes to the window that
   * registered it.
   * @zh 全局热键触发事件，定向投递给发起注册的窗口页面
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

interface SetReservedKeysParams {
  /**
   * Combinations to keep for the page, written like `registerHotkey` keys; duplicates are fine.
   * @zh 留给页面的按键组合，写法与 `registerHotkey` 的 key 相同；重复项无妨
   */
  keys: KeyCombination[];
  /**
   * Keep every key for the page, not only `keys`. A later call without it keeps only `keys` again.
   * @zh 是否把全部按键都留给页面，而不只是 `keys`。之后再调用一次不带它的，就回到只保留 `keys`
   * @default false
   */
  all?: boolean;
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
  /**
   * Id of the window that registered the hotkey, the one its presses go to. Absent for a
   * shortcut, and for a hotkey whose caller could not be matched to a window.
   * @zh 注册该热键的窗口 id，即按下时事件投递到的窗口。应用内快捷键的条目，以及调用方匹配不到窗口的热键，应答里没有此字段
   */
  windowId?: string;
}

interface GetRegisteredHotkeysResult {
  /**
   * Hotkeys first, then shortcuts.
   * @zh 已注册条目列表（系统级全局热键在前，应用内快捷键在后）
   */
  hotkeys: KeyboardHotkey[];
}
