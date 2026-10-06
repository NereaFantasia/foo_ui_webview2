# `fb.playlist` playlist management

## getAll() 

Returns all playlists as `PlaylistInfo[]`.

```javascript
const playlists = await fb.playlist.getAll();
// [{index: 0, name: "Default", trackCount: 100, isActive: true, isPlaying: false, isLocked: false, isAutoplaylist: false}, ...]
```

::: tip
`PlaylistInfo` does not expose a `duration` field. Use the documented fields such as `index`, `name`, `trackCount`, `isActive`, `isPlaying`, `isLocked`, and optional `isAutoplaylist`.
:::

## getActive() / setActive(index) 

Gets or sets the active playlist. `getActive()` resolves to `PlaylistInfo | null`.

```javascript
const info = await fb.playlist.getActive();
// {index, name, trackCount, isActive, isPlaying, isLocked, isAutoplaylist?}
console.log(info.index, info.name);

await fb.playlist.setActive(1);
```

## getTracks(index, start, count) 

Fetches a slice of playlist tracks. The SDK unwraps the native page envelope and resolves directly to `PlaylistTrack[]`.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `start` | `number?` | Start offset |
| `count` | `number?` | Maximum number of tracks |
| `formats` | `Record<string, string>?` | Optional named Title Formatting expressions |
| `fields` | `string[]?` | Track keys to project; every row then holds exactly these plus `index` |

```javascript
const tracks = await fb.playlist.getTracks(0, 0, 50);
console.log(`Received ${tracks.length} tracks`);
```

`fields` draws from the same case-sensitive whitelist `library.query` uses, minus `composer` and `comment`. Omit it to keep the full row. Projected rows are narrower than `PlaylistTrack` declares, so read only the keys you asked for. A malformed list makes the host resolve with `{ success: false, code: 'INVALID_PARAMS' }`, which has no `tracks` to unwrap and therefore surfaces here as an empty array.

Use `getTracksPage()` when the pagination envelope or the host's error envelope is required.

## getTracksPage(index, start, count, formats?, fields?)

Same request as `getTracks()`, but keeps the page envelope instead of unwrapping it.

Resolves with `{ playlist, start, count, total, tracks }`. Compare `total` with the `total` from `getGroupRuns()` for the same playlist to detect a change in track count between the reads. Equal totals do not rule out replacements or reordering: the calls return independent snapshots, not a shared playlist revision.

```javascript
const page = await fb.playlist.getTracksPage(0, 200, 200, undefined, ['title', 'album']);
console.log(`${page.tracks.length} of ${page.total}`);
```

## getGroupRuns(patterns, index?)

Groups a whole playlist into consecutive runs and returns only the run boundaries, never the rows.

A run is a maximal stretch of adjacent tracks whose group key is equal, compared case-insensitively over ASCII `A-Z`/`a-z`. Rows are never reordered, so runs follow playlist order.

| Parameter | Type | Description |
| --- | --- | --- |
| `patterns` | `string[]` | One or two Title Formatting expressions; the second sub-groups within each run |
| `index` | `number?` | Playlist to group; defaults to the active playlist |

```javascript
const { runs, total } = await fb.playlist.getGroupRuns([
  '%album artist% | %album%',
  "$if(%discnumber%,'Disc '%discnumber%,)",
]);
// runs[1].sub[0].start is an absolute row index, not an offset in the parent
```

On success, `runs` covers the whole playlist. For a non-empty playlist, `runs[0].start` is 0, adjacent runs meet end to end, and the `count` values sum to `total`. An empty playlist returns `total: 0` and `runs: []`. `sub` appears only when two patterns are given, and its `start` uses the same absolute basis as the parent's.

Pair it with `getTracksPage()` to drive a grouped virtual list: the runs give header positions and let the UI calculate total scroll height; each visible page of rows is fetched separately. A 100k-track playlist yields roughly 10k top-level runs only if each run contains about 10 tracks on average. Payload size depends on the group keys and any second-level runs; the size of full track rows also depends on the requested fields and metadata.

A pattern that groups every row on its own makes `runs` as long as `total`. There is no cap or fixed byte size per run; choose patterns that combine adjacent tracks.

Malformed `patterns`, or a pattern that fails to compile, resolves with `{ success: false, code: 'INVALID_PARAMS' }`; when one pattern is at fault, `details.pattern` carries its index.

## playTrack(playlistIndex, trackIndex, options?) 

Starts a specific playlist item. Returns a `BaseResponse`.

| Parameter | Type | Description |
| --- | --- | --- |
| `playlistIndex` | `number` | Playlist index |
| `trackIndex` | `number` | Track index |
| `options` | `Omit<PlaylistPlayTrackParams, 'playlist' \| 'track'>?` | Optional native play-track fields |

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
console.log(`Added ${r.addedCount} tracks`);
```

## removeTracks(index, indices) 

Removes the specified track indices. Returns a `BaseResponse`.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `indices` | `number[]` | Track indices to remove |

```javascript
await fb.playlist.removeTracks(0, [0, 2, 5]);
```

## getPlaying() 

Returns the playing playlist as `PlaylistInfo | null`.

```javascript
const playing = await fb.playlist.getPlaying();
```

## getCount(index) 

Returns the playlist's track count as `{ count }`.

```javascript
const r = await fb.playlist.getCount(0);
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
console.log(`New playlist index: ${r.index}`);
```

## remove(index) / clear(index) 

Removes a playlist or clears all of its tracks.

```javascript
await fb.playlist.remove(2);
await fb.playlist.clear(0);  // Keep the playlist, remove its tracks
```

## rename(index, name) 

Renames a playlist. Returns a `BaseResponse`.

```javascript
await fb.playlist.rename(0, 'Favorites');
```

## duplicate(index) 

Duplicates a playlist and returns the new `{ index }`.

```javascript
const r = await fb.playlist.duplicate(0);
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

Adds metadb-handle-like objects without automatic CUE expansion.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `handles` | `unknown[]` | Handle objects accepted by the native API |

```javascript
await fb.playlist.addHandles(0, [
    { path: 'E:\\Music\\album.cue', subsong: 1 },
    { path: 'E:\\Music\\album.cue', subsong: 2 },
]);
```

## insertTracks(index, insertIndex, handles) 

Inserts handle objects at a playlist position.

| Parameter | Type | Description |
| --- | --- | --- |
| `index` | `number` | Playlist index |
| `insertIndex` | `number` | Insertion position |
| `handles` | `unknown[]` | Handle objects accepted by the native API |

```javascript
await fb.playlist.insertTracks(0, 5, handles);
```

## removeSelectedTracks(index) 

Removes the currently selected tracks. Returns a `BaseResponse`.

## getFocused(index) / setFocused(index, trackIndex) 

Gets or sets the focused playlist item. `getFocused()` returns `{ index, track? }`; `setFocused()` returns a `BaseResponse`.

```javascript
const r = await fb.playlist.getFocused(0);
console.log(`Focused index: ${r.index}`);
await fb.playlist.setFocused(0, 10);
```

## getSelection(index) / getSelectedTracks(index) 

Gets selected indices or selected track details.

- `getSelection()` returns `{ items: number[], count, playlist? }`.
- `getSelectedTracks()` unwraps the native envelope and returns `PlaylistTrack[]` directly.

```javascript
const sel = await fb.playlist.getSelection(0); // {items: [0, 5, 10], count: 3}
const tracks = await fb.playlist.getSelectedTracks(0);
```

## setSelection(index, indices, clearOthers?) / selectAll(index) / deselectAll(index) 

Sets, selects all, or clears selection. Each method returns a `BaseResponse`.

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

Moves playlist items by a relative delta.

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
if (info.isAutoplaylist) {
    console.log(info.source, info.keepSorted);
}
```

### getAutoplaylistQuery(index) 

Returns the native `{ query }` result. The wrapper currently types `query` as `string`; consult host-version behavior before assuming every autoplaylist source exposes its original query.

### convertToAutoplaylist(index, query, sort?, keepSorted?) 

Converts a normal playlist to an autoplaylist. Parameters mirror `createAutoplaylist()` plus the playlist index.

```javascript
await fb.playlist.convertToAutoplaylist(0, '%rating% GREATER 3', '%rating%', true);
```

### removeAutoplaylist(index) 

Removes autoplaylist behavior and returns the operation result.

## isLocked(index) / getLockInfo(index) 

Gets playlist lock state or lock details.

```javascript
const locked = await fb.playlist.isLocked(0);   // {isLocked}
const info = await fb.playlist.getLockInfo(0);   // {isLocked, lockName, ...}
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

Reorders all playlists. `order` is the new sequence of existing playlist indices.

```javascript
await fb.playlist.reorderPlaylists([2, 0, 1]);
```

## Supplemental method reference

### focusTrack(playlistIndex, trackIndex)

Signature: `fb.playlist.focusTrack(playlistIndex: number, trackIndex: number): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlistIndex` | `number` | Yes | Playlist index |
| `trackIndex` | `number` | Yes | Track index |

Returns the focus operation result.

```javascript
await fb.playlist.focusTrack(0, 12);
```

### getAutoplaylistQuery(index)

Signature: `fb.playlist.getAutoplaylistQuery(index: number): Promise<{ query: string }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `number` | Yes | Playlist index |

Returns the autoplaylist query exposed by the host.

```javascript
const query = await fb.playlist.getAutoplaylistQuery(0);
```

### getAvailableColumns()

Signature: `fb.playlist.getAvailableColumns(): Promise<PlaylistAvailableColumnsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns available DUI playlist column definitions.

```javascript
const columns = await fb.playlist.getAvailableColumns();
```

### getFocusTrack(index)

Signature: `fb.playlist.getFocusTrack(index: number): Promise<{ index: number; track?: TrackInfo }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `number` | Yes | Playlist index |

Returns the focused index and optional track snapshot.

```javascript
const focus = await fb.playlist.getFocusTrack(0);
```

### getPlaylistCount()

Signature: `fb.playlist.getPlaylistCount(): Promise<{ count: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the total playlist count.

```javascript
const { count } = await fb.playlist.getPlaylistCount();
```

### removeAutoplaylist(index)

Signature: `fb.playlist.removeAutoplaylist(index: number): Promise<PlaylistRemoveAutoplaylistResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `number` | Yes | Playlist index |

Returns the conversion-to-normal-playlist result.

```javascript
await fb.playlist.removeAutoplaylist(0);
```

### removeSelectedTracks(index)

Signature: `fb.playlist.removeSelectedTracks(index: number): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `number` | Yes | Playlist index |

Returns the remove operation result.

```javascript
await fb.playlist.removeSelectedTracks(0);
```
