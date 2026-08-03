# Playlist API

Playlist management, track operations, autoplaylists, and helpers. 47 APIs.

> **Parameter compatibility**: every Playlist API accepts both `playlist` and `index` for the playlist index.

## List management

### playlist.getCount

Get the number of playlists.

- **Parameters**: none
- **Returns**: `{ "count": 5 }`

```javascript
const { count } = await fb2k.invoke('playlist.getCount');
```

### playlist.getAll


Get information for all playlists.

- **Parameters**: none

**Returns**:

```json
[
    {
        "index": 0,
        "name": "Default",
        "trackCount": 150,
        "isActive": true,
        "isPlaying": true,
        "isLocked": false,
        "isAutoplaylist": false
    }
]
```

::: warning Breaking Change (v1.1.18)
`playlist.getAll` no longer returns `duration` (avoids loading every track across every playlist). Use `playlist.getActive` or `playlist.getPlaying` when you need a single playlist duration.
:::

::: tip v1.1.18 added
The `isAutoplaylist` field is now inlined in `playlist.getAll`; you no longer need per-playlist `playlist.isAutoplaylist` calls.
:::

### playlist.getActive

Get the active playlist. Includes a `duration` field.

- **Parameters**: none

**Returns**: `{"duration":"...","found":true,"index":0,"isActive":true,"isLocked":true,"isPlaying":true,"name":"...","success":true,"trackCount":"..."}`


> Returns `{ "success": true, "found": false }` when there is no active playlist.

### playlist.setActive

Set the active playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | Yes | Target index. There is **no** active-playlist fallback, so omitting it fails. |

**Returns**: `{ "success": true }`

Unlike most playlist methods, omitting `playlist` does not fall back to the active playlist — it returns `{ "success": false, "error": "Invalid playlist index" }`.

```javascript
await fb2k.invoke('playlist.setActive', { playlist: 1 });
```

### playlist.getPlaying

Get the currently playing playlist. Includes a `duration` field.

- **Parameters**: none

**Returns**: `{"duration":"...","found":true,"index":0,"isActive":true,"isLocked":true,"isPlaying":true,"name":"...","success":true,"trackCount":"..."}`


> Returns `{ "success": true, "found": false }` when nothing is playing.

### playlist.create

Create a new playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | `string` | No | Optional; default New Playlist. |
| `position` | `integer` | No | Optional; default append. |

**Returns**: `{ "success": true, "index": 2 }`

```javascript
const result = await fb2k.invoke('playlist.create', { name: 'Rock Music' });
```

### playlist.remove


Remove a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{ "success": true }`

A locked playlist cannot be removed and returns `{ "success": false, "error": "Playlist is locked", "code": "LOCKED" }`. Removal can also report a plain `success: false` with no `error` when the operation is refused for another reason.

### playlist.rename

Rename a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | Yes | Target index. There is **no** active-playlist fallback, so omitting it fails. |
| `name` | `string` | No | New name. Defaults to an empty string. |

**Returns**: `{ "success": true }`

As with `playlist.setActive`, omitting `playlist` does not fall back to the active playlist — it returns `{ "success": false, "error": "Invalid playlist index" }`.

```javascript
await fb2k.invoke('playlist.rename', { playlist: 0, name: 'My Favorites' });
```

### playlist.clear


Remove all tracks from a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**:

```json
{
    "success": true,
    "playlist": 0,
    "clearedCount": 22,
    "remainingCount": 0
}
```

### playlist.duplicate


Duplicate a playlist. The copy is inserted immediately after the source playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `name` | `string` | No | Optional; default source name + ' (Copy)'. |

**Returns**: `{ "success": true, "index": 1, "sourcePlaylist": 0, "newPlaylist": 1, "name": "Default (Copy)", "trackCount": 150 }`

## Track operations

### playlist.getTrackCount


Get the number of tracks in a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Alias for `playlist`, read only when `playlist` is absent. |

**Returns**: `{ "count": 150 }`

This method returns no `success` field. An index that cannot be resolved yields `{ "count": 0 }` rather than an error, so a zero count does not distinguish an empty playlist from an invalid target.

### playlist.getTracks

Get a paged list of tracks in a playlist. The response has no `success` field.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `start` | `integer` | No | Optional; default 0. |
| `count` | `integer` | No | Optional; default 100. |
| `formats` | `object` | No | Optional; default {}. |

**Returns**:

```json
{
    "playlist": 0,
    "start": 0,
    "count": 20,
    "total": 150,
    "tracks": [
        {
            "index": 0,
            "title": "Song 1",
            "artist": "Artist 1",
            "album": "Album 1",
            "albumArtist": "Artist 1",
            "genre": "Rock",
            "date": "2024",
            "trackNumber": 1,
            "discNumber": 1,
            "duration": 180.5,
            "path": "file://C:/Music/song1.flac",
            "absolutePath": "C:\\Music\\song1.flac",
            "fileSize": 25600000,
            "subsong": 0,
            "rating": 5,
            "codec": "FLAC",
            "bitrate": 1411,
            "sampleRate": 44100,
            "channels": 2,
            "composer": "Lennon/McCartney",
            "comment": "",
            "playCount": "15",
            "firstPlayed": "2024-01-15 10:30:00",
            "lastPlayed": "2026-02-10 20:00:00",
            "added": "2024-01-10 08:00:00"
        }
    ]
}
```

::: tip column (`formats` Parameter)
`playlist.getTracks` supports  `formats` Parameter TitleFormat column :

```javascript
const result = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 50,
    formats: {
        myRating: '%rating%',
        codec: '%codec%'
    }
});
// Each track object gains the extra myRating and codec fields
```
:::

::: tip Paths
`absolutePath` is the local filesystem path and can be passed directly to APIs such as `artwork.getForTrack`. `path` is the foobar2000 internal form.
:::

### playlist.playTrack


Play a specific track in a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Track index. Falls back to `track`, then `0`. |
| `track` | `integer` | No | Legacy alias for `index`. |
| `deferred` | `boolean` | No | Optional; default false. |
| `muted` | `boolean` | No | Optional; default false. |

**Returns**: `{ "success": true }`

`muted: true` mutes before playback starts and never unmutes afterwards, so the player is left muted — restore the volume yourself when the intent was only to suppress the start of the track. An out-of-range `index` returns `{ "success": false, "error": "Invalid track index" }`.

```javascript
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 5 });

// Deferred start, recommended for streaming sources
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 0, deferred: true });
```

### playlist.removeTracks


Remove the specified tracks from a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Alias for `playlist`, read only when `playlist` is absent. |
| `items` | `array<integer>` | No | Track indices to remove. |

**Returns**: `{ "success": true }`

A locked playlist is rejected with `{ "success": false, "error": "Playlist is locked", "code": "LOCKED" }`.

### playlist.removeSelectedTracks

Remove the currently selected tracks from a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Alias for `playlist`, read only when `playlist` is absent. |

**Returns**: `{ "success": true }`

A locked playlist is rejected with `{ "success": false, "error": "Playlist is locked", "code": "LOCKED" }`.

### playlist.moveTracks


Move selected tracks by `delta`. When `items` is non-empty, those indices become the selection first; when `items` is empty, the current selection is moved (SMP-compatible).

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `items` | `array<integer>` | No | Optional; default []. |
| `delta` | `integer` | No | Optional; default 0. |

**Returns**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.moveTracks', { items: [0, 1, 2], delta: 3 });
await fb2k.invoke('playlist.moveTracks', { items: [5, 6], delta: -2 });
```

### playlist.addPaths


Add files or folders to a playlist. Paths are resolved synchronously and CUE sheets are expanded automatically.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `paths` | `array<string>` | Yes | File or folder paths. An empty array fails with `No paths specified`. |

**Returns**:

```json
{
    "success": true,
    "playlist": 0,
    "requestedPaths": 25,
    "addedCount": 25,
    "invalidCount": 0,
    "countBefore": 0,
    "totalCount": 25
}
```

## Additional public APIs

### playlist.addHandles


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `handles` | `array<object \| string>` | Yes | Entries as `{ path, subsong }` objects or `path\|subsong:N` strings. |

**Returns**: `{"addedCount":"...","countBefore":"...","error":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
await fb2k.invoke('playlist.addHandles', { handles: ['C:\\Music\\song.flac'] });
```

### playlist.addPathsAsync


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `paths` | `array<string>` | Yes | File or folder paths. An empty array fails with `No paths specified`. |

**Returns**: `{"error":"...","invalidCount":"...","operationId":"...","status":"...","success":true,"totalCount":"..."}`

```js
const { operationId } = await fb2k.invoke('playlist.addPathsAsync', { paths: ['C:\\Music\\Album'] });
```

### playlist.addPathsSequential


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `paths` | `array<string>` | Yes | File or folder paths. An empty array fails with `No paths specified`. |

**Returns**: `{"addedCount":"...","error":"...","order":"...","playlist":"...","success":true}`

```js
await fb2k.invoke('playlist.addPathsSequential', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```

### playlist.convertToAutoplaylist


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `query` | `string` | Yes | Filter expression. An empty value fails with `Query is required`. |
| `sort` | `string` | No | Optional. |
| `keepSorted` | `boolean` | No | Optional; default false. |

**Returns**: `{"error":"...","playlist":"...","success":true}`

```js
await fb2k.invoke('playlist.convertToAutoplaylist', { playlist: 0, query: '%genre% IS Rock' });
```

### playlist.createAutoplaylist


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `keepSorted` | `boolean` | No | Optional; default false. |
| `name` | `string` | No | Optional; default New Autoplaylist. |
| `query` | `string` | Yes | Filter expression. An empty value fails with `Query is required`. |
| `sort` | `string` | No | Optional. |

**Returns**: `{"error":"...","index":"...","name":"...","playlist":"...","query":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.createAutoplaylist', { name: 'Rock', query: '%genre% IS Rock' });
```

### playlist.deselectAll


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('playlist.deselectAll', { playlist: 0 });
```

### playlist.focusTrack


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default no focused item. |
| `track` | `integer` | No | Optional; default no focused item. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.focusTrack', { playlist: 0, index: 3 });
```

### playlist.getAutoplaylistInfo

Reports whether a playlist is an autoplaylist, and its sort/source metadata when it is.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns** — an autoplaylist: `{ "isAutoplaylist": true, "playlist": 0, "keepSorted": false, "source": "sdk" }`. Anything else: `{ "isAutoplaylist": false, "playlist": 0 }`, with `keepSorted` and `source` absent.

`source` is `"sdk"` or `"dui"`; `keepSorted` is always `false` for a `dui` source. `lockName` accompanies a `dui` source only — an autoplaylist created through the SDK never carries it, even when the playlist is locked. Neither branch returns a `success` field — test `isAutoplaylist` instead. An out-of-range `playlist` returns `{ "success": false, "error": "Invalid playlist index" }`.

```js
const info = await fb2k.invoke('playlist.getAutoplaylistInfo', { playlist: 0 });
if (info.isAutoplaylist) console.log(info.source, info.keepSorted);
```

### playlist.getAutoplaylistQuery

Reports autoplaylist metadata for a playlist. The query string itself is not retrievable.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns** — an autoplaylist: `{ "isAutoplaylist": true, "playlist": 0, "query": null, "keepSorted": false, "source": "sdk", "note": "Query string not exposed by SDK" }`. Anything else: `{ "isAutoplaylist": false, "playlist": 0, "query": null }`.

`query` is **always** `null` — foobar2000 does not expose the filter expression, so this method cannot be used to read it back. `keepSorted`, `source`, and `note` appear only for an autoplaylist, and `lockName` only alongside a `dui` source. Neither branch returns a `success` field. An out-of-range `playlist` returns `{ "success": false, "error": "Invalid playlist index" }`.

```js
const q = await fb2k.invoke('playlist.getAutoplaylistQuery', { playlist: 0 });
// q.query is null even when q.isAutoplaylist is true
```

### playlist.getAvailableColumns

Lists the columns provided by the Default UI, for use as titleformat patterns.

_No parameters._

**Returns**: a bare JSON array — not an envelope, so there is no `success` field. Each entry carries `id`, `name`, `pattern`, `alignment` (`left` / `right` / `center`), and `numeric`; `sortPattern` is present only when the column defines a distinct sort script. The array is empty when no provider is registered.

```js
const result = await fb2k.invoke('playlist.getAvailableColumns');
```

### playlist.getFocusTrack


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","index":"...","playlist":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.getFocusTrack', { playlist: 0 });
```

### playlist.getFocusedTrack


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"index":"...","playlist":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.getFocusedTrack', { playlist: 0 });
```

### playlist.getLockInfo


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","isLocked":"...","playlist":"...","success":true}`

```js
const { isLocked } = await fb2k.invoke('playlist.getLockInfo', { playlist: 0 });
```

### playlist.getSelectedTracks


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default ignored when playlist is supplied. |

**Returns**: `{"error":"...","success":true,"tracks":"..."}`

```js
const result = await fb2k.invoke('playlist.getSelectedTracks');
```

### playlist.getSelection


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"count":"...","error":"...","items":"...","playlist":"...","success":true}`

```js
const { items, count } = await fb2k.invoke('playlist.getSelection', { playlist: 0 });
```

### playlist.insertTracks


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `position` | `integer` | No | Insert position. Falls back to `index`, then `0`. |
| `index` | `integer` | No | Legacy alias for `position`. |
| `handles` | `array<object \| string>` | Yes | Entries as `{ path, subsong }` objects or `path\|subsong:N` strings. |

**Returns**: `{"addedCount":"...","countBefore":"...","error":"...","insertIndex":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.insertTracks', {
    playlist: 0,
    position: 5,
    handles: ['C:\\Music\\song.flac'],
});
```

### playlist.isAutoplaylist

Tests whether a playlist is an autoplaylist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{ "playlist": 0, "isAutoplaylist": true }`

`lockName` is added whenever the playlist carries a named lock, independently of the result — so unlike the two methods above, it can appear together with `isAutoplaylist: false` for an ordinary locked playlist. The success path has **no** `success` field — test `isAutoplaylist` instead. An out-of-range `playlist` returns `{ "success": false, "error": "Invalid playlist index" }`.

```js
const { isAutoplaylist } = await fb2k.invoke('playlist.isAutoplaylist', { playlist: 0 });
```

### playlist.isLocked


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","isLocked":"...","success":true}`

```js
const { isLocked } = await fb2k.invoke('playlist.isLocked', { playlist: 0 });
```

### playlist.redo


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.redo', { playlist: 0 });
```

### playlist.removeAutoplaylist


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","note":"...","playlist":"...","source":"...","success":true}`

```js
await fb2k.invoke('playlist.removeAutoplaylist', { playlist: 0 });
```

### playlist.reorder


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `newOrder` | `array<integer>` | Yes | A full permutation of the playlist's track indices. Its length must equal the current item count. |

**Returns**: `{"error":"...","expected":"...","got":"...","index":"...","itemCount":"...","playlist":"...","success":true}`

```js
// newOrder must be a full permutation of the playlist's current track indices
await fb2k.invoke('playlist.reorder', { playlist: 0, newOrder: [2, 0, 1] });
```

### playlist.reorderPlaylists


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `newOrder` | `array<integer>` | Yes | A full permutation of the playlist indices. Its length must equal the playlist count. |

**Returns**: `{"count":"...","error":"...","expected":"...","got":"...","index":"...","success":true}`

```js
// newOrder must be a full permutation of the existing playlist indices
await fb2k.invoke('playlist.reorderPlaylists', { newOrder: [2, 0, 1] });
```

### playlist.replaceAllAndPlay


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `paths` | `array<string>` | Yes | File or folder paths. An empty array fails with `No paths specified`. |
| `playIndex` | `integer` | No | Optional; default 0. |
| `stopFirst` | `boolean` | No | Optional; default true. |
| `autoPlay` | `boolean` | No | Optional; default true. |

**Returns**: `{"addedCount":"...","clearedCount":"...","error":"...","invalidCount":"...","playIndex":"...","playlist":"...","success":true,"totalCount":"..."}`

```js
await fb2k.invoke('playlist.replaceAllAndPlay', { paths: ['C:\\Music\\song.flac'] });
```

### playlist.reverse


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('playlist.reverse', { playlist: 0 });
```

### playlist.selectAll


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('playlist.selectAll', { playlist: 0 });
```

### playlist.setFocusedTrack


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default no focused item. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.setFocusedTrack', { playlist: 0, index: 3 });
```

### playlist.setSelection


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `indices` | `array<integer>` | No | Optional; default []. |
| `clearOthers` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.setSelection', { playlist: 0, indices: [0, 1, 2] });
```

### playlist.shuffle


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.shuffle', { playlist: 0 });
```

### playlist.sort


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |
| `index` | `integer` | No | Optional; default used only if playlist is absent. |
| `pattern` | `string` | No | Optional; default %title%. |
| `descending` | `boolean` | No | Optional; default false. |
| `selectedOnly` | `boolean` | No | Optional; default false. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.sort', { playlist: 0, pattern: '%artist% - %title%' });
```

### playlist.undo


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Optional; default active playlist. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.undo', { playlist: 0 });
```

## Related playlist events

The following playlist lifecycle events are broadcast. Item-level events for the JIT queue shadow playlist are intentionally suppressed.

| Event | Fired when | Payload keys |
| --- | --- | --- |
| `playlist:itemsAdded` | Items were inserted into a playlist. | `{ playlist, start, count }` |
| `playlist:itemsRemoved` | Items were removed from a playlist. | `{ playlist, oldCount, newCount }` |
| `playlist:itemsReordered` | Items were reordered within a single playlist. | `{ playlist, count }` |
| `playlist:selectionChanged` | The selection in a playlist changed. | `{ playlist }` |
| `playlist:focusChanged` | The focused item in a playlist changed. | `{ playlist, from, to }` |
| `playlist:itemsReplaced` | Items in a playlist were replaced. | `{ playlist, count }` |
| `playlist:created` | A playlist was created. | `{ index, name }` |
| `playlist:removed` | One or more playlists were removed. | `{ oldCount, newCount }` |
| `playlist:reordered` | The playlist collection was reordered. | `{ count }` |
| `playlist:activated` | The active playlist changed. | `{ oldIndex, newIndex }` |
| `playlist:renamed` | A playlist was renamed. | `{ index, name }` |
| `playlist:lockChanged` | A playlist's lock state changed. | `{ playlist, locked }` |
| `playlist:defaultFormatChanged` | The default playlist format changed. | `{}` |
| `playlist:addComplete` | An asynchronous path-add operation finished. | `{ operationId, success, addedCount, totalCount }` |

`from`, `to`, `oldIndex`, and `newIndex` are `-1` when the corresponding index is unavailable.
