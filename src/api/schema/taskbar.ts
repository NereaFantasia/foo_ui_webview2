import type { Int } from './common.js';

export interface Api {
  /**
   * Install the thumbnail toolbar shown on the taskbar preview. Windows lets a window install it
   * once; change the buttons afterwards with `taskbar.updateButton`.
   * @zh 安装任务栏预览缩略图上的工具栏。Windows 只允许每个窗口安装一次，之后用 `taskbar.updateButton` 改按钮状态。
   */
  setThumbnailButtons(params: SetThumbnailButtonsParams): void;

  /**
   * Update one installed thumbnail button in place. Buttons cannot be added or removed.
   * @zh 原地更新一个已安装的缩略图按钮，不能新增或删除按钮。
   */
  updateButton(params: UpdateButtonParams): void;

  /**
   * Set the progress bar drawn on the taskbar button. Once a theme sets it, the plugin stops
   * mirroring playback progress there until the next playback state change.
   * @zh 设置任务栏按钮上的进度条。主题写过进度后，插件到下一次播放状态变化前不再用播放进度覆盖它。
   */
  setProgress(params: SetProgressParams): void;

  /**
   * Draw a small overlay badge on the taskbar button, or clear it.
   * @zh 在任务栏按钮上叠加一个小徽标，或清除它。
   */
  setOverlayIcon(params: SetOverlayIconParams): void;

  /**
   * Flash the taskbar button to draw attention.
   * @zh 闪烁任务栏按钮以吸引注意。
   */
  flash(params: FlashParams): void;
}

export interface Events {
  /**
   * A thumbnail toolbar button installed with `taskbar.setThumbnailButtons` was clicked. The
   * default playback buttons shown before a page installs its own run natively and send
   * nothing.
   * @zh 用 `taskbar.setThumbnailButtons` 安装的缩略图工具栏按钮被点击。页面安装自己的按钮之前显示的默认播放按钮由插件原生执行，不发此事件。
   * @delivery broadcast
   */
  buttonClicked: ButtonClickedPayload;
}

interface ButtonClickedPayload {
  /**
   * The button's `id`.
   * @zh 按钮的 `id`。
   */
  id: string;
}

/** One button of the thumbnail toolbar. */
interface ThumbnailButton {
  /**
   * Identifier reported by `taskbar:buttonClicked` and used by `taskbar.updateButton`.
   * @zh 按钮标识；`taskbar:buttonClicked` 事件与 `taskbar.updateButton` 都用它指认按钮。
   * @minLength 1
   */
  id: string;
  /**
   * Button icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted uses
   * the foobar2000 main icon.
   * @zh 按钮图标，裸 Base64 编码的 `.ico` 文件字节，不带前缀。空、`null` 或省略时用 foobar2000 主图标。
   */
  icon?: string | null;
  /**
   * Hover text.
   * @zh 悬停提示文本。
   * @default ""
   */
  tooltip?: string;
  /**
   * Whether the button accepts clicks.
   * @zh 是否可点击。
   * @default true
   */
  enabled?: boolean;
  /**
   * Whether the button is shown.
   * @zh 是否显示。
   * @default true
   */
  visible?: boolean;
  /**
   * Close the thumbnail preview after a click.
   * @zh 点击后是否关闭缩略图预览。
   * @default false
   */
  dismissOnClick?: boolean;
}

// ---- setThumbnailButtons ----

interface SetThumbnailButtonsParams {
  /**
   * Buttons in display order, at most seven. A longer list fails the whole call; it is never
   * truncated.
   * @zh 按显示顺序排列的按钮，最多七个。超过时整个调用失败，不会截断。
   */
  buttons: ThumbnailButton[];
}

// ---- updateButton ----

interface UpdateButtonParams {
  /**
   * Id of the button, as passed to `taskbar.setThumbnailButtons`.
   * @zh 按钮标识，即传给 `taskbar.setThumbnailButtons` 的 `id`。
   * @minLength 1
   */
  id: string;
  /**
   * New icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted keeps
   * the current icon.
   * @zh 新图标，裸 Base64 编码的 `.ico` 文件字节，不带前缀。空、`null` 或省略时保持当前图标。
   */
  icon?: string | null;
  /**
   * New hover text. Empty or omitted keeps the current text.
   * @zh 新的悬停提示文本。空或省略时保持当前文本。
   * @default ""
   */
  tooltip?: string;
  /**
   * Whether the button accepts clicks. Omitted keeps the current state.
   * @zh 是否可点击。省略时保持当前状态。
   */
  enabled?: boolean;
  /**
   * Whether the button is shown. Omitted keeps the current state.
   * @zh 是否显示。省略时保持当前状态。
   */
  visible?: boolean;
}

// ---- setProgress ----

interface SetProgressParams {
  /**
   * Progress bar state. `none` removes the bar.
   * @zh 进度条状态，`none` 表示不显示进度条。
   * @default "none"
   */
  state?: 'none' | 'indeterminate' | 'normal' | 'error' | 'paused';
  /**
   * Fill fraction, meaningful for the `normal`, `error` and `paused` states.
   * @zh 填充比例，对 `normal`、`error`、`paused` 三种状态有意义。
   * @minimum 0
   * @maximum 1
   */
  value?: number;
}

// ---- setOverlayIcon ----

interface SetOverlayIconParams {
  /**
   * Overlay icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted
   * clears the overlay.
   * @zh 叠加图标，裸 Base64 编码的 `.ico` 文件字节，不带前缀。空、`null` 或省略时清除叠加图标。
   */
  icon?: string | null;
  /**
   * Accessibility text for the overlay.
   * @zh 叠加图标的无障碍说明文本。
   * @default ""
   */
  description?: string;
}

// ---- flash ----

interface FlashParams {
  /**
   * Number of flashes.
   * @zh 闪烁次数。
   * @default 3
   * @minimum 0
   */
  count?: Int;
  /**
   * Milliseconds between flashes; `0` uses the system cursor blink rate.
   * @zh 两次闪烁的间隔毫秒数，`0` 用系统光标闪烁速率。
   * @default 0
   * @minimum 0
   */
  interval?: Int;
}
