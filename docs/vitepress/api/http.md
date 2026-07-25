# Http API

English API reference for the `http` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## http

### http.abort

Public API method. Runtime authority: `src/api/HttpApi.cpp:1531`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `requestId` | `string` | No | Optional; default . |

**Returns**: `{"cancelled":"...","error":"...","requestId":"...","success":true}`

```js
const result = await fb2k.invoke('http.abort', { requestId: /* value */ });
```

### http.delete

Public API method. Runtime authority: `src/api/HttpApi.cpp:1525`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | No | Optional; default . |
| `body` | `json` | No | Optional; default omitted. |
| `headers` | `object` | No | Optional; default {}. |
| `timeout` | `integer` | No | Optional; default 30000. |
| `async` | `boolean` | No | Optional; default true. |
| `redirect` | `string` | No | Optional; default follow. |
| `responseType` | `string` | No | Optional; default text. |
| `insecureTls` | `boolean` | No | Optional; default false. |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
const result = await fb2k.invoke('http.delete', { async: /* value */, body: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, responseType: /* value */, timeout: /* value */, url: /* value */ });
```

### http.download

Public API method. Runtime authority: `src/api/HttpApi.cpp:1519`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `async` | `boolean` | No | Optional; default false. |
| `headers` | `object` | No | Optional; default omitted. |
| `insecureTls` | `boolean` | No | Optional; default false. |
| `redirect` | `string` | No | Optional; default follow. |
| `saveTo` | `string` | No | Optional; default . |
| `timeout` | `integer` | No | Optional; default 60000. |
| `url` | `string` | No | Optional; default . |

**Returns**: `{"async":"...","code":"...","error":"...","message":"...","requestId":"...","success":true}`

```js
const result = await fb2k.invoke('http.download', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, saveTo: /* value */, timeout: /* value */, url: /* value */ });
```

### http.get

Public API method. Runtime authority: `src/api/HttpApi.cpp:1510`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `async` | `boolean` | No | Optional; default true. |
| `headers` | `object` | No | Optional; default omitted. |
| `insecureTls` | `boolean` | No | Optional; default false. |
| `redirect` | `string` | No | Optional; default follow. |
| `responseType` | `string` | No | Optional; default text. |
| `timeout` | `integer` | No | Optional; default 30000. |
| `url` | `string` | No | Optional; default . |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
const result = await fb2k.invoke('http.get', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, responseType: /* value */, timeout: /* value */, url: /* value */ });
```

### http.head

Public API method. Runtime authority: `src/api/HttpApi.cpp:1516`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `async` | `boolean` | No | Optional; default true. |
| `headers` | `object` | No | Optional; default omitted. |
| `insecureTls` | `boolean` | No | Optional; default false. |
| `redirect` | `string` | No | Optional; default follow. |
| `timeout` | `integer` | No | Optional; default 30000. |
| `url` | `string` | No | Optional; default . |

**Returns**: `{"async":"...","contentLength":"...","requestId":"...","success":true}`

```js
const result = await fb2k.invoke('http.head', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, timeout: /* value */, url: /* value */ });
```

### http.patch

Public API method. Runtime authority: `src/api/HttpApi.cpp:1528`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | No | Optional; default . |
| `body` | `json` | No | Optional; default omitted. |
| `headers` | `object` | No | Optional; default {}. |
| `timeout` | `integer` | No | Optional; default 30000. |
| `async` | `boolean` | No | Optional; default true. |
| `redirect` | `string` | No | Optional; default follow. |
| `responseType` | `string` | No | Optional; default text. |
| `insecureTls` | `boolean` | No | Optional; default false. |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
const result = await fb2k.invoke('http.patch', { async: /* value */, body: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, responseType: /* value */, timeout: /* value */, url: /* value */ });
```

### http.post

Public API method. Runtime authority: `src/api/HttpApi.cpp:1513`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | No | Optional; default . |
| `body` | `json` | No | Optional; default omitted. |
| `headers` | `object` | No | Optional; default {}. |
| `timeout` | `integer` | No | Optional; default 30000. |
| `async` | `boolean` | No | Optional; default true. |
| `redirect` | `string` | No | Optional; default follow. |
| `responseType` | `string` | No | Optional; default text. |
| `insecureTls` | `boolean` | No | Optional; default false. |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
const result = await fb2k.invoke('http.post', { async: /* value */, body: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, responseType: /* value */, timeout: /* value */, url: /* value */ });
```

### http.put

Public API method. Runtime authority: `src/api/HttpApi.cpp:1522`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `url` | `string` | No | Optional; default . |
| `body` | `json` | No | Optional; default omitted. |
| `headers` | `object` | No | Optional; default {}. |
| `timeout` | `integer` | No | Optional; default 30000. |
| `async` | `boolean` | No | Optional; default true. |
| `redirect` | `string` | No | Optional; default follow. |
| `responseType` | `string` | No | Optional; default text. |
| `insecureTls` | `boolean` | No | Optional; default false. |

**Returns**: `{"async":"...","error":"...","requestId":"...","success":true}`

```js
const result = await fb2k.invoke('http.put', { async: /* value */, body: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, responseType: /* value */, timeout: /* value */, url: /* value */ });
```

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:http.delete -->
### Contract supplement: `http.delete`

Verified contract supplement. Runtime authority: `src/api/HttpApi.cpp:1396-1432`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | No | `` | Optional; default . |
| `body` | `json` | No | omitted | Optional; default omitted. |
| `headers` | `object` | No | `{}` | Optional; default {}. |
| `timeout` | `integer` | No | `30000` | Optional; default 30000. |
| `async` | `boolean` | No | `true` | Optional; default true. |
| `redirect` | `string` | No | `follow` | Optional; default follow. |
| `responseType` | `string` | No | `text` | Optional; default text. |
| `insecureTls` | `boolean` | No | `false` | Optional; default false. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `async` | `json` | No |
| `requestId` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('http.delete', { url: /* value */, body: /* value */, headers: /* value */, timeout: /* value */, async: /* value */, redirect: /* value */, responseType: /* value */, insecureTls: /* value */ });
```
<!-- phase3-supplement:http.download -->
### Contract supplement: `http.download`

Verified contract supplement. Runtime authority: `src/api/HttpApi.cpp:1237-1352`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `async` | `boolean` | No | `false` | Optional; default false. |
| `headers` | `object` | No | omitted | Optional; default omitted. |
| `insecureTls` | `boolean` | No | `false` | Optional; default false. |
| `redirect` | `string` | No | `follow` | Optional; default follow. |
| `saveTo` | `string` | No | `` | Optional; default . |
| `timeout` | `integer` | No | `60000` | Optional; default 60000. |
| `url` | `string` | No | `` | Optional; default . |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `code` | `string` | Yes |
| `async` | `json` | No |
| `message` | `json` | No |
| `requestId` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('http.download', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, saveTo: /* value */, timeout: /* value */, url: /* value */ });
```
<!-- phase3-supplement:http.get -->
### Contract supplement: `http.get`

Verified contract supplement. Runtime authority: `src/api/HttpApi.cpp:822-862`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `async` | `boolean` | No | `true` | Optional; default true. |
| `headers` | `object` | No | omitted | Optional; default omitted. |
| `insecureTls` | `boolean` | No | `false` | Optional; default false. |
| `redirect` | `string` | No | `follow` | Optional; default follow. |
| `responseType` | `string` | No | `text` | Optional; default text. |
| `timeout` | `integer` | No | `30000` | Optional; default 30000. |
| `url` | `string` | No | `` | Optional; default . |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `async` | `json` | No |
| `requestId` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('http.get', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, responseType: /* value */, timeout: /* value */, url: /* value */ });
```
<!-- phase3-supplement:http.head -->
### Contract supplement: `http.head`

Verified contract supplement. Runtime authority: `src/api/HttpApi.cpp:906-950`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `async` | `boolean` | No | `true` | Optional; default true. |
| `headers` | `object` | No | omitted | Optional; default omitted. |
| `insecureTls` | `boolean` | No | `false` | Optional; default false. |
| `redirect` | `string` | No | `follow` | Optional; default follow. |
| `timeout` | `integer` | No | `30000` | Optional; default 30000. |
| `url` | `string` | No | `` | Optional; default . |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `async` | `json` | No |
| `requestId` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('http.head', { async: /* value */, headers: /* value */, insecureTls: /* value */, redirect: /* value */, timeout: /* value */, url: /* value */ });
```
<!-- phase3-supplement:http.patch -->
### Contract supplement: `http.patch`

Verified contract supplement. Runtime authority: `src/api/HttpApi.cpp:1436-1472`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | No | `` | Optional; default . |
| `body` | `json` | No | omitted | Optional; default omitted. |
| `headers` | `object` | No | `{}` | Optional; default {}. |
| `timeout` | `integer` | No | `30000` | Optional; default 30000. |
| `async` | `boolean` | No | `true` | Optional; default true. |
| `redirect` | `string` | No | `follow` | Optional; default follow. |
| `responseType` | `string` | No | `text` | Optional; default text. |
| `insecureTls` | `boolean` | No | `false` | Optional; default false. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `async` | `json` | No |
| `requestId` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('http.patch', { url: /* value */, body: /* value */, headers: /* value */, timeout: /* value */, async: /* value */, redirect: /* value */, responseType: /* value */, insecureTls: /* value */ });
```
<!-- phase3-supplement:http.post -->
### Contract supplement: `http.post`

Verified contract supplement. Runtime authority: `src/api/HttpApi.cpp:866-902`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | No | `` | Optional; default . |
| `body` | `json` | No | omitted | Optional; default omitted. |
| `headers` | `object` | No | `{}` | Optional; default {}. |
| `timeout` | `integer` | No | `30000` | Optional; default 30000. |
| `async` | `boolean` | No | `true` | Optional; default true. |
| `redirect` | `string` | No | `follow` | Optional; default follow. |
| `responseType` | `string` | No | `text` | Optional; default text. |
| `insecureTls` | `boolean` | No | `false` | Optional; default false. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `async` | `json` | No |
| `requestId` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('http.post', { url: /* value */, body: /* value */, headers: /* value */, timeout: /* value */, async: /* value */, redirect: /* value */, responseType: /* value */, insecureTls: /* value */ });
```
<!-- phase3-supplement:http.put -->
### Contract supplement: `http.put`

Verified contract supplement. Runtime authority: `src/api/HttpApi.cpp:1356-1392`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `url` | `string` | No | `` | Optional; default . |
| `body` | `json` | No | omitted | Optional; default omitted. |
| `headers` | `object` | No | `{}` | Optional; default {}. |
| `timeout` | `integer` | No | `30000` | Optional; default 30000. |
| `async` | `boolean` | No | `true` | Optional; default true. |
| `redirect` | `string` | No | `follow` | Optional; default follow. |
| `responseType` | `string` | No | `text` | Optional; default text. |
| `insecureTls` | `boolean` | No | `false` | Optional; default false. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `async` | `json` | No |
| `requestId` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('http.put', { url: /* value */, body: /* value */, headers: /* value */, timeout: /* value */, async: /* value */, redirect: /* value */, responseType: /* value */, insecureTls: /* value */ });
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
