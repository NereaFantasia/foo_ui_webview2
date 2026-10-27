import type { Int } from './common.js';

export interface Api {
  /**
   * Install the thumbnail toolbar shown on the taskbar preview. Windows lets a window install it
   * once; change the buttons afterwards with `taskbar.updateButton`.
   * @zh 配置任务栏缩略图预览工具栏按钮（Windows 系统限制每个窗口仅可初始化安装一次；后续状态变更须调用 taskbar.updateButton）
   */
  setThumbnailButtons(params: SetThumbnailButtonsParams): void;

  /**
   * Update one installed thumbnail button in place. Buttons cannot be added or removed.
   * @zh 更新已安装缩略图按钮的状态与属性（工具栏不支持动态增删按钮）
   */
  updateButton(params: UpdateButtonParams): void;

  /**
   * Set the progress bar drawn on the taskbar button. Once a theme sets it, the plugin stops
   * mirroring playback progress there until the next playback state change.
   * @zh 设置任务栏图标上的进度条状态与进度值（主题主动写入进度后，宿主暂停自动镜像播放进度，直至下一次播放状态发生变更）
   */
  setProgress(params: SetProgressParams): void;

  /**
   * Draw a small overlay badge on the taskbar button, or clear it.
   * @zh 设置或清除任务栏图标上的状态徽标图标
   */
  setOverlayIcon(params: SetOverlayIconParams): void;

  /**
   * Flash the taskbar button to draw attention.
   * @zh 触发任务栏窗口按钮闪烁以提示用户关注
   */
  flash(params: FlashParams): void;
}

export interface Events {
  /**
   * A thumbnail toolbar button installed with `taskbar.setThumbnailButtons` was clicked. The
   * default playback buttons shown before a page installs its own run natively and send
   * nothing.
   * @zh 缩略图工具栏自定义按钮被点击时广播（未配置自定义按钮前显示的默认原生播放控制按钮不派发此事件）
   * @delivery broadcast
   */
  buttonClicked: ButtonClickedPayload;
}

interface ButtonClickedPayload {
  /**
   * The button's `id`.
   * @zh 触发点击事件的按钮唯一标识符 id
   */
  id: string;
}

/** One button of the thumbnail toolbar. */
interface ThumbnailButton {
  /**
   * Identifier reported by `taskbar:buttonClicked` and used by `taskbar.updateButton`.
   * @zh 按钮唯一标识符（用于 taskbar:buttonClicked 事件派发与 taskbar.updateButton 状态更新）
   * @minLength 1
   */
  id: string;
  /**
   * Button icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted uses
   * the foobar2000 main icon.
   * @zh 按钮图标（.ico 文件未经前缀修饰的原始 Base64 编码字符串；为空、null 或省略时使用 foobar2000 主程序图标）
   */
  icon?: string | null;
  /**
   * Hover text.
   * @zh 鼠标悬停时的提示文本
   * @default ""
   */
  tooltip?: string;
  /**
   * Whether the button accepts clicks.
   * @zh 按钮是否处于启用可点击状态
   * @default true
   */
  enabled?: boolean;
  /**
   * Whether the button is shown.
   * @zh 按钮是否可见
   * @default true
   */
  visible?: boolean;
  /**
   * Close the thumbnail preview after a click.
   * @zh 点击按钮后是否自动收起任务栏缩略图预览窗口
   * @default false
   */
  dismissOnClick?: boolean;
}

// ---- setThumbnailButtons ----

interface SetThumbnailButtonsParams {
  /**
   * Buttons in display order, at most seven. A longer list fails the whole call; it is never
   * truncated.
   * @zh 缩略图按钮配置列表（按显示顺序排列，最多允许 7 个；超出限制将直接报错拒绝而不做截断）
   */
  buttons: ThumbnailButton[];
}

// ---- updateButton ----

interface UpdateButtonParams {
  /**
   * Id of the button, as passed to `taskbar.setThumbnailButtons`.
   * @zh 待更新按钮的标识符（对应 taskbar.setThumbnailButtons 中配置的 id）
   * @minLength 1
   */
  id: string;
  /**
   * New icon as raw Base64 of an `.ico` file, without a prefix. Empty, `null` or omitted keeps
   * the current icon.
   * @zh 新按钮图标（.ico 文件原始 Base64 编码；为空、null 或省略时保持原图标不变）
   */
  icon?: string | null;
  /**
   * New hover text. Empty or omitted keeps the current text.
   * @zh 新悬停提示文本（省略时保持原提示不变）
   * @default ""
   */
  tooltip?: string;
  /**
   * Whether the button accepts clicks. Omitted keeps the current state.
   * @zh 按钮是否可点击（省略时保持原状态不变）
   */
  enabled?: boolean;
  /**
   * Whether the button is shown. Omitted keeps the current state.
   * @zh 按钮是否显示（省略时保持原状态不变）
   */
  visible?: boolean;
}

// ---- setProgress ----

interface SetProgressParams {
  /**
   * Progress bar state. `none` removes the bar.
   * @zh 进度条显示状态（none 清除进度条、indeterminate 不确定循环、normal 正常绿色、error 红色错误、paused 黄色暂停）
   * @default "none"
   */
  state?: 'none' | 'indeterminate' | 'normal' | 'error' | 'paused';
  /**
   * Fill fraction, meaningful for the `normal`, `error` and `paused` states.
   * @zh 进度条填充比例（取值范围 0.0 至 1.0，仅在 normal、error、paused 状态下生效）
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
   * @zh 徽标图标（.ico 文件的原始 Base64 编码；为空、null 或省略时清除徽标）
   */
  icon?: string | null;
  /**
   * Accessibility text for the overlay.
   * @zh 徽标图标的无障碍辅助说明文本
   * @default ""
   */
  description?: string;
}

// ---- flash ----

interface FlashParams {
  /**
   * Number of flashes.
   * @zh 任务栏按钮闪烁总次数
   * @default 3
   * @minimum 0
   */
  count?: Int;
  /**
   * Milliseconds between flashes; `0` uses the system cursor blink rate.
   * @zh 闪烁间隔时间（单位毫秒；传入 0 表示采用系统默认光标闪烁速率）
   * @default 0
   * @minimum 0
   */
  interval?: Int;
}
