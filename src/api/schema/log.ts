import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Write a line to a log file in the foobar2000 profile directory: `webview_ui.log`, or the
   * file named by `file`.
   * @zh 向 foobar2000 配置目录里的日志文件写一行：默认是 `webview_ui.log`，也可以用 `file` 指定。
   */
  write(params: WriteParams): WriteResult;

  /**
   * Read the last lines of `webview_ui.log`. A file that does not exist yet reads as empty.
   * @zh 读取 `webview_ui.log` 末尾的若干行。文件还不存在时按空文件返回。
   */
  read(params: ReadParams): ReadResult;

  /**
   * Empty `webview_ui.log`.
   * @zh 清空 `webview_ui.log`。
   */
  clear(): void;
}

// ---- write ----

// A call that yields no text fails with INVALID_PARAMS ("message is required").
interface WriteParams {
  /**
   * What to write. A string is written as it is; any other JSON value as its JSON text. Takes
   * precedence over `args`.
   * @zh 要写的内容。字符串原样写出，其他 JSON 值写成 JSON 文本。与 `args` 同时给出时以它为准。
   */
  message?: Json;
  /**
   * Values to write, separated by spaces: strings as they are, other values as their JSON text.
   * Used only when `message` is absent.
   * @zh 要写的若干值，以空格分隔：字符串原样写出，其他值写成 JSON 文本。仅在没有 `message` 时使用。
   */
  args?: Json[];
  /**
   * Level written in brackets before the message, upper-cased.
   * @zh 写在消息前方括号里的级别，会转成大写。
   * @default "info"
   */
  level?: string;
  /**
   * Append to the file; `false` replaces its content with this line.
   * @zh 追加到文件末尾；为 `false` 时用这一行替换文件内容。
   * @default true
   */
  append?: boolean;
  /**
   * Prefix the line with the local time, as `[YYYY-MM-DD HH:MM:SS.mmm]`.
   * @zh 在行首加上本地时间，形如 `[YYYY-MM-DD HH:MM:SS.mmm]`。
   * @default true
   */
  timestamp?: boolean;
  /**
   * Name of another file in the profile directory. It must be a plain file name ending in `.log`
   * or `.txt` and not a Windows device name such as `CON`; any other value is ignored and the
   * line goes to `webview_ui.log`.
   * @zh 配置目录里另一个文件的名字。必须是单纯的文件名、以 `.log` 或 `.txt` 结尾，且不是 `CON` 这类 Windows 设备名；否则忽略，这一行写进 `webview_ui.log`。
   */
  file?: string;
}

interface WriteResult {
  /**
   * Full path of the file that was written.
   * @zh 实际写入的文件的完整路径。
   */
  path: string;
}

// ---- read ----

interface ReadParams {
  /**
   * How many lines to return from the end of the file.
   * @zh 从文件末尾返回多少行。
   * @minimum 0
   * @default 100
   */
  lines?: Int;
}

interface ReadResult {
  /**
   * The returned lines joined with `\n`.
   * @zh 返回的各行以 `\n` 连接。
   */
  content: string;
  /**
   * The returned lines. Absent when the file does not exist.
   * @zh 返回的各行。文件不存在时不出现。
   */
  lines?: string[];
  /**
   * Number of lines returned.
   * @zh 返回的行数。
   */
  lineCount: Int;
  /**
   * Number of lines in the whole file. Absent when the file does not exist.
   * @zh 整个文件的行数。文件不存在时不出现。
   */
  totalLines?: Int;
}
