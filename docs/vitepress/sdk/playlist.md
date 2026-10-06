# `fb.playlist` playlist management

Every method on this page that takes a playlist (`index`, `playlistIndex`) accepts a `PlaylistRef`: the playlist's index, or the `guid` string that `getAll()`, `getActive()`, `create()` and `duplicate()` report. An index names whichever playlist is at that position when the host handles the call; a `guid` keeps naming the same playlist while others are added, removed or reordered, and fails with `NOT_FOUND` once that playlist is gone. `fb.library.addToPlaylist` and `fb.artwork.getByPlaylistItem` take a `PlaylistRef` too.

## getAll() 

Resolves with `{ playlists, count }`, every playlist as `PlaylistInfo[]` in playlist order, or with a failure envelope.

```javascript
const res = await fb.playlist.getAll();
if (res.success === false) throw new Error(res.error);
// res.playlists: [{index: 0, guid: "{…}", name: "Default", trackCount: 100, isActive: true, isPlaying: false, isLocked: false, isAutoplaylist: false}, ...]
```

::: tip
`PlaylistInfo` does not expose a `duration` field. Its fields are `index`, `guid`, `name`, `trackCount`, `isActive`, `isPlaying`, `isLocked` and `isAutoplaylist`; `getActive()` and `getPlaying()` report the duration.
:::

Keep the `guid` of each playlist a menu or picker offers, and pass it back when the user chooses:

```javascript
const res = await fb.playlist.getAll();
if (res.success === false) throw new Error(res.error);
const target = res.playlists.find((p) => p.name === 'Favorites');
// Later, when the menu closes; the playlist list may have changed meanwhile.
if (target) await fb.library.addToPlaylist(['E:\\Music\\song.flac'], target.guid);
```

## getActive() / setActive(index) 

Gets or sets the active playlist. `getActive()` resolves with `PlaylistGetActiveResponse`: `found` is `false`, and the other fields are absent, when there is no active playlist.

```javascript
const info = await fb.playlist.getActive();
if (info.success === false) throw new Error(info.error);
// {found, index, name, trackCount, isActive, isPlaying, isLocked, duration}
if (info.found) console.log(info.index, info.name);

await fb.playlist.setActive(1);
```

## getTracks(index, start, count) 

Fetches a slice of playlist tracks. Resolves with the page `{ playlist, start, count, total, tracks }`; rows are typed `PlaylistTrackPartial` because `fields` can narrow them.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `start` | `number?` | Start offset |
| `count` | `number?` | Maximum number of tracks |
| `formats` | `Record<string, string>?` | Named Title Formatting columns; each row carries their values under `formats` |
| `fields` | `string[]?` | Track keys to project; every row then holds exactly these plus `index` |

```javascript
const page = await fb.playlist.getTracks(0, 200, 200, undefined, ['title', 'album']);
if (page.success === false) throw new Error(page.error);
console.log(`${page.tracks.length} of ${page.total}`);
```

`fields` draws from the same case-sensitive list `library.query` takes. Omit it to keep the whole row, which also carries `composer`, `comment` and the foo_playcount statistics. A rejected request, such as one naming an unknown field, resolves with `{ success: false, code: 'INVALID_PARAMS' }`.

Compare `total` with the `total` from `getGroupRuns()` for the same playlist to detect a change in track count between the reads. Equal totals do not rule out replacements or reordering: the calls return independent snapshots, not a shared playlist revision.

## getTracksPage(index, start, count, formats?, fields?)

Deprecated: the same call as `getTracks()`, resolving with the same page. Use `getTracks()`.

## getGroupRuns(patterns, index?)

Groups a whole playlist into consecutive runs and returns only the run boundaries, never the rows.

A run is a maximal stretch of adjacent tracks whose group key is equal, compared case-insensitively over ASCII `A-Z`/`a-z`. Rows are never reordered, so runs follow playlist order.

| Parameter | Type | Description |
| --- | --- | --- |
| `patterns` | `string[]` | One or two Title Formatting expressions; the second sub-groups within each run |
| `index` | `number?` | Playlist to group; defaults to the active playlist |

```javascript
const res = await fb.playlist.getGroupRuns([
  '%album artist% | %album%',
  "$if(%discnumber%,'Disc '%discnumber%,)",
]);
if (res.success === false) throw new Error(res.error);
const { runs, total } = res;
// runs[1].sub[0].start is an absolute row index, not an offset in the parent
```

On success, `runs` covers the whole playlist. For a non-empty playlist, `runs[0].start` is 0, adjacent runs meet end to end, and the `count` values sum to `total`. An empty playlist returns `total: 0` and `runs: []`. `sub` appears only when two patterns are given, and its `start` uses the same absolute basis as the parent's.

Pair it with `getTracks()` to drive a grouped virtual list: the runs give header positions and let the UI calculate total scroll height; each visible page of rows is fetched separately. A 100k-track playlist yields roughly 10k top-level runs only if each run contains about 10 tracks on average. Payload size depends on the group keys and any second-level runs; the size of full track rows also depends on the requested fields and metadata.

A pattern that groups every row on its own makes `runs` as long as `total`. There is no cap or fixed byte size per run; choose patterns that combine adjacent tracks.

Malformed `patterns`, or a pattern that fails to compile, resolves with `{ success: false, code: 'INVALID_PARAMS' }`; for an empty pattern or one that fails to compile, `details.pattern` carries its index. A playlist index past the last one resolves with `INVALID_INDEX`, and an omitted index with no active playlist with `NO_ACTIVE_ITEM`.

## getMatchingRows(query, index?) / getTracksAt(index, rows, formats?, fields?)

Filter a playlist of any length in two calls. `getMatchingRows()` resolves with `{ playlist, playlistGuid, total, items, count }`, `items` being the rows whose tracks match the foobar2000 query, ascending. `getTracksAt()` reads rows by number in the order given, shaped as in `getTracks()`; rows past the last one are skipped, and every row carries its `index`.

```javascript
const guid = '{A9624480-77F0-4A2B-A76F-7EAB0C2EEC1C}';
const hits = await fb.playlist.getMatchingRows('artist HAS nachi', guid);
if (hits.success === false) {
  // details.param is 'query' when the query itself was refused
} else {
  const page = await fb.playlist.getTracksAt(guid, hits.items.slice(0, 50), undefined, ['title']);
}
```

A query the parser rejects, or one carrying `SORT BY`, resolves with `INVALID_PARAMS` and `details.param: 'query'`; the `error` text is in the host's language. A lowercase letter matches either case in the tag, an uppercase one only the same case. `getMatchingRows()` fails for a playlist that does not exist (`INVALID_INDEX`, `NOT_FOUND`, `NO_ACTIVE_ITEM`); `getTracksAt()` answers an empty result, as `getTracks()` does.

The rows are a snapshot. Drop them on `playlist:itemsAdded`, `itemsRemoved`, `itemsReordered`, `itemsReplaced` and `metadb:changed`; `total` equal to the playlist's current length does not rule out a reorder or a tag edit. A query that depends on the time, such as `%added% DURING LAST 2 WEEKS`, also drifts with no event.

## playTrack(playlistIndex, trackIndex, options?) 

Starts a specific playlist item, as double-clicking it does. Resolves with `PlaylistPlayTrackResponse`; a row past the last one fails with `INVALID_INDEX`.

| Parameter | Type | Description |
| --- | --- | --- |
| `playlistIndex` | `number` | Playlist index |
| `trackIndex` | `number` | Track index |
| `options` | `Omit<PlaylistPlayTrackParams, 'playlist' \| 'index'>?` | `deferred` and `muted` |

```javascript
await fb.playlist.playTrack(0, 4); // Fifth item in the first playlist
await fb.playlist.playTrack(0, 0, { muted: true });
```

## add(index, paths) 

Adds paths or URLs to a playlist. Each entry is limited to 2048 characters; longer entries are skipped and counted in `invalidCount`.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `paths` | `string[]` | Paths or URLs |

```javascript
const r = await fb.playlist.add(0, ['E:\\Music\\song1.flac', 'E:\\Music\\song2.mp3']);
if (r.success === false) throw new Error(r.error);
console.log(`Added ${r.addedCount} tracks`);
```

## removeTracks(index, indices) 

Removes the specified track indices; indices past the last row, and negative ones, are ignored. Resolves with `PlaylistRemoveTracksResponse`; a locked playlist fails with `LOCKED`.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `indices` | `number[]` | Track indices to remove |

```javascript
await fb.playlist.removeTracks(0, [0, 2, 5]);
```

## getPlaying() 

Resolves with `PlaylistGetPlayingResponse`, the same shape as `getActive()`; `found` is `false` when nothing is playing.

```javascript
const playing = await fb.playlist.getPlaying();
```

## getCount(index) 

Returns the number of tracks in the playlist at `index` as `{ count }`; `count` is `0` when the playlist does not exist. It calls the host's `playlist.getTrackCount`. The host's `playlist.getCount` returns the number of playlists, which the SDK exposes as [`getPlaylistCount()`](#getplaylistcount).

```javascript
const r = await fb.playlist.getCount(0);
if (r.success === false) throw new Error(r.error);
console.log(r.count);
```

## create(name?) 

Creates a playlist and returns `{ index }`. The SDK requires a name; optional native creation fields may be passed as the second argument.

| Parameter | Type | Description |
| --- | --- | --- |
| `name` | `string` | Playlist name |
| `options` | `Omit<PlaylistCreateParams, 'name'>?` | Optional native creation fields |

```javascript
const r = await fb.playlist.create('Rock');
if (r.success === false) throw new Error(r.error);
console.log(`New playlist index: ${r.index}`);
```

## remove(index) / clear(index) 

Removes a playlist or clears all of its tracks.

```javascript
await fb.playlist.remove(2);
await fb.playlist.clear(0);  // Keep the playlist, remove its tracks
```

## rename(index, name) 

Renames a playlist. Resolves with `PlaylistRenameResponse`; a lock that refuses the new name fails with `LOCKED`.

```javascript
await fb.playlist.rename(0, 'Favorites');
```

## duplicate(index, name?) 

Duplicates a playlist and resolves with `PlaylistDuplicateResponse`, whose `index` is the copy. With `name` omitted or empty, the copy is named after the original with ` (Copy)` appended.

```javascript
const r = await fb.playlist.duplicate(0);
if (r.success === false) throw new Error(r.error);
console.log(`Duplicate index: ${r.index}`);
```

## addAsync(index, paths) / addSequential(index, paths) 

Adds paths asynchronously or sequentially. The asynchronous form has operation metadata; the sequential form preserves one-by-one insertion order. The 2048-character path/URL limit also applies.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `paths` | `string[]` | Paths or URLs |

```javascript
await fb.playlist.addAsync(0, ['E:\\Music\\*.flac']);
await fb.playlist.addSequential(0, paths); // Preserve insertion order
```

## addHandles(index, handles) 

Adds tracks by reference, without expanding folders or cue sheets. Each `PlaylistHandleRef` is a path with an optional `|subsong:N` suffix, or `{ path, subsong }`. Unusable entries are counted in `invalidCount`; when none is usable the call fails with `NOT_FOUND`.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `handles` | `PlaylistHandleRef[]` | Tracks to append |

```javascript
await fb.playlist.addHandles(0, [
    { path: 'E:\\Music\\album.cue', subsong: 1 },
    { path: 'E:\\Music\\album.cue', subsong: 2 },
]);
```

## insertTracks(index, insertIndex, handles) 

Inserts tracks before row `insertIndex`; past the last row, they are appended. Resolves with `PlaylistInsertTracksResponse`.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `insertIndex` | `number` | Insertion position |
| `handles` | `PlaylistHandleRef[]` | Tracks to insert, as for `addHandles()` |

```javascript
await fb.playlist.insertTracks(0, 5, handles);
```

## removeSelectedTracks(index)

Signature: `fb.playlist.removeSelectedTracks(index: PlaylistRef): Promise<PlaylistRemoveSelectedTracksResponse>`

Removes the selected rows from the playlist, saving an undo point first. A locked playlist fails with `LOCKED`.

```javascript
await fb.playlist.removeSelectedTracks(0);
```

## getFocused(index) / setFocused(index, trackIndex) 

Gets or sets the focused playlist item. `getFocused()` resolves with `{ playlist, index }`, `index` being `-1` when the playlist has no focus or does not exist; a negative `trackIndex` makes `setFocused()` remove the focus.

```javascript
const r = await fb.playlist.getFocused(0);
if (r.success === false) throw new Error(r.error);
console.log(`Focused index: ${r.index}`);
await fb.playlist.setFocused(0, 10);
```

## getSelection(index) / getSelectedTracks(index) 

Gets selected indices or selected track details.

- `getSelection()` resolves with `{ items: number[], count, playlist }`.
- `getSelectedTracks()` resolves with `{ playlist, count, tracks }`, the rows as whole `PlaylistTrack`s without the play statistics. An empty selection succeeds with an empty `tracks`; a playlist that does not exist resolves with a failure envelope, so the two stay distinguishable.

```javascript
const sel = await fb.playlist.getSelection(0); // {items: [0, 5, 10], count: 3}
const selected = await fb.playlist.getSelectedTracks(0);
if (selected.success === false) throw new Error(selected.error);
console.log(selected.tracks.length);
```

## setSelection(index, indices, clearOthers?) / selectAll(index) / deselectAll(index) 

Sets, selects all, or clears selection. `setSelection()` ignores indices past the last row and negative ones.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `indices` | `number[]` | Track indices to select |
| `clearOthers` | `boolean` | Clear existing selection first; defaults to `true` |

```javascript
await fb.playlist.setSelection(0, [1, 3, 5]);
await fb.playlist.selectAll(0);
await fb.playlist.deselectAll(0);
```

## moveTracks(index, indices, delta) 

Moves playlist items by a relative delta. The call first replaces the selection with `indices`, so the caller's own selection is lost; an empty `indices` moves the current selection.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `indices` | `number[]` | Item indices to move |
| `delta` | `number` | Relative offset; positive moves down, negative moves up |

```javascript
await fb.playlist.moveTracks(0, [0, 1], 3);  // Down three slots
await fb.playlist.moveTracks(0, [5, 6], -2); // Up two slots
```

## sort(index, pattern, descending?, selectedOnly?) 

Sorts tracks using a Title Formatting expression.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `pattern` | `string` | Title Formatting expression |
| `descending` | `boolean` | Descending order; defaults to `false` |
| `selectedOnly` | `boolean` | Sort selected items only; defaults to `false` |

```javascript
await fb.playlist.sort(0, '%artist%');
await fb.playlist.sort(0, '%date%', true); // Descending by date
```

## reorder(index, order) / shuffle(index) / reverse(index) 

Applies an explicit item order, shuffles tracks, or reverses the playlist.

```javascript
await fb.playlist.reorder(0, [3, 1, 0, 2]);
await fb.playlist.shuffle(0);
await fb.playlist.reverse(0);
```

## undo(index) / redo(index) 

Undoes or redoes a playlist operation.

```javascript
await fb.playlist.undo(0);
await fb.playlist.redo(0);
```

## Autoplaylists

### isAutoplaylist(index) 

Returns `{ isAutoplaylist }`.

```javascript
const r = await fb.playlist.isAutoplaylist(0);
if (r.success === false) throw new Error(r.error);
if (r.isAutoplaylist) console.log('This is an autoplaylist');
```

### createAutoplaylist(name, query, sort?, keepSorted?) 

Creates an autoplaylist and returns `{ index }`.

| Parameter | Type | Description |
| --- | --- | --- |
| `name` | `string` | Playlist name |
| `query` | `string` | foobar2000 query expression |
| `sort` | `string?` | Optional Title Formatting sort expression |
| `keepSorted` | `boolean?` | Keep the list sorted; defaults to `false` |

```javascript
await fb.playlist.createAutoplaylist(
    'Recent',
    '%added% DURING LAST 2 WEEKS',
    '%added%',
    true
);
```

### getAutoplaylistInfo(index) 

Returns autoplaylist metadata.

- `source` identifies the creation source when the host can determine it.
- A normal playlist returns `isAutoplaylist: false`.

```javascript
const info = await fb.playlist.getAutoplaylistInfo(0);
if (info.success === false) throw new Error(info.error);
if (info.isAutoplaylist) {
    console.log(info.source, info.keepSorted);
}
```

### getAutoplaylistQuery(index)

Signature: `fb.playlist.getAutoplaylistQuery(index: PlaylistRef): Promise<PlaylistGetAutoplaylistQueryResponse>`

Resolves with the same status as `getAutoplaylistInfo()` plus `query`, which is always `null`: foobar2000 does not expose an autoplaylist's query.

```javascript
const res = await fb.playlist.getAutoplaylistQuery(0);
```

### convertToAutoplaylist(index, query, sort?, keepSorted?) 

Converts a normal playlist to an autoplaylist. Parameters mirror `createAutoplaylist()` plus the playlist index.

```javascript
await fb.playlist.convertToAutoplaylist(0, '%rating% GREATER 3', '%rating%', true);
```

### removeAutoplaylist(index)

Signature: `fb.playlist.removeAutoplaylist(index: PlaylistRef): Promise<PlaylistRemoveAutoplaylistResponse>`

Turns an autoplaylist back into an ordinary playlist that keeps its current tracks; the response's `source` is then `sdk`. An autoplaylist that another component manages cannot be released this way: the call succeeds with `source` `dui` and a `note`, and changes nothing; `remove()` deletes such a playlist. A playlist that is not an autoplaylist fails with `NOT_FOUND`.

```javascript
await fb.playlist.removeAutoplaylist(0);
```

## isLocked(index) / getLockInfo(index) 

Gets playlist lock state or lock details.

```javascript
const locked = await fb.playlist.isLocked(0);   // {isLocked}
const info = await fb.playlist.getLockInfo(0);   // {playlist, isLocked}
```

## replaceAllAndPlay(options) 

Atomically clears a playlist, adds paths, and optionally starts playback.

| Parameter | Type | Description |
| --- | --- | --- |
| `options.playlist` | `number` | Playlist index |
| `options.paths` | `string[]` | Paths to load |
| `options.playIndex` | `number?` | Start index; host default is `0` |
| `options.stopFirst` | `boolean?` | Stop current playback first; host default is `true` |
| `options.autoPlay` | `boolean?` | Start playback; host default is `true` |

```javascript
await fb.playlist.replaceAllAndPlay({
    playlist: 0,
    paths: ['E:\\Music\\album\\*.flac'],
    playIndex: 0
});
```

## reorderPlaylists(order)

Reorders all playlists. `order` is the new sequence, every playlist exactly once: all current indices, or all `guid`s from `getAll()`. Indices are read as the host handles the call, so an order built from an earlier `getAll()` moves the wrong playlists once one was added and another removed in between; GUIDs keep naming the playlists they were read from, and a GUID no playlist has fails with `NOT_FOUND`.

```javascript
await fb.playlist.reorderPlaylists([2, 0, 1]);

const all = await fb.playlist.getAll();
if (all.success) {
  await fb.playlist.reorderPlaylists(all.playlists.map((p) => p.guid).reverse());
}
```

## focusTrack(playlistIndex, trackIndex)

Signature: `fb.playlist.focusTrack(playlistIndex: PlaylistRef, trackIndex: number): Promise<PlaylistFocusTrackResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlistIndex` | `number` | Yes | Playlist index |
| `trackIndex` | `number` | Yes | Track index |

Deprecated: the same operation as `setFocused()`.

```javascript
await fb.playlist.focusTrack(0, 12);
```

## getAvailableColumns()

Signature: `fb.playlist.getAvailableColumns(): Promise<PlaylistGetAvailableColumnsResponse>`

Resolves with `{ columns, count }`: the playlist columns that foobar2000 and the installed components define for the Default UI playlist view.

```javascript
const res = await fb.playlist.getAvailableColumns();
if (res.success === false) throw new Error(res.error);
console.log(res.columns.map((c) => c.name));
```

## getFocusTrack(index)

Signature: `fb.playlist.getFocusTrack(index: PlaylistRef): Promise<PlaylistGetFocusTrackResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `number` | Yes | Playlist index |

Deprecated: returns `{ playlist, index }` as `getFocused()` does, except that a playlist that does not exist fails with `INVALID_INDEX`.

```javascript
const focus = await fb.playlist.getFocusTrack(0);
```

## getPlaylistCount()

Signature: `fb.playlist.getPlaylistCount(): Promise<PlaylistGetCountResponse>`

Reports how many playlists there are, as `count`.

```javascript
const res = await fb.playlist.getPlaylistCount();
if (res.success === false) throw new Error(res.error);
const { count } = res;
```
