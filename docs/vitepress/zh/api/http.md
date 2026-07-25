# HTTP API

HTTP 请求功能，支持 GET/POST/PUT/DELETE/PATCH/HEAD/Download/Abort。共 8 个 API。

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

## 共享请求选项

下表参数为所有 `http.*` verb（除 `http.abort`）通用：

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| url | string | ✓ | 请求 URL（仅允许 `http://` / `https://`） |
| headers | object | ✗ | 自定义请求头 |
| timeout | number | ✗ | 超时时间 ms（默认 30000；download 默认 60000） |
| async | boolean | ✗ | 是否异步请求（除 `download` 默认 false 外，默认 true） |
| redirect | string | ✗ | 重定向策略（`follow` / `error` / `manual`，默认 `follow`） |
| responseType | string | ✗ | 响应类型，见下表（默认 `text`；`download` / `head` 不支持） |
| insecureTls | boolean | ✗ | 跳过 TLS 证书校验（默认 `false`），见下方说明 |

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

发送 HTTP GET 请求。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `async` | `boolean` | 否 | 可选；默认 true。 |
| `headers` | `object` | 否 | 可选；默认 omitted。 |
| `insecureTls` | `boolean` | 否 | 可选；默认 false。 |
| `redirect` | `string` | 否 | 可选；默认 follow。 |
| `responseType` | `string` | 否 | 可选；默认 text。 |
| `timeout` | `integer` | 否 | 可选；默认 30000。 |
| `url` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"async":"...","requestId":"...","success":true}`


### http.post

发送 HTTP POST 请求。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 否 | 可选；默认 。 |
| `body` | `json` | 否 | 可选；默认 omitted。 |
| `headers` | `object` | 否 | 可选；默认 {}。 |
| `timeout` | `integer` | 否 | 可选；默认 30000。 |
| `async` | `boolean` | 否 | 可选；默认 true。 |
| `redirect` | `string` | 否 | 可选；默认 follow。 |
| `responseType` | `string` | 否 | 可选；默认 text。 |
| `insecureTls` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: 同 `http.get`。


### http.head

发送 HTTP HEAD 请求（仅获取响应头，不下载内容）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `async` | `boolean` | 否 | 可选；默认 true。 |
| `headers` | `object` | 否 | 可选；默认 omitted。 |
| `insecureTls` | `boolean` | 否 | 可选；默认 false。 |
| `redirect` | `string` | 否 | 可选；默认 follow。 |
| `timeout` | `integer` | 否 | 可选；默认 30000。 |
| `url` | `string` | 否 | 可选；默认 。 |

**返回值**: `{"async":"...","contentLength":"...","requestId":"...","success":true}`


### http.put

发送 HTTP PUT 请求。默认为异步模式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 否 | 可选；默认 。 |
| `body` | `json` | 否 | 可选；默认 omitted。 |
| `headers` | `object` | 否 | 可选；默认 {}。 |
| `timeout` | `integer` | 否 | 可选；默认 30000。 |
| `async` | `boolean` | 否 | 可选；默认 true。 |
| `redirect` | `string` | 否 | 可选；默认 follow。 |
| `responseType` | `string` | 否 | 可选；默认 text。 |
| `insecureTls` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: 同 `http.get`，异步结果通过 `http:response` 推送。


### http.delete

发送 HTTP DELETE 请求。默认为异步模式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 否 | 可选；默认 。 |
| `body` | `json` | 否 | 可选；默认 omitted。 |
| `headers` | `object` | 否 | 可选；默认 {}。 |
| `timeout` | `integer` | 否 | 可选；默认 30000。 |
| `async` | `boolean` | 否 | 可选；默认 true。 |
| `redirect` | `string` | 否 | 可选；默认 follow。 |
| `responseType` | `string` | 否 | 可选；默认 text。 |
| `insecureTls` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: 同 `http.get`，异步结果通过 `http:response` 推送。


### http.patch

发送 HTTP PATCH 请求。默认为异步模式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `url` | `string` | 否 | 可选；默认 。 |
| `body` | `json` | 否 | 可选；默认 omitted。 |
| `headers` | `object` | 否 | 可选；默认 {}。 |
| `timeout` | `integer` | 否 | 可选；默认 30000。 |
| `async` | `boolean` | 否 | 可选；默认 true。 |
| `redirect` | `string` | 否 | 可选；默认 follow。 |
| `responseType` | `string` | 否 | 可选；默认 text。 |
| `insecureTls` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: 同 `http.get`，异步结果通过 `http:response` 推送。


### http.abort

中止一个正在进行的异步 HTTP 请求。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `requestId` | `string` | 否 | 可选；默认 。 |

**返回值**：

```json
{
    "success": true,
    "requestId": "http_1",
    "cancelled": true
}
```

> 当 `requestId` 对应的请求已完成或不存在时，返回 `cancelled: false`。当 `requestId` 为空时，返回 `success: false`。

**示例**：

```javascript
// 发起异步请求
const { requestId } = await fb2k.invoke('http.get', {
    url: 'https://api.example.com/large-data',
    async: true
});

// 中止请求
const result = await fb2k.invoke('http.abort', { requestId });
if (result.cancelled) {
    console.log('请求已中止');
} else {
    console.log('请求已完成，无法中止');
}
```

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

### http.download

下载文件到本地。默认 **同步**（与其他 verb 不同），需显式 `async: true` 启用异步。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `async` | `boolean` | 否 | 可选；默认 false。 |
| `headers` | `object` | 否 | 可选；默认 omitted。 |
| `insecureTls` | `boolean` | 否 | 可选；默认 false。 |
| `redirect` | `string` | 否 | 可选；默认 follow。 |
| `saveTo` | `string` | 否 | 可选；默认 。 |
| `timeout` | `integer` | 否 | 可选；默认 60000。 |
| `url` | `string` | 否 | 可选；默认 。 |

**返回值**: 同步返回 path/bytesWritten/status；异步返回 requestId。

## 合同补充

以下章节补齐严格参数审计发现的公开 contract；不会改变前文的已有说明。

<!-- phase3-supplement:http.delete -->
### Contract 补充：`http.delete`

经复核的补充 contract。权威源：`src/api/HttpApi.cpp:1396-1432`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `url` | `string` | 否 | `` | 可选；默认 。 |
| `body` | `json` | 否 | 可省略 | 可选；默认 omitted。 |
| `headers` | `object` | 否 | `{}` | 可选；默认 {}。 |
| `timeout` | `integer` | 否 | `30000` | 可选；默认 30000。 |
| `async` | `boolean` | 否 | `true` | 可选；默认 true。 |
| `redirect` | `string` | 否 | `follow` | 可选；默认 follow。 |
| `responseType` | `string` | 否 | `text` | 可选；默认 text。 |
| `insecureTls` | `boolean` | 否 | `false` | 可选；默认 false。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `async` | `json` | 否 |
| `requestId` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('http.delete', { url: /* value */, body: /* value */, headers: /* value */, timeout: /* value */, async: /* value */, redirect: /* value */, responseType: /* value */, insecureTls: /* value */ });
```
<!-- phase3-supplement:http.download -->
### Contract 补充：`http.download`

经复核的补充 contract。权威源：`src/api/HttpApi.cpp:1237-1352`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `async` | `boolean` | 否 | `false` | 可选；默认 false。 |
| `headers` | `object` | 否 | 可省略 | 可选；默认 omitted。 |
| `insecureTls` | `boolean` | 否 | `false` | 可选；默认 false。 |
| `redirect` | `string` | 否 | `follow` | 可选；默认 follow。 |
| `saveTo` | `string` | 否 | `` | 可选；默认 。 |
| `timeout` | `integer` | 否 | `60000` | 可选；默认 60000。 |
| `url` | `string` | 否 | `` | 可选；默认 。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `code` | `string` | 是 |
| `async` | `json` | 否 |
| `message` | `json` | 否 |
| `requestId` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('http.download', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, saveTo: /* value */, timeout: /* value */, url: /* value */ });
```
<!-- phase3-supplement:http.get -->
### Contract 补充：`http.get`

经复核的补充 contract。权威源：`src/api/HttpApi.cpp:822-862`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `async` | `boolean` | 否 | `true` | 可选；默认 true。 |
| `headers` | `object` | 否 | 可省略 | 可选；默认 omitted。 |
| `insecureTls` | `boolean` | 否 | `false` | 可选；默认 false。 |
| `redirect` | `string` | 否 | `follow` | 可选；默认 follow。 |
| `responseType` | `string` | 否 | `text` | 可选；默认 text。 |
| `timeout` | `integer` | 否 | `30000` | 可选；默认 30000。 |
| `url` | `string` | 否 | `` | 可选；默认 。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `async` | `json` | 否 |
| `requestId` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('http.get', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, responseType: /* value */, timeout: /* value */, url: /* value */ });
```
<!-- phase3-supplement:http.head -->
### Contract 补充：`http.head`

经复核的补充 contract。权威源：`src/api/HttpApi.cpp:906-950`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `async` | `boolean` | 否 | `true` | 可选；默认 true。 |
| `headers` | `object` | 否 | 可省略 | 可选；默认 omitted。 |
| `insecureTls` | `boolean` | 否 | `false` | 可选；默认 false。 |
| `redirect` | `string` | 否 | `follow` | 可选；默认 follow。 |
| `timeout` | `integer` | 否 | `30000` | 可选；默认 30000。 |
| `url` | `string` | 否 | `` | 可选；默认 。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `async` | `json` | 否 |
| `requestId` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('http.head', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, timeout: /* value */, url: /* value */ });
```
<!-- phase3-supplement:http.patch -->
### Contract 补充：`http.patch`

经复核的补充 contract。权威源：`src/api/HttpApi.cpp:1436-1472`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `url` | `string` | 否 | `` | 可选；默认 。 |
| `body` | `json` | 否 | 可省略 | 可选；默认 omitted。 |
| `headers` | `object` | 否 | `{}` | 可选；默认 {}。 |
| `timeout` | `integer` | 否 | `30000` | 可选；默认 30000。 |
| `async` | `boolean` | 否 | `true` | 可选；默认 true。 |
| `redirect` | `string` | 否 | `follow` | 可选；默认 follow。 |
| `responseType` | `string` | 否 | `text` | 可选；默认 text。 |
| `insecureTls` | `boolean` | 否 | `false` | 可选；默认 false。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `async` | `json` | 否 |
| `requestId` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('http.patch', { url: /* value */, body: /* value */, headers: /* value */, timeout: /* value */, async: /* value */, redirect: /* value */, responseType: /* value */, insecureTls: /* value */ });
```
<!-- phase3-supplement:http.post -->
### Contract 补充：`http.post`

经复核的补充 contract。权威源：`src/api/HttpApi.cpp:866-902`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `url` | `string` | 否 | `` | 可选；默认 。 |
| `body` | `json` | 否 | 可省略 | 可选；默认 omitted。 |
| `headers` | `object` | 否 | `{}` | 可选；默认 {}。 |
| `timeout` | `integer` | 否 | `30000` | 可选；默认 30000。 |
| `async` | `boolean` | 否 | `true` | 可选；默认 true。 |
| `redirect` | `string` | 否 | `follow` | 可选；默认 follow。 |
| `responseType` | `string` | 否 | `text` | 可选；默认 text。 |
| `insecureTls` | `boolean` | 否 | `false` | 可选；默认 false。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `async` | `json` | 否 |
| `requestId` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('http.post', { url: /* value */, body: /* value */, headers: /* value */, timeout: /* value */, async: /* value */, redirect: /* value */, responseType: /* value */, insecureTls: /* value */ });
```
<!-- phase3-supplement:http.put -->
### Contract 补充：`http.put`

经复核的补充 contract。权威源：`src/api/HttpApi.cpp:1356-1392`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `url` | `string` | 否 | `` | 可选；默认 。 |
| `body` | `json` | 否 | 可省略 | 可选；默认 omitted。 |
| `headers` | `object` | 否 | `{}` | 可选；默认 {}。 |
| `timeout` | `integer` | 否 | `30000` | 可选；默认 30000。 |
| `async` | `boolean` | 否 | `true` | 可选；默认 true。 |
| `redirect` | `string` | 否 | `follow` | 可选；默认 follow。 |
| `responseType` | `string` | 否 | `text` | 可选；默认 text。 |
| `insecureTls` | `boolean` | 否 | `false` | 可选；默认 false。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `async` | `json` | 否 |
| `requestId` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('http.put', { url: /* value */, body: /* value */, headers: /* value */, timeout: /* value */, async: /* value */, redirect: /* value */, responseType: /* value */, insecureTls: /* value */ });
```

## 请求生命周期与安全

- `http.get`、`http.post`、`http.put`、`http.delete`、`http.patch` 和 `http.head` 默认异步执行。立即返回的 `success: true` 只表示派发成功，并包含 `requestId`；它不是 HTTP 最终结果。最终结果会以 `http:response` 发回调用窗口，调用方必须按 `requestId` 关联。
- 若需同步执行，传入 `async: false`。成功的非下载响应包含 `status`、`headers`、`body` 和 `responseType`。当响应包含 `Content-Length` 时，成功的同步或异步 HEAD 结果还可能包含数值 `contentLength`。
- `http.download` 默认同步。传入 `async: true` 后，最终结果会作为带 `requestId` 的 `http:downloadComplete` 发出；`http.abort` 用于请求取消活跃的异步请求。
- 仅接受 `http` 与 `https` URL。除非 host 的 Advanced Settings 显式允许，否则私有或本地网络目标会被拒绝；每次重定向都会重新检查，最多允许 10 跳。
- 只有调用方传入 `insecureTls: true` **且** host 的无效证书设置已启用时，才会跳过 TLS 校验。此旁路不适用于公网流量。
- 响应体上限为 100 MB，下载上限为 500 MB。`http.download.saveTo` 是受 Bridge 安全策略保护的写入路径参数。
