# Port API

English API reference for the `event`, `port`, `state` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## event

### event.emit

Public API method. Runtime authority: `src/api/PortApi.cpp:121`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `event` | `string` | No | Optional; default . |
| `excludeSelf` | `boolean` | No | Optional; default false. |
| `payload` | `object` | No | Optional; default {}. |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('event.emit', { event: /* value */, excludeSelf: /* value */, payload: /* value */ });
```

### event.emitTo

Public API method. Runtime authority: `src/api/PortApi.cpp:133`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `event` | `string` | No | Optional; default . |
| `payload` | `object` | No | Optional; default {}. |
| `targetWindowId` | `string` | No | Optional; default . |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('event.emitTo', { event: /* value */, payload: /* value */, targetWindowId: /* value */ });
```

## port

### port.connect

Public API method. Runtime authority: `src/api/PortApi.cpp:58`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | `string` | No | Optional; default . |

**Returns**: `{"code":"...","error":"..."}`

```js
const result = await fb2k.invoke('port.connect', { name: /* value */ });
```

### port.disconnect

Public API method. Runtime authority: `src/api/PortApi.cpp:68`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `portId` | `string` | No | Optional; default . |

**Returns**: `{"code":"...","error":"..."}`

```js
const result = await fb2k.invoke('port.disconnect', { portId: /* value */ });
```

### port.getPorts

Public API method. Runtime authority: `src/api/PortApi.cpp:108-114` → `PortHub::GetPorts`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `name` | `string` | No | omitted | Optional channel-name filter; omit to list all ports. |

**Returns**: `{"success":true,"ports":[{"portId":"...","name":"...","windowId":"..."}]}`

```js
const result = await fb2k.invoke('port.getPorts', { name: /* value */ });
```

### port.postMessage

Public API method. Runtime authority: `src/api/PortApi.cpp:77`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | Yes | Required. |
| `portId` | `string` | No | Optional; default . |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('port.postMessage', { message: /* value */, portId: /* value */ });
```

### port.postMessageTo

Public API method. Runtime authority: `src/api/PortApi.cpp:92`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | Yes | Required. |
| `portId` | `string` | No | Optional; default . |
| `targetPortId` | `string` | No | Optional; default . |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('port.postMessageTo', { message: /* value */, portId: /* value */, targetPortId: /* value */ });
```

## state

### state.delete

Public API method. Runtime authority: `src/api/PortApi.cpp:178`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | No | Optional; default . |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('state.delete', { key: /* value */ });
```

### state.get

Public API method. Runtime authority: `src/api/PortApi.cpp:149`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | No | Optional; default . |

**Returns**: `{"code":"...","error":"..."}`

```js
const result = await fb2k.invoke('state.get', { key: /* value */ });
```

### state.keys

Public API method. Runtime authority: `src/api/PortApi.cpp:188-191` → `PortHub::GetStateKeys`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `pattern` | `string` | No | `*` | Glob-like filter; `*` matches all, trailing `*` is a prefix match. |

**Returns**: `{"success":true,"keys":["..."]}`

```js
const result = await fb2k.invoke('state.keys', { pattern: /* value */ });
```

### state.set

Public API method. Runtime authority: `src/api/PortApi.cpp:158`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `key` | `string` | No | Optional; default . |
| `silent` | `boolean` | No | Optional; default false. |
| `ttlMs` | `integer` | No | Optional; default omitted. |
| `value` | `json` | Yes | Required. |

**Returns**: `{"code":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('state.set', { key: /* value */, silent: /* value */, ttlMs: /* value */, value: /* value */ });
```

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:state.set -->
### Contract supplement: `state.set`

Verified contract supplement. Runtime authority: `src/api/PortApi.cpp:158-175`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `key` | `string` | No | `` | Optional; default . |
| `silent` | `boolean` | No | `false` | Optional; default false. |
| `ttlMs` | `integer` | No | omitted | Optional; default omitted. |
| `value` | `json` | Yes | none | Required. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `code` | `string` | Yes |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('state.set', { key: /* value */, silent: /* value */, ttlMs: /* value */, value: /* value */ });
```

## Routing, state, and event envelopes

- `port.connect` binds the new port to the invoking window. Only that owner may disconnect it or send through it; `port.postMessage` excludes the sending port and routes `port:message` to peer ports on the same name.
- `event.emit` broadcasts the requested event name and `event.emitTo` targets one window. Receivers get the envelope `{ payload, sourceWindowId }`; `excludeSelf` affects only `event.emit`. Use the `namespace:eventName` convention for application-defined event names, such as `ui:themeChanged` or `lyrics:update`.
- State keys are opaque strings; `lyrics:offset` and `lyrics:theme` are ordinary application key examples, not reserved runtime state names.
- `state.*` is an in-memory store owned by the process-wide `PortHub` singleton. It is shared across this component's WebView windows in the current foobar2000 process, but it is not written to disk, does not survive process restart, and is not a cross-process or SMP/global persistence mechanism. It is distinct from the SDK `fb.state` playback-state mirror.
- `state.get` returns `exists: false` and `value: null` when a key is absent. `state.set` requires both `key` and `value`; positive `ttlMs` creates an expiration timestamp, and `silent: true` suppresses `state:changed`.
- `state.delete` returns `existed`. Explicit deletion emits `state:deleted` with `reason: "deleted"`; expiration emits the same event with `reason: "expired"` and an empty `sourceWindowId`.
- Public PortHub events are `port:connected`, `port:disconnected`, `port:message`, `state:changed`, and `state:deleted`. Their payloads are emitted by `src/api/PortHub.cpp`.
