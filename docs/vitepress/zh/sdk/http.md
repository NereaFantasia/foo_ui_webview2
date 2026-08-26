# fb.http HTTP 请求

本页是 `fb.http` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### get(url, options?)

签名：`fb.http.get(url: string, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

派发 GET 请求。宿主默认异步执行请求，因此当前响应通常是 `{ success, requestId, async: true }`，最终响应通过 `http:response` 发出。传入 `{ async: false }` 可直接取得完整响应，也可以使用 `request()` 等待事件驱动结果。

```javascript
const response = await fb.http.get('https://api.example.com/data', { async: false });
```

### post(url, body?, options?)

签名：`fb.http.post(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

派发 POST 请求；宿主会序列化非字符串 JSON 请求体。

### head(url, options?)

签名：`fb.http.head(url: string, options?: HttpRequestOptions): Promise<HttpResponse>`

派发 HEAD 请求。同步响应可能包含解析后的便利字段 `contentLength`。

### download(url, saveTo, options?)

签名：`fb.http.download(url: string, saveTo: string, options?: HttpDownloadOptions): Promise<BaseResponse & { requestId?: string; path?: string; bytesWritten?: number; cancelled?: boolean }>`

将 URL 下载到本地路径。默认以 60 秒宿主超时同步执行；传入 `{ async: true }` 时，完成结果通过 `http:downloadComplete` 发出，并用 `requestId` 关联。

```javascript
const receipt = await fb.http.download(
	'https://example.com/cover.jpg',
	'C:\\Covers\\cover.jpg',
	{ async: true }
);
```

### abort()

封装 `http.abort`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.http.abort(/* 参数见 TypeScript 声明 */);
```

### delete()

封装 `http.delete`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.http.delete(/* 参数见 TypeScript 声明 */);
```

### patch()

封装 `http.patch`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.http.patch(/* 参数见 TypeScript 声明 */);
```

### put()

封装 `http.put`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.http.put(/* 参数见 TypeScript 声明 */);
```

### request()

封装 `http.get`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.http.request(/* 参数见 TypeScript 声明 */);
```

### disableDefaultDownloadLogger()

签名：`fb.http.disableDefaultDownloadLogger(): void`

移除模块级默认日志器；该日志器会通过 `console.warn` 报告未取消的 `http:downloadComplete` 失败。此操作可重复调用。

## 事件

- `http:response` 携带 `{ requestId, success, status, body, headers, error?, responseType? }`。
- `http:downloadComplete` 携带 `{ requestId, success, status?, bytesWritten?, path?, error?, cancelled? }`。

<!-- END AUTO-GENERATED SDK STUBS -->
