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

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### flush()

Signature: `fb.queue.flush(): Promise<BaseResponse & { clearedCount?: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Clears the playback queue. `flush()` is the SDK wrapper for the `queue.flush` alias; `clear()` provides the equivalent clear operation through `queue.clear`.

```javascript
const result = await fb.queue.flush();
```

### getCount()

Signature: `fb.queue.getCount(): Promise<{ count: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the current queue length as `{ count }`.

```javascript
const result = await fb.queue.getCount();
console.log(result.count);
```

<!-- END AUTO-GENERATED SDK STUBS -->
