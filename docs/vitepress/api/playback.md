# Playback API

English API reference for the `playback` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## playback

### playback.getCurrentTrack


<!-- phase3-major1-review:playback.getCurrentTrack -->
#### Source-reviewed contract
Authority: `src/api/PlaybackApi.cpp:372-380`.

_No public parameters._

**Return keys (vary by response variant)**: `found`, `playing`, `success`

**Semantics**: No request fields are read. The service returns a full current-track object when available, otherwise the explicit { success, found: false, playing: false } no-track variant; path fields distinguish native absolutePath from the optional subsong-bearing fullPath.

<!-- phase3-major1-review-end:playback.getCurrentTrack -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:691`.

_No parameters._

**Returns**: `{"found":"...","playing":"...","success":true}`

```js
const result = await fb2k.invoke('playback.getCurrentTrack');
```

### playback.getCurrentTrackIndex

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:702`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | No | Optional; default false. |

**Returns**: `{"found":true,"index":0,"playlist":0,"success":true,"track":{}}`

```js
const result = await fb2k.invoke('playback.getCurrentTrackIndex', { includeTrackInfo: /* value */ });
```

### playback.getPlaybackOrder

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:692`.

_No parameters._

**Returns**: `{"name":"...","order":"...","orderIndex":"...","orderName":"..."}`

```js
const result = await fb2k.invoke('playback.getPlaybackOrder');
```

### playback.getPlayingPlaylist

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:703`.

_No parameters._

**Returns**: `{"found":"...","name":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playback.getPlayingPlaylist');
```

### playback.getPosition


<!-- phase3-major1-review:playback.getPosition -->
#### Source-reviewed contract
Authority: `src/api/PlaybackApi.cpp:231-246`.

_No public parameters._

**Return keys (vary by response variant)**: `duration`, `path`, `position`, `subsong`

**Semantics**: No request fields are read. Position, duration, subsong, and foobar path are sampled from the current playback service state and do not imply that a track is seekable.

<!-- phase3-major1-review-end:playback.getPosition -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:683`.

_No parameters._

**Returns**: `{"duration":"...","path":"...","position":"...","subsong":"..."}`

```js
const result = await fb2k.invoke('playback.getPosition');
```

### playback.getState


<!-- phase3-major1-review:playback.getState -->
#### Source-reviewed contract
Authority: `src/api/PlaybackApi.cpp:216-231`.

_No public parameters._

**Return keys (vary by response variant)**: `canPause`, `canSeek`, `state`

**Semantics**: No request fields are read. State is stopped, paused, or playing from playback service state; canSeek is authoritative while canPause is always returned true.

<!-- phase3-major1-review-end:playback.getState -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:682`.

_No parameters._

**Returns**: `{"canPause":"...","canSeek":"...","state":"..."}`

```js
const result = await fb2k.invoke('playback.getState');
```

### playback.getStopAfterCurrent

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:694`.

_No parameters._

**Returns**: `{"enabled":"..."}`

```js
const result = await fb2k.invoke('playback.getStopAfterCurrent');
```

### playback.getVolume


<!-- phase3-major1-review:playback.getVolume -->
#### Source-reviewed contract
Authority: `src/api/PlaybackApi.cpp:291-314`.

_No public parameters._

**Return keys (vary by response variant)**: `isMuted`, `muted`, `volume`, `volumeDb`

**Semantics**: No request fields are read. volume is the clamped 0–100 linear conversion of volumeDb; muted and isMuted are equal compatibility fields.

<!-- phase3-major1-review-end:playback.getVolume -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:685`.

_No parameters._

**Returns**: `{"isMuted":"...","muted":"...","volume":"...","volumeDb":"..."}`

```js
const result = await fb2k.invoke('playback.getVolume');
```

### playback.mute

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:689`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `muted` | `boolean` | No | Optional; default true. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.mute', { muted: /* value */ });
```

### playback.next

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:679`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.next');
```

### playback.pause

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:675`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.pause');
```

### playback.play

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:674`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.play');
```

### playback.playOrPause

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:678`.

_No parameters._

**Returns**: `{"isPlaying":"...","success":true}`

```js
const result = await fb2k.invoke('playback.playOrPause');
```

### playback.playPath

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:699`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","path":"...","subsong":"...","success":true,"tracksAdded":"..."}`

```js
const result = await fb2k.invoke('playback.playPath', { path: /* value */ });
```

Use a `path|subsong:N` value to address a CUE subsong explicitly. The handler separates the file path from the optional subsong suffix before playback.

### playback.playPaths


<!-- phase3-major1-review:playback.playPaths -->
#### Source-reviewed contract
Authority: `src/api/PlaybackApi.cpp:586-615`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `paths` | `array<string>` | Yes | none |
| `startIndex` | `integer` | No | `0` |
| `replace` | `boolean` | No | `false` |

**Return keys (vary by response variant)**: `error`, `success`; `error`, `success`; `startedAt`, `success`, `tracksAdded`; `error`, `success`

**Semantics**: paths must be an array or the handler returns success:false with an error. Elements are converted to strings by the service call; replace selects replacement versus append behavior and startIndex identifies the item to start.

<!-- phase3-major1-review-end:playback.playPaths -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:700`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array<string>` | Yes | Required. |
| `startIndex` | `integer` | No | Optional; default 0. |
| `replace` | `boolean` | No | Optional; default false. |

**Returns**: `{"error":"...","startedAt":"...","success":true,"tracksAdded":"..."}`

```js
const result = await fb2k.invoke('playback.playPaths', { paths: /* value */, replace: /* value */, startIndex: /* value */ });
```

### playback.playPause

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:677`.

_No parameters._

**Returns**: `{"isPlaying":"...","success":true}`

```js
const result = await fb2k.invoke('playback.playPause');
```

### playback.previous

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:680`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.previous');
```

### playback.random

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:681`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.random');
```

### playback.setPlaybackOrder


<!-- phase3-major1-review:playback.setPlaybackOrder -->
#### Source-reviewed contract
Authority: `src/api/PlaybackApi.cpp:395-418`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `order` | `integer or string` | No | `0` |

**Return keys (vary by response variant)**: `order`, `orderName`, `success`

**Semantics**: An omitted or unsupported order resolves to the default order 0; numeric values pass through to the playback service while accepted strings map to the documented named orders.

<!-- phase3-major1-review-end:playback.setPlaybackOrder -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:693`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `order` | `integer or string` | No | Optional; default 0. |

| --- | --- | --- | --- |
| `order` | `integer\\\|string` | No | | Optional; default 0. |

**Returns**: `{"order":"...","orderName":"...","success":true}`

```js
const result = await fb2k.invoke('playback.setPlaybackOrder', { order: /* value */ });
```

### playback.setPosition

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:684`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `position` | `number` | No | Optional; default 0. |
| `seconds` | `number` | No | Optional; default 0. |

**Returns**: `{"actualPosition":"...","duration":"...","error":"...","newPosition":"...","oldPosition":"...","requestedPosition":"...","subsong":"...","success":true}`

```js
const result = await fb2k.invoke('playback.setPosition', { position: /* value */, seconds: /* value */ });
```

### playback.setStopAfterCurrent

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:695`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | No | Optional; default false. |

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('playback.setStopAfterCurrent', { enabled: /* value */ });
```

### playback.setVolume


<!-- phase3-major1-review:playback.setVolume -->
#### Source-reviewed contract
Authority: `src/api/PlaybackApi.cpp:314-333`.

| Parameter | Type | Required | Default |
| --- | --- | --- | --- |
| `volume` | `number` | No | `100` |

**Return keys (vary by response variant)**: `success`

**Semantics**: volume is interpreted as a linear 0–100 percentage and converted to dB. Values at or below zero mute at -100 dB, and positive values are clamped to the supported -100..0 dB range.

<!-- phase3-major1-review-end:playback.setVolume -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:686`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `volume` | `number` | No | Optional; default 100. |

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.setVolume', { volume: /* value */ });
```

### playback.stop

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:676`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.stop');
```

### playback.toggleMute

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:690`.

_No parameters._

**Returns**: `{"muted":"...","success":true}`

```js
const result = await fb2k.invoke('playback.toggleMute');
```

### playback.toggleStopAfterCurrent

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:696`.

_No parameters._

**Returns**: `{"enabled":"..."}`

```js
const result = await fb2k.invoke('playback.toggleStopAfterCurrent');
```

### playback.volumeDown

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:688`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.volumeDown');
```

### playback.volumeUp

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:687`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.volumeUp');
```

## Related events

Related event `playback:stopAfterCurrentChanged` uses payload `{ enabled }` (same field name as the API).

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:playback.setPosition -->
### Contract supplement: `playback.setPosition`

Verified contract supplement. Runtime authority: `src/api/PlaybackApi.cpp:246-288`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `position` | `number` | No | `0` | Optional; default 0. |
| `seconds` | `number` | No | `0` | Optional; default 0. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `actualPosition` | `json` | No |
| `duration` | `json` | No |
| `newPosition` | `json` | No |
| `oldPosition` | `json` | No |
| `requestedPosition` | `json` | No |
| `subsong` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('playback.setPosition', { position: /* value */, seconds: /* value */ });
```
