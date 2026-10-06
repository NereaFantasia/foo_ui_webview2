import type { Json } from './common.js';

export interface Api {
  /**
   * Write a line to the foobar2000 console, prefixed with `[WebView]`.
   * @zh 向 foobar2000 控制台写一行，前缀为 `[WebView]`。
   */
  log(params: MessageParams): void;

  /**
   * Write a warning line to the foobar2000 console, prefixed with `[WebView][WARN]`.
   * @zh 向 foobar2000 控制台写一行警告，前缀为 `[WebView][WARN]`。
   */
  warn(params: MessageParams): void;

  /**
   * Write an error line to the foobar2000 console, prefixed with `[WebView][ERROR]`.
   * @zh 向 foobar2000 控制台写一行错误，前缀为 `[WebView][ERROR]`。
   */
  error(params: MessageParams): void;
}

// Give `message`, or `args` for several values. A call that yields no text fails with
// INVALID_PARAMS ("message is required").
interface MessageParams {
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
}
