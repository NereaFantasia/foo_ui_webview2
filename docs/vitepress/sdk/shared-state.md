# fb.sharedState Cross-window Shared State

`fb.sharedState` exposes a cross-window key/value store. It is distinct from the synchronous `fb.state` playback mirror: shared-state values can be assigned a TTL and changes are published through the `state:*` event family.

The store is process-local memory owned by the component's `PortHub` singleton.
It is shared by this component's WebView windows in the current foobar2000
process, but it is not persisted to disk, does not survive restart, and is not
shared with other processes or SMP runtimes.

## delete(key)

Signature: `fb.sharedState.delete(key: string): Promise<StateDeleteResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Logical key to remove. |

The response's `existed` tells whether the key existed before the call. Removing a key that existed emits `state:deleted` with `reason: 'deleted'`.

```javascript
const result = await fb.sharedState.delete('playlist:active-filter');
```

## keys(pattern?)

Signature: `fb.sharedState.keys(pattern?: string): Promise<StateKeysResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `pattern` | `string` | No | Key pattern; defaults to `'*'`. |

Resolves with the matching keys as `{ keys }`, in no particular order. `'*'` matches every key; a pattern ending in `*` matches by prefix, such as `'playlist:*'`; any other pattern matches one whole key, a `*` elsewhere being an ordinary character. Expired keys are swept first, and each one broadcasts `state:deleted` with `reason: 'expired'`.

```javascript
const res = await fb.sharedState.keys('playlist:*');
if (res.success === false) throw new Error(res.error);
const { keys } = res;
```

## set(key, value, silent?, ttlMs?)

Signature: `fb.sharedState.set(key: string, value: JsonValue, silent?: boolean, ttlMs?: number): Promise<StateSetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Logical key to update. |
| `value` | `unknown` | Yes | JSON-serializable value. A top-level `null` is refused; remove a key with `delete()`. |
| `silent` | `boolean` | No | Suppresses the change event when `true`; defaults to `false`. |
| `ttlMs` | `number` | No | Time to live in milliseconds; omitted, `0` or less, the value does not expire. |

Broadcasts `state:changed` afterwards unless `silent` is `true`, with `key`, `value`, `previousValue` (`null` for a new key) and `sourceWindowId`. With a positive `ttlMs`, the result and the event both carry `expiresAt`, in milliseconds since the Unix epoch.

```javascript
await fb.sharedState.set('playlist:active-filter', 'favorites', false, 60_000);
```

## get()

`fb.sharedState.get(key: string): Promise<StateGetResponse>` reads one value.

```javascript
const res = await fb.sharedState.get('playlist:active-filter');
if (res.success === false) throw new Error(res.error);
const { value } = res;
```

## Events

- `fb.sharedState.onChange(handler)` subscribes to `state:changed` and returns an unsubscribe function. The typed payload is `StateChangedPayload<T>` with `key`, `value`, `previousValue`, `sourceWindowId`, and optional `expiresAt` fields.
- `fb.sharedState.onDelete(handler)` subscribes to `state:deleted` and returns an unsubscribe function. `StateDeletedPayload.reason` is either `'deleted'` or `'expired'`.

```javascript
const off = fb.sharedState.onChange(({ key, value }) => {
	console.log(key, value);
});

off();
```
