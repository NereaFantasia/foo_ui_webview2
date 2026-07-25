# Playback API

English API reference for the `playback` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## playback

### playback.getCurrentTrack


<!-- phase3-major1-review:playback.getCurrentTrack -->
#### 源码复核 contract
权威源: `src/api/PlaybackApi.cpp:372-380`.

_无公开参数。_

**返回字段（按变体取值）**: `found`, `playing`, `success`

**语义**: No request fields are read. The service returns a full current-track object when available, otherwise the explicit { success, found: false, playing: false } no-track variant; path fields distinguish native absolutePath from the optional subsong-bearing fullPath.

<!-- phase3-major1-review-end:playback.getCurrentTrack -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:691`.

_No parameters._

**Returns**: `{"found":"...","playing":"...","success":true}`

```js
const result = await fb2k.invoke('playback.getCurrentTrack');
```

### playback.getCurrentTrackIndex

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:702`.

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | 否 | 可选；默认 false。 |

**返回值**: `{"found":true,"index":0,"playlist":0,"success":true,"track":{}}`

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
#### 源码复核 contract
权威源: `src/api/PlaybackApi.cpp:231-246`.

_无公开参数。_

**返回字段（按变体取值）**: `duration`, `path`, `position`, `subsong`

**语义**: No request fields are read. Position, duration, subsong, and foobar path are sampled from the current playback service state and do not imply that a track is seekable.

<!-- phase3-major1-review-end:playback.getPosition -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:683`.

_No parameters._

**Returns**: `{"duration":"...","path":"...","position":"...","subsong":"..."}`

```js
const result = await fb2k.invoke('playback.getPosition');
```

### playback.getState


<!-- phase3-major1-review:playback.getState -->
#### 源码复核 contract
权威源: `src/api/PlaybackApi.cpp:216-231`.

_无公开参数。_

**返回字段（按变体取值）**: `canPause`, `canSeek`, `state`

**语义**: No request fields are read. State is stopped, paused, or playing from playback service state; canSeek is authoritative while canPause is always returned true.

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
#### 源码复核 contract
权威源: `src/api/PlaybackApi.cpp:291-314`.

_无公开参数。_

**返回字段（按变体取值）**: `isMuted`, `muted`, `volume`, `volumeDb`

**语义**: No request fields are read. volume is the clamped 0–100 linear conversion of volumeDb; muted and isMuted are equal compatibility fields.

<!-- phase3-major1-review-end:playback.getVolume -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:685`.

_No parameters._

**Returns**: `{"isMuted":"...","muted":"...","volume":"...","volumeDb":"..."}`

```js
const result = await fb2k.invoke('playback.getVolume');
```

### playback.mute

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:689`.

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `muted` | `boolean` | 否 | 可选；默认 true。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选；默认 。 |

**Returns**: `{"error":"...","path":"...","subsong":"...","success":true,"tracksAdded":"..."}`

```js
const result = await fb2k.invoke('playback.playPath', { path: /* value */ });
```

Use a `path|subsong:N` value to address a CUE subsong explicitly. The handler separates the file path from the optional subsong suffix before playback.

### playback.playPaths


<!-- phase3-major1-review:playback.playPaths -->
#### 源码复核 contract
权威源: `src/api/PlaybackApi.cpp:586-615`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `paths` | `array<string>` | 是 | 无 |
| `startIndex` | `integer` | 否 | `0` |
| `replace` | `boolean` | 否 | `false` |

**返回字段（按变体取值）**: `error`, `success`；`error`, `success`；`startedAt`, `success`, `tracksAdded`；`error`, `success`

**语义**: paths must be an array or the handler returns success:false with an error. Elements are converted to strings by the service call; replace selects replacement versus append behavior and startIndex identifies the item to start.

<!-- phase3-major1-review-end:playback.playPaths -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:700`.

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array<string>` | 是 | 必填。 |
| `startIndex` | `integer` | 否 | 可选；默认 0。 |
| `replace` | `boolean` | 否 | 可选；默认 false。 |

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
#### 源码复核 contract
权威源: `src/api/PlaybackApi.cpp:395-418`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `order` | `integer or string` | 否 | `0` |

**返回字段（按变体取值）**: `order`, `orderName`, `success`

**语义**: An omitted or unsupported order resolves to the default order 0; numeric values pass through to the playback service while accepted strings map to the documented named orders.

<!-- phase3-major1-review-end:playback.setPlaybackOrder -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:693`.

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `order` | `integer or string` | 否 | 可选；默认 0。 |

| --- | --- | --- | --- |
| `order` | `integer\\\|string` | 否 | | 可选；默认 0。 |

**Returns**: `{"order":"...","orderName":"...","success":true}`

```js
const result = await fb2k.invoke('playback.setPlaybackOrder', { order: /* value */ });
```

### playback.setPosition

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:684`.

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `position` | `number` | 否 | 可选；默认 0。 |
| `seconds` | `number` | 否 | 可选；默认 0。 |

**Returns**: `{"actualPosition":"...","duration":"...","error":"...","newPosition":"...","oldPosition":"...","requestedPosition":"...","subsong":"...","success":true}`

```js
const result = await fb2k.invoke('playback.setPosition', { position: /* value */, seconds: /* value */ });
```

### playback.setStopAfterCurrent

Public API method. Runtime authority: `src/api/PlaybackApi.cpp:695`.

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 否 | 可选；默认 false。 |

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('playback.setStopAfterCurrent', { enabled: /* value */ });
```

### playback.setVolume


<!-- phase3-major1-review:playback.setVolume -->
#### 源码复核 contract
权威源: `src/api/PlaybackApi.cpp:314-333`.

| 参数 | 类型 | 必填 | 默认值 |
| --- | --- | --- | --- |
| `volume` | `number` | 否 | `100` |

**返回字段（按变体取值）**: `success`

**语义**: volume is interpreted as a linear 0–100 percentage and converted to dB. Values at or below zero mute at -100 dB, and positive values are clamped to the supported -100..0 dB range.

<!-- phase3-major1-review-end:playback.setVolume -->
Public API method. Runtime authority: `src/api/PlaybackApi.cpp:686`.

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `volume` | `number` | 否 | 可选；默认 100。 |

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
### Contract 补充：`playback.setPosition`

经复核的补充 contract。权威源：`src/api/PlaybackApi.cpp:246-288`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `position` | `number` | 否 | `0` | 可选；默认 0。 |
| `seconds` | `number` | 否 | `0` | 可选；默认 0。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |
| `actualPosition` | `json` | 否 |
| `duration` | `json` | 否 |
| `newPosition` | `json` | 否 |
| `oldPosition` | `json` | 否 |
| `requestedPosition` | `json` | 否 |
| `subsong` | `json` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('playback.setPosition', { position: /* value */, seconds: /* value */ });
```
