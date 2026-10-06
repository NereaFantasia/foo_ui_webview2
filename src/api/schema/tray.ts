import type { Int } from './common.js';

export interface Api {
  /**
   * Create the tray icon. Call it once before any other `tray.*` method: without it the
   * other calls succeed but no icon shows, no tray event fires and `isVisible` reports `false`.
   * Fails in panel mode, when the main window has no handle, and when the shell refuses the
   * icon.
   * @zh 创建托盘图标。其他 `tray.*` 方法之前先调用一次：没创建时它们照样成功返回，但图标不显示、托盘事件不触发、`isVisible` 报 `false`。面板模式、主窗口没有句柄、shell 拒绝注册图标时失败。
   */
  create(params: CreateParams): void;

  /**
   * Remove the tray icon.
   * @zh 移除托盘图标。
   */
  destroy(): void;

  /**
   * Replace the tray icon image; fails while no icon is created.
   * @zh 更换托盘图标图像；图标未创建时失败。
   */
  setIcon(params: SetIconParams): void;

  /**
   * Update the icon's hover text; fails while no icon is created.
   * @zh 更新托盘图标的悬停提示文本；图标未创建时失败。
   */
  setTooltip(params: SetTooltipParams): void;

  /**
   * Show a balloon notification from the tray icon; fails while no icon is created.
   * @zh 从托盘图标显示气泡通知；图标未创建时失败。
   */
  showBalloon(params: ShowBalloonParams): void;

  /**
   * Replace the user rows of one menu zone (`config.customPosition`, default `top`); the
   * other zones are left as they are. Nothing is stored when any row is rejected or the
   * resource limits are exceeded. An ordinary row reports its click through
   * `tray:menuItemClicked`; the built-in playback and system rows, and rows declaring
   * `playbackAction`, run natively and do not. To replace every zone at once, use
   * `setMenuZones`.
   * @zh 整体替换某一区（`config.customPosition`，默认 `top`）的用户行，其余区不动。任一行被拒或超出资源上限时什么都不存。普通行点击经 `tray:menuItemClicked` 上报；内置播放/系统行与声明了 `playbackAction` 的行由插件原生执行，不发该事件。要一次替换全部分区，用 `setMenuZones`。
   */
  setContextMenu(params: SetContextMenuParams): void;

  /**
   * Replace the user rows of all three zones in one call; a zone that is not given is cleared.
   * Rows and `config` follow the rules of `setContextMenu`, and nothing is stored, `config`
   * included, when any row is rejected or the resource limits are exceeded. Use it when the
   * whole menu changes, instead of `clearMenuItems` followed by `appendMenuItems`: the menu
   * can open between separate calls and show only part of the update, and two such updates
   * that interleave add their rows twice.
   * @zh 一次替换三个区的全部用户行，没给出的区清空。行与 `config` 的规则同 `setContextMenu`；任一行被拒或超出资源上限时什么都不存，`config` 也不改。整份菜单要变时用它，不要先 `clearMenuItems` 再 `appendMenuItems`：分开的几次调用之间菜单可能被打开，只显示更新的一部分；两次这样的更新交错时，行会被加两遍。
   */
  setMenuZones(params: SetMenuZonesParams): void;

  /**
   * Append rows to one menu zone; the same validation as `setContextMenu`.
   * @zh 向某一区追加行；校验规则同 `setContextMenu`。
   */
  appendMenuItems(params: AppendMenuItemsParams): void;

  /**
   * Remove rows by id from every zone, submenus included.
   * @zh 按 id 从所有区（含子菜单）移除行。
   */
  removeMenuItems(params: RemoveMenuItemsParams): RemoveMenuItemsResult;

  /**
   * Clear one zone, or every zone when `position` is omitted.
   * @zh 清空某一区；省略 `position` 时清空全部三区。
   */
  clearMenuItems(params: ClearMenuItemsParams): void;

  /**
   * List the user rows of every zone, flattened in `top`, `playback`, `bottom` order, as they
   * were stored. The rows the runtime injects for `showPlaybackControls` and
   * `showSystemItems` are not included.
   * @zh 按 `top`、`playback`、`bottom` 的顺序平铺列出各区存储的用户行；运行时为 `showPlaybackControls` / `showSystemItems` 注入的行不在其中。
   */
  getMenuItems(): GetMenuItemsResult;

  /**
   * Change one row's `checked` or `enabled` state in place, searching every zone and submenu
   * for the id. Giving `checked` (even `false`) makes the row checkable. The native menu is
   * rebuilt each time it opens, so the change shows on the next opening.
   * @zh 原地改一行的 `checked` / `enabled`，按 id 跨三区递归（含子菜单）查找。给了 `checked`（哪怕是 `false`）就把该行变成可勾选。原生菜单每次打开时重建，改动在下次打开时可见。
   */
  setMenuItemState(params: SetMenuItemStateParams): SetMenuItemStateResult;

  /**
   * Hide the window to the tray instead of the taskbar when it is minimized.
   * @zh 窗口最小化时隐藏到托盘而不是任务栏。
   */
  setMinimizeToTray(params: SetMinimizeToTrayParams): void;

  /**
   * Hide the window to the tray instead of quitting when it is closed.
   * @zh 关闭窗口时隐藏到托盘而不是退出。
   */
  setCloseToTray(params: SetCloseToTrayParams): void;

  /**
   * Report whether the tray icon exists.
   * @zh 报告托盘图标是否存在。
   */
  isVisible(): IsVisibleResult;
}

export interface Events {
  /**
   * The tray icon was clicked with the left button, on release. A right click opens the context
   * menu instead and sends `tray:beforeContextMenu`.
   * @zh 托盘图标被左键单击，在松开按键时发出。右键打开上下文菜单，发的是 `tray:beforeContextMenu`。
   * @delivery broadcast
   */
  click: ClickPayload;

  /**
   * The tray icon was double-clicked with the left button.
   * @zh 托盘图标被左键双击。
   * @delivery broadcast
   */
  doubleClick: CursorPositionPayload;

  /**
   * A row of the tray menu was chosen, or a rich control in it changed value. The built-in
   * playback and system rows, and rows that declare `playbackAction`, run natively and send
   * nothing. With `render: 'webview'` a rich control reports every change and the menu stays
   * open; the default native menu closes on each pick, offers a slider as five stops and shows
   * a segmented row as an ordinary row, which reports no `value`.
   * @zh 托盘菜单的某一行被选中，或其中的富控件改了值。内置的播放行、系统行以及声明了 `playbackAction` 的行由插件原生执行，不发此事件。`render: 'webview'` 时富控件每次改值都会上报，菜单保持打开；默认的原生菜单每次选择后都会关闭，滑块只提供五档，分段行按普通行显示，上报时不带 `value`。
   * @delivery broadcast
   */
  menuItemClicked: MenuItemClickedPayload;

  /**
   * The tray context menu is about to open on a right click. The menu does not wait for the
   * page, so changes a handler makes to it take effect from the next right click. It is sent
   * even when the menu has no visible row and nothing opens.
   * @zh 托盘上下文菜单即将在右键时打开。菜单不等页面，处理函数对菜单的改动从下一次右键起生效。菜单没有可见的行、什么也不弹出时同样会发。
   * @delivery broadcast
   */
  beforeContextMenu: MenuPositionPayload;
}

interface ClickPayload {
  /**
   * Always `0`, the left button.
   * @zh 恒为 `0`，即左键。
   */
  button: Int;
  /**
   * Cursor x in screen coordinates.
   * @zh 光标的屏幕横坐标。
   */
  x: Int;
  /**
   * Cursor y in screen coordinates.
   * @zh 光标的屏幕纵坐标。
   */
  y: Int;
}

interface CursorPositionPayload {
  /**
   * Cursor x in screen coordinates.
   * @zh 光标的屏幕横坐标。
   */
  x: Int;
  /**
   * Cursor y in screen coordinates.
   * @zh 光标的屏幕纵坐标。
   */
  y: Int;
}

interface MenuPositionPayload {
  /**
   * Screen x of the cursor, where the menu opens.
   * @zh 光标的屏幕横坐标，菜单在此打开。
   */
  x: Int;
  /**
   * Screen y of the cursor, where the menu opens.
   * @zh 光标的屏幕纵坐标，菜单在此打开。
   */
  y: Int;
}

interface MenuItemClickedPayload {
  /**
   * The row's `id`; `""` for a row without one.
   * @zh 该行的 `id`；没有 id 的行为 `""`。
   */
  id: string;
  /**
   * The new value of a rich control: stars `0` to `5` for `rating` (`0` clears), a value within
   * `min` to `max` for `slider`, the index of the chosen segment for `segmented`. Absent for
   * other rows.
   * @zh 富控件的新值：`rating` 为 `0` 到 `5` 星（`0` 为清除），`slider` 为 `min` 到 `max` 内的值，`segmented` 为选中分段的索引。其他行不带。
   */
  value?: Int;
}

/**
 * Raw Base64 of an `.ico` file, without a `data:` or `base64:` prefix. An empty or undecodable
 * value falls back to the foobar2000 main icon.
 * @zh 裸 Base64 编码的 `.ico` 文件字节，不带 `data:` 或 `base64:` 前缀；空或解码失败时回退到 foobar2000 主图标。
 */
type IcoBase64 = string;

/**
 * A menu zone: `top` and `bottom` hold user rows above and below the built-in playback rows of
 * the `playback` zone.
 * @zh 菜单分区：`top` 与 `bottom` 是内置播放行所在 `playback` 区上下的用户行区。
 */
type TrayMenuPosition = 'top' | 'playback' | 'bottom';

interface CreateParams {
  /**
   * Icon to show.
   * @zh 要显示的图标。
   */
  icon?: IcoBase64;
  /**
   * Hover text.
   * @zh 悬停提示文本。
   * @default "foobar2000"
   */
  tooltip?: string;
}

interface SetIconParams {
  /**
   * Icon to show.
   * @zh 要显示的图标。
   */
  icon?: IcoBase64;
}

interface SetTooltipParams {
  /**
   * Hover text; empty clears it.
   * @zh 悬停提示文本；空串即清除。
   * @default ""
   */
  tooltip?: string;
}

interface ShowBalloonParams {
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
  message?: string;
  /**
   * Icon shown in the balloon.
   * @zh 气泡里的图标。
   * @default "info"
   */
  icon?: 'info' | 'warning' | 'error';
}

/**
 * An inline monochrome SVG icon: the `viewBox` and the inner markup (for example
 * `<path d="..."/>`). Drawn by the `webview` backend only, through an allowlist of shape
 * elements and attributes; an illegal or oversized (over 32 KiB) icon is dropped and the row
 * is shown without it.
 * @zh 内联单色 SVG 图标：`viewBox` 与内部标记（如 `<path d="..."/>`）。只有 `webview` 后端绘制，经元素与属性白名单克隆；非法或超过 32 KiB 的图标被丢弃，该行不带图标继续显示。
 */
interface TrayIconSvg {
  /**
   * The SVG `viewBox` attribute.
   * @zh SVG 的 `viewBox` 属性。
   */
  viewBox: string;
  /**
   * The SVG inner markup.
   * @zh SVG 的内部标记。
   */
  content: string;
}

/**
 * One option of a `segmented` row.
 * @zh `segmented` 行的一个分段。
 */
interface TraySegment {
  /**
   * Text of the segment, shown when it has no icon.
   * @zh 分段文字；没有图标时显示。
   */
  label?: string;
  /**
   * Icon of the segment, preferred over the label.
   * @zh 分段图标；优先于文字。
   */
  iconSvg?: TrayIconSvg;
  /**
   * `false` greys the segment out so it cannot be picked.
   * @zh `false` 置灰，不可选。
   * @default true
   */
  enabled?: boolean;
}

/**
 * One tray menu row. The rich kinds (`nowplaying`, `rating`, `slider`, `segmented`) are fully
 * rendered by the `webview` backend only; the native backend degrades them (a disabled header
 * line, a 1 to 5 stars submenu, a stepped-level submenu, a plain text row).
 * @zh 一行托盘菜单。富类型（`nowplaying`、`rating`、`slider`、`segmented`）只有 `webview` 后端完整渲染；原生后端降级（灰色标题行、1 到 5 星子菜单、分级子菜单、纯文字行）。
 */
interface TrayMenuItem {
  /**
   * Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids
   * `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the
   * caller's label and skip the matching built-in injection.
   * @zh `tray:menuItemClicked` 上报的标识；分隔线可省略。精确的 `_sys_show` 与 `_sys_exit` 是原生的「显示主窗口」与「退出」行：保留调用方的文字，并跳过对应的内置注入。
   */
  id?: string;
  /**
   * Display text.
   * @zh 显示文本。
   */
  label?: string;
  /**
   * Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is
   * otherwise driven by `checked`.
   * @zh 行类型。`checkbox` 是「可勾选的 normal 行」的旧写法；勾选状态本身由 `checked` 决定。
   * @default "normal"
   */
  type?: 'normal' | 'separator' | 'checkbox' | 'submenu' | 'nowplaying' | 'rating' | 'slider' | 'segmented';
  /**
   * Whether the row can be clicked.
   * @zh 是否可点击。
   * @default true
   */
  enabled?: boolean;
  /**
   * Whether the row is shown.
   * @zh 是否显示。
   * @default true
   */
  visible?: boolean;
  /**
   * Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview`
   * backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the
   * row is not checkable.
   * @zh 勾选状态。给了这个键（含 `false`）该行就是可勾选的：`webview` 后端映射为 `menuitemcheckbox`，`getMenuItems` 会带回这个键。省略则不可勾选。
   */
  checked?: boolean;
  /**
   * Reserved; neither backend draws it. Use `iconSvg` for a row icon.
   * @zh 保留字段，两个后端都不绘制；行图标用 `iconSvg`。
   */
  icon?: string;
  /**
   * Icon drawn before the label by the `webview` backend. When any row of a menu level has a
   * renderable icon, every `normal` and `submenu` row of that level reserves the icon column.
   * @zh `webview` 后端画在文字前的图标。同一层只要有一行带可渲染图标，该层所有 `normal` 与 `submenu` 行都预留图标列。
   */
  iconSvg?: TrayIconSvg;
  /**
   * `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With
   * `config.autoNowPlaying` an empty value is filled from the playing track's front art
   * (`webview` only).
   * @zh `nowplaying` 行的封面：`data:` URL、`http(s)://` URL 或裸 Base64 JPEG。开了 `config.autoNowPlaying` 时留空由正在播放曲目的封面补上（仅 `webview`）。
   */
  cover?: string;
  /**
   * `nowplaying` first line, usually the track title; falls back to `label`.
   * @zh `nowplaying` 行的第一行，通常是曲名；缺省用 `label`。
   */
  title?: string;
  /**
   * `nowplaying` second line, usually artist or album.
   * @zh `nowplaying` 行的第二行，通常是艺术家或专辑。
   */
  subtitle?: string;
  /**
   * Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider`
   * (clamped), the selected index for `segmented`. Reported back only for those kinds.
   * @zh 当前值：`rating` 是 `0` 到 `5` 星，`slider` 是 `min` 到 `max` 内的值（会被夹取），`segmented` 是选中段的索引。只对这三种类型回报。
   */
  value?: Int;
  /**
   * `slider` range minimum; swapped with `max` when larger.
   * @zh `slider` 的下限；大于 `max` 时两者互换。
   */
  min?: Int;
  /**
   * `slider` range maximum.
   * @zh `slider` 的上限。
   */
  max?: Int;
  /**
   * `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom.
   * @zh `slider` 的方向（仅 `webview`）；省略即水平。纵向时 `min` 在底部。
   */
  orientation?: 'horizontal' | 'vertical';
  /**
   * `segmented` options, one row of mutually exclusive choices; `value` is the selected index.
   * Picking one reports `{ id, value }` and keeps the menu open.
   * @zh `segmented` 的分段，一行互斥选项；`value` 是选中段的索引。点选上报 `{ id, value }` 且菜单保持打开。
   */
  segments?: TraySegment[];
  /**
   * Child rows of a `submenu` row.
   * @zh `submenu` 行的子行。
   */
  submenu?: TrayMenuItem[];
  /**
   * A playback command the plugin runs natively when the row is picked, so it keeps working
   * while the page is suspended (minimized, hidden to the tray, session locked). Such a row does
   * not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails
   * the whole call.
   * @zh 该行被选中时由插件原生执行的播放命令，页面挂起（最小化、藏到托盘、锁屏）时仍可用；这样的行不发 `tray:menuItemClicked`。只能写在 `normal` 叶子上，写在别处整次调用失败。
   */
  playbackAction?: 'play-pause' | 'previous' | 'next' | 'stop';
}

/**
 * Menu-wide options. Every key is optional and only the keys given change the stored
 * configuration.
 * @zh 菜单级选项。每个键都可选，只有给了的键才改变存储的配置。
 */
interface TrayMenuConfig {
  /**
   * Inject the built-in previous, play/pause, next and stop rows into the `playback` zone.
   * @zh 向 `playback` 区注入内置的上一首、播放/暂停、下一首、停止行。默认 `true`。
   */
  showPlaybackControls?: boolean;
  /**
   * Inject the native show-main-window and exit rows into the `bottom` zone; both keep
   * working while the page is suspended.
   * @zh 向 `bottom` 区注入原生的「显示主窗口」与「退出」行；页面挂起时两者仍可用。默认 `true`。
   */
  showSystemItems?: boolean;
  /**
   * The zone `setContextMenu` writes its rows into.
   * @zh `setContextMenu` 把行写进哪个区。默认 `top`。
   */
  customPosition?: TrayMenuPosition;
  /**
   * Menu backend: the Win32 menu, or the self-drawn WebView2 overlay that renders the rich
   * row kinds and the styling options below.
   * @zh 菜单后端：Win32 菜单，或自绘的 WebView2 浮层（渲染富类型行与下面的样式选项）。默认 `native`。
   */
  render?: 'native' | 'webview';
  /**
   * Fill the empty `cover`, `title` and `subtitle` of `nowplaying` rows from the playing
   * track when the menu opens; a value given by the caller always wins. Art is downscaled to
   * 64 px and omitted above 256 KiB; `cover` filling is `webview` only.
   * @zh 菜单打开时用正在播放的曲目补 `nowplaying` 行留空的 `cover`、`title`、`subtitle`；调用方给的值优先。封面缩到 64 px，超过 256 KiB 省略；`cover` 只在 `webview` 下补。默认 `false`。
   */
  autoNowPlaying?: boolean;
  /**
   * Stylesheet injected into the `webview` menu on every opening, on top of the built-in
   * styles; target the menu's stable class names (`.fb-menu`, `.fb-item`, `.fb-sep`, ...).
   * @zh 每次打开 `webview` 菜单时注入的样式表，叠加在内置样式之上；按菜单的稳定 class 名（`.fb-menu`、`.fb-item`、`.fb-sep` 等）编写。
   */
  css?: string;
  /**
   * `true` disables the built-in styles so only `css` and the protected structural layer
   * apply (`webview` only).
   * @zh `true` 时停用内置样式，只留 `css` 与受保护的结构层（仅 `webview`）。默认 `false`。
   */
  cssReplace?: boolean;
  /**
   * DWM backdrop of the `webview` menu, the same vocabulary as the windows. It snaps in and out
   * with the window and cannot fade with CSS.
   * @zh `webview` 菜单的 DWM 背景材质，取值与窗口相同。随窗口瞬间出现/消失，不能用 CSS 淡入淡出。默认 `acrylic`。
   */
  backdrop?: 'acrylic' | 'mica' | 'mica-alt' | 'none';
  /**
   * Dark tint for the backdrop (`webview` only); `false` follows a light theme.
   * @zh 背景材质的暗色调（仅 `webview`）；`false` 跟随浅色主题。默认 `true`。
   */
  backdropDarkMode?: boolean;
  /**
   * Milliseconds the `webview` menu plays its exit transition (`#menu.out`) before hiding on a
   * user close; clamped to `0` to `1000`, `0` hides at once.
   * @zh `webview` 菜单在用户关闭时先播放退场过渡（`#menu.out`）的毫秒数，夹到 `0` 到 `1000`；`0` 立即隐藏。默认 `0`。
   */
  closeAnimationMs?: Int;
  /**
   * DOM layout of the `webview` menu: `flat` keeps rows as direct children of the root,
   * `zones` wraps each non-empty zone in `.fb-zone[data-zone]`.
   * @zh `webview` 菜单的 DOM 布局：`flat` 让行直接挂在根下，`zones` 把每个非空区包进 `.fb-zone[data-zone]`。默认 `flat`。
   */
  layoutMode?: 'flat' | 'zones';
}

interface SetContextMenuParams {
  /**
   * The rows of the zone.
   * @zh 该区的行。
   */
  items: TrayMenuItem[];
  /**
   * Menu-wide options to change.
   * @zh 要改的菜单级选项。
   */
  config?: TrayMenuConfig;
}

interface SetMenuZonesParams {
  /**
   * Rows of the `top` zone; omitted clears the zone.
   * @zh `top` 区的行；省略即清空该区。
   */
  top?: TrayMenuItem[];
  /**
   * Rows of the `playback` zone; omitted clears the zone. The rows `config.showPlaybackControls`
   * injects are not stored in the zone, so clearing it leaves them in place.
   * @zh `playback` 区的行；省略即清空该区。`config.showPlaybackControls` 注入的内置行不存在区里，清空该区不会去掉它们。
   */
  playback?: TrayMenuItem[];
  /**
   * Rows of the `bottom` zone; omitted clears the zone. The rows `config.showSystemItems`
   * injects are not stored in the zone, so clearing it leaves them in place.
   * @zh `bottom` 区的行；省略即清空该区。`config.showSystemItems` 注入的内置行不存在区里，清空该区不会去掉它们。
   */
  bottom?: TrayMenuItem[];
  /**
   * Menu-wide options to change. `customPosition` is stored for later `setContextMenu` calls
   * and does not affect this one.
   * @zh 要改的菜单级选项。`customPosition` 存下来供之后的 `setContextMenu` 使用，对本次调用不起作用。
   */
  config?: TrayMenuConfig;
}

interface AppendMenuItemsParams {
  /**
   * Rows to append.
   * @zh 要追加的行。
   */
  items: TrayMenuItem[];
  /**
   * Zone to append to.
   * @zh 追加到哪个区。
   * @default "top"
   */
  position?: TrayMenuPosition;
}

interface RemoveMenuItemsParams {
  /**
   * Ids of the rows to remove.
   * @zh 要移除的行的 id。
   */
  ids: string[];
}

interface RemoveMenuItemsResult {
  /**
   * Number of rows removed; less than the number of ids when some did not exist.
   * @zh 实际移除的行数；有 id 不存在时少于 id 的个数。
   */
  removed: Int;
}

interface ClearMenuItemsParams {
  /**
   * Zone to clear; every zone when omitted.
   * @zh 要清空的区；省略即全部。
   */
  position?: TrayMenuPosition;
}

interface GetMenuItemsResult {
  /**
   * The stored user rows.
   * @zh 存储的用户行。
   */
  items: TrayMenuItem[];
}

interface SetMenuItemStateParams {
  /**
   * Id of the row.
   * @zh 行的 id。
   * @minLength 1
   */
  id: string;
  /**
   * New checkmark state.
   * @zh 新的勾选状态。
   */
  checked?: boolean;
  /**
   * New enabled state.
   * @zh 新的可用状态。
   */
  enabled?: boolean;
}

interface SetMenuItemStateResult {
  /**
   * Whether a row with the id existed; `true` on success.
   * @zh 是否存在该 id 的行；成功时为 `true`。
   */
  found: boolean;
}

interface SetMinimizeToTrayParams {
  /**
   * `true` hides to the tray.
   * @zh `true` 即隐藏到托盘。
   */
  enabled: boolean;
}

interface SetCloseToTrayParams {
  /**
   * `true` hides to the tray.
   * @zh `true` 即隐藏到托盘。
   */
  enabled: boolean;
}

interface IsVisibleResult {
  /**
   * Whether the icon exists.
   * @zh 图标是否存在。
   */
  visible: boolean;
}
