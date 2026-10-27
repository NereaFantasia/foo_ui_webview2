import type { Int } from './common.js';

export interface Api {
  /**
   * Show a native popup menu at the system cursor position and wait for the choice. The
   * chosen row is reported as `selectedId` and also announced to the calling window as
   * `ui:menuItemClicked` with `{ id, label }`; dismissing the menu reports `selectedId: null`.
   * @zh 在系统光标位置弹出原生上下文菜单并阻塞等待用户选择；选中菜单项时返回对应 `selectedId` 并向发起窗口定向投递 `ui:menuItemClicked` 事件（载荷为 `{ id, label }`）；用户取消或点击外部关闭菜单时返回 `selectedId: null`
   */
  showCustomMenu(params: ShowCustomMenuParams): ShowCustomMenuResult;

  /**
   * Ask the calling window's page to show a toast: nothing is painted natively, the payload is
   * delivered to that window as `ui:toast` with `{ message, duration, type, position }`.
   * @zh 向发起调用窗口投递 `ui:toast` 事件以触发前端 Toast 提示（宿主不负责原生绘制，由前端页面接收 `{ message, duration, type, position }` 自行渲染）
   */
  showToast(params: ShowToastParams): void;

  /**
   * Show a Windows balloon notification from the plugin's notification icon, creating the icon
   * on first use. At least one of `title` and `body` must be given.
   * @zh 通过插件托盘通知图标弹出 Windows 系统气泡通知（首次调用自动创建托盘图标；`title` 与 `body` 必须至少提供一项）
   */
  showNotification(params: ShowNotificationParams): ShowNotificationResult;

  /**
   * Hide the balloon shown by `showNotification`; nothing to do when none was shown.
   * @zh 隐藏当前由 `ui.showNotification` 弹出的系统气泡通知（当前无活动通知时不执行任何操作）
   */
  hideNotification(): void;

  /**
   * Open the main window's own context menu. Coordinates that are omitted, not positive, or
   * more than 50 px away from the real cursor are replaced by the cursor position.
   * @zh 弹出 foobar2000 主窗口上下文菜单（省略坐标、传入非正数或传入坐标与实际光标距离超过 50 像素时，自动纠正为当前系统光标位置以适配高 DPI 场景）
   */
  showContextMenu(params: ShowContextMenuParams): void;
}

export interface Events {
  /**
   * A row of a menu opened with `ui.showCustomMenu` was chosen. The page that opened the menu
   * gets this in addition to the method's `selectedId`; dismissing the menu sends nothing.
   * @zh `ui.showCustomMenu` 原生菜单项选中事件；定向投递至发起菜单调用的页面窗口（附带选中的 `{ id, label }`；用户取消菜单时不触发）
   * @delivery caller
   */
  menuItemClicked: MenuItemClickedPayload;

  /**
   * `ui.showToast` asks the calling page to show a toast. The host draws nothing; the page
   * renders it from these fields.
   * @zh 由 `ui.showToast` 触发的 Toast 通知渲染请求；定向投递给发起调用窗口，由前端按载荷参数自行实现视觉呈现
   * @delivery caller
   */
  toast: ToastPayload;

  /**
   * The colours of foobar2000's Default UI changed. Only pages in this plugin's Default UI
   * panels receive it, together with `system:themeChanged`; Columns UI panels and the plugin's
   * own windows do not.
   * @zh foobar2000 Default UI 宿主配色方案变更事件；仅投递给承载于 Default UI 面板中的页面（与 `system:themeChanged` 联动派发；Columns UI 面板与插件独立弹窗不接收此事件）
   * @delivery window
   */
  coloursChanged: void;

  /**
   * The fonts of foobar2000's Default UI changed. Only pages in this plugin's Default UI panels
   * receive it; Columns UI panels and the plugin's own windows do not.
   * @zh foobar2000 Default UI 宿主字体配置变更事件；仅投递给承载于 Default UI 面板中的页面（Columns UI 面板与插件独立弹窗不接收此事件）
   * @delivery window
   */
  fontChanged: void;
}

interface MenuItemClickedPayload {
  /**
   * The row's `id`; `""` for a row without one.
   * @zh 被点击菜单项的标识符 id（未配置 id 时为空字符串 `""`）
   */
  id: string;
  /**
   * The row's `label` as given, without the shortcut hint; `""` for a row without one.
   * @zh 被点击菜单项的显示文本（不含右侧快捷键提示；未配置 label 时为空字符串 `""`）
   */
  label: string;
}

interface ToastPayload {
  /**
   * Toast text; never empty.
   * @zh Toast 提示正文文本内容
   */
  message: string;
  /**
   * Display time in milliseconds as given, `3000` when omitted. It is not range-checked and can
   * be `0` or negative.
   * @zh 提示显示时长（毫秒；未指定时默认 3000，不执行范围校验）
   */
  duration: Int;
  /**
   * Toast kind; `info` when omitted.
   * @zh Toast 提示类型：info、success、warning 或 error（未指定时默认 info）
   */
  type: 'info' | 'success' | 'warning' | 'error';
  /**
   * Screen corner as given and not checked; `bottom-right` when omitted.
   * @zh 提示在屏幕中的对齐角落位置（未指定时默认 "bottom-right"）
   */
  position: string;
}

/**
 * One row of a custom menu.
 * @zh 自定义原生弹出菜单的单个条目数据结构
 */
interface UiMenuItem {
  /**
   * Identifier reported as `selectedId` and in `ui:menuItemClicked`.
   * @zh 菜单项唯一标识符（选中时通过 selectedId 与 ui:menuItemClicked 上报）
   */
  id?: string;
  /**
   * Display text.
   * @zh 菜单项显示文本内容
   */
  label?: string;
  /**
   * `separator` draws a separator; any other value is an ordinary row.
   * @zh 条目类型：设置为 "separator" 渲染为原生菜单分隔线，其他值均为普通可点击条目
   * @default "item"
   */
  type?: string;
  /**
   * Whether the row can be picked.
   * @zh 菜单项是否处于可用状态（为 false 时呈现为置灰禁用）
   * @default true
   */
  enabled?: boolean;
  /**
   * Whether the row shows a checkmark.
   * @zh 菜单项左侧是否显示勾选标记
   * @default false
   */
  checked?: boolean;
  /**
   * Shortcut hint drawn right-aligned after the label.
   * @zh 渲染在菜单项右侧的快捷键提示文本（如 "Ctrl+P"）
   */
  shortcut?: string;
  /**
   * Child rows; a row with this key opens a submenu instead of being picked.
   * @zh 级联子菜单条目列表（包含子条目时该项表现为级联展开菜单，不可直接点击选中）
   */
  submenu?: UiMenuItem[];
}

interface ShowCustomMenuParams {
  /**
   * The rows.
   * @zh 待弹出的原生菜单条目树结构数组
   */
  items: UiMenuItem[];
  /**
   * Accepted for compatibility; the menu opens at the system cursor.
   * @zh 兼容保留参数（原生菜单固定在当前系统光标位置弹出）
   * @default 0
   */
  x?: Int;
  /**
   * Accepted for compatibility; the menu opens at the system cursor.
   * @zh 兼容保留参数（原生菜单固定在当前系统光标位置弹出）
   * @default 0
   */
  y?: Int;
  /**
   * With `h`, the size of a rectangle below the cursor the menu must not cover; the menu then
   * opens below it.
   * @zh 与 h 搭配指定光标下方菜单避让矩形区域的宽度（菜单将在避让区域下方或上方弹出）
   * @default 0
   */
  w?: Int;
  /**
   * See `w`.
   * @zh 与 w 搭配指定光标下方菜单避让矩形区域的高度（菜单将在避让区域下方或上方弹出）
   * @default 0
   */
  h?: Int;
  /**
   * Accepted for compatibility; has no effect.
   * @zh 兼容保留参数（已废弃无实际效果）
   * @default false
   */
  suppressDefault?: boolean;
}

interface ShowCustomMenuResult {
  /**
   * Id of the chosen row; `null` when the menu was dismissed.
   * @zh 用户选中的菜单项标识符 id；用户取消菜单或点击外部区域关闭时为 null
   */
  selectedId: string | null;
}

interface ShowToastParams {
  /**
   * Toast text.
   * @zh Toast 提示正文文本内容
   * @minLength 1
   */
  message: string;
  /**
   * Display time in milliseconds.
   * @zh 提示显示时长（毫秒）
   * @default 3000
   */
  duration?: Int;
  /**
   * Toast kind, passed through to the page.
   * @zh Toast 提示类型：info、success、warning 或 error
   * @default "info"
   */
  type?: 'info' | 'success' | 'warning' | 'error';
  /**
   * Screen corner, passed through to the page.
   * @zh 提示在屏幕中的对齐角落位置（默认 "bottom-right"）
   * @default "bottom-right"
   */
  position?: string;
}

interface ShowNotificationParams {
  /**
   * Notification title.
   * @zh 系统气泡通知标题文本
   * @default ""
   */
  title?: string;
  /**
   * Notification body.
   * @zh 系统气泡通知正文文本
   * @default ""
   */
  body?: string;
  /**
   * `true` shows it without the notification sound.
   * @zh 是否静音弹出通知（为 true 时不播放系统提示音）
   * @default false
   */
  silent?: boolean;
  /**
   * Display time in milliseconds, as a hint to the shell.
   * @zh 通知建议展示时长（毫秒，作为提示提供给系统 Shell）
   * @default 5000
   */
  timeout?: Int;
}

interface ShowNotificationResult {
  /**
   * Sequence number of the notification, from `1`.
   * @zh 系统通知自增序号标识符（从 1 开始递增）
   */
  id: Int;
}

interface ShowContextMenuParams {
  /**
   * Screen x in pixels.
   * @zh 目标屏幕横坐标 X（像素；缺省或传入 -1 时自动采用当前鼠标光标横坐标）
   * @default -1
   */
  x?: Int;
  /**
   * Screen y in pixels.
   * @zh 目标屏幕纵坐标 Y（像素；缺省或传入 -1 时自动采用当前鼠标光标纵坐标）
   * @default -1
   */
  y?: Int;
}
