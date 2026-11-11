# `fb.player` playback control

## Endpoint mapping

The methods on `fb.player` are developer-friendly convenience aliases, with some names mapped differently from the underlying `fb2k.invoke` endpoints. Consult this table when communicating through the bridge directly or debugging endpoint names:

| Method | Invokes |
| --- | --- |
| `play()` | `playback.play` |
| `pause()` | `playback.pause` |
| `stop()` | `playback.stop` |
| `next()` | `playback.next` |
| `prev()` | `playback.previous` |
| `random()` | `playback.random` |
| `toggle()` | `playback.playOrPause` |
| `playPause()` | `playback.playPause` |
| `seek(seconds)` | `playback.setPosition` |
| `getVolume()` | `playback.getVolume` |
| `setVolume(volume)` | `playback.setVolume` |
| `volumeUp()` | `playback.volumeUp` |
| `volumeDown()` | `playback.volumeDown` |
| `mute()` | `playback.mute` |
| `toggleMute()` | `playback.toggleMute` |
| `getState()` | `playback.getState` |
| `getCurrentTrack()` | `playback.getCurrentTrack` |
| `getPosition()` | `playback.getPosition` |
| `getOrder()` | `playback.getPlaybackOrder` |
| `setOrder(order)` | `playback.setPlaybackOrder` |
| `getStopAfterCurrent()` | `playback.getStopAfterCurrent` |
| `setStopAfterCurrent(enabled)` | `playback.setStopAfterCurrent` |
| `toggleStopAfterCurrent()` | `playback.toggleStopAfterCurrent` |
| `getCurrentTrackIndex(includeTrackInfo?)` | `playback.getCurrentTrackIndex` |
| `getPlayingPlaylist()` | `playback.getPlayingPlaylist` |
| `playPath(path)` | `playback.playPath` |
| `playPaths(paths, options?)` | `playback.playPaths` |

## play()

Starts playback. If playback is paused, it resumes. Resolves with `{ success: true }` on success.

```javascript
await fb.player.play();
```

## pause()

Pauses playback. Resolves with `{ success: true }` on success.

```javascript
await fb.player.pause();
```

## stop()

Stops playback. Resolves with `{ success: true }` on success.

```javascript
await fb.player.stop();
```

## next() / prev()

Starts the next or previous track. Both resolve with `{ success: true }` on success.

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
if (info.success === false) throw new Error(info.error);
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

Sets the mute state. The `muted` argument defaults to `true`; pass `false` to unmute. Use `fb.player.toggleMute()` to flip between the two.

```javascript
await fb.player.mute();
```

## toggle()

Toggles play/pause through `playback.playOrPause`. The response includes the post-toggle `isPlaying` state.

```javascript
const r = await fb.player.toggle();
if (r.success === false) throw new Error(r.error);
console.log(r.isPlaying ? 'Playing' : 'Paused');
```

## random()

Starts a random track. Resolves with `{ success: true }` on success.

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

Returns `PlaybackGetCurrentTrackResponse`: `found` indicates whether a track is loaded, and `track` (the shared [Track](../reference/types.md#track) row) is present only when `found` is `true`. The host omits `track` rather than returning `null`.

> `artist`, `albumArtist` and `genre` join multi-value tags with `, ` in tag order; `artists` and `albumArtists` carry the atomic values behind `artist` and `albumArtist`.

```javascript
const result = await fb.player.getCurrentTrack();
if (result.success === false) throw new Error(result.error);
if (!result.found) {
    console.log('nothing is playing');
} else {
    const t = result.track;
    console.log(`${t.artist} - ${t.title} [${t.codec} ${t.bitrate}kbps]`);
}
```

## getPosition()

Returns current playback position and duration in seconds, plus path and subsong details. `hostTime` represents the host-side sampling timestamp in Unix milliseconds (sharing the clock of `Date.now()`): during playback, real-time position can be computed as `pos.position + (Date.now() - pos.hostTime) / 1000`. [`PlaybackClock`](#playbackclock) keeps this estimate up to date between updates.

```javascript
const pos = await fb.player.getPosition();
if (pos.success === false) throw new Error(pos.error);
console.log(`${pos.position} / ${pos.duration}`);
// pos.subsong — subsong index
// pos.path — current source path
```

## getOrder() / setOrder(order)

Gets or sets the playback order. `setOrder()` accepts either the numeric host order or a canonical `PlaybackOrder` name; the host refuses anything else and reports the order in effect (`order`, `orderName`).

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
if (r.success === false) throw new Error(r.error);
console.log(r.order, r.orderName); // 0, 'default'
await fb.player.setOrder(2);  // Repeat track
await fb.player.setOrder('shuffle-albums');
```

## getStopAfterCurrent() / setStopAfterCurrent(enabled)

Gets or sets stop-after-current.

```javascript
const r = await fb.player.getStopAfterCurrent();
if (r.success === false) throw new Error(r.error);
console.log(r.enabled); // false
await fb.player.setStopAfterCurrent(true);
```

## getCurrentTrackIndex(includeTrackInfo?)

Returns `{ found, playlist, index, track? }`; `playlist` and `index` are `null` when the playing track has no playlist location. Pass `true` to add the `track` row.

```javascript
const r = await fb.player.getCurrentTrackIndex();
if (r.success === false) throw new Error(r.error);
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

## getPlayingPlaylist()

Signature: `fb.player.getPlayingPlaylist(): Promise<PlaybackGetPlayingPlaylistResponse>`

Reports the playlist playback was last started from: `found`, `playlist` (index), `playlistGuid`, and `name`. When no playlist history exists, `found` is `false`, `playlist` and `playlistGuid` are `null`, and `name` is omitted. foobar2000 retains this playlist reference after playback stops, allowing stopped instances to report the last active playback source.

```javascript
const res = await fb.player.getPlayingPlaylist();
if (res.success && res.found) console.log(`Playing from ${res.name}`);
```

## playPause()

Signature: `fb.player.playPause(): Promise<PlaybackPlayPauseResponse>`

Toggles between playing and paused, and starts playback when stopped. It is the same operation as `toggle()` under another host name (`playback.playPause`). Resolves with `{ isPlaying }`, whether playback runs after the toggle.

```javascript
const res = await fb.player.playPause();
if (res.success) console.log(res.isPlaying ? 'Playing' : 'Paused');
```

## toggleMute()

Signature: `fb.player.toggleMute(): Promise<PlaybackToggleMuteResponse>`

Toggles mute state and resolves with `{ muted }` indicating the resulting status. A `playback:volumeChanged` event is broadcast afterwards carrying `muted`.

```javascript
const res = await fb.player.toggleMute();
if (res.success) console.log(res.muted ? 'Muted' : 'Unmuted');
```

## toggleStopAfterCurrent()

Signature: `fb.player.toggleStopAfterCurrent(): Promise<PlaybackToggleStopAfterCurrentResponse>`

Toggles the "stop after current track" setting and resolves with `{ enabled }` indicating the active status. State changes broadcast `playback:stopAfterCurrentChanged` carrying `{ enabled }`.

```javascript
const res = await fb.player.toggleStopAfterCurrent();
if (res.success) console.log(res.enabled ? 'Stops after this track' : 'Keeps playing');
```

## PlaybackClock

`import { PlaybackClock } from 'foo-webview-sdk/bridge'`, or `new fb.PlaybackClock()` with the `<script>` bundle.

Interpolates real-time playback position between discrete host events, designed for high-refresh rendering tasks such as progress bars, word-timed lyrics, and muted companion videos. The host attaches `hostTime`, the system time at which it read the position, to each sample; the clock uses it to tell how old a sample is on arrival and advances from there with `performance.now()`.

- Tracks `playback:timeHighRes`, `playback:seeked`, `playback:stateChanged`, and `playback:trackChanged` events. It samples `playback.getState` and `playback.getPosition` upon creation, when the page becomes visible, and on `resync()`. `ready` resolves once initial synchronization completes and never rejects.
- `resync()` ignores read failures by default. When playback resumption requires a verified fresh snapshot, use `resync({ rejectOnFailure: true })`: both calls must succeed before readings are applied. Host failures reject with `ApiCallError` preserving the native `code`; bridge transport errors also reject. If concurrent requests prevent acquiring a complete snapshot, strict synchronization may reject and should be handled before continuing playback.
- `position(at?)` returns the estimated position in seconds at the given `performance.now()` timestamp (defaulting to now), bounded by `duration` when known. The value remains static while `state` is `paused` and returns `0` when `stopped`.
- When deviation between incoming events and current estimation is small, the clock moves its estimate by a quarter of the difference to prevent visual jitter caused by host sampling noise. If deviation exceeds `resyncThresholdSeconds` (0.25 s by default), the clock immediately snaps to the reported position. Delayed updates arriving immediately after a seek are discarded.
- `onChange(listener)` notifies listeners of non-continuous jumps: `start` upon initial readiness, `seek`, `state`, `track`, and `resync` following manual synchronization or threshold realignment. Muted companion videos align timestamps on these transitions and adjust playback rates during steady playback.
- Updates delayed past `maxDeliveryDelayMs` (1000 ms by default) or bearing anomalous timestamps caused by system clock adjustments are ignored. If the host omits `hostTime`, updates fall back to arrival time as the sampling baseline, introducing a small latency equal to transport delay.
- Call `dispose()` when the instance is no longer needed.

```javascript
import { PlaybackClock } from 'foo-webview-sdk/bridge';

const clock = new PlaybackClock();
await clock.ready;
const bar = document.querySelector('#progress');
const draw = () => {
    if (bar instanceof HTMLElement && clock.duration > 0) {
        bar.style.width = `${(clock.position() / clock.duration) * 100}%`;
    }
    requestAnimationFrame(draw);
};
requestAnimationFrame(draw);
const off = clock.onChange((change) => console.log(change.reason, change.position));
// Cleanup
off();
clock.dispose();
```
