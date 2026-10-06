# State API

Methods of the `state` namespace.

## state

### state.delete

<!-- api-schema:begin state.delete -->
Remove a value from the shared state; when it existed, announce it with `state:deleted` and reason `deleted`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | The key. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `existed` | `boolean` | Whether the key existed before the call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('state.delete', { key: 'lyrics:offset' });
```

### state.get

<!-- api-schema:begin state.get -->
Read a value from the state shared by all windows. Expired values are removed first, each announced with `state:deleted` and reason `expired`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | The key. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `exists` | `boolean` | Whether the key exists. |
| `value` | `any` | The value; `null` when the key does not exist. |
| `key` | `string` | The key; present when it exists. |
| `expiresAt` | `integer` | When the value expires, in milliseconds since the Unix epoch; present for a value stored with `ttlMs`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('state.get', { key: 'lyrics:offset' });
if (res.success === false) throw new Error(res.error);
const { value, exists } = res;
```

### state.keys

<!-- api-schema:begin state.keys -->
List the keys of the shared state. Expired values are removed first, as in `state.get`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `pattern` | `string` | No | `*` for every key, a prefix followed by `*` such as `lyrics:*`, or an exact key. Default: `"*"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `keys` | `string[]` | The matching keys, in no particular order. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('state.keys', { pattern: 'lyrics:*' });
if (res.success === false) throw new Error(res.error);
const { keys } = res;
```

### state.set

<!-- api-schema:begin state.set -->
Store a value in the shared state and announce it with `state:changed`, unless `silent`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | The key. Must not be empty. |
| `value` | `any` | Yes | The value to store. `null` counts as missing; remove a key with `state.delete`. |
| `silent` | `boolean` | No | Store without announcing `state:changed`. Default: `false`. |
| `ttlMs` | `integer` | No | Lifetime in milliseconds, after which the value is removed; `0` or less stores it without expiry. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `expiresAt` | `integer` | When the value expires, in milliseconds since the Unix epoch; present when `ttlMs` is positive. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('state.set', { key: 'lyrics:offset', value: 120 });
```

## State behavior

- State keys are opaque strings; `lyrics:offset` and `lyrics:theme` are ordinary application key examples, not reserved runtime state names.
- `state.*` is an in-memory store owned by the process-wide `PortHub` singleton. It is shared across this component's WebView windows in the current foobar2000 process, but it is not written to disk, does not survive process restart, and is not a cross-process or SMP/global persistence mechanism. It is distinct from the SDK `fb.state` playback-state mirror.
- `state.get` returns `exists: false` and `value: null` when a key is absent. `state.set` requires both `key` and `value`; positive `ttlMs` creates an expiration timestamp, and `silent: true` suppresses `state:changed`.
- `state.delete` returns `existed`. Explicit deletion emits `state:deleted` with `reason: "deleted"`; expiration emits the same event with `reason: "expired"` and an empty `sourceWindowId`.
- The full list of public PortHub events is under [Routing and PortHub events](./port.md#routing-and-porthub-events).
