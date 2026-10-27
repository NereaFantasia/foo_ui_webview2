import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Write a line to a log file in the foobar2000 profile directory: `webview_ui.log`, or the
   * file named by `file`.
   * @zh 向 foobar2000 配置目录内的日志文件写入一行日志（默认写入 webview_ui.log，支持通过 file 指定自定义文件名）
   */
  write(params: WriteParams): WriteResult;

  /**
   * Read the last lines of `webview_ui.log`. A file that does not exist yet reads as empty.
   * @zh 读取 webview_ui.log 文件末尾的指定行数（文件不存在时返回空内容）
   */
  read(params: ReadParams): ReadResult;

  /**
   * Empty `webview_ui.log`.
   * @zh 清空 webview_ui.log 日志文件内容
   */
  clear(): void;
}

// ---- write ----

// A call that yields no text fails with INVALID_PARAMS ("message is required").
interface WriteParams {
  /**
   * What to write. A string is written as it is; any other JSON value as its JSON text. Takes
   * precedence over `args`.
   * @zh 待写入的日志正文内容（字符串原样输出，对象或数组序列化为 JSON 文本；若与 args 同时提供则以本字段为准）
   */
  message?: Json;
  /**
   * Values to write, separated by spaces: strings as they are, other values as their JSON text.
   * Used only when `message` is absent.
   * @zh 待写入的参数值列表（以空格连接输出；字符串原样输出，其他类型序列化为 JSON 文本；仅在缺省 message 时生效）
   */
  args?: Json[];
  /**
   * Level written in brackets before the message, upper-cased.
   * @zh 日志记录级别（自动转为大写并置于行首方括号内）
   * @default "info"
   */
  level?: string;
  /**
   * Append to the file; `false` replaces its content with this line.
   * @zh 是否以追加模式写入（默认 true；为 false 时清空并替换文件内容）
   * @default true
   */
  append?: boolean;
  /**
   * Prefix the line with the local time, as `[YYYY-MM-DD HH:MM:SS.mmm]`.
   * @zh 是否在行首添加本地时间戳（格式为 [YYYY-MM-DD HH:MM:SS.mmm]）
   * @default true
   */
  timestamp?: boolean;
  /**
   * Name of another file in the profile directory. It must be a plain file name ending in `.log`
   * or `.txt` and not a Windows device name such as `CON`; any other value is ignored and the
   * line goes to `webview_ui.log`.
   * @zh 配置目录内的自定义日志文件名（须为以 .log 或 .txt 结尾的合法纯文件名，且不可使用 CON 等 Windows 保留设备名；不合法时将自动写入 webview_ui.log）
   */
  file?: string;
}

interface WriteResult {
  /**
   * Full path of the file that was written.
   * @zh 实际写入的日志文件完整绝对路径
   */
  path: string;
}

// ---- read ----

interface ReadParams {
  /**
   * How many lines to return from the end of the file.
   * @zh 从日志文件末尾逆向读取的最大行数
   * @minimum 0
   * @default 100
   */
  lines?: Int;
}

interface ReadResult {
  /**
   * The returned lines joined with `\n`.
   * @zh 读取的各行日志内容（以换行符 \n 拼接）
   */
  content: string;
  /**
   * The returned lines. Absent when the file does not exist.
   * @zh 读取的各行日志字符串数组（文件不存在时缺省）
   */
  lines?: string[];
  /**
   * Number of lines returned.
   * @zh 实际读取返回的日志行数
   */
  lineCount: Int;
  /**
   * Number of lines in the whole file. Absent when the file does not exist.
   * @zh 日志文件包含的总行数（文件不存在时缺省）
   */
  totalLines?: Int;
}
