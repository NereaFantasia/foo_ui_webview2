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

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `name` | `string` | No | `New Playlist` |  |
| `position` | `integer` | No | — | Insert position; appends when omitted. |

**Returns**: `{ "success": true, "index": 2 }`

```javascript
const result = await fb2k.invoke('playlist.create', { name: 'Rock Music' });
```

### playlist.remove


Remove a playlist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{ "success": true }`

A locked playlist cannot be removed and returns `{ "success": false, "error": "Playlist is locked", "code": "LOCKED" }`. Removal can also report a plain `success: false` with no `error` when the operation is refused for another reason.

### playlist.rename

Rename a playlist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | Yes | — | Target index. There is **no** active-playlist fallback, so omitting it fails. |
| `name` | `string` | No | `""` | New name. |

**Returns**: `{ "success": true }`

As with `playlist.setActive`, omitting `playlist` does not fall back to the active playlist — it returns `{ "success": false, "error": "Invalid playlist index" }`.

```javascript
await fb2k.invoke('playlist.rename', { playlist: 0, name: 'My Favorites' });
```

### playlist.clear


Remove all tracks from a playlist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

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

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `name` | `string` | No | source name + ` (Copy)` |  |

**Returns**: `{ "success": true, "index": 1, "sourcePlaylist": 0, "newPlaylist": 1, "name": "Default (Copy)", "trackCount": 150 }`

## Track operations

### playlist.getTrackCount


Get the number of tracks in a playlist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |

**Returns**: `{ "count": 150 }`

This method returns no `success` field. An index that cannot be resolved yields `{ "count": 0 }` rather than an error, so a zero count does not distinguish an empty playlist from an invalid target.

### playlist.getTracks

Get a paged list of tracks in a playlist. The response has no `success` field.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |
| `start` | `integer` | No | `0` | Page offset. |
| `count` | `integer` | No | `100` | Page size. |
| `formats` | `object` | No | `{}` | Extra TitleFormat columns (see tip below). |
| `fields` | `string[]` | No | all fields | Return only these fields (see tip below). |

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

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

> `rating` first reads `%rating%` and uses a value from `1` to `5`. If that value is missing or outside this range, it reads the file's `RATING` tag and clamps it to `0`-`5`; no available rating gives `0` (unrated). `rating.get` uses the same rule: `storage` is `'stats'` for an accepted `%rating%` value and `'file'` otherwise, including when no file tag exists.

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

::: tip Projection (`fields` parameter)
Pass `fields` to get back only the fields you name. `index` is always present, whether you ask for it or not.

```javascript
const page = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 200,
    fields: ['title', 'artist', 'album', 'duration', 'path']
});
```

Accepted names are `index`, `title`, `artist`, `artists`, `album`, `albumArtist`, `genre`, `date`, `trackNumber`, `discNumber`, `duration`, `path`, `absolutePath`, `fileSize`, `bitrate`, `sampleRate`, `channels`, `codec`, `subsong` and `rating`. Names are matched exactly and are case sensitive; an unknown one fails the whole call with `INVALID_PARAMS` and lists the offenders in `details.unknownFields`, so a typo never silently drops a field. `composer` and `comment` are not accepted — they are returned only when you ask for no projection at all. `artists` works the other way round: this endpoint returns it only under a projection, as the array of atomic values behind `artist`.

Omitting `absolutePath` and `rating` avoids resolving the filesystem path and evaluating `%rating%` for each row. The foo_playcount columns (`playCount`, `firstPlayed`, `lastPlayed`, `added`) are not produced under a projection either; request them through `formats` if you need them alongside one.
:::

### playlist.getGroupRuns

Group a whole playlist and get back only the run boundaries, never the rows. A run is a maximal stretch of adjacent tracks whose group key is equal, so runs follow playlist order — nothing is reordered.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `patterns` | `string[]` | Yes | — | One TitleFormat pattern, or two for a second grouping level inside each run. |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |

**Returns**:

```json
{
    "success": true,
    "playlist": 0,
    "total": 150,
    "runs": [
        { "start": 0, "count": 12, "key": "Album A | Artist A" },
        { "start": 12, "count": 9, "key": "Album B | Artist B" }
    ]
}
```

The example above shows only the first two runs. A successful response covers the entire playlist: for a non-empty list, `runs[0].start` is 0, adjacent runs meet end to end, and their `count` values sum to `total`. An empty playlist returns `total: 0` and `runs: []`.

With two patterns each run also carries `sub`, and the sub-run `start` values are absolute row indices, not offsets within the parent:

```json
{
    "start": 12,
    "count": 9,
    "key": "Album B | Artist B",
    "sub": [
        { "start": 12, "count": 5, "key": "1" },
        { "start": 17, "count": 4, "key": "2" }
    ]
}
```

Keys are compared case-insensitively over ASCII `A-Z`/`a-z` only; every other code point is compared byte for byte. An empty key is an ordinary key, not a missing one — a pattern that evaluates to an empty string still forms runs, so handle the fallback text on the display side if you want one.

`INVALID_PARAMS` is returned when `patterns` is absent, is not an array, is empty, holds more than two entries, holds a non-string or an empty string, or holds a pattern that fails to compile. Only errors in a specific entry carry its index in `details.pattern`; array-level errors have no such index. An out-of-range `playlist` also fails with `INVALID_PARAMS`, except that `-1` selects the active playlist.

::: tip Driving a grouped virtual list
Pair this with `playlist.getTracks`: the runs provide every header position and let the UI calculate total scroll height; each visible page of rows is fetched separately. Response size depends on the number of runs, key lengths and second-level groups, not on full track metadata. A pattern that puts every track in its own run makes `runs` as long as the playlist, and the host sets no upper bound. Compare `total` with a page response for the same playlist to detect a change in track count. Equal totals do not rule out replacements or reordering between the independent reads.
:::

### playlist.playTrack


Play a specific track in a playlist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | `0` | Track index. Falls back to `track`, then `0`. |
| `track` | `integer` | No | `0` | Legacy alias for `index`. |
| `deferred` | `boolean` | No | `false` | Deferred start, recommended for streaming sources. |
| `muted` | `boolean` | No | `false` | Mutes before playback starts (see note below). |

**Returns**: `{ "success": true }`

`muted: true` mutes before playback starts and never unmutes afterwards, so the player is left muted — restore the volume yourself when the intent was only to suppress the start of the track. An out-of-range `index` returns `{ "success": false, "error": "Invalid track index" }`.

```javascript
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 5 });

// Deferred start, recommended for streaming sources
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 0, deferred: true });
```

### playlist.removeTracks


Remove the specified tracks from a playlist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |
| `items` | `array<integer>` | No | `[]` | Track indices to remove. |

**Returns**: `{ "success": true }`

A locked playlist is rejected with `{ "success": false, "error": "Playlist is locked", "code": "LOCKED" }`.

### playlist.removeSelectedTracks

Remove the currently selected tracks from a playlist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |

**Returns**: `{ "success": true }`

A locked playlist is rejected with `{ "success": false, "error": "Playlist is locked", "code": "LOCKED" }`.

### playlist.moveTracks


Move selected tracks by `delta`. When `items` is non-empty, those indices become the selection first; when `items` is empty, the current selection is moved (SMP-compatible).

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |
| `items` | `array<integer>` | No | `[]` | Empty moves the current selection (SMP-compatible). |
| `delta` | `integer` | No | `0` | Displacement; negative moves up. |

**Returns**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.moveTracks', { items: [0, 1, 2], delta: 3 });
await fb2k.invoke('playlist.moveTracks', { items: [5, 6], delta: -2 });
```

### playlist.addPaths


Add files or folders to a playlist. Paths are resolved synchronously via `playlist_incoming_item_filter` and CUE sheets are expanded automatically.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `paths` | `array<string>` | Yes | — | File or folder paths, optionally with a `\|subsong:N` suffix. An empty array fails with `No paths specified`. |

::: tip Order follows foobar2000's incoming-item filter, not the input array
Plain paths go through the same filter foobar2000 applies when you add files from its own UI: duplicates are removed and the result is sorted by the user's *Sort incoming files by* preference (pointer order when that preference is empty). The added tracks therefore do **not** necessarily appear in the order you passed them, and `playlist.replaceAllAndPlay`'s `playIndex` refers to the resulting playlist position, not to an index into `paths`. Use `playlist.addPathsSequential` when input order must be preserved. Entries with a `\|subsong:N` suffix bypass the filter and are inserted ahead of the filtered batch.
:::

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `handles` | `array<object \| string>` | Yes | — | Entries as `{ path, subsong }` objects or `path\|subsong:N` strings. |

**Returns**: `{"addedCount":"...","countBefore":"...","error":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
await fb2k.invoke('playlist.addHandles', { handles: ['C:\\Music\\song.flac'] });
```

### playlist.addPathsAsync


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `paths` | `array<string>` | Yes | — | File or folder paths, optionally with a `\|subsong:N` suffix. An empty array fails with `No paths specified`. |

**Returns**: `{"error":"...","invalidCount":"...","operationId":"...","status":"...","success":true,"totalCount":"..."}`

Local files and stream URLs are added synchronously without a progress dialog; only playlist wrappers (`.pls` / `.m3u` / `.cue`) go through asynchronous expansion. A `\|subsong:N` suffix selects that subsong of a multi-track file, rather than the default subsong 0.

```js
const { operationId } = await fb2k.invoke('playlist.addPathsAsync', { paths: ['C:\\Music\\Album'] });
```

### playlist.addPathsSequential


Add paths in exactly the order given. Unlike `playlist.addPaths`, the incoming-item filter is bypassed: nothing is re-sorted or de-duplicated, so passing the same file twice adds it twice, and a folder or CUE sheet expands in place at its position in `paths`. `order` lists the playlist indices the new items landed on, in input order.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `paths` | `array<string>` | Yes | — | File or folder paths, optionally with a `\|subsong:N` suffix. An empty array fails with `No paths specified`. |

**Returns**: `{"addedCount":"...","error":"...","order":"...","playlist":"...","success":true}`

```js
await fb2k.invoke('playlist.addPathsSequential', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```

### playlist.convertToAutoplaylist


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `query` | `string` | Yes | — | Filter expression. An empty value fails with `Query is required`. |
| `sort` | `string` | No | — | Titleformat sort pattern. |
| `keepSorted` | `boolean` | No | `false` | Keeps the playlist sorted by `sort`. |

**Returns**: `{"error":"...","playlist":"...","success":true}`

```js
await fb2k.invoke('playlist.convertToAutoplaylist', { playlist: 0, query: '%genre% IS Rock' });
```

### playlist.createAutoplaylist


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `name` | `string` | No | `New Autoplaylist` |  |
| `query` | `string` | Yes | — | Filter expression. An empty value fails with `Query is required`. |
| `sort` | `string` | No | — | Titleformat sort pattern. |
| `keepSorted` | `boolean` | No | `false` | Keeps the playlist sorted by `sort`. |

**Returns**: `{"error":"...","index":"...","name":"...","playlist":"...","query":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.createAutoplaylist', { name: 'Rock', query: '%genre% IS Rock' });
```

### playlist.deselectAll


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('playlist.deselectAll', { playlist: 0 });
```

### playlist.focusTrack


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Target track; synonym of `track`, takes precedence. Omitting focuses nothing. |
| `track` | `integer` | No | — | Legacy alias for `index`. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.focusTrack', { playlist: 0, index: 3 });
```

### playlist.getAutoplaylistInfo

Reports whether a playlist is an autoplaylist, and its sort/source metadata when it is.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns** — an autoplaylist: `{ "isAutoplaylist": true, "playlist": 0, "keepSorted": false, "source": "sdk" }`. Anything else: `{ "isAutoplaylist": false, "playlist": 0 }`, with `keepSorted` and `source` absent.

`source` is `"sdk"` or `"dui"`; `keepSorted` is always `false` for a `dui` source. `lockName` accompanies a `dui` source only — an autoplaylist created through the SDK never carries it, even when the playlist is locked. Neither branch returns a `success` field — test `isAutoplaylist` instead. An out-of-range `playlist` returns `{ "success": false, "error": "Invalid playlist index" }`.

```js
const info = await fb2k.invoke('playlist.getAutoplaylistInfo', { playlist: 0 });
if (info.isAutoplaylist) console.log(info.source, info.keepSorted);
```

### playlist.getAutoplaylistQuery

Reports autoplaylist metadata for a playlist. The query string itself is not retrievable.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"error":"...","index":"...","playlist":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.getFocusTrack', { playlist: 0 });
```

### playlist.getFocusedTrack


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"index":"...","playlist":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.getFocusedTrack', { playlist: 0 });
```

### playlist.getLockInfo


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"error":"...","isLocked":"...","playlist":"...","success":true}`

```js
const { isLocked } = await fb2k.invoke('playlist.getLockInfo', { playlist: 0 });
```

### playlist.getSelectedTracks


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |

**Returns**: `{"error":"...","success":true,"tracks":"..."}`

```js
const result = await fb2k.invoke('playlist.getSelectedTracks');
```

### playlist.getSelection


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"count":"...","error":"...","items":"...","playlist":"...","success":true}`

```js
const { items, count } = await fb2k.invoke('playlist.getSelection', { playlist: 0 });
```

### playlist.insertTracks


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `position` | `integer` | No | `0` | Insert position. Falls back to `index`. |
| `index` | `integer` | No | — | Legacy alias for `position`. |
| `handles` | `array<object \| string>` | Yes | — | Entries as `{ path, subsong }` objects or `path\|subsong:N` strings. |

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

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{ "playlist": 0, "isAutoplaylist": true }`

`lockName` is added whenever the playlist carries a named lock, independently of the result — so unlike the two methods above, it can appear together with `isAutoplaylist: false` for an ordinary locked playlist. The success path has **no** `success` field — test `isAutoplaylist` instead. An out-of-range `playlist` returns `{ "success": false, "error": "Invalid playlist index" }`.

```js
const { isAutoplaylist } = await fb2k.invoke('playlist.isAutoplaylist', { playlist: 0 });
```

### playlist.isLocked


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"error":"...","isLocked":"...","success":true}`

```js
const { isLocked } = await fb2k.invoke('playlist.isLocked', { playlist: 0 });
```

### playlist.redo


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.redo', { playlist: 0 });
```

### playlist.removeAutoplaylist


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"error":"...","note":"...","playlist":"...","source":"...","success":true}`

```js
await fb2k.invoke('playlist.removeAutoplaylist', { playlist: 0 });
```

### playlist.reorder


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `newOrder` | `array<integer>` | Yes | — | A full permutation of the playlist's track indices. Its length must equal the current item count. |

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `paths` | `array<string>` | Yes | — | File or folder paths. An empty array fails with `No paths specified`. |
| `playIndex` | `integer` | No | `0` | Track to start playing after the add. |
| `stopFirst` | `boolean` | No | `true` | Stops current playback before adding. |
| `autoPlay` | `boolean` | No | `true` | Starts playback after adding. |

**Returns**: `{"addedCount":"...","clearedCount":"...","error":"...","invalidCount":"...","playIndex":"...","playlist":"...","success":true,"totalCount":"..."}`

```js
await fb2k.invoke('playlist.replaceAllAndPlay', { paths: ['C:\\Music\\song.flac'] });
```

### playlist.reverse


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('playlist.reverse', { playlist: 0 });
```

### playlist.selectAll


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

**Returns**: `{"success":true}`

```js
await fb2k.invoke('playlist.selectAll', { playlist: 0 });
```

### playlist.setFocusedTrack


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Target track; omitting focuses nothing. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.setFocusedTrack', { playlist: 0, index: 3 });
```

### playlist.setSelection


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |
| `indices` | `array<integer>` | No | `[]` | Track indices to select. |
| `clearOthers` | `boolean` | No | `true` | Clears the existing selection first. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.setSelection', { playlist: 0, indices: [0, 1, 2] });
```

### playlist.shuffle


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.shuffle', { playlist: 0 });
```

### playlist.sort


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |
| `index` | `integer` | No | — | Alias for `playlist`, read only when `playlist` is absent. |
| `pattern` | `string` | No | `%title%` | Titleformat sort pattern. |
| `descending` | `boolean` | No | `false` | Sort descending. |
| `selectedOnly` | `boolean` | No | `false` | Sort only the selected tracks. |

**Returns**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.sort', { playlist: 0, pattern: '%artist% - %title%' });
```

### playlist.undo


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | No | active playlist |  |

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
