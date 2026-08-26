# Playback API

English API reference for the `playback` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## playback

### playback.getCurrentTrack


_No parameters._

**Returns**: `{"found":"...","playing":"...","success":true}`

```js
const result = await fb2k.invoke('playback.getCurrentTrack');
```

### playback.getCurrentTrackIndex


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | No | `false` | Includes the full `track` info object. |

**Returns**: `{"found":true,"index":0,"playlist":0,"success":true,"track":{}}`

```js
const { playlist, index } = await fb2k.invoke('playback.getCurrentTrackIndex');
```

### playback.getPlaybackOrder


_No parameters._

**Returns**: `{"name":"...","order":"...","orderIndex":"...","orderName":"..."}`

```js
const result = await fb2k.invoke('playback.getPlaybackOrder');
```

### playback.getPlayingPlaylist


_No parameters._

**Returns**: `{"found":"...","name":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playback.getPlayingPlaylist');
```

### playback.getPosition


_No parameters._

**Returns**: `{"duration":"...","path":"...","position":"...","subsong":"..."}`

```js
const result = await fb2k.invoke('playback.getPosition');
```

### playback.getState


_No parameters._

**Returns**: `{"canPause":"...","canSeek":"...","state":"..."}`

```js
const result = await fb2k.invoke('playback.getState');
```

### playback.getStopAfterCurrent


_No parameters._

**Returns**: `{"enabled":"..."}`

```js
const result = await fb2k.invoke('playback.getStopAfterCurrent');
```

### playback.getVolume


_No parameters._

**Returns**: `{"isMuted":"...","muted":"...","volume":"...","volumeDb":"..."}`

```js
const result = await fb2k.invoke('playback.getVolume');
```

### playback.mute


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `muted` | `boolean` | No | `true` | `false` unmutes. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('playback.mute', { muted: true });
```

### playback.next


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.next');
```

### playback.pause


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.pause');
```

### playback.play


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.play');
```

### playback.playOrPause


_No parameters._

**Returns**: `{"isPlaying":"...","success":true}`

```js
const result = await fb2k.invoke('playback.playOrPause');
```

### playback.playPath


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File path to play; a missing or empty value returns `path is required`. |

**Returns**: `{"error":"...","path":"...","subsong":"...","success":true,"tracksAdded":"..."}`

```js
await fb2k.invoke('playback.playPath', { path: 'C:\\Music\\song.flac' });
```

Use a `path|subsong:N` value to address a CUE subsong explicitly. The handler separates the file path from the optional subsong suffix before playback.

### playback.playPaths


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `paths` | `array<string>` | Yes | — | File paths or URLs. |
| `startIndex` | `integer` | No | `0` | Index to start playing after the add. |
| `replace` | `boolean` | No | `false` | Replaces existing content instead of appending. |

**Returns**: `{"error":"...","startedAt":"...","success":true,"tracksAdded":"..."}`

```js
await fb2k.invoke('playback.playPaths', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```

### playback.playPause


_No parameters._

**Returns**: `{"isPlaying":"...","success":true}`

```js
const result = await fb2k.invoke('playback.playPause');
```

### playback.previous


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.previous');
```

### playback.random


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.random');
```

### playback.setPlaybackOrder


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `order` | `integer \| string` | No | Order index or name. Default `0` (`default`). |

**Returns**: `{ "success": true, "order": 3, "orderName": "random" }`

Accepts either the index or the name: `0` default, `1` repeat-playlist, `2` repeat-track, `3` random, `4` shuffle-tracks, `5` shuffle-albums, `6` shuffle-folders. An unrecognized name falls back to `0`. An out-of-range index is passed through unvalidated and echoed back in `order`, with `orderName` reported as `default` — read the returned pair to confirm what took effect.

```js
await fb2k.invoke('playback.setPlaybackOrder', { order: 'random' });
```

### playback.setPosition


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `position` | `number` | No | `0` | Target position in seconds. |
| `seconds` | `number` | No | `0` | Alias of `position`; ignored when `position` is present. |

**Returns**: `{"actualPosition":"...","duration":"...","error":"...","newPosition":"...","oldPosition":"...","requestedPosition":"...","subsong":"...","success":true}`

```js
await fb2k.invoke('playback.setPosition', { position: 42.5 });
```

### playback.setStopAfterCurrent


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `enabled` | `boolean` | No | `false` |  |

**Returns**: `{"enabled":"...","success":true}`

```js
await fb2k.invoke('playback.setStopAfterCurrent', { enabled: true });
```

### playback.setVolume


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `volume` | `number` | No | `100` | Volume percent (0-100). |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('playback.setVolume', { volume: 80 });
```

### playback.stop


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.stop');
```

### playback.toggleMute


_No parameters._

**Returns**: `{"muted":"...","success":true}`

```js
const result = await fb2k.invoke('playback.toggleMute');
```

### playback.toggleStopAfterCurrent


_No parameters._

**Returns**: `{"enabled":"..."}`

```js
const result = await fb2k.invoke('playback.toggleStopAfterCurrent');
```

### playback.volumeDown


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.volumeDown');
```

### playback.volumeUp


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.volumeUp');
```

## Related events

Related event `playback:stopAfterCurrentChanged` uses payload `{ enabled }` (same field name as the API).
