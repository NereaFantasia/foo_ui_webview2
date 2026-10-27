import type { Int } from './common.js';

export interface Api {
  /**
   * Show the system dialog for opening files.
   * @zh 弹出系统原生文件打开对话框
   */
  openFile(params: OpenFileParams): OpenFileResult;

  /**
   * Show the system dialog for saving a file.
   * @zh 弹出系统原生文件保存对话框
   */
  saveFile(params: SaveFileParams): SaveFileResult;

  /**
   * Show the system dialog for choosing a folder.
   * @zh 弹出系统原生文件夹选择对话框
   */
  openFolder(params: OpenFolderParams): OpenFolderResult;

  /**
   * Show a modal task dialog with custom buttons. Escape and the close button do not dismiss it.
   * @zh 弹出模态系统任务对话框（Windows TaskDialog；支持自定义按钮；Esc 键与窗口关闭按钮均不可直接关闭）
   */
  confirm(params: ConfirmParams): ConfirmResult;
}

/** One entry in the dialog's file type list. */
interface FileFilter {
  /**
   * Label shown in the file type list. Defaults to "Files", localized to the UI language.
   * @zh 文件类型列表中显示的类型说明名称（默认“文件”，自动随宿主界面语言本地化）
   */
  name?: string;
  /**
   * Extensions without the leading dot, such as `flac`; `*` matches every file.
   * @zh 匹配的文件扩展名列表（不包含前导句点，例如 flac；传入 * 匹配全部文件）
   */
  extensions?: string[];
}

/**
 * File type list. When omitted or empty, the dialog shows a single "All Files" entry.
 * @zh 文件类型过滤器列表（缺省或为空时默认仅显示“所有文件”选项）
 */
type FileFilters = FileFilter[];

/**
 * Folder the dialog opens in, every time it is shown. `%music%` expands to the Music folder.
 * Ignored when the path does not resolve to a folder.
 * @zh 对话框打开时的初始定位目录（支持展开 %music% 变量；若路径无法解析为有效目录则自动忽略）
 * @default ""
 */
type InitialFolder = string;

// ---- openFile ----

interface OpenFileParams {
  /**
   * Window title. Defaults to "Open File", localized to the UI language.
   * @zh 对话框窗口标题（默认“打开文件”，自动随宿主界面语言本地化）
   */
  title?: string;
  /**
   * Allow selecting several files.
   * @zh 是否允许选择多个文件
   * @default false
   */
  multiple?: boolean;
  defaultPath?: InitialFolder;
  filters?: FileFilters;
}

interface OpenFileResult {
  /**
   * `true` when the user closed the dialog without choosing.
   * @zh 用户是否取消操作或直接关闭对话框
   */
  canceled: boolean;
  /**
   * Chosen files; empty when canceled.
   * @zh 用户最终选中的文件绝对路径列表（取消或未选择时为空数组）
   */
  filePaths: string[];
}

// ---- saveFile ----

interface SaveFileParams {
  /**
   * Window title. Defaults to "Save File", localized to the UI language.
   * @zh 对话框窗口标题（默认“保存文件”，自动随宿主界面语言本地化）
   */
  title?: string;
  /**
   * File name filled in when the dialog opens.
   * @zh 对话框打开时预填的默认文件名
   * @default ""
   */
  defaultName?: string;
  filters?: FileFilters;
}

interface SaveFileResult {
  /**
   * `true` when the user closed the dialog without choosing.
   * @zh 用户是否取消操作或直接关闭对话框
   */
  canceled: boolean;
  /**
   * Chosen file; empty when canceled. The system asks before an existing file is overwritten.
   * @zh 用户确认保存的目标文件绝对路径（取消时为空字符串；覆盖已有文件时系统对话框将弹出覆盖确认）
   */
  filePath: string;
}

// ---- openFolder ----

interface OpenFolderParams {
  /**
   * Window title. Defaults to "Select Folder", localized to the UI language.
   * @zh 对话框窗口标题（默认“选择文件夹”，自动随宿主界面语言本地化）
   */
  title?: string;
  defaultPath?: InitialFolder;
}

interface OpenFolderResult {
  /**
   * `true` when the user closed the dialog without choosing.
   * @zh 用户是否取消操作或直接关闭对话框
   */
  canceled: boolean;
  /**
   * Chosen folder, which need not be `defaultPath`; empty when canceled.
   * @zh 用户确认选择的文件夹绝对路径（不一定等于 defaultPath；取消时为空字符串）
   */
  folderPath: string;
}

// ---- confirm ----

interface ConfirmParams {
  /**
   * Window title. Defaults to "Confirm", localized to the UI language.
   * @zh 确认对话框窗口标题（默认“确认”，自动随宿主界面语言本地化）
   */
  title?: string;
  /**
   * Body text.
   * @zh 对话框提示正文内容
   * @default ""
   */
  message?: string;
  /**
   * Icon. The task dialog shows `question` with the information icon.
   * @zh 对话框图标类型（TaskDialog 原生无问号图标，question 将显示为信息图标）
   * @default "question"
   */
  type?: 'info' | 'warning' | 'error' | 'question';
  /**
   * Button labels, in order. Defaults to OK and Cancel, localized to the UI language.
   * @zh 自定义按钮标签文本列表（按顺序排列；默认“确定”“取消”，自动随宿主界面语言本地化）
   */
  buttons?: string[];
  /**
   * Index of the initially focused button.
   * @zh 对话框打开时默认获得焦点的按钮零基索引
   * @default 0
   */
  defaultButton?: Int;
}

interface ConfirmResult {
  /**
   * Zero-based index of the clicked button in `buttons`. `-1` only when not even the fallback
   * message box could be shown.
   * @zh 用户点击的按钮在 buttons 中的零基索引（仅在连备用消息框都无法弹出显示时返回 -1）
   */
  response: Int;
}
