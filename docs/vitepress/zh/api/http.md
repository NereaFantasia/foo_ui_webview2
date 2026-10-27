# HTTP API

提供基于 WinHTTP 的网络请求能力，支持 GET、POST、PUT、DELETE、PATCH、HEAD、文件下载及任务中止控制。

::: warning 安全限制

- 仅允许 `http://` 和 `https://` 协议
- 默认禁止访问内网地址（SSRF 防护），包括 IPv4/IPv6 私有地址
- **重定向安全**: 每次重定向跳转都会重新检查 SSRF 规则，最多 10 次跳转
- 单次响应体上限 100MB，单次下载上限 500MB
- 最大并发异步请求数 10（含异步下载）
- TLS 连接失败时返回详细诊断信息
- 可在高级设置中配置内网白名单
- 建议仅用于访问可信的外部 API

:::

## 自定义 User-Agent

传入 `headers: { 'User-Agent': 'MyTheme/1.0' }` 即可标识自己的主题，适用于 GET、POST、PUT、DELETE、PATCH、HEAD 和下载。不指定时，宿主发送 `foo_ui_webview2/1.0 (WinHTTP)`。

自定义请求头仅用于首次请求。重定向后不再携带，`User-Agent` 也恢复为宿主默认值。如果服务要求每次请求都使用你的 User-Agent，应直接请求最终 URL。

## 共享请求选项

`http.get`、`http.post`、`http.put`、`http.delete`、`http.patch` 的参数相同，只是后四个多一个 `body`；`http.head` 没有 `responseType`，`http.download` 另有 `saveTo`，默认同步、超时 60000 毫秒。各方法的参数表见下文，下面两项需要额外说明。

### responseType 取值

| 取值 | body 字段类型（C++ 原始返回） | SDK facade 自动解码后的类型 |
| --- | --- | --- |
| `text` | UTF-8 字符串 | `string` |
| `base64` | base64 字符串 | `string` |
| `arraybuffer` | base64 字符串（host 内部传输） | `ArrayBuffer`（SDK 自动 base64 → ArrayBuffer） |
| `binary` | base64 字符串（`arraybuffer` 别名） | `ArrayBuffer`（同上） |

::: tip 二进制响应建议
通过 SDK facade 调用 `fb.http.get(url, { responseType: 'arraybuffer' })` 时返回值的 `body` 已是 `ArrayBuffer`，无需手动 base64 解码。直接用 `bridge.invoke('http.get', ...)` 时 `body` 仍为 base64 字符串，需要主题侧自行解码。
:::

### insecureTls：跳过 TLS 校验（双层门禁）

某些场景需要访问自签证书或证书已过期的 https 服务（局域网仪表盘、本地 Plex/Jellyfin、自建 Lidarr）。`insecureTls: true` 跳过 WinHTTP 的证书校验，但**必须同时满足两层条件**才生效：

1. **全局开关 ON**：foobar2000 高级设置 `Tools → WebView UI → HTTP Security → Allow self-signed / invalid TLS certificates`（默认 OFF）
2. **每请求 opt-in**：调用时显式传 `insecureTls: true`

任一条件不满足时，WinHTTP 仍执行严格 TLS 校验。这是有意为之 —— 全局开关让用户对 host 具有最终控制，per-request flag 防止一次配置后所有请求长期裸奔。

::: warning 安全风险
启用此选项会跳过所有证书校验（包括过期、域名不匹配、未知 CA）。仅在以下场景使用：

- 访问内网受控的自签证书服务
- 调试本地开发环境
- **绝对禁止**对公网 https 流量启用 — 等同于关闭 https 防中间人保护

更安全的替代方案：把目标 CA 加入系统受信任根证书。
:::

```javascript
// 启用前请先打开高级设置全局开关，否则此请求仍会被严格校验
const result = await fb2k.invoke('http.get', {
    url: 'https://192.168.1.100:8096/Status',
    insecureTls: true,
    timeout: 5000
});
```

### http.get

<!-- api-schema:begin http.get -->
发送 HTTP GET 请求（默认立即返回回执并通过携带相同 requestId 的 http:response 事件异步回送响应；传 async: false 时宿主阻塞至收到响应后直接返回；除非宿主高级设置明确允许，否则请求本地或局域网私有地址将以 PERMISSION_DENIED 拒绝，每次重定向均会重新校验）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 目标请求 URL（仅支持 http 与 https 协议）不能为空。 |
| `headers` | `Record<string, string>` | 否 | 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值） |
| `timeout` | `integer` | 否 | 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）默认 `30000`。 |
| `async` | `boolean` | 否 | 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | 响应体数据格式（text 返回纯文本；base64 及其别名 arraybuffer、binary 返回 Base64 编码字符串）默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 异步请求任务分配的唯一标识符 requestId（仅在异步回执中提供，用于关联 http:response 或传给 http.abort） |
| `async` | `boolean` | 是否为异步任务回执（异步模式下固定为 true） |
| `status` | `integer` | HTTP 状态码（仅在同步请求完成时提供） |
| `headers` | `Record<string, string>` | 响应头键值对（键名转为小写；重复响应头以最后出现的数值为准；仅在同步请求完成时提供） |
| `body` | `string` | 响应体内容（依据 responseType 格式化；仅在同步请求完成时提供） |
| `responseType` | `"text" \| "base64"` | 响应体内容的编码格式（text 或 base64；仅在同步请求完成时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.post

<!-- api-schema:begin http.post -->
发送 HTTP POST 请求（其余参数行为与 http.get 一致）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 目标请求 URL（仅支持 http 与 https 协议）不能为空。 |
| `body` | `any` | 否 | 请求体内容（字符串原样发送，对象或数组序列化为 JSON 文本发送，其余类型作为空请求体发送） |
| `headers` | `Record<string, string>` | 否 | 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值） |
| `timeout` | `integer` | 否 | 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）默认 `30000`。 |
| `async` | `boolean` | 否 | 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | 响应体数据格式（text 返回纯文本；base64 及其别名 arraybuffer、binary 返回 Base64 编码字符串）默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 异步请求任务分配的唯一标识符 requestId（仅在异步回执中提供，用于关联 http:response 或传给 http.abort） |
| `async` | `boolean` | 是否为异步任务回执（异步模式下固定为 true） |
| `status` | `integer` | HTTP 状态码（仅在同步请求完成时提供） |
| `headers` | `Record<string, string>` | 响应头键值对（键名转为小写；重复响应头以最后出现的数值为准；仅在同步请求完成时提供） |
| `body` | `string` | 响应体内容（依据 responseType 格式化；仅在同步请求完成时提供） |
| `responseType` | `"text" \| "base64"` | 响应体内容的编码格式（text 或 base64；仅在同步请求完成时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.head

<!-- api-schema:begin http.head -->
发送 HTTP HEAD 请求（其余参数行为与 http.get 一致；响应体恒为空字符串，若服务器返回数字形式的 Content-Length 则同步附带 contentLength）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 目标请求 URL（仅支持 http 与 https 协议）不能为空。 |
| `headers` | `Record<string, string>` | 否 | 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值） |
| `timeout` | `integer` | 否 | 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）默认 `30000`。 |
| `async` | `boolean` | 否 | 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）默认 `"follow"`。 |
| `insecureTls` | `boolean` | 否 | 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 异步请求任务分配的唯一标识符 requestId（仅在异步回执中提供，用于关联 http:response 或传给 http.abort） |
| `async` | `boolean` | 是否为异步任务回执（异步模式下固定为 true） |
| `status` | `integer` | HTTP 状态码（仅在同步请求完成时提供） |
| `headers` | `Record<string, string>` | 响应头键值对（键名转为小写；重复响应头以最后出现的数值为准；仅在同步请求完成时提供） |
| `body` | `string` | 响应体内容（HEAD 请求固定为空字符串；仅在同步请求完成时提供） |
| `responseType` | `"text" \| "base64"` | 响应体编码格式（HEAD 请求固定为 text；仅在同步请求完成时提供） |
| `contentLength` | `integer` | 数值形式的 Content-Length 响应头（仅在同步请求完成且服务端返回合法数字时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.put

<!-- api-schema:begin http.put -->
发送 HTTP PUT 请求（其余参数行为与 http.get 一致）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 目标请求 URL（仅支持 http 与 https 协议）不能为空。 |
| `body` | `any` | 否 | 请求体内容（字符串原样发送，对象或数组序列化为 JSON 文本发送，其余类型作为空请求体发送） |
| `headers` | `Record<string, string>` | 否 | 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值） |
| `timeout` | `integer` | 否 | 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）默认 `30000`。 |
| `async` | `boolean` | 否 | 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | 响应体数据格式（text 返回纯文本；base64 及其别名 arraybuffer、binary 返回 Base64 编码字符串）默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 异步请求任务分配的唯一标识符 requestId（仅在异步回执中提供，用于关联 http:response 或传给 http.abort） |
| `async` | `boolean` | 是否为异步任务回执（异步模式下固定为 true） |
| `status` | `integer` | HTTP 状态码（仅在同步请求完成时提供） |
| `headers` | `Record<string, string>` | 响应头键值对（键名转为小写；重复响应头以最后出现的数值为准；仅在同步请求完成时提供） |
| `body` | `string` | 响应体内容（依据 responseType 格式化；仅在同步请求完成时提供） |
| `responseType` | `"text" \| "base64"` | 响应体内容的编码格式（text 或 base64；仅在同步请求完成时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.delete

<!-- api-schema:begin http.delete -->
发送 HTTP DELETE 请求（其余参数行为与 http.get 一致）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 目标请求 URL（仅支持 http 与 https 协议）不能为空。 |
| `body` | `any` | 否 | 请求体内容（字符串原样发送，对象或数组序列化为 JSON 文本发送，其余类型作为空请求体发送） |
| `headers` | `Record<string, string>` | 否 | 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值） |
| `timeout` | `integer` | 否 | 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）默认 `30000`。 |
| `async` | `boolean` | 否 | 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | 响应体数据格式（text 返回纯文本；base64 及其别名 arraybuffer、binary 返回 Base64 编码字符串）默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 异步请求任务分配的唯一标识符 requestId（仅在异步回执中提供，用于关联 http:response 或传给 http.abort） |
| `async` | `boolean` | 是否为异步任务回执（异步模式下固定为 true） |
| `status` | `integer` | HTTP 状态码（仅在同步请求完成时提供） |
| `headers` | `Record<string, string>` | 响应头键值对（键名转为小写；重复响应头以最后出现的数值为准；仅在同步请求完成时提供） |
| `body` | `string` | 响应体内容（依据 responseType 格式化；仅在同步请求完成时提供） |
| `responseType` | `"text" \| "base64"` | 响应体内容的编码格式（text 或 base64；仅在同步请求完成时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.patch

<!-- api-schema:begin http.patch -->
发送 HTTP PATCH 请求（其余参数行为与 http.get 一致）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 目标请求 URL（仅支持 http 与 https 协议）不能为空。 |
| `body` | `any` | 否 | 请求体内容（字符串原样发送，对象或数组序列化为 JSON 文本发送，其余类型作为空请求体发送） |
| `headers` | `Record<string, string>` | 否 | 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；键名或键值包含换行符的请求头将被直接丢弃；发生重定向后将丢弃全部自定义头并恢复 User-Agent 为宿主默认值） |
| `timeout` | `integer` | 否 | 域名解析、建立连接、发送数据与接收响应各自的超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）默认 `30000`。 |
| `async` | `boolean` | 否 | 是否以异步模式执行（默认 true，立即返回任务回执并通过 http:response 派发响应；传 false 时阻塞当前调用至接收到响应）默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | 重定向处理策略（follow 最多自动跟随 10 次重定向；error 在遇到重定向时以 OPERATION_FAILED 报错；manual 原样返回重定向响应）默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | 响应体数据格式（text 返回纯文本；base64 及其别名 arraybuffer、binary 返回 Base64 编码字符串）默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 是否接受无效或自签名 TLS 证书（仅在宿主高级设置中同时启用了不安全 TLS 选项时方可生效）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 异步请求任务分配的唯一标识符 requestId（仅在异步回执中提供，用于关联 http:response 或传给 http.abort） |
| `async` | `boolean` | 是否为异步任务回执（异步模式下固定为 true） |
| `status` | `integer` | HTTP 状态码（仅在同步请求完成时提供） |
| `headers` | `Record<string, string>` | 响应头键值对（键名转为小写；重复响应头以最后出现的数值为准；仅在同步请求完成时提供） |
| `body` | `string` | 响应体内容（依据 responseType 格式化；仅在同步请求完成时提供） |
| `responseType` | `"text" \| "base64"` | 响应体内容的编码格式（text 或 base64；仅在同步请求完成时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.abort

<!-- api-schema:begin http.abort -->
中止正在执行的异步 HTTP 请求或下载任务（中止后请求将以报告 CANCELLED 状态的事件结束）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `requestId` | `string` | 是 | 待中止请求或下载任务的唯一标识符 requestId不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 传入的任务标识符 requestId |
| `cancelled` | `boolean` | 任务中止结果（若请求仍在执行且已发送中止信号则为 true；若任务已结束或不存在则为 false） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 当 `requestId` 对应的请求已完成或不存在时，返回 `cancelled: false`。当 `requestId` 为空时，返回 `success: false`。

**示例**：

```javascript
// 发起异步请求
const res = await fb2k.invoke('http.get', {
    url: 'https://api.example.com/large-data',
    async: true
});
if (res.success === false) throw new Error(res.error);
const { requestId } = res;

// 中止请求
const result = await fb2k.invoke('http.abort', { requestId });
if (result.success === false) throw new Error(result.error);
if (result.cancelled) {
    console.log('请求已中止');
} else {
    console.log('请求已完成，无法中止');
}
```

### http.download

<!-- api-schema:begin http.download -->
将指定 URL 资源下载并写入本地文件（自动展开路径变量并创建缺失的父级目录；默认阻塞至文件写入完成；传 async: true 时立即返回回执并随后派发 http:downloadComplete 事件；超过 500 MB 的下载将以 OPERATION_FAILED 失败）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 目标下载资源的 URL（仅支持 http 与 https 协议）不能为空。 |
| `saveTo` | `string` | 是 | 待保存的目标文件路径（支持路径变量展开并自动创建缺失的父级目录；若目标文件已存在将被覆盖）不能为空。 |
| `headers` | `Record<string, string>` | 否 | 自定义 HTTP 请求头（User-Agent 可覆盖首次请求的宿主默认值；包含换行符的请求头将被丢弃；发生重定向后将丢弃自定义头） |
| `timeout` | `integer` | 否 | 各阶段网络超时时间（单位毫秒；传入 0 或负数保留 WinHTTP 默认值）默认 `60000`。 |
| `async` | `boolean` | 否 | 是否以异步模式下载（默认 false，阻塞调用至文件写入完成；传 true 立即返回回执并通过 http:downloadComplete 派发结果）默认 `false`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | 重定向处理策略（follow 最多跟随 10 次重定向；error 遇到重定向报错；manual 原样保存重定向响应）默认 `"follow"`。 |
| `insecureTls` | `boolean` | 否 | 是否允许无效或自签名 TLS 证书（须宿主全局设置开启方可生效）默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 异步下载任务分配的唯一标识符 requestId（仅在异步回执中提供） |
| `async` | `boolean` | 是否为异步任务回执（异步模式下固定为 true） |
| `message` | `string` | 异步任务启动状态描述（异步回执中固定为 Download started） |
| `status` | `integer` | HTTP 状态码（仅在同步下载完成时提供） |
| `bytesWritten` | `integer` | 实际写入磁盘的文件总字节数（仅在同步下载完成时提供） |
| `path` | `string` | 实际写入文件的完整绝对路径（已展开路径变量；仅在同步下载完成时提供） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 错误处理

下例显式使用同步模式，因为默认 `async: true` 时当前返回值只是
`{ success: true, requestId, async: true }` 派发回执，不包含最终 HTTP
状态与 body。默认异步模式应按 `requestId` 等待 `http:response`，或使用
SDK 的 `fb.http.request()` awaited helper。

```javascript
try {
    const result = await fb2k.invoke('http.get', {
        url: 'https://api.example.com/data',
        async: false
    });

    if (!result.success) {
        console.error('请求失败');
        return;
    }

    if (result.status !== 200) {
        console.error('HTTP 错误:', result.status);
        return;
    }

    // 处理响应
    const data = JSON.parse(result.body);

} catch (error) {
    console.error('异常:', error.message);
}
```

## 安全配置

在 foobar2000 高级设置中配置 HTTP 安全选项：

**Preferences → Advanced → Tools → WebView UI → HTTP Security**

- `Allow Internal Network`: 允许访问内网地址（默认禁用）
- `Allow self-signed / invalid TLS certificates`: 允许 `insecureTls` 跳过 TLS 校验（默认禁用，是 `insecureTls: true` 生效的必要条件）
- `Allowed Hosts`: 白名单域名列表（逗号分隔）
- `Blocked Hosts`: 黑名单域名列表（逗号分隔）

```text
# 示例配置
Allowed Hosts: api.example.com, cdn.example.com
Blocked Hosts: localhost, 127.0.0.1, 192.168.*
```

## 相关文档

- [File API](./file) - 文件读写
- [Events API](./events) - 事件监听系统

## 请求生命周期与安全

- `http.get`、`http.post`、`http.put`、`http.delete`、`http.patch` 和 `http.head` 默认异步执行。立即返回的 `success: true` 只表示派发成功，并包含 `requestId`；它不是 HTTP 最终结果。最终结果会以 `http:response` 发回调用窗口，调用方必须按 `requestId` 关联。
- 若需同步执行，传入 `async: false`。成功的非下载响应包含 `status`、`headers`、`body` 和 `responseType`。当响应包含 `Content-Length` 时，成功的同步或异步 HEAD 结果还可能包含数值 `contentLength`。
- `http.download` 默认同步。传入 `async: true` 后，最终结果会作为带 `requestId` 的 `http:downloadComplete` 发出；`http.abort` 用于请求取消活跃的异步请求。
- 仅接受 `http` 与 `https` URL。除非 host 的 Advanced Settings 显式允许，否则私有或本地网络目标会被拒绝；每次重定向都会重新检查，最多允许 10 跳。
- 失败带 `code`，直接返回的结果与 `http:response`、`http:downloadComplete` 事件一致：本地或私有网络目标被拒为 `PERMISSION_DENIED`；URL 无法解析或协议不是 `http`、`https` 为 `INVALID_PARAMS`；被 `http.abort` 取消为 `CANCELLED`，并带 `cancelled: true`；网络错误、`redirect: "error"` 遇到重定向（带 `status`）以及超过大小上限为 `OPERATION_FAILED`。
- 只有调用方传入 `insecureTls: true` **且** host 的无效证书设置已启用时，才会跳过 TLS 校验。该绕过方式不适用于公网流量。
- 响应体上限为 100 MB，下载上限为 500 MB。`http.download.saveTo` 是受 Bridge 安全策略保护的写入路径参数。
