# Test API

Methods of the `test` namespace.

## test

### test.echo

Echoes the request back, for round-trip diagnostics.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | No | Any JSON value, returned in `echo`. When omitted, the whole params object is echoed instead. |

**Returns**: `{"echo":"...","input":"...","success":true}`

```js
const { echo } = await fb2k.invoke('test.echo', { message: 'ping' });
```

### test.ping

Returns a liveness marker and the host's current Unix timestamp.

_No parameters._

**Returns**: `{"pong":"...","timestamp":"..."}`

```js
const result = await fb2k.invoke('test.ping');
```

## Owner-family behavior and limits

- `test.*` is diagnostic surface rather than application behavior.
