# `fb.player` playback control

## play() 

Starts playback. If playback is paused, it resumes. Returns a `BaseResponse`.

```javascript
await fb.player.play();
```

## pause() 

Pauses playback. Returns a `BaseResponse`.

```javascript
await fb.player.pause();
```

## stop() 

Stops playback. Returns a `BaseResponse`.

```javascript
await fb.player.stop();
```

## next() / prev() 

Starts the next or previous track. Both methods return a `BaseResponse`.

```javascript
await fb.player.next();
await fb.player.prev();
```

## seek(seconds) 

Seeks to a position in the current track.

| Parameter | Type | Description |
| --- | --- | --- |
| `seconds` | `number` | Target position in seconds |

```javascript
await fb.player.seek(90); // 1 minute 30 seconds
```

## getVolume() 

Returns the current linear volume, decibel volume, and mute state. `volume` is in the range `0..100`.

```javascript
const info = await fb.player.getVolume();
console.log(info.volume);   // 0-100
console.log(info.volumeDb); // -100..0 dB
console.log(info.muted);    // boolean
```

## setVolume(volume) 

Sets the linear volume.

| Parameter | Type | Description |
| --- | --- | --- |
| `volume` | `number` | Linear volume in the range `0..100` |

```javascript
await fb.player.setVolume(80); // 80%
await fb.player.setVolume(0);  // Minimum linear volume
```

## mute() 

Mutes playback. Use `fb.player.toggleMute()` when the desired operation is a toggle.

```javascript
await fb.player.mute();
```

## toggle() 

Toggles play/pause through `playback.playOrPause`. The response includes the post-toggle `isPlaying` state.

```javascript
const r = await fb.player.toggle();
console.log(r.isPlaying ? 'Playing' : 'Paused');
```

## random() 

Starts a random track. Returns a `BaseResponse`.

```javascript
await fb.player.random();
```

## getState() 

Returns `{ state, canSeek, canPause }`.

```javascript
const state = await fb.player.getState();
// state.state: 'playing' | 'paused' | 'stopped'
```

## getCurrentTrack() 

Returns the current `TrackInfo`, or `null` when no track is loaded.

| Field | Type | Description |
| --- | --- | --- |
| `title`, `artist`, `album` | `string` | Core metadata |
| `duration` | `number` | Duration in seconds |
| `path` | `string` | Original foobar2000 path |
| `absolutePath` | `string?` | Native absolute path when available |
| `id` | `string?` | Canonical track identifier when available |
| `subsong` | `number?` | Subsong index |
| `albumArtist`, `genre`, `date` | `string?` | Optional metadata |
| `trackNumber`, `discNumber` | `number?` | Optional track/disc numbers |
| `fileSize` | `number?` | File size in bytes |
| `bitrate` | `number?` | Bitrate in kbps |
| `sampleRate` | `number?` | Sample rate in Hz |
| `channels` | `number?` | Channel count |
| `codec` | `string?` | Codec identifier |

```javascript
const track = await fb.player.getCurrentTrack();
if (track) {
    console.log(`${track.artist} - ${track.title} [${track.codec} ${track.bitrate}kbps]`);
}
```

## getPosition() 

Returns the current position and duration in seconds, plus path/subsong information when exposed by the host.

```javascript
const pos = await fb.player.getPosition();
console.log(`${pos.position} / ${pos.duration}`);
// pos.subsong — subsong index
// pos.path — current source path
```

## getOrder() / setOrder(order) 

Gets or sets the playback order. `setOrder()` accepts either a numeric host order or a canonical `PlaybackOrder` string.

| Value | Name |
| --- | --- |
| 0 | Default |
| 1 | Repeat (playlist) |
| 2 | Repeat (track) |
| 3 | Random |
| 4 | Shuffle (tracks) |
| 5 | Shuffle (albums) |
| 6 | Shuffle (directories) |

```javascript
const r = await fb.player.getOrder();
console.log(r.order, r.name); // 0, 'Default'
await fb.player.setOrder(2);  // Repeat track
await fb.player.setOrder('shuffle-albums');
```

## getStopAfterCurrent() / setStopAfterCurrent(enabled) 

Gets or sets stop-after-current.

```javascript
const r = await fb.player.getStopAfterCurrent();
console.log(r.enabled); // false
await fb.player.setStopAfterCurrent(true);
```

## getCurrentTrackIndex(includeTrackInfo?)

Returns `{ index, track? }`. Pass `true` to request the optional `track` snapshot.

```javascript
const r = await fb.player.getCurrentTrackIndex();
console.log(`Current item index: ${r.index}`);
```

## playPath(path)

Starts playback from a path. CUE entries may use the host's subsong suffix format.

```javascript
await fb.player.playPath('E:\\Music\\song.flac');
```

## playPaths(paths, options?)

Adds and plays multiple paths. The second argument may be a numeric `startIndex` or `{ startIndex?, replace? }`; `replace: true` clears the active playlist before insertion, while the default appends.

| Parameter | Type | Description |
| --- | --- | --- |
| `paths` | `string[]` | Absolute paths; entries may carry a `|subsong:N` suffix |
| `options` | `number \| { startIndex?: number; replace?: boolean }` | Start offset or extended options |

```javascript
await fb.player.playPaths(paths, { startIndex: 2, replace: true });
```

## volumeUp() / volumeDown()

Moves the volume up or down by one host-defined step.

```javascript
await fb.player.volumeUp();
await fb.player.volumeDown();
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block completes SDK method coverage and may later be expanded with richer examples and guidance.

### getPlayingPlaylist()

Signature: `fb.player.getPlayingPlaylist(): Promise<{ playlist: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the result of `playback.getPlayingPlaylist`.

```javascript
const result = await fb.player.getPlayingPlaylist();
```

### playPause()

Signature: `fb.player.playPause(): Promise<PlaybackToggleResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the result of `playback.playPause`.

```javascript
const result = await fb.player.playPause();
```

### toggleMute()

Signature: `fb.player.toggleMute(): Promise<PlaybackToggleMuteResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the result of `playback.toggleMute`.

```javascript
const result = await fb.player.toggleMute();
```

### toggleStopAfterCurrent()

Signature: `fb.player.toggleStopAfterCurrent(): Promise<PlaybackStopAfterCurrentState>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the result of `playback.toggleStopAfterCurrent`.

```javascript
const result = await fb.player.toggleStopAfterCurrent();
```

<!-- END AUTO-GENERATED SDK STUBS -->
