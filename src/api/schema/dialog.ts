import type { Int } from './common.js';

export interface Api {
  /**
   * Show the system dialog for opening files.
   * @zh 打开文件选择对话框。
   */
  openFile(params: OpenFileParams): OpenFileResult;

  /**
   * Show the system dialog for saving a file.
   * @zh 打开文件保存对话框。
   */
  saveFile(params: SaveFileParams): SaveFileResult;

  /**
   * Show the system dialog for choosing a folder.
   * @zh 打开文件夹选择对话框。
   */
  openFolder(params: OpenFolderParams): OpenFolderResult;

  /**
   * Show a modal task dialog with custom buttons. Escape and the close button do not dismiss it.
   * @zh 显示模态确认对话框（Windows TaskDialog），支持自定义按钮；Esc 与关闭按钮不能关闭它。
   */
  confirm(params: ConfirmParams): ConfirmResult;
}

/** One entry in the dialog's file type list. */
interface FileFilter {
  /**
   * Label shown in the file type list. Defaults to "Files", localized to the UI language.
   * @zh 文件类型列表里显示的名称。默认“文件”，随界面语言本地化。
   */
  name?: string;
  /**
   * Extensions without the leading dot, such as `flac`; `*` matches every file.
   * @zh 不带点的扩展名，如 `flac`；`*` 匹配所有文件。
   */
  extensions?: string[];
}

/**
 * File type list. When omitted or empty, the dialog shows a single "All Files" entry.
 * @zh 文件类型过滤器。缺省或为空时只显示“所有文件”一项。
 */
type FileFilters = FileFilter[];

/**
 * Folder the dialog opens in, every time it is shown. `%music%` expands to the Music folder.
 * Ignored when the path does not resolve to a folder.
 * @zh 对话框打开时定位到的目录，每次打开都定位；支持 `%music%`。路径解析不到文件夹时忽略。
 * @default ""
 */
type InitialFolder = string;

// ---- openFile ----

interface OpenFileParams {
  /**
   * Window title. Defaults to "Open File", localized to the UI language.
   * @zh 窗口标题。默认“打开文件”，随界面语言本地化。
   */
  title?: string;
  /**
   * Allow selecting several files.
   * @zh 允许多选。
   * @default false
   */
  multiple?: boolean;
  defaultPath?: InitialFolder;
  filters?: FileFilters;
}

interface OpenFileResult {
  /**
   * `true` when the user closed the dialog without choosing.
   * @zh 用户未选择就关闭对话框时为 `true`。
   */
  canceled: boolean;
  /**
   * Chosen files; empty when canceled.
   * @zh 选中的文件；取消时为空数组。
   */
  filePaths: string[];
}

// ---- saveFile ----

interface SaveFileParams {
  /**
   * Window title. Defaults to "Save File", localized to the UI language.
   * @zh 窗口标题。默认“保存文件”，随界面语言本地化。
   */
  title?: string;
  /**
   * File name filled in when the dialog opens.
   * @zh 预填的文件名。
   * @default ""
   */
  defaultName?: string;
  filters?: FileFilters;
}

interface SaveFileResult {
  /**
   * `true` when the user closed the dialog without choosing.
   * @zh 用户未选择就关闭对话框时为 `true`。
   */
  canceled: boolean;
  /**
   * Chosen file; empty when canceled. The system asks before an existing file is overwritten.
   * @zh 选定的文件；取消时为空字符串。覆盖已有文件前系统会先询问。
   */
  filePath: string;
}

// ---- openFolder ----

interface OpenFolderParams {
  /**
   * Window title. Defaults to "Select Folder", localized to the UI language.
   * @zh 窗口标题。默认“选择文件夹”，随界面语言本地化。
   */
  title?: string;
  defaultPath?: InitialFolder;
}

interface OpenFolderResult {
  /**
   * `true` when the user closed the dialog without choosing.
   * @zh 用户未选择就关闭对话框时为 `true`。
   */
  canceled: boolean;
  /**
   * Chosen folder, which need not be `defaultPath`; empty when canceled.
   * @zh 用户确认的目录，不一定等于 `defaultPath`；取消时为空字符串。
   */
  folderPath: string;
}

// ---- confirm ----

interface ConfirmParams {
  /**
   * Window title. Defaults to "Confirm", localized to the UI language.
   * @zh 窗口标题。默认“确认”，随界面语言本地化。
   */
  title?: string;
  /**
   * Body text.
   * @zh 正文文本。
   * @default ""
   */
  message?: string;
  /**
   * Icon. The task dialog shows `question` with the information icon.
   * @zh 图标。TaskDialog 没有问号图标，`question` 显示为信息图标。
   * @default "question"
   */
  type?: 'info' | 'warning' | 'error' | 'question';
  /**
   * Button labels, in order. Defaults to OK and Cancel, localized to the UI language.
   * @zh 按钮文本，按顺序排列。默认“确定”“取消”，随界面语言本地化。
   */
  buttons?: string[];
  /**
   * Index of the initially focused button.
   * @zh 默认聚焦的按钮索引。
   * @default 0
   */
  defaultButton?: Int;
}

interface ConfirmResult {
  /**
   * Zero-based index of the clicked button in `buttons`. `-1` only when not even the fallback
   * message box could be shown.
   * @zh 被点按钮在 `buttons` 中的索引（从 0 开始）。只有连备用消息框都无法显示时才为 `-1`。
   */
  response: Int;
}
