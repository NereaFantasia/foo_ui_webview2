# `fb.player` playback control

## Endpoint mapping

`fb.player` method names are SDK-side aliases and do not always match the underlying `fb2k.invoke` endpoint. Use this table when calling the bridge directly, or when a `Method not found` error names something like `playback.prev`.

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

Returns `PlaybackGetCurrentTrackResponse`: `found` says whether a track is loaded, and `track`, the shared [Track](../reference/types.md#track) row, is present only then. The host never answers `null`.

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

Returns the current position and duration in seconds, plus path/subsong information when exposed by the host. `hostTime` is the system time the host read the position at, in Unix milliseconds on the clock `Date.now()` reads: while playing, `pos.position + (Date.now() - pos.hostTime) / 1000` is where playback is now. [`PlaybackClock`](#playbackclock) keeps that estimate up to date.

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

Reports the playlist playback was last started from: `found`, `playlist` (its index), `playlistGuid` and `name`. When none is known, `found` is `false`, `playlist` and `playlistGuid` are `null` and `name` is absent. foobar2000 keeps the pointer after playback stops, so a stopped instance that has played before still reports one.

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

Flips the mute state and resolves with `{ muted }`, whether output is muted afterwards. `playback:volumeChanged` follows, its payload carrying `muted`.

```javascript
const res = await fb.player.toggleMute();
if (res.success) console.log(res.muted ? 'Muted' : 'Unmuted');
```

## toggleStopAfterCurrent()

Signature: `fb.player.toggleStopAfterCurrent(): Promise<PlaybackToggleStopAfterCurrentResponse>`

Flips "stop after current" and resolves with `{ enabled }`, the value afterwards. The change broadcasts `playback:stopAfterCurrentChanged` with `{ enabled }`.

```javascript
const res = await fb.player.toggleStopAfterCurrent();
if (res.success) console.log(res.enabled ? 'Stops after this track' : 'Keeps playing');
```

## PlaybackClock

`import { PlaybackClock } from 'foo-webview-sdk/bridge'`, or `new fb.PlaybackClock()` with the `<script>` bundle.

Estimates where foobar2000 is in the playing track at any moment, for things that move with the audio between position updates: a progress bar, word-timed lyrics, a muted `<video>` that follows playback. The host stamps every position with `hostTime`, the system time it read the position at, so the clock knows how old a position is when it arrives and moves on from there with `performance.now()`.

- It follows `playback:timeHighRes`, `playback:seeked`, `playback:stateChanged` and `playback:trackChanged`, and reads `playback.getState` and `playback.getPosition` when created, when the page becomes visible again, and on `resync()`. `ready` resolves once the first read settled and never rejects.
- `resync()` ignores read failures by default. Use `resync({ rejectOnFailure: true })` when playback must wait for a fresh snapshot: both calls must succeed before either answer is applied. A host failure rejects with `ApiCallError`, preserving its `code`; bridge failures also reject. An overlapping read that prevents a complete fresh snapshot can reject the strict request, so handle failure before resuming media.
- `position(at?)` is the estimate in seconds at `at` on the `performance.now()` scale, now when omitted, kept within `duration` when the length is known. It does not move while `state` is `paused`, and is `0` while `stopped`.
- An update that disagrees with the estimate by a little moves it by a quarter of the difference, so the position does not stutter on the host's reading jitter. A difference above `resyncThresholdSeconds` (0.25 by default) replaces the estimate. Right after a seek, updates that still carry the old position are ignored.
- `onChange(listener)` reports every jump: `start` for the first read, `seek`, `state`, `track`, and `resync` for a replaced estimate or a `resync()` read. A muted video that follows playback seeks on these and otherwise only adjusts its rate.
- Updates older than `maxDeliveryDelayMs` (1000 by default), or that look older or newer than possible because the system time was changed in between, are ignored. With a host that does not send `hostTime`, updates count as read when they arrive, so the estimate trails the audio by the delivery delay.
- Call `dispose()` when done.

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
// later
off();
clock.dispose();
```
