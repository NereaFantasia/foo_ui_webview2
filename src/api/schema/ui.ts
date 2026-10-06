import type { Int } from './common.js';

export interface Api {
  /**
   * Show a native popup menu at the system cursor position and wait for the choice. The
   * chosen row is reported as `selectedId` and also announced to the calling window as
   * `ui:menuItemClicked` with `{ id, label }`; dismissing the menu reports `selectedId: null`.
   * @zh 在系统光标处弹出原生菜单并等待选择。选中的行以 `selectedId` 报回，并向调用窗口发 `ui:menuItemClicked`（`{ id, label }`）；菜单被取消时 `selectedId` 为 `null`。
   */
  showCustomMenu(params: ShowCustomMenuParams): ShowCustomMenuResult;

  /**
   * Ask the calling window's page to show a toast: nothing is painted natively, the payload is
   * delivered to that window as `ui:toast` with `{ message, duration, type, position }`.
   * @zh 让调用窗口的页面显示一条 toast：宿主不绘制，只把载荷以 `ui:toast`（`{ message, duration, type, position }`）发给该窗口。
   */
  showToast(params: ShowToastParams): void;

  /**
   * Show a Windows balloon notification from the plugin's notification icon, creating the icon
   * on first use. At least one of `title` and `body` must be given.
   * @zh 从插件的通知图标显示一条 Windows 气泡通知，首次使用时创建图标。`title` 与 `body` 至少给一个。
   */
  showNotification(params: ShowNotificationParams): ShowNotificationResult;

  /**
   * Hide the balloon shown by `showNotification`; nothing to do when none was shown.
   * @zh 隐藏 `showNotification` 显示的气泡；没显示过时什么也不做。
   */
  hideNotification(): void;

  /**
   * Open the main window's own context menu. Coordinates that are omitted, not positive, or
   * more than 50 px away from the real cursor are replaced by the cursor position.
   * @zh 打开主窗口自己的上下文菜单。省略、非正数或与真实光标相差超过 50 px 的坐标改用光标位置。
   */
  showContextMenu(params: ShowContextMenuParams): void;
}

export interface Events {
  /**
   * A row of a menu opened with `ui.showCustomMenu` was chosen. The page that opened the menu
   * gets this in addition to the method's `selectedId`; dismissing the menu sends nothing.
   * @zh `ui.showCustomMenu` 打开的菜单里某一行被选中。打开菜单的页面除了拿到方法返回的 `selectedId`，还会收到此事件；取消菜单时不发。
   * @delivery caller
   */
  menuItemClicked: MenuItemClickedPayload;

  /**
   * `ui.showToast` asks the calling page to show a toast. The host draws nothing; the page
   * renders it from these fields.
   * @zh `ui.showToast` 请调用它的页面显示一条 toast。宿主不绘制，由页面按这些字段渲染。
   * @delivery caller
   */
  toast: ToastPayload;

  /**
   * The colours of foobar2000's Default UI changed. Only pages in this plugin's Default UI
   * panels receive it, together with `system:themeChanged`; Columns UI panels and the plugin's
   * own windows do not.
   * @zh foobar2000 默认界面（Default UI）的配色变了。只有本插件 Default UI 面板里的页面会收到，与 `system:themeChanged` 一起发；Columns UI 面板与插件自己的窗口收不到。
   * @delivery window
   */
  coloursChanged: void;

  /**
   * The fonts of foobar2000's Default UI changed. Only pages in this plugin's Default UI panels
   * receive it; Columns UI panels and the plugin's own windows do not.
   * @zh foobar2000 默认界面（Default UI）的字体变了。只有本插件 Default UI 面板里的页面会收到；Columns UI 面板与插件自己的窗口收不到。
   * @delivery window
   */
  fontChanged: void;
}

interface MenuItemClickedPayload {
  /**
   * The row's `id`; `""` for a row without one.
   * @zh 该行的 `id`；没有 id 的行为 `""`。
   */
  id: string;
  /**
   * The row's `label` as given, without the shortcut hint; `""` for a row without one.
   * @zh 该行的 `label` 原文，不含快捷键提示；没有 label 的行为 `""`。
   */
  label: string;
}

interface ToastPayload {
  /**
   * Toast text; never empty.
   * @zh toast 文本，不为空。
   */
  message: string;
  /**
   * Display time in milliseconds as given, `3000` when omitted. It is not range-checked and can
   * be `0` or negative.
   * @zh 显示时长（毫秒），原样转交，省略时为 `3000`。不检查范围，可能为 `0` 或负数。
   */
  duration: Int;
  /**
   * Toast kind; `info` when omitted.
   * @zh toast 种类；省略时为 `info`。
   */
  type: 'info' | 'success' | 'warning' | 'error';
  /**
   * Screen corner as given and not checked; `bottom-right` when omitted.
   * @zh 屏幕角落，原样转交、不做检查；省略时为 `bottom-right`。
   */
  position: string;
}

/**
 * One row of a custom menu.
 * @zh 自定义菜单的一行。
 */
interface UiMenuItem {
  /**
   * Identifier reported as `selectedId` and in `ui:menuItemClicked`.
   * @zh 以 `selectedId` 与 `ui:menuItemClicked` 报回的标识。
   */
  id?: string;
  /**
   * Display text.
   * @zh 显示文本。
   */
  label?: string;
  /**
   * `separator` draws a separator; any other value is an ordinary row.
   * @zh `separator` 画分隔线；其他值都是普通行。
   * @default "item"
   */
  type?: string;
  /**
   * Whether the row can be picked.
   * @zh 是否可选。
   * @default true
   */
  enabled?: boolean;
  /**
   * Whether the row shows a checkmark.
   * @zh 是否显示勾选标记。
   * @default false
   */
  checked?: boolean;
  /**
   * Shortcut hint drawn right-aligned after the label.
   * @zh 画在文字右侧的快捷键提示。
   */
  shortcut?: string;
  /**
   * Child rows; a row with this key opens a submenu instead of being picked.
   * @zh 子行；带这个键的行展开子菜单而不能被选中。
   */
  submenu?: UiMenuItem[];
}

interface ShowCustomMenuParams {
  /**
   * The rows.
   * @zh 各行。
   */
  items: UiMenuItem[];
  /**
   * Accepted for compatibility; the menu opens at the system cursor.
   * @zh 兼容保留；菜单固定在系统光标处弹出。
   * @default 0
   */
  x?: Int;
  /**
   * Accepted for compatibility; the menu opens at the system cursor.
   * @zh 兼容保留；菜单固定在系统光标处弹出。
   * @default 0
   */
  y?: Int;
  /**
   * With `h`, the size of a rectangle below the cursor the menu must not cover; the menu then
   * opens below it.
   * @zh 与 `h` 一起给出光标下方一块菜单不得遮盖的矩形；菜单在其下方弹出。
   * @default 0
   */
  w?: Int;
  /**
   * See `w`.
   * @zh 见 `w`。
   * @default 0
   */
  h?: Int;
  /**
   * Accepted for compatibility; has no effect.
   * @zh 兼容保留；没有作用。
   * @default false
   */
  suppressDefault?: boolean;
}

interface ShowCustomMenuResult {
  /**
   * Id of the chosen row; `null` when the menu was dismissed.
   * @zh 选中行的 id；菜单被取消时为 `null`。
   */
  selectedId: string | null;
}

interface ShowToastParams {
  /**
   * Toast text.
   * @zh toast 文本。
   * @minLength 1
   */
  message: string;
  /**
   * Display time in milliseconds.
   * @zh 显示时长，单位毫秒。
   * @default 3000
   */
  duration?: Int;
  /**
   * Toast kind, passed through to the page.
   * @zh toast 种类，原样传给页面。
   * @default "info"
   */
  type?: 'info' | 'success' | 'warning' | 'error';
  /**
   * Screen corner, passed through to the page.
   * @zh 屏幕角落，原样传给页面。
   * @default "bottom-right"
   */
  position?: string;
}

interface ShowNotificationParams {
  /**
   * Notification title.
   * @zh 通知标题。
   * @default ""
   */
  title?: string;
  /**
   * Notification body.
   * @zh 通知正文。
   * @default ""
   */
  body?: string;
  /**
   * `true` shows it without the notification sound.
   * @zh `true` 不播放通知提示音。
   * @default false
   */
  silent?: boolean;
  /**
   * Display time in milliseconds, as a hint to the shell.
   * @zh 显示时长，单位毫秒，供 shell 参考。
   * @default 5000
   */
  timeout?: Int;
}

interface ShowNotificationResult {
  /**
   * Sequence number of the notification, from `1`.
   * @zh 通知的序号，从 `1` 起。
   */
  id: Int;
}

interface ShowContextMenuParams {
  /**
   * Screen x in pixels.
   * @zh 屏幕 x，单位像素。
   * @default -1
   */
  x?: Int;
  /**
   * Screen y in pixels.
   * @zh 屏幕 y，单位像素。
   * @default -1
   */
  y?: Int;
}
