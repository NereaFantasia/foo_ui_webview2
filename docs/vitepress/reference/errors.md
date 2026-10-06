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
| `REQUIRED_PARAM` | A required parameter is missing. Returned only by `audio.cancelFullWaveform`; every other endpoint reports a missing parameter as `INVALID_PARAMS` with the message `<name> is required` |
| `INVALID_PARAMS` | A parameter value is invalid |
| `INVALID_INDEX` | An index is out of range |

### State / resource errors

| Code | Meaning |
| --- | --- |
| `NOT_FOUND` | Resource does not exist |
| `LOCKED` | Playlist or resource is locked |
| `NOT_SUPPORTED` | The environment or the data does not support this operation |
| `PANEL_MODE_UNSUPPORTED` | Called from a DUI/CUI panel, but the method needs the standalone main window. Use `window.getMode` to learn the mode beforehand |
| `LIBRARY_DISABLED` | Media library is disabled |
| `NO_ACTIVE_ITEM` | No active playlist or now-playing item |

### Operation failures

| Code | Meaning |
| --- | --- |
| `OPERATION_FAILED` | Generic operation failure |
| `CANCELLED` | The caller cancelled an async task (for example `audio.cancelFullWaveform`) |

### Security

| Code | Meaning |
| --- | --- |
| `PERMISSION_DENIED` | Path security policy rejected the request. A parameter that fails on shape or type instead of on path policy returns `INVALID_PARAMS` |
| `ORIGIN_DENIED` | The page's origin is not trusted for this capability. Every call from a page the host does not trust fails with it (see [Security reference](./security.md#which-pages-can-call-the-api)); `dnd.prepareDrag` returns it for a page that is not given file paths, and `audio:pcmFailed` carries it when the page navigated to an untrusted origin while `audio.decodePcm` was running |

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

### Menu and port

These codes were published before the shared codes above and are kept as they are.

| Code | Meaning |
| --- | --- |
| `MENU_ITEM_DISABLED` | The menu command exists but is disabled (`menu.runMainMenuCommand`) |
| `MENU_MATCH_AMBIGUOUS` | A command name or path matched several commands; the failure lists them in `candidates` (`menu.runMainMenuCommand`) |
| `MENU_COMMAND_NOT_FOUND` | No menu command has that name or path (`menu.runMainMenuCommand`, `menu.runContextCommand`) |
| `PORT_NOT_FOUND` | No open port has that id (`port.disconnect`, `port.postMessage`, `port.postMessageTo`) |
| `TARGET_NOT_FOUND` | The target port of `port.postMessageTo` does not exist |

## TypeScript types

```typescript
import type { ErrorEnvelope, FailureEventPayload, ApiErrorCode, BaseResponse } from 'foo-webview-sdk';
```

- `ErrorEnvelope` — minimum synchronous failure shape
- `FailureEventPayload` — minimum async failure event data
- `ApiErrorCode` — union of standard codes
- `BaseResponse` — shared optional `error` / `code` fields

## Handling examples

```javascript
const result = await fb2k.invoke('library.browseTree', { rootId: 'invalid' });
if (result.success === false) {
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
