# fb.sharedState Cross-window Shared State

`fb.sharedState` exposes a cross-window key/value store. It is distinct from the synchronous `fb.state` playback mirror: shared-state values can be assigned a TTL and changes are published through the `state:*` event family.

The store is maintained in process memory by the component `PortHub` singleton. It is shared across all WebView windows managed by this component in the current foobar2000 process. Data resides only in memory, does not persist across restarts, and is not shared with other processes or SMP runtimes.

## delete(key)

Signature: `fb.sharedState.delete(key: string): Promise<StateDeleteResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Logical key to remove |

The response's `existed` property indicates whether the key existed prior to deletion; deleting an existing key emits `state:deleted` with `reason: 'deleted'`.

```javascript
const result = await fb.sharedState.delete('playlist:active-filter');
```

## keys(pattern?)

Signature: `fb.sharedState.keys(pattern?: string): Promise<StateKeysResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `pattern` | `string` | No | Key pattern; defaults to `'*'` |

Resolves with `{ keys }` containing matched keys in undefined order. `'*'` matches all keys; patterns ending with `*` perform prefix matching (such as `'playlist:*'`); all other patterns perform exact key matching, treating any middle `*` as a literal character. Expired keys are swept before evaluation, and each purged key emits `state:deleted` with `reason: 'expired'`.

```javascript
const res = await fb.sharedState.keys('playlist:*');
if (res.success === false) throw new Error(res.error);
const { keys } = res;
```

## set(key, value, silent?, ttlMs?)

Signature: `fb.sharedState.set(key: string, value: JsonValue, silent?: boolean, ttlMs?: number): Promise<StateSetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Logical key to update |
| `value` | `unknown` | Yes | JSON-serializable value. A top-level `null` is rejected; use `delete()` to remove a key |
| `silent` | `boolean` | No | Suppresses the change event when `true`; defaults to `false` |
| `ttlMs` | `number` | No | Time to live in milliseconds; when omitted or non-positive, the key does not expire |

Upon successful write, broadcasts `state:changed` (suppressed when `silent` is `true`); payload includes `key`, `value`, `previousValue` (`null` for newly created keys), and `sourceWindowId`. When positive `ttlMs` is specified, the method return value and the event payload both carry `expiresAt` (Unix epoch timestamp in milliseconds).

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
