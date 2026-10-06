# Http API

Methods of the `http` namespace.

## Custom User-Agent

Set `headers: { 'User-Agent': 'MyTheme/1.0' }` to identify your theme. This works for GET, POST, PUT, DELETE, PATCH, HEAD and downloads. Without an override, the host sends `foo_ui_webview2/1.0 (WinHTTP)`.

Custom headers apply only to the initial request. After a redirect, they are omitted and `User-Agent` returns to the host default. Use the final URL directly when a service requires your User-Agent on every request.

## http

### http.abort

<!-- api-schema:begin http.abort -->
Cancel an async request or download. The request then ends with its event reporting `CANCELLED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `requestId` | `string` | Yes | Id from the receipt of the request or download. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | The id, as given. |
| `cancelled` | `boolean` | `true` when the request was still running and has been told to stop; `false` when it had already ended or never existed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('http.get', { url: 'https://example.com/large' });
if (res.success === false) throw new Error(res.error);
const { requestId } = res;
await fb2k.invoke('http.abort', { requestId });
```

### http.delete

<!-- api-schema:begin http.delete -->
Send a DELETE request; otherwise the same as `http.get`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | Request URL; only `http` and `https` are allowed. Must not be empty. |
| `body` | `any` | No | Request body: a string is sent as it is, an object or array as its JSON text, and any other value as an empty body. |
| `headers` | `Record<string, string>` | No | Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. |
| `timeout` | `integer` | No | Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults. Default: `30000`. |
| `async` | `boolean` | No | Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in. Default: `true`. |
| `redirect` | `"follow" \| "error" \| "manual"` | No | `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is. Default: `"follow"`. |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | No | `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded. Default: `"text"`. |
| `insecureTls` | `boolean` | No | Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id of the request; present in an async receipt. `http:response` and `http.abort` use it. |
| `async` | `boolean` | `true` in an async receipt. |
| `status` | `integer` | HTTP status code; present in a synchronous response. |
| `headers` | `Record<string, string>` | Response headers by name; of repeated headers the last one wins. Present in a synchronous response. |
| `body` | `string` | Response body, as text or Base64 according to `responseType`; present in a synchronous response. |
| `responseType` | `"text" \| "base64"` | `text` or `base64`, the encoding of `body`; present in a synchronous response. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('http.delete', { url: 'https://example.com/api/items/1' });
```

### http.download

<!-- api-schema:begin http.download -->
Download a URL into a file, creating the missing folders. By default the call blocks until the file is written; with `async: true` it returns a receipt at once and the outcome arrives as `http:downloadComplete`. Downloads over 500 MB fail with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | URL to download; only `http` and `https` are allowed. Must not be empty. |
| `saveTo` | `string` | Yes | File to write; path variables are expanded and missing folders are created. An existing file is replaced. Must not be empty. |
| `headers` | `Record<string, string>` | No | Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. |
| `timeout` | `integer` | No | Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults. Default: `60000`. |
| `async` | `boolean` | No | Return a receipt at once and deliver the outcome as `http:downloadComplete`; `false` blocks until the file is written. Default: `false`. |
| `redirect` | `"follow" \| "error" \| "manual"` | No | `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` saves the redirect response as it is. Default: `"follow"`. |
| `insecureTls` | `boolean` | No | Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id of the download; present in an async receipt. `http:downloadComplete` and `http.abort` use it. |
| `async` | `boolean` | `true` in an async receipt. |
| `message` | `string` | `Download started` in an async receipt. |
| `status` | `integer` | HTTP status code; present when the download finished synchronously. |
| `bytesWritten` | `integer` | Bytes written; present when the download finished synchronously. |
| `path` | `string` | The file written, with path variables expanded; present when the download finished synchronously. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Unlike the other `http` methods, `async` defaults to `false` here, so the call resolves only once the file is written.

```js
const result = await fb2k.invoke('http.download', {
    url: 'https://example.com/cover.jpg',
    saveTo: 'C:\\Temp\\cover.jpg',
});
```

### http.get

<!-- api-schema:begin http.get -->
Send a GET request. By default the call returns a receipt at once and the response arrives as `http:response` carrying the same `requestId`; with `async: false` the host blocks until the response is in and returns it. Requests to a local or private network address fail with `PERMISSION_DENIED` unless the host setting allows them, checked again at every redirect.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | Request URL; only `http` and `https` are allowed. Must not be empty. |
| `headers` | `Record<string, string>` | No | Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. |
| `timeout` | `integer` | No | Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults. Default: `30000`. |
| `async` | `boolean` | No | Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in. Default: `true`. |
| `redirect` | `"follow" \| "error" \| "manual"` | No | `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is. Default: `"follow"`. |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | No | `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded. Default: `"text"`. |
| `insecureTls` | `boolean` | No | Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id of the request; present in an async receipt. `http:response` and `http.abort` use it. |
| `async` | `boolean` | `true` in an async receipt. |
| `status` | `integer` | HTTP status code; present in a synchronous response. |
| `headers` | `Record<string, string>` | Response headers by name; of repeated headers the last one wins. Present in a synchronous response. |
| `body` | `string` | Response body, as text or Base64 according to `responseType`; present in a synchronous response. |
| `responseType` | `"text" \| "base64"` | `text` or `base64`, the encoding of `body`; present in a synchronous response. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// async is the default: the response arrives on the http:response event
const res2 = await fb2k.invoke('http.get', { url: 'https://example.com/api' });
if (res2.success === false) throw new Error(res2.error);
const { requestId } = res2;

// pass async: false to receive status, headers, and body directly
const res = await fb2k.invoke('http.get', { url: 'https://example.com/api', async: false });
```

### http.head

<!-- api-schema:begin http.head -->
Send a HEAD request; otherwise the same as `http.get`. The response body is always empty text, and `contentLength` is added when the server sends a numeric `Content-Length`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | Request URL; only `http` and `https` are allowed. Must not be empty. |
| `headers` | `Record<string, string>` | No | Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. |
| `timeout` | `integer` | No | Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults. Default: `30000`. |
| `async` | `boolean` | No | Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in. Default: `true`. |
| `redirect` | `"follow" \| "error" \| "manual"` | No | `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is. Default: `"follow"`. |
| `insecureTls` | `boolean` | No | Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id of the request; present in an async receipt. `http:response` and `http.abort` use it. |
| `async` | `boolean` | `true` in an async receipt. |
| `status` | `integer` | HTTP status code; present in a synchronous response. |
| `headers` | `Record<string, string>` | Response headers by name; of repeated headers the last one wins. Present in a synchronous response. |
| `body` | `string` | Always empty; present in a synchronous response. |
| `responseType` | `"text" \| "base64"` | Always `text`; present in a synchronous response. |
| `contentLength` | `integer` | The `Content-Length` header as a number; present in a synchronous response when the server sends one that parses. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`contentLength` is present only on a synchronous call whose response carried that header.

```js
const res = await fb2k.invoke('http.head', { url: 'https://example.com/file.zip', async: false });
```

### http.patch

<!-- api-schema:begin http.patch -->
Send a PATCH request; otherwise the same as `http.get`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | Request URL; only `http` and `https` are allowed. Must not be empty. |
| `body` | `any` | No | Request body: a string is sent as it is, an object or array as its JSON text, and any other value as an empty body. |
| `headers` | `Record<string, string>` | No | Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. |
| `timeout` | `integer` | No | Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults. Default: `30000`. |
| `async` | `boolean` | No | Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in. Default: `true`. |
| `redirect` | `"follow" \| "error" \| "manual"` | No | `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is. Default: `"follow"`. |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | No | `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded. Default: `"text"`. |
| `insecureTls` | `boolean` | No | Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id of the request; present in an async receipt. `http:response` and `http.abort` use it. |
| `async` | `boolean` | `true` in an async receipt. |
| `status` | `integer` | HTTP status code; present in a synchronous response. |
| `headers` | `Record<string, string>` | Response headers by name; of repeated headers the last one wins. Present in a synchronous response. |
| `body` | `string` | Response body, as text or Base64 according to `responseType`; present in a synchronous response. |
| `responseType` | `"text" \| "base64"` | `text` or `base64`, the encoding of `body`; present in a synchronous response. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('http.patch', {
    url: 'https://example.com/api/items/1',
    body: { rating: 5 },
    headers: { 'Content-Type': 'application/json' },
});
```

### http.post

<!-- api-schema:begin http.post -->
Send a POST request; otherwise the same as `http.get`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | Request URL; only `http` and `https` are allowed. Must not be empty. |
| `body` | `any` | No | Request body: a string is sent as it is, an object or array as its JSON text, and any other value as an empty body. |
| `headers` | `Record<string, string>` | No | Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. |
| `timeout` | `integer` | No | Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults. Default: `30000`. |
| `async` | `boolean` | No | Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in. Default: `true`. |
| `redirect` | `"follow" \| "error" \| "manual"` | No | `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is. Default: `"follow"`. |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | No | `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded. Default: `"text"`. |
| `insecureTls` | `boolean` | No | Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id of the request; present in an async receipt. `http:response` and `http.abort` use it. |
| `async` | `boolean` | `true` in an async receipt. |
| `status` | `integer` | HTTP status code; present in a synchronous response. |
| `headers` | `Record<string, string>` | Response headers by name; of repeated headers the last one wins. Present in a synchronous response. |
| `body` | `string` | Response body, as text or Base64 according to `responseType`; present in a synchronous response. |
| `responseType` | `"text" \| "base64"` | `text` or `base64`, the encoding of `body`; present in a synchronous response. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('http.post', {
    url: 'https://example.com/api/items',
    body: { title: 'New item' },
    headers: { 'Content-Type': 'application/json' },
});
```

### http.put

<!-- api-schema:begin http.put -->
Send a PUT request; otherwise the same as `http.get`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | Yes | Request URL; only `http` and `https` are allowed. Must not be empty. |
| `body` | `any` | No | Request body: a string is sent as it is, an object or array as its JSON text, and any other value as an empty body. |
| `headers` | `Record<string, string>` | No | Custom request headers. `User-Agent` overrides the host default on the initial request. A header whose name or value contains a line break is dropped. Custom headers are omitted after a redirect, including `User-Agent`, so redirected requests use the host default. |
| `timeout` | `integer` | No | Timeout in milliseconds for resolving, connecting, sending and receiving each; `0` or less keeps the WinHTTP defaults. Default: `30000`. |
| `async` | `boolean` | No | Return a receipt at once and deliver the response as `http:response`; `false` blocks until the response is in. Default: `true`. |
| `redirect` | `"follow" \| "error" \| "manual"` | No | `follow` follows up to 10 redirects; `error` fails with `OPERATION_FAILED` on a redirect; `manual` returns the redirect response as it is. Default: `"follow"`. |
| `responseType` | `"text" \| "base64" \| "arraybuffer" \| "binary"` | No | `text` returns the body as text; `base64`, and its spellings `arraybuffer` and `binary`, return it Base64-encoded. Default: `"text"`. |
| `insecureTls` | `boolean` | No | Accept invalid or self-signed certificates; takes effect only when the host setting allows insecure TLS as well. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestId` | `string` | Id of the request; present in an async receipt. `http:response` and `http.abort` use it. |
| `async` | `boolean` | `true` in an async receipt. |
| `status` | `integer` | HTTP status code; present in a synchronous response. |
| `headers` | `Record<string, string>` | Response headers by name; of repeated headers the last one wins. Present in a synchronous response. |
| `body` | `string` | Response body, as text or Base64 according to `responseType`; present in a synchronous response. |
| `responseType` | `"text" \| "base64"` | `text` or `base64`, the encoding of `body`; present in a synchronous response. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('http.put', {
    url: 'https://example.com/api/items/1',
    body: { title: 'Updated' },
    headers: { 'Content-Type': 'application/json' },
});
```

## Request lifecycle and security

- `http.get`, `http.post`, `http.put`, `http.delete`, `http.patch`, and `http.head` default to asynchronous execution. Their immediate `success: true` response means only that dispatch succeeded and contains `requestId`; it is not the HTTP result. Final results are sent to the invoking window as `http:response` and must be correlated by `requestId`.
- The SDK `fb.http.request()` helper waits for the matching completion event; use it when application code needs an awaited final result without manually subscribing to `http:response`.
- The SDK convenience call `fb.http.get(` is a facade over the same invoke contract and may decode binary response bodies for its caller.
- For synchronous execution, pass `async: false`. Successful non-download responses include `status`, `headers`, `body`, and `responseType`. A successful synchronous or asynchronous HEAD response may additionally include numeric `contentLength` when the response exposes `Content-Length`.
- `http.download` defaults to synchronous execution. With `async: true`, its final result is emitted as `http:downloadComplete` with `requestId`; `http.abort` requests cancellation of an active asynchronous request.
- Only `http` and `https` URLs are accepted. Private or local-network destinations are denied unless the host's Advanced Settings permits them; redirects are checked again at every hop and the redirect limit is 10.
- Failures carry `code`, in the reply and in the `http:response` / `http:downloadComplete` events alike: `PERMISSION_DENIED` for a denied local or private destination, `INVALID_PARAMS` for a URL that does not parse or whose scheme is not `http` or `https`, `CANCELLED` (with `cancelled: true`) after `http.abort`, and `OPERATION_FAILED` for network errors, `redirect: "error"` meeting a redirect (with `status`), and the size limits.
- `insecureTls: true` takes effect only when the caller opts in **and** the host's invalid-certificate setting is enabled. This bypass is unsuitable for public Internet traffic.
- Response bodies are limited to 100 MB and downloads to 500 MB. `http.download.saveTo` is a write-protected path parameter and is subject to the Bridge security policy.
