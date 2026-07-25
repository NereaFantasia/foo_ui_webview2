# fb.sharedState Cross-window Shared State

`fb.sharedState` exposes a cross-window key/value store. It is distinct from the synchronous `fb.state` playback mirror: shared-state values can be assigned a TTL and changes are published through the `state:*` event family.

The store is process-local memory owned by the component's `PortHub` singleton.
It is shared by this component's WebView windows in the current foobar2000
process, but it is not persisted to disk, does not survive restart, and is not
shared with other processes or SMP runtimes.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK Method Stubs

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### delete()

Signature: `fb.sharedState.delete(key: string): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Logical key to remove. |

Returns the `state.delete` response envelope. A successful explicit removal emits `state:deleted` with `reason: 'deleted'`.

```javascript
const result = await fb.sharedState.delete('playlist:active-filter');
```

### keys()

Signature: `fb.sharedState.keys(pattern = '*'): Promise<{ keys: string[] }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `pattern` | `string` | No | Key pattern; defaults to `'*'`. |

Returns the matching keys in `{ keys }`.

```javascript
const { keys } = await fb.sharedState.keys('playlist:*');
```

### set()

Signature: `fb.sharedState.set(key: string, value: unknown, silent = false, ttlMs?: number): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | Logical key to update. |
| `value` | `unknown` | Yes | JSON-serializable value. |
| `silent` | `boolean` | No | Suppresses the change event when `true`; defaults to `false`. |
| `ttlMs` | `number` | No | Time to live in milliseconds. |

Returns the `state.set` response envelope.

```javascript
await fb.sharedState.set('playlist:active-filter', 'favorites', false, 60_000);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Additional Methods

### get()

`fb.sharedState.get(key: string): Promise<{ value: unknown }>` reads one value.

```javascript
const { value } = await fb.sharedState.get('playlist:active-filter');
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
