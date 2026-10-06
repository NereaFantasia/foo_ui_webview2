# fb.http HTTP 请求

`fb.http` 由宿主进程发出请求，因此不受 WebView2 的 CORS 规则限制。只允许 `http` 和 `https` URL；请求本地或私有网络地址会以 `PERMISSION_DENIED` 失败，除非宿主的高级设置允许。

## get(url, options?)

签名：`fb.http.get(url: string, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

派发 GET 请求。宿主默认异步执行请求，因此当前响应通常是 `{ success, requestId, async: true }`，最终响应通过 `http:response` 发出。传入 `{ async: false }` 可直接取得完整响应，也可以使用 `request()` 等待事件驱动结果。

```javascript
const response = await fb.http.get('https://api.example.com/data', { async: false });
```

## post(url, body?, options?)

签名：`fb.http.post(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

派发 POST 请求；宿主会序列化非字符串 JSON 请求体。

```javascript
const receipt = await fb.http.post('https://api.example.com/submit', { title: 'test' });
if (receipt.success === false) throw new Error(receipt.error);
```

## put(url, body?, options?)

签名：`fb.http.put(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

发出 PUT 请求，请求体与选项的语义同 `post()`。

```javascript
await fb.http.put('https://api.example.com/items/42', { title: '新标题' });
```

## delete(url, body?, options?)

签名：`fb.http.delete(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

发出 DELETE 请求。可选的请求体是第二个位置参数。

```javascript
await fb.http.delete('https://api.example.com/items/42');
```

## patch(url, body?, options?)

签名：`fb.http.patch(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

发出 PATCH 请求，请求体与选项的语义同 `post()`。

```javascript
await fb.http.patch('https://api.example.com/items/42', { title: '新标题' });
```

## head(url, options?)

签名：`fb.http.head(url: string, options?: Omit<HttpRequestOptions, 'responseType'>): Promise<HttpHeadResponse>`

派发 HEAD 请求。和 `get()` 一样默认异步：要直接拿到响应就传 `{ async: false }`，否则结果经 `http:response` 到达。同步响应可能包含解析后的便利字段 `contentLength`。

```javascript
const res = await fb.http.head('https://example.com/file.zip', { async: false });
if (res.success === false) throw new Error(res.error);
console.log(res.status, res.contentLength);
```

## request(url, options?)

签名：`fb.http.request(url: string, options?: HttpRequestOptions): Promise<HttpGetSuccess | HttpBinarySuccess>`

以事件驱动方式发出 GET，等到对应的 `http:response` 事件到达才 resolve；宿主同步返回时也能处理。客户端看门狗默认 35 秒，传了宿主超时则为 `options.timeout + 5000`。无论从哪条路径结束，SDK 都会移除自己的事件监听并清掉看门狗。

与其他方法不同，`request()` 只以成功的响应 resolve，失败一律 reject：宿主报告的失败变成以其 `error` 为消息的 `Error`，经 `http:response` 送达的失败另把整个载荷放在错误的 `response` 属性上；看门狗超时同样 reject。

```javascript
try {
  const response = await fb.http.request('https://api.example.com/data');
  console.log(response.status, response.body);
} catch (err) {
  console.error('请求失败：', err);
}
```

## download(url, saveTo, options?)

签名：`fb.http.download(url: string, saveTo: string, options?: HttpDownloadOptions): Promise<HttpDownloadResponse>`

将 URL 下载到本地路径。默认以 60 秒宿主超时同步执行；传入 `{ async: true }` 时，完成结果通过 `http:downloadComplete` 发出，并用 `requestId` 关联。

```javascript
const receipt = await fb.http.download(
	'https://example.com/cover.jpg',
	'C:\\Covers\\cover.jpg',
	{ async: true }
);
```

## abort(requestId)

签名：`fb.http.abort(requestId: string): Promise<HttpAbortResponse>`

按回执里的 `requestId` 取消进行中的异步请求或下载。请求已经结束时 `cancelled` 为 `false`。

```javascript
const receipt = await fb.http.download(
	'https://example.com/cover.jpg',
	'C:\\Covers\\cover.jpg',
	{ async: true }
);
if (receipt.success === false) throw new Error(receipt.error);
if (receipt.requestId) {
	await fb.http.abort(receipt.requestId);
}
```

## disableDefaultDownloadLogger()

签名：`fb.http.disableDefaultDownloadLogger(): void`

移除模块级默认日志器；该日志器会通过 `console.warn` 报告未取消的 `http:downloadComplete` 失败。此操作可重复调用。

```javascript
fb.http.disableDefaultDownloadLogger();
fb.on('http:downloadComplete', (e) => {
	if (e.success === false && !e.cancelled) console.error('下载失败：', e.error);
});
```

## 请求选项

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `headers` | `Record<string, string>` | 请求头 |
| `timeout` | `number` | 宿主超时，单位毫秒，默认 `30000` |
| `async` | `boolean` | 为 `true` 时异步派发；各请求方法默认为 `true` |
| `redirect` | `'follow' \| 'error' \| 'manual'` | 重定向策略，默认 `follow` |
| `responseType` | `'text' \| 'base64' \| 'arraybuffer' \| 'binary'` | 响应体的解码方式 |
| `insecureTls` | `boolean` | 请求跳过无效证书检查，宿主的高级设置也允许时才生效 |

`arraybuffer` 与 `binary` 由 SDK 把宿主以 base64 传回的响应体解码成 `ArrayBuffer`。携带凭据或个人数据的请求不要打开 `insecureTls`。已弃用的 `verifyTls`、`sync` 选项，以及 `download()` 的 `requestId` 选项都不起作用，也不会发给宿主。

## 事件

- `http:response` 携带回执里的 `requestId` 与结果：成功时有 `status`、`headers`、`body`、`responseType`（HEAD 另有 `contentLength`），失败时有 `error` 与 `code`（被 `abort()` 取消时另有 `cancelled: true`）。
- `http:downloadComplete` 携带 `requestId`：成功时有 `status`、`bytesWritten`、`path`，失败时有 `error` 与 `code`（被 `abort()` 取消时另有 `cancelled: true`）。
