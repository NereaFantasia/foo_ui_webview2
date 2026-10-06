# fb.jitQueue Just-in-Time Queue

`fb.jitQueue` controls the streaming preload queue used for adaptive playback. It is separate from `fb.queue`, which represents the foobar2000 playback queue. Tracks are identified by a caller-assigned `trackId` together with a `title` and a `url`, not by playlist indices.

## getState()

Signature: `fb.jitQueue.getState(): Promise<JitQueueGetStateResponse>`

Returns the current queue state, including `isActive`, `state`, `currentTrackId`, `nextTrackId`, `bufferSize`, and `shadowPlaylist`. `state` is one of `Idle`, `Active`, `WaitingNext`, `Exhausted`, and `Unknown`; `shadowPlaylist` is `-1` until the shadow playlist has been created.

```javascript
const state = await fb.jitQueue.getState();
```

## enqueueNext(opts)

Signature: `fb.jitQueue.enqueueNext(opts: JitQueueEnqueueNextParams): Promise<JitQueueEnqueueNextResponse>`

Queues the next adaptive-playback item, usually in answer to `jitQueue:needNext`. `opts` accepts `trackId`, `title`, and `url`. URLs longer than 2048 characters resolve with `success: false` and the code `INVALID_PARAMS`. The call is refused with `NO_ACTIVE_ITEM` unless a session is playing or waiting for its next track.

The response carries the `trackId` given and `bufferSize`, the number of tracks buffered afterwards. The track is added after the call has returned, so a track that cannot be added is reported only through `jitQueue:error`.

```javascript
await fb.jitQueue.enqueueNext({
	trackId: 'track-42',
	title: 'Next track',
	url: 'https://media.example.com/next.flac'
});
```

## playNow(opts)

Signature: `fb.jitQueue.playNow(opts: JitQueuePlayNowParams): Promise<JitQueuePlayNowResponse>`

Starts the supplied item immediately. It accepts the same `trackId`, `title`, and `url` fields as `enqueueNext()`, including the 2048-character URL limit.

Each call starts a new session: the buffer is cleared and playback begins. The host detects the kind of `url` itself: `http(s)://` addresses are streams, Windows and UNC paths are local files. The response carries the `trackId` given and `shadowPlaylist`, the shadow playlist's index. As with `enqueueNext()`, a track that cannot be added is reported through `jitQueue:error`.

```javascript
await fb.jitQueue.playNow({
	trackId: 'track-41',
	title: 'Current track',
	url: 'https://media.example.com/current.flac'
});
```

## clear()

Signature: `fb.jitQueue.clear(): Promise<JitQueueClearResponse>`

Clears the buffered just-in-time queue and returns the session to idle.

```javascript
await fb.jitQueue.clear();
```

## notifyEmpty()

Signature: `fb.jitQueue.notifyEmpty(): Promise<JitQueueNotifyEmptyResponse>`

Notifies the host that the producer has no more items to enqueue. The session moves to `Exhausted` and the host emits `jitQueue:listExhausted`, even when no session is active.

```javascript
await fb.jitQueue.notifyEmpty();
```

## preloadBatch(opts)

Signature: `fb.jitQueue.preloadBatch(opts: JitQueuePreloadBatchParams): Promise<JitQueuePreloadBatchResponse>`

| Field | Type | Description |
| --- | --- | --- |
| `urls` | `string[]` | Track URLs or `path\|subsong:N` values to insert |
| `startIndex` | `number` | Optional entry to start playback from when the session is not already playing; defaults to `0` |
| `replace` | `boolean` | Whether to replace the existing preload list; `false` appends. Defaults to `true` |

Inserts a batch of tracks into the shadow playlist. Each URL is limited to 2048 characters; longer entries are skipped and included in `invalidCount`, and at most 10000 entries may remain.

The response carries `tracksAdded` and `invalidCount`. `jitQueue:preloadComplete` is sent before the call returns; a failed call sends nothing.

```javascript
const result = await fb.jitQueue.preloadBatch({
	urls: ['https://media.example.com/1.flac', 'https://media.example.com/2.flac'],
	startIndex: 0,
	replace: true
});
```

## skip()

Signature: `fb.jitQueue.skip(): Promise<JitQueueSkipResponse>`

Skips to the next buffered item. The response carries `currentTrackId`, the track playing after the skip. The call is refused with `NO_ACTIVE_ITEM` while no session is active.

```javascript
const result = await fb.jitQueue.skip();
```

## stop(opts?)

Signature: `fb.jitQueue.stop(opts?: JitQueueStopParams): Promise<JitQueueStopResponse>`

Stops just-in-time playback. `opts.clearBuffer` also empties the buffer and defaults to `true`; pass `{ clearBuffer: false }` to keep the buffered tracks.

```javascript
await fb.jitQueue.stop();
await fb.jitQueue.stop({ clearBuffer: false });
```

## Events

Subscribe through `fb.on()` using colon-separated event names:

- `jitQueue:needNext` — `{ currentTrackId, reason }`
- `jitQueue:trackChanged` — `{ trackId, title }`
- `jitQueue:listExhausted` — `{ lastTrackId }`
- `jitQueue:preloadComplete` — `{ count, startIndex, replace }`
- `jitQueue:error` — `{ trackId, error, url? }` or `{ trackId, error, path? }`

The payload types (`JitQueueNeedNextPayload`, `JitQueueTrackChangedPayload`, `JitQueueListExhaustedPayload`, `JitQueuePreloadCompletePayload`, `JitQueueErrorPayload`) are exported from the package root.

The events go to the page that started the session, with `playNow()` or with a `preloadBatch()` that started playback. `jitQueue:needNext` fires when no next track is buffered and no earlier request is still waiting for an answer; answer it with `enqueueNext()`, or with `notifyEmpty()` when there are no more tracks.

```javascript
const off = fb.on('jitQueue:needNext', ({ currentTrackId, reason }) => {
	console.log(currentTrackId, reason);
});
```
