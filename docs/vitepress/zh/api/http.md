# HTTP API

HTTP 请求功能，支持 GET/POST/PUT/DELETE/PATCH/HEAD/Download/Abort。

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
发送 GET 请求。缺省时立即返回回执，响应随后以带同一 `requestId` 的 `http:response` 事件到达；`async: false` 时宿主阻塞到收到响应再返回。请求本地或私有网络地址会以 `PERMISSION_DENIED` 失败，除非宿主设置允许，每次重定向都会重新检查。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 请求 URL；只允许 `http` 与 `https`。不能为空。 |
| `headers` | `Record<string, string>` | 否 | 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。 |
| `timeout` | `integer` | 否 | 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。默认 `30000`。 |
| `async` | `boolean` | 否 | 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | `text` 以文本返回响应体；`base64` 及其别名 `arraybuffer`、`binary` 以 Base64 编码返回。默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 请求的 id；异步回执里出现。`http:response` 与 `http.abort` 用它。 |
| `async` | `boolean` | 异步回执里为 `true`。 |
| `status` | `integer` | HTTP 状态码；同步响应里出现。 |
| `headers` | `Record<string, string>` | 响应头，以名称为键；重复的响应头取最后一个。同步响应里出现。 |
| `body` | `string` | 响应体，按 `responseType` 为文本或 Base64；同步响应里出现。 |
| `responseType` | `"text" \| "base64"` | `text` 或 `base64`，即 `body` 的编码；同步响应里出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.post

<!-- api-schema:begin http.post -->
发送 POST 请求；其余同 `http.get`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 请求 URL；只允许 `http` 与 `https`。不能为空。 |
| `body` | `any` | 否 | 请求体：字符串原样发送，对象或数组发送其 JSON 文本，其他值按空请求体发送。 |
| `headers` | `Record<string, string>` | 否 | 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。 |
| `timeout` | `integer` | 否 | 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。默认 `30000`。 |
| `async` | `boolean` | 否 | 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | `text` 以文本返回响应体；`base64` 及其别名 `arraybuffer`、`binary` 以 Base64 编码返回。默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 请求的 id；异步回执里出现。`http:response` 与 `http.abort` 用它。 |
| `async` | `boolean` | 异步回执里为 `true`。 |
| `status` | `integer` | HTTP 状态码；同步响应里出现。 |
| `headers` | `Record<string, string>` | 响应头，以名称为键；重复的响应头取最后一个。同步响应里出现。 |
| `body` | `string` | 响应体，按 `responseType` 为文本或 Base64；同步响应里出现。 |
| `responseType` | `"text" \| "base64"` | `text` 或 `base64`，即 `body` 的编码；同步响应里出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.head

<!-- api-schema:begin http.head -->
发送 HEAD 请求；其余同 `http.get`。响应体恒为空文本，服务器发来数字形式的 `Content-Length` 时另带 `contentLength`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 请求 URL；只允许 `http` 与 `https`。不能为空。 |
| `headers` | `Record<string, string>` | 否 | 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。 |
| `timeout` | `integer` | 否 | 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。默认 `30000`。 |
| `async` | `boolean` | 否 | 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。默认 `"follow"`。 |
| `insecureTls` | `boolean` | 否 | 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 请求的 id；异步回执里出现。`http:response` 与 `http.abort` 用它。 |
| `async` | `boolean` | 异步回执里为 `true`。 |
| `status` | `integer` | HTTP 状态码；同步响应里出现。 |
| `headers` | `Record<string, string>` | 响应头，以名称为键；重复的响应头取最后一个。同步响应里出现。 |
| `body` | `string` | 恒为空；同步响应里出现。 |
| `responseType` | `"text" \| "base64"` | 恒为 `text`；同步响应里出现。 |
| `contentLength` | `integer` | 数字形式的 `Content-Length` 响应头；同步响应里、服务器发来可解析的值时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.put

<!-- api-schema:begin http.put -->
发送 PUT 请求；其余同 `http.get`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 请求 URL；只允许 `http` 与 `https`。不能为空。 |
| `body` | `any` | 否 | 请求体：字符串原样发送，对象或数组发送其 JSON 文本，其他值按空请求体发送。 |
| `headers` | `Record<string, string>` | 否 | 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。 |
| `timeout` | `integer` | 否 | 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。默认 `30000`。 |
| `async` | `boolean` | 否 | 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | `text` 以文本返回响应体；`base64` 及其别名 `arraybuffer`、`binary` 以 Base64 编码返回。默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 请求的 id；异步回执里出现。`http:response` 与 `http.abort` 用它。 |
| `async` | `boolean` | 异步回执里为 `true`。 |
| `status` | `integer` | HTTP 状态码；同步响应里出现。 |
| `headers` | `Record<string, string>` | 响应头，以名称为键；重复的响应头取最后一个。同步响应里出现。 |
| `body` | `string` | 响应体，按 `responseType` 为文本或 Base64；同步响应里出现。 |
| `responseType` | `"text" \| "base64"` | `text` 或 `base64`，即 `body` 的编码；同步响应里出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.delete

<!-- api-schema:begin http.delete -->
发送 DELETE 请求；其余同 `http.get`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 请求 URL；只允许 `http` 与 `https`。不能为空。 |
| `body` | `any` | 否 | 请求体：字符串原样发送，对象或数组发送其 JSON 文本，其他值按空请求体发送。 |
| `headers` | `Record<string, string>` | 否 | 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。 |
| `timeout` | `integer` | 否 | 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。默认 `30000`。 |
| `async` | `boolean` | 否 | 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | `text` 以文本返回响应体；`base64` 及其别名 `arraybuffer`、`binary` 以 Base64 编码返回。默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 请求的 id；异步回执里出现。`http:response` 与 `http.abort` 用它。 |
| `async` | `boolean` | 异步回执里为 `true`。 |
| `status` | `integer` | HTTP 状态码；同步响应里出现。 |
| `headers` | `Record<string, string>` | 响应头，以名称为键；重复的响应头取最后一个。同步响应里出现。 |
| `body` | `string` | 响应体，按 `responseType` 为文本或 Base64；同步响应里出现。 |
| `responseType` | `"text" \| "base64"` | `text` 或 `base64`，即 `body` 的编码；同步响应里出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.patch

<!-- api-schema:begin http.patch -->
发送 PATCH 请求；其余同 `http.get`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 请求 URL；只允许 `http` 与 `https`。不能为空。 |
| `body` | `any` | 否 | 请求体：字符串原样发送，对象或数组发送其 JSON 文本，其他值按空请求体发送。 |
| `headers` | `Record<string, string>` | 否 | 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。 |
| `timeout` | `integer` | 否 | 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。默认 `30000`。 |
| `async` | `boolean` | 否 | 立即返回回执、以 `http:response` 事件送达响应；为 `false` 时阻塞到收到响应。默认 `true`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样返回重定向响应。默认 `"follow"`。 |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | 否 | `text` 以文本返回响应体；`base64` 及其别名 `arraybuffer`、`binary` 以 Base64 编码返回。默认 `"text"`。 |
| `insecureTls` | `boolean` | 否 | 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 请求的 id；异步回执里出现。`http:response` 与 `http.abort` 用它。 |
| `async` | `boolean` | 异步回执里为 `true`。 |
| `status` | `integer` | HTTP 状态码；同步响应里出现。 |
| `headers` | `Record<string, string>` | 响应头，以名称为键；重复的响应头取最后一个。同步响应里出现。 |
| `body` | `string` | 响应体，按 `responseType` 为文本或 Base64；同步响应里出现。 |
| `responseType` | `"text" \| "base64"` | `text` 或 `base64`，即 `body` 的编码；同步响应里出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### http.abort

<!-- api-schema:begin http.abort -->
取消异步请求或下载。该请求随后以报告 `CANCELLED` 的事件结束。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `requestId` | `string` | 是 | 请求或下载回执里的 id。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 原样的 id。 |
| `cancelled` | `boolean` | 请求仍在进行并已被通知停止时为 `true`；已结束或从未存在时为 `false`。 |

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
把 URL 下载到文件，缺失的文件夹会被创建。缺省时阻塞到文件写完；`async: true` 时立即返回回执，结果随后以 `http:downloadComplete` 事件到达。超过 500 MB 的下载以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 是 | 要下载的 URL；只允许 `http` 与 `https`。不能为空。 |
| `saveTo` | `string` | 是 | 要写入的文件；会展开路径变量并创建缺失的文件夹。已有的文件会被覆盖。不能为空。 |
| `headers` | `Record<string, string>` | 否 | 自定义请求头。`User-Agent` 可覆盖首次请求的宿主默认值。名称或值含换行的请求头会被丢弃。重定向后不再携带自定义头，`User-Agent` 也恢复为宿主默认值。 |
| `timeout` | `integer` | 否 | 解析、连接、发送、接收各自的超时，单位毫秒；`0` 或负数保留 WinHTTP 缺省值。默认 `60000`。 |
| `async` | `boolean` | 否 | 立即返回回执、以 `http:downloadComplete` 事件送达结果；为 `false` 时阻塞到文件写完。默认 `false`。 |
| `redirect` | `"follow" \| "error" \| "manual"` | 否 | `follow` 最多跟随 10 次重定向；`error` 遇到重定向时以 `OPERATION_FAILED` 失败；`manual` 原样保存重定向响应。默认 `"follow"`。 |
| `insecureTls` | `boolean` | 否 | 接受无效或自签名证书；只有宿主设置同时允许不安全 TLS 时才生效。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestId` | `string` | 下载的 id；异步回执里出现。`http:downloadComplete` 与 `http.abort` 用它。 |
| `async` | `boolean` | 异步回执里为 `true`。 |
| `message` | `string` | 异步回执里为 `Download started`。 |
| `status` | `integer` | HTTP 状态码；同步下载完成时出现。 |
| `bytesWritten` | `integer` | 写入的字节数；同步下载完成时出现。 |
| `path` | `string` | 写入的文件（已展开路径变量）；同步下载完成时出现。 |

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
