# fb.http HTTP Client

`fb.http` routes requests through the host process, allowing access to localhost, `file://` URLs, and origins that would otherwise be blocked by WebView2 CORS rules.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

### get(url, options?)

Signature: `fb.http.get(url: string, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a GET request. Host requests are asynchronous by default, so the immediate result is normally `{ success, requestId, async: true }`; the final response is emitted as `http:response`. Pass `{ async: false }` for a direct full response, or use `request()` to await the event-driven result.

```javascript
const response = await fb.http.get('https://api.example.com/data', { async: false });
```

### post(url, body?, options?)

Signature: `fb.http.post(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a POST request. Non-string JSON bodies are serialized by the host.

### put(url, body?, options?)

Signature: `fb.http.put(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a PUT request using the same body and option semantics as `post()`.

### delete(url, body?, options?)

Signature: `fb.http.delete(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a DELETE request. The optional body is the second positional argument.

### patch(url, body?, options?)

Signature: `fb.http.patch(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a PATCH request using the same body and option semantics as `post()`.

### head(url, options?)

Signature: `fb.http.head(url: string, options?: HttpRequestOptions): Promise<HttpResponse>`

Dispatches a HEAD request. A synchronous response may include the parsed `contentLength` convenience field.

### request(url, options?)

Signature: `fb.http.request(url: string, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Performs an event-driven GET and resolves only after the matching `http:response` event arrives. It also handles a synchronous host response. A client-side watchdog defaults to 35 seconds, or to `options.timeout + 5000` when a host timeout is supplied.

```javascript
const response = await fb.http.request('https://api.example.com/data');
console.log(response.status, response.body);
```

### download(url, saveTo, options?)

Signature: `fb.http.download(url: string, saveTo: string, options?: HttpDownloadOptions): Promise<BaseResponse & { requestId?: string; path?: string; bytesWritten?: number; cancelled?: boolean }>`

Downloads a URL to a local path. Download mode is synchronous by default with a 60-second host timeout. With `{ async: true }`, completion is emitted as `http:downloadComplete` and correlated by `requestId`.

```javascript
const receipt = await fb.http.download(
	'https://example.com/cover.jpg',
	'C:\\Covers\\cover.jpg',
	{ async: true }
);
```

### abort(requestId)

Signature: `fb.http.abort(requestId: string): Promise<BaseResponse & { cancelled?: boolean }>`

Cancels an in-flight request by correlation ID.

```javascript
if (receipt.requestId) {
	await fb.http.abort(receipt.requestId);
}
```

### disableDefaultDownloadLogger()

Signature: `fb.http.disableDefaultDownloadLogger(): void`

Detaches the module-level logger that writes non-cancelled `http:downloadComplete` failures to `console.warn`. The operation is idempotent.

## Request Options

| Field | Type | Description |
| --- | --- | --- |
| `headers` | `Record<string, string>` | Request headers |
| `timeout` | `number` | Host timeout in milliseconds; defaults to `30000` |
| `async` | `boolean` | Asynchronous dispatch when `true`; defaults to `true` for verbs |
| `redirect` | `string` | Redirect policy; the host default is `follow` |
| `responseType` | `'text' \| 'base64' \| 'arraybuffer' \| 'binary'` | Response decoding mode |
| `insecureTls` | `boolean` | Requests invalid-certificate bypass when the host advanced setting also permits it |

For `arraybuffer` or `binary`, the SDK decodes the host's base64 transport body into an `ArrayBuffer`. Do not enable `insecureTls` for requests carrying credentials or personal data.

## Events

- `http:response` carries `{ requestId, success, status, body, headers, error?, responseType? }`.
- `http:downloadComplete` carries `{ requestId, success, status?, bytesWritten?, path?, error?, cancelled? }`.

<!-- END AUTO-GENERATED SDK STUBS -->
