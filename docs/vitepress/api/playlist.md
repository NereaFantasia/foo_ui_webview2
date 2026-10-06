# Playlist API

Playlist management, track operations, autoplaylists, and helpers.

## Naming a playlist

A method that acts on one playlist takes either `playlist`, its index, or `playlistGuid`, the
`guid` that `playlist.getAll`, `getActive`, `getPlaying`, `create`, `duplicate` and
`createAutoplaylist` report. A result that names a playlist by index carries its GUID next to it
as `playlistGuid`; the `playlist:*` events name their playlists by GUID as well, each under the
field its payload lists. The same two keys work for
`library.addToPlaylist`, `queue.add`, `queue.addPaths` and `artwork.getByPlaylistItem`, and in
each entry of `queue.setContents` and `queue.insertNext`; `playlist.reorderPlaylists` takes the
new order as GUIDs in `newOrderGuids`.

An index names whichever playlist is at that position when the host handles the call, so it can
point somewhere else once playlists are removed or reordered, for example while a menu that
listed them is open. A GUID stays with its playlist: a call made with it lands in that playlist
or, when the playlist has been removed, fails with `NOT_FOUND`. Two playlists with the same name
have different GUIDs. Give one key or the other; both fail with `INVALID_PARAMS`.

```javascript
const res = await fb2k.invoke('playlist.getAll');
if (res.success === false) throw new Error(res.error);
const target = res.playlists.find((p) => p.name === 'Favorites');
if (target) {
    await fb2k.invoke('library.addToPlaylist', {
        paths: ['E:\\Music\\song.flac'],
        playlistGuid: target.guid,
    });
}
```

## List management

### playlist.getCount

<!-- api-schema:begin playlist.getCount -->
Report how many playlists there are.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of playlists. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('playlist.getCount');
if (res.success === false) throw new Error(res.error);
const { count } = res;
```

### playlist.getAll

<!-- api-schema:begin playlist.getAll -->
List every playlist in playlist order.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlists` | `PlaylistInfo[]` | Every playlist, in playlist order. |
| `playlists[].index` | `integer` | Position in the playlist list, from `0`. |
| `playlists[].guid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `playlists[].name` | `string` | Playlist name. |
| `playlists[].trackCount` | `integer` | Number of tracks. |
| `playlists[].isActive` | `boolean` | Whether this is the active playlist. |
| `playlists[].isPlaying` | `boolean` | Whether this is the playing playlist. |
| `playlists[].isLocked` | `boolean` | Whether the playlist carries a lock. |
| `playlists[].isAutoplaylist` | `boolean` | Whether the playlist is an autoplaylist, as `playlist.isAutoplaylist` reports it. |
| `count` | `integer` | Number of entries in `playlists`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

::: warning Breaking Change (v1.1.18)
`playlist.getAll` no longer returns `duration` (avoids loading every track across every playlist). Use `playlist.getActive` or `playlist.getPlaying` when you need a single playlist duration.
:::

::: tip v1.1.18 added
The `isAutoplaylist` field is now inlined in `playlist.getAll`; you no longer need per-playlist `playlist.isAutoplaylist` calls.
:::

### playlist.getActive

<!-- api-schema:begin playlist.getActive -->
Describe the active playlist, the one the user is looking at. `found` is `false` when there is none, and the other fields are then absent.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `found` | `boolean` | Whether there is an active playlist. |
| `index` | `integer` | Index of the active playlist. |
| `guid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `name` | `string` | Its name. |
| `trackCount` | `integer` | Number of tracks. |
| `isActive` | `boolean` | Always `true` here. |
| `isPlaying` | `boolean` | Whether it is also the playing playlist. |
| `isLocked` | `boolean` | Whether it carries a lock. |
| `duration` | `number` | Total length of its tracks in seconds; tracks of unknown length count as `0`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### playlist.setActive

<!-- api-schema:begin playlist.setActive -->
Make a playlist the active one. An index past the last playlist fails with `INVALID_INDEX`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist to activate. Give this or `playlistGuid`; with neither the call fails with `INVALID_PARAMS`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Unlike most playlist methods, `playlist` has no active-playlist fallback: omitting it fails with `INVALID_PARAMS`.

```javascript
await fb2k.invoke('playlist.setActive', { playlist: 1 });
```

### playlist.getPlaying

<!-- api-schema:begin playlist.getPlaying -->
Describe the playing playlist, the one playback takes its next track from. `found` is `false` when there is none, and the other fields are then absent.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `found` | `boolean` | Whether there is a playing playlist. |
| `index` | `integer` | Index of the playing playlist. |
| `guid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `name` | `string` | Its name. |
| `trackCount` | `integer` | Number of tracks. |
| `isActive` | `boolean` | Whether it is also the active playlist. |
| `isPlaying` | `boolean` | Always `true` here. |
| `isLocked` | `boolean` | Whether it carries a lock. |
| `duration` | `number` | Total length of its tracks in seconds; tracks of unknown length count as `0`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### playlist.create

<!-- api-schema:begin playlist.create -->
Create an empty playlist. Fails with `OPERATION_FAILED` when foobar2000 refuses to create one.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | `string` | No | Name of the new playlist. Default: `"New Playlist"`. |
| `position` | `integer` | No | Where to insert it in the playlist list; omitted, at the end. At least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `index` | `integer` | Index of the new playlist. |
| `guid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('playlist.create', { name: 'Rock Music' });
```

### playlist.remove

<!-- api-schema:begin playlist.remove -->
Delete a playlist. Fails with `LOCKED` when a lock on the playlist refuses the removal, and with `OPERATION_FAILED` when foobar2000 refuses it otherwise.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist to delete; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### playlist.rename

<!-- api-schema:begin playlist.rename -->
Rename a playlist. Fails with `LOCKED` when a lock on the playlist refuses the new name, and with `OPERATION_FAILED` when foobar2000 refuses it otherwise.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist to rename. An index past the last playlist fails with `INVALID_INDEX`. Give this or `playlistGuid`; with neither the call fails with `INVALID_PARAMS`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `name` | `string` | Yes | New name. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

As with `playlist.setActive`, `playlist` has no active-playlist fallback: omitting it fails with `INVALID_PARAMS`.

```javascript
await fb2k.invoke('playlist.rename', { playlist: 0, name: 'My Favorites' });
```

### playlist.clear

<!-- api-schema:begin playlist.clear -->
Remove every track from a playlist, saving an undo point first. A locked playlist fails with `LOCKED` before anything changes; tracks left behind afterwards fail with `OPERATION_FAILED`, the failure carrying the same fields as a success.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist to empty; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `clearedCount` | `integer` | Number of tracks it held before. |
| `remainingCount` | `integer` | Number of tracks left; `0` on success. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### playlist.duplicate

<!-- api-schema:begin playlist.duplicate -->
Copy a playlist, tracks included, into a new playlist placed right after it.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist to copy; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `name` | `string` | No | Name of the copy; omitted or empty, the original name followed by ` (Copy)`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `index` | `integer` | Index of the copy. |
| `sourcePlaylist` | `integer` | Index of the playlist that was copied. |
| `sourcePlaylistGuid` | `string` | GUID of the playlist that was copied, as `playlistGuid` takes it. |
| `newPlaylist` | `integer` | Index of the copy; the same as `index`. |
| `guid` | `string` | GUID of the copy, as `playlistGuid` takes it. |
| `name` | `string` | Name of the copy. |
| `trackCount` | `integer` | Number of tracks copied. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

## Track operations

### playlist.getTrackCount

<!-- api-schema:begin playlist.getTrackCount -->
Report how many tracks a playlist holds. An index past the last playlist, a `playlistGuid` no playlist has, or no active playlist when both are omitted, reports `0` rather than failing.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written as the results and events that name a playlist report it, for a query that answers an empty result for a missing playlist: a GUID no playlist has answers the same way. Either hex case is accepted. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of tracks. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

A zero count does not tell an empty playlist from one that does not exist.

### playlist.getTracks

<!-- api-schema:begin playlist.getTracks -->
A page of a playlist's rows. Without `fields` each row is a whole playlist row with the play statistics foo_playcount provides; `fields` narrows every row to the named keys, from the list `library.query` takes, and always keeps `index`. A playlist index past the last one, a `playlistGuid` no playlist has, and no active playlist when both are omitted, answer an empty page.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Playlist index; omitted, the active playlist. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written as the results and events that name a playlist report it, for a query that answers an empty result for a missing playlist: a GUID no playlist has answers the same way. Either hex case is accepted. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. |
| `start` | `integer` | No | First row to return. At least `0`. Default: `0`. |
| `count` | `integer` | No | Most rows to return. At least `0`. Default: `100`. |
| `formats` | `Record<string, string>` | No | Extra columns, each a Title Formatting pattern under a name of your choosing; every row carries their values under `formats`, in projected rows too. |
| `fields` | `string[]` | No | Keys each row carries besides `index`; omitted, the whole row. The names are those `library.query` takes; an unknown name fails with `INVALID_PARAMS` and is listed in `details.unknownFields`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | The playlist read, as given or resolved. `-1` when there is none to name: `playlist` was omitted and there is no active playlist, or no playlist has the `playlistGuid`. An index past the last playlist comes back as given. |
| `playlistGuid` | `string` | GUID of the playlist read, as `playlistGuid` takes it; absent when the playlist does not exist, so its absence is what tells a missing playlist from an empty one. |
| `start` | `integer` | The `start` applied. |
| `count` | `integer` | Rows returned. |
| `total` | `integer` | Rows in the playlist; `0` when it does not exist. |
| `tracks` | `PlaylistTrackPartial[]` | The rows from `start`: whole rows, or `index` plus the `fields` asked for. |
| `tracks[].handle` | `string` | The key that identifies the track across endpoints: `absolutePath`, with a `\|subsong:N` suffix when `subsong` is not `0`. The same track yields the same handle from every such method; because of the suffix it is not a plain file path, and a method that takes a path documents whether it accepts one. |
| `tracks[].path` | `string` | Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong suffix. |
| `tracks[].absolutePath` | `string` | Native filesystem path without the subsong suffix; the same as `path` for a remote URL. |
| `tracks[].subsong` | `integer` | Subsong identifier the decoder assigns inside the file, not necessarily a sequence number; `0` for a whole file and for a remote stream. |
| `tracks[].title` | `string` | First TITLE value; empty when untagged. |
| `tracks[].artist` | `string` | Every ARTIST value joined with `", "`; empty when untagged. |
| `tracks[].artists` | `string[]` | Every ARTIST value in tag order; empty when untagged. |
| `tracks[].album` | `string` | First ALBUM value; empty when untagged. |
| `tracks[].albumArtist` | `string` | Every ALBUM ARTIST value joined with `", "`; empty when untagged. |
| `tracks[].albumArtists` | `string[]` | Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`; empty when untagged. `library.getAlbums` files a track that has an `album` under the name `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty (`""` when both are); those are the `name` and `albumArtist` of that album's row. |
| `tracks[].genre` | `string` | Every GENRE value joined with `", "`; empty when untagged. |
| `tracks[].date` | `string` | First DATE value as tagged, such as `2019` or `2019-05-01`; empty when untagged. |
| `tracks[].trackNumber` | `integer` | TRACKNUMBER read as an integer; `0` when absent or not a number. |
| `tracks[].discNumber` | `integer` | DISCNUMBER read as an integer; `0` when absent or not a number. |
| `tracks[].duration` | `number` | Length in seconds; `0` when unknown. |
| `tracks[].fileSize` | `integer` | File size in bytes; `-1` when unknown, as for a remote stream. |
| `tracks[].bitrate` | `integer` | Average bitrate in kbit/s; `0` when unknown. |
| `tracks[].sampleRate` | `integer` | Sample rate in Hz; `0` when unknown. |
| `tracks[].channels` | `integer` | Channel count; `0` when unknown. |
| `tracks[].codec` | `string` | Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. |
| `tracks[].rating` | `integer` | Rating from 0 to 5: the `%rating%` statistic (foo_playcount) when it is 1 to 5, otherwise the RATING tag clamped to that range; `0` when neither rates the track. |
| `tracks[].index` | `integer` | Row in the playlist, from `0`. |
| `tracks[].composer` | `string` | Every COMPOSER value joined with `", "`; empty when untagged. |
| `tracks[].comment` | `string` | First COMMENT value; empty when untagged. |
| `tracks[].playCount` | `integer` | `%play_count%` from foo_playcount; absent when it gives no number, and on rows of `playlist.getSelectedTracks` or of a `fields` request. |
| `tracks[].firstPlayed` | `string` | `%first_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].lastPlayed` | `string` | `%last_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].added` | `string` | `%added%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].formats` | `Record<string, string>` | The `formats` columns of `playlist.getTracks`, by the names given; present only when `formats` was passed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

> `rating` first reads `%rating%` and uses a value from `1` to `5`. If that value is missing or outside this range, it reads the file's `RATING` tag and clamps it to `0`-`5`; no available rating gives `0` (unrated). `rating.get` uses the same rule: `storage` is `'stats'` for an accepted `%rating%` value and `'file'` otherwise, including when no file tag exists.

::: tip Extra columns (`formats`)
`formats` maps column names to Title Formatting patterns; every row, projected rows included, carries the evaluated values under its own `formats` object:

```javascript
const result = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 50,
    formats: {
        myRating: '%rating%',
        codec: '%codec%'
    }
});
// Each row gains formats: { myRating: '5', codec: 'FLAC' }
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

Accepted names are `index`, `handle`, `title`, `artist`, `artists`, `album`, `albumArtist`, `albumArtists`, `genre`, `date`, `trackNumber`, `discNumber`, `duration`, `path`, `absolutePath`, `fileSize`, `bitrate`, `sampleRate`, `channels`, `codec`, `subsong` and `rating`. Names are matched exactly and are case sensitive; an unknown one fails the whole call with `INVALID_PARAMS` and lists the offenders in `details.unknownFields`, so a typo never silently drops a field. `composer`, `comment` and the play statistics are not accepted; they come only with whole rows.

Leaving out `handle`, `absolutePath` and `rating` avoids resolving the filesystem path and evaluating `%rating%` for each row. To get play statistics alongside a projection, ask for them through `formats`.
:::

### playlist.getGroupRuns

<!-- api-schema:begin playlist.getGroupRuns -->
Split a whole playlist into runs of adjacent rows whose group key is equal, with ASCII letters compared case-insensitively; a second pattern sub-groups each run. Rows are never reordered and only the run boundaries come back, so the answer grows with the number of runs, not of rows. Keys are evaluated off the main thread. More than two patterns, an empty pattern or one that fails to compile fails with `INVALID_PARAMS`, the last two with `details.pattern` giving its position. An index past the last playlist fails with `INVALID_INDEX` and `details.playlist` giving the index asked for, a `playlistGuid` no playlist has with `NOT_FOUND` and `details.playlistGuid`; with both omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. An error while the keys are evaluated fails with `OPERATION_FAILED`, the failure carrying an empty `runs` and `total` `0`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `patterns` | `string[]` | Yes | One or two Title Formatting patterns; the second sub-groups each run. Must not be empty. |
| `playlist` | `integer` | No | Playlist index; omitted, the active playlist. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | The playlist grouped. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `total` | `integer` | Rows in the playlist; the runs' `count` values add up to it. |
| `runs` | `PlaylistGroupRun[]` | The runs in playlist order, the first starting at row `0`; empty for an empty playlist. |
| `runs[].start` | `integer` | First row of the run. |
| `runs[].count` | `integer` | Rows in the run. |
| `runs[].key` | `string` | The group key as the first row of the run spells it. |
| `runs[].sub` | `PlaylistGroupSubRun[]` | Second-level runs inside this one; present only when two patterns were given. |
| `runs[].sub[].start` | `integer` | First row of the run. |
| `runs[].sub[].count` | `integer` | Rows in the run. |
| `runs[].sub[].key` | `string` | The second-level key as the first row of the run spells it. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

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

::: tip Driving a grouped virtual list
Pair this with `playlist.getTracks`: the runs provide every header position and let the UI calculate total scroll height; each visible page of rows is fetched separately. Response size depends on the number of runs, key lengths and second-level groups, not on full track metadata. A pattern that puts every track in its own run makes `runs` as long as the playlist, and the host sets no upper bound. Compare `total` with a page response for the same playlist to detect a change in track count. Equal totals do not rule out replacements or reordering between the independent reads.
:::

### playlist.getMatchingRows

<!-- api-schema:begin playlist.getMatchingRows -->
The rows of a playlist whose tracks match a foobar2000 query, in playlist order, in one call however long the playlist is; read the rows themselves with `playlist.getTracksAt`. The query takes the Media Library search syntax without `SORT BY`. A query the parser rejects fails with `INVALID_PARAMS` and `details.param` `query`, and so does one carrying `SORT BY`; many malformed queries are not rejected but simply match nothing. An index past the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`, and with both omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. The answer is a snapshot taken on the main thread: editing the playlist or a track's tags can change it, and so can the passing of time for a query such as `%added% DURING LAST 2 WEEKS`, which no event announces.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | Yes | foobar2000 query, as in the Media Library search; `SORT BY` is refused. Must not be empty. |
| `playlist` | `integer` | No | Playlist index; omitted, the active playlist. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | The playlist searched. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `total` | `integer` | Rows in the playlist when the query ran. Equal to the `total` of a later `playlist.getTracks` does not mean the playlist is unchanged: reordering and tag edits keep the count. |
| `items` | `integer[]` | The matching rows, ascending. |
| `count` | `integer` | Number of entries in `items`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Filtering a long playlist takes two calls however many rows it holds: one for the matching row numbers, then `playlist.getTracksAt` for the rows on screen.

```javascript
// The playlist on screen, by the guid playlist.getAll reported for it
const playlistGuid = '{A9624480-77F0-4A2B-A76F-7EAB0C2EEC1C}';
const hits = await fb2k.invoke('playlist.getMatchingRows', {
    playlistGuid,
    query: 'artist HAS nachi OR title HAS nachi',
});
if (hits.success) {
    const page = await fb2k.invoke('playlist.getTracksAt', {
        playlistGuid,
        rows: hits.items.slice(0, 50),
        fields: ['title', 'artist', 'duration'],
    });
}
```

A lowercase letter in the query matches either case in the tag, an uppercase one only the same case, so lowercase the words a user types. `HAS` matches `*` and `?` literally; `IS` treats them as wildcards. A double quote cannot be written inside a quoted value.

### playlist.getTracksAt

<!-- api-schema:begin playlist.getTracksAt -->
Rows of a playlist picked by row number, in the order given, such as the rows `playlist.getMatchingRows` answers, which need not be adjacent. Each row is shaped as in `playlist.getTracks` and carries its `index`; a row past the last one is skipped, and a row given twice comes back twice. A playlist index past the last one, a `playlistGuid` no playlist has, and no active playlist when both are omitted, answer an empty result as `playlist.getTracks` does.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `rows` | `integer[]` | Yes | Rows to read, in the order to return them. Each item: at least `0`. |
| `playlist` | `integer` | No | Playlist index; omitted, the active playlist. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written as the results and events that name a playlist report it, for a query that answers an empty result for a missing playlist: a GUID no playlist has answers the same way. Either hex case is accepted. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. |
| `formats` | `Record<string, string>` | No | Extra columns, as in `playlist.getTracks`. |
| `fields` | `string[]` | No | Keys each row carries besides `index`, as in `playlist.getTracks`; omitted, the whole row. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | The playlist read, as `playlist` in the result of `playlist.getTracks`. |
| `playlistGuid` | `string` | GUID of the playlist read; absent when it does not exist, as in `playlist.getTracks`. |
| `total` | `integer` | Rows in the playlist; `0` when it does not exist. |
| `count` | `integer` | Rows returned. |
| `tracks` | `PlaylistTrackPartial[]` | The rows in the order asked for, skipping those past the last row. |
| `tracks[].handle` | `string` | The key that identifies the track across endpoints: `absolutePath`, with a `\|subsong:N` suffix when `subsong` is not `0`. The same track yields the same handle from every such method; because of the suffix it is not a plain file path, and a method that takes a path documents whether it accepts one. |
| `tracks[].path` | `string` | Path as foobar2000 stores it: `file://` for a local file, `file-relative://` for a path stored relative to the foobar2000 folder (a portable install), or a remote URL. No subsong suffix. |
| `tracks[].absolutePath` | `string` | Native filesystem path without the subsong suffix; the same as `path` for a remote URL. |
| `tracks[].subsong` | `integer` | Subsong identifier the decoder assigns inside the file, not necessarily a sequence number; `0` for a whole file and for a remote stream. |
| `tracks[].title` | `string` | First TITLE value; empty when untagged. |
| `tracks[].artist` | `string` | Every ARTIST value joined with `", "`; empty when untagged. |
| `tracks[].artists` | `string[]` | Every ARTIST value in tag order; empty when untagged. |
| `tracks[].album` | `string` | First ALBUM value; empty when untagged. |
| `tracks[].albumArtist` | `string` | Every ALBUM ARTIST value joined with `", "`; empty when untagged. |
| `tracks[].albumArtists` | `string[]` | Every ALBUM ARTIST value in tag order, so `albumArtists.join(", ")` equals `albumArtist`; empty when untagged. `library.getAlbums` files a track that has an `album` under the name `album` and the album artist `albumArtists[0]`, or `artists[0]` when this array is empty (`""` when both are); those are the `name` and `albumArtist` of that album's row. |
| `tracks[].genre` | `string` | Every GENRE value joined with `", "`; empty when untagged. |
| `tracks[].date` | `string` | First DATE value as tagged, such as `2019` or `2019-05-01`; empty when untagged. |
| `tracks[].trackNumber` | `integer` | TRACKNUMBER read as an integer; `0` when absent or not a number. |
| `tracks[].discNumber` | `integer` | DISCNUMBER read as an integer; `0` when absent or not a number. |
| `tracks[].duration` | `number` | Length in seconds; `0` when unknown. |
| `tracks[].fileSize` | `integer` | File size in bytes; `-1` when unknown, as for a remote stream. |
| `tracks[].bitrate` | `integer` | Average bitrate in kbit/s; `0` when unknown. |
| `tracks[].sampleRate` | `integer` | Sample rate in Hz; `0` when unknown. |
| `tracks[].channels` | `integer` | Channel count; `0` when unknown. |
| `tracks[].codec` | `string` | Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. |
| `tracks[].rating` | `integer` | Rating from 0 to 5: the `%rating%` statistic (foo_playcount) when it is 1 to 5, otherwise the RATING tag clamped to that range; `0` when neither rates the track. |
| `tracks[].index` | `integer` | Row in the playlist, from `0`. |
| `tracks[].composer` | `string` | Every COMPOSER value joined with `", "`; empty when untagged. |
| `tracks[].comment` | `string` | First COMMENT value; empty when untagged. |
| `tracks[].playCount` | `integer` | `%play_count%` from foo_playcount; absent when it gives no number, and on rows of `playlist.getSelectedTracks` or of a `fields` request. |
| `tracks[].firstPlayed` | `string` | `%first_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].lastPlayed` | `string` | `%last_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].added` | `string` | `%added%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].formats` | `Record<string, string>` | The `formats` columns of `playlist.getTracks`, by the names given; present only when `formats` was passed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### playlist.playTrack

<!-- api-schema:begin playlist.playTrack -->
Play a row of a playlist, as double-clicking it does. A row past the last one fails with `INVALID_INDEX`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `index` | `integer` | No | Row to play. At least `0`. Default: `0`. |
| `deferred` | `boolean` | No | Start the playback after the current message has been handled instead of before the call returns. The playlist is looked up again by its GUID when the playback starts: moved meanwhile, the same playlist plays; removed meanwhile, or shorter than `index` by then, nothing plays. Default: `false`. |
| `muted` | `boolean` | No | Mute before starting, so a page that sets the volume right afterwards leaves no audible gap. The mute is not undone by this call. Default: `false`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`muted: true` mutes before playback starts and never unmutes afterwards, so the player is left muted — restore the volume yourself when the intent was only to suppress the start of the track.

```javascript
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 5 });

// Deferred start, recommended for streaming sources
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 0, deferred: true });
```

### playlist.removeTracks

<!-- api-schema:begin playlist.removeTracks -->
Remove rows from a playlist, saving an undo point first. Rows past the last one are ignored; a negative row fails with `INVALID_PARAMS`. A locked playlist fails with `LOCKED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `items` | `integer[]` | Yes | Rows to remove. Each item: at least `0`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### playlist.removeSelectedTracks

<!-- api-schema:begin playlist.removeSelectedTracks -->
Remove the selected rows from a playlist, saving an undo point first. A locked playlist fails with `LOCKED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

### playlist.moveTracks

<!-- api-schema:begin playlist.moveTracks -->
Move the selected rows of a playlist by `delta` positions, saving an undo point first. With `items`, the selection is first replaced by those rows; the caller's own selection is lost. A locked playlist fails with `LOCKED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `items` | `integer[]` | No | Rows to move; they replace the current selection first. Omitted or empty, the current selection moves. Each item: at least `0`. |
| `delta` | `integer` | Yes | How many positions to move: negative towards the top, positive towards the bottom. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```javascript
await fb2k.invoke('playlist.moveTracks', { items: [0, 1, 2], delta: 3 });
await fb2k.invoke('playlist.moveTracks', { items: [5, 6], delta: -2 });
```

### playlist.addPaths

<!-- api-schema:begin playlist.addPaths -->
Append files, folders or URLs to a playlist, saving an undo point first. foobar2000 resolves the batch as its own Add Files does: folders and playlist files are expanded, duplicates are dropped and the rows are sorted by the user's incoming-item order, so they do not keep the given order (`playlist.addPathsSequential` does). Entries longer than 2048 characters, and entries that resolve to nothing, are counted in `invalidCount`. When nothing is added the call fails with `NOT_FOUND`. A locked playlist fails with `LOCKED`. foobar2000 can show a progress dialog while it resolves the paths, and the playlist is looked up again afterwards: moved meanwhile, the tracks still go into it and `playlist` in the result is its new index; removed meanwhile, the call fails with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `paths` | `string[]` | Yes | Files, folders or URLs; a `\|subsong:N` suffix selects a subsong. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `requestedPaths` | `integer` | Number of entries in `paths`. |
| `addedCount` | `integer` | Tracks added; a folder counts every track it holds. |
| `invalidCount` | `integer` | Entries that were not usable. |
| `countBefore` | `integer` | Number of tracks before the addition. |
| `totalCount` | `integer` | Number of tracks after the addition. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

## Additional public APIs

### playlist.addHandles

<!-- api-schema:begin playlist.addHandles -->
Append tracks to a playlist, saving an undo point first. `handles` takes the same entries as `playlist.insertTracks`; when none is usable the call fails with `NOT_FOUND`. A locked playlist fails with `LOCKED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `handles` | `any[]` | Yes | Tracks to append: each a path with an optional `\|subsong:N` suffix, or `{ path, subsong }`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `requestedCount` | `integer` | Number of entries in `handles`. |
| `addedCount` | `integer` | Tracks appended. |
| `invalidCount` | `integer` | Entries that were not usable. |
| `countBefore` | `integer` | Number of tracks before the addition. |
| `totalCount` | `integer` | Number of tracks after the addition. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.addHandles', { handles: ['C:\\Music\\song.flac'] });
```

### playlist.addPathsAsync

<!-- api-schema:begin playlist.addPathsAsync -->
Start appending paths to a playlist without waiting: the call returns a receipt, and `playlist:addComplete` with the same `operationId` reports the outcome. Plain files and URLs are added before the call returns; playlist files (`.pls`, `.m3u`, `.cue` and the like) are expanded in the background. Entries that are empty or longer than 2048 characters are counted in `invalidCount`; when no entry is left the call fails with `INVALID_PARAMS`. A locked playlist fails with `LOCKED`. The expanded tracks go into the same playlist even if it was moved meanwhile; if it was removed or locked meanwhile they are dropped.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `paths` | `string[]` | Yes | Files, folders, URLs or playlist files; a `\|subsong:N` suffix selects a subsong. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id of the operation; `playlist:addComplete` carries the same id. |
| `playlistGuid` | `string` | GUID of the playlist the paths go to; `playlist:addComplete` carries the same one. |
| `status` | `"pending"` | Always `pending`. |
| `totalCount` | `integer` | Entries that were accepted for processing. |
| `invalidCount` | `integer` | Entries refused before processing: empty or longer than 2048 characters. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.addPathsAsync', { paths: ['C:\\Music\\Album'] });
if (res.success === false) throw new Error(res.error);
const { operationId } = res;
```

### playlist.addPathsSequential

<!-- api-schema:begin playlist.addPathsSequential -->
Append paths to a playlist in the given order, saving an undo point first; a path that expands to several tracks (a folder, a cue sheet) keeps its place. Entries longer than 2048 characters, and entries that resolve to nothing, are skipped. A locked playlist fails with `LOCKED`. A playlist moved or removed while the paths are resolved is handled as in `playlist.addPaths`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `paths` | `string[]` | Yes | Files, folders or URLs, in the order to add them; a `\|subsong:N` suffix selects a subsong. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `addedCount` | `integer` | Tracks added; `0` when nothing resolved, which still succeeds. |
| `order` | `integer[]` | Row of each added track, in the order they were added. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.addPathsSequential', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```

### playlist.convertToAutoplaylist

<!-- api-schema:begin playlist.convertToAutoplaylist -->
Turn an existing playlist into an autoplaylist; its tracks are replaced by the query results. Fails with `OPERATION_FAILED` when foobar2000 refuses, for instance because the playlist is already an autoplaylist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist to convert; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `query` | `string` | Yes | Library query that selects the tracks, in foobar2000's query syntax. Must not be empty. |
| `sort` | `string` | No | Title Formatting pattern to sort the results by; empty, library order. Default: `""`. |
| `keepSorted` | `boolean` | No | Keep the playlist sorted by `sort`: when set, the user cannot reorder its tracks. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.convertToAutoplaylist', { playlist: 0, query: '%genre% IS Rock' });
```

### playlist.createAutoplaylist

<!-- api-schema:begin playlist.createAutoplaylist -->
Create a playlist that foobar2000 fills from a library query and keeps up to date. An autoplaylist foobar2000 refuses to set up fails with `OPERATION_FAILED`, and the new playlist is removed again.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | `string` | No | Name of the new playlist. Default: `"New Autoplaylist"`. |
| `query` | `string` | Yes | Library query that selects the tracks, in foobar2000's query syntax. Must not be empty. |
| `sort` | `string` | No | Title Formatting pattern to sort the results by; empty, library order. Default: `""`. |
| `keepSorted` | `boolean` | No | Keep the playlist sorted by `sort`: when set, the user cannot reorder its tracks. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `index` | `integer` | Index of the new playlist. |
| `playlist` | `integer` | Index of the new playlist; the same as `index`. |
| `guid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `name` | `string` | Name of the new playlist. |
| `query` | `string` | The query, as given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.createAutoplaylist', { name: 'Rock', query: '%genre% IS Rock' });
if (res.success === false) throw new Error(res.error);
const { index } = res;
```

### playlist.deselectAll

<!-- api-schema:begin playlist.deselectAll -->
Clear the selection of a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.deselectAll', { playlist: 0 });
```

### playlist.focusTrack

<!-- api-schema:begin playlist.focusTrack -->
Move the focus of a playlist; the same operation as `playlist.setFocusedTrack` under its older name.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `index` | `integer` | No | Row to focus; omitted or negative, the playlist loses its focus. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.focusTrack', { playlist: 0, index: 3 });
```

### playlist.getAutoplaylistInfo

<!-- api-schema:begin playlist.getAutoplaylistInfo -->
Describe a playlist's autoplaylist status.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `isAutoplaylist` | `boolean` | Whether the playlist is an autoplaylist; the remaining fields other than `playlist` are present only when it is. |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `keepSorted` | `boolean` | Whether the autoplaylist keeps its tracks sorted; always `false` when `source` is `dui`. |
| `source` | `"sdk" \| "dui"` | `sdk` for an autoplaylist foobar2000 runs, `dui` for one another component manages. |
| `lockName` | `string` | Name of the playlist's lock, present when `source` is `dui`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const info = await fb2k.invoke('playlist.getAutoplaylistInfo', { playlist: 0 });
if (info.success === false) throw new Error(info.error);
if (info.isAutoplaylist) console.log(info.source, info.keepSorted);
```

### playlist.getAutoplaylistQuery

<!-- api-schema:begin playlist.getAutoplaylistQuery -->
Describe a playlist's autoplaylist status, as `playlist.getAutoplaylistInfo` does. foobar2000 does not expose the query of an autoplaylist, so `query` is always `null`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `isAutoplaylist` | `boolean` | Whether the playlist is an autoplaylist; `keepSorted`, `source`, `note` and `lockName` are present only when it is. |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `query` | `string \| null` | Always `null`: foobar2000 does not expose the query. |
| `keepSorted` | `boolean` | Whether the autoplaylist keeps its tracks sorted; always `false` when `source` is `dui`. |
| `source` | `"sdk" \| "dui"` | `sdk` for an autoplaylist foobar2000 runs, `dui` for one another component manages. |
| `note` | `string` | Says that the query is not available. |
| `lockName` | `string` | Name of the playlist's lock, present when `source` is `dui`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const q = await fb2k.invoke('playlist.getAutoplaylistQuery', { playlist: 0 });
// q.query is null even when q.isAutoplaylist is true
```

### playlist.getAvailableColumns

<!-- api-schema:begin playlist.getAvailableColumns -->
List the playlist columns that foobar2000 and the installed components define for the Default UI playlist view.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `columns` | `PlaylistColumnDefinition[]` | Every column, component by component. |
| `columns[].id` | `string` | Column GUID, written as `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX` in upper case without braces, unlike a playlist GUID. |
| `columns[].name` | `string` | Display name. |
| `columns[].pattern` | `string` | Title Formatting pattern of the cell text. |
| `columns[].alignment` | `"left" \| "right" \| "center"` | Alignment of the cell text. |
| `columns[].numeric` | `boolean` | Whether the column holds numbers. |
| `columns[].sortPattern` | `string` | Title Formatting pattern used to sort by this column; absent when sorting uses `pattern`. |
| `count` | `integer` | Number of entries in `columns`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playlist.getAvailableColumns');
```

### playlist.getFocusTrack

<!-- api-schema:begin playlist.getFocusTrack -->
Report the focused row of a playlist; the same as `playlist.getFocusedTrack` under its older name, except that a missing playlist fails instead of answering `index` `-1`: an index past the last playlist with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`, and no active playlist when both are omitted with `NO_ACTIVE_ITEM`. On success `playlist` and `playlistGuid` are always present.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `index` | `integer` | Focused row; `-1` when the playlist has no focus. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getFocusTrack', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { index } = res;
```

### playlist.getFocusedTrack

<!-- api-schema:begin playlist.getFocusedTrack -->
Report the focused row of a playlist. An index past the last playlist, a `playlistGuid` no playlist has, or no active playlist when both are omitted, reports `index` `-1` without `playlist` and `playlistGuid` rather than failing.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written as the results and events that name a playlist report it, for a query that answers an empty result for a missing playlist: a GUID no playlist has answers the same way. Either hex case is accepted. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist; absent when there is no such playlist. |
| `playlistGuid` | `string` | GUID of the playlist, as `playlistGuid` takes it; absent when there is no such playlist. |
| `index` | `integer` | Focused row; `-1` when the playlist has no focus or does not exist. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getFocusedTrack', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { index } = res;
```

### playlist.getLockInfo

<!-- api-schema:begin playlist.getLockInfo -->
Report whether a playlist carries a lock, such as an autoplaylist's.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `isLocked` | `boolean` | Whether it carries a lock. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getLockInfo', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { isLocked } = res;
```

### playlist.getSelectedTracks

<!-- api-schema:begin playlist.getSelectedTracks -->
The selected rows of a playlist in playlist order, without the play statistics. An index past the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`; with both omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. Each of these failures, and giving both keys or a malformed GUID, carries an empty `tracks`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Playlist index; omitted, the active playlist. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | The playlist read. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `tracks` | `PlaylistTrack[]` | The selected rows in playlist order. |
| `tracks[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `tracks[].index` | `integer` | Row in the playlist, from `0`. |
| `tracks[].composer` | `string` | Every COMPOSER value joined with `", "`; empty when untagged. |
| `tracks[].comment` | `string` | First COMMENT value; empty when untagged. |
| `tracks[].playCount` | `integer` | `%play_count%` from foo_playcount; absent when it gives no number, and on rows of `playlist.getSelectedTracks` or of a `fields` request. |
| `tracks[].firstPlayed` | `string` | `%first_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].lastPlayed` | `string` | `%last_played%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].added` | `string` | `%added%` as foo_playcount formats it; absent when empty, on rows of `playlist.getSelectedTracks` and on rows of a `fields` request. |
| `tracks[].formats` | `Record<string, string>` | The `formats` columns of `playlist.getTracks`, by the names given; present only when `formats` was passed. |
| `count` | `integer` | Rows returned. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playlist.getSelectedTracks');
```

### playlist.getSelection

<!-- api-schema:begin playlist.getSelection -->
List the selected rows of a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `items` | `integer[]` | Selected rows, in playlist order. |
| `count` | `integer` | Number of entries in `items`. |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getSelection', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { items, count } = res;
```

### playlist.insertTracks

<!-- api-schema:begin playlist.insertTracks -->
Insert tracks at a position of a playlist, saving an undo point first. Each entry of `handles` is a path with an optional `|subsong:N` suffix, or `{ path, subsong }`; entries that are neither, or that name nothing, are counted in `invalidCount`. When none is usable the call fails with `NOT_FOUND`. A locked playlist fails with `LOCKED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `position` | `integer` | No | Row before which the tracks go; past the last row, they are appended. At least `0`. Default: `0`. |
| `handles` | `any[]` | Yes | Tracks to insert: each a path with an optional `\|subsong:N` suffix, or `{ path, subsong }`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `insertIndex` | `integer` | The `position` that was asked for, before it was limited to the end of the playlist. |
| `requestedCount` | `integer` | Number of entries in `handles`. |
| `addedCount` | `integer` | Tracks inserted. |
| `invalidCount` | `integer` | Entries that were not usable. |
| `countBefore` | `integer` | Number of tracks before the insertion. |
| `totalCount` | `integer` | Number of tracks after the insertion. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playlist.insertTracks', {
    playlist: 0,
    position: 5,
    handles: ['C:\\Music\\song.flac'],
});
```

### playlist.isAutoplaylist

<!-- api-schema:begin playlist.isAutoplaylist -->
Report whether a playlist is an autoplaylist: one foobar2000 fills from a library query, or one carrying a lock whose name contains `Auto` (autoplaylists that other components manage).

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `isAutoplaylist` | `boolean` | Whether it is an autoplaylist. |
| `lockName` | `string` | Name of the playlist's lock, present when the playlist carries a named lock and is not an autoplaylist foobar2000 runs, whether or not the lock makes it count as one. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.isAutoplaylist', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { isAutoplaylist } = res;
```

### playlist.isLocked

<!-- api-schema:begin playlist.isLocked -->
Report whether a playlist carries a lock. An index past the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`. Every failure to find the playlist, and giving both keys or a malformed GUID, carries `isLocked` `false`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. With this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `isLocked` | `boolean` | Whether the playlist carries a lock. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.isLocked', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { isLocked } = res;
```

### playlist.redo

<!-- api-schema:begin playlist.redo -->
Reapply the change the last `playlist.undo` reverted. Fails with `NOT_FOUND` when there is nothing to redo, and with `LOCKED` when a lock on the playlist refuses the change.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.redo', { playlist: 0 });
```

### playlist.removeAutoplaylist

<!-- api-schema:begin playlist.removeAutoplaylist -->
Turn an autoplaylist back into an ordinary playlist that keeps its current tracks. An autoplaylist that another component manages cannot be released this way: the call succeeds with `source` `dui` and changes nothing, and `playlist.remove` deletes it. A playlist that is not an autoplaylist fails with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `source` | `"sdk" \| "dui"` | `sdk` when foobar2000's autoplaylist was released, `dui` when the playlist is an autoplaylist another component manages and nothing changed. |
| `note` | `string` | Explanation, present when `source` is `dui`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.removeAutoplaylist', { playlist: 0 });
```

### playlist.reorder

<!-- api-schema:begin playlist.reorder -->
Reorder a playlist's tracks, saving an undo point first: `newOrder[i]` is the current row of the track that moves to row `i`. A locked playlist fails with `LOCKED`, a list whose length is not the track count or that names a row twice with `INVALID_PARAMS`, an entry past the last row with `INVALID_INDEX`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `newOrder` | `integer[]` | Yes | For each new row, the current row of the track that goes there; every row exactly once. Each item: at least `0`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `itemCount` | `integer` | Number of tracks reordered. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// newOrder must be a full permutation of the playlist's current track indices
await fb2k.invoke('playlist.reorder', { playlist: 0, newOrder: [2, 0, 1] });
```

### playlist.reorderPlaylists

<!-- api-schema:begin playlist.reorderPlaylists -->
Reorder the playlist list, giving for each new position either the current index of the playlist that goes there (`newOrder`) or its GUID (`newOrderGuids`); give exactly one of the two, or the call fails with `INVALID_PARAMS`. Indices are read as the call arrives, so indices taken from `playlist.getAll` before another change reorder the wrong playlists without an error as long as the count still matches; GUIDs keep naming the playlists they were read from. A list whose length is not the playlist count fails with `INVALID_PARAMS`, and so does one naming a playlist twice or a malformed GUID; an index past the last playlist fails with `INVALID_INDEX`, a GUID no playlist has with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `newOrder` | `integer[]` | No | For each new position, the current index of the playlist that goes there; every playlist exactly once. Each item: at least `0`. |
| `newOrderGuids` | `string[]` | No | For each new position, the `guid` of the playlist that goes there, as `playlist.getAll` reports it; every playlist exactly once. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of playlists. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// newOrder must be a full permutation of the existing playlist indices
await fb2k.invoke('playlist.reorderPlaylists', { newOrder: [2, 0, 1] });
```

### playlist.replaceAllAndPlay

<!-- api-schema:begin playlist.replaceAllAndPlay -->
Replace the whole content of a playlist and play it: stop playback, clear the playlist (saving an undo point), add the paths as `playlist.addPaths` does, make the playlist active and play or focus `playIndex`. When nothing could be added the call fails with `NOT_FOUND`, and the playlist stays empty. A locked playlist fails with `LOCKED` before anything changes. The paths are resolved after the clear, as in `playlist.addPaths`: a playlist moved meanwhile is still the one filled and played; one removed meanwhile fails with `OPERATION_FAILED`, and one locked meanwhile fails with `LOCKED` and stays empty.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `paths` | `string[]` | Yes | The new content: files, folders or URLs. Must not be empty. |
| `playIndex` | `integer` | No | Row to play, or to focus with `autoPlay` `false`; past the last row, the first one. The rows do not keep the order of `paths`, so this names a position, not one of the given paths. At least `0`. Default: `0`. |
| `stopFirst` | `boolean` | No | Stop playback first, when something is playing. Default: `true`. |
| `autoPlay` | `boolean` | No | Play `playIndex`; `false` only focuses it. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playlist` | `integer` | Index of the playlist. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `clearedCount` | `integer` | Number of tracks removed. |
| `addedCount` | `integer` | Tracks added. |
| `totalCount` | `integer` | Number of tracks afterwards. |
| `playIndex` | `integer` | Row that was played or focused. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.replaceAllAndPlay', { paths: ['C:\\Music\\song.flac'] });
```

### playlist.reverse

<!-- api-schema:begin playlist.reverse -->
Reverse the order of a playlist's tracks, saving an undo point first. A locked playlist fails with `LOCKED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.reverse', { playlist: 0 });
```

### playlist.selectAll

<!-- api-schema:begin playlist.selectAll -->
Select every row of a playlist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.selectAll', { playlist: 0 });
```

### playlist.setFocusedTrack

<!-- api-schema:begin playlist.setFocusedTrack -->
Move the focus of a playlist to a row, or remove it. A row past the last one fails with `INVALID_INDEX`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `index` | `integer` | No | Row to focus; omitted or negative, the playlist loses its focus. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.setFocusedTrack', { playlist: 0, index: 3 });
```

### playlist.setSelection

<!-- api-schema:begin playlist.setSelection -->
Select rows of a playlist. Rows past the last one are ignored; a negative row fails with `INVALID_PARAMS`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `indices` | `integer[]` | Yes | Rows to select; an empty list selects nothing. Each item: at least `0`. |
| `clearOthers` | `boolean` | No | Deselect every other row; `false` adds `indices` to the current selection. Default: `true`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.setSelection', { playlist: 0, indices: [0, 1, 2] });
```

### playlist.shuffle

<!-- api-schema:begin playlist.shuffle -->
Put a playlist's tracks in random order, saving an undo point first. A locked playlist fails with `LOCKED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.shuffle', { playlist: 0 });
```

### playlist.sort

<!-- api-schema:begin playlist.sort -->
Sort a playlist by a Title Formatting pattern, saving an undo point first. A locked playlist fails with `LOCKED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `pattern` | `string` | No | Title Formatting pattern to sort by. Default: `"%title%"`. |
| `descending` | `boolean` | No | Reverse the whole playlist after sorting. The reversal covers every track, the unselected ones too, even with `selectedOnly`. Default: `false`. |
| `selectedOnly` | `boolean` | No | Sort only the selected tracks among themselves, keeping the others in place. Default: `false`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.sort', { playlist: 0, pattern: '%artist% - %title%' });
```

### playlist.undo

<!-- api-schema:begin playlist.undo -->
Revert a playlist to its last undo point. Fails with `NOT_FOUND` when there is none, and with `LOCKED` when a lock on the playlist refuses the change.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Index of the playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

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
