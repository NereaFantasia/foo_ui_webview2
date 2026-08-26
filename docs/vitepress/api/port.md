# Port API

English API reference for the `event`, `port`, `state` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## event

### event.emit


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `event` | `string` | Yes | — | Event name to broadcast; a missing or empty value returns `INVALID_PARAMS`. |
| `payload` | `object` | No | `{}` |  |
| `excludeSelf` | `boolean` | No | `false` | Skips the calling window. |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
await fb2k.invoke('event.emit', { event: 'ui:themeChanged', payload: { theme: 'dark' } });
```

### event.emitTo


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `event` | `string` | Yes | — | Event name to deliver; a missing or empty value returns `INVALID_PARAMS`. |
| `targetWindowId` | `string` | Yes | — | Receiving window id; a missing or empty value returns `INVALID_PARAMS`. |
| `payload` | `object` | No | `{}` |  |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
await fb2k.invoke('event.emitTo', { event: 'lyrics:update', targetWindowId: 'popup_01', payload: { line: 5 } });
```

## port

### port.connect


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | `string` | Yes | Channel name to bind; a missing or empty value returns `INVALID_PARAMS`. |

**Returns**: `{"code":"...","error":"..."}`

```js
const { portId } = await fb2k.invoke('port.connect', { name: 'lyrics' });
```

### port.disconnect


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `portId` | `string` | Yes | Port to destroy; a missing or empty value returns `INVALID_PARAMS`. |

**Returns**: `{"code":"...","error":"..."}`

```js
await fb2k.invoke('port.disconnect', { portId: 'port_00000001' });
```

### port.getPorts


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `name` | `string` | No | omitted | Optional channel-name filter; omit to list all ports. |

**Returns**: `{"success":true,"ports":[{"portId":"...","name":"...","windowId":"..."}]}`

```js
const { ports } = await fb2k.invoke('port.getPorts', { name: 'lyrics' });
```

### port.postMessage


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | Yes | Message body. |
| `portId` | `string` | Yes | Sending port; a missing or empty value returns `INVALID_PARAMS`. |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
await fb2k.invoke('port.postMessage', { portId: 'port_00000001', message: { text: 'hello' } });
```

### port.postMessageTo


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | Yes | Message body. |
| `portId` | `string` | Yes | Sending port; a missing or empty value returns `INVALID_PARAMS`. |
| `targetPortId` | `string` | Yes | Receiving port; a missing or empty value returns `INVALID_PARAMS`. |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
await fb2k.invoke('port.postMessageTo', { portId: 'port_00000001', targetPortId: 'port_00000002', message: { text: 'sync' } });
```

## state

### state.delete


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | State key to delete; a missing or empty value returns `INVALID_PARAMS`. |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
await fb2k.invoke('state.delete', { key: 'lyrics:offset' });
```

### state.get


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | Yes | State key to read; a missing or empty value returns `INVALID_PARAMS`. |

**Returns**: `{"code":"...","error":"..."}`

```js
const { value, exists } = await fb2k.invoke('state.get', { key: 'lyrics:offset' });
```

### state.keys


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `pattern` | `string` | No | `*` | Glob-like filter; `*` matches all, trailing `*` is a prefix match. |

**Returns**: `{"success":true,"keys":["..."]}`

```js
const { keys } = await fb2k.invoke('state.keys', { pattern: 'lyrics:*' });
```

### state.set


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `key` | `string` | Yes | — | State key to write; a missing or empty value returns `INVALID_PARAMS`. |
| `value` | `json` | Yes | — | Any JSON value. |
| `ttlMs` | `integer` | No | — | Positive values create an expiration timestamp (ms). |
| `silent` | `boolean` | No | `false` | Suppresses `state:changed`. |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
await fb2k.invoke('state.set', { key: 'lyrics:offset', value: 120 });
```

## Routing, state, and event envelopes

- `port.connect` binds the new port to the invoking window. Only that owner may disconnect it or send through it; `port.postMessage` excludes the sending port and routes `port:message` to peer ports on the same name.
- `event.emit` broadcasts the requested event name and `event.emitTo` targets one window. Receivers get the envelope `{ payload, sourceWindowId }`; `excludeSelf` affects only `event.emit`. Use the `namespace:eventName` convention for application-defined event names, such as `ui:themeChanged` or `lyrics:update`.
- State keys are opaque strings; `lyrics:offset` and `lyrics:theme` are ordinary application key examples, not reserved runtime state names.
- `state.*` is an in-memory store owned by the process-wide `PortHub` singleton. It is shared across this component's WebView windows in the current foobar2000 process, but it is not written to disk, does not survive process restart, and is not a cross-process or SMP/global persistence mechanism. It is distinct from the SDK `fb.state` playback-state mirror.
- `state.get` returns `exists: false` and `value: null` when a key is absent. `state.set` requires both `key` and `value`; positive `ttlMs` creates an expiration timestamp, and `silent: true` suppresses `state:changed`.
- `state.delete` returns `existed`. Explicit deletion emits `state:deleted` with `reason: "deleted"`; expiration emits the same event with `reason: "expired"` and an empty `sourceWindowId`.
- Public PortHub events are `port:connected`, `port:disconnected`, `port:message`, `state:changed`, and `state:deleted`.
