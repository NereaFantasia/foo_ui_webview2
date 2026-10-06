# JIT Queue API

Methods of the `jitQueue` namespace.

## jitQueue

### jitQueue.clear

<!-- api-schema:begin jitQueue.clear -->
Empty the buffer and return the session to idle.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('jitQueue.clear');
```

### jitQueue.enqueueNext

<!-- api-schema:begin jitQueue.enqueueNext -->
Preload the next track into the buffer, in answer to `jitQueue:needNext`. Refused with `NO_ACTIVE_ITEM` unless a JIT session is playing or waiting for its next track.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `trackId` | `string` | Yes | Caller-assigned identifier of the track. Must not be empty. |
| `title` | `string` | No | Display title. Default: `""`. |
| `url` | `string` | Yes | Stream URL or file path. At most 2048 characters. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `trackId` | `string` | The identifier given. |
| `bufferSize` | `integer` | Tracks in the buffer afterwards. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('jitQueue.enqueueNext', { trackId: 'track-2', url: 'https://example.com/next.mp3' });
```

### jitQueue.getState

<!-- api-schema:begin jitQueue.getState -->
Report the JIT session state.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `isActive` | `boolean` | Whether a JIT session is active. |
| `state` | `"Idle" \| "Active" \| "WaitingNext" \| "Exhausted" \| "Unknown"` | The session state. |
| `currentTrackId` | `string` | Identifier of the playing track; empty when none. |
| `nextTrackId` | `string` | Identifier of the buffered next track; empty when none. |
| `bufferSize` | `integer` | Tracks in the buffer. |
| `shadowPlaylist` | `integer` | Index of the shadow playlist; `-1` until a `jitQueue.playNow` or `jitQueue.preloadBatch` of this foobar2000 session creates or takes it over, even when one is left from an earlier session, and again after the playlist is removed, which also ends the JIT session. Stopping or clearing keeps the index of the emptied playlist. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('jitQueue.getState');
```

### jitQueue.notifyEmpty

<!-- api-schema:begin jitQueue.notifyEmpty -->
Tell the host the page has no more tracks to offer; the session moves to `Exhausted` and `jitQueue:listExhausted` is emitted.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('jitQueue.notifyEmpty');
```

### jitQueue.playNow

<!-- api-schema:begin jitQueue.playNow -->
Start a new JIT session with this track: the buffer is cleared and playback begins. The host detects the URL kind itself (`http(s)://` streams, Windows or UNC paths are local).

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `trackId` | `string` | Yes | Caller-assigned identifier of the track, echoed in the session state and events. Must not be empty. |
| `title` | `string` | No | Display title. Default: `""`. |
| `url` | `string` | Yes | Stream URL or file path. At most 2048 characters. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `trackId` | `string` | The identifier given. |
| `shadowPlaylist` | `integer` | Index of the shadow playlist, which the call creates or takes over when this session has none; `-1` only if foobar2000 could not create it. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('jitQueue.playNow', { trackId: 'track-1', url: 'https://example.com/stream.mp3' });
```

### jitQueue.preloadBatch

<!-- api-schema:begin jitQueue.preloadBatch -->
Insert a batch of tracks into the shadow playlist, starting playback from `startIndex` when the session is not already playing.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `urls` | `string[]` | No | Track URLs or `path\|subsong:N` values. Entries longer than 2048 characters are dropped and counted in `invalidCount`; at most 10000 entries may remain. |
| `startIndex` | `integer` | No | Which entry to start playing from. At least `0`. Default: `0`. |
| `replace` | `boolean` | No | Empty the shadow playlist before inserting. `false` appends. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tracksAdded` | `integer` | How many tracks were inserted. |
| `invalidCount` | `integer` | How many entries were dropped for being too long. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Preloads a batch of tracks into the JIT shadow playlist.

`replace: true` is the default and empties the shadow playlist before inserting, so pass `false` to add tracks without disturbing what is already queued. An entry that is not a string is refused with `INVALID_PARAMS`; entries longer than 2048 characters are dropped and counted in `invalidCount` rather than failing the call.

The 10000 limit is applied *after* those invalid entries are dropped, so it counts valid entries only. Exceeding it fails the whole batch — the list is never truncated — with `INVALID_PARAMS` (`Batch exceeds maximum size (10000)`) and no `tracksAdded`. An empty `urls` array or an out-of-range `startIndex` also fails with `INVALID_PARAMS`, carrying `tracksAdded: 0` and `invalidCount`; a host-side failure while inserting is `OPERATION_FAILED`.

```js
// replace the shadow playlist
await fb2k.invoke('jitQueue.preloadBatch', { urls, startIndex: 0 });
// append without disturbing playback
await fb2k.invoke('jitQueue.preloadBatch', { urls: moreUrls, replace: false });
```

### jitQueue.skip

<!-- api-schema:begin jitQueue.skip -->
Skip to the next buffered track. Refused with `NO_ACTIVE_ITEM` while no JIT session is active.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `currentTrackId` | `string` | Identifier of the track playing after the skip. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('jitQueue.skip');
```

### jitQueue.stop

<!-- api-schema:begin jitQueue.stop -->
Stop playback, optionally keeping the buffer.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `clearBuffer` | `boolean` | No | Also empty the buffer. Default: `true`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// stop and keep the preloaded buffer
await fb2k.invoke('jitQueue.stop', { clearBuffer: false });
```

## JIT Queue events

These events are emitted while the JIT shadow playlist is maintained. Subscribe before issuing operations when the frontend needs to refill or observe that buffer.

| Event | Meaning | Payload keys |
| --- | --- | --- |
| `jitQueue:needNext` | The manager needs the next logical track. | `{ currentTrackId, reason }` |
| `jitQueue:trackChanged` | The JIT current track changed. | `{ trackId, title }` |
| `jitQueue:listExhausted` | No further tracks are available — emitted on end of playback with an empty buffer, or after `jitQueue.notifyEmpty`. | `{ lastTrackId }` |
| `jitQueue:preloadComplete` | A batch preload completed. | `{ count, startIndex, replace }` |
| `jitQueue:error` | A JIT operation failed for a track. | `{ trackId, error }` plus **exactly one** of `url` (streaming source) or `path` (local file) |
