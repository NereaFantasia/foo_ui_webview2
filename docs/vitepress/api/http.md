# Http API

English API reference for the `http` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## http

### http.abort


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `requestId` | `string` | Yes | The `requestId` returned by an async request. |

**Returns**: `{"cancelled":"...","error":"...","requestId":"...","success":true}`

```js
const { requestId } = await fb2k.invoke('http.get', { url: 'https://example.com/large' });
await fb2k.invoke('http.abort', { requestId });
```

### http.delete


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | Yes | — | Request URL. |
| `body` | `json` | No | — | Objects and arrays are serialized automatically. |
| `headers` | `object` | No | `{}` |  |
| `timeout` | `integer` | No | `30000` |  |
| `async` | `boolean` | No | `true` |  |
| `redirect` | `string` | No | `follow` |  |
| `responseType` | `string` | No | `text` |  |
| `insecureTls` | `boolean` | No | `false` |  |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
await fb2k.invoke('http.delete', { url: 'https://example.com/api/items/1' });
```

### http.download


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | Yes | — | Source URL. |
| `saveTo` | `string` | Yes | — | Destination path; subject to the Bridge security policy. |
| `headers` | `object` | No | — |  |
| `timeout` | `integer` | No | `60000` |  |
| `async` | `boolean` | No | `false` | Unlike other verbs, defaults to synchronous. |
| `redirect` | `string` | No | `follow` |  |
| `insecureTls` | `boolean` | No | `false` |  |

**Returns**: `{"async":"...","code":"...","error":"...","message":"...","requestId":"...","success":true}`

Unlike the other `http` methods, `async` defaults to `false` here, so the call resolves only once the file is written.

```js
const result = await fb2k.invoke('http.download', {
    url: 'https://example.com/cover.jpg',
    saveTo: 'C:\\Temp\\cover.jpg',
});
```

### http.get


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | Yes | — | Request URL. |
| `headers` | `object` | No | — |  |
| `timeout` | `integer` | No | `30000` |  |
| `async` | `boolean` | No | `true` |  |
| `redirect` | `string` | No | `follow` |  |
| `responseType` | `string` | No | `text` |  |
| `insecureTls` | `boolean` | No | `false` |  |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
// async is the default: the response arrives on the http:response event
const { requestId } = await fb2k.invoke('http.get', { url: 'https://example.com/api' });

// pass async: false to receive status, headers, and body directly
const res = await fb2k.invoke('http.get', { url: 'https://example.com/api', async: false });
```

### http.head


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | Yes | — | Request URL. |
| `headers` | `object` | No | — |  |
| `timeout` | `integer` | No | `30000` |  |
| `async` | `boolean` | No | `true` |  |
| `redirect` | `string` | No | `follow` |  |
| `insecureTls` | `boolean` | No | `false` |  |

**Returns**: `{"async":"...","contentLength":"...","requestId":"...","success":true}`

`contentLength` is present only on a synchronous call whose response carried that header.

```js
const res = await fb2k.invoke('http.head', { url: 'https://example.com/file.zip', async: false });
```

### http.patch


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | Yes | — | Request URL. |
| `body` | `json` | No | — | Request body. |
| `headers` | `object` | No | `{}` |  |
| `timeout` | `integer` | No | `30000` |  |
| `async` | `boolean` | No | `true` |  |
| `redirect` | `string` | No | `follow` |  |
| `responseType` | `string` | No | `text` |  |
| `insecureTls` | `boolean` | No | `false` |  |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
await fb2k.invoke('http.patch', {
    url: 'https://example.com/api/items/1',
    body: { rating: 5 },
    headers: { 'Content-Type': 'application/json' },
});
```

### http.post


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | Yes | — | Request URL. |
| `body` | `json` | No | — | Request body. |
| `headers` | `object` | No | `{}` |  |
| `timeout` | `integer` | No | `30000` |  |
| `async` | `boolean` | No | `true` |  |
| `redirect` | `string` | No | `follow` |  |
| `responseType` | `string` | No | `text` |  |
| `insecureTls` | `boolean` | No | `false` |  |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
await fb2k.invoke('http.post', {
    url: 'https://example.com/api/items',
    body: { title: 'New item' },
    headers: { 'Content-Type': 'application/json' },
});
```

### http.put


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | Yes | — | Request URL. |
| `body` | `json` | No | — | Objects and arrays are serialized automatically. |
| `headers` | `object` | No | `{}` |  |
| `timeout` | `integer` | No | `30000` |  |
| `async` | `boolean` | No | `true` |  |
| `redirect` | `string` | No | `follow` |  |
| `responseType` | `string` | No | `text` |  |
| `insecureTls` | `boolean` | No | `false` |  |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

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
- `insecureTls: true` takes effect only when the caller opts in **and** the host's invalid-certificate setting is enabled. This bypass is unsuitable for public Internet traffic.
- Response bodies are limited to 100 MB and downloads to 500 MB. `http.download.saveTo` is a write-protected path parameter and is subject to the Bridge security policy.
