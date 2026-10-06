import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Send a GET request. By default the call returns a receipt at once and the response arrives
   * as `http:response` carrying the same `requestId`; with `async: false` the host blocks until
   * the response is in and returns it. Requests to a local or private network address fail with
   * `PERMISSION_DENIED` unless the host setting allows them, checked again at every redirect.
   * @zh 发送 GET 请求。缺省时立即返回回执，响应随后以带同一 `requestId` 的 `http:response` 事件到达；`async: false` 时宿主阻塞到收到响应再返回。请求本地或私有网络地址会以 `PERMISSION_DENIED` 失败，除非宿主设置允许，每次重定向都会重新检查。
   */
  get(params: RequestParams): RequestResult;

  /**
   * Send a POST request; otherwise the same as `http.get`.
   * @zh 发送 POST 请求；其余同 `http.get`。
   */
  post(params: BodyRequestParams): RequestResult;

  /**
   * Send a PUT request; otherwise the same as `http.get`.
   * @zh 发送 PUT 请求；其余同 `http.get`。
   */
  put(params: BodyRequestParams): RequestResult;

  /**
   * Send a DELETE request; otherwise the same as `http.get`.
   * @zh 发送 DELETE 请求；其余同 `http.get`。
   */
  delete(params: BodyRequestParams): RequestResult;

  /**
   * Send a PATCH request; otherwise the same as `http.get`.
   * @zh 发送 PATCH 请求；其余同 `http.get`。
   */
  patch(params: BodyRequestParams): RequestResult;

  /**
   * Send a HEAD request; otherwise the same as `http.get`. The response body is always empty
   * text, and `contentLength` is added when the server sends a numeric `Content-Length`.
   * @zh 发送 HEAD 请求；其余同 `http.get`。响应体恒为空文本，服务器发来数字形式的 `Content-Length` 时另带 `contentLength`。
   */
  head(params: HeadParams): HeadResult;

  /**
   * Download a URL into a file, creating the missing folders. By default the call blocks until
   * the file is written; with `async: true` it returns a receipt at once and the outcome arrives
   * as `http:downloadComplete`. Downloads over 500 MB fail with `OPERATION_FAILED`.
   * @zh 把 URL 下载到文件，缺失的文件夹会被创建。缺省时阻塞到文件写完；`async: true` 时立即返回回执，结果随后以 `http:downloadComplete` 事件到达。超过 500 MB 的下载以 `OPERATION_FAILED` 失败。
   */
  download(params: DownloadParams): DownloadResult;

  /**
   * Cancel an async request or download. The request then ends with its event reporting
   * `CANCELLED`.
   * @zh 取消异步请求或下载。该请求随后以报告 `CANCELLED` 的事件结束。
   */
  abort(params: AbortParams): AbortResult;
}

export interface Events {
  /**
   * The outcome of an async `http.get`, `http.post`, `http.put`, `http.delete`, `http.patch` or
   * `http.head`, carrying the receipt's `requestId`. On success it has the fields of the
   * synchronous response; on failure `error` and `code`, like a failed call. Sent to the page
   * that made the request, or to a page under the same top-level window when that one cannot be
   * found, or else to the main window's page.
   * @zh 异步的 `http.get`、`http.post`、`http.put`、`http.delete`、`http.patch` 或 `http.head` 的结果，带回执里的 `requestId`。成功时带同步响应的各字段；失败时带 `error` 与 `code`，同失败的调用。发给发起请求的页面；找不到它时发给同一顶层窗口下的页面，再没有就发给主窗口的页面。
   * @delivery caller
   */
  response: ResponsePayload;

  /**
   * The outcome of `http.download` with `async: true`, carrying the receipt's `requestId`. On
   * success it has the fields of a synchronous download; on failure `error` and `code`.
   * Delivered like `http:response`.
   * @zh 带 `async: true` 的 `http.download` 的结果，带回执里的 `requestId`。成功时带同步下载的各字段；失败时带 `error` 与 `code`。投递范围同 `http:response`。
   * @delivery caller
   */
  downloadComplete: DownloadCompletePayload;
}

interface ResponsePayload {
  /**
   * Id from the receipt.
   * @zh 回执里的 id。
   */
  requestId: string;
  /**
   * Whether a response came back. A response with an error status such as `404` is a success.
   * @zh 是否拿到了响应。`404` 之类的错误状态码也算成功。
   */
  success: boolean;
  /**
   * HTTP status code: on success that of the response; on failure only when a redirect was
   * refused under `redirect: "error"`, that of the redirect.
   * @zh HTTP 状态码：成功时是响应的状态码；失败时只在 `redirect: "error"` 拒绝了重定向时出现，是该重定向的状态码。
   */
  status?: Int;
  /**
   * Response headers by name; of repeated headers the last one wins. Present on success.
   * @zh 响应头，以名称为键；重复的响应头取最后一个。成功时出现。
   */
  headers?: Record<string, string>;
  /**
   * Response body, as text or Base64 according to `responseType`; always empty for `http.head`.
   * Present on success.
   * @zh 响应体，按 `responseType` 为文本或 Base64；`http.head` 恒为空。成功时出现。
   */
  body?: string;
  /**
   * `text` or `base64`, the encoding of `body`. Present on success.
   * @zh `text` 或 `base64`，即 `body` 的编码。成功时出现。
   */
  responseType?: 'text' | 'base64';
  /**
   * The `Content-Length` header as a number; present on success of `http.head` when the server
   * sends one that parses.
   * @zh 数字形式的 `Content-Length` 响应头；`http.head` 成功且服务器发来可解析的值时出现。
   */
  contentLength?: Int;
  /**
   * Why the request failed; present on failure.
   * @zh 请求失败的原因；失败时出现。
   */
  error?: string;
  /**
   * The error code: `INVALID_PARAMS` for a URL that is not `http` or `https`,
   * `PERMISSION_DENIED` for a local or private network address the host setting does not allow,
   * `CANCELLED` after `http.abort`, `OPERATION_FAILED` otherwise. Present on failure.
   * @zh 错误码：URL 不是 `http` 或 `https` 时为 `INVALID_PARAMS`，宿主设置不允许的本地或私有网络地址为 `PERMISSION_DENIED`，`http.abort` 之后为 `CANCELLED`，其余为 `OPERATION_FAILED`。失败时出现。
   */
  code?: 'INVALID_PARAMS' | 'PERMISSION_DENIED' | 'CANCELLED' | 'OPERATION_FAILED';
  /**
   * `true` when the request was cancelled by `http.abort`; absent otherwise.
   * @zh 请求被 `http.abort` 取消时为 `true`；否则不出现。
   */
  cancelled?: boolean;
}

interface DownloadCompletePayload {
  /**
   * Id from the receipt.
   * @zh 回执里的 id。
   */
  requestId: string;
  /**
   * Whether the file was written. A response with an error status such as `404` is saved and
   * counts as a success.
   * @zh 文件是否写成。`404` 之类的错误状态码的响应也会保存，算成功。
   */
  success: boolean;
  /**
   * HTTP status code: on success that of the response; on failure only when a redirect was
   * refused under `redirect: "error"`, that of the redirect.
   * @zh HTTP 状态码：成功时是响应的状态码；失败时只在 `redirect: "error"` 拒绝了重定向时出现，是该重定向的状态码。
   */
  status?: Int;
  /**
   * Bytes written; present on success.
   * @zh 写入的字节数；成功时出现。
   */
  bytesWritten?: Int;
  /**
   * The file written, with path variables expanded; present on success.
   * @zh 写入的文件（已展开路径变量）；成功时出现。
   */
  path?: string;
  /**
   * Why the download failed; present on failure.
   * @zh 下载失败的原因；失败时出现。
   */
  error?: string;
  /**
   * The error code, as in `http:response`. Present on failure.
   * @zh 错误码，同 `http:response`。失败时出现。
   */
  code?: 'INVALID_PARAMS' | 'PERMISSION_DENIED' | 'CANCELLED' | 'OPERATION_FAILED';
  /**
   * `true` when the download was cancelled by `http.abort`; absent otherwise.
   * @zh 下载被 `http.abort` 取消时为 `true`；否则不出现。
   */
  cancelled?: boolean;
}

interface RequestParams {
  /**
   * Request URL; only `http` and `https` are allowed.
   * @zh 请求 URL；只允许 `http` 与 `https`。
   * @minLength 1
   */
  url: string;
  /**
   * Custom request headers. `User-Agent` overrides the host default on the initial request.
   * A header whose name or value contains a line break is dropped. Custom headers are omitted
   * after a redirect, including `User-Agent`, so redirected requests use the host default.
   * @zh 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。
   */
  headers?: Record<string, string>;
  /**
   * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less
   * keeps the WinHTTP defaults.
   * @zh 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。
   * @default 30000
   */
  timeout?: Int;
  /**
   * Return a receipt at once and deliver the response as `http:response`; `false` blocks until
   * the response is in.
   * @zh 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。
   * @default true
   */
  async?: boolean;
  /**
   * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect;
   * `manual` returns the redirect response as it is.
   * @zh `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。
   * @default "follow"
   */
  redirect?: 'follow' | 'error' | 'manual';
  /**
   * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`,
   * return it Base64-encoded.
   * @zh `text` 以文本返回响应体；`base64` 及其别名 `arraybuffer`、`binary` 以 Base64 编码返回。
   * @default "text"
   */
  responseType?: 'text' | 'base64' | 'arraybuffer' | 'binary';
  /**
   * Accept invalid or self-signed certificates; takes effect only when the host setting allows
   * insecure TLS as well.
   * @zh 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。
   * @default false
   */
  insecureTls?: boolean;
}

interface BodyRequestParams {
  /**
   * Request URL; only `http` and `https` are allowed.
   * @zh 请求 URL；只允许 `http` 与 `https`。
   * @minLength 1
   */
  url: string;
  /**
   * Request body: a string is sent as it is, an object or array as its JSON text, and any other
   * value as an empty body.
   * @zh 请求体：字符串原样发送，对象或数组发送其 JSON 文本，其他值按空请求体发送。
   */
  body?: Json;
  /**
   * Custom request headers. `User-Agent` overrides the host default on the initial request.
   * A header whose name or value contains a line break is dropped. Custom headers are omitted
   * after a redirect, including `User-Agent`, so redirected requests use the host default.
   * @zh 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。
   */
  headers?: Record<string, string>;
  /**
   * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less
   * keeps the WinHTTP defaults.
   * @zh 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。
   * @default 30000
   */
  timeout?: Int;
  /**
   * Return a receipt at once and deliver the response as `http:response`; `false` blocks until
   * the response is in.
   * @zh 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。
   * @default true
   */
  async?: boolean;
  /**
   * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect;
   * `manual` returns the redirect response as it is.
   * @zh `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。
   * @default "follow"
   */
  redirect?: 'follow' | 'error' | 'manual';
  /**
   * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`,
   * return it Base64-encoded.
   * @zh `text` 以文本返回响应体；`base64` 及其别名 `arraybuffer`、`binary` 以 Base64 编码返回。
   * @default "text"
   */
  responseType?: 'text' | 'base64' | 'arraybuffer' | 'binary';
  /**
   * Accept invalid or self-signed certificates; takes effect only when the host setting allows
   * insecure TLS as well.
   * @zh 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。
   * @default false
   */
  insecureTls?: boolean;
}

interface HeadParams {
  /**
   * Request URL; only `http` and `https` are allowed.
   * @zh 请求 URL；只允许 `http` 与 `https`。
   * @minLength 1
   */
  url: string;
  /**
   * Custom request headers. `User-Agent` overrides the host default on the initial request.
   * A header whose name or value contains a line break is dropped. Custom headers are omitted
   * after a redirect, including `User-Agent`, so redirected requests use the host default.
   * @zh 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。
   */
  headers?: Record<string, string>;
  /**
   * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less
   * keeps the WinHTTP defaults.
   * @zh 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。
   * @default 30000
   */
  timeout?: Int;
  /**
   * Return a receipt at once and deliver the response as `http:response`; `false` blocks until
   * the response is in.
   * @zh 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。
   * @default true
   */
  async?: boolean;
  /**
   * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect;
   * `manual` returns the redirect response as it is.
   * @zh `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。
   * @default "follow"
   */
  redirect?: 'follow' | 'error' | 'manual';
  /**
   * Accept invalid or self-signed certificates; takes effect only when the host setting allows
   * insecure TLS as well.
   * @zh 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。
   * @default false
   */
  insecureTls?: boolean;
}

interface RequestResult {
  /**
   * Id of the request; present in an async receipt. `http:response` and `http.abort` use it.
   * @zh 请求的 id；异步回执里出现。`http:response` 与 `http.abort` 用它。
   */
  requestId?: string;
  /**
   * `true` in an async receipt.
   * @zh 异步回执里为 `true`。
   */
  async?: boolean;
  /**
   * HTTP status code; present in a synchronous response.
   * @zh HTTP 状态码；同步响应里出现。
   */
  status?: Int;
  /**
   * Response headers by name; of repeated headers the last one wins. Present in a synchronous
   * response.
   * @zh 响应头，以名称为键；重复的响应头取最后一个。同步响应里出现。
   */
  headers?: Record<string, string>;
  /**
   * Response body, as text or Base64 according to `responseType`; present in a synchronous
   * response.
   * @zh 响应体，按 `responseType` 为文本或 Base64；同步响应里出现。
   */
  body?: string;
  /**
   * `text` or `base64`, the encoding of `body`; present in a synchronous response.
   * @zh `text` 或 `base64`，即 `body` 的编码；同步响应里出现。
   */
  responseType?: 'text' | 'base64';
}

interface HeadResult {
  /**
   * Id of the request; present in an async receipt. `http:response` and `http.abort` use it.
   * @zh 请求的 id；异步回执里出现。`http:response` 与 `http.abort` 用它。
   */
  requestId?: string;
  /**
   * `true` in an async receipt.
   * @zh 异步回执里为 `true`。
   */
  async?: boolean;
  /**
   * HTTP status code; present in a synchronous response.
   * @zh HTTP 状态码；同步响应里出现。
   */
  status?: Int;
  /**
   * Response headers by name; of repeated headers the last one wins. Present in a synchronous
   * response.
   * @zh 响应头，以名称为键；重复的响应头取最后一个。同步响应里出现。
   */
  headers?: Record<string, string>;
  /**
   * Always empty; present in a synchronous response.
   * @zh 恒为空；同步响应里出现。
   */
  body?: string;
  /**
   * Always `text`; present in a synchronous response.
   * @zh 恒为 `text`；同步响应里出现。
   */
  responseType?: 'text' | 'base64';
  /**
   * The `Content-Length` header as a number; present in a synchronous response when the server
   * sends one that parses.
   * @zh 数字形式的 `Content-Length` 响应头；同步响应里、服务器发来可解析的值时出现。
   */
  contentLength?: Int;
}

// ---- download ----

interface DownloadParams {
  /**
   * URL to download; only `http` and `https` are allowed.
   * @zh 要下载的 URL；只允许 `http` 与 `https`。
   * @minLength 1
   */
  url: string;
  /**
   * File to write; path variables are expanded and missing folders are created. An existing
   * file is replaced.
   * @zh 要写入的文件；会展开路径变量并创建缺失的文件夹。已有的文件会被覆盖。
   * @minLength 1
   * @security Write
   */
  saveTo: string;
  /**
   * Custom request headers. `User-Agent` overrides the host default on the initial request.
   * A header whose name or value contains a line break is dropped. Custom headers are omitted
   * after a redirect, including `User-Agent`, so redirected requests use the host default.
   * @zh 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。
   */
  headers?: Record<string, string>;
  /**
   * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less
   * keeps the WinHTTP defaults.
   * @zh 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。
   * @default 60000
   */
  timeout?: Int;
  /**
   * Return a receipt at once and deliver the outcome as `http:downloadComplete`; `false` blocks
   * until the file is written.
   * @zh 立即返回回执、以 `http:downloadComplete` 事件送达结果；为 `false` 时阻塞到文件写完。
   * @default false
   */
  async?: boolean;
  /**
   * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect;
   * `manual` saves the redirect response as it is.
   * @zh `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样保存重定向响应。
   * @default "follow"
   */
  redirect?: 'follow' | 'error' | 'manual';
  /**
   * Accept invalid or self-signed certificates; takes effect only when the host setting allows
   * insecure TLS as well.
   * @zh 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。
   * @default false
   */
  insecureTls?: boolean;
}

interface DownloadResult {
  /**
   * Id of the download; present in an async receipt. `http:downloadComplete` and `http.abort`
   * use it.
   * @zh 下载的 id；异步回执里出现。`http:downloadComplete` 与 `http.abort` 用它。
   */
  requestId?: string;
  /**
   * `true` in an async receipt.
   * @zh 异步回执里为 `true`。
   */
  async?: boolean;
  /**
   * `Download started` in an async receipt.
   * @zh 异步回执里为 `Download started`。
   */
  message?: string;
  /**
   * HTTP status code; present when the download finished synchronously.
   * @zh HTTP 状态码；同步下载完成时出现。
   */
  status?: Int;
  /**
   * Bytes written; present when the download finished synchronously.
   * @zh 写入的字节数；同步下载完成时出现。
   */
  bytesWritten?: Int;
  /**
   * The file written, with path variables expanded; present when the download finished
   * synchronously.
   * @zh 写入的文件（已展开路径变量）；同步下载完成时出现。
   */
  path?: string;
}

// ---- abort ----

interface AbortParams {
  /**
   * Id from the receipt of the request or download.
   * @zh 请求或下载回执里的 id。
   * @minLength 1
   */
  requestId: string;
}

interface AbortResult {
  /**
   * The id, as given.
   * @zh 原样的 id。
   */
  requestId: string;
  /**
   * `true` when the request was still running and has been told to stop; `false` when it had
   * already ended or never existed.
   * @zh 请求仍在进行并已被通知停止时为 `true`；已结束或从未存在时为 `false`。
   */
  cancelled: boolean;
}
