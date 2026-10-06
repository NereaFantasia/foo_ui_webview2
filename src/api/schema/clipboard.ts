import type { Int } from './common.js';

export interface Api {
  /**
   * Report what the Windows clipboard holds: text, a file list, an image.
   * @zh 报告 Windows 剪贴板当前持有的内容：文本、文件列表、图片。
   */
  read(): ReadResult;

  /**
   * Replace the clipboard contents with Unicode text.
   * @zh 用 Unicode 文本替换剪贴板内容。
   */
  write(params: WriteParams): void;

  /**
   * Replace the clipboard contents with rich text in `HTML Format`, plus a plain-text fallback.
   * @zh 用 `HTML Format` 富文本替换剪贴板内容，并同时放入纯文本回退。
   */
  writeHTML(params: WriteHTMLParams): WriteHTMLResult;

  /**
   * Place a file list on the clipboard (`CF_HDROP`) for Explorer and other shell targets to paste.
   * @zh 把文件列表放到剪贴板（`CF_HDROP`），供资源管理器等粘贴。
   */
  writeFiles(params: WriteFilesParams): WriteFilesResult;
}

interface ReadResult {
  /**
   * The clipboard holds text (`CF_UNICODETEXT` or `CF_TEXT`).
   * @zh 剪贴板有文本（`CF_UNICODETEXT` 或 `CF_TEXT`）。
   */
  hasText: boolean;
  /**
   * The clipboard holds a bitmap (`CF_DIB` or `CF_BITMAP`). The image itself is not returned.
   * @zh 剪贴板有位图（`CF_DIB` 或 `CF_BITMAP`）。图片本身不返回。
   */
  hasImage: boolean;
  /**
   * The clipboard holds a file list (`CF_HDROP`) with at least one file.
   * @zh 剪贴板有文件列表（`CF_HDROP`）且至少有一个文件。
   */
  hasFiles: boolean;
  /**
   * The text; empty when the clipboard holds none.
   * @zh 文本内容；剪贴板没有文本时为空。
   */
  text: string;
  /**
   * The file paths, present only when `hasFiles` is `true`.
   * @zh 文件路径列表，只在 `hasFiles` 为 `true` 时出现。
   */
  files?: string[];
}

interface WriteParams {
  /**
   * Text to place on the clipboard.
   * @zh 要放到剪贴板的文本。
   * @minLength 1
   */
  text: string;
}

interface WriteHTMLParams {
  /**
   * HTML fragment to write as `HTML Format`.
   * @zh 以 `HTML Format` 写入的 HTML 片段。
   * @minLength 1
   */
  html: string;
  /**
   * Plain-text fallback written as `CF_UNICODETEXT`; when omitted or empty, the HTML text itself.
   * @zh 以 `CF_UNICODETEXT` 写入的纯文本回退；缺省或为空时用 HTML 文本本身。
   */
  plainText?: string;
}

interface WriteHTMLResult {
  /**
   * The `HTML Format` data was placed on the clipboard.
   * @zh `HTML Format` 数据已放到剪贴板。
   */
  htmlWritten: boolean;
  /**
   * The plain-text fallback was placed on the clipboard.
   * @zh 纯文本回退已放到剪贴板。
   */
  textWritten: boolean;
}

interface WriteFilesParams {
  /**
   * Absolute file paths, in the order they are placed on the clipboard.
   * @zh 绝对文件路径，按放入剪贴板的顺序。
   * @minItems 1
   * @security Read
   */
  paths: string[];
}

interface WriteFilesResult {
  /**
   * Number of paths placed on the clipboard.
   * @zh 放到剪贴板的路径数。
   */
  fileCount: Int;
}
