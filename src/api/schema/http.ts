import type { Int, Json } from './common.js';

export interface Api {
  /**
   * Send a GET request. By default the call returns a receipt at once and the response arrives
   * as `http:response` carrying the same `requestId`; with `async: false` the host blocks until
   * the response is in and returns it. Requests to a local or private network address fail with
   * `PERMISSION_DENIED` unless the host setting allows them, checked again at every redirect.
   * @zh 发送 HTTP GET 请求（默认立即返回回执并通过携带相同 requestId 的 http:response 事件异步回送响应；传 async: false 时宿主阻塞至收到响应后直接返回；除非宿主高级设置明确允许，否则请求本地或局域网私有地址将以 PERMISSION_DENIED 拒绝，每次重定向均会重新校验）
   */
  get(params: RequestParams): RequestResult;

  /**
   * Send a POST request; otherwise the same as `http.get`.
   * @zh 发送 HTTP POST 请求（其余参数行为与 http.get 一致）
   */
  post(params: BodyRequestParams): RequestResult;

  /**
   * Send a PUT request; otherwise the same as `http.get`.
   * @zh 发送 HTTP PUT 请求（其余参数行为与 http.get 一致）
   */
  put(params: BodyRequestParams): RequestResult;

  /**
   * Send a DELETE request; otherwise the same as `http.get`.
   * @zh 发送 HTTP DELETE 请求（其余参数行为与 http.get 一致）
   */
  delete(params: BodyRequestParams): RequestResult;

  /**
   * Send a PATCH request; otherwise the same as `http.get`.
   * @zh 发送 HTTP PATCH 请求（其余参数行为与 http.get 一致）
   */
  patch(params: BodyRequestParams): RequestResult;

  /**
   * Send a HEAD request; otherwise the same as `http.get`. The response body is always empty
   * text, and `contentLength` is added when the server sends a numeric `Content-Length`.
   * @zh 发送 HTTP HEAD 请求（其余参数行为与 http.get 一致；响应体恒为空字符串，若服务器返回数字形式的 Content-Length 则同步附带 contentLength）
   */
  head(params: HeadParams): HeadResult;

  /**
   * Download a URL into a file, creating the missing folders. By default the call blocks until
   * the file is written; with `async: true` it returns a receipt at once and the outcome arrives
   * as `http:downloadComplete`. Downloads over 500 MB fail with `OPERATION_FAILED`.
   * @zh 将指定 URL 资源下载并写入本地文件（自动展开路径变量并创建缺失的父级目录；默认阻塞至文件写入完成；传 async: true 时立即返回回执并随后派发 http:downloadComplete 事件；超过 500 MB 的下载将以 OPERATION_FAILED 失败）
   */
  download(params: DownloadParams): DownloadResult;

  /**
   * Cancel an async request or download. The request then ends with its event reporting
   * `CANCELLED`.
   * @zh 中止正在执行的异步 HTTP 请求或下载任务（中止后请求将以报告 CANCELLED 状态的事件结束）
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
   * @zh 异步 HTTP 请求结果事件（涵盖 get、post、put、delete、patch 或 head 调用，载荷携带回执中的 requestId；成功时包含各同步响应字段，失败时包含 error 与 code；定向派发给发起请求的页面，若页面不可达则派发给同一顶层窗口的其他页面或主窗口页面）
   * @delivery caller
   */
  response: ResponsePayload;

  /**
   * The outcome of `http.download` with `async: true`, carrying the receipt's `requestId`. On
   * success it has the fields of a synchronous download; on failure `error` and `code`.
   * Delivered like `http:response`.
   * @zh 异步 http.download 任务完成或失败事件（载荷携带回执中的 requestId；成功时包含各同步下载字段，失败时包含 error 与 code；投递范围规则同 http:response）
   * @delivery caller
   */
  downloadComplete: DownloadCompletePayload;
}

interface ResponsePayload {
  /**
   * Id from the receipt.
   * @zh 请求回执中分配的唯一标识符 requestId
   */
  requestId: string;
  /**
   * Whether a response came back. A response with an error status such as `404` is a success.
   * @zh 是否成功接收到 HTTP 响应（接收到 404 等 HTTP 错误状态码亦视为网络请求成功）
   */
  success: boolean;
  /**
   * HTTP status code: on success that of the response; on failure only when a redirect was
   * refused under `redirect: "error"`, that of the redirect.
   * @zh HTTP 状态码（请求成功时为服务端返回的状态码；仅当 redirect: "error" 拒绝重定向时作为失败原因提供重定向状态码）
   */
  status?: Int;
  /**
   * Response headers by name; of repeated headers the last one wins. Present on success.
   * @zh 响应头键值对（键名转为小写；存在重复响应头时以最后出现的数值为准；请求成功时提供）
   */
  headers?: Record<string, string>;
  /**
   * Response body, as text or Base64 according to `responseType`; always empty for `http.head`.
   * Present on success.
   * @zh 响应体内容（依据 responseType 格式化为纯文本或 Base64 编码字符串；http.head 恒为空字符串；请求成功时提供）
   */
  body?: string;
  /**
   * `text` or `base64`, the encoding of `body`. Present on success.
   * @zh 响应体内容的编码格式（text 或 base64；请求成功时提供）
   */
  responseType?: 'text' | 'base64';
  /**
   * The `Content-Length` header as a number; present on success of `http.head` when the server
   * sends one that parses.
   * @zh 数值形式的 Content-Length 响应头（仅在 http.head 请求成功且服务端返回合法数字时提供）
   */
  contentLength?: Int;
  /**
   * Why the request failed; present on failure.
   * @zh 请求失败的具体错误原因描述（仅在请求失败时提供）
   */
  error?: string;
  /**
   * The error code: `INVALID_PARAMS` for a URL that is not `http` or `https`,
   * `PERMISSION_DENIED` for a local or private network address the host setting does not allow,
   * `CANCELLED` after `http.abort`, `OPERATION_FAILED` otherwise. Present on failure.
   * @zh 错误码代码（URL 非 http/https 时为 INVALID_PARAMS；未授权访问本地或局域网私有地址为 PERMISSION_DENIED；被 http.abort 中止为 CANCELLED；其余网络错误为 OPERATION_FAILED；仅在失败时提供）
   */
  code?: 'INVALID_PARAMS' | 'PERMISSION_DENIED' | 'CANCELLED' | 'OPERATION_FAILED';
  /**
   * `true` when the request was cancelled by `http.abort`; absent otherwise.
   * @zh 请求是否由 http.abort 主动中止（仅在被取消时为 true，其余情况缺省）
   */
  cancelled?: boolean;
}

interface DownloadCompletePayload {
  /**
   * Id from the receipt.
   * @zh 下载任务回执中分配的唯一标识符 requestId
   */
  requestId: string;
  /**
   * Whether the file was written. A response with an error status such as `404` is saved and
   * counts as a success.
   * @zh 文件是否已成功写入本地磁盘（服务端返回 404 等响应内容亦会写入并视为成功）
   */
  success: boolean;
  /**
   * HTTP status code: on success that of the response; on failure only when a redirect was
   * refused under `redirect: "error"`, that of the redirect.
   * @zh HTTP 状态码（成功时为服务端响应码；仅在 redirect: "error" 拦截重定向时提供重定向状态码）
   */
  status?: Int;
  /**
   * Bytes written; present on success.
   * @zh 实际写入磁盘的文件总字节数（仅在成功时提供）
   */
  bytesWritten?: Int;
  /**
   * The file written, with path variables expanded; present on success.
   * @zh 实际写入文件的完整绝对路径（已展开路径变量；仅在成功时提供）
   */
  path?: string;
  /**
   * Why the download failed; present on failure.
   * @zh 下载失败的具体错误原因描述（仅在失败时提供）
   */
  error?: string;
  /**
   * The error code, as in `http:response`. Present on failure.
   * @zh 错误码代码（规则同 http:response；仅在失败时提供）
   */
  code?: 'INVALID_PARAMS' | 'PERMISSION_DENIED' | 'CANCELLED' | 'OPERATION_FAILED';
  /**
   * `true` when the download was cancelled by `http.abort`; absent otherwise.
   * @zh 下载任务是否由 http.abort 主动中止（仅在被取消时为 true，其余情况缺省）
   */
  cancelled?: boolean;
}

interface RequestParams {
  /**
   * Request URL; only `http` and `https` are allowed.
   * @zh 目标请求 URL（仅支持 http 与 https 协议）
   * @minLength 1
   */
  url: string;
  /**
   * Custom request headers. `User-Agent` overrides the host default on the initial request.
   * A header whose name or value contains a line break is dropped. Custom headers are omitted
   * after a redirect, including `User-Agent`, so redirected requests use the host default.
   * @zh 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值）
   */
  headers?: Record<string, string>;
  /**
   * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less
   * keeps the WinHTTP defaults.
   * @zh 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）
   * @default 30000
   */
  timeout?: Int;
  /**
   * Return a receipt at once and deliver the response as `http:response`; `false` blocks until
   * the response is in.
   * @zh 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）
   * @default true
   */
  async?: boolean;
  /**
   * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect;
   * `manual` returns the redirect response as it is.
   * @zh 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）
   * @default "follow"
   */
  redirect?: 'follow' | 'error' | 'manual';
  /**
   * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`,
   * return it Base64-encoded.
   * @zh 响应体数据格式（text 返回纯文本；base64 及其别名 arraybuffer、binary 返回 Base64 编码字符串）
   * @default "text"
   */
  responseType?: 'text' | 'base64' | 'arraybuffer' | 'binary';
  /**
   * Accept invalid or self-signed certificates; takes effect only when the host setting allows
   * insecure TLS as well.
   * @zh 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）
   * @default false
   */
  insecureTls?: boolean;
}

interface BodyRequestParams {
  /**
   * Request URL; only `http` and `https` are allowed.
   * @zh 目标请求 URL（仅支持 http 与 https 协议）
   * @minLength 1
   */
  url: string;
  /**
   * Request body: a string is sent as it is, an object or array as its JSON text, and any other
   * value as an empty body.
   * @zh 请求体内容（字符串原样发送，对象或数组序列化为 JSON 文本发送，其余类型作为空请求体发送）
   */
  body?: Json;
  /**
   * Custom request headers. `User-Agent` overrides the host default on the initial request.
   * A header whose name or value contains a line break is dropped. Custom headers are omitted
   * after a redirect, including `User-Agent`, so redirected requests use the host default.
   * @zh 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值）
   */
  headers?: Record<string, string>;
  /**
   * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less
   * keeps the WinHTTP defaults.
   * @zh 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）
   * @default 30000
   */
  timeout?: Int;
  /**
   * Return a receipt at once and deliver the response as `http:response`; `false` blocks until
   * the response is in.
   * @zh 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）
   * @default true
   */
  async?: boolean;
  /**
   * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect;
   * `manual` returns the redirect response as it is.
   * @zh 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）
   * @default "follow"
   */
  redirect?: 'follow' | 'error' | 'manual';
  /**
   * `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`,
   * return it Base64-encoded.
   * @zh 响应体数据格式（text 返回纯文本；base64 及其别名 arraybuffer、binary 返回 Base64 编码字符串）
   * @default "text"
   */
  responseType?: 'text' | 'base64' | 'arraybuffer' | 'binary';
  /**
   * Accept invalid or self-signed certificates; takes effect only when the host setting allows
   * insecure TLS as well.
   * @zh 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）
   * @default false
   */
  insecureTls?: boolean;
}

interface HeadParams {
  /**
   * Request URL; only `http` and `https` are allowed.
   * @zh 目标请求 URL（仅支持 http 与 https 协议）
   * @minLength 1
   */
  url: string;
  /**
   * Custom request headers. `User-Agent` overrides the host default on the initial request.
   * A header whose name or value contains a line break is dropped. Custom headers are omitted
   * after a redirect, including `User-Agent`, so redirected requests use the host default.
   * @zh 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值）
   */
  headers?: Record<string, string>;
  /**
   * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less
   * keeps the WinHTTP defaults.
   * @zh 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）
   * @default 30000
   */
  timeout?: Int;
  /**
   * Return a receipt at once and deliver the response as `http:response`; `false` blocks until
   * the response is in.
   * @zh 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）
   * @default true
   */
  async?: boolean;
  /**
   * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect;
   * `manual` returns the redirect response as it is.
   * @zh 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）
   * @default "follow"
   */
  redirect?: 'follow' | 'error' | 'manual';
  /**
   * Accept invalid or self-signed certificates; takes effect only when the host setting allows
   * insecure TLS as well.
   * @zh 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）
   * @default false
   */
  insecureTls?: boolean;
}

interface RequestResult {
  /**
   * Id of the request; present in an async receipt. `http:response` and `http.abort` use it.
   * @zh 异步请求任务分配的唯一标识符 requestId（仅在异步回执中提供，用于关联 http:response 或传给 http.abort）
   */
  requestId?: string;
  /**
   * `true` in an async receipt.
   * @zh 是否为异步任务回执（异步模式下固定为 true）
   */
  async?: boolean;
  /**
   * HTTP status code; present in a synchronous response.
   * @zh HTTP 状态码（仅在同步请求完成时提供）
   */
  status?: Int;
  /**
   * Response headers by name; of repeated headers the last one wins. Present in a synchronous
   * response.
   * @zh 响应头键值对（键名转为小写；重复响应头以最后出现的数值为准；仅在同步请求完成时提供）
   */
  headers?: Record<string, string>;
  /**
   * Response body, as text or Base64 according to `responseType`; present in a synchronous
   * response.
   * @zh 响应体内容（依据 responseType 格式化；仅在同步请求完成时提供）
   */
  body?: string;
  /**
   * `text` or `base64`, the encoding of `body`; present in a synchronous response.
   * @zh 响应体内容的编码格式（text 或 base64；仅在同步请求完成时提供）
   */
  responseType?: 'text' | 'base64';
}

interface HeadResult {
  /**
   * Id of the request; present in an async receipt. `http:response` and `http.abort` use it.
   * @zh 异步请求任务分配的唯一标识符 requestId（仅在异步回执中提供，用于关联 http:response 或传给 http.abort）
   */
  requestId?: string;
  /**
   * `true` in an async receipt.
   * @zh 是否为异步任务回执（异步模式下固定为 true）
   */
  async?: boolean;
  /**
   * HTTP status code; present in a synchronous response.
   * @zh HTTP 状态码（仅在同步请求完成时提供）
   */
  status?: Int;
  /**
   * Response headers by name; of repeated headers the last one wins. Present in a synchronous
   * response.
   * @zh 响应头键值对（键名转为小写；重复响应头以最后出现的数值为准；仅在同步请求完成时提供）
   */
  headers?: Record<string, string>;
  /**
   * Always empty; present in a synchronous response.
   * @zh 响应体内容（HEAD 请求固定为空字符串；仅在同步请求完成时提供）
   */
  body?: string;
  /**
   * Always `text`; present in a synchronous response.
   * @zh 响应体编码格式（HEAD 请求固定为 text；仅在同步请求完成时提供）
   */
  responseType?: 'text' | 'base64';
  /**
   * The `Content-Length` header as a number; present in a synchronous response when the server
   * sends one that parses.
   * @zh 数值形式的 Content-Length 响应头（仅在同步请求完成且服务端返回合法数字时提供）
   */
  contentLength?: Int;
}

// ---- download ----

interface DownloadParams {
  /**
   * URL to download; only `http` and `https` are allowed.
   * @zh 目标下载资源的 URL（仅支持 http 与 https 协议）
   * @minLength 1
   */
  url: string;
  /**
   * File to write; path variables are expanded and missing folders are created. An existing
   * file is replaced.
   * @zh 待保存的目标文件路径（支持路径变量展开并自动创建缺失的父级目录；若目标文件已存在将被覆盖）
   * @minLength 1
   * @security Write
   */
  saveTo: string;
  /**
   * Custom request headers. `User-Agent` overrides the host default on the initial request.
   * A header whose name or value contains a line break is dropped. Custom headers are omitted
   * after a redirect, including `User-Agent`, so redirected requests use the host default.
   * @zh 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；包含换行符的请求头将被丢弃；发生重定向后将丢弃自定义头）
   */
  headers?: Record<string, string>;
  /**
   * Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less
   * keeps the WinHTTP defaults.
   * @zh 各阶段网络超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）
   * @default 60000
   */
  timeout?: Int;
  /**
   * Return a receipt at once and deliver the outcome as `http:downloadComplete`; `false` blocks
   * until the file is written.
   * @zh 是否以异步模式下载（默认 false，阻塞调用至文件写入完成；传 true 立即返回回执并通过 http:downloadComplete 派发结果）
   * @default false
   */
  async?: boolean;
  /**
   * `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect;
   * `manual` saves the redirect response as it is.
   * @zh 重定向处理策略（follow 最多跟随 10 次重定向；error 遇到重定向报错；manual 原样保存重定向响应）
   * @default "follow"
   */
  redirect?: 'follow' | 'error' | 'manual';
  /**
   * Accept invalid or self-signed certificates; takes effect only when the host setting allows
   * insecure TLS as well.
   * @zh 是否允许无效或自签名 TLS 证书（须宿主全局设置开启方可生效）
   * @default false
   */
  insecureTls?: boolean;
}

interface DownloadResult {
  /**
   * Id of the download; present in an async receipt. `http:downloadComplete` and `http.abort`
   * use it.
   * @zh 异步下载任务分配的唯一标识符 requestId（仅在异步回执中提供）
   */
  requestId?: string;
  /**
   * `true` in an async receipt.
   * @zh 是否为异步任务回执（异步模式下固定为 true）
   */
  async?: boolean;
  /**
   * `Download started` in an async receipt.
   * @zh 异步任务启动状态描述（异步回执中固定为 Download started）
   */
  message?: string;
  /**
   * HTTP status code; present when the download finished synchronously.
   * @zh HTTP 状态码（仅在同步下载完成时提供）
   */
  status?: Int;
  /**
   * Bytes written; present when the download finished synchronously.
   * @zh 实际写入磁盘的文件总字节数（仅在同步下载完成时提供）
   */
  bytesWritten?: Int;
  /**
   * The file written, with path variables expanded; present when the download finished
   * synchronously.
   * @zh 实际写入文件的完整绝对路径（已展开路径变量；仅在同步下载完成时提供）
   */
  path?: string;
}

// ---- abort ----

interface AbortParams {
  /**
   * Id from the receipt of the request or download.
   * @zh 待中止请求或下载任务的唯一标识符 requestId
   * @minLength 1
   */
  requestId: string;
}

interface AbortResult {
  /**
   * The id, as given.
   * @zh 传入的任务标识符 requestId
   */
  requestId: string;
  /**
   * `true` when the request was still running and has been told to stop; `false` when it had
   * already ended or never existed.
   * @zh 任务中止结果（若请求仍在执行且已发送中止信号则为 true；若任务已结束或不存在则为 false）
   */
  cancelled: boolean;
}
