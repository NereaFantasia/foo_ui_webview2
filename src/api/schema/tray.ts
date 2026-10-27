import type { Int } from './common.js';

export interface Api {
  /**
   * Create the tray icon. It must exist to receive input or display notifications.
   * Keeps the click action previously set with `setClickAction`, including on repeated calls.
   * Fails in panel mode, when the main window has no handle, and when the shell refuses the
   * icon.
   * @zh 创建系统托盘图标（图标成功创建后方可接收用户输入与展示气泡通知；重复调用将保留此前通过 setClickAction 设置的点击动作；面板模式下、主窗口句柄缺失或 Shell 拒绝注册时调用失败）
   */
  create(params: CreateParams): void;

  /**
   * Remove the tray icon, cancel pending clicks and reset the click action to `event`. A main
   * window hidden to the tray is shown again, since nothing else could bring it back.
   * @zh 销毁系统托盘图标（取消挂起的待处理点击，并将左键点击动作重置为 event；若主窗口当前处于隐藏到托盘状态，将自动恢复显示以防界面无法唤回）
   */
  destroy(): void;

  /**
   * Replace the tray icon image; fails while no icon is created.
   * @zh 更换系统托盘图标图像（若尚未通过 tray.create 创建图标则调用失败）
   */
  setIcon(params: SetIconParams): void;

  /**
   * Update the icon's hover text; fails while no icon is created.
   * @zh 更新系统托盘图标的悬停提示文本（若尚未通过 tray.create 创建图标则调用失败）
   */
  setTooltip(params: SetTooltipParams): void;

  /**
   * Show a balloon notification from the tray icon; fails while no icon is created.
   * @zh 通过系统托盘图标弹出气泡通知（若尚未通过 tray.create 创建图标则调用失败）
   */
  showBalloon(params: ShowBalloonParams): void;

  /**
   * Replace the user rows of one menu zone (`config.customPosition`, default `top`); the
   * other zones are left as they are. Nothing is stored when any row is rejected or the
   * resource limits are exceeded. An ordinary row reports its click through
   * `tray:menuItemClicked`; the built-in playback and system rows, and rows declaring
   * `playbackAction`, run natively and do not. To replace every zone at once, use
   * `setMenuZones`.
   * @zh 整体替换指定分区（由 config.customPosition 指定，默认为 top）的自定义菜单项（其余分区保持不变；任一菜单项校验失败或超出资源配额时整体放弃更新；普通菜单项点击时通过 tray:menuItemClicked 派发事件，内置播放/系统项与声明了 playbackAction 的项由宿主原生直接执行而不派发事件；如需单次原子更新全部三个分区请使用 setMenuZones）
   */
  setContextMenu(params: SetContextMenuParams): void;

  /**
   * Replace the user rows of all three zones in one call; a zone that is not given is cleared.
   * Rows and `config` follow the rules of `setContextMenu`, and nothing is stored, `config`
   * included, when any row is rejected or the resource limits are exceeded. Use it when the
   * whole menu changes, instead of `clearMenuItems` followed by `appendMenuItems`: the menu
   * can open between separate calls and show only part of the update, and two such updates
   * that interleave add their rows twice.
   * @zh 单次原子替换全部三个分区的自定义菜单项（未传入的分区将被清空；菜单项校验规则与 config 语义同 setContextMenu，任一菜单项校验失败或超限时整体放弃保存且不修改配置；整份菜单更新时建议优先使用本方法，避免通过 clearMenuItems 与 appendMenuItems 组合调用导致中间状态闪烁或并发交错重复）
   */
  setMenuZones(params: SetMenuZonesParams): void;

  /**
   * Append rows to one menu zone; the same validation as `setContextMenu`.
   * @zh 向指定菜单分区追加菜单项（参数校验规则与 setContextMenu 保持一致）
   */
  appendMenuItems(params: AppendMenuItemsParams): void;

  /**
   * Remove rows by id from every zone, submenus included.
   * @zh 按菜单项唯一标识符 id 从所有分区（递归包含子菜单）中移除匹配的菜单项
   */
  removeMenuItems(params: RemoveMenuItemsParams): RemoveMenuItemsResult;

  /**
   * Clear one zone, or every zone when `position` is omitted.
   * @zh 清空指定菜单分区的自定义菜单项（省略 position 时同时清空全部三个分区）
   */
  clearMenuItems(params: ClearMenuItemsParams): void;

  /**
   * List the user rows of every zone, flattened in `top`, `playback`, `bottom` order, as they
   * were stored. The rows the runtime injects for `showPlaybackControls` and
   * `showSystemItems` are not included.
   * @zh 平铺获取全部存储的自定义菜单项列表（按 top、playback、bottom 分区顺序排列；运行时自动注入的 showPlaybackControls 与 showSystemItems 菜单项不包含在内）
   */
  getMenuItems(): GetMenuItemsResult;

  /**
   * Change one row's `checked` or `enabled` state in place, searching every zone and submenu
   * for the id. Giving `checked` (even `false`) makes the row checkable. The native menu is
   * rebuilt each time it opens, so the change shows on the next opening.
   * @zh 原地更新指定菜单项的 checked 或 enabled 状态（跨全部三个分区递归检索子菜单匹配 id；传入 checked 参数（含 false）将自动使该项具备可勾选属性；原生菜单在每次弹出时动态重建，状态变更于下一次展开菜单时生效）
   */
  setMenuItemState(params: SetMenuItemStateParams): SetMenuItemStateResult;

  /**
   * Hide the window to the tray instead of the taskbar when it is minimized. Without a tray icon
   * from `create` the window is minimized as usual. `create` resets this from the preferences, so
   * call it afterwards.
   * @zh 配置主窗口最小化时是否自动隐藏至系统托盘（未通过 create 创建托盘图标时维持系统默认最小化行为；tray.create 会根据首选项重置该状态，故须在 create 之后调用）
   */
  setMinimizeToTray(params: SetMinimizeToTrayParams): void;

  /**
   * Hide the window to the tray instead of quitting when it is closed. Without a tray icon from
   * `create` the window is minimized instead, so it can still be found on the taskbar. `create`
   * resets this from the preferences, so call it afterwards.
   * @zh 配置主窗口关闭时是否拦截退出并隐藏至系统托盘（未通过 create 创建托盘图标时将降级为最小化窗口以防失去窗口句柄；tray.create 会根据首选项重置该状态，故须在 create 之后调用）
   */
  setCloseToTray(params: SetCloseToTrayParams): void;

  /**
   * Set the process-wide left-click action, even before `create`. `event` is the default
   * and emits the existing click events. `toggleMainWindow` hides a visible, non-minimized
   * standalone main window, or shows, restores and requests focus otherwise, without page
   * execution or the old click events. A double-click performs one toggle. Pending clicks
   * are cancelled on every setter call; the latest successful caller wins. Shell recreation
   * preserves this setting; `destroy` resets it. Fails with `PANEL_MODE_UNSUPPORTED` in panels.
   * Remove legacy click handlers that issue window commands before enabling native actions;
   * already queued page events cannot be recalled. The OS may refuse foreground activation.
   * @zh 配置系统托盘图标进程级左键点击行为（支持在 create 前预先设置；默认 event 模式派发常规点击事件；toggleMainWindow 模式由宿主原生切换独立主窗口的显隐与恢复聚焦状态，无需唤醒页面脚本亦不派发点击事件；双击仅触发单次状态切换；调用时自动取消挂起的待处理点击，以最后一次成功设置的值为准；Shell 重建时保留此配置，destroy 时重置为 event；面板模式下调用返回 PANEL_MODE_UNSUPPORTED；启用原生切换前应移除前端已注册的窗口控制监听器，系统可能因焦点策略拒绝前台激活）
   */
  setClickAction(params: SetClickActionParams): void;

  /**
   * Report whether the tray icon exists.
   * @zh 查询当前系统托盘图标是否存在并处于激活状态
   */
  isVisible(): IsVisibleResult;
}

export interface Events {
  /**
   * The tray icon was clicked with the left button, on release. A right click opens the context
   * menu instead and sends `tray:beforeContextMenu`. Only emitted in the `event` click action.
   * @zh 系统托盘图标被鼠标左键单击释放时广播（仅在 event 点击模式下派发；鼠标右键触发上下文菜单时派发 tray:beforeContextMenu）
   * @delivery broadcast
   */
  click: ClickPayload;

  /**
   * The tray icon was double-clicked with the left button in the `event` click action.
   * @zh 系统托盘图标被鼠标左键双击时广播（仅在 event 点击模式下派发）
   * @delivery broadcast
   */
  doubleClick: CursorPositionPayload;

  /**
   * A row of the tray menu was chosen, or a rich control in it changed value. The built-in
   * playback and system rows, and rows that declare `playbackAction`, run natively and send
   * nothing. With `render: 'webview'` a rich control reports every change and the menu stays
   * open; the default native menu closes on each pick, offers a slider as five stops and shows
   * a segmented row as an ordinary row, which reports no `value`.
   * @zh 自定义托盘菜单项被点击或内嵌富控件值发生变更时广播（内置播放/系统项及声明了 playbackAction 的项由宿主原生直接执行而不派发此事件；在 render: 'webview' 模式下富控件每次变更均会实时派发且菜单保持展开，在默认 native 模式下每次点击后菜单自动关闭、滑块离散为五档、分段项降级为普通项且不携带 value）
   * @delivery broadcast
   */
  menuItemClicked: MenuItemClickedPayload;

  /**
   * The tray context menu is about to open on a right click. The menu does not wait for the
   * page, so changes a handler makes to it take effect from the next right click. It is sent
   * even when the menu has no visible row and nothing opens.
   * @zh 鼠标右键点击托盘图标且上下文菜单即将展开前广播（菜单弹出过程不阻塞等待前端响应，事件处理函数中对菜单结构的修改将于下一次右键时生效；即使当前菜单无可见菜单项导致未弹出菜单亦会派发此事件）
   * @delivery broadcast
   */
  beforeContextMenu: MenuPositionPayload;
}

interface ClickPayload {
  /**
   * Always `0`, the left button.
   * @zh 鼠标按键索引（固定为 0，表示鼠标左键）
   */
  button: Int;
  /**
   * Cursor x in screen coordinates.
   * @zh 光标所在的屏幕横坐标（像素）
   */
  x: Int;
  /**
   * Cursor y in screen coordinates.
   * @zh 光标所在的屏幕纵坐标（像素）
   */
  y: Int;
}

interface CursorPositionPayload {
  /**
   * Cursor x in screen coordinates.
   * @zh 光标所在的屏幕横坐标（像素）
   */
  x: Int;
  /**
   * Cursor y in screen coordinates.
   * @zh 光标所在的屏幕纵坐标（像素）
   */
  y: Int;
}

interface MenuPositionPayload {
  /**
   * Screen x of the cursor, where the menu opens.
   * @zh 托盘菜单弹出的屏幕横坐标锚点（像素）
   */
  x: Int;
  /**
   * Screen y of the cursor, where the menu opens.
   * @zh 托盘菜单弹出的屏幕纵坐标锚点（像素）
   */
  y: Int;
}

interface MenuItemClickedPayload {
  /**
   * The row's `id`; `""` for a row without one.
   * @zh 被触发菜单项的唯一标识符 id（无 id 的项为空字符串）
   */
  id: string;
  /**
   * The new value of a rich control: stars `0` to `5` for `rating` (`0` clears), a value within
   * `min` to `max` for `slider`, the index of the chosen segment for `segmented`. Absent for
   * other rows.
   * @zh 富控件的最新数值（rating 返回 0 至 5 星，0 表示清除评分；slider 返回 min 至 max 范围内的整数；segmented 返回当前选中分段的索引；普通菜单项不附带该字段）
   */
  value?: Int;
}

/**
 * Raw Base64 of an `.ico` file, without a `data:` or `base64:` prefix. An empty or undecodable
 * value falls back to the foobar2000 main icon.
 * @zh .ico 图标文件的原始 Base64 编码字符串（不含 data: 或 base64: 前缀；为空或解码失败时自动回退为 foobar2000 主程序图标）
 */
type IcoBase64 = string;

/**
 * A menu zone: `top` and `bottom` hold user rows above and below the built-in playback rows of
 * the `playback` zone.
 * @zh 托盘菜单分区（top 位于内置播放控制区上方；playback 为内置播放控制所在区；bottom 位于其下方）
 */
type TrayMenuPosition = 'top' | 'playback' | 'bottom';

interface CreateParams {
  /**
   * Icon to show.
   * @zh 托盘图标（.ico 文件原始 Base64 编码字符串；省略时使用默认图标）
   */
  icon?: IcoBase64;
  /**
   * Hover text.
   * @zh 鼠标悬停时的提示文本
   * @default "foobar2000"
   */
  tooltip?: string;
}

interface SetIconParams {
  /**
   * Icon to show.
   * @zh 待更新的托盘图标（.ico 文件原始 Base64 编码字符串）
   */
  icon?: IcoBase64;
}

interface SetTooltipParams {
  /**
   * Hover text; empty clears it.
   * @zh 待更新的悬停提示文本（传入空字符串表示清除提示）
   * @default ""
   */
  tooltip?: string;
}

interface ShowBalloonParams {
  /**
   * Notification title.
   * @zh 气泡通知标题文本
   * @default ""
   */
  title?: string;
  /**
   * Notification body.
   * @zh 气泡通知正文文本
   * @default ""
   */
  message?: string;
  /**
   * Icon shown in the balloon.
   * @zh 气泡通知图标类型（info 信息、warning 警告、error 错误）
   * @default "info"
   */
  icon?: 'info' | 'warning' | 'error';
}

/**
 * An inline monochrome SVG icon: the `viewBox` and the inner markup (for example
 * `<path d="..."/>`). Drawn by the `webview` backend only, through an allowlist of shape
 * elements and attributes; an illegal or oversized (over 32 KiB) icon is dropped and the row
 * is shown without it.
 * @zh 内联单色 SVG 图标定义（仅 webview 渲染后端支持；包含 viewBox 与内部元素标记，经过安全属性与标签白名单过滤；非法或体积超过 32 KiB 的图标将被丢弃并仅展示文字）
 */
interface TrayIconSvg {
  /**
   * The SVG `viewBox` attribute.
   * @zh SVG viewBox 视口属性
   */
  viewBox: string;
  /**
   * The SVG inner markup.
   * @zh SVG 内部矢量图形标记（例如 <path d="..."/>）
   */
  content: string;
}

/**
 * One option of a `segmented` row.
 * @zh 分段控件中的单个可选分段
 */
interface TraySegment {
  /**
   * Text of the segment, shown when it has no icon.
   * @zh 分段文本标签（未提供图标时展示）
   */
  label?: string;
  /**
   * Icon of the segment, preferred over the label.
   * @zh 分段矢量图标（优先级高于文本标签）
   */
  iconSvg?: TrayIconSvg;
  /**
   * `false` greys the segment out so it cannot be picked.
   * @zh 分段是否可用（为 false 时置灰且不可点击）
   * @default true
   */
  enabled?: boolean;
}

/**
 * One tray menu row. The rich kinds (`nowplaying`, `rating`, `slider`, `segmented`) are fully
 * rendered by the `webview` backend only; the native backend degrades them (a disabled header
 * line, a 1 to 5 stars submenu, a stepped-level submenu, a plain text row).
 * @zh 托盘菜单项定义（富类型 nowplaying、rating、slider、segmented 仅在 webview 渲染后端下完整支持；native 原生后端将自动降级为禁用标题、星级子菜单、分档子菜单或普通文本行）
 */
interface TrayMenuItem {
  /**
   * Identifier reported by `tray:menuItemClicked`; omit for separators. The exact ids
   * `_sys_show` and `_sys_exit` are the native show-main-window and exit rows, which keep the
   * caller's label and skip the matching built-in injection.
   * @zh 菜单项唯一标识符（由 tray:menuItemClicked 事件上报；分隔线可省略；预留的 _sys_show 与 _sys_exit 将绑定原生显示主窗口与退出行为并抑制默认系统项注入）
   */
  id?: string;
  /**
   * Display text.
   * @zh 菜单项显示文本
   */
  label?: string;
  /**
   * Row kind. `checkbox` is an older spelling of a checkable `normal` row; a checkmark is
   * otherwise driven by `checked`.
   * @zh 菜单项展现类型（checkbox 为可勾选 normal 项的历史兼容写法，建议统一通过 checked 属性控制勾选状态）
   * @default "normal"
   */
  type?: 'normal' | 'separator' | 'checkbox' | 'submenu' | 'nowplaying' | 'rating' | 'slider' | 'segmented';
  /**
   * Whether the row can be clicked.
   * @zh 菜单项是否处于启用可交互状态
   * @default true
   */
  enabled?: boolean;
  /**
   * Whether the row is shown.
   * @zh 菜单项是否可见
   * @default true
   */
  visible?: boolean;
  /**
   * Checkmark state. Giving the key, `false` included, makes the row checkable: the `webview`
   * backend maps it to `menuitemcheckbox` and `getMenuItems` reports the key back. Omitted, the
   * row is not checkable.
   * @zh 菜单项勾选状态（传入该属性包括 false 将使菜单项转为可勾选模式，webview 后端映射为 menuitemcheckbox 并支持通过 getMenuItems 读取；缺省时表示不可勾选普通菜单项）
   */
  checked?: boolean;
  /**
   * Reserved; neither backend draws it. Use `iconSvg` for a row icon.
   * @zh 保留字段（双后端均不绘制，图标定义请使用 iconSvg）
   */
  icon?: string;
  /**
   * Icon drawn before the label by the `webview` backend. When any row of a menu level has a
   * renderable icon, every `normal` and `submenu` row of that level reserves the icon column.
   * @zh 菜单项前置矢量图标（仅 webview 后端绘制；同一层级内只要有一项配置了图标，同级全部 normal 与 submenu 项均会自动对齐预留图标列宽度）
   */
  iconSvg?: TrayIconSvg;
  /**
   * `nowplaying` album art: a `data:` URL, an `http(s)://` URL, or raw Base64 JPEG. With
   * `config.autoNowPlaying` an empty value is filled from the playing track's front art
   * (`webview` only).
   * @zh nowplaying 正在播放卡片封面（支持 data: URL、http(s):// 链接或原始 Base64 JPEG；开启 config.autoNowPlaying 时缺省将自动拉取当前曲目前置封面；仅 webview 后端生效）
   */
  cover?: string;
  /**
   * `nowplaying` first line, usually the track title; falls back to `label`.
   * @zh nowplaying 正在播放卡片主标题（通常为曲目名称；缺省时回退使用 label）
   */
  title?: string;
  /**
   * `nowplaying` second line, usually artist or album.
   * @zh nowplaying 正在播放卡片副标题（通常为艺术家或专辑信息）
   */
  subtitle?: string;
  /**
   * Current value: stars `0` to `5` for `rating`, a value within `min` to `max` for `slider`
   * (clamped), the selected index for `segmented`. Reported back only for those kinds.
   * @zh 控件当前数值（rating 支持 0 至 5 星；slider 支持 min 至 max 范围内的数值，超出范围自动夹取；segmented 为当前选中分段索引；仅在此三类控件上生效并回传）
   */
  value?: Int;
  /**
   * `slider` range minimum; swapped with `max` when larger.
   * @zh slider 滑块数值下限（若大于 max 将自动与 max 互换）
   */
  min?: Int;
  /**
   * `slider` range maximum.
   * @zh slider 滑块数值上限
   */
  max?: Int;
  /**
   * `slider` axis (`webview` only); horizontal when omitted. Vertical puts `min` at the bottom.
   * @zh slider 滑块布局方向（仅 webview 后端支持；缺省为水平方向，垂直方向时 min 位于底部）
   */
  orientation?: 'horizontal' | 'vertical';
  /**
   * `segmented` options, one row of mutually exclusive choices; `value` is the selected index.
   * Picking one reports `{ id, value }` and keeps the menu open.
   * @zh segmented 分段选项列表（单行互斥选择项，value 为当前选中项索引；点击分段上报 { id, value } 且菜单保持展开）
   */
  segments?: TraySegment[];
  /**
   * Child rows of a `submenu` row.
   * @zh submenu 子菜单包含的子菜单项列表
   */
  submenu?: TrayMenuItem[];
  /**
   * A playback command the plugin runs natively when the row is picked, so it keeps working
   * while the page is suspended (minimized, hidden to the tray, session locked). Such a row does
   * not fire `tray:menuItemClicked`. Allowed on a `normal` leaf only; any other placement fails
   * the whole call.
   * @zh 声明由宿主原生直接执行的播放命令（支持在主页面挂起、最小化或锁屏状态下稳定响应；配置后点击该项不触发 tray:menuItemClicked；仅允许配置在 normal 叶子节点上）
   */
  playbackAction?: 'play-pause' | 'previous' | 'next' | 'stop';
}

/**
 * Menu-wide options. Every key is optional and only the keys given change the stored
 * configuration.
 * @zh 托盘菜单全局配置选项（所有键均为可选，仅传入的键会更新存储配置）
 */
interface TrayMenuConfig {
  /**
   * Inject the built-in previous, play/pause, next and stop rows into the `playback` zone.
   * @zh 是否在 playback 分区自动注入上一首、播放/暂停、下一首与停止等原生播放控制项（缺省为 true）
   */
  showPlaybackControls?: boolean;
  /**
   * Inject the native show-main-window and exit rows into the `bottom` zone; both keep
   * working while the page is suspended.
   * @zh 是否在 bottom 分区自动注入显示主窗口与退出等原生系统控制项（支持在页面深度挂起状态下可靠响应；缺省为 true）
   */
  showSystemItems?: boolean;
  /**
   * The zone `setContextMenu` writes its rows into.
   * @zh setContextMenu 默认写入的目标菜单分区（缺省为 top）
   */
  customPosition?: TrayMenuPosition;
  /**
   * Menu backend: the Win32 menu, or the self-drawn WebView2 overlay that renders the rich
   * row kinds and the styling options below.
   * @zh 菜单渲染后端（native 为 Win32 原生菜单；webview 为基于 WebView2 的自绘现代浮层，支持富控件与自定义 CSS 样式；缺省为 native）
   */
  render?: 'native' | 'webview';
  /**
   * Fill the empty `cover`, `title` and `subtitle` of `nowplaying` rows from the playing
   * track when the menu opens; a value given by the caller always wins. Art is downscaled to
   * 64 px and omitted above 256 KiB; `cover` filling is `webview` only.
   * @zh 展开菜单时是否自动使用当前播放曲目填充 nowplaying 卡片中留空的 cover、title 与 subtitle 字段（调用方显式指定的字段优先；封面图像自动下采样至 64 像素且限制最大 256 KiB；仅 webview 后端生效；缺省为 false）
   */
  autoNowPlaying?: boolean;
  /**
   * Stylesheet injected into the `webview` menu on every opening, on top of the built-in
   * styles; target the menu's stable class names (`.fb-menu`, `.fb-item`, `.fb-sep`, ...).
   * @zh 注入 webview 自绘菜单的自定义 CSS 样式代码（叠加在内置样式之上，可通过 .fb-menu、.fb-item、.fb-sep 等稳定类名定制视觉样式）
   */
  css?: string;
  /**
   * `true` disables the built-in styles so only `css` and the protected structural layer
   * apply (`webview` only).
   * @zh 是否完全停用内置 CSS 样式（仅保留受保护的 DOM 结构层与用户自定义 css 规则；仅 webview 后端生效；缺省为 false）
   */
  cssReplace?: boolean;
  /**
   * DWM backdrop of the `webview` menu, the same vocabulary as the windows. It snaps in and out
   * with the window and cannot fade with CSS.
   * @zh webview 自绘菜单的 Windows DWM 材质背景效果（acrylic 亚克力、mica、mica-alt 或 none；材质紧贴窗口即时切换，不支持 CSS 透明度渐变过渡；缺省为 acrylic）
   */
  backdrop?: 'acrylic' | 'mica' | 'mica-alt' | 'none';
  /**
   * Dark tint for the backdrop (`webview` only); `false` follows a light theme.
   * @zh DWM 背景材质是否强制启用深色调（仅 webview 后端生效；设为 false 时跟随浅色视觉风格；缺省为 true）
   */
  backdropDarkMode?: boolean;
  /**
   * Milliseconds the `webview` menu plays its exit transition (`#menu.out`) before hiding on a
   * user close; clamped to `0` to `1000`, `0` hides at once.
   * @zh webview 自绘菜单在用户主动关闭时的退场过渡动画时长（单位毫秒，取值范围 0 至 1000；传入 0 表示立即关闭窗口；可配合 #menu.out CSS 选择器定制动画；缺省为 0）
   */
  closeAnimationMs?: Int;
  /**
   * DOM layout of the `webview` menu: `flat` keeps rows as direct children of the root,
   * `zones` wraps each non-empty zone in `.fb-zone[data-zone]`.
   * @zh webview 自绘菜单的 DOM 容器布局模式（flat 表示所有菜单项平铺于根容器下；zones 表示将非空分区分别包裹于 .fb-zone[data-zone] 容器内；缺省为 flat）
   */
  layoutMode?: 'flat' | 'zones';
}

interface SetContextMenuParams {
  /**
   * The rows of the zone.
   * @zh 写入目标分区的自定义菜单项列表
   */
  items: TrayMenuItem[];
  /**
   * Menu-wide options to change.
   * @zh 待更新的托盘菜单全局配置选项
   */
  config?: TrayMenuConfig;
}

interface SetMenuZonesParams {
  /**
   * Rows of the `top` zone; omitted clears the zone.
   * @zh top 分区菜单项列表（省略时清空该分区）
   */
  top?: TrayMenuItem[];
  /**
   * Rows of the `playback` zone; omitted clears the zone. The rows `config.showPlaybackControls`
   * injects are not stored in the zone, so clearing it leaves them in place.
   * @zh playback 分区菜单项列表（省略时清空该分区；showPlaybackControls 注入的原生项独立维护，不受此清空影响）
   */
  playback?: TrayMenuItem[];
  /**
   * Rows of the `bottom` zone; omitted clears the zone. The rows `config.showSystemItems`
   * injects are not stored in the zone, so clearing it leaves them in place.
   * @zh bottom 分区菜单项列表（省略时清空该分区；showSystemItems 注入的原生项独立维护，不受此清空影响）
   */
  bottom?: TrayMenuItem[];
  /**
   * Menu-wide options to change. `customPosition` is stored for later `setContextMenu` calls
   * and does not affect this one.
   * @zh 待更新的托盘菜单全局配置选项（传入的 customPosition 将保存供后续 setContextMenu 调用使用）
   */
  config?: TrayMenuConfig;
}

interface AppendMenuItemsParams {
  /**
   * Rows to append.
   * @zh 待追加的菜单项列表
   */
  items: TrayMenuItem[];
  /**
   * Zone to append to.
   * @zh 目标追加分区
   * @default "top"
   */
  position?: TrayMenuPosition;
}

interface RemoveMenuItemsParams {
  /**
   * Ids of the rows to remove.
   * @zh 待移除菜单项的标识符 id 列表
   */
  ids: string[];
}

interface RemoveMenuItemsResult {
  /**
   * Number of rows removed; less than the number of ids when some did not exist.
   * @zh 实际成功移除的菜单项总数（若部分 id 未匹配则小于传入 id 数量）
   */
  removed: Int;
}

interface ClearMenuItemsParams {
  /**
   * Zone to clear; every zone when omitted.
   * @zh 待清空的目标菜单分区（省略时清空全部三个分区）
   */
  position?: TrayMenuPosition;
}

interface GetMenuItemsResult {
  /**
   * The stored user rows.
   * @zh 当前存储的全部自定义菜单项列表（按 top、playback、bottom 分区顺序平铺展示）
   */
  items: TrayMenuItem[];
}

interface SetMenuItemStateParams {
  /**
   * Id of the row.
   * @zh 待修改菜单项的唯一标识符 id
   * @minLength 1
   */
  id: string;
  /**
   * New checkmark state.
   * @zh 新的勾选状态（传入布尔值时自动确保菜单项支持勾选交互）
   */
  checked?: boolean;
  /**
   * New enabled state.
   * @zh 新的启用状态
   */
  enabled?: boolean;
}

interface SetMenuItemStateResult {
  /**
   * Whether a row with the id existed; `true` on success.
   * @zh 是否成功定位并更新了指定 id 的菜单项
   */
  found: boolean;
}

interface SetClickActionParams {
  /**
   * `event` sends clicks to pages; `toggleMainWindow` executes once natively per gesture.
   * @zh 左键点击行为模式（event 向页面派发点击事件；toggleMainWindow 触发原生窗口显隐切换）
   */
  action: 'event' | 'toggleMainWindow';
}

interface SetMinimizeToTrayParams {
  /**
   * `true` hides to the tray.
   * @zh 是否启用最小化时隐藏至系统托盘
   */
  enabled: boolean;
}

interface SetCloseToTrayParams {
  /**
   * `true` hides to the tray.
   * @zh 是否启用关闭窗口时隐藏至系统托盘
   */
  enabled: boolean;
}

interface IsVisibleResult {
  /**
   * Whether the icon exists.
   * @zh 托盘图标当前是否存在且处于显示状态
   */
  visible: boolean;
}
