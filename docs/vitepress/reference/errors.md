# Error reference

> Overview: the unified `ErrorEnvelope` failure contract shared by every API.

## Error structure

All Bridge failures follow a machine-readable envelope. Successful payloads remain endpoint-specific; failures always include a stable `code`.

### Synchronous API failures

`fb2k.invoke()` failures return:

```json
{
  "success": false,
  "error": "Human-readable error message",
  "code": "MACHINE_READABLE_CODE"
}
```

- `success` — always `false`
- `error` — human-readable message (`string`)
- `code` — machine-readable code (`string`, `UPPER_SNAKE_CASE`)

Some handlers also attach a `details` object:

```json
{
  "success": false,
  "error": "Playlist is locked",
  "code": "LOCKED",
  "details": { "playlist": 3, "isLocked": true }
}
```

### Asynchronous failure events

Background or async work may push a failure event payload:

```json
{
  "error": "Failed to open decoder",
  "code": "DECODER_FAILED",
  "taskId": "waveform_42",
  "path": "E:\\Music\\song.flac"
}
```

### Framework-level failures

Invalid requests or unknown methods are rejected by BridgeCore before a handler runs:

```json
{
  "error": "Method not found: foo.bar",
  "code": "METHOD_NOT_FOUND"
}
```

## Standard error codes (`ApiErrorCode`)

### Framework

| Code | Meaning |
| --- | --- |
| `INVALID_REQUEST` | Request is missing the method field |
| `METHOD_NOT_FOUND` | The method name is not registered |
| `INTERNAL_ERROR` | Handler threw an uncaught exception |

### Parameter errors

| Code | Meaning |
| --- | --- |
| `REQUIRED_PARAM` | A required parameter is missing |
| `INVALID_PARAMS` | A parameter value is invalid |
| `INVALID_INDEX` | An index is out of range |

### State / resource errors

| Code | Meaning |
| --- | --- |
| `NOT_FOUND` | Resource does not exist |
| `LOCKED` | Playlist or resource is locked |
| `NOT_SUPPORTED` | Operation is unsupported in the current mode |
| `LIBRARY_DISABLED` | Media library is disabled |
| `NO_ACTIVE_ITEM` | No active playlist or now-playing item |

### Operation failures

| Code | Meaning |
| --- | --- |
| `OPERATION_FAILED` | Generic operation failure |

### Security

| Code | Meaning |
| --- | --- |
| `PERMISSION_DENIED` | Path security policy rejected the request. A parameter that fails on shape or type instead of on path policy returns `INVALID_PARAMS` |

### Media / path

| Code | Meaning |
| --- | --- |
| `MISSING_PATH` | Path parameter was not provided |
| `INVALID_PATH` | Path is invalid or the file does not exist |
| `INVALID_HANDLE` | Metadb handle creation failed |
| `NO_INFO` | Technical file information is unavailable |
| `DECODER_FAILED` | Audio decoder open failed |
| `DECODE_FAILED` | Decoding failed |
| `UNKNOWN_ERROR` | Unknown error |
| `EXCEPTION` | Catch-all exception path |

## TypeScript types

```typescript
import type { ErrorEnvelope, FailureEventPayload, ApiErrorCode } from 'sdk/index.d.ts';
```

- `ErrorEnvelope` — minimum synchronous failure shape
- `FailureEventPayload` — minimum async failure event data
- `ApiErrorCode` — union of standard codes
- `BaseResponse` — shared optional `error` / `code` fields

## Handling examples

```javascript
const result = await fb2k.invoke('library.browseTree', { rootId: 'invalid' });
if (!result.success) {
  console.error(`Error [${result.code}]: ${result.error}`);
}

fb2k.on('audio:fullWaveformFailed', (event) => {
  console.error(`Waveform failed [${event.code}]: ${event.error}`);
  console.error(`taskId=${event.taskId} path=${event.path}`);
});

fb2k.on('http:response', (data) => {
  if (data.error) {
    console.error(`HTTP ${data.requestId} failed [${data.code}]: ${data.error}`);
  }
});
```

## Compatibility notes

- Existing `{ success: false, error: string }` responses remain valid.
- `code` is additive; consumers should treat it as optional when reading older payloads.
- Artwork endpoints keep their dedicated availability fields.
- Framework `SendError` responses include `code` while preserving `error`.
