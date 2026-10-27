import type { Int } from './common.js';

export interface Api {
  /**
   * Report what the Windows clipboard holds: text, a file list, an image.
   * @zh 读取 Windows 系统剪贴板当前承载的数据状态（检测文本、文件列表及位图图片）
   */
  read(): ReadResult;

  /**
   * Replace the clipboard contents with Unicode text.
   * @zh 以 Unicode 纯文本替换 Windows 剪贴板内容
   */
  write(params: WriteParams): void;

  /**
   * Replace the clipboard contents with rich text in `HTML Format`, plus a plain-text fallback.
   * @zh 以 `HTML Format` 富文本替换剪贴板内容，并同步写入纯文本回退内容
   */
  writeHTML(params: WriteHTMLParams): WriteHTMLResult;

  /**
   * Place a file list on the clipboard (`CF_HDROP`) for Explorer and other shell targets to paste.
   * @zh 将文件绝对路径列表写入剪贴板（以 `CF_HDROP` 格式供文件资源管理器等 Shell 目标粘贴）
   */
  writeFiles(params: WriteFilesParams): WriteFilesResult;
}

interface ReadResult {
  /**
   * The clipboard holds text (`CF_UNICODETEXT` or `CF_TEXT`).
   * @zh 剪贴板当前是否包含文本数据（`CF_UNICODETEXT` 或 `CF_TEXT`）
   */
  hasText: boolean;
  /**
   * The clipboard holds a bitmap (`CF_DIB` or `CF_BITMAP`). The image itself is not returned.
   * @zh 剪贴板当前是否包含位图数据（`CF_DIB` 或 `CF_BITMAP`；仅报告存在状态，不返回图像二进制数据）
   */
  hasImage: boolean;
  /**
   * The clipboard holds a file list (`CF_HDROP`) with at least one file.
   * @zh 剪贴板当前是否包含至少一个文件的文件列表数据（`CF_HDROP`）
   */
  hasFiles: boolean;
  /**
   * The text; empty when the clipboard holds none.
   * @zh 提取的纯文本内容（剪贴板无文本时为空字符串）
   */
  text: string;
  /**
   * The file paths, present only when `hasFiles` is `true`.
   * @zh 提取的文件绝对路径列表（仅在 `hasFiles` 为 `true` 时提供）
   */
  files?: string[];
}

interface WriteParams {
  /**
   * Text to place on the clipboard.
   * @zh 待写入剪贴板的纯文本内容
   * @minLength 1
   */
  text: string;
}

interface WriteHTMLParams {
  /**
   * HTML fragment to write as `HTML Format`.
   * @zh 待以 `HTML Format` 写入剪贴板的 HTML 片段
   * @minLength 1
   */
  html: string;
  /**
   * Plain-text fallback written as `CF_UNICODETEXT`; when omitted or empty, the HTML text itself.
   * @zh 待以 `CF_UNICODETEXT` 格式写入的纯文本回退内容（缺省或为空时自动使用 HTML 文本内容）
   */
  plainText?: string;
}

interface WriteHTMLResult {
  /**
   * The `HTML Format` data was placed on the clipboard.
   * @zh `HTML Format` 富文本数据是否成功写入剪贴板
   */
  htmlWritten: boolean;
  /**
   * The plain-text fallback was placed on the clipboard.
   * @zh 纯文本回退内容是否成功写入剪贴板
   */
  textWritten: boolean;
}

interface WriteFilesParams {
  /**
   * Absolute file paths, in the order they are placed on the clipboard.
   * @zh 待放入剪贴板的文件绝对路径列表（保持传入顺序）
   * @minItems 1
   * @security MediaRead
   */
  paths: string[];
}

interface WriteFilesResult {
  /**
   * Number of paths placed on the clipboard.
   * @zh 实际成功写入剪贴板的文件路径数量
   */
  fileCount: Int;
}
