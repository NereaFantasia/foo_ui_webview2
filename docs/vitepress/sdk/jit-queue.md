# fb.jitQueue Just-in-Time Queue

`fb.jitQueue` controls the streaming preload queue used for adaptive playback. It is separate from `fb.queue`, which represents the foobar2000 playback queue.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

### getState()

Signature: `fb.jitQueue.getState(): Promise<JitQueueStateInfo>`

Returns the current queue state, including `isActive`, `state`, `currentTrackId`, `nextTrackId`, `bufferSize`, and `shadowPlaylist`.

```javascript
const state = await fb.jitQueue.getState();
```

### enqueueNext(opts)

Signature: `fb.jitQueue.enqueueNext(opts: JitQueueEnqueueNextParams): Promise<BaseResponse & { bufferSize?: number }>`

Queues the next adaptive-playback item. `opts` accepts `trackId`, `title`, and `url`. URLs longer than 2048 characters resolve with `success: false`.

```javascript
await fb.jitQueue.enqueueNext({
	trackId: 'track-42',
	title: 'Next track',
	url: 'https://media.example.com/next.flac'
});
```

### playNow(opts)

Signature: `fb.jitQueue.playNow(opts: JitQueuePlayNowParams): Promise<BaseResponse & { shadowPlaylist?: number }>`

Starts the supplied item immediately. It accepts the same `trackId`, `title`, and `url` fields as `enqueueNext()`, including the 2048-character URL limit.

```javascript
await fb.jitQueue.playNow({
	trackId: 'track-41',
	title: 'Current track',
	url: 'https://media.example.com/current.flac'
});
```

### clear()

Signature: `fb.jitQueue.clear(): Promise<BaseResponse>`

Clears the buffered just-in-time queue.

```javascript
await fb.jitQueue.clear();
```

### notifyEmpty()

Signature: `fb.jitQueue.notifyEmpty(): Promise<BaseResponse>`

Notifies the host that the producer has no more items to enqueue.

```javascript
await fb.jitQueue.notifyEmpty();
```

### preloadBatch(opts)

Signature: `fb.jitQueue.preloadBatch(opts: JitQueuePreloadBatchParams): Promise<BaseResponse & { tracksAdded?: number; invalidCount?: number }>`

| Field | Type | Description |
| --- | --- | --- |
| `urls` | `string[]` | URLs to preload |
| `startIndex` | `number` | Optional starting index |
| `replace` | `boolean` | Whether to replace the existing preload list; defaults to `true` |

Each URL is limited to 2048 characters. Invalid entries are skipped and included in `invalidCount`.

```javascript
const result = await fb.jitQueue.preloadBatch({
	urls: ['https://media.example.com/1.flac', 'https://media.example.com/2.flac'],
	startIndex: 0,
	replace: true
});
```

### skip()

Signature: `fb.jitQueue.skip(): Promise<BaseResponse & { currentTrackId?: string }>`

Skips to the next buffered item.

```javascript
const result = await fb.jitQueue.skip();
```

### stop()

Signature: `fb.jitQueue.stop(): Promise<BaseResponse>`

Stops just-in-time playback. The SDK facade does not expose the host's optional `clearBuffer` argument, so the host default applies.

```javascript
await fb.jitQueue.stop();
```

## Events

Subscribe through `fb.on()` using colon-separated event names:

- `jitQueue:needNext` — `{ currentTrackId, reason }`
- `jitQueue:trackChanged` — `{ trackId, title }`
- `jitQueue:listExhausted` — `{ lastTrackId }`
- `jitQueue:preloadComplete` — `{ count, startIndex, replace }`
- `jitQueue:error` — `{ trackId, error, url? }` or `{ trackId, error, path? }`

<!-- END AUTO-GENERATED SDK STUBS -->
