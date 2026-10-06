# Queue API

English API reference for the `jitQueue`, `queue`, `selection` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## jitQueue

### jitQueue.clear


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('jitQueue.clear');
```

### jitQueue.enqueueNext


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `trackId` | `string` | Yes | Caller-assigned track identifier. An empty value fails with `trackId is required`. |
| `url` | `string` | Yes | Stream or file URL. An empty value fails with `url is required`. |
| `title` | `string` | No | Optional display title. |

**Returns**: `{"bufferSize":"...","error":"...","success":true,"trackId":"..."}`

```js
await fb2k.invoke('jitQueue.enqueueNext', { trackId: 'track-2', url: 'https://example.com/next.mp3' });
```

### jitQueue.getState


_No parameters._

**Returns**: `{"bufferSize":"...","currentTrackId":"...","isActive":"...","nextTrackId":"...","shadowPlaylist":"...","state":"..."}`

```js
const result = await fb2k.invoke('jitQueue.getState');
```

### jitQueue.notifyEmpty


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('jitQueue.notifyEmpty');
```

### jitQueue.playNow


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `trackId` | `string` | Yes | Caller-assigned track identifier. An empty value fails with `trackId is required`. |
| `url` | `string` | Yes | Stream or file URL. An empty value fails with `url is required`. |
| `title` | `string` | No | Optional display title. |

**Returns**: `{"error":"...","shadowPlaylist":"...","success":true,"trackId":"..."}`

```js
await fb2k.invoke('jitQueue.playNow', { trackId: 'track-1', url: 'https://example.com/stream.mp3' });
```

### jitQueue.preloadBatch

Preloads a batch of tracks into the JIT shadow playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `urls` | `array<string>` | Yes | Track URLs or `path\|subsong:N` values. At most 10000 valid entries per call. |
| `startIndex` | `integer` | No | Starting playback position. Default `0`. |
| `replace` | `boolean` | No | Default `true`, which **clears the shadow playlist first**. Pass `false` to append. |

**Returns**: `{ "success": true, "tracksAdded": 2 }`, plus `invalidCount` only when one or more entries were rejected.

`replace: true` is the default and empties the shadow playlist before inserting, so pass `false` to add tracks without disturbing what is already queued. Entries that are not strings, or longer than 2048 characters, are dropped and counted in `invalidCount` rather than failing the call.

The 10000 limit is applied *after* those invalid entries are dropped, so it counts valid entries only. Exceeding it fails the whole batch — the list is never truncated — and that particular failure returns `{ "success": false, "error": "Batch exceeds maximum size (10000)" }` with no `tracksAdded`. Other failures, such as an empty `urls` array or an out-of-range `startIndex`, return `{ "success": false, "tracksAdded": 0, "error": "..." }`.

```js
// replace the shadow playlist
await fb2k.invoke('jitQueue.preloadBatch', { urls, startIndex: 0 });
// append without disturbing playback
await fb2k.invoke('jitQueue.preloadBatch', { urls: moreUrls, replace: false });
```

### jitQueue.skip


_No parameters._

**Returns**: `{"currentTrackId":"...","success":true}`

```js
const result = await fb2k.invoke('jitQueue.skip');
```

### jitQueue.stop


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `clearBuffer` | `boolean` | No | `true` | Also clears the buffer when stopping. |

**Returns**: `{"success":true}`

```js
// stop and keep the preloaded buffer
await fb2k.invoke('jitQueue.stop', { clearBuffer: false });
```

## queue

### queue.add

Queues one or more tracks by their position in a playlist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `tracks` | `array<integer>` | No | — | Track indices. Takes precedence over `track`. |
| `track` | `integer` | No | — | A single track index. Used only when `tracks` is absent. |

**Returns**: `{ "success": true, "addedCount": 2, "queueCount": 5 }`

Supply either `tracks` or `track`, not both — when `tracks` is an array it wins and `track` is ignored. `success` is simply `addedCount > 0`. Out-of-range track indices are skipped silently, so a call that matches nothing returns `success: false` with `addedCount: 0` and **no** `error` field; the same is true for an empty `tracks` array. An invalid `playlist` returns `{ "success": false, "error": "Invalid playlist index" }`. Entries in `tracks` must be numbers — a non-numeric element is a parameter type error rather than a skipped entry.

```js
// queue several tracks from the active playlist
await fb2k.invoke('queue.add', { tracks: [0, 1, 2] });
// queue a single track
await fb2k.invoke('queue.add', { track: 0 });
```

### queue.addPaths

Queues tracks by file path, adding them to a playlist first.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array<string>` | Yes | File paths, optionally with a `\|subsong:N` suffix. |
| `useQueuePlaylist` | `boolean` | No | Default `true`, which targets a dedicated `[WebView Queue]` playlist, creating it if needed. |
| `playlist` | `integer` | No | Target playlist index. Read only when `useQueuePlaylist` is `false`. |

**Returns**: `{ "success": true, "addedCount": 2, "invalidCount": 0, "playlist": 3, "queueCount": 5 }`

Because queueing requires playlist membership, this method appends the paths to a playlist and then queues them. By default that target is a dedicated `[WebView Queue]` playlist rather than your active one. Setting `useQueuePlaylist: false` targets `playlist` instead, or the active playlist when `playlist` is also omitted — which fails with `No active playlist` if there is none.

A locked target playlist is rejected with `{ "success": false, "error": "Playlist is locked", "isLocked": true, "playlist": N }`. When nothing resolves, the response carries `invalidCount` alongside the error. Other failures return `{ "success": false, "error": "..." }`: an empty `paths` array or an invalid `playlist`.

```js
await fb2k.invoke('queue.addPaths', { paths: ['C:\\Music\\a.flac'] });
```

### queue.clear


_No parameters._

**Returns**: `{"clearedCount":"...","success":true}`

```js
const result = await fb2k.invoke('queue.clear');
```

### queue.flush


_No parameters._

**Returns**: `{"clearedCount":"...","success":true}`

```js
const result = await fb2k.invoke('queue.flush');
```

### queue.get

Returns the entire playback queue.

_No parameters._

**Returns**: `{ "items": [...], "count": 5 }`

There is no paging and no `success` field; the whole queue is always returned. Each entry carries `queueIndex`, `path`, `absolutePath`, `subsong`, `fileSize`, the usual metadata fields, and the originating `playlist` and `playlistItem`. Both position fields are always present: exact integers for an entry that has a playlist position, and both `null` for an entry that has none — one queued through the `paths` of `queue.insertNext`, or one whose referenced playlist item has since been removed. `item.playlist == null` is the whole test.

```js
const { items, count } = await fb2k.invoke('queue.get');
```

### queue.getCount

Returns the queue length.

_No parameters._

**Returns**: `{ "count": 5, "hasItems": true }`

No `success` field. `hasItems` is exactly `count > 0`, provided as a convenience.

```js
const { count } = await fb2k.invoke('queue.getCount');
```

### queue.moveToTop

Moves a queued entry to the front of the queue.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | Yes | Queue index to promote. Must not already be `0`. |

**Returns**: `{ "success": true, "movedIndex": 3, "queueCount": 5 }`

Only queue order changes — playlist membership is untouched. The relative order of the remaining entries is preserved. A missing `index`, an out-of-range value, an entry already at the top, or an empty queue all return `{ "success": false, "error": "Invalid index or already at top" }`.

```js
await fb2k.invoke('queue.moveToTop', { index: 3 });
```

### queue.remove

Removes one or more entries from the queue.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | A single queue index. Takes precedence over `indices`. |
| `indices` | `array<integer>` | No | Multiple queue indices. Used only when `index` is absent. |

**Returns** — single: `{ "success": true, "removedIndex": 2, "queueCount": 4 }`. Batch: `{ "success": true, "removedCount": 3, "queueCount": 2 }`.

Supply either `index` or `indices`, not both — `index` wins when present. Duplicate and out-of-range values in `indices` are skipped, and a batch that matches nothing returns `success: false` with **no** `error`. An empty queue, an out-of-range `index`, or neither field returns `{ "success": false, "error": "..." }`.

```js
await fb2k.invoke('queue.remove', { indices: [0, 2] });
```

### queue.setContents

Replaces the entire queue with an ordered list of references.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `array<{ queueIndex: number } \| { playlist: number, item: number }>` | Yes | Ordered references — `{ queueIndex }` keeps/reorders an existing queue slot, `{ playlist, item }` adds a playlist track. |

**Returns**: `{ "success": true, "queueCount": 5 }`

An entry that matches neither shape fails the whole call before anything is written — unlike `queue.add`, which skips bad entries individually, any single invalid reference here rejects the entire call and leaves the queue untouched. `queueCount` is present on every response, success or failure. Passing an empty array clears the queue, equivalent to `queue.clear`. `items.length` is capped at `max(256, current queue length)`; exceeding it fails without changing the queue.

::: tip Call pattern for drag-to-reorder UIs
Call `setContents` once when the user releases the drag, not on every intermediate frame — each call flushes and rebuilds the whole queue, so per-frame calls during a drag gesture are wasted work and can visibly stutter.
:::

```js
// Reorder: move queue slot 2 to the front, keep the rest in order
await fb2k.invoke('queue.setContents', {
  items: [{ queueIndex: 2 }, { queueIndex: 0 }, { queueIndex: 1 }],
});
// Clear the queue
await fb2k.invoke('queue.setContents', { items: [] });
```

### queue.insertNext

Inserts tracks so they play next, ahead of everything already queued. Entries are given as file paths, as playlist positions, or both.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array<string>` | No¹ | File paths or URLs, optionally with a `\|subsong:N` suffix. The resulting entries carry no playlist position. |
| `items` | `array<{ playlist, item }>` | No¹ | Playlist positions, in the shape `queue.setContents` accepts. The resulting entries carry that position, so the playback cursor follows them. |
| `position` | `integer` | No | Insertion index within the queue *after* any moved entries are removed. Defaults to `0` (the front). |

¹ At least one of `paths` and `items` must be non-empty.

**Returns**: `{ "success": true, "insertedCount": 1, "movedCount": 1, "queueCount": 5, "invalidCount": 0 }`

A track already present in the queue is moved to `position` instead of being queued a second time; only one occurrence moves, selected by the coordinate rules below. Within one call the `items` block lands first and the `paths` block after it; each block keeps its own order. `insertedCount` counts newly queued tracks and `movedCount` counts relocated entries. `invalidCount` is the input path count minus the number of tracks resolved from paths, floored at zero. It is not an exact count of failed paths: folder or container expansion can offset failures. `items` never counts toward `invalidCount`: one bad entry, such as a non-object, missing `playlist` or `item`, or an out-of-range position, fails the whole call with an `error` naming `items[i]`. Nothing is written, and the `paths` of the same call are not queued either.

All input entries are deduplicated by track, not by position. For duplicate `items`, the first supplied coordinate already present in the queue is retained; if none matches, the first supplied coordinate is used. The track keeps its first-occurrence order within the input block. An existing entry at that coordinate moves first; otherwise the first queued entry for the track moves and takes the supplied coordinate. A path-only move preserves the existing entry's usable coordinate; only a newly inserted path lacks one. `queue.add` and `queue.setContents` preserve duplicate references. A lower `insertedCount + movedCount` can reflect deduplication or invalid paths; folders and containers can also expand to multiple tracks.

Both arrays empty or absent fails with `{ "success": false, "error": "No paths or items specified" }`; `items` that is not an array fails with `"items must be an array"`; `paths` resolving to zero valid tracks with no `items` fails with `{ "success": false, "error": "No valid tracks found", "invalidCount": ... }`.

::: warning Known limitation: legacy `|subsong:N` matching
A track that entered the queue through the `path|subsong:N` suffix accepted by `queue.addPaths` may fail to match as "already in the queue" when passed to `insertNext` again — it may be queued a second time instead of moved. This only affects tracks queued before upgrading; new calls are unaffected. A bare path to a multi-subsong file and that same path with an explicit `|subsong:0` suffix are also treated as distinct identities rather than equivalent — do not assume they deduplicate against each other.
:::

::: warning Known limitation: the playlist cursor does not follow a track inserted by path
Entries added through `paths` carry no playlist position (they are not written into any playlist, which is what keeps your playlists clean). foobar2000 therefore treats them as played *outside* the playlist: while such an entry plays, `playback.getCurrentTrackIndex` reports no position, and once it finishes playback resumes from the item **after the one that was playing before the queue was consumed**, not from the inserted track's own position. Two visible consequences when the inserted track also lives in the playing playlist: the sequence continues where it left off (e.g. item 8 → queued item → item 9), and the inserted track plays **again** when the sequence reaches it later. Entries added through `items`, like `queue.add({ playlist, tracks })` entries, carry a position and do not have this behaviour — the cursor jumps to the consumed entry and continues from there. When you know where the track sits in a playlist, pass `items`. Existing entries whose playlist position has become invalid (the referenced playlist was cleared) fall back to the same position-less form after `moveToTop` / `setContents` / `playNow` rebuild the queue.

`playback.previous` is affected the same way. foobar2000 resolves "previous" from the cursor position in playlist order, not from playback history, and a consumed queue entry is never put back. While a position-less entry plays, `previous` lands on the item **before** the one that was interrupted (the interrupted track itself is skipped); while a positioned entry plays, it lands on the item before that entry's own playlist position. Neither returns to the interrupted track.
:::

```js
// By path: the entry carries no playlist position
await fb2k.invoke('queue.insertNext', { paths: ['C:\\Music\\a.flac'], position: 0 });
// By playlist position: the cursor follows the entry once it plays
await fb2k.invoke('queue.insertNext', { items: [{ playlist: 0, item: 12 }] });
```

### queue.playNow

Plays the queue entry at `index` immediately, promoting it to the front of the queue first if it is not already there.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | Queue index to play. Defaults to `0` (the current queue head). |

**Returns**: `{ "success": true, "playedIndex": 0, "queueCount": 4 }`

Fails with `{ "success": false, "error": "Queue is empty" }` when nothing is queued, or `{ "success": false, "error": "Invalid queue index" }` for a negative, non-integer, or out-of-range `index`.

::: warning `queueCount` timing is not guaranteed
`queueCount` is read immediately after playback starts. The host does not guarantee that its consumption of the queue head happens synchronously with that read, so the value may reflect the queue either just before or just after the played entry is removed. Call `queue.getCount` afterward if the exact post-play length matters.
:::

```js
// Play whatever is at the head of the queue
await fb2k.invoke('queue.playNow');
// Play the 3rd entry, promoting it to the head first
await fb2k.invoke('queue.playNow', { index: 2 });
```

### Explicit playback clears the queue

`playlist.playTrack`, `playlist.replaceAllAndPlay`, `playback.playPath`, `playback.playPaths`, and `jitQueue.preloadBatch` when it starts playback (first call, or `replace: true`) all go through foobar2000's default-action path, which **flushes the whole playback queue first**. This is core behaviour, not something the component adds: the core implements "play this item" as *clear the queue, enqueue the target, consume it*, which is why you observe three `playback:queueChanged` events in a row (`user_removed` with `count: 0`, `user_added`, `playback_advance`). None of these calls can be asked to keep the queue.

**Play a playlist item while keeping the queue.** Put the target at the head with `setContents`, then consume it with `playNow`:

```js
const { count } = await fb2k.invoke('queue.getCount');
await fb2k.invoke('queue.setContents', {
  items: [
    { playlist, item },                                            // the track to play now
    ...Array.from({ length: count }, (_, i) => ({ queueIndex: i })), // existing entries, in order
  ],
});
await fb2k.invoke('queue.playNow'); // consumes the head, leaves the rest queued
```

After the count query, this uses two calls and one queue rebuild. The played track enters the queue with a playlist position, so the cursor follows it and the "cursor does not follow" limitation above does not apply.

**Restore a queue that was cleared.** The host keeps no record of flushed entries, so snapshot `queue.get` *before* the call that starts playback. Entries whose `playlist` is not `null` can be added through `setContents` or `insertNext({ items })`; entries with `playlist: null` need `insertNext({ paths })`. Both forms can share one `insertNext` call, but it groups coordinates before paths and deduplicates by track. Use `setContents` afterwards to restore ordering and duplicate counts, not necessarily the original coordinate forms: if one track originally had both a coordinate entry and a coordinate-less entry, copying its remaining `queueIndex` also copies that entry's coordinate. Recreating every entry by path loses playlist coordinates and may collapse duplicates.

## selection

### selection.get

Reads the current selection. This method observes state only and never modifies a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `offset` | `integer` | No | Starting index. Default `0`. |
| `limit` | `integer` | No | Maximum entries to return. Defaults to `100`; pass `0` for all. |

**Returns**: `{ "count": 250, "type": "...", "handles": [...], "offset": 0, "hasMore": true }`, plus `truncated: true` when the result was auto-capped.

This method returns **no** `success` field and has no failure branch. The `100` cap applies only when `limit` is omitted — an explicit `limit` is honored as given, and `limit: 0` means "no limit", returning every entry from `offset` onward.

`truncated: true` appears only when the automatic cap was actually applied, that is when the selection exceeds 100 **and** `limit` was omitted. Asking for `limit: 10` out of a 250-item selection is not truncation and reports nothing. The field is never present with a `false` value, so test for its presence rather than its value, and use `hasMore` to decide whether to page further. `count` is the total size of the selection, not the number of entries returned.

```js
const { handles, count, hasMore } = await fb2k.invoke('selection.get', { limit: 0 });
```

### selection.getType


_No parameters._

**Returns**: `{"type":"...","typeName":"..."}`

```js
const result = await fb2k.invoke('selection.getType');
```

### selection.getViewerMode


_No parameters._

**Returns**: `{"mode":"..."}`

```js
const result = await fb2k.invoke('selection.getViewerMode');
```

### selection.getViewingTrack


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | No | `false` | Includes the full `track` info object. |

**Returns**: `{"found":true,"handle":"...","itemIndex":"...","mode":"...","playlistIndex":"...","source":"...","success":true,"track":{}}`

```js
const { found, track } = await fb2k.invoke('selection.getViewingTrack', { includeTrackInfo: true });
```

### selection.set


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `handles` | `array<string>` | Yes | Non-empty array of paths, optionally with a `\|subsong:N` suffix. |

**Returns**: `{ "success": true, "count": 2 }`

Entries that are not strings are skipped silently, and a malformed `|subsong:` suffix falls back to subsong `0`. Failures return `{ "success": false, "error": "..." }` — a missing or non-array `handles`, an empty array, no resolvable entries, or a failure to acquire the selection holder.

```js
await fb2k.invoke('selection.set', { handles: ['C:\\Music\\a.flac'] });
```

### selection.setPlaylistTracking


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | `'playlist'` or `'selection'`. Any other value is treated as `'selection'`, which is also the default. |

**Returns**: `{"error":"...","mode":"...","success":true}`

```js
await fb2k.invoke('selection.setPlaylistTracking', { mode: 'playlist' });
```

## Selection behavior

`selection.getViewerMode` returns either `prefer_playing` or `prefer_selection`, derived from the live selection type rather than a stored setting. `selection.getViewingTrack` applies that preference and falls back to the other source when the preferred one has no track; it always reports `success: true`, so test `found` instead. `selection:changed` is broadcast to every WebView after a selection update and is throttled to 50 ms; its payload is documented in the event reference.

Queue and selection handles share one string form: a native path with `|subsong:N` appended only when the subsong is greater than `0`. Paths supplied to `queue.addPaths` or the JIT Queue operations accept the same suffix. Individual paths and URLs are capped at 2048 characters.

## JIT Queue events

These events are emitted while the JIT shadow playlist is maintained. Subscribe before issuing operations when the frontend needs to refill or observe that buffer.

| Event | Meaning | Payload keys |
| --- | --- | --- |
| `jitQueue:needNext` | The manager needs the next logical track. | `{ currentTrackId, reason }` |
| `jitQueue:trackChanged` | The JIT current track changed. | `{ trackId, title }` |
| `jitQueue:listExhausted` | No further tracks are available — emitted on end of playback with an empty buffer, or after `jitQueue.notifyEmpty`. | `{ lastTrackId }` |
| `jitQueue:preloadComplete` | A batch preload completed. | `{ count, startIndex, replace }` |
| `jitQueue:error` | A JIT operation failed for a track. | `{ trackId, error }` plus **exactly one** of `url` (streaming source) or `path` (local file) |
