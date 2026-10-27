import type { Json } from './common.js';

export interface Api {
  /**
   * Write a line to the foobar2000 console, prefixed with `[WebView]`.
   * @zh 向 foobar2000 系统控制台输出普通日志信息（前缀为 [WebView]）
   */
  log(params: MessageParams): void;

  /**
   * Write a warning line to the foobar2000 console, prefixed with `[WebView][WARN]`.
   * @zh 向 foobar2000 系统控制台输出警告日志信息（前缀为 [WebView][WARN]）
   */
  warn(params: MessageParams): void;

  /**
   * Write an error line to the foobar2000 console, prefixed with `[WebView][ERROR]`.
   * @zh 向 foobar2000 系统控制台输出错误日志信息（前缀为 [WebView][ERROR]）
   */
  error(params: MessageParams): void;
}

// Give `message`, or `args` for several values. A call that yields no text fails with
// INVALID_PARAMS ("message is required").
interface MessageParams {
  /**
   * What to write. A string is written as it is; any other JSON value as its JSON text. Takes
   * precedence over `args`.
   * @zh 待输出的控制台正文内容（字符串原样输出，其余类型序列化为 JSON 文本；与 args 同时提供时优先采用）
   */
  message?: Json;
  /**
   * Values to write, separated by spaces: strings as they are, other values as their JSON text.
   * Used only when `message` is absent.
   * @zh 待输出的参数值列表（以空格连接输出；字符串原样输出，其余类型序列化为 JSON 文本；仅在缺省 message 时使用）
   */
  args?: Json[];
}
