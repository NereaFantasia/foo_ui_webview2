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

参数：见上方"共享请求选项"。

**同步模式返回值**（`async: false`）:

```json
{
    "success": true,
    "status": 200,
    "body": "...",
    "headers": {
        "content-type": "application/json",
        "content-length": "1234"
    },
    "responseType": "text"
}
```

> `responseType` 字段在响应中固定为 `text` 或 `base64`（`arraybuffer` / `binary` 在 host 端被映射为 `base64`，由 SDK facade 解码为 `ArrayBuffer`）。

**异步模式返回值**（`async: true`，默认）:

```json
{
    "success": true,
    "requestId": "req-123",
    "async": true
}
```

**异步模式事件**: `http:response` — 携带 `{ requestId, status, body, headers, responseType }` 或 `{ requestId, error }`

### 同步请求示例 

```javascript
const result = await fb2k.invoke('http.get', {
    url: 'https://api.example.com/data',
    async: false,
    headers: {
        'User-Agent': 'foo_ui_webview2/1.0',
        'Accept': 'application/json'
    },
    timeout: 10000
});

if (result.success) {
    const data = JSON.parse(result.body);
    console.log('响应数据:', data);
}
```

### 异步请求示例 

```javascript
// 发起异步请求
const result = await fb2k.invoke('http.get', {
    url: 'https://api.example.com/data',
    async: true
});
const requestId = result.requestId;

// 监听响应事件
fb2k.on('http:response', (data) => {
    if (data.requestId === requestId) {
        if (data.error) {
            console.error('请求失败:', data.error);
        } else {
            console.log('状态码:', data.status);
            console.log('响应体:', data.body);
            console.log('响应头:', data.headers);
        }
    }
});
```

### 二进制响应示例

```javascript
// 通过 SDK facade（推荐）— body 自动解码为 ArrayBuffer
import { fb } from 'foo-webview-sdk/bridge';
const resp = await fb.http.get('https://example.com/cover.jpg', {
    responseType: 'arraybuffer'
});
const blob = new Blob([resp.body]);
imgElement.src = URL.createObjectURL(blob);

// 直接 invoke — body 仍是 base64，需要手动解码
const raw = await fb2k.invoke('http.get', {
    url: 'https://example.com/cover.jpg',
    responseType: 'arraybuffer',
    async: false
});
const buf = Uint8Array.from(atob(raw.body), c => c.charCodeAt(0)).buffer;
```

::: tip 何时使用异步模式

- 请求耗时较长（>5秒）
- 需要同时发起多个请求
- 不希望阻塞 UI 线程

:::

### http.post 

发送 HTTP POST 请求。

参数：见上方"共享请求选项"，外加：

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| body | string \| object | ✗ | 请求体内容（object/array 自动 JSON 序列化） |

**返回值**: 同 `http.get`

```javascript
const result = await fb2k.invoke('http.post', {
    url: 'https://api.example.com/submit',
    body: JSON.stringify({ name: 'test', value: 123 }),
    headers: {
        'Content-Type': 'application/json'
    }
});

if (result.success && result.status === 200) {
    console.log('提交成功');
}
```

### http.head 

发送 HTTP HEAD 请求（仅获取响应头，不下载内容）。

参数：`url` / `headers` / `timeout` / `async` / `redirect` / `insecureTls`（不支持 `responseType`）。

**返回值**:

```json
{
    "success": true,
    "status": 200,
    "headers": {
        "content-type": "audio/mpeg",
        "content-length": "5242880",
        "last-modified": "Mon, 01 Jan 2024 00:00:00 GMT"
    }
}
```

```javascript
// 检查文件大小和类型
const result = await fb2k.invoke('http.head', {
    url: 'https://example.com/music.mp3'
});

if (result.success) {
    const size = parseInt(result.headers['content-length']);
    const type = result.headers['content-type'];
    console.log(`文件大小: ${(size / 1024 / 1024).toFixed(2)} MB`);
    console.log(`文件类型: ${type}`);
}
```

### http.download 

下载文件到本地。默认 **同步**（与其他 verb 不同），需显式 `async: true` 启用异步。

参数：除"共享请求选项"外，加：

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| saveTo | string | ✓ | 保存路径（绝对路径） |

> `download` 不支持 `responseType`（直接落盘）；`async` 默认 `false`；`timeout` 默认 60000；支持 `insecureTls`。

**同步模式返回值**（`async: false`，默认）:

```json
{
    "success": true,
    "path": "E:\\Downloads\\file.zip",
    "bytesWritten": 5242880,
    "status": 200
}
```

**异步模式返回值**（`async: true`）:

```json
{
    "success": true,
    "requestId": "req-abc123",
    "async": true,
    "message": "Download started"
}
```

**异步模式事件**: `http:downloadComplete` — 携带 `{ requestId, success, status, bytesWritten, path }` 或 `{ requestId, success: false, error, cancelled }`

```javascript
// 同步下载
const result = await fb2k.invoke('http.download', {
    url: 'https://example.com/album.zip',
    saveTo: 'E:\\Downloads\\album.zip',
    timeout: 600000  // 10 分钟
});

if (result.success) {
    console.log(`下载完成: ${(result.bytesWritten / 1024 / 1024).toFixed(2)} MB`);
}
```

```javascript
// 异步下载 + 取消支持
const result = await fb2k.invoke('http.download', {
    url: 'https://example.com/large-file.zip',
    saveTo: 'E:\\Downloads\\large-file.zip',
    async: true
});

const requestId = result.requestId;

fb2k.on('http:downloadComplete', (data) => {
    if (data.requestId === requestId) {
        if (data.success) {
            console.log(`下载完成: ${data.bytesWritten} bytes → ${data.path}`);
        } else if (data.cancelled) {
            console.log('下载已取消');
        } else {
            console.error('下载失败:', data.error);
        }
    }
});

// 取消下载
// await fb2k.invoke('http.abort', { requestId });
```

::: warning 路径限制
`saveTo` 必须是绝对路径，且只能保存到以下目录：
- foobar2000 配置目录
- 用户临时目录
- 用户下载目录

禁止写入系统目录或其他受保护位置。
:::

### http.put 

发送 HTTP PUT 请求。默认为异步模式。

参数：见"共享请求选项"，外加 `body`（同 `http.post`）。

**同步模式返回值**（`async: false`）：

```json
{
    "success": true,
    "status": 200,
    "headers": { "Content-Type": "application/json" },
    "body": "...",
    "responseType": "text"
}
```

**异步模式返回值**（`async: true`，默认）：

```json
{
    "success": true,
    "requestId": "http_1",
    "async": true
}
```

异步结果通过 `http:response` 事件推送。

**示例**：

```javascript
// 同步 PUT
const result = await fb2k.invoke('http.put', {
    url: 'https://api.example.com/resource/1',
    body: JSON.stringify({ name: '新名称' }),
    headers: { 'Content-Type': 'application/json' },
    async: false
});

// 异步 PUT
const { requestId } = await fb2k.invoke('http.put', {
    url: 'https://api.example.com/resource/1',
    body: JSON.stringify({ name: '新名称' }),
    headers: { 'Content-Type': 'application/json' }
});
fb2k.on('http:response', (data) => {
    if (data.requestId === requestId) {
        console.log('PUT 完成:', data.status);
    }
});
```

### http.delete 

发送 HTTP DELETE 请求。默认为异步模式。

参数：见"共享请求选项"，外加 `body`（可选）。

**同步模式返回值**（`async: false`）：

```json
{
    "success": true,
    "status": 200,
    "headers": { "Content-Type": "application/json" },
    "body": "...",
    "responseType": "text"
}
```

**异步模式返回值**（`async: true`，默认）：

```json
{
    "success": true,
    "requestId": "http_2",
    "async": true
}
```

异步结果通过 `http:response` 事件推送。

**示例**：

```javascript
// 同步 DELETE
const result = await fb2k.invoke('http.delete', {
    url: 'https://api.example.com/resource/1',
    async: false
});

// 异步 DELETE
const { requestId } = await fb2k.invoke('http.delete', {
    url: 'https://api.example.com/resource/1'
});
fb2k.on('http:response', (data) => {
    if (data.requestId === requestId) {
        console.log('DELETE 完成:', data.status);
    }
});
```

### http.patch 

发送 HTTP PATCH 请求。默认为异步模式。

参数：见"共享请求选项"，外加 `body`（同 `http.post`）。

**同步模式返回值**（`async: false`）：

```json
{
    "success": true,
    "status": 200,
    "headers": { "Content-Type": "application/json" },
    "body": "...",
    "responseType": "text"
}
```

**异步模式返回值**（`async: true`，默认）：

```json
{
    "success": true,
    "requestId": "http_3",
    "async": true
}
```

异步结果通过 `http:response` 事件推送。

**示例**：

```javascript
// 同步 PATCH
const result = await fb2k.invoke('http.patch', {
    url: 'https://api.example.com/resource/1',
    body: JSON.stringify({ status: 'updated' }),
    headers: { 'Content-Type': 'application/json' },
    async: false
});

// 异步 PATCH
const { requestId } = await fb2k.invoke('http.patch', {
    url: 'https://api.example.com/resource/1',
    body: JSON.stringify({ status: 'updated' }),
    headers: { 'Content-Type': 'application/json' }
});
fb2k.on('http:response', (data) => {
    if (data.requestId === requestId) {
        console.log('PATCH 完成:', data.status);
    }
});
```

### http.abort 

中止一个正在进行的异步 HTTP 请求。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| requestId | string | ✓ | 异步请求返回的请求 ID |

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

```javascript
try {
    const result = await fb2k.invoke('http.get', {
        url: 'https://api.example.com/data'
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
