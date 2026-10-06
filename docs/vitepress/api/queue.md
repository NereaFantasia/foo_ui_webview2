# Queue API

Methods of the `queue` namespace.

## queue

### queue.add

<!-- api-schema:begin queue.add -->
Queue one or more tracks by their position in a playlist. Positions past the last row are skipped; when none is in range the call fails with `INVALID_INDEX`. A negative position fails with `INVALID_PARAMS`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Playlist the positions refer to; the active playlist when omitted. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `tracks` | `integer[]` | No | Rows to queue, in order. Takes precedence over `track`. Each item: at least `0`. |
| `track` | `integer` | No | A single row to queue; read only when `tracks` is absent. At least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `addedCount` | `integer` | How many rows were queued. |
| `queueCount` | `integer` | Queue length afterwards. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Queues one or more tracks by their position in a playlist.

Supply either `tracks` or `track`, not both — when `tracks` is an array it wins and `track` is ignored. Out-of-range track indices are skipped, so a call that matches nothing fails with `INVALID_INDEX` and carries `addedCount: 0`; the same is true for an empty `tracks` array. An invalid `playlist` fails with `INVALID_INDEX` (`Invalid playlist index`), and neither field with `INVALID_PARAMS`. Entries in `tracks` must be integers — a non-numeric element is refused by parameter validation rather than skipped.

```js
// queue several tracks from the active playlist
await fb2k.invoke('queue.add', { tracks: [0, 1, 2] });
// queue a single track
await fb2k.invoke('queue.add', { track: 0 });
```

### queue.addPaths

<!-- api-schema:begin queue.addPaths -->
Queue tracks by path. Queueing needs playlist membership, so the paths are appended to a playlist first: by default a dedicated `[WebView Queue]` playlist, created when missing. A locked target playlist fails with `LOCKED`, and nothing is added or queued. The target is looked up again after the paths are resolved, as in `playlist.addPaths`: moved meanwhile, it still gets the tracks and `playlist` in the result is its new index; removed meanwhile, the call fails with `OPERATION_FAILED` and nothing is queued. A path the media-root check refuses fails the whole call with `PERMISSION_DENIED`. Paths that resolve to nothing are dropped; when none resolves the call fails with `NOT_FOUND`, the failure carrying `invalidCount`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | File paths or URLs, each optionally with a `\|subsong:N` suffix. Entries longer than 2048 characters are dropped and counted in `invalidCount`. Must not be empty. |
| `useQueuePlaylist` | `boolean` | No | Append to the dedicated `[WebView Queue]` playlist, creating it when missing. `false` uses `playlist`, or the active playlist when that is omitted too. Default: `true`. |
| `playlist` | `integer` | No | Target playlist; read only when `useQueuePlaylist` is `false`. An index past the last playlist fails with `INVALID_INDEX`; with this and `playlistGuid` omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | The target playlist's `guid`, instead of `playlist`; like `playlist` it is read only when `useQueuePlaylist` is `false`, and not checked at all otherwise. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `addedCount` | `integer` | How many tracks were appended and queued. |
| `invalidCount` | `integer` | Entries of `paths` dropped: empty ones, ones longer than 2048 characters, a `\|subsong:N` no track could be made for, and the plain paths when none of them resolved. A plain path that resolves to nothing while another plain path of the call resolves is not counted, so a call that succeeds can have dropped more than this. |
| `playlist` | `integer` | The playlist the tracks were appended to. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `queueCount` | `integer` | Queue length afterwards. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Queues tracks by file path, adding them to a playlist first.

Because queueing requires playlist membership, this method appends the paths to a playlist and then queues them. By default that target is a dedicated `[WebView Queue]` playlist rather than your active one. Setting `useQueuePlaylist: false` targets `playlist` instead, or the active playlist when `playlist` is also omitted — which fails with `No active playlist` if there is none.

A locked target playlist fails with `LOCKED` (`Playlist is locked`), with `details` carrying `playlist` and `isLocked: true`. When nothing resolves, the `NOT_FOUND` failure carries `invalidCount`. An empty `paths` array is refused with `INVALID_PARAMS`, an invalid `playlist` with `INVALID_INDEX`, and no active playlist with `NO_ACTIVE_ITEM`.

```js
await fb2k.invoke('queue.addPaths', { paths: ['C:\\Music\\a.flac'] });
```

### queue.clear

<!-- api-schema:begin queue.clear -->
Empty the queue.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `clearedCount` | `integer` | How many entries the queue held. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('queue.clear');
```

### queue.flush

<!-- api-schema:begin queue.flush -->
Empty the queue; the same operation as `queue.clear` under its older name.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `clearedCount` | `integer` | How many entries the queue held. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('queue.flush');
```

### queue.get

<!-- api-schema:begin queue.get -->
Read the whole playback queue, in play order. No paging: every entry comes back.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `items` | `QueueItem[]` | The entries, in play order. |
| `items[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `items[].queueIndex` | `integer` | Position in the queue, from `0`. |
| `items[].playlist` | `integer \| null` | Playlist the entry was queued from; `null` when it carries no playlist position: queued by path, or the position foobar2000 keeps for it no longer holds this track (the row or the playlist was removed, or the rows moved). |
| `items[].playlistGuid` | `string \| null` | GUID of that playlist, as `playlistGuid` takes it; `null` exactly when `playlist` is `null`. |
| `items[].playlistItem` | `integer \| null` | Row in that playlist; `null` exactly when `playlist` is `null`. |
| `count` | `integer` | Number of entries. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Returns the entire playback queue.

There is no paging; the whole queue is always returned. Each entry is the shared [Track](../reference/types.md#track) row plus `queueIndex` and the originating `playlist`, `playlistGuid` and `playlistItem`. All three position fields are always present. An entry whose recorded position still holds its track reports it; the others report all three as `null`: one queued through the `paths` of `queue.insertNext`, or one whose recorded position no longer holds its track after rows or the playlist were removed or moved. A reported position therefore never points at a different track. `item.playlist == null` is the whole test.

```js
const res = await fb2k.invoke('queue.get');
if (res.success === false) throw new Error(res.error);
const { items, count } = res;
```

### queue.getCount

<!-- api-schema:begin queue.getCount -->
Report how many entries the queue holds.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of entries. |
| `hasItems` | `boolean` | Whether the queue holds anything; the same as `count > 0`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Returns the queue length.

`hasItems` is exactly `count > 0`, provided as a convenience.

```js
const res = await fb2k.invoke('queue.getCount');
if (res.success === false) throw new Error(res.error);
const { count } = res;
```

### queue.moveToTop

<!-- api-schema:begin queue.moveToTop -->
Move a queued entry to the front so it plays next. Only queue order changes; the relative order of the other entries is kept. An `index` of `0`, one past the end, and any index while the queue is empty fail with `INVALID_INDEX`. When the queue cannot be rebuilt the call fails with `OPERATION_FAILED`, the failure carrying `queueCount`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | Yes | Position of the entry to promote; `0` is already the front and is refused. At least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `movedIndex` | `integer` | The position the entry came from. |
| `queueCount` | `integer` | Queue length afterwards. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Moves a queued entry to the front of the queue.

Only queue order changes — playlist membership is untouched. The relative order of the remaining entries is preserved. A missing `index` is refused with `INVALID_PARAMS`; an out-of-range value, an entry already at the top, or an empty queue all fail with `INVALID_INDEX` (`Invalid index or already at top`).

```js
await fb2k.invoke('queue.moveToTop', { index: 3 });
```

### queue.remove

<!-- api-schema:begin queue.remove -->
Remove one entry by `index`, or several by `indices`. An empty queue fails with `NOT_FOUND` whatever is given. An `index` past the end fails with `INVALID_INDEX`. In a batch, duplicate indices and those past the end of the queue are skipped; when none is in range the call fails with `INVALID_INDEX`, the failure carrying `removedCount` `0` and `queueCount`. Giving neither key, or a negative index, fails with `INVALID_PARAMS`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | One queue position to remove. Takes precedence over `indices`. At least `0`. |
| `indices` | `integer[]` | No | Queue positions to remove; read only when `index` is absent. Each item: at least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `removedIndex` | `integer` | The position removed; only when `index` was given. |
| `removedCount` | `integer` | How many entries were removed; only when `indices` was given. |
| `queueCount` | `integer` | Queue length afterwards. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Removes one or more entries from the queue.

Supply either `index` or `indices`, not both — `index` wins when present. Duplicate and out-of-range values in `indices` are skipped, and a batch that matches nothing fails with `INVALID_INDEX` carrying `removedCount: 0`. An empty queue fails with `NOT_FOUND`, an out-of-range `index` with `INVALID_INDEX`, and neither field with `INVALID_PARAMS`.

```js
await fb2k.invoke('queue.remove', { indices: [0, 2] });
```

### queue.setContents

<!-- api-schema:begin queue.setContents -->
Replace the whole queue with an ordered list of references. One unusable reference fails the whole call before anything is written. An empty list clears the queue. A playlist index is read as the call arrives, so one taken before the playlist list changed queues a row of another playlist; `playlistGuid` keeps naming the playlist it was read from. An entry giving both `playlist` and `playlistGuid`, or a malformed GUID, fails with `INVALID_PARAMS`, a GUID no playlist has with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `QueueContentRef[]` | Yes | The new queue, in order. At most `max(256, current queue length)` entries; more fail with `INVALID_PARAMS`. |
| `items[].queueIndex` | `integer` | No | Position of an entry already in the queue. At least `0`. |
| `items[].playlist` | `integer` | No | Playlist of the row to add; needs `item` as well. At least `0`. |
| `items[].playlistGuid` | `string` | No | The playlist of the row to add by its `guid`, instead of `playlist`; needs `item` as well. |
| `items[].item` | `integer` | No | Row in that playlist; needs `playlist` or `playlistGuid` as well. At least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `queueCount` | `integer` | Queue length afterwards. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Replaces the entire queue with an ordered list of references.

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

<!-- api-schema:begin queue.insertNext -->
Insert tracks so they play next, ahead of everything already queued. A track that is already queued is moved instead of queued twice. `items` land before `paths`; each block keeps its own order. A call with neither `paths` nor `items` fails with `INVALID_PARAMS`. Every entry of `items` is checked before any path is resolved, and one bad entry fails the whole call with nothing written: an entry naming its playlist by neither or both of `playlist` and `playlistGuid`, or by a malformed GUID, fails with `INVALID_PARAMS`, a GUID no playlist has with `NOT_FOUND`, a playlist index or row out of range with `INVALID_INDEX`. A path the media-root check refuses fails the whole call with `PERMISSION_DENIED`; a path that resolves to nothing is dropped and counted in `invalidCount`, and when nothing at all is left to insert the call fails with `NOT_FOUND`. A playlist row that changed while the paths were resolved fails the call with `OPERATION_FAILED`, and so does a queue that cannot be rebuilt.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | No | File paths or URLs, each optionally with a `\|subsong:N` suffix. The entries they produce carry no playlist position. |
| `items` | `QueueListRef[]` | No | Playlist rows; the entries they produce carry that position, so the playback cursor follows them. At most `max(256, current queue length)` entries; more fail with `INVALID_PARAMS`. |
| `items[].playlist` | `integer` | No | Playlist index; give this or `playlistGuid`. At least `0`. |
| `items[].playlistGuid` | `string` | No | The playlist's `guid`, instead of `playlist`. |
| `items[].item` | `integer` | Yes | Row in that playlist. At least `0`. |
| `position` | `integer` | No | Where to insert, counted after any moved entries are taken out; `0` is the front. At least `0`. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `insertedCount` | `integer` | Tracks newly queued. |
| `movedCount` | `integer` | Entries that were already queued and moved. |
| `queueCount` | `integer` | Queue length afterwards. |
| `invalidCount` | `integer` | Input path count minus the tracks resolved from paths, floored at zero. A folder or playlist file resolves to several tracks, so this is not a count of the paths that failed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Inserts tracks so they play next, ahead of everything already queued. Entries are given as file paths, as playlist positions, or both.

¹ At least one of `paths` and `items` must be non-empty.

A track already present in the queue is moved to `position` instead of being queued a second time; only one occurrence moves, selected by the coordinate rules below. Within one call the `items` block lands first and the `paths` block after it; each block keeps its own order. `insertedCount` counts newly queued tracks and `movedCount` counts relocated entries. `invalidCount` is the input path count minus the number of tracks resolved from paths, floored at zero. It is not an exact count of failed paths: folder or container expansion can offset failures. `items` never counts toward `invalidCount`: one bad entry, such as a non-object, missing `playlist` or `item`, or an out-of-range position, fails the whole call with an `error` naming `items[i]`. Nothing is written, and the `paths` of the same call are not queued either.

All input entries are deduplicated by track, not by position. For duplicate `items`, the first supplied coordinate already present in the queue is retained; if none matches, the first supplied coordinate is used. The track keeps its first-occurrence order within the input block. An existing entry at that coordinate moves first; otherwise the first queued entry for the track moves and takes the supplied coordinate. A path-only move preserves the existing entry's usable coordinate; only a newly inserted path lacks one. `queue.add` and `queue.setContents` preserve duplicate references. A lower `insertedCount + movedCount` can reflect deduplication or invalid paths; folders and containers can also expand to multiple tracks.

Both arrays empty or absent fails with `INVALID_PARAMS` (`No paths or items specified`); `items` that is not an array fails with `items must be an array`; `paths` resolving to zero valid tracks with no `items` fails with `NOT_FOUND` (`No valid tracks found`) carrying `invalidCount`. A coordinate out of range is `INVALID_INDEX`, a playlist item that cannot be fetched `NOT_FOUND`, and a playlist that changed while the paths were being resolved `OPERATION_FAILED`, each naming `items[i]`.

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

<!-- api-schema:begin queue.playNow -->
Play the queue entry at `index` now, moving it to the front first when it is not already there. An empty queue fails with `NOT_FOUND`, an `index` past the end with `INVALID_INDEX`. When the entry has to be moved and the queue cannot be rebuilt, the call fails with `OPERATION_FAILED` and playback is not started.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | Queue position to play. At least `0`. Default: `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playedIndex` | `integer` | The position that was played. |
| `queueCount` | `integer` | Queue length read right after playback started; the host may consume the played entry before or after that read. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Plays the queue entry at `index` immediately, promoting it to the front of the queue first if it is not already there.

Fails with `NOT_FOUND` (`Queue is empty`) when nothing is queued, or `INVALID_INDEX` (`Invalid queue index`) for an out-of-range `index`; a negative or non-integer value is refused by parameter validation.

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
const res = await fb2k.invoke('queue.getCount');
if (res.success === false) throw new Error(res.error);
const { count } = res;
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
