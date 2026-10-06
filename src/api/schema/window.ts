import type { Int, Json } from './common.js';

// "The calling window" is the top-level window of the page that made the call, or the main window
// when the call carries no usable caller. Methods that take `windowId` resolve it instead (see
// TargetWindowId) and never fall back to the main window.

export interface Api {
  /**
   * Minimize the calling window with the system animation. The request is posted, so the window
   * changes state after the call returns. Fails in panel mode and when there is no window.
   * @zh 以系统动画最小化调用方窗口。请求是投递的，窗口在调用返回之后才变状态。面板模式与没有窗口时失败。
   */
  minimize(): void;

  /**
   * Maximize the calling window with the system animation, leaving fullscreen first when it is
   * fullscreen. The request is posted. Fails in panel mode and when there is no window.
   * @zh 以系统动画最大化调用方窗口；窗口处于全屏时先退出全屏。请求是投递的。面板模式与没有窗口时失败。
   */
  maximize(): void;

  /**
   * Restore the calling window from minimized or maximized with the system animation. A
   * fullscreen window leaves fullscreen instead and returns to the state it had before. Fails in
   * panel mode and when there is no window.
   * @zh 以系统动画把调用方窗口从最小化或最大化还原。全屏的窗口改为退出全屏，回到全屏前的状态。面板模式与没有窗口时失败。
   */
  restore(): void;

  /**
   * Close the calling window as the system close command does. Succeeds without doing anything
   * when there is no window. Fails in panel mode.
   * @zh 像系统关闭命令那样关闭调用方窗口。没有窗口时什么也不做并照常成功。面板模式下失败。
   */
  close(): void;

  /**
   * Maximize the calling window, or restore it when it is maximized. A fullscreen window leaves
   * fullscreen instead. Fails in panel mode, and with `maximized: false` when there is no window.
   * @zh 最大化调用方窗口，已最大化时还原。全屏的窗口改为退出全屏。面板模式下失败；没有窗口时失败并带 `maximized: false`。
   */
  toggleMaximize(): ToggleMaximizeResult;

  /**
   * Report whether the calling window is maximized; `false` when there is no window.
   * @zh 报告调用方窗口是否最大化；没有窗口时为 `false`。
   */
  isMaximized(): IsMaximizedResult;

  /**
   * Report whether the calling window is minimized; `false` when there is no window.
   * @zh 报告调用方窗口是否最小化；没有窗口时为 `false`。
   */
  isMinimized(): IsMinimizedResult;

  /**
   * Report whether a window is fullscreen.
   * @zh 报告窗口是否全屏。
   */
  isFullscreen(params: IsFullscreenParams): IsFullscreenResult;

  /**
   * Report the calling window's state flags and rectangle. Every flag is sent twice, bare and
   * `is`-prefixed. When there is no window every flag is `false` and the rectangle is zero.
   * @zh 报告调用方窗口的状态标志与矩形。每个标志都发两遍，一遍不带前缀、一遍带 `is` 前缀。没有窗口时标志全为 `false`，矩形全为 0。
   */
  getState(): GetStateResult;

  /**
   * Start moving the calling window with the mouse, as pressing its title bar does. Call it from
   * a `mousedown` handler while the button is still down. Fails in panel mode and when there is
   * no window.
   * @zh 开始用鼠标拖动调用方窗口，效果与按住标题栏相同。在 `mousedown` 处理函数里、按键仍按着时调用。面板模式与没有窗口时失败。
   */
  startDrag(): void;

  /**
   * Start resizing the calling window from one edge or corner with the mouse. Call it from a
   * `mousedown` handler while the button is still down. Fails in panel mode and when there is no
   * window.
   * @zh 开始用鼠标从某条边或某个角调整调用方窗口的大小。在 `mousedown` 处理函数里、按键仍按着时调用。面板模式与没有窗口时失败。
   */
  startResize(params: StartResizeParams): void;

  /**
   * Keep the calling window above other windows, or stop doing so. Fails in panel mode and when
   * there is no window.
   * @zh 让调用方窗口保持在其他窗口之上，或取消。面板模式与没有窗口时失败。
   */
  setAlwaysOnTop(params: SetAlwaysOnTopParams): void;

  /**
   * Report whether the calling window is kept above other windows; `false` when there is no
   * window.
   * @zh 报告调用方窗口是否保持在其他窗口之上；没有窗口时为 `false`。
   */
  isAlwaysOnTop(): IsAlwaysOnTopResult;

  /**
   * Flip whether the calling window is kept above other windows. Fails in panel mode, and with
   * `enabled: false` when there is no window.
   * @zh 切换调用方窗口是否保持在其他窗口之上。面板模式下失败；没有窗口时失败并带 `enabled: false`。
   */
  toggleAlwaysOnTop(): ToggleAlwaysOnTopResult;

  /**
   * Report the calling window's rectangle in screen coordinates, in physical pixels, frame
   * included. While the window is minimized this is where Windows parks it, not the geometry it
   * restores to. The rectangle is zero when there is no window.
   * @zh 报告调用方窗口的矩形：屏幕坐标，物理像素，含边框。最小化时报的是 Windows 停放窗口的位置，不是还原后的几何。没有窗口时矩形全为 0。
   */
  getBounds(): GetBoundsResult;

  /**
   * Move or resize the calling window in one call; omitted keys keep their current value and
   * fractions are dropped. Fails in panel mode and when there is no window.
   * @zh 一次移动或缩放调用方窗口；省略的键保持当前值，小数部分舍去。面板模式与没有窗口时失败。
   */
  setBounds(params: SetBoundsParams): void;

  /**
   * Center the calling window on the work area of its monitor, keeping its size. Fails in panel
   * mode and when there is no window.
   * @zh 把调用方窗口移到所在显示器工作区的中央，大小不变。面板模式与没有窗口时失败。
   */
  center(): void;

  /**
   * Set a window's minimum size; fractions are dropped. The host keeps it in DIPs of the target
   * window, so it reads back within 1 px. A window smaller than the new minimum grows at once, unless it is maximized,
   * fullscreen or minimized (a minimized window grows when restored). Fails in panel mode.
   * @zh 设置窗口的最小尺寸，小数部分舍去。宿主按目标窗口的 DIP 保存，所以读回时可能差 1 px。窗口比新的下限小时立即长大；最大化、全屏时不动，最小化的窗口在还原时再长大。面板模式下失败。
   */
  setMinSize(params: SetMinSizeParams): SetMinSizeResult;

  /**
   * Report a window's requested minimum size, which is the value last set rather than the
   * effective window size.
   * @zh 报告窗口请求的最小尺寸，即最后一次设置的值，而不是窗口实际的尺寸。
   */
  getMinSize(params: GetMinSizeParams): GetMinSizeResult;

  /**
   * Set a window's maximum size; `0` removes the bound and fractions are dropped. The host keeps it in DIPs of the target
   * window, so it reads back within 1 px (`0` exactly). A window larger than the new maximum
   * shrinks at once, unless it is maximized, fullscreen or minimized. Fails in panel mode.
   * @zh 设置窗口的最大尺寸；`0` 表示不设上限，小数部分舍去。宿主按目标窗口的 DIP 保存，所以读回时可能差 1 px（`0` 读回仍是 `0`）。窗口比新的上限大时立即缩小；最大化、全屏、最小化时不动。面板模式下失败。
   */
  setMaxSize(params: SetMaxSizeParams): SetMaxSizeResult;

  /**
   * Report a window's requested maximum size; `0` means no bound.
   * @zh 报告窗口请求的最大尺寸；`0` 表示不设上限。
   */
  getMaxSize(params: GetMaxSizeParams): GetMaxSizeResult;

  /**
   * Set whether the user can resize a window by its frame. Setting the value the window already
   * has succeeds. Fails in panel mode, and with `OPERATION_FAILED` when Windows refuses the new
   * style; that failure carries `windowId`.
   * @zh 设置用户能否拖动边框调整窗口大小。设为窗口已有的值照常成功。面板模式下失败；Windows 拒绝新样式时以 `OPERATION_FAILED` 失败，失败里带 `windowId`。
   */
  setResizable(params: SetResizableParams): SetResizableResult;

  /**
   * Report whether the user can resize a window by its frame.
   * @zh 报告用户能否拖动边框调整窗口大小。
   */
  isResizable(params: IsResizableParams): IsResizableResult;

  /**
   * Enter or leave fullscreen. Setting the state the window already has succeeds. Leaving
   * fullscreen restores the rectangle, the maximized state and the always-on-top state the window
   * had before. Fails in panel mode.
   * @zh 进入或退出全屏。设为窗口已有的状态照常成功。退出全屏时恢复进入前的矩形、最大化状态与置顶状态。面板模式下失败。
   */
  setFullscreen(params: SetFullscreenParams): SetFullscreenResult;

  /**
   * Enter fullscreen, or leave it when the window is fullscreen. Fails in panel mode.
   * @zh 进入全屏，窗口已全屏时退出。面板模式下失败。
   */
  toggleFullscreen(params: ToggleFullscreenParams): ToggleFullscreenResult;

  /**
   * Enter fullscreen. A window that is already fullscreen fails with `OPERATION_FAILED`. Fails in
   * panel mode.
   * @zh 进入全屏。窗口已经全屏时以 `OPERATION_FAILED` 失败。面板模式下失败。
   */
  enterFullscreen(params: EnterFullscreenParams): EnterFullscreenResult;

  /**
   * Leave fullscreen and restore the state the window had before. A window that is not
   * fullscreen fails with `OPERATION_FAILED`. Fails in panel mode.
   * @zh 退出全屏并恢复进入前的状态。窗口不在全屏时以 `OPERATION_FAILED` 失败。面板模式下失败。
   */
  exitFullscreen(params: ExitFullscreenParams): ExitFullscreenResult;

  /**
   * Bring a window to the foreground, restoring it first when minimized and showing it when
   * hidden. Fails with `NOT_FOUND` when there is no such window.
   * @zh 把窗口带到前台：最小化的先还原，隐藏的先显示。没有这个窗口时以 `NOT_FOUND` 失败。
   */
  focus(params: FocusParams): void;

  /**
   * Hand the foreground to the window below the calling window in the z-order. Fails when there
   * is no window.
   * @zh 把前台交给 Z 序里位于调用方窗口之下的那个窗口。没有窗口时失败。
   */
  blur(): void;

  /**
   * Set the calling window's title, shown in the title bar and on the taskbar. Fails in panel
   * mode and when there is no window.
   * @zh 设置调用方窗口的标题，显示在标题栏与任务栏上。面板模式与没有窗口时失败。
   */
  setTitle(params: SetTitleParams): void;

  /**
   * Report the calling window's title, up to 255 characters; empty when there is no window.
   * @zh 报告调用方窗口的标题，最多 255 个字符；没有窗口时为空串。
   */
  getTitle(): GetTitleResult;

  /**
   * Flash the calling window's caption and taskbar button to draw attention, or stop flashing.
   * Fails in panel mode and when there is no window.
   * @zh 闪烁调用方窗口的标题栏与任务栏按钮以吸引注意，或停止闪烁。面板模式与没有窗口时失败。
   */
  flash(params: FlashParams): void;

  /**
   * Open the calling window's system menu (restore, move, size, minimize, maximize, close) and
   * run the command picked from it. Coordinates are screen pixels; fractions are dropped. With a positive `w` and `h`
   * the menu opens below the rectangle `x`, `y`, `w`, `h` and keeps clear of it; otherwise it
   * opens at `x`, `y`. Fails in panel mode, when there is no window, and with `OPERATION_FAILED`
   * when the window has no system menu.
   * @zh 打开调用方窗口的系统菜单（还原、移动、大小、最小化、最大化、关闭）并执行选中的命令。坐标是屏幕像素，小数部分舍去。`w` 与 `h` 都为正时菜单在矩形 `x`、`y`、`w`、`h` 下方弹出并避开它，否则在 `x`、`y` 处弹出。面板模式、没有窗口时失败；窗口没有系统菜单时以 `OPERATION_FAILED` 失败。
   */
  showSystemMenu(params: ShowSystemMenuParams): void;

  /**
   * Move the calling window's top-left corner, keeping its size; fractions are dropped. Fails in
   * panel mode and when there is no window.
   * @zh 移动调用方窗口的左上角，大小不变，小数部分舍去。面板模式与没有窗口时失败。
   */
  setPosition(params: SetPositionParams): void;

  /**
   * Resize the calling window, keeping its position; fractions are dropped. The size stays within the window's minimum
   * and maximum, and the response does not say whether it was held back. Fails in panel mode and
   * when there is no window.
   * @zh 调整调用方窗口的大小，位置不变，小数部分舍去。尺寸受窗口的最小、最大尺寸约束，响应不说明是否被约束过。面板模式与没有窗口时失败。
   */
  setSize(params: SetSizeParams): void;

  /**
   * Report the DPI of the calling window's device context and its ratio to 96; `96` when there is
   * no window.
   * @zh 报告调用方窗口设备上下文的 DPI 及其与 96 的比值；没有窗口时为 `96`。
   */
  getDpiScale(): GetDpiScaleResult;

  /**
   * Flash the calling window's taskbar button and caption a number of times. Fails in panel mode
   * and when there is no window.
   * @zh 让调用方窗口的任务栏按钮与标题栏闪烁若干次。面板模式与没有窗口时失败。
   */
  flashTaskbar(params: FlashTaskbarParams): void;

  /**
   * Report whether an earlier session saved the main window's position, which a page can use to
   * decide whether to apply a default size on first launch.
   * @zh 报告此前的会话是否保存过主窗口的位置，页面可据此决定首次启动时是否设置默认大小。
   */
  hasSavedBounds(): HasSavedBoundsResult;

  /**
   * Report the title bar height of the calling window, the main window or a popup, in physical
   * pixels. A caller that is neither gets the default, `32`.
   * @zh 报告调用方窗口（主窗口或 popup）的标题栏高度，物理像素。两者都不是的调用方得到默认值 `32`。
   */
  getTitlebarHeight(): GetTitlebarHeightResult;

  /**
   * Set the title bar height of the calling window, the main window or a popup; a popup's height
   * leaves the main window alone. The height must be 24 to 100, otherwise the call fails with
   * `INVALID_PARAMS`. Fails in panel mode, and with `NOT_FOUND` when the caller is neither the
   * main window nor a popup.
   * @zh 设置调用方窗口（主窗口或 popup）的标题栏高度；popup 的高度不影响主窗口。高度须在 24 到 100 之间，否则以 `INVALID_PARAMS` 失败。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。
   */
  setTitlebarHeight(params: SetTitlebarHeightParams): SetTitlebarHeightResult;

  /**
   * Report the width of the main window's caption buttons (minimize, maximize, close) in physical
   * pixels at its DPI. Main window only; without one the call reports `138` and `46` rather than
   * failing.
   * @zh 报告主窗口标题栏按钮（最小化、最大化、关闭）的宽度，按其 DPI 换算的物理像素。只看主窗口；没有主窗口时报 `138` 与 `46`，不失败。
   */
  getCaptionButtonsWidth(): GetCaptionButtonsWidthResult;

  /**
   * Set the rectangles that drag the calling window, the main window or a popup, replacing the
   * earlier ones. Rectangles are CSS pixels of the page, scaled by the window's DPI; rectangles
   * without a positive width and height are skipped. Fails in panel mode, and with `NOT_FOUND`
   * when the caller is neither the main window nor a popup.
   * @zh 设置拖动调用方窗口（主窗口或 popup）的矩形，替换之前的设置。矩形是页面的 CSS 像素，按窗口 DPI 缩放；宽或高不为正的矩形被跳过。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。
   */
  setDragRegions(params: SetDragRegionsParams): SetDragRegionsResult;

  /**
   * Remove the calling window's drag rectangles. Fails in panel mode, and with `NOT_FOUND` when
   * the caller is neither the main window nor a popup.
   * @zh 清除调用方窗口的拖动矩形。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。
   */
  clearDragRegions(): void;

  /**
   * Set the rectangles that never drag the calling window, such as buttons inside a drag area,
   * replacing the earlier ones. Rectangles are CSS pixels of the page, scaled by the window's DPI;
   * rectangles without a positive width and height are skipped. Fails in panel mode, and with
   * `NOT_FOUND` when the caller is neither the main window nor a popup.
   * @zh 设置永远不拖动调用方窗口的矩形（例如拖动区域里的按钮），替换之前的设置。矩形是页面的 CSS 像素，按窗口 DPI 缩放；宽或高不为正的矩形被跳过。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。
   */
  setNoDragRegions(params: SetNoDragRegionsParams): SetNoDragRegionsResult;

  /**
   * Remove the calling window's no-drag rectangles. Fails in panel mode, and with `NOT_FOUND`
   * when the caller is neither the main window nor a popup.
   * @zh 清除调用方窗口的不可拖动矩形。面板模式下失败；调用方既不是主窗口也不是 popup 时以 `NOT_FOUND` 失败。
   */
  clearNoDragRegions(): void;

  /**
   * Tell the host where the page draws the main window's maximize button, so that on Windows 11
   * hovering it offers Snap layouts as a standard window's button does. The rectangle is CSS
   * pixels of the page, scaled by the window's DPI and the page zoom, the same factor as
   * `devicePixelRatio`, and replaces the earlier one; `region` omitted, or without a positive
   * width and height, removes it. While the window is frameless, resizable and not full screen,
   * the host answers the rectangle as the window's maximize button and passes the mouse input it
   * receives there on to the page, so the button keeps its hover and pressed styles and its own
   * click handler; on Windows 10 and in every other state the rectangle stays ordinary page
   * content. Set it again whenever layout moves the button; a navigation or reload of the page
   * removes it. Main window only: fails with `NOT_SUPPORTED` from a popup, in panel mode, and
   * with `NOT_FOUND` when the caller is no window.
   * @zh 告诉宿主页面把主窗口的最大化键画在哪里，这样在 Windows 11 上悬停它会像标准窗口的按钮一样弹出贴靠布局。矩形是页面的 CSS 像素，按窗口 DPI 与页面缩放换算，与 `devicePixelRatio` 同一个系数，替换之前的矩形；省略 `region`，或宽高不为正，则移除。窗口无标题栏、可调整大小且不在全屏时，宿主把这个矩形当作窗口的最大化键回答，并把在这里收到的鼠标输入转给页面，所以按钮照常有悬停与按下样式，照常由它自己的点击处理；在 Windows 10 上和其他状态下，这个矩形仍是普通的页面内容。布局挪动按钮后要重新设置；页面导航或重载时它被移除。只对主窗口：从 popup 调用以 `NOT_SUPPORTED` 失败，面板模式下失败，调用方不是窗口时以 `NOT_FOUND` 失败。
   */
  setMaximizeButtonRegion(params: SetMaximizeButtonRegionParams): SetMaximizeButtonRegionResult;

  /**
   * Report the main window's title bar height, caption button widths and maximized state
   * together, in physical pixels. Main window only; without one the call reports `32`, `138`,
   * `46` and `false`.
   * @zh 一并报告主窗口的标题栏高度、标题栏按钮宽度与最大化状态，物理像素。只看主窗口；没有主窗口时报 `32`、`138`、`46` 与 `false`。
   */
  getTitlebarInfo(): GetTitlebarInfoResult;

  /**
   * Set the main window's Windows 11 corner rounding. Main window only, whichever window calls;
   * popups manage their corners themselves. Fails in panel mode and when there is no main
   * window.
   * @zh 设置主窗口的 Windows 11 圆角。不论哪个窗口调用都只作用于主窗口；popup 自行管理圆角。面板模式与没有主窗口时失败。
   */
  setCornerPreference(params: SetCornerPreferenceParams): void;

  /**
   * Report the main window's corner rounding as last set; `default` when there is no main window.
   * @zh 报告主窗口最后一次设置的圆角；没有主窗口时为 `default`。
   */
  getCornerPreference(): GetCornerPreferenceResult;

  /**
   * Turn the Mica backdrop of a window on or off. The setting is stored even when the window does
   * not draw it right away, for example while it is still hidden at startup; the call then fails
   * with `OPERATION_FAILED` and the failure carries the result fields. Fails in panel mode.
   * @zh 打开或关闭窗口的 Mica 背景。即使窗口没能立即画出（例如启动时仍隐藏），设置也会保存；此时调用以 `OPERATION_FAILED` 失败，失败里带返回值的字段。面板模式下失败。
   */
  setMica(params: SetMicaParams): SetMicaResult;

  /**
   * Same as `window.setMica`.
   * @zh 与 `window.setMica` 相同。
   */
  setMicaEffect(params: SetMicaEffectParams): SetMicaEffectResult;

  /**
   * Turn the blur-behind effect of a window on or off. The setting is stored even when the window
   * does not draw it right away; the call then fails with `OPERATION_FAILED` and the failure
   * carries `enabled`. Fails in panel mode.
   * @zh 打开或关闭窗口的背景模糊。即使窗口没能立即画出，设置也会保存；此时调用以 `OPERATION_FAILED` 失败，失败里带 `enabled`。面板模式下失败。
   */
  setBlur(params: SetBlurParams): SetBlurResult;

  /**
   * Turn the acrylic backdrop of a window on or off. The setting is stored even when the window
   * does not draw it right away; the call then fails with `OPERATION_FAILED` and the failure
   * carries the result fields. Fails in panel mode.
   * @zh 打开或关闭窗口的亚克力背景。即使窗口没能立即画出，设置也会保存；此时调用以 `OPERATION_FAILED` 失败，失败里带返回值的字段。面板模式下失败。
   */
  setAcrylic(params: SetAcrylicParams): SetAcrylicResult;

  /**
   * Switch a window's backdrop between its dark and light variant. The setting is stored even
   * when the window does not draw it right away; the call then fails with `OPERATION_FAILED` and
   * the failure carries `enabled`. Fails in panel mode.
   * @zh 在深色与浅色之间切换窗口的背景。即使窗口没能立即画出，设置也会保存；此时调用以 `OPERATION_FAILED` 失败，失败里带 `enabled`。面板模式下失败。
   */
  setDarkMode(params: SetDarkModeParams): SetDarkModeResult;

  /**
   * Make a window's WebView background transparent, so the backdrop effect shows through, or
   * opaque. Fails in panel mode, and with `OPERATION_FAILED` when neither the window nor its
   * WebView took the change; that failure carries the result fields unless the window has no
   * WebView.
   * @zh 让窗口的 WebView 背景透明（背景效果得以透出）或不透明。面板模式下失败；窗口与其 WebView 都没接受时以 `OPERATION_FAILED` 失败，窗口有 WebView 时失败里带返回值的字段。
   */
  setBackgroundTransparency(params: SetBackgroundTransparencyParams): SetBackgroundTransparencyResult;

  /**
   * Redraw the calling window's WebView, which clears rendering left behind by a backdrop change.
   * Fails with `NOT_FOUND` when the calling window has no WebView of its own, as with a DUI/CUI
   * panel.
   * @zh 重绘调用方窗口的 WebView，清除背景效果变化后残留的画面。调用方窗口自己没有 WebView 时（例如 DUI/CUI 面板）以 `NOT_FOUND` 失败。
   */
  refreshWebView(): void;

  /**
   * Reload the calling window's page. Fails with `NOT_FOUND` when the calling window has no
   * WebView of its own, as with a DUI/CUI panel.
   * @zh 重新加载调用方窗口的页面。调用方窗口自己没有 WebView 时（例如 DUI/CUI 面板）以 `NOT_FOUND` 失败。
   */
  reload(): void;

  /**
   * Report whether pages load from a development server instead of the installed files, and that
   * server's address.
   * @zh 报告页面是否从开发服务器而不是已安装的文件加载，以及开发服务器的地址。
   */
  getDevServerConfig(): GetDevServerConfigResult;

  /**
   * Change the development server settings; omitted keys keep their value. They are used the next
   * time a page loads. The response reports both settings as stored.
   * @zh 修改开发服务器设置；省略的键保持原值。下次加载页面时生效。响应报告两项设置保存后的值。
   */
  setDevServerConfig(params: SetDevServerConfigParams): SetDevServerConfigResult;

  /**
   * Set the zoom factor of the calling window's WebView. From then on, for the rest of the
   * session, the default zoom in the preferences no longer applies to this WebView. Fails with
   * `NOT_FOUND` when the calling window has no WebView of its own, and with `OPERATION_FAILED`
   * when WebView2 refuses the factor; that failure carries the `zoom` in effect.
   * @zh 设置调用方窗口 WebView 的缩放倍数。此后在本次会话里，首选项的默认缩放不再作用于这个 WebView。调用方窗口自己没有 WebView 时以 `NOT_FOUND` 失败；WebView2 拒绝该倍数时以 `OPERATION_FAILED` 失败，失败里带当前生效的 `zoom`。
   */
  setZoom(params: SetZoomParams): SetZoomResult;

  /**
   * Report the zoom factor of the calling window's WebView and the window's DPI. Without a WebView
   * of its own the call reports zoom `1` and leaves out `dpi` and `dpiScale`.
   * @zh 报告调用方窗口 WebView 的缩放倍数与窗口的 DPI。调用方窗口自己没有 WebView 时报缩放 `1`，并省略 `dpi` 与 `dpiScale`。
   */
  getZoom(): GetZoomResult;

  /**
   * Set the zoom factor of the calling window's WebView back to `1`; as with `window.setZoom`, the
   * default zoom in the preferences no longer applies to it afterwards. Fails with `NOT_FOUND`
   * when the calling window has no WebView of its own, and with `OPERATION_FAILED` when WebView2
   * refuses the factor.
   * @zh 把调用方窗口 WebView 的缩放倍数恢复为 `1`；与 `window.setZoom` 一样，此后首选项的默认缩放不再作用于它。调用方窗口自己没有 WebView 时以 `NOT_FOUND` 失败，WebView2 拒绝该倍数时以 `OPERATION_FAILED` 失败。
   */
  resetZoom(): ResetZoomResult;

  /**
   * Set the zoom factor of the calling window's WebView to `dpi / 96`; as with `window.setZoom`,
   * the default zoom in the preferences no longer applies to it afterwards. Fails with
   * `NOT_FOUND` when the calling window has no WebView of its own, and with `OPERATION_FAILED`
   * when WebView2 refuses the factor; that failure carries the result fields.
   * @zh 把调用方窗口 WebView 的缩放倍数设为 `dpi / 96`；与 `window.setZoom` 一样，此后首选项的默认缩放不再作用于它。调用方窗口自己没有 WebView 时以 `NOT_FOUND` 失败；WebView2 拒绝该倍数时以 `OPERATION_FAILED` 失败，失败里带返回值的字段。
   */
  setZoomForDpi(params: SetZoomForDpiParams): SetZoomForDpiResult;

  /**
   * Remove or restore the native frame and caption of a window. Fails in panel mode.
   * @zh 去掉或恢复窗口的原生边框与标题栏。面板模式下失败。
   */
  setFrameless(params: SetFramelessParams): SetFramelessResult;

  /**
   * Open a popup window with its own WebView and bridge; at most 8 popups can be open. Works
   * from a DUI/CUI panel too. Fails with `OPERATION_FAILED` when 8 popups are already open or the
   * window cannot be created.
   * @zh 打开一个拥有独立 WebView 与桥的 popup 窗口；最多同时打开 8 个。从 DUI/CUI 面板也能调用。已有 8 个 popup 或窗口创建失败时以 `OPERATION_FAILED` 失败。
   */
  createPopup(params: CreatePopupParams): CreatePopupResult;

  /**
   * Close a popup as its close button does, so a popup created with `beforeClose` first gets
   * `window:beforeClose`. `main` fails with `INVALID_PARAMS`, an id no popup has with
   * `NOT_FOUND`.
   * @zh 像点关闭按钮那样关闭一个 popup，所以以 `beforeClose` 创建的 popup 会先收到 `window:beforeClose`。传 `main` 以 `INVALID_PARAMS` 失败，没有 popup 是该 id 时以 `NOT_FOUND` 失败。
   */
  closePopup(params: ClosePopupParams): void;

  /**
   * Close every popup as their close buttons do.
   * @zh 像点各自的关闭按钮那样关闭所有 popup。
   */
  closeAllPopups(): void;

  /**
   * List the main window and every popup with their policies, capabilities and rectangles.
   * @zh 列出主窗口与每个 popup，连同它们的策略、能力与矩形。
   */
  getAllWindows(): GetAllWindowsResult;

  /**
   * Report the id of the calling window: `main`, a popup id or a panel's id. A caller that cannot
   * be matched gets `main`.
   * @zh 报告调用方窗口的 id：`main`、popup 的 id 或面板的 id。无法匹配的调用方得到 `main`。
   */
  getCurrentWindowId(): GetCurrentWindowIdResult;

  /**
   * Report a popup's behavior preset, the overrides set on it and the behavior in effect. Popups
   * only: `main` fails with `NOT_SUPPORTED`; without `windowId` the calling popup is used, and a
   * caller that is not a popup fails with `NOT_FOUND`.
   * @zh 报告 popup 的行为预设、在它上面设置的覆盖项与实际生效的行为。只适用于 popup：传 `main` 以 `NOT_SUPPORTED` 失败；不传 `windowId` 时取调用方 popup，调用方不是 popup 时以 `NOT_FOUND` 失败。
   */
  getPopupBehavior(params: GetPopupBehaviorParams): GetPopupBehaviorResult;

  /**
   * Change a popup's behavior preset or its per-field overrides at runtime, then announce the
   * result with `window:behaviorChanged`. `profile` and `behavior` are independent: passing one
   * leaves the other alone. Targets as `window.getPopupBehavior` does; an unknown preset fails
   * with `INVALID_PARAMS`.
   * @zh 运行时修改 popup 的行为预设或逐字段覆盖，然后以 `window:behaviorChanged` 通告结果。`profile` 与 `behavior` 互相独立：只传一个不影响另一个。目标的选取同 `window.getPopupBehavior`；认不出的预设以 `INVALID_PARAMS` 失败。
   */
  setPopupBehavior(params: SetPopupBehaviorParams): SetPopupBehaviorResult;

  /**
   * Report a window's backdrop policy: the overrides set on it and the policy in effect.
   * @zh 报告窗口的背景策略：在它上面设置的覆盖项与实际生效的策略。
   */
  getBackdropPolicy(params: GetBackdropPolicyParams): GetBackdropPolicyResult;

  /**
   * Merge per-field overrides into a window's backdrop policy; a `null` value removes that
   * override. The overrides are stored even when the window does not draw the result right away,
   * for example while it is still hidden at startup; the call then fails with `OPERATION_FAILED`.
   * @zh 把逐字段覆盖合并进窗口的背景策略；值为 `null` 的键删除该覆盖。即使窗口没能立即画出结果（例如启动时仍隐藏），覆盖也会保存；此时调用以 `OPERATION_FAILED` 失败。
   */
  setBackdropPolicy(params: SetBackdropPolicyParams): SetBackdropPolicyResult;

  /**
   * Keep the calling popup open after it received `window:beforeClose`; nothing happens when no
   * close is pending. Fails with `NOT_FOUND` when the caller is not a popup.
   * @zh 收到 `window:beforeClose` 后让调用方 popup 保持打开；没有待定的关闭时什么也不做。调用方不是 popup 时以 `NOT_FOUND` 失败。
   */
  cancelClose(): void;

  /**
   * Let the calling popup close after it received `window:beforeClose`; nothing happens when no
   * close is pending. Fails with `NOT_FOUND` when the caller is not a popup.
   * @zh 收到 `window:beforeClose` 后让调用方 popup 关闭；没有待定的关闭时什么也不做。调用方不是 popup 时以 `NOT_FOUND` 失败。
   */
  confirmClose(): void;

  /**
   * Let mouse input pass through a popup to the windows below, except over the rectangles set
   * with `window.setClickThroughExcludeRegions`. Popups only: without `windowId` the calling
   * window is used, and a window that is not a popup fails with `NOT_FOUND`.
   * @zh 让鼠标输入穿过 popup 落到下面的窗口，经 `window.setClickThroughExcludeRegions` 设置的矩形除外。只适用于 popup：不传 `windowId` 时取调用方窗口，它不是 popup 时以 `NOT_FOUND` 失败。
   */
  setClickThrough(params: SetClickThroughParams): SetClickThroughResult;

  /**
   * Report whether mouse input passes through a popup. Targets as `window.setClickThrough` does.
   * @zh 报告鼠标输入是否穿过 popup。目标的选取同 `window.setClickThrough`。
   */
  isClickThrough(params: IsClickThroughParams): IsClickThroughResult;

  /**
   * Set the rectangles of a popup that keep taking mouse input while it lets input pass through,
   * replacing the earlier ones. Rectangles are CSS pixels of the page, scaled by the popup's DPI;
   * rectangles without a positive width and height are skipped, and at most 32 are kept. Targets
   * as `window.setClickThrough` does.
   * @zh 设置 popup 在鼠标穿透时仍接收鼠标输入的矩形，替换之前的设置。矩形是页面的 CSS 像素，按 popup 的 DPI 缩放；宽或高不为正的矩形被跳过，最多保留 32 个。目标的选取同 `window.setClickThrough`。
   */
  setClickThroughExcludeRegions(params: SetClickThroughExcludeRegionsParams): SetClickThroughExcludeRegionsResult;

  /**
   * Remove a popup's click-through exclude rectangles. Targets as `window.setClickThrough` does.
   * @zh 清除 popup 的鼠标穿透排除矩形。目标的选取同 `window.setClickThrough`。
   */
  clearClickThroughExcludeRegions(params: ClearClickThroughExcludeRegionsParams): ClearClickThroughExcludeRegionsResult;

  /**
   * Send a message to one window, delivered as `window:message` with
   * `{ sourceWindowId, message }`. Fails with `NOT_FOUND` when no window has that id.
   * @zh 给一个窗口发消息，以 `window:message`（`{ sourceWindowId, message }`）送达。没有窗口是该 id 时以 `NOT_FOUND` 失败。
   */
  sendMessage(params: SendMessageParams): void;

  /**
   * Send a message to every window except the caller, delivered as `window:message` with
   * `{ sourceWindowId, message }`.
   * @zh 给除调用方以外的每个窗口发消息，以 `window:message`（`{ sourceWindowId, message }`）送达。
   */
  broadcast(params: BroadcastParams): void;

  /**
   * Report the calling page's hosting mode and window id. The main window and popups report
   * `standalone` with `panelMode: false`, including popups opened from a panel. A registered
   * window with an id reports the same id as `window.getCurrentWindowId`.
   * Pages that adapt to panel mode read it on startup; DUI/CUI panels also announce the same
   * fields with `panel:initialized`.
   * @zh 报告调用方页面的宿主模式和窗口 id。主窗口与 popup 都返回 `standalone` 和 `panelMode: false`，从面板打开的 popup 也如此。已注册且有 id 的窗口与 `window.getCurrentWindowId` 返回相同的 id。需要适配面板模式的页面在启动时读取它；DUI/CUI 面板还会以 `panel:initialized` 事件通告同样的字段。
   */
  getMode(): GetModeResult;
}

export interface Events {
  /**
   * The main window was activated or deactivated. Only the main window's page gets it; popups
   * and panels do not.
   * @zh 主窗口被激活或失去激活。只有主窗口的页面会收到；popup 与面板收不到。
   * @delivery window
   */
  activated: ActivatedPayload;

  /**
   * The main window's DPI changed, because it moved to another display or the display's scaling
   * changed. Only the main window's page gets it. Sizes are physical pixels at the new DPI; the
   * host also updates the page's titlebar CSS variables.
   * @zh 主窗口的 DPI 变了：移到了另一块显示器，或显示器的缩放改了。只有主窗口的页面会收到。尺寸是新 DPI 下的物理像素；宿主同时更新页面上的标题栏 CSS 变量。
   * @delivery window
   */
  dpiChanged: DpiChangedPayload;

  /**
   * The main window was maximized, minimized, restored, activated or deactivated, or entered or
   * left fullscreen; a popup sends it only when it enters or leaves fullscreen. It goes to every
   * window, and `windowId` says which window changed: a page that only cares about its own window
   * compares it with `window.getCurrentWindowId`. The main window sends it only when one of the
   * four states differs from what it last sent.
   * @zh 主窗口最大化、最小化、还原、激活、失去激活，或进入、退出全屏；popup 只在进入或退出全屏时发。它发给所有窗口，`windowId` 说明是哪个窗口变了：只关心自己窗口的页面拿它与 `window.getCurrentWindowId` 比较。主窗口只在四种状态之一与上次发出的不同时才发。
   * @delivery broadcast
   */
  stateChanged: StateChangedPayload;

  /**
   * The backdrop of the main window or a popup changed: it was applied with another effect,
   * switched between its active and inactive variant, or was applied again by a forced refresh.
   * Sent only after the backdrop was applied successfully.
   * @zh 主窗口或 popup 的背景变了：换了效果、在激活与失焦两种变体之间切换，或被强制刷新重新写入。只在背景写入成功之后发。
   * @delivery broadcast
   */
  backdropStateChanged: BackdropStateChangedPayload;

  /**
   * A popup created with `beforeClose` is being closed, by its close button or
   * `window.closePopup`. It stays open until its page calls `window.confirmClose` or
   * `window.cancelClose`; with neither within 3 seconds, it closes anyway.
   * @zh 创建时带 `beforeClose` 的 popup 正在被关闭（关闭按钮或 `window.closePopup`）。它保持打开，直到页面调用 `window.confirmClose` 或 `window.cancelClose`；3 秒内两者都没调用，它照样关闭。
   * @delivery window
   */
  beforeClose: BeforeClosePayload;

  /**
   * `window.setPopupBehavior` changed a popup's preset or overrides. The payload has the same
   * fields as `window.getPopupBehavior` returns.
   * @zh `window.setPopupBehavior` 改了 popup 的行为预设或覆盖项。载荷与 `window.getPopupBehavior` 的返回字段相同。
   * @delivery broadcast
   */
  behaviorChanged: GetPopupBehaviorResult;

  /**
   * The cursor entered or left a popup that lets clicks through (`window.setClickThrough`). Such
   * a window gets no mouse messages, so the host polls the cursor while click-through is on;
   * turning click-through off sends `hovering: false`.
   * @zh 光标进入或离开了一个让点击穿透的 popup（`window.setClickThrough`）。这样的窗口收不到鼠标消息，所以点击穿透开着时由宿主轮询光标位置；关闭点击穿透时发一次 `hovering: false`。
   * @delivery broadcast
   */
  hoverStateChanged: HoverStateChangedPayload;

  /**
   * A page sent a message with `window.sendMessage` or `window.broadcast`. A directed message
   * goes only to the named window; a broadcast goes to every window except the sender's, or to
   * every window when the sender's window cannot be found.
   * @zh 某个页面用 `window.sendMessage` 或 `window.broadcast` 发了消息。定向消息只发给指定的窗口；广播发给发送方以外的所有窗口，找不到发送方窗口时发给所有窗口。
   * @delivery target
   */
  message: MessagePayload;

  /**
   * A popup whose behavior has `keepVisibleOnShowDesktop` ignored a minimize command.
   * @zh 行为带 `keepVisibleOnShowDesktop` 的 popup 忽略了一次最小化命令。
   * @delivery broadcast
   */
  minimizeSuppressed: MinimizeSuppressedPayload;

  /**
   * A popup was created. The windows the host uses for its own menus do not send it.
   * @zh 创建了一个 popup。宿主自己用来显示菜单的窗口不发。
   * @delivery broadcast
   */
  popupOpened: PopupOpenedPayload;

  /**
   * A popup closed and its page was destroyed. The windows the host uses for its own menus do not
   * send it.
   * @zh 一个 popup 关闭了，页面已销毁。宿主自己用来显示菜单的窗口不发。
   * @delivery broadcast
   */
  popupClosed: PopupClosedPayload;

  /**
   * foobar2000's own Always on top option changed, from its menu or from a component. It is not
   * about the always-on-top state of this plugin's popups.
   * @zh foobar2000 自己的「总在最前」选项变了（经它的菜单或某个组件）。与本插件 popup 的置顶状态无关。
   * @delivery broadcast
   */
  alwaysOnTopChanged: AlwaysOnTopChangedPayload;
}

interface ActivatedPayload {
  /**
   * Whether the main window is now active.
   * @zh 主窗口现在是否处于激活状态。
   */
  active: boolean;
}

interface DpiChangedPayload {
  /**
   * The new DPI; 96 is 100 % scaling.
   * @zh 新的 DPI；96 对应 100% 缩放。
   */
  dpi: Int;
  /**
   * `dpi / 96`.
   * @zh `dpi / 96`。
   */
  dpiScale: number;
  /**
   * Height of the titlebar.
   * @zh 标题栏高度。
   */
  titlebarHeight: Int;
  /**
   * Width of one caption button.
   * @zh 一个标题栏按钮的宽度。
   */
  captionButtonWidth: Int;
  /**
   * Width of the three caption buttons together, `captionButtonWidth * 3`.
   * @zh 三个标题栏按钮的总宽度，即 `captionButtonWidth * 3`。
   */
  captionButtonsWidth: Int;
}

interface StateChangedPayload {
  /**
   * Id of the window that changed: `main` for the main window, else the popup's id. The same
   * value `window.getCurrentWindowId` gives that window's page.
   * @zh 状态变了的窗口的 id：主窗口为 `main`，否则为 popup 的 id。与该窗口的页面从 `window.getCurrentWindowId` 得到的值相同。
   */
  windowId: string;
  /**
   * Whether the window is maximized.
   * @zh 窗口是否最大化。
   */
  isMaximized: boolean;
  /**
   * Whether the window is minimized.
   * @zh 窗口是否最小化。
   */
  isMinimized: boolean;
  /**
   * Same as `isMaximized`.
   * @zh 同 `isMaximized`。
   */
  maximized: boolean;
  /**
   * Same as `isMinimized`.
   * @zh 同 `isMinimized`。
   */
  minimized: boolean;
  /**
   * Whether the window is active.
   * @zh 窗口是否处于激活状态。
   */
  isActive: boolean;
  /**
   * Same as `isActive`.
   * @zh 同 `isActive`。
   */
  active: boolean;
  /**
   * Whether the window is fullscreen.
   * @zh 窗口是否全屏。
   */
  isFullscreen: boolean;
  /**
   * Same as `isFullscreen`.
   * @zh 同 `isFullscreen`。
   */
  fullscreen: boolean;
}

interface BackdropStateChangedPayload {
  /**
   * Id of the window; `main` for the main window.
   * @zh 窗口 id；主窗口为 `main`。
   */
  windowId: string;
  /**
   * Whether the window is active.
   * @zh 窗口是否处于激活状态。
   */
  active: boolean;
  /**
   * Which variant was applied, matching `active`.
   * @zh 写入的是哪种变体，与 `active` 一致。
   */
  mode: 'active' | 'inactive';
  /**
   * The effect in use, after `inherit` is resolved and an effect the window cannot show falls
   * back; `system` when the platform draws the frame.
   * @zh 实际使用的效果：`inherit` 已解析，窗口显示不了的效果已回退；`system` 表示由平台绘制边框背景。
   */
  effect: 'none' | 'system' | 'mica' | 'mica-alt' | 'acrylic';
}

interface BeforeClosePayload {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
}

interface HoverStateChangedPayload {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
  /**
   * Whether the cursor is over the popup.
   * @zh 光标是否在 popup 上。
   */
  hovering: boolean;
}

interface MessagePayload {
  /**
   * Id of the sending page's window: `main`, a popup id or a panel id; `main` when the sender
   * cannot be matched to a window.
   * @zh 发送方页面所在窗口的 id：`main`、popup id 或面板 id；对不上窗口时为 `main`。
   */
  sourceWindowId: string;
  /**
   * The message as the sender passed it.
   * @zh 发送方传入的消息，原样送达。
   */
  message: Json;
}

interface MinimizeSuppressedPayload {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
  /**
   * Why the minimize was ignored; currently always `policy.keepVisibleOnShowDesktop`.
   * @zh 忽略最小化的原因；目前恒为 `policy.keepVisibleOnShowDesktop`。
   */
  reason: string;
}

interface PopupOpenedPayload {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
  /**
   * Title the popup was created with.
   * @zh 创建 popup 时给的标题。
   */
  title: string;
  /**
   * URL the popup was created with.
   * @zh 创建 popup 时给的 URL。
   */
  url: string;
}

interface PopupClosedPayload {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
}

interface AlwaysOnTopChangedPayload {
  /**
   * The new value of the option.
   * @zh 选项的新值。
   */
  enabled: boolean;
}

/**
 * Target window: `main` or a popup id; omitted or empty, the calling window. There is no fallback
 * to the main window: a caller that is not one of the plugin's windows fails with `NOT_FOUND`, and
 * a DUI/CUI panel with `PANEL_MODE_UNSUPPORTED`.
 * @zh 目标窗口：`main` 或 popup 的 id；省略或为空串时为调用方窗口。不会回退到主窗口：调用方不是插件的窗口时以 `NOT_FOUND` 失败，DUI/CUI 面板以 `PANEL_MODE_UNSUPPORTED` 失败。
 */
type TargetWindowId = string;

/**
 * Id of the window the call acted on.
 * @zh 调用实际作用的窗口的 id。
 */
type ResolvedWindowId = string;

interface ToggleMaximizeResult {
  /**
   * Whether the window is maximized once the posted request lands. After leaving fullscreen it is
   * the state the window returned to.
   * @zh 投递的请求生效后窗口是否最大化。若是退出全屏，则是窗口回到的状态。
   */
  maximized: boolean;
}

interface IsMaximizedResult {
  /**
   * Whether the window is maximized.
   * @zh 窗口是否最大化。
   */
  maximized: boolean;
  /**
   * Same value as `maximized`.
   * @zh 与 `maximized` 相同。
   */
  isMaximized: boolean;
}

interface IsMinimizedResult {
  /**
   * Whether the window is minimized.
   * @zh 窗口是否最小化。
   */
  minimized: boolean;
}

interface IsFullscreenParams {
  windowId?: TargetWindowId;
}

interface IsFullscreenResult {
  /**
   * Whether the window is fullscreen.
   * @zh 窗口是否全屏。
   */
  fullscreen: boolean;
  /**
   * Same value as `fullscreen`.
   * @zh 与 `fullscreen` 相同。
   */
  isFullscreen: boolean;
  windowId: ResolvedWindowId;
}

interface GetStateResult {
  /**
   * Whether the window is maximized.
   * @zh 窗口是否最大化。
   */
  maximized: boolean;
  /**
   * Whether the window is minimized.
   * @zh 窗口是否最小化。
   */
  minimized: boolean;
  /**
   * Whether the window is fullscreen.
   * @zh 窗口是否全屏。
   */
  fullscreen: boolean;
  /**
   * Whether the window is kept above other windows.
   * @zh 窗口是否保持在其他窗口之上。
   */
  alwaysOnTop: boolean;
  /**
   * Whether the window is the foreground window.
   * @zh 窗口是否为前台窗口。
   */
  focused: boolean;
  /**
   * Same value as `maximized`.
   * @zh 与 `maximized` 相同。
   */
  isMaximized: boolean;
  /**
   * Same value as `minimized`.
   * @zh 与 `minimized` 相同。
   */
  isMinimized: boolean;
  /**
   * Same value as `fullscreen`.
   * @zh 与 `fullscreen` 相同。
   */
  isFullscreen: boolean;
  /**
   * Same value as `alwaysOnTop`.
   * @zh 与 `alwaysOnTop` 相同。
   */
  isAlwaysOnTop: boolean;
  /**
   * Same value as `focused`.
   * @zh 与 `focused` 相同。
   */
  isFocused: boolean;
  /**
   * Width in physical pixels, frame included.
   * @zh 宽度，物理像素，含边框。
   */
  width: Int;
  /**
   * Height in physical pixels, frame included.
   * @zh 高度，物理像素，含边框。
   */
  height: Int;
  /**
   * Left edge in screen coordinates.
   * @zh 左边缘的屏幕坐标。
   */
  x: Int;
  /**
   * Top edge in screen coordinates.
   * @zh 上边缘的屏幕坐标。
   */
  y: Int;
}

interface StartResizeParams {
  /**
   * Edge or corner to drag: `left`, `right`, `top`, `bottom`, `topleft`, `topright`, `bottomleft`
   * or `bottomright`. Any other value drags the bottom-right corner.
   * @zh 要拖动的边或角：`left`、`right`、`top`、`bottom`、`topleft`、`topright`、`bottomleft` 或 `bottomright`。其他值都拖右下角。
   * @default "bottomright"
   */
  edge?: string;
}

interface SetAlwaysOnTopParams {
  /**
   * Whether to keep the window above other windows.
   * @zh 是否让窗口保持在其他窗口之上。
   * @default true
   */
  enabled?: boolean;
}

interface IsAlwaysOnTopResult {
  /**
   * Whether the window is kept above other windows.
   * @zh 窗口是否保持在其他窗口之上。
   */
  enabled: boolean;
  /**
   * Same value as `enabled`.
   * @zh 与 `enabled` 相同。
   */
  isAlwaysOnTop: boolean;
}

interface ToggleAlwaysOnTopResult {
  /**
   * Whether the window is now kept above other windows.
   * @zh 窗口现在是否保持在其他窗口之上。
   */
  enabled: boolean;
}

interface GetBoundsResult {
  /**
   * Left edge in screen coordinates.
   * @zh 左边缘的屏幕坐标。
   */
  x: Int;
  /**
   * Top edge in screen coordinates.
   * @zh 上边缘的屏幕坐标。
   */
  y: Int;
  /**
   * Width in physical pixels, frame included.
   * @zh 宽度，物理像素，含边框。
   */
  width: Int;
  /**
   * Height in physical pixels, frame included.
   * @zh 高度，物理像素，含边框。
   */
  height: Int;
}

interface SetBoundsParams {
  /**
   * New left edge in screen coordinates.
   * @zh 新的左边缘屏幕坐标。
   */
  x?: number;
  /**
   * New top edge in screen coordinates.
   * @zh 新的上边缘屏幕坐标。
   */
  y?: number;
  /**
   * New width in physical pixels, frame included.
   * @zh 新的宽度，物理像素，含边框。
   */
  width?: number;
  /**
   * New height in physical pixels, frame included.
   * @zh 新的高度，物理像素，含边框。
   */
  height?: number;
}

interface SetMinSizeParams {
  windowId?: TargetWindowId;
  /**
   * Minimum width in physical pixels; `0` or less lowers it to 1 DIP.
   * @zh 最小宽度，物理像素；`0` 及以下降到 1 DIP。
   * @default 0
   */
  width?: number;
  /**
   * Minimum height in physical pixels; `0` or less lowers it to 1 DIP.
   * @zh 最小高度，物理像素；`0` 及以下降到 1 DIP。
   * @default 0
   */
  height?: number;
}

interface SetMinSizeResult {
  windowId: ResolvedWindowId;
}

interface GetMinSizeParams {
  windowId?: TargetWindowId;
}

interface GetMinSizeResult {
  /**
   * Minimum width in physical pixels at the window's current DPI.
   * @zh 最小宽度，按窗口当前 DPI 换算的物理像素。
   */
  width: Int;
  /**
   * Minimum height in physical pixels at the window's current DPI.
   * @zh 最小高度，按窗口当前 DPI 换算的物理像素。
   */
  height: Int;
  windowId: ResolvedWindowId;
}

interface SetMaxSizeParams {
  windowId?: TargetWindowId;
  /**
   * Maximum width in physical pixels; `0` removes the bound.
   * @zh 最大宽度，物理像素；`0` 表示不设上限。
   * @default 0
   */
  width?: number;
  /**
   * Maximum height in physical pixels; `0` removes the bound.
   * @zh 最大高度，物理像素；`0` 表示不设上限。
   * @default 0
   */
  height?: number;
}

interface SetMaxSizeResult {
  windowId: ResolvedWindowId;
}

interface GetMaxSizeParams {
  windowId?: TargetWindowId;
}

interface GetMaxSizeResult {
  /**
   * Maximum width in physical pixels at the window's current DPI; `0` means no bound.
   * @zh 最大宽度，按窗口当前 DPI 换算的物理像素；`0` 表示不设上限。
   */
  width: Int;
  /**
   * Maximum height in physical pixels at the window's current DPI; `0` means no bound.
   * @zh 最大高度，按窗口当前 DPI 换算的物理像素；`0` 表示不设上限。
   */
  height: Int;
  windowId: ResolvedWindowId;
}

interface SetResizableParams {
  windowId?: TargetWindowId;
  /**
   * Whether the user can resize the window by its frame.
   * @zh 用户能否拖动边框调整窗口大小。
   * @default true
   */
  resizable?: boolean;
}

interface SetResizableResult {
  windowId: ResolvedWindowId;
}

interface IsResizableParams {
  windowId?: TargetWindowId;
}

interface IsResizableResult {
  /**
   * Whether the user can resize the window by its frame.
   * @zh 用户能否拖动边框调整窗口大小。
   */
  resizable: boolean;
  windowId: ResolvedWindowId;
}

interface SetFullscreenParams {
  windowId?: TargetWindowId;
  /**
   * `true` to enter fullscreen, `false` to leave it.
   * @zh `true` 进入全屏，`false` 退出全屏。
   * @default true
   */
  enabled?: boolean;
}

interface SetFullscreenResult {
  /**
   * Whether the window is fullscreen now.
   * @zh 窗口现在是否全屏。
   */
  fullscreen: boolean;
}

interface ToggleFullscreenParams {
  windowId?: TargetWindowId;
}

interface ToggleFullscreenResult {
  /**
   * Whether the window is fullscreen now.
   * @zh 窗口现在是否全屏。
   */
  fullscreen: boolean;
}

interface EnterFullscreenParams {
  windowId?: TargetWindowId;
}

interface EnterFullscreenResult {
  /**
   * Always `true`.
   * @zh 恒为 `true`。
   */
  isFullscreen: boolean;
}

interface ExitFullscreenParams {
  windowId?: TargetWindowId;
}

interface ExitFullscreenResult {
  /**
   * Always `false`.
   * @zh 恒为 `false`。
   */
  isFullscreen: boolean;
}

interface FocusParams {
  windowId?: TargetWindowId;
}

interface SetTitleParams {
  /**
   * The new title.
   * @zh 新标题。
   * @default "foobar2000"
   */
  title?: string;
}

interface GetTitleResult {
  /**
   * The window title.
   * @zh 窗口标题。
   */
  title: string;
}

interface FlashParams {
  /**
   * `true` to flash, `false` to stop.
   * @zh `true` 开始闪烁，`false` 停止。
   * @default true
   */
  enabled?: boolean;
  /**
   * How many times to flash.
   * @zh 闪烁次数。
   * @default 3
   */
  count?: Int;
}

interface ShowSystemMenuParams {
  /**
   * Left edge of the rectangle to keep clear, or the menu position when `w` or `h` is not
   * positive.
   * @zh 要避开的矩形的左边缘；`w` 或 `h` 不为正时是菜单的位置。
   * @default 0
   */
  x?: number;
  /**
   * Top edge of the rectangle to keep clear, or the menu position when `w` or `h` is not
   * positive.
   * @zh 要避开的矩形的上边缘；`w` 或 `h` 不为正时是菜单的位置。
   * @default 0
   */
  y?: number;
  /**
   * Width of the rectangle to keep clear.
   * @zh 要避开的矩形的宽度。
   * @default 0
   */
  w?: number;
  /**
   * Height of the rectangle to keep clear.
   * @zh 要避开的矩形的高度。
   * @default 0
   */
  h?: number;
}

interface SetPositionParams {
  /**
   * New left edge in screen coordinates.
   * @zh 新的左边缘屏幕坐标。
   * @default 0
   */
  x?: number;
  /**
   * New top edge in screen coordinates.
   * @zh 新的上边缘屏幕坐标。
   * @default 0
   */
  y?: number;
}

interface SetSizeParams {
  /**
   * New width in physical pixels, frame included.
   * @zh 新的宽度，物理像素，含边框。
   * @default 800
   */
  width?: number;
  /**
   * New height in physical pixels, frame included.
   * @zh 新的高度，物理像素，含边框。
   * @default 600
   */
  height?: number;
}

interface GetDpiScaleResult {
  /**
   * DPI; `96` is 100 %.
   * @zh DPI；`96` 即 100 %。
   */
  dpi: Int;
  /**
   * `dpi / 96`.
   * @zh `dpi / 96`。
   */
  scale: number;
}

interface FlashTaskbarParams {
  /**
   * How many times to flash.
   * @zh 闪烁次数。
   * @default 3
   */
  count?: Int;
}

interface HasSavedBoundsResult {
  /**
   * Whether a position from an earlier session is saved.
   * @zh 是否保存有此前会话的位置。
   */
  hasSavedBounds: boolean;
  /**
   * An English sentence restating `hasSavedBounds`, for logs.
   * @zh 复述 `hasSavedBounds` 的一句英文，供日志使用。
   */
  description: string;
}

interface GetTitlebarHeightResult {
  /**
   * Title bar height in physical pixels.
   * @zh 标题栏高度，物理像素。
   */
  height: Int;
}

interface SetTitlebarHeightParams {
  /**
   * Title bar height in physical pixels, 24 to 100; fractions are dropped.
   * @zh 标题栏高度，物理像素，24 到 100；小数部分舍去。
   * @default 32
   */
  height?: number;
}

interface SetTitlebarHeightResult {
  /**
   * The height that was set.
   * @zh 设置后的高度。
   */
  height: Int;
}

interface GetCaptionButtonsWidthResult {
  /**
   * Width of the three buttons together.
   * @zh 三个按钮的总宽度。
   */
  width: Int;
  /**
   * Width of one button.
   * @zh 单个按钮的宽度。
   */
  buttonWidth: Int;
}

/**
 * A rectangle in CSS pixels, from the top-left corner of the page. Fractions are dropped before
 * scaling.
 * @zh 一个矩形，CSS 像素，从页面左上角算起。缩放前舍去小数部分。
 */
interface WindowRegion {
  /**
   * Left edge.
   * @zh 左边缘。
   * @default 0
   */
  x?: number;
  /**
   * Top edge.
   * @zh 上边缘。
   * @default 0
   */
  y?: number;
  /**
   * Width.
   * @zh 宽度。
   * @default 0
   */
  width?: number;
  /**
   * Height.
   * @zh 高度。
   * @default 0
   */
  height?: number;
}

interface SetDragRegionsParams {
  /**
   * The drag rectangles; omitted, none.
   * @zh 拖动矩形；省略时为空。
   */
  regions?: WindowRegion[];
}

interface SetDragRegionsResult {
  /**
   * How many rectangles were kept.
   * @zh 保留下来的矩形个数。
   */
  count: Int;
  /**
   * Scale applied to the rectangles, the window's DPI divided by 96.
   * @zh 矩形所乘的缩放比例，即窗口 DPI 除以 96。
   */
  dpiScale: number;
}

interface SetNoDragRegionsParams {
  /**
   * The no-drag rectangles; omitted, none.
   * @zh 不可拖动矩形；省略时为空。
   */
  regions?: WindowRegion[];
}

interface SetNoDragRegionsResult {
  /**
   * How many rectangles were kept.
   * @zh 保留下来的矩形个数。
   */
  count: Int;
  /**
   * Scale applied to the rectangles, the window's DPI divided by 96.
   * @zh 矩形所乘的缩放比例，即窗口 DPI 除以 96。
   */
  dpiScale: number;
}

interface SetMaximizeButtonRegionParams {
  /**
   * Where the page draws the maximize button; omitted, the host forgets the button.
   * @zh 页面画最大化键的位置；省略时宿主忘掉这个按钮。
   */
  region?: WindowRegion;
}

interface SetMaximizeButtonRegionResult {
  /**
   * Whether a rectangle is now set: `false` after removing it, or when the rectangle had no
   * positive width and height.
   * @zh 现在是否设着矩形：移除之后，或矩形宽高不为正时为 `false`。
   */
  hasRegion: boolean;
  /**
   * The system offers Snap layouts for a maximize button, which means Windows 11 or later. Where
   * it is `false` the host never answers the rectangle as the maximize button. Where it is `true`
   * the button's `title` tooltip, which the page still shows on hover, would cover the Snap
   * layouts flyout, so leave the title off and name the button with `aria-label`.
   * @zh 系统会为最大化键提供贴靠布局，即 Windows 11 或更新。为 `false` 时宿主从不把这个矩形当作最大化键。为 `true` 时，页面悬停时仍会显示按钮的 `title` 提示，它会挡住贴靠布局浮层，所以不要设 title，改用 `aria-label` 给按钮命名。
   */
  snapLayouts: boolean;
  /**
   * Scale applied to the rectangle: the window's DPI divided by 96, times the page zoom.
   * @zh 矩形所乘的缩放比例：窗口 DPI 除以 96，再乘页面缩放。
   */
  scale: number;
}

interface GetTitlebarInfoResult {
  /**
   * Title bar height in physical pixels.
   * @zh 标题栏高度，物理像素。
   */
  height: Int;
  /**
   * Width of the three caption buttons together.
   * @zh 三个标题栏按钮的总宽度。
   */
  captionButtonsWidth: Int;
  /**
   * Width of one caption button.
   * @zh 单个标题栏按钮的宽度。
   */
  captionButtonWidth: Int;
  /**
   * Whether the main window is maximized.
   * @zh 主窗口是否最大化。
   */
  isMaximized: boolean;
}

interface SetCornerPreferenceParams {
  /**
   * `default` or `round` rounds the corners, `small` rounds them slightly and `none` keeps them
   * square. Any other value rounds them and is reported back as given.
   * @zh `default` 或 `round` 为圆角，`small` 为小圆角，`none` 为直角。其他值按圆角处理，并原样报回。
   * @default "default"
   */
  mode?: string;
}

interface GetCornerPreferenceResult {
  /**
   * The corner rounding as last set.
   * @zh 最后一次设置的圆角。
   */
  mode: string;
  /**
   * Same value as `mode`.
   * @zh 与 `mode` 相同。
   */
  preference: string;
}

interface SetMicaParams {
  windowId?: TargetWindowId;
  /**
   * `true` shows Mica; `false` removes the backdrop set through `setMica` or `setAcrylic`.
   * @zh `true` 显示 Mica；`false` 去掉经 `setMica` 或 `setAcrylic` 设置的背景。
   * @default true
   */
  enabled?: boolean;
  /**
   * `mica-alt` for the tabbed variant; any other value is `mica`.
   * @zh `mica-alt` 为标签页变体；其他值都按 `mica`。
   * @default "mica"
   */
  variant?: string;
  /**
   * Dark (`true`) or light (`false`) backdrop; omitted, unchanged.
   * @zh 深色（`true`）或浅色（`false`）背景；省略时不变。
   */
  darkMode?: boolean;
}

interface SetMicaResult {
  /**
   * Echo of `enabled`.
   * @zh 回显 `enabled`。
   */
  enabled: boolean;
  /**
   * The variant used, `mica` or `mica-alt`.
   * @zh 实际使用的变体，`mica` 或 `mica-alt`。
   */
  variant: string;
  /**
   * Echo of `darkMode`; present when it was given.
   * @zh 回显 `darkMode`；传了才有。
   */
  darkMode?: boolean;
}

interface SetMicaEffectParams {
  windowId?: TargetWindowId;
  /**
   * `true` shows Mica; `false` removes the backdrop set through `setMica` or `setAcrylic`.
   * @zh `true` 显示 Mica；`false` 去掉经 `setMica` 或 `setAcrylic` 设置的背景。
   * @default true
   */
  enabled?: boolean;
  /**
   * `mica-alt` for the tabbed variant; any other value is `mica`.
   * @zh `mica-alt` 为标签页变体；其他值都按 `mica`。
   * @default "mica"
   */
  variant?: string;
  /**
   * Dark (`true`) or light (`false`) backdrop; omitted, unchanged.
   * @zh 深色（`true`）或浅色（`false`）背景；省略时不变。
   */
  darkMode?: boolean;
}

interface SetMicaEffectResult {
  /**
   * Echo of `enabled`.
   * @zh 回显 `enabled`。
   */
  enabled: boolean;
  /**
   * The variant used, `mica` or `mica-alt`.
   * @zh 实际使用的变体，`mica` 或 `mica-alt`。
   */
  variant: string;
  /**
   * Echo of `darkMode`; present when it was given.
   * @zh 回显 `darkMode`；传了才有。
   */
  darkMode?: boolean;
}

interface SetBlurParams {
  windowId?: TargetWindowId;
  /**
   * Whether to blur what is behind the window.
   * @zh 是否模糊窗口后面的内容。
   * @default true
   */
  enabled?: boolean;
}

interface SetBlurResult {
  /**
   * Echo of `enabled`.
   * @zh 回显 `enabled`。
   */
  enabled: boolean;
}

interface SetAcrylicParams {
  windowId?: TargetWindowId;
  /**
   * `true` shows acrylic; `false` removes the backdrop set through `setMica` or `setAcrylic`.
   * @zh `true` 显示亚克力；`false` 去掉经 `setMica` 或 `setAcrylic` 设置的背景。
   * @default true
   */
  enabled?: boolean;
  /**
   * Dark (`true`) or light (`false`) backdrop; omitted, unchanged.
   * @zh 深色（`true`）或浅色（`false`）背景；省略时不变。
   */
  darkMode?: boolean;
}

interface SetAcrylicResult {
  /**
   * Echo of `enabled`.
   * @zh 回显 `enabled`。
   */
  enabled: boolean;
  /**
   * Echo of `darkMode`; present when it was given.
   * @zh 回显 `darkMode`；传了才有。
   */
  darkMode?: boolean;
}

interface SetDarkModeParams {
  windowId?: TargetWindowId;
  /**
   * `true` for the dark variant, `false` for the light one.
   * @zh `true` 为深色，`false` 为浅色。
   * @default true
   */
  enabled?: boolean;
}

interface SetDarkModeResult {
  /**
   * Echo of `enabled`.
   * @zh 回显 `enabled`。
   */
  enabled: boolean;
}

interface SetBackgroundTransparencyParams {
  windowId?: TargetWindowId;
  /**
   * `true` for a transparent background, `false` for an opaque one.
   * @zh `true` 为透明背景，`false` 为不透明。
   * @default true
   */
  transparent?: boolean;
}

interface SetBackgroundTransparencyResult {
  /**
   * Echo of `transparent`.
   * @zh 回显 `transparent`。
   */
  transparent: boolean;
  /**
   * An English sentence describing the new state, for logs.
   * @zh 描述新状态的一句英文，供日志使用。
   */
  description: string;
}

interface GetDevServerConfigResult {
  /**
   * Whether pages load from the development server.
   * @zh 页面是否从开发服务器加载。
   */
  useDevServer: boolean;
  /**
   * Address of the development server.
   * @zh 开发服务器的地址。
   */
  devServerUrl: string;
}

interface SetDevServerConfigParams {
  /**
   * Whether pages load from the development server.
   * @zh 页面是否从开发服务器加载。
   */
  useDevServer?: boolean;
  /**
   * Address of the development server, such as `http://localhost:5173`; stored as given.
   * @zh 开发服务器的地址，例如 `http://localhost:5173`；原样保存。
   */
  devServerUrl?: string;
}

interface SetDevServerConfigResult {
  /**
   * Whether pages load from the development server, as stored.
   * @zh 保存后的「页面是否从开发服务器加载」。
   */
  useDevServer: boolean;
  /**
   * Address of the development server, as stored.
   * @zh 保存后的开发服务器地址。
   */
  devServerUrl: string;
}

interface SetZoomParams {
  /**
   * Zoom factor; `1` is 100 %.
   * @zh 缩放倍数；`1` 即 100 %。
   * @default 1
   */
  zoom?: number;
}

interface SetZoomResult {
  /**
   * The zoom factor in effect after the call.
   * @zh 调用之后生效的缩放倍数。
   */
  zoom: number;
}

interface GetZoomResult {
  /**
   * The zoom factor; `1` is 100 %.
   * @zh 缩放倍数；`1` 即 100 %。
   */
  zoom: number;
  /**
   * The window's DPI; left out without a WebView.
   * @zh 窗口的 DPI；没有 WebView 时省略。
   */
  dpi?: Int;
  /**
   * `dpi / 96`; left out without a WebView.
   * @zh `dpi / 96`；没有 WebView 时省略。
   */
  dpiScale?: number;
}

interface ResetZoomResult {
  /**
   * Always `1`.
   * @zh 恒为 `1`。
   */
  zoom: number;
}

interface SetZoomForDpiParams {
  /**
   * DPI to match; `0` or less uses the window's DPI, or `96` when it is unknown.
   * @zh 要匹配的 DPI；`0` 及以下用窗口的 DPI，未知时用 `96`。
   * @default 0
   */
  dpi?: Int;
}

interface SetZoomForDpiResult {
  /**
   * The DPI used.
   * @zh 实际使用的 DPI。
   */
  dpi: Int;
  /**
   * The zoom factor in effect after the call.
   * @zh 调用之后生效的缩放倍数。
   */
  zoom: number;
}

interface SetFramelessParams {
  windowId?: TargetWindowId;
  /**
   * `true` removes the frame, `false` restores it.
   * @zh `true` 去掉边框，`false` 恢复。
   * @default true
   */
  frameless?: boolean;
}

interface SetFramelessResult {
  /**
   * Whether the window is frameless now.
   * @zh 窗口现在是否无边框。
   */
  frameless: boolean;
}

interface CreatePopupParams {
  /**
   * Page to load. An `http://`, `https://`, `file:///` or `data:` URL is loaded as it is; a path
   * containing `.html` is loaded from the theme (or the development server); anything else,
   * including empty, loads the theme's `index.html` with this value as the `route` query
   * parameter. Every URL except `data:` gets a `windowId` query parameter. A page loaded from an
   * `http://` or `https://` URL can use the bridge only when the calling page already trusts that
   * origin; any other absolute URL loads a page that cannot. Such a page still shows, but its calls
   * fail with `ORIGIN_DENIED` and no event reaches it.
   * @zh 要加载的页面。`http://`、`https://`、`file:///` 或 `data:` 地址原样加载；含 `.html` 的路径从主题（或开发服务器）加载；其他值（含空串）加载主题的 `index.html`，并把该值作为 `route` 查询参数。除 `data:` 外每个地址都会加上 `windowId` 查询参数。`http://` 或 `https://` 地址的页面只有在调用方页面已经信任该来源时才能使用桥，其他绝对地址加载的页面都不能。这样的页面照常显示，但调用以 `ORIGIN_DENIED` 失败，也收不到任何事件。
   * @default ""
   */
  url?: string;
  /**
   * Window title.
   * @zh 窗口标题。
   * @default ""
   */
  title?: string;
  /**
   * Left edge in screen coordinates; omitted, Windows places the window. Fractions are dropped.
   * @zh 左边缘的屏幕坐标；省略时由 Windows 决定位置。小数部分舍去。
   */
  x?: number;
  /**
   * Top edge in screen coordinates; omitted, Windows places the window. Fractions are dropped.
   * @zh 上边缘的屏幕坐标；省略时由 Windows 决定位置。小数部分舍去。
   */
  y?: number;
  /**
   * Width in physical pixels; fractions are dropped.
   * @zh 宽度，物理像素；小数部分舍去。
   * @default 400
   */
  width?: number;
  /**
   * Height in physical pixels; fractions are dropped.
   * @zh 高度，物理像素；小数部分舍去。
   * @default 300
   */
  height?: number;
  /**
   * Minimum width for resizing, in physical pixels.
   * @zh 调整大小时的最小宽度，物理像素。
   * @default 200
   */
  minWidth?: number;
  /**
   * Minimum height for resizing, in physical pixels.
   * @zh 调整大小时的最小高度，物理像素。
   * @default 150
   */
  minHeight?: number;
  /**
   * Maximum width for resizing, in physical pixels; `0` means no bound.
   * @zh 调整大小时的最大宽度，物理像素；`0` 表示不设上限。
   * @default 0
   */
  maxWidth?: number;
  /**
   * Maximum height for resizing, in physical pixels; `0` means no bound.
   * @zh 调整大小时的最大高度，物理像素；`0` 表示不设上限。
   * @default 0
   */
  maxHeight?: number;
  /**
   * Whether the user can resize the popup.
   * @zh 用户能否调整 popup 的大小。
   * @default true
   */
  resizable?: boolean;
  /**
   * Whether to draw the native frame and caption; `false` creates a borderless popup.
   * @zh 是否画原生边框与标题栏；`false` 创建无边框 popup。
   * @default true
   */
  frame?: boolean;
  /**
   * Whether the background is transparent.
   * @zh 背景是否透明。
   * @default false
   */
  transparent?: boolean;
  /**
   * Keep the popup above every other window. To keep it above the main window only, use the
   * `standard` preset or `behavior.owner: "main"` instead.
   * @zh 让 popup 保持在所有其他窗口之上。只想让它位于主窗口之上时，改用 `standard` 预设或 `behavior.owner: "main"`。
   * @default false
   */
  alwaysOnTop?: boolean;
  /**
   * Whether the popup shows on the taskbar and in Alt+Tab; omitted, the preset decides.
   * @zh popup 是否出现在任务栏与 Alt+Tab 里；省略时由预设决定。
   */
  showInTaskbar?: boolean;
  /**
   * Let mouse input pass through the popup.
   * @zh 让鼠标输入穿过 popup。
   * @default false
   */
  clickThrough?: boolean;
  /**
   * Send `window:beforeClose` and wait for `window.confirmClose` or `window.cancelClose`
   * before closing.
   * @zh 关闭前先发 `window:beforeClose`，等待 `window.confirmClose` 或 `window.cancelClose`。
   * @default false
   */
  beforeClose?: boolean;
  /**
   * Behavior preset: `standard`, `miniPlayer` or `desktopLyrics`, case-insensitive, with `-` or
   * `_` allowed (`mini-player`); an unknown value is `standard`. Omitted, no preset.
   * @zh 行为预设：`standard`、`miniPlayer` 或 `desktopLyrics`，不区分大小写，可写 `-` 或 `_`（`mini-player`）；认不出的值按 `standard`。省略时不用预设。
   */
  profile?: string;
  /**
   * Behavior overrides on top of the preset, keyed like `resolvedBehavior` of
   * `window.getPopupBehavior`.
   * @zh 叠加在预设之上的行为覆盖，键同 `window.getPopupBehavior` 的 `resolvedBehavior`。
   */
  behavior?: Record<string, Json>;
  /**
   * Backdrop policy overrides on top of the preset, keyed like `resolvedBackdropPolicy` of
   * `window.getBackdropPolicy`.
   * @zh 叠加在预设之上的背景策略覆盖，键同 `window.getBackdropPolicy` 的 `resolvedBackdropPolicy`。
   */
  backdropPolicy?: Record<string, Json>;
}

interface CreatePopupResult {
  /**
   * Id of the new popup.
   * @zh 新 popup 的 id。
   */
  windowId: string;
}

interface ClosePopupParams {
  /**
   * Id of the popup to close.
   * @zh 要关闭的 popup 的 id。
   * @minLength 1
   */
  windowId: string;
}

/**
 * A window rectangle in screen coordinates, physical pixels, frame included.
 * @zh 窗口矩形：屏幕坐标，物理像素，含边框。
 */
interface WindowBounds {
  /**
   * Left edge.
   * @zh 左边缘。
   */
  x: Int;
  /**
   * Top edge.
   * @zh 上边缘。
   */
  y: Int;
  /**
   * Width.
   * @zh 宽度。
   */
  width: Int;
  /**
   * Height.
   * @zh 高度。
   */
  height: Int;
}

/**
 * The backdrop policy a window uses after defaults and overrides are combined.
 * @zh 窗口合并默认值与覆盖项之后实际使用的背景策略。
 */
interface WindowBackdropPolicyState {
  /**
   * Backdrop while the window is active; `inherit` follows the foobar2000 preferences.
   * @zh 窗口激活时的背景；`inherit` 跟随 foobar2000 首选项。
   */
  activeEffect: 'inherit' | 'none' | 'mica' | 'mica-alt' | 'acrylic';
  /**
   * Backdrop while the window is inactive: `inherit` keeps the active one and lets Windows dim
   * it, `system` hands the frame back to the platform backdrop.
   * @zh 窗口失焦时的背景：`inherit` 沿用激活时的效果、由 Windows 调暗，`system` 交还平台背景。
   */
  inactiveEffect: 'inherit' | 'system' | 'none' | 'mica' | 'mica-alt' | 'acrylic';
  /**
   * Whether the backdrop uses its dark variant.
   * @zh 背景是否用深色变体。
   */
  darkMode: boolean;
  /**
   * Whether the backdrop is written again on every activation.
   * @zh 每次激活时是否重新写入背景。
   */
  reapplyOnActivate: boolean;
}

/**
 * The behavior a popup uses after its preset and overrides are combined.
 * @zh popup 合并预设与覆盖项之后实际使用的行为。
 */
interface WindowPopupBehaviorState {
  /**
   * Whether the popup shows on the taskbar.
   * @zh 是否出现在任务栏上。
   */
  showInTaskbar: boolean;
  /**
   * Whether the popup shows in Alt+Tab.
   * @zh 是否出现在 Alt+Tab 里。
   */
  showInAltTab: boolean;
  /**
   * Whether the popup stays visible when the desktop is shown.
   * @zh 显示桌面时是否仍然可见。
   */
  keepVisibleOnShowDesktop: boolean;
  /**
   * Whether the popup can be minimized.
   * @zh 能否最小化。
   */
  allowMinimize: boolean;
  /**
   * `main` keeps the popup above the main window and minimizes it with the main window; `none`
   * makes it independent.
   * @zh `main` 让 popup 位于主窗口之上并随主窗口最小化；`none` 让它独立。
   */
  owner: 'none' | 'main';
  /**
   * Whether showing or clicking the popup leaves the focus where it was.
   * @zh 显示或点击 popup 时是否不抢焦点。
   */
  noActivate: boolean;
}

/**
 * Features a window supports; the last three are reported for popups only.
 * @zh 窗口支持的功能；最后三项只有 popup 才报。
 */
interface WindowObservationCapabilities {
  /**
   * Whether `window.setBackdropPolicy` applies.
   * @zh `window.setBackdropPolicy` 是否生效。
   */
  supportsBackdropPolicy: boolean;
  /**
   * Whether `window.setFrameless` applies.
   * @zh `window.setFrameless` 是否生效。
   */
  supportsFrameless: boolean;
  /**
   * Whether `window.setCornerPreference` applies.
   * @zh `window.setCornerPreference` 是否生效。
   */
  supportsCornerPreference: boolean;
  /**
   * Whether `window.setPopupBehavior` applies.
   * @zh `window.setPopupBehavior` 是否生效。
   */
  supportsPopupBehavior: boolean;
  /**
   * Whether the Mica Alt backdrop can be drawn.
   * @zh 能否画 Mica Alt 背景。
   */
  supportsMicaAlt: boolean;
  /**
   * Whether the window can go fullscreen.
   * @zh 能否进入全屏。
   */
  supportsFullscreen: boolean;
  /**
   * Whether `behavior.owner` applies.
   * @zh `behavior.owner` 是否生效。
   */
  supportsOwnerPolicy?: boolean;
  /**
   * Whether `behavior.noActivate` applies.
   * @zh `behavior.noActivate` 是否生效。
   */
  supportsNoActivate?: boolean;
  /**
   * Whether `beforeClose` applies.
   * @zh `beforeClose` 是否生效。
   */
  supportsBeforeClose?: boolean;
}

/**
 * One window of `window.getAllWindows`. The popup-only fields are absent for the main window.
 * @zh `window.getAllWindows` 列出的一个窗口。主窗口没有只属于 popup 的字段。
 */
interface WindowInfo {
  /**
   * `main` or the popup id.
   * @zh `main` 或 popup 的 id。
   */
  windowId: string;
  /**
   * Whether this is the main window.
   * @zh 是否为主窗口。
   */
  isMain: boolean;
  /**
   * Window title.
   * @zh 窗口标题。
   */
  title: string;
  /**
   * Popups only: the `url` the popup was created with.
   * @zh 仅 popup：创建 popup 时给的 `url`。
   */
  url?: string;
  /**
   * Popups only: the behavior preset; `legacy` when the popup was created without one.
   * @zh 仅 popup：行为预设；创建时没给预设为 `legacy`。
   */
  profile?: 'legacy' | 'standard' | 'miniPlayer' | 'desktopLyrics';
  /**
   * Popups only: the behavior overrides set on the popup.
   * @zh 仅 popup：在 popup 上设置的行为覆盖。
   */
  behavior?: Record<string, Json>;
  /**
   * Popups only: the behavior in effect.
   * @zh 仅 popup：实际生效的行为。
   */
  resolvedBehavior?: WindowPopupBehaviorState;
  /**
   * The backdrop policy overrides set on the window.
   * @zh 在窗口上设置的背景策略覆盖。
   */
  backdropPolicy: Record<string, Json>;
  /**
   * The backdrop policy in effect.
   * @zh 实际生效的背景策略。
   */
  resolvedBackdropPolicy: WindowBackdropPolicyState;
  /**
   * Features the window supports.
   * @zh 窗口支持的功能。
   */
  capabilities: WindowObservationCapabilities;
  /**
   * The window rectangle.
   * @zh 窗口矩形。
   */
  bounds: WindowBounds;
  /**
   * Diagnostic snapshot of the window shell (lifecycle and startup state); its shape can change
   * between versions.
   * @zh 窗口外壳的诊断快照（生命周期与启动状态）；形状可能随版本变化。
   */
  shell: Json;
}

interface GetAllWindowsResult {
  /**
   * The main window first, then the popups.
   * @zh 主窗口在前，其后是各 popup。
   */
  items: WindowInfo[];
}

interface GetCurrentWindowIdResult {
  /**
   * Id of the calling window.
   * @zh 调用方窗口的 id。
   */
  windowId: string;
}

interface GetPopupBehaviorParams {
  /**
   * Id of the popup; omitted or empty, the calling popup.
   * @zh popup 的 id；省略或为空串时为调用方 popup。
   */
  windowId?: string;
}

interface GetPopupBehaviorResult {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
  /**
   * The behavior preset; `legacy` when the popup was created without one.
   * @zh 行为预设；创建时没给预设为 `legacy`。
   */
  profile: 'legacy' | 'standard' | 'miniPlayer' | 'desktopLyrics';
  /**
   * The behavior overrides set on the popup.
   * @zh 在 popup 上设置的行为覆盖。
   */
  behavior: Record<string, Json>;
  /**
   * The behavior in effect.
   * @zh 实际生效的行为。
   */
  resolvedBehavior: WindowPopupBehaviorState;
}

interface SetPopupBehaviorParams {
  /**
   * Id of the popup; omitted or empty, the calling popup.
   * @zh popup 的 id；省略或为空串时为调用方 popup。
   */
  windowId?: string;
  /**
   * New preset: `standard`, `miniPlayer` or `desktopLyrics`, case-insensitive, with `-` or `_`
   * allowed (`mini-player`).
   * @zh 新的预设：`standard`、`miniPlayer` 或 `desktopLyrics`，不区分大小写，可写 `-` 或 `_`（`mini-player`）。
   */
  profile?: string;
  /**
   * Overrides merged into the current ones; a `null` value removes that override.
   * @zh 合并进现有覆盖的新覆盖；值为 `null` 的键删除该覆盖。
   */
  behavior?: Record<string, Json>;
}

interface SetPopupBehaviorResult {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
  /**
   * The behavior preset.
   * @zh 行为预设。
   */
  profile: 'legacy' | 'standard' | 'miniPlayer' | 'desktopLyrics';
  /**
   * The behavior overrides after the merge.
   * @zh 合并之后的行为覆盖。
   */
  behavior: Record<string, Json>;
  /**
   * The behavior in effect.
   * @zh 实际生效的行为。
   */
  resolvedBehavior: WindowPopupBehaviorState;
}

interface GetBackdropPolicyParams {
  windowId?: TargetWindowId;
}

interface GetBackdropPolicyResult {
  windowId: ResolvedWindowId;
  /**
   * The backdrop policy overrides set on the window.
   * @zh 在窗口上设置的背景策略覆盖。
   */
  backdropPolicy: Record<string, Json>;
  /**
   * The backdrop policy in effect.
   * @zh 实际生效的背景策略。
   */
  resolvedBackdropPolicy: WindowBackdropPolicyState;
}

interface SetBackdropPolicyParams {
  windowId?: TargetWindowId;
  /**
   * Overrides keyed like `resolvedBackdropPolicy`, merged into the current ones; a `null` value
   * removes that override. Other keys are stored and echoed but have no effect, and an effect
   * name outside the allowed values leaves the effect in use unchanged.
   * @zh 以 `resolvedBackdropPolicy` 的键为键的覆盖，合并进现有覆盖；值为 `null` 的键删除该覆盖。其他键照存照回显但不起作用，取值表之外的效果名不改变正在使用的效果。
   */
  backdropPolicy: Record<string, Json>;
}

interface SetBackdropPolicyResult {
  windowId: ResolvedWindowId;
  /**
   * The backdrop policy overrides after the merge.
   * @zh 合并之后的背景策略覆盖。
   */
  backdropPolicy: Record<string, Json>;
  /**
   * The backdrop policy in effect.
   * @zh 实际生效的背景策略。
   */
  resolvedBackdropPolicy: WindowBackdropPolicyState;
}

interface SetClickThroughParams {
  /**
   * Id of the popup; omitted or empty, the calling window.
   * @zh popup 的 id；省略或为空串时为调用方窗口。
   */
  windowId?: string;
  /**
   * Whether mouse input passes through.
   * @zh 鼠标输入是否穿过。
   * @default true
   */
  enabled?: boolean;
}

interface SetClickThroughResult {
  /**
   * Whether mouse input passes through now.
   * @zh 现在鼠标输入是否穿过。
   */
  clickThrough: boolean;
}

interface IsClickThroughParams {
  /**
   * Id of the popup; omitted or empty, the calling window.
   * @zh popup 的 id；省略或为空串时为调用方窗口。
   */
  windowId?: string;
}

interface IsClickThroughResult {
  /**
   * Whether mouse input passes through.
   * @zh 鼠标输入是否穿过。
   */
  clickThrough: boolean;
}

interface SetClickThroughExcludeRegionsParams {
  /**
   * Id of the popup; omitted or empty, the calling window.
   * @zh popup 的 id；省略或为空串时为调用方窗口。
   */
  windowId?: string;
  /**
   * The rectangles; omitted, none.
   * @zh 矩形；省略时为空。
   */
  regions?: WindowRegion[];
}

interface SetClickThroughExcludeRegionsResult {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
  /**
   * How many rectangles were kept.
   * @zh 保留下来的矩形个数。
   */
  count: Int;
  /**
   * Scale applied to the rectangles, the popup's DPI divided by 96.
   * @zh 矩形所乘的缩放比例，即 popup 的 DPI 除以 96。
   */
  dpiScale: number;
  /**
   * `regions truncated to 32` when more than 32 rectangles were given.
   * @zh 给出的矩形超过 32 个时为 `regions truncated to 32`。
   */
  warning?: string;
}

interface ClearClickThroughExcludeRegionsParams {
  /**
   * Id of the popup; omitted or empty, the calling window.
   * @zh popup 的 id；省略或为空串时为调用方窗口。
   */
  windowId?: string;
}

interface ClearClickThroughExcludeRegionsResult {
  /**
   * Id of the popup.
   * @zh popup 的 id。
   */
  windowId: string;
}

interface SendMessageParams {
  /**
   * Id of the receiving window.
   * @zh 接收窗口的 id。
   * @minLength 1
   */
  targetWindowId: string;
  /**
   * The message, delivered as `message` of the event. `null` counts as missing.
   * @zh 消息，作为事件的 `message` 送达。`null` 按没传处理。
   */
  message: Json;
}

interface BroadcastParams {
  /**
   * The message, delivered as `message` of the event. `null` counts as missing.
   * @zh 消息，作为事件的 `message` 送达。`null` 按没传处理。
   */
  message: Json;
}

interface GetModeResult {
  /**
   * `standalone`, `dui` or `cui`; `panel` when the page is in a panel that could not be told
   * apart, `unknown` for a panel of an unrecognized kind.
   * @zh `standalone`、`dui` 或 `cui`；页面在面板里但分辨不出类型时为 `panel`，面板类型未知时为 `unknown`。
   */
  mode: 'standalone' | 'dui' | 'cui' | 'panel' | 'unknown';
  /**
   * Whether the page is in a DUI or CUI panel; `false` for the main window and popups.
   * @zh 页面是否在 DUI 或 CUI 面板里；主窗口与 popup 为 `false`。
   */
  panelMode: boolean;
  /**
   * Id of the calling window: `main` for the main window, the popup's actual id, or the panel's
   * id. A panel without an id reports `panel`; an unmatched caller reports `main`.
   * @zh 调用方窗口的 id：主窗口为 `main`，popup 为实际 popup id，面板为面板 id。面板没有 id 时为 `panel`；无法匹配的调用方为 `main`。
   */
  windowId: string;
}
