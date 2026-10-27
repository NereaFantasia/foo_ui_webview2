# Test API

The `test` namespace provides round-trip diagnostic endpoints for verification and liveness checks.

## test

### test.echo

Echoes the request back, for round-trip diagnostics.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `message` | `json` | No | Any JSON value, returned in `echo`; when omitted, the entire params object is echoed instead |

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

- Endpoints in `test.*` provide diagnostic capabilities rather than theme application logic.
