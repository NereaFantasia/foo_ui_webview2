# fb.http HTTP Client

`fb.http` sends requests from the host process, so WebView2 CORS rules do not apply. Only `http` and `https` URLs are allowed, and local or private network addresses fail with `PERMISSION_DENIED` unless the host's Advanced Settings allow them.

## get(url, options?)

Signature: `fb.http.get(url: string, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a GET request. Host requests are asynchronous by default, so the immediate result is normally `{ success, requestId, async: true }`; the final response is emitted as `http:response`. Pass `{ async: false }` for a direct full response, or use `request()` to await the event-driven result.

```javascript
const response = await fb.http.get('https://api.example.com/data', { async: false });
```

## post(url, body?, options?)

Signature: `fb.http.post(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a POST request. Non-string JSON bodies are serialized by the host.

```javascript
const receipt = await fb.http.post('https://api.example.com/submit', { title: 'test' });
if (receipt.success === false) throw new Error(receipt.error);
```

## put(url, body?, options?)

Signature: `fb.http.put(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a PUT request using the same body and option semantics as `post()`.

```javascript
await fb.http.put('https://api.example.com/items/42', { title: 'Renamed' });
```

## delete(url, body?, options?)

Signature: `fb.http.delete(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a DELETE request. The optional body is the second positional argument.

```javascript
await fb.http.delete('https://api.example.com/items/42');
```

## patch(url, body?, options?)

Signature: `fb.http.patch(url: string, body?: JsonValue, options?: HttpRequestOptions): Promise<HttpResponse | HttpBinaryResponse>`

Dispatches a PATCH request using the same body and option semantics as `post()`.

```javascript
await fb.http.patch('https://api.example.com/items/42', { title: 'Renamed' });
```

## head(url, options?)

Signature: `fb.http.head(url: string, options?: Omit<HttpRequestOptions, 'responseType'>): Promise<HttpHeadResponse>`

Dispatches a HEAD request. Like `get()`, it is asynchronous by default: pass `{ async: false }` for a direct response; otherwise the result arrives via `http:response`. A synchronous response may include the parsed `contentLength` convenience field.

```javascript
const res = await fb.http.head('https://example.com/file.zip', { async: false });
if (res.success === false) throw new Error(res.error);
console.log(res.status, res.contentLength);
```

## request(url, options?)

Signature: `fb.http.request(url: string, options?: HttpRequestOptions): Promise<HttpGetSuccess | HttpBinarySuccess>`

Performs an event-driven GET and resolves once the matching `http:response` event arrives; a synchronous host response is handled as well. The client-side timeout defaults to 35 seconds, or `options.timeout + 5000` ms when `options.timeout` is specified. The SDK removes its event listener and clears the timeout on every completion path.

Unlike other methods, `request()` resolves only on a successful response and rejects on failure: a host-reported failure throws an `Error` with its `error` string as the message; a failure delivered via `http:response` attaches the full payload to the `response` property of the `Error` instance. Client watchdog timeout also triggers rejection.

```javascript
try {
  const response = await fb.http.request('https://api.example.com/data');
  console.log(response.status, response.body);
} catch (err) {
  console.error('Request failed:', err);
}
```

## download(url, saveTo, options?)

Signature: `fb.http.download(url: string, saveTo: string, options?: HttpDownloadOptions): Promise<HttpDownloadResponse>`

Downloads a URL to a local path. Download mode is synchronous by default with a 60-second host timeout. With `{ async: true }`, completion is emitted as `http:downloadComplete` and correlated by `requestId`.

```javascript
const receipt = await fb.http.download(
	'https://example.com/cover.jpg',
	'C:\\Covers\\cover.jpg',
	{ async: true }
);
```

## abort(requestId)

Signature: `fb.http.abort(requestId: string): Promise<HttpAbortResponse>`

Cancels an in-flight asynchronous request or download by its receipt `requestId`. If the request has already completed, `cancelled` is `false`.

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

Signature: `fb.http.disableDefaultDownloadLogger(): void`

Detaches the module-level logger that writes non-cancelled `http:downloadComplete` failures to `console.warn`. The operation is idempotent.

```javascript
fb.http.disableDefaultDownloadLogger();
fb.on('http:downloadComplete', (e) => {
	if (e.success === false && !e.cancelled) console.error('Download failed:', e.error);
});
```

## Request Options

| Field | Type | Description |
| --- | --- | --- |
| `headers` | `Record<string, string>` | Request headers |
| `timeout` | `number` | Host timeout in milliseconds; defaults to `30000` |
| `async` | `boolean` | Asynchronous dispatch when `true`; defaults to `true` for verbs |
| `redirect` | `'follow' \| 'error' \| 'manual'` | Redirect policy; defaults to `follow` |
| `responseType` | `'text' \| 'base64' \| 'arraybuffer' \| 'binary'` | Response decoding mode |
| `insecureTls` | `boolean` | Requests invalid-certificate bypass when the host advanced setting also permits it |

For `arraybuffer` or `binary`, the SDK decodes the host base64 transport payload into an `ArrayBuffer`. Avoid enabling `insecureTls` for requests carrying credentials or sensitive data. The deprecated `verifyTls` and `sync` options, as well as the `requestId` option of `download()`, have no effect and are omitted from host requests.

## Events

- `http:response` carries the receipt `requestId` and result: on success, it includes `status`, `headers`, `body`, and `responseType` (plus `contentLength` for HEAD requests); on failure, it includes `error` and `code` (plus `cancelled: true` when cancelled by `abort()`).
- `http:downloadComplete` carries the `requestId`: on success, it includes `status`, `bytesWritten`, and `path`; on failure, it includes `error` and `code` (plus `cancelled: true` when cancelled by `abort()`).
