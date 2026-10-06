# Playback API

Methods of the `playback` namespace.

## Playback position and timing

`playback.getPosition`, `playback:timeHighRes` and `playback:stateChanged` report a position that already accounts for output-buffer and DSP latency. Do not subtract that latency again when synchronizing lyrics or video. The reading advances in steps, typically 10–30 ms apart; this is separate from the roughly 100 ms interval of `playback:timeHighRes` events.

`hostTime` records when the host sampled the position, in Unix milliseconds on the same clock as `Date.now()`. Extrapolation estimates movement between samples; it does not remove stalls or jumps after starting, seeking or resuming, or during crossfades. For video, use the SDK's [MediaElementFollower](../sdk/media.md).

`playback:time` is a coarse display update without a sample timestamp. `playback:seeked.position` and `playback.setPosition.actualPosition` are seek targets; the immediately sampled `newPosition` may still contain the old position.

## playback

### playback.getCurrentTrack

<!-- api-schema:begin playback.getCurrentTrack -->
Report the track now loaded, playing or paused. Always succeeds; `found` says whether there is one.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `found` | `boolean` | Whether a track is loaded. |
| `track` | [Track](../reference/types.md#track) | The loaded track; absent when `found` is `false`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getCurrentTrack');
```

### playback.getCurrentTrackIndex

<!-- api-schema:begin playback.getCurrentTrackIndex -->
Locate the playing track in its playlist. Known only when playback started from a playlist item; a track played from elsewhere has no location even while a playing playlist exists. The location is also lost once the playing row is removed from the playlist it started from, the playlist included, while the track keeps playing; and there is none while stopped.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | No | `true` adds the track row at that location. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `found` | `boolean` | Whether the playing track has a playlist location. |
| `playlist` | `integer \| null` | Playlist index; `null` when there is no location. |
| `playlistGuid` | `string \| null` | GUID of that playlist, as `playlistGuid` takes it; `null` when there is no location. |
| `index` | `integer \| null` | Row in that playlist; `null` when there is no location. |
| `track` | [Track](../reference/types.md#track) | The track at that location; only with `includeTrackInfo`, and only when the row still exists. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playback.getCurrentTrackIndex');
if (res.success === false) throw new Error(res.error);
const { playlist, index } = res;
```

### playback.getPlaybackOrder

<!-- api-schema:begin playback.getPlaybackOrder -->
Report the active playback order as its index and its name.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `order` | `integer` | Index of the active order, `0` to `6`. |
| `orderName` | `"default" \| "repeat-playlist" \| "repeat-track" \| "random" \| "shuffle-tracks" \| "shuffle-albums" \| "shuffle-folders"` | Name of the active order. |
| `name` | `"default" \| "repeat-playlist" \| "repeat-track" \| "random" \| "shuffle-tracks" \| "shuffle-albums" \| "shuffle-folders"` | The same as `orderName`. |
| `orderIndex` | `integer` | The same as `order`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getPlaybackOrder');
```

### playback.getPlayingPlaylist

<!-- api-schema:begin playback.getPlayingPlaylist -->
Report the playlist playback was last started from. foobar2000 keeps the pointer after playback stops, so a stopped instance that has played before still reports one.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `found` | `boolean` | Whether a playing playlist is known. |
| `playlist` | `integer \| null` | Playlist index; `null` when none is known. |
| `playlistGuid` | `string \| null` | GUID of that playlist, as `playlistGuid` takes it; `null` when none is known. |
| `name` | `string` | Name of that playlist; absent when none is known. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getPlayingPlaylist');
```

### playback.getPosition

<!-- api-schema:begin playback.getPosition -->
Report the playback position together with the length and identity of the track it is in. Everything is zero or empty when nothing is playing.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `position` | `number` | Audible position in seconds from the start of the track, already adjusted for output-buffer and DSP latency; do not subtract that latency again. The host advances this reading in steps, typically 10–30 ms apart. It can briefly stall or jump after starting, seeking or resuming, and during crossfades; it is not a sample-accurate clock. |
| `duration` | `number` | Length of the track in seconds; `0` when nothing is playing or the length is unknown. |
| `subsong` | `integer` | Subsong identifier of the track; `0` for a whole file. |
| `path` | `string` | Path as foobar2000 stores it, without a subsong suffix; empty when nothing is playing. |
| `hostTime` | `number` | Host system time when `position` was read, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. While playing and not paused, `position + (Date.now() - hostTime) / 1000` estimates the current position. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getPosition');
```

### playback.getState

<!-- api-schema:begin playback.getState -->
Report the transport state and what the current track allows.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `state` | `"stopped" \| "playing" \| "paused"` | Transport state. |
| `canSeek` | `boolean` | Whether the current track can be seeked; `false` when stopped and for streams that do not support it. |
| `canPause` | `boolean` | Whether the current track can be paused; always `true`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getState');
```

### playback.getStopAfterCurrent

<!-- api-schema:begin playback.getStopAfterCurrent -->
Report whether playback stops after the current track.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether playback stops after the current track. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getStopAfterCurrent');
```

### playback.getVolume

<!-- api-schema:begin playback.getVolume -->
Report the output volume on both scales the host uses, and whether it is muted.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `volume` | `number` | Volume as a percentage from `0` to `100`, converted from the decibel value; `0` when muted. |
| `volumeDb` | `number` | Volume in decibels, `-100` (silence) to `0` (full). |
| `muted` | `boolean` | Whether output is muted; volume `0` counts as muted. |
| `isMuted` | `boolean` | The same as `muted`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getVolume');
```

### playback.mute

<!-- api-schema:begin playback.mute -->
Mute or unmute; a call that asks for the state already in effect does nothing. Unmuting restores the level from before the mute.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `muted` | `boolean` | No | `true` to mute, `false` to unmute. Default: `true`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.mute', { muted: true });
```

### playback.next

<!-- api-schema:begin playback.next -->
Skip to the next track under the current playback order.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.next');
```

### playback.pause

<!-- api-schema:begin playback.pause -->
Pause playback. Does nothing when stopped.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.pause');
```

### playback.play

<!-- api-schema:begin playback.play -->
Start playback, or resume when paused.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.play');
```

### playback.playOrPause

<!-- api-schema:begin playback.playOrPause -->
The same as `playPause`.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `isPlaying` | `boolean` | Whether playback is running after the toggle. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.playOrPause');
```

### playback.playPath

<!-- api-schema:begin playback.playPath -->
Append one file to the active playlist and play it; a playlist is created when there is none. A `|subsong:N` suffix selects a subsong of the file. When the active playlist is locked the call fails with `LOCKED`, and nothing is added or played. A path the media-root check refuses fails with `PERMISSION_DENIED`; one foobar2000 resolves to nothing fails with `NOT_FOUND`, the failure carrying `path` and `subsong`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File path, optionally with a `\|subsong:N` suffix. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tracksAdded` | `integer` | Number of playlist rows added; `1`. |
| `path` | `string` | The file path without the subsong suffix. |
| `subsong` | `integer` | Subsong identifier of the track that plays: the one asked for with `\|subsong:N`, even when foobar2000 did not list it among the file's subsongs; without the suffix, the file's first. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.playPath', { path: 'C:\\Music\\song.flac' });
```

Use a `path|subsong:N` value to address a CUE subsong explicitly. The handler separates the file path from the optional subsong suffix before playback.

### playback.playPaths

<!-- api-schema:begin playback.playPaths -->
Add several files to the active playlist and start playing one of them. Entries that are not strings or fail the media-root check are dropped and counted in `skippedPaths`, which only a successful call reports; when every entry is dropped the call fails with `INVALID_PARAMS`. Each remaining path becomes a track as written: folders and playlist files are not expanded and the file is not opened, so a missing file is added too. When no track can be made the call fails with `NOT_FOUND`. When the active playlist is locked the call fails with `LOCKED`, and nothing is cleared, added or played.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | File paths, each optionally with a `\|subsong:N` suffix. Must not be empty. |
| `startIndex` | `integer` | No | Which of the added tracks to start with, counted among the tracks that resolved; a value past the end starts the first. At least `0`. Default: `0`. |
| `replace` | `boolean` | No | `true` clears the active playlist before adding; `false` appends. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tracksAdded` | `integer` | Number of tracks added to the playlist. |
| `startedAt` | `integer` | The `startIndex` given, echoed even when it was past the end and playback started at the first added track. |
| `skippedPaths` | `integer` | Number of paths the security check dropped before the call ran; present only when at least one was dropped. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.playPaths', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```

### playback.playPause

<!-- api-schema:begin playback.playPause -->
Toggle between playing and paused; starts playback when stopped. Reports the state that results.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `isPlaying` | `boolean` | Whether playback is running after the toggle. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.playPause');
```

### playback.previous

<!-- api-schema:begin playback.previous -->
Skip to the previous track under the current playback order.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.previous');
```

### playback.random

<!-- api-schema:begin playback.random -->
Start a random track of the active playlist.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.random');
```

### playback.setPlaybackOrder

<!-- api-schema:begin playback.setPlaybackOrder -->
Select a playback order by index or by name; exactly one of the two must be given. Reports the order in effect afterwards.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `order` | `integer` | No | Order by index, `0` to `6` in the order of `PlaybackOrderName`. Between `0` and `6` inclusive. |
| `name` | `"default" \| "repeat-playlist" \| "repeat-track" \| "random" \| "shuffle-tracks" \| "shuffle-albums" \| "shuffle-folders"` | No | Order by name. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `order` | `integer` | Index of the order in effect after the call. |
| `orderName` | `"default" \| "repeat-playlist" \| "repeat-track" \| "random" \| "shuffle-tracks" \| "shuffle-albums" \| "shuffle-folders"` | Name of the order in effect after the call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Pass exactly one of `order` (the index) and `name`; the indices are `0` default, `1` repeat-playlist, `2` repeat-track, `3` random, `4` shuffle-tracks, `5` shuffle-albums, `6` shuffle-folders. An index outside that range, a name no order has, both keys at once, or neither is refused with `INVALID_PARAMS` and the order is left alone. The response reports the order in effect after the call, not the input.

```js
await fb2k.invoke('playback.setPlaybackOrder', { name: 'random' });
```

### playback.setPosition

<!-- api-schema:begin playback.setPosition -->
Seek within the current track, playing or paused. The position is clamped to the track: negative values seek to the start, values past the end stop just short of it so playback does not advance to the next track; a track of unknown length has no upper bound. With nothing playing the call fails with `NO_ACTIVE_ITEM`, and a track that cannot seek, such as most streams, fails with `NOT_SUPPORTED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `position` | `number` | Yes | Target position in seconds. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `requestedPosition` | `number` | The position asked for, before clamping. |
| `actualPosition` | `number` | The position handed to the host after clamping to the track. |
| `oldPosition` | `number` | Position read before the seek, in seconds, with the same latency compensation as `playback.getPosition`. |
| `newPosition` | `number` | Position read right after the seek, in seconds, with the same latency compensation as `playback.getPosition`; the engine may still be settling, so this can still be the old position. |
| `hostTime` | `number` | Host system time when `newPosition` was read, Unix epoch milliseconds with a fractional part, the same clock as `Date.now()`. |
| `duration` | `number` | Length of the track in seconds; `0` or less when the length is unknown. |
| `subsong` | `integer` | Subsong identifier of the track. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.setPosition', { position: 42.5 });
```

### playback.setStopAfterCurrent

<!-- api-schema:begin playback.setStopAfterCurrent -->
Arm or disarm stopping after the current track.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | Yes | `true` to stop after the current track. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | The value now in effect. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.setStopAfterCurrent', { enabled: true });
```

### playback.setVolume

<!-- api-schema:begin playback.setVolume -->
Set the output volume as a percentage; values outside `0` to `100` are clamped. `0` mutes.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `volume` | `number` | Yes | Volume as a percentage; clamped to `0` to `100`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.setVolume', { volume: 80 });
```

### playback.stop

<!-- api-schema:begin playback.stop -->
Stop playback.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.stop');
```

### playback.toggleMute

<!-- api-schema:begin playback.toggleMute -->
Flip the mute state and report the new one.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `muted` | `boolean` | Whether output is muted after the toggle. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.toggleMute');
```

### playback.toggleStopAfterCurrent

<!-- api-schema:begin playback.toggleStopAfterCurrent -->
Flip the stop-after-current flag and report the new value.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | The value after the toggle. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.toggleStopAfterCurrent');
```

### playback.volumeDown

<!-- api-schema:begin playback.volumeDown -->
Lower the volume by one of the host's steps: one decibel, landing on a whole decibel.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.volumeDown');
```

### playback.volumeUp

<!-- api-schema:begin playback.volumeUp -->
Raise the volume by one of the host's steps: one decibel, landing on a whole decibel.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.volumeUp');
```

## Related events

Related event `playback:stopAfterCurrentChanged` uses payload `{ enabled }` (same field name as the API).
