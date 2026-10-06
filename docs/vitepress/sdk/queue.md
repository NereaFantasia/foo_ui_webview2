# fb.queue Playback Queue

`fb.queue` manages the foobar2000 playback queue. It is distinct from the just-in-time queue exposed as `fb.jitQueue`.

## setContents(items)

Replaces the entire queue with an ordered list of `QueueContentRef` entries — `{ queueIndex }` to keep/reorder an existing queue slot, or `{ playlist, item }` to add a playlist track.

| Parameter | Type | Description |
| --- | --- | --- |
| `items` | `QueueContentRef[]` | Ordered list of queue-slot or playlist-item references |

Any entry with an unrecognized shape fails the whole call before anything is written — unlike `fb.queue.add`, which skips out-of-range entries individually. Passing `[]` clears the queue, equivalent to `fb.queue.clear()`.

Call this once when the user releases a drag-to-reorder gesture, not on every intermediate frame: each call flushes and rebuilds the whole queue.

```javascript
// Reorder: move queue slot 2 to the front, keep the rest in order
await fb.queue.setContents([{ queueIndex: 2 }, { queueIndex: 0 }, { queueIndex: 1 }]);
```

## insertNext(entries, position?)

Inserts tracks so they play next, ahead of everything already queued. Each entry is a file path (`string`) or a playlist position (`QueueListRef`, `{ playlist, item }`); a track already in the queue is moved to `position` instead of being queued a second time.

| Parameter | Type | Description |
| --- | --- | --- |
| `entries` | `Array<string \| QueueListRef>` | File paths or URLs (optionally with a `\|subsong:N` suffix), playlist positions, or a mix |
| `position` | `number?` | Insertion index after any moved entries are removed; defaults to `0` |

New path entries have no playlist position, so the playback cursor cannot follow that track's position; moving an existing entry by path preserves its usable coordinate. Pass a `QueueListRef` to select a particular playlist position. A mixed array is split by type: coordinates first, paths second, with each block keeping its order. For distinct tracks, `[p1, I1, p2]` queues as `I1, p1, p2`; use `setContents` afterwards if the exact interleaving matters. All entries deduplicate by track. For duplicate coordinates, the first supplied coordinate already present in the queue is retained, or the first supplied coordinate if none matches. The track keeps its first-occurrence order within the input block.

The response separates `insertedCount` (new tracks) and `movedCount` (relocated existing entries). `invalidCount` is the input path count minus the resolved track count, floored at zero; folder or container expansion can offset failed paths, so it is not an exact failure count. One bad position entry fails the whole call before anything is written.

```javascript
const result = await fb.queue.insertNext(['C:\\Music\\a.flac'], 0);
if (result.success === false) throw new Error(result.error);
console.log(result.insertedCount, result.movedCount);
// Play playlist 0, item 12 next; the cursor follows it
await fb.queue.insertNext([{ playlist: 0, item: 12 }]);
```

## playNow(index?)

Plays the queue entry at `index` immediately, moving it to the front of the queue first if it is not already there. Defaults to `index: 0`, the current queue head.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number?` | Queue index to play; defaults to `0` |

`queueCount` in the response is read immediately after playback starts and is not guaranteed to reflect the state before or after the played entry is consumed — call `fb.queue.getCount()` afterward if the exact post-play length matters.

```javascript
await fb.queue.playNow(); // play the current queue head
await fb.queue.playNow(2); // promote and play the 3rd entry
```

## remove(target)

Removes queue entries: one position, sent as the host's `index`, or an array of positions, sent as `indices` and removed in one call.

| Parameter | Type | Description |
| --- | --- | --- |
| `target` | `number \| number[]` | A queue position, or an array of queue positions |

With an array, duplicate and out-of-range positions are skipped and the response reports `removedCount`; with a single position it reports `removedIndex`. Both report `queueCount`, the queue length afterwards. The call fails with `INVALID_INDEX` when no given position is in range. Removing an array in one call avoids the index shift that removing the same positions one by one would cause.

```javascript
const one = await fb.queue.remove(0);
if (one.success === false) throw new Error(one.error);
console.log(one.removedIndex, one.queueCount);

const many = await fb.queue.remove([4, 1, 2]);
if (many.success === false) throw new Error(many.error);
console.log(many.removedCount, many.queueCount);
```

## add(opts)

Signature: `fb.queue.add(opts: QueueAddParams): Promise<QueueAddResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts.playlist` | `number` | No | Playlist the rows belong to; the active playlist when omitted |
| `opts.playlistGuid` | `string` | No | The playlist's `guid` from `playlist.getAll`, in place of `playlist` |
| `opts.tracks` | `number[]` | No | Rows to queue, in order; takes precedence over `track` |
| `opts.track` | `number` | No | A single row to queue; read only when `tracks` is absent |

Queues one or more playlist rows by position. Rows past the last one are skipped; when none is in range the call fails with `INVALID_INDEX`, and a negative row fails with `INVALID_PARAMS`. A `playlist` index past the last playlist fails with `INVALID_INDEX`; with `playlist` omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. Giving both `playlist` and `playlistGuid`, or a malformed GUID, fails with `INVALID_PARAMS`, and a playlist that no longer exists fails with `NOT_FOUND`.

The response carries `addedCount`, the number of rows queued, and `queueCount`, the queue length afterwards.

```javascript
const result = await fb.queue.add({ playlist: 0, tracks: [3, 5] });
if (result.success === false) throw new Error(result.error);
console.log(result.addedCount, result.queueCount);
```

## addPaths(paths, opts?)

Signature: `fb.queue.addPaths(paths: string[], opts?: Omit<QueueAddPathsParams, 'paths'>): Promise<QueueAddPathsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | File paths or URLs, each optionally with a `\|subsong:N` suffix; at least one |
| `opts.useQueuePlaylist` | `boolean` | No | Append to the dedicated `[WebView Queue]` playlist, created when missing; defaults to `true` |
| `opts.playlist` | `number` | No | Target playlist, read only when `useQueuePlaylist` is `false`; the active playlist when omitted |
| `opts.playlistGuid` | `string` | No | The target playlist's `guid` from `playlist.getAll`, in place of `playlist`; read only when `useQueuePlaylist` is `false` |

Queues tracks by path or URL. A queue entry needs playlist membership, so the paths are appended to a playlist first: by default the dedicated `[WebView Queue]` playlist. Each path or URL is limited to 2048 characters; longer entries are skipped and counted in `invalidCount`.

A locked target playlist fails with `LOCKED`, and nothing is added or queued. Giving both `playlist` and `playlistGuid`, or a malformed GUID, fails with `INVALID_PARAMS`; a playlist that no longer exists fails with `NOT_FOUND`. The target is looked up again after the paths are resolved: if it moved in the meantime it still gets the tracks and `playlist` in the response is its new index; if it was removed, the call fails with `OPERATION_FAILED` and nothing is queued.

The response carries `addedCount` (tracks appended and queued), `invalidCount` (entries of `paths` that produced no track, over-length ones included), `playlist` (the playlist the tracks went to), and `queueCount`.

```javascript
const result = await fb.queue.addPaths(['E:\\Music\\song.flac'], { useQueuePlaylist: true });
if (result.success === false) throw new Error(result.error);
console.log(result.addedCount, result.invalidCount, result.queueCount);
```

## clear()

Signature: `fb.queue.clear(): Promise<QueueClearResponse>`

Empties the playback queue. The response carries `clearedCount`, the number of entries the queue held. `flush()` performs the same operation under its older name.

```javascript
const result = await fb.queue.clear();
if (result.success === false) throw new Error(result.error);
console.log(result.clearedCount);
```

## flush()

Signature: `fb.queue.flush(): Promise<QueueFlushResponse>`

Clears the playback queue. `flush()` is the SDK wrapper for the `queue.flush` alias; `clear()` provides the equivalent clear operation through `queue.clear`. The response carries `clearedCount`, the number of entries the queue held.

```javascript
const result = await fb.queue.flush();
```

## get()

Signature: `fb.queue.get(): Promise<QueueGetResponse>`

Reads the whole queue in play order, without paging, and returns `{ items, count }`. Each `QueueItem` is the shared `Track` row plus `queueIndex`, its position in the queue from `0`, and `playlist`, `playlistGuid` and `playlistItem`, the playlist position the entry was queued from. All three are `null` when the entry has no recorded position that still holds its track: it was queued by path, or rows or the playlist were removed or moved since.

```javascript
const queue = await fb.queue.get();
if (queue.success === false) throw new Error(queue.error);
for (const item of queue.items) {
	console.log(item.queueIndex, item.path, item.playlist, item.playlistItem);
}
```

## getCount()

Signature: `fb.queue.getCount(): Promise<QueueGetCountResponse>`

Returns the current queue length as `{ count, hasItems }`; `hasItems` is the same as `count > 0`.

```javascript
const result = await fb.queue.getCount();
if (result.success === false) throw new Error(result.error);
console.log(result.count);
```

## moveToTop(index)

Signature: `fb.queue.moveToTop(index: number): Promise<QueueMoveToTopResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `number` | Yes | Queue position of the entry to move; `0` is refused |

Moves a queued entry to the front so it plays next. Only the queue order changes, and the other entries keep their relative order. An `index` of `0`, which is already the front, or one past the end of the queue fails with `INVALID_INDEX`.

The response carries `movedIndex`, the position the entry came from, and `queueCount`.

```javascript
const result = await fb.queue.moveToTop(3);
if (result.success === false) throw new Error(result.error);
console.log(result.movedIndex, result.queueCount);
```
