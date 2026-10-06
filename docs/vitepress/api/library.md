# Library API

Methods of the `library` namespace.

## library

### library.addToPlaylist

<!-- api-schema:begin library.addToPlaylist -->
Append tracks to a playlist by path. A path does not have to be in the library, and a file that does not exist is added all the same. A locked playlist fails with `LOCKED` and nothing is added.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Paths to add, in this order; a `\|subsong:N` suffix selects a subsong. Must not be empty. |
| `playlist` | `integer` | No | Index of the target playlist; omitted, the active playlist. An index past the last playlist fails with `INVALID_INDEX`; with this omitted and no active playlist the call fails with `NO_ACTIVE_ITEM`. At least `0`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `added` | `integer` | Tracks added. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Minimal call: append to the active playlist
const res = await fb2k.invoke('library.addToPlaylist', {
    paths: ['C:\\Music\\song.flac', 'C:\\Music\\other.mp3']
});
if (res.success === false) throw new Error(res.error);
const { added } = res;

// Target a specific playlist by index
await fb2k.invoke('library.addToPlaylist', {
    paths: ['C:\\Music\\song.flac'],
    playlist: 0
});
```

### library.browseDirectory

<!-- api-schema:begin library.browseDirectory -->
List the library by path prefix: the folders one level below `path` and, with `includeFiles`, every track under it at any depth. The prefix is matched against the paths as foobar2000 stores them, so a local folder has to be written as `file://` followed by its path; a plain absolute path matches nothing and succeeds with empty lists. `library.browseTree` walks the real library roots instead. Fails with `LIBRARY_DISABLED` when the library is disabled.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Path prefix, compared with ASCII letters case-insensitive; empty matches every track. Default: `""`. |
| `includeFiles` | `boolean` | No | Add every track under `path` as `files`. On by default, so an empty `path` returns the whole library. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `directories` | `string[]` | Folders one level below `path`, as stored paths, in byte order. |
| `items` | `string[]` | The same list as `directories`. |
| `files` | `LibraryTrack[]` | Tracks under `path` in library order; `index` is the position in the library. Empty without `includeFiles`. |
| `files[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `files[].index` | `integer` | Row number; the list the row is in says what it counts. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Top-level directories only
const res = await fb2k.invoke('library.browseDirectory', {
    includeFiles: false
});
if (res.success === false) throw new Error(res.error);
const { directories } = res;

// Descend into one directory, including its tracks
const result = await fb2k.invoke('library.browseDirectory', {
    path: 'C:\\Music'
});
```

### library.browseTree

<!-- api-schema:begin library.browseTree -->
One folder under a library root: its subfolders and, with `includeFiles`, its tracks. Reads the directory tree index `library.getRoots` uses, building it first when needed. An unknown `rootId` or `pathId` fails with `NOT_FOUND`; a failed index build fails with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `rootId` | `string` | Yes | `id` of a root from `library.getRoots`, compared case-insensitively. Must not be empty. |
| `pathId` | `string` | No | `pathId` of the folder to open, as a `directories` entry reports it; empty opens the root. Folder names are separated by `/`, so a path written with `\` is not found. Default: `""`. |
| `includeFiles` | `boolean` | No | Add the folder's tracks as `files`. Default: `false`. |
| `recursiveFiles` | `boolean` | No | With `includeFiles`, add the tracks of every folder below as well. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `root` | `LibraryRootInfo` | The root. |
| `root.id` | `string` | Stable identifier of the root, currently its `absolutePath`; `library.browseTree` takes it. |
| `root.displayName` | `string` | The folder name, or the full path when two roots share a name. |
| `root.rawPath` | `string` | Currently the same as `absolutePath`. |
| `root.absolutePath` | `string` | Canonical local path of the folder. |
| `root.trackCount` | `integer` | Library tracks under the folder. |
| `pathId` | `string` | The `pathId` opened. |
| `absolutePath` | `string` | Local path of the folder. |
| `directories` | `LibraryDirectoryNodeInfo[]` | Subfolders directly inside, ordered by `displayName` and then `absolutePath`, ignoring case. |
| `directories[].id` | `string` | `rootId`, then `::`, then `pathId`. |
| `directories[].rootId` | `string` | The root the folder is under. |
| `directories[].pathId` | `string` | Path below the root with `/` between folder names; pass it to `library.browseTree` to open the folder. |
| `directories[].parentPathId` | `string` | `pathId` of the parent folder; `""` directly under the root. |
| `directories[].name` | `string` | Folder name. |
| `directories[].displayName` | `string` | Currently the same as `name`. |
| `directories[].rawPath` | `string` | Currently the same as `absolutePath`. |
| `directories[].absolutePath` | `string` | Local path of the folder. |
| `directories[].relativePath` | `string` | Currently the same as `pathId`. |
| `directories[].depth` | `integer` | Folder names in `pathId`: `1` directly under the root. |
| `directories[].trackCount` | `integer` | Library tracks in the folder and every folder below it. |
| `directories[].childDirectoryCount` | `integer` | Subfolders directly inside. |
| `directories[].hasChildren` | `boolean` | Whether `childDirectoryCount` is above `0`. |
| `files` | `LibraryTrack[]` | The folder's tracks in library order, followed with `recursiveFiles` by those of the folders below in no fixed order; empty without `includeFiles`. `index` is the position in the library when the tree index was built. |
| `files[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `files[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `fromCache` | `boolean` | `true` when the tree index already existed before this call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('library.getRoots');
if (res.success === false) throw new Error(res.error);
const { roots } = res;

// Minimal call: directory structure of a root, no files
const tree = await fb2k.invoke('library.browseTree', {
    rootId: roots[0].id
});
if (tree.success === false) throw new Error(tree.error);

// Descend into a subdirectory and include its tracks
const withFiles = await fb2k.invoke('library.browseTree', {
    rootId: roots[0].id,
    pathId: tree.directories[0].pathId,
    includeFiles: true
});
```

### library.getAlbumTracks

<!-- api-schema:begin library.getAlbumTracks -->
The tracks of one `library.getAlbums` row, named by the row's `name` and `albumArtist`. Tracks are grouped exactly as `library.getAlbums` groups them, so `total` equals the row's `trackCount`. They are sorted by disc number, then track number, then library order. The grouping is kept until the library changes and `library.getAlbums` builds and reads the same one, so a call after it, or after an earlier call, does not read the library again. A name and album artist that no row has, and a disabled library, succeed with no tracks and no `row`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `album` | `string` | Yes | The row's `name`: the first `album` value of its tracks, compared byte for byte. |
| `albumArtist` | `string` | Yes | The row's `albumArtist`: the first `album artist` value of its tracks, or their first `artist` value when they have no `album artist`, or `""` when they have neither. Compared byte for byte, so the row's `artist` is not a substitute. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `album` | `string` | The album name, as given. |
| `albumArtist` | `string` | The album artist, as given. |
| `row` | `AlbumInfo` | The album's row as `library.getAlbums` returns it, without `coverDataUrl` and `tracks`; absent when no row has this name and album artist. |
| `row.name` | `string` | Album name. |
| `row.artist` | `string` | `albumArtist` when it is set, otherwise the first `artist` value found among the tracks. |
| `row.albumArtist` | `string` | The first `album artist` value found among the tracks, falling back to the track's `artist`; empty when neither exists. |
| `row.trackCount` | `integer` | Tracks counted into the row. |
| `row.discCount` | `integer` | Distinct disc numbers among those tracks; `1` when none carries one. |
| `row.duration` | `number` | Summed length of those tracks, in seconds. |
| `row.year` | `string` | The first `date` value found among the tracks; empty when none has one. |
| `row.genre` | `string` | The first `genre` value found among the tracks; empty when none has one. |
| `row.label` | `string` | The first `publisher` value found among the tracks, or else the first `label` value; empty when none has either. |
| `row.firstTrackPath` | `string` | Path of the first track counted, as foobar2000 stores it; it can be a `file-relative://` URI. |
| `row.firstTrackAbsolutePath` | `string` | `firstTrackPath` as a native file path, the form to pass to `artwork.getForTrack`; absent when there is no first track path. |
| `row.coverDataUrl` | `string` | Front cover as a `data:image/...` URL; only `library.getAlbums` with `includeCover` fills it, and only when a cover exists. |
| `row.tracks` | `AlbumTrackRef[]` | The album's tracks in track-number order; only `library.getAlbums` with `includeTracks` fills it. |
| `row.tracks[].trackNumber` | `integer` | Track number; `0` when the track has none. |
| `row.tracks[].path` | `string` | Track path, as foobar2000 stores it. |
| `row.tracks[].absolutePath` | `string` | `path` as a native file path. |
| `tracks` | `LibraryTrack[]` | The tracks, sorted by disc number, then track number, then library order; `index` is the position in this list. |
| `tracks[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `tracks[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `items` | `LibraryTrack[]` | The same list as `tracks`. |
| `items[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `items[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `total` | `integer` | Number of tracks. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Name the album by a library.getAlbums row
const page = await fb2k.invoke('library.getAlbums', { query: 'Abbey Road', limit: 1 });
if (page.success === false) throw new Error(page.error);
const album = page.albums[0];
if (album) {
    const res = await fb2k.invoke('library.getAlbumTracks', {
        album: album.name,
        albumArtist: album.albumArtist
    });
    if (res.success === false) throw new Error(res.error);
    console.log(res.total === album.trackCount); // true
}
```

A compilation without `album artist` tags is several rows in `library.getAlbums`, one per first `artist` value, and each row gets only its own tracks here. To get the tracks of one artist across albums, use `library.getArtistTracks`.

### library.getAlbums

<!-- api-schema:begin library.getAlbums -->
Albums of the whole library, grouped by album name plus album artist; tracks without an `album` tag are skipped. A complete list (`offset` 0, no `includeTracks`, every album within `limit`) is kept until the library changes, and a later call with the same `query`, `sort` and `includeCover` from `offset` 0 without `includeTracks` is answered from it. The grouping behind the list is kept the same way and shared with `library.getAlbumTracks`, so other calls do not read the library again either until it changes. With the library disabled it succeeds with no albums.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sort` | `string` | No | `name`, `artist` (the album artist), `year` or `trackCount`; any other value sorts by name. Default: `"name"`. |
| `query` | `string` | No | Keep only albums whose name or artist contains this text, compared case-insensitively for ASCII letters; empty keeps every album. Default: `""`. |
| `offset` | `integer` | No | Albums to skip. At least `0`. Default: `0`. |
| `limit` | `integer` | No | Most albums to return. At least `0`. Default: `100`. |
| `includeTracks` | `boolean` | No | Add each album's tracks as `tracks`. Default: `false`. |
| `includeCover` | `boolean` | No | Add each album's front cover as `coverDataUrl`, read from its first track. Default: `false`. |
| `coverMaxSize` | `integer` | No | Largest cover to inline, in KiB; a larger cover is left out. `0` or less inlines any size. Default: `500`. |
| `useCache` | `boolean` | No | Answer from the kept list when there is one; `false` always scans the library, and the result is still kept. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `albums` | `AlbumInfo[]` | The albums of this page, sorted by `sort`. |
| `albums[].name` | `string` | Album name. |
| `albums[].artist` | `string` | `albumArtist` when it is set, otherwise the first `artist` value found among the tracks. |
| `albums[].albumArtist` | `string` | The first `album artist` value found among the tracks, falling back to the track's `artist`; empty when neither exists. |
| `albums[].trackCount` | `integer` | Tracks counted into the row. |
| `albums[].discCount` | `integer` | Distinct disc numbers among those tracks; `1` when none carries one. |
| `albums[].duration` | `number` | Summed length of those tracks, in seconds. |
| `albums[].year` | `string` | The first `date` value found among the tracks; empty when none has one. |
| `albums[].genre` | `string` | The first `genre` value found among the tracks; empty when none has one. |
| `albums[].label` | `string` | The first `publisher` value found among the tracks, or else the first `label` value; empty when none has either. |
| `albums[].firstTrackPath` | `string` | Path of the first track counted, as foobar2000 stores it; it can be a `file-relative://` URI. |
| `albums[].firstTrackAbsolutePath` | `string` | `firstTrackPath` as a native file path, the form to pass to `artwork.getForTrack`; absent when there is no first track path. |
| `albums[].coverDataUrl` | `string` | Front cover as a `data:image/...` URL; only `library.getAlbums` with `includeCover` fills it, and only when a cover exists. |
| `albums[].tracks` | `AlbumTrackRef[]` | The album's tracks in track-number order; only `library.getAlbums` with `includeTracks` fills it. |
| `albums[].tracks[].trackNumber` | `integer` | Track number; `0` when the track has none. |
| `albums[].tracks[].path` | `string` | Track path, as foobar2000 stores it. |
| `albums[].tracks[].absolutePath` | `string` | `path` as a native file path. |
| `total` | `integer` | Albums matching `query`, before `offset` and `limit`. |
| `offset` | `integer` | The `offset` applied. |
| `limit` | `integer` | The `limit` applied. |
| `hasMore` | `boolean` | Whether albums remain after this page. |
| `includeCover` | `boolean` | The `includeCover` applied. |
| `fromCache` | `boolean` | `true` when the answer came from the kept list. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

> Albums are grouped by album name plus the first value of `album artist`, falling back to the first value of `artist` when that tag is absent. A row's `artist` and `albumArtist` both carry whichever of the two the host resolved, so on an album with no `album artist` tag both report the first credited artist of the first track seen. Neither key is a per-track credit, and a multi-value `album artist` contributes only its first value. To list the albums one artist appears on, use `library.getArtistAlbums`.

```js
// Minimal call: first 100 albums sorted by name
const res = await fb2k.invoke('library.getAlbums');
if (res.success === false) throw new Error(res.error);
const { albums, total, hasMore } = res;

// Second page, newest first, with cover thumbnails
const page2 = await fb2k.invoke('library.getAlbums', {
    limit: 50,
    offset: 100,
    sort: 'year',
    includeCover: true,
    coverMaxSize: 300
});

// Filter by album name or album artist
const filtered = await fb2k.invoke('library.getAlbums', { query: 'Beatles' });
```

### library.getAll

<!-- api-schema:begin library.getAll -->
Tracks of the whole library in library order, one page at a time. A request from `offset` 0 that covers every track is kept until the library changes, and a later call from `offset` 0 with `useCache` is answered from it. With `asyncResult`, such a request, when it is not answered from the kept list, is built off the main thread: the call answers `{ pending: true, requestId }` at once and the page arrives as the `library:getAllResult` event on the calling window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `offset` | `integer` | No | Tracks to skip. At least `0`. Default: `0`. |
| `limit` | `integer` | No | Most tracks to return. At least `0`. Default: `100`. |
| `useCache` | `boolean` | No | From `offset` 0, answer from the kept list when there is one. A request from `offset` 0 that covers every track is kept either way. Default: `true`. |
| `asyncResult` | `boolean` | No | Build a request from `offset` 0 that covers every track off the main thread and deliver it as the `library:getAllResult` event; takes effect only with `useCache`, and not when the kept list answers. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `pending` | `boolean` | `true` when the page will arrive as the `library:getAllResult` event; the page fields are then absent. |
| `requestId` | `string` | Id the `library:getAllResult` event carries; present with `pending`. |
| `tracks` | `LibraryTrack[]` | The page in library order; `index` is the position in the library. |
| `tracks[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `tracks[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `items` | `LibraryTrack[]` | The same list as `tracks`. |
| `items[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `items[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `total` | `integer` | Tracks in the library. |
| `offset` | `integer` | The `offset` applied. |
| `limit` | `integer` | The `limit` applied. |
| `fromCache` | `boolean` | `true` when the page came from the kept list. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

> `artists` holds the atomic values behind `artist`: `artists.join(', ')` is exactly `artist`, so a multi-value tag can be recovered from `artists` and not from `artist`. It is on every shared `Track` row — the media-library track APIs, `playlist.getTracks`, `playback.getCurrentTrack`, `queue.get` and the track events — and on the flat answer of `library.getByPath`; artwork payloads do not carry it. `albumArtists` does the same for `albumArtist` wherever a `Track` row is returned: its first value, or `artists[0]` when it is empty, is the album artist `library.getAlbums` groups the track under.

```js
// Minimal call: first 100 tracks
const res = await fb2k.invoke('library.getAll');
if (res.success === false) throw new Error(res.error);
const { items, total } = res;

// Explicit page
const page2 = await fb2k.invoke('library.getAll', { limit: 50, offset: 100 });
```

### library.getArtistAlbums

<!-- api-schema:begin library.getArtistAlbums -->
The albums an artist appears on. `trackCount`, `duration` and `discCount` count only the tracks of this artist, so an album the artist appears on only partly reports smaller figures than in `library.getAlbums`. Rows are grouped by album name alone; tracks without an `album` tag fall under `(Unknown Album)`. Fails with `LIBRARY_DISABLED` when the library is disabled.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `artist` | `string` | Yes | Artist name, compared byte for byte against each value of the `artist` tag, so a name from `library.getArtists` matches even when it is not the first of several credited artists. Must not be empty. |
| `limit` | `integer` | No | Most albums to return, applied after grouping; there is no offset. At least `0`. Default: `100`. |
| `sort` | `string` | No | `name`, `artist`, `year` or `trackCount`, as in `library.getAlbums`; any other value sorts by name. Default: `"name"`. |
| `match` | `"exact" \| "substring"` | No | `exact` needs a whole tag value to equal `artist`; `substring` matches a tag value that contains it, and then a lowercase letter in `artist` matches either case while an uppercase one matches only uppercase. Default: `"exact"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `artist` | `string` | The artist, as given. |
| `albums` | `AlbumInfo[]` | The albums, sorted by `sort`. Rows never carry `coverDataUrl` or `tracks`. |
| `albums[].name` | `string` | Album name. |
| `albums[].artist` | `string` | `albumArtist` when it is set, otherwise the first `artist` value found among the tracks. |
| `albums[].albumArtist` | `string` | The first `album artist` value found among the tracks, falling back to the track's `artist`; empty when neither exists. |
| `albums[].trackCount` | `integer` | Tracks counted into the row. |
| `albums[].discCount` | `integer` | Distinct disc numbers among those tracks; `1` when none carries one. |
| `albums[].duration` | `number` | Summed length of those tracks, in seconds. |
| `albums[].year` | `string` | The first `date` value found among the tracks; empty when none has one. |
| `albums[].genre` | `string` | The first `genre` value found among the tracks; empty when none has one. |
| `albums[].label` | `string` | The first `publisher` value found among the tracks, or else the first `label` value; empty when none has either. |
| `albums[].firstTrackPath` | `string` | Path of the first track counted, as foobar2000 stores it; it can be a `file-relative://` URI. |
| `albums[].firstTrackAbsolutePath` | `string` | `firstTrackPath` as a native file path, the form to pass to `artwork.getForTrack`; absent when there is no first track path. |
| `albums[].coverDataUrl` | `string` | Front cover as a `data:image/...` URL; only `library.getAlbums` with `includeCover` fills it, and only when a cover exists. |
| `albums[].tracks` | `AlbumTrackRef[]` | The album's tracks in track-number order; only `library.getAlbums` with `includeTracks` fills it. |
| `albums[].tracks[].trackNumber` | `integer` | Track number; `0` when the track has none. |
| `albums[].tracks[].path` | `string` | Track path, as foobar2000 stores it. |
| `albums[].tracks[].absolutePath` | `string` | `path` as a native file path. |
| `total` | `integer` | Albums found, before `limit`. |
| `hasMore` | `boolean` | Whether `limit` cut the list short. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

> A row carries the same keys as a `library.getAlbums` row except `coverDataUrl` and `tracks`: `name`, `artist`, `albumArtist`, `trackCount`, `discCount`, `duration`, `year`, `genre`, `label`, `firstTrackPath` and `firstTrackAbsolutePath`. For cover art, pass `firstTrackAbsolutePath` to `artwork.getForTrack` — `firstTrackPath` can be a `file-relative://` URI, which that endpoint refuses. A failure raised by the endpoint itself (library disabled, a failed search) still carries an empty `albums`; a parameter error does not.

> **`trackCount`, `duration` and `discCount` count only the tracks this artist appears on**, not the whole album. `library.getAlbums` returns the album-wide figures for the same album, so the two disagree on any album the artist appears on only partly — a one-track guest spot on a 20-track compilation reports `trackCount: 1`. Rendering a row here as an album card will understate it.

> `total` counts albums before `limit` truncation. There is no `offset`: when `hasMore` is true, the only way to see the rest is a larger `limit`.

> A name from `library.getArtists` matches here even when it is not the first of several credited artists, because the host compares each atomic tag value. The joined `artist` string on a track object is not a valid key — pass one artist name. Under `substring`, a short name also returns albums by other artists whose name contains it, and its case rule is asymmetric: a lowercase letter in the query matches either case in the tag, while an uppercase letter demands uppercase in the same position — `camellia` finds `Camellia`, `CAMELLIA` does not. `exact` is free of this.

> Rows are grouped by album name alone, so identically titled albums by different artists collapse into one row. Tracks whose `album` tag is missing are grouped under `(Unknown Album)`; a tag present but empty forms its own group with an empty name. `library.getAlbums` differs on both counts: it keys on album plus album artist, and skips a track whose album is missing or empty.

```js
const res = await fb2k.invoke('library.getArtistAlbums', {
    artist: 'The Beatles'
});
if (res.success === false) throw new Error(res.error);
const { albums } = res;

// Discography newest first, capped at 20 albums
const recent = await fb2k.invoke('library.getArtistAlbums', {
    artist: 'The Beatles',
    sort: 'year',
    limit: 20
});
```

### library.getArtistTracks

<!-- api-schema:begin library.getArtistTracks -->
Tracks an artist is credited on, in library order. `artist` is matched as `library.getArtistAlbums` matches it under `match: 'exact'`. An empty `artist` and a disabled library both succeed with no tracks.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `artist` | `string` | No | Artist name, compared byte for byte against each value of the `artist` tag; empty, the call succeeds with no tracks. Default: `""`. |
| `limit` | `integer` | No | Most tracks to return; the first ones in library order are kept. At least `0`. Default: `500`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `artist` | `string` | The artist, as given. |
| `tracks` | `LibraryTrack[]` | The tracks in library order; `index` is the position in this list. |
| `tracks[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `tracks[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `items` | `LibraryTrack[]` | The same list as `tracks`. |
| `items[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `items[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `total` | `integer` | The same as `count`; there is no count before `limit`, so a full page means there may be more. |
| `count` | `integer` | Tracks returned. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('library.getArtistTracks', {
    artist: 'The Beatles'
});
if (res.success === false) throw new Error(res.error);
const { items } = res;
```

### library.getArtists

<!-- api-schema:begin library.getArtists -->
Every credited artist with participation counts: each value of a multi-value `artist` gets its own row. The scan is kept until the library changes. Fails with `LIBRARY_DISABLED` when the library is disabled.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sort` | `string` | No | `name`, `trackCount` or `albumCount` (both largest first); any other value keeps name order. Default: `"name"`. |
| `limit` | `integer` | No | Most artists to return. At least `0`. Default: `1000`. |
| `includeAlbums` | `boolean` | No | Add each artist's albums as `albums`. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `items` | `ArtistInfo[]` | The artists, sorted by `sort`. |
| `items[].name` | `string` | The artist. |
| `items[].albumCount` | `integer` | Distinct album names among the artist's tracks. |
| `items[].trackCount` | `integer` | Tracks the artist is credited on. |
| `items[].totalDuration` | `number` | Summed length of those tracks, in seconds. |
| `items[].albums` | `ArtistAlbumRef[]` | The albums the artist is credited on, sorted by name and then artist; present only with `includeAlbums`, and never cut by `limit`. |
| `items[].albums[].name` | `string` | Album name. |
| `items[].albums[].artist` | `string` | The first `album artist` value, or the first `artist` value when there is none. |
| `count` | `integer` | Rows returned, after `limit`; there is no total, so a full page means there may be more. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Each `items` entry is `{ name, albumCount, trackCount, totalDuration }`, plus `albums: [{ name, artist }]` when `includeAlbums` is `true`.

> Every credited artist gets its own entry, so a track tagged with several artists is counted under each of them. `trackCount` is a participation count and the entries add up to more than the total number of tracks; `albumCount` and `totalDuration` are counted per artist the same way.

> An entry `name` is a single atomic value and can be passed straight to `library.getArtistAlbums` or `library.getArtistTracks`. The joined `artist` string on a track object cannot: the host compares each atomic value, so on a multi-value track the joined string matches nothing. On a single-value track it happens to equal the atomic value, which makes it look usable — do not rely on it.

> `albums` gives the "artist → albums" mapping for the whole library in the one scan this endpoint already performs, where calling `library.getArtistAlbums` per artist would scan the library once per artist. Each element's `(name, artist)` is the identity `library.getAlbums` groups by — `artist` is the first `album artist` value, falling back to the first `artist` value — so it matches exactly one `library.getAlbums` row, whose `firstTrackAbsolutePath` then leads to the cover. A track without an `album` tag contributes nothing here, as in `library.getAlbums`. Elements are de-duplicated by `(name, artist)` and sorted by `name`, then `artist`, in byte order. `albumCount` keeps its own rule and de-duplicates by album name only: two same-named albums by different album artists count as 1 in `albumCount` and appear as 2 elements in `albums`. Without `includeAlbums` the response is unchanged from earlier releases — the key is not present.

```js
// Minimal call: up to 1000 artists sorted by name
const res = await fb2k.invoke('library.getArtists');
if (res.success === false) throw new Error(res.error);
const { items } = res;

// Top 50 artists by track count
const top = await fb2k.invoke('library.getArtists', {
    limit: 50,
    sort: 'trackCount'
});

// Artist -> albums mapping in one call (raise limit above the artist count)
const res2 = await fb2k.invoke('library.getArtists', {
    includeAlbums: true,
    limit: 100000
});
if (res2.success === false) throw new Error(res2.error);
const { items: credited } = res2;
for (const artist of credited) {
    for (const album of artist.albums) {
        // album.name / album.artist pair up with a library.getAlbums row
    }
}
```

### library.getByPath

<!-- api-schema:begin library.getByPath -->
Look up one file in the library and return its main fields as a flat object. A file that is not in the library succeeds with `found: false`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File path. It is looked up as the file's first subsong; a `\|subsong:N` suffix is not recognized. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `found` | `boolean` | Whether the file is in the library; the other fields besides `path` are present only when it is. |
| `path` | `string` | The track path as foobar2000 stores it when found; otherwise the path as given. |
| `absolutePath` | `string` | The track path as a native file path. |
| `title` | `string` | The first `title` value; empty when absent. |
| `artist` | `string` | Every `artist` value joined with `, ` in their original order. |
| `artists` | `string[]` | Every `artist` value; `artists.join(', ')` equals `artist`. |
| `album` | `string` | The first `album` value; empty when absent. |
| `duration` | `number` | Length in seconds. |
| `trackNumber` | `string` | The first `tracknumber` value as written in the tag, such as `2` or `02/12`; empty when absent. |
| `genre` | `string` | Every `genre` value joined with `, `. |
| `date` | `string` | The first `date` value; empty when absent. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

> `artists` holds the atomic values behind `artist`: `artists.join(', ')` is exactly `artist`, so a multi-value tag can be recovered from `artists` and not from `artist`. It is on every shared `Track` row — the media-library track APIs, `playlist.getTracks`, `playback.getCurrentTrack`, `queue.get` and the track events — and on the flat answer of `library.getByPath`; artwork payloads do not carry it. `albumArtists` does the same for `albumArtist` wherever a `Track` row is returned: its first value, or `artists[0]` when it is empty, is the album artist `library.getAlbums` groups the track under. This API is a flat object, so `artists` is the only array key it gains.

```js
const res = await fb2k.invoke('library.getByPath', {
    path: 'C:\\Music\\song.flac'
});
if (res.success === false) throw new Error(res.error);
const { found, title } = res;
```

### library.getCacheStats

<!-- api-schema:begin library.getCacheStats -->
Counters of the host's library caches and of the directory tree index, for diagnostics.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `valid` | `boolean` | Whether any library result was kept since the cache was last dropped. |
| `lastModified` | `integer` | When the cache was last dropped, in milliseconds since the Unix epoch. |
| `albumsCacheEntries` | `integer` | Kept `library.getAlbums` lists, one per `query`, `sort` and `includeCover`. |
| `tracksCached` | `boolean` | Whether a full `library.getAll` result is kept. |
| `artistsCached` | `boolean` | Whether the `library.getArtists` scan is kept. |
| `genresCached` | `boolean` | Always `false`; genres are not kept. |
| `statsCached` | `boolean` | Whether the `library.getStatus` answer is kept. |
| `coversCached` | `integer` | Always `0`; covers are not kept. |
| `coverCacheBytes` | `integer` | Always `0`; covers are not kept. |
| `coverCacheMB` | `number` | Always `0`; covers are not kept. |
| `cacheHits` | `integer` | Lookups answered from a kept result since the host started. |
| `cacheMisses` | `integer` | Lookups that found nothing kept since the host started. |
| `treeIndexValid` | `boolean` | Whether the directory tree index is built. |
| `rootsCached` | `integer` | Roots in the tree index; `0` when it is not built. |
| `treeIndexedTracks` | `integer` | Tracks placed in the tree index at its last build. |
| `treeSkippedTracks` | `integer` | Tracks left out of the tree index at its last build. |
| `treeLastBuilt` | `integer` | When the tree index was last built, in milliseconds since the Unix epoch; `0` before the first build. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.getCacheStats');
```

### library.getCount

<!-- api-schema:begin library.getCount -->
Number of tracks in the library.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Tracks in the library. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.getCount');
```

### library.getFieldValues

<!-- api-schema:begin library.getFieldValues -->
Every distinct value of one tag across the library with its track count, most used first. Fails with `LIBRARY_DISABLED` when the library is disabled.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `field` | `string` | Yes | Tag name, such as `genre`. Must not be empty. |
| `separator` | `string` | No | Splits each tag value into several values at this string, trimming spaces and tabs around each; empty, values are taken whole. Default: `""`. |
| `limit` | `integer` | No | Most values to return. At least `0`. Default: `5000`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `values` | `LibraryValueCount[]` | The values, most tracks first. |
| `values[].name` | `string` | The value. |
| `values[].trackCount` | `integer` | Tracks carrying the value. |
| `total` | `integer` | Distinct values found, before `limit`. |
| `field` | `string` | The tag name, as given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Values are returned sorted by descending trackCount
const res = await fb2k.invoke('library.getFieldValues', { field: 'genre' });
if (res.success === false) throw new Error(res.error);
const { values } = res;

// Split multi-value fields and cap the result
const artists = await fb2k.invoke('library.getFieldValues', {
    field: 'artist',
    separator: ';',
    limit: 50
});
```

### library.getGenres

<!-- api-schema:begin library.getGenres -->
Every genre in the library with its track count, ordered by name. Every value of a multi-value `genre` gets its own entry, and a track tagged with several genres is counted under each. Fails with `LIBRARY_DISABLED` when the library is disabled.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `genres` | `LibraryValueCount[]` | Every genre with its track count. |
| `genres[].name` | `string` | The value. |
| `genres[].trackCount` | `integer` | Tracks carrying the value. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.getGenres');
```

### library.getRandomTracks

<!-- api-schema:begin library.getRandomTracks -->
Tracks drawn at random from the whole library without repeats, a new draw on every call. A disabled or empty library succeeds with no tracks.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `integer` | No | Tracks to draw; at most the library size is returned. At least `0`. Default: `10`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tracks` | `LibraryTrack[]` | The drawn tracks; `index` is the position in this list. |
| `tracks[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `tracks[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `count` | `integer` | Tracks returned. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('library.getRandomTracks', { count: 50 });
if (res.success === false) throw new Error(res.error);
const { tracks } = res;
```

### library.getRecentlyAdded

<!-- api-schema:begin library.getRecentlyAdded -->
The newest tracks of the library. `added` orders by the `%added%` field of foo_playcount; when no track has it, the call orders by file modification time instead and says so in `fallback`. The library is walked on every call.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `limit` | `integer` | No | Most tracks to return. At least `0`. Default: `50`. |
| `sortBy` | `"added" \| "modified"` | No | `added` orders by `%added%`, newest first, with tracks that lack it last; `modified` orders by file modification time, newest first. Default: `"added"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tracks` | `RecentLibraryTrack[]` | The tracks, newest first; `index` is the position in the library. |
| `tracks[].…` | [Track](../reference/types.md#track) | Every field of [Track](../reference/types.md#track). |
| `tracks[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `tracks[].added` | `string` | The `%added%` value as foo_playcount formats it, such as `2024-05-01 12:34:56`; present only when the list is ordered by it and the track has one. |
| `tracks[].modified` | `integer` | File modification time, in seconds since the Unix epoch; present only when the list is ordered by it and the time is known. |
| `total` | `integer` | Tracks in the library, not the number returned. |
| `limit` | `integer` | The `limit` applied. |
| `sortBy` | `"added" \| "modified"` | The order used: `modified` when `added` was asked for and no track has `%added%`. |
| `fallback` | `boolean` | Whether the call fell back from `added` to `modified`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Minimal call: 50 most recently added tracks
const res = await fb2k.invoke('library.getRecentlyAdded');
if (res.success === false) throw new Error(res.error);
const { tracks, fallback } = res;

// Sort by file modification time instead
const byMtime = await fb2k.invoke('library.getRecentlyAdded', {
    limit: 50,
    sortBy: 'modified'
});
```

### library.getRoots

<!-- api-schema:begin library.getRoots -->
The library's root folders. Only tracks that resolve to a stable local path count toward the roots; `http://`, `file-relative://`, `unpack://`, `archive://` and similar ones are counted in `skippedTracks`. The first call builds the index synchronously and later calls reuse it until the library changes or `library.invalidateCache` is called. With the library disabled it succeeds with `enabled: false` and no roots; a failed build fails with `OPERATION_FAILED`.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether the media library is enabled. |
| `roots` | `LibraryRootInfo[]` | The root folders. |
| `roots[].id` | `string` | Stable identifier of the root, currently its `absolutePath`; `library.browseTree` takes it. |
| `roots[].displayName` | `string` | The folder name, or the full path when two roots share a name. |
| `roots[].rawPath` | `string` | Currently the same as `absolutePath`. |
| `roots[].absolutePath` | `string` | Canonical local path of the folder. |
| `roots[].trackCount` | `integer` | Library tracks under the folder. |
| `total` | `integer` | Number of roots. |
| `indexedTracks` | `integer` | Tracks placed under a root. |
| `skippedTracks` | `integer` | Tracks left out because they have no stable local path. |
| `fromCache` | `boolean` | `true` when the index already existed before this call. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.getRoots');
```

### library.getStats

<!-- api-schema:begin library.getStats -->
Aggregate counts over the whole library. With the library disabled every count is `0`. The library is walked on every call.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `totalTracks` | `integer` | Tracks in the library. |
| `totalAlbums` | `integer` | Albums, counted by album name plus album artist, as `library.getAlbums` groups them. |
| `totalArtists` | `integer` | Credited artists: every value of a multi-value `artist` counts, matching the entry count of `library.getArtists`. |
| `totalDuration` | `number` | Summed length of all tracks, in seconds. |
| `totalSize` | `integer` | Summed file size, in bytes. |
| `cacheValid` | `boolean` | Whether the host holds cached library results written since the cache was last dropped. |
| `lastModified` | `integer` | When the cache was last dropped, in milliseconds since the Unix epoch. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

> `totalArtists` counts credited artists — a track tagged with several artists contributes to each of them — so it matches the entry count of `library.getArtists`.

```js
const result = await fb2k.invoke('library.getStats');
```

### library.getStatus

<!-- api-schema:begin library.getStatus -->
Whether the library is enabled and how many tracks it holds. The answer is kept until the library changes; with the library disabled nothing is kept.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether the media library is enabled. |
| `initialized` | `boolean` | Always the same as `enabled`. |
| `scanning` | `boolean` | Always `false`; the host does not report scanning. |
| `itemCount` | `integer` | Tracks in the library; `0` when it is disabled. |
| `count` | `integer` | The same as `itemCount`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.getStatus');
```

### library.invalidateCache

<!-- api-schema:begin library.invalidateCache -->
Drop the host's cached library results and the directory tree index; the next call to an endpoint that uses them rebuilds them. They are also dropped whenever the library changes.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `timestamp` | `integer` | When the cache was dropped, in milliseconds since the Unix epoch. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.invalidateCache');
```

### library.isEnabled

<!-- api-schema:begin library.isEnabled -->
Whether the foobar2000 media library is enabled, that is, whether any library folder is configured.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `enabled` | `boolean` | Whether the media library is enabled. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.isEnabled');
```

### library.query

<!-- api-schema:begin library.query -->
Tracks matching a foobar2000 query, optionally sorted by `sort`, up to `limit`. A query the parser rejects fails with `INVALID_PARAMS` and `details.param` `query`, and so does one carrying `SORT BY`; many malformed queries are not rejected but simply match nothing. The rows are written off the main thread. Fails with `LIBRARY_DISABLED` when the library is disabled.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | Yes | foobar2000 query expression. Must not be empty. |
| `sort` | `string` | No | Title Formatting expression to sort the matches by before `limit` applies; empty keeps library order, and an expression that fails to compile is ignored. Default: `""`. |
| `limit` | `integer` | No | Most rows to return; `total` still counts every match. At least `0`. Default: `100`. |
| `fields` | `string[]` | No | Keys each row carries; omitted, every key of a library track row. Names are matched case-sensitively against those keys: `index`, `handle`, `title`, `artist`, `artists`, `album`, `albumArtist`, `albumArtists`, `genre`, `date`, `trackNumber`, `discNumber`, `duration`, `path`, `absolutePath`, `fileSize`, `bitrate`, `sampleRate`, `channels`, `codec`, `subsong` and `rating`. An unknown name fails with `INVALID_PARAMS` and is listed in `details.unknownFields`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tracks` | `LibraryTrackPartial[]` | The matches, sorted by `sort` or in library order; `index` is the position in this list. Each row carries exactly the `fields` asked for. |
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
| `tracks[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `total` | `integer` | Matches, before `limit`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Minimal call
const res = await fb2k.invoke('library.query', {
    query: '%rating% GREATER 3'
});
if (res.success === false) throw new Error(res.error);
const { tracks } = res;

// Sort matches with a titleformat pattern
const sorted = await fb2k.invoke('library.query', {
    query: 'artist HAS Beatles',
    sort: '%album% - %tracknumber%',
    limit: 50
});

// Project a single key: rows come back as { absolutePath } only
const paths = await fb2k.invoke('library.query', {
    query: '%codec% IS FLAC',
    limit: 100000,
    fields: ['absolutePath']
});
```

### library.refresh

<!-- api-schema:begin library.refresh -->
Same as `library.rescan`.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.refresh');
```

### library.rescan

<!-- api-schema:begin library.rescan -->
Ask foobar2000 to rescan the library folders by calling `library_manager::rescan()`, which the foobar2000 SDK marks as obsolete and not to be called. The library follows file changes on its own.

This method takes no parameters.

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('library.rescan');
```

### library.search

<!-- api-schema:begin library.search -->
One page of the tracks matching a foobar2000 query, in library order: a `SORT BY` clause is accepted but not applied. A query the parser rejects fails with `INVALID_PARAMS` and `details.param` `query`. An empty `query` and a disabled library both succeed with no tracks. The rows are written off the main thread.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | No | foobar2000 query expression; empty, the call succeeds with no tracks. Default: `""`. |
| `offset` | `integer` | No | Matches to skip. At least `0`. Default: `0`. |
| `limit` | `integer` | No | Most rows to return. At least `0`. Default: `100`. |
| `fields` | `string[]` | No | Keys each row carries, as for `library.query`. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tracks` | `LibraryTrackPartial[]` | This page of the matches in library order; `index` is the position among all matches. Each row carries exactly the `fields` asked for. |
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
| `tracks[].index` | `integer` | Row number; the list the row is in says what it counts. |
| `total` | `integer` | Matches, before `offset` and `limit`. |
| `offset` | `integer` | The `offset` applied. |
| `limit` | `integer` | The `limit` applied. |
| `hasMore` | `boolean` | Whether matches remain after this page. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Minimal call
const res = await fb2k.invoke('library.search', {
    query: 'artist HAS Beatles'
});
if (res.success === false) throw new Error(res.error);
const { tracks, total, hasMore } = res;

// Second page
const page2 = await fb2k.invoke('library.search', {
    query: '%rating% GREATER 3',
    limit: 50,
    offset: 100
});

// Project two keys; the narrowed rows arrive under `tracks`
const albums = await fb2k.invoke('library.search', {
    query: 'artist HAS Beatles',
    limit: 500,
    fields: ['absolutePath', 'album']
});
```

## Field projection

`library.query` and `library.search` accept an optional `fields` array that
restricts which track keys each returned row carries. Omitting `fields`, or
passing `null`, returns every key of a library track row.

When `fields` is present, each row holds **exactly** the requested keys and no
others — including rows whose metadata container could not be read, where the
requested keys are filled with type defaults (empty string, zero) so that a
requested key is never missing. The response envelope itself is unchanged:
`library.query` still returns `success` / `tracks` / `total`, and
`library.search` still returns `success` / `tracks` / `total` / `offset` /
`limit` / `hasMore`.

**Accepted key names** (exact match, case-sensitive):

`index`, `handle`, `title`, `artist`, `artists`, `album`, `albumArtist`, `albumArtists`,
`genre`, `date`, `trackNumber`, `discNumber`, `duration`, `path`, `absolutePath`, `fileSize`,
`bitrate`, `sampleRate`, `channels`, `codec`, `subsong`, `rating`

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

> `artists` holds the atomic values behind `artist`: `artists.join(', ')` is exactly `artist`, so a multi-value tag can be recovered from `artists` and not from `artist`. It is on every shared `Track` row — the media-library track APIs, `playlist.getTracks`, `playback.getCurrentTrack`, `queue.get` and the track events — and on the flat answer of `library.getByPath`; artwork payloads do not carry it. `albumArtists` does the same for `albumArtist` wherever a `Track` row is returned: its first value, or `artists[0]` when it is empty, is the album artist `library.getAlbums` groups the track under. Projecting an array without its joined string (or the other way round) is allowed; each pair is read in one pass. On a row whose metadata container could not be read, a requested `artists` or `albumArtists` comes back as `[]`.

Duplicate names are de-duplicated. `rating` is only computed when it is
requested (or when `fields` is omitted), which is where most of the saving on
tag-free projections comes from.

**Validation** always *resolves* — it never rejects the promise. A name
outside the list produces:

```js
const bad = await fb2k.invoke('library.query', {
    query: 'artist HAS Beatles',
    fields: ['absolutepath', 'Rating']   // wrong case
});
// {
//   success: false,
//   error: 'fields contains unknown field names',
//   code: 'INVALID_PARAMS',
//   details: { unknownFields: ['absolutepath', 'Rating'] }
// }
```

A value that is not an array, an empty array or a non-string element fails
like any other malformed parameter: `INVALID_PARAMS`, with no `details`.

**When to use it**

| Scenario | Suggested `fields` |
| --- | --- |
| Filtering tens of thousands of hits and only paths are needed | `['absolutePath']` |
| Search results shown in a UI (hundreds of rows, all columns) | omit `fields` |
| Filtering plus per-album grouping | `['absolutePath', 'album']` |

For an 80,000-row result set the single-key projection measured 45.1 MB down to
8.4 MB on the wire and `JSON.parse` in the page from 147 ms down to 37 ms.

### Large result sets

**The host's main thread is occupied in proportion to the response size.** The
cost is not in producing the rows — those are built on a worker thread — but in
handing the finished response to the page, which measured 15–27 ms per MiB.
Controlling how many bytes one call returns is therefore the only effective
lever, and there are two:

| Lever | How | Effect |
| --- | --- | --- |
| Projection | `fields: [...]` | Full-field down to `['absolutePath']` is roughly 4.6× less payload (44.3 → 9.6 MiB at 80,000 rows) |
| Paging | `offset` / `limit` | Occupancy scales with the rows in that page, so it can be brought under any target |

**Supported access pattern**: up to about 20,000 rows per call after paging
and/or projection, which keeps a single call's main-thread occupancy under
100 ms. Past that, page it — do not ask one call for everything.

::: warning 32-bit (x86) hosts must avoid large result sets
A 32-bit process has roughly 2–4 GB of user address space, and a whole-library
full-field response is resident in several forms at once while it is parsed
(host-side UTF-8 string, wide string, materialized value in the renderer).
Estimated instantaneous peaks:

| Rows | Full-field | Projected to `['absolutePath']` |
| ---: | ---: | ---: |
| 80,000 | ≈ 178 MB | ≈ 39 MB |
| 165,306 | ≈ 367 MB | ≈ 80 MB |

Those figures are not automatically fatal, but they stack on top of the host's
existing usage. On a 32-bit host,
project or page when a query can match tens of thousands of rows; do not issue
a full-field request for a whole library. A 64-bit host has no such address
space limit, but the main-thread occupancy above applies equally.

The peaks are analytic upper bounds from the parse model, not measured working
sets — treat them as guidance, not a budget.
:::

## Usage notes

- `asyncResult` defaults to `false`. When it is `true` for a full-library request, the immediate result is `{ pending, requestId }`; the completed `{ requestId, tracks, items, total, offset, limit, fromCache }` payload is delivered to the calling WebView through `library:getAllResult`.
- `library.getRoots` and `library.browseTree` are the typed library-navigation APIs. `library.browseDirectory` is the legacy path-prefix projection and does not represent the real root set.
- `library.getAlbums` adds `coverDataUrl` only when `includeCover` is enabled and artwork is available. It is a `data:image/...` URL, not an `fb2k://` URL.
- `library.search` and `library.query` use foobar2000 query syntax. The implementation relies on `search_filter_v2`; clients should treat invalid expressions as a handler error rather than attempting to parse the syntax locally.
- `library.getStatus` and `library.getCount` enumerate through `enum_items` rather than returning a `metadb_handle_list`. The `library_callback_v2` callback invalidates the cache before it broadcasts the events below.

## Library events

All four events are broadcast to every WebView.

| Event | Payload |
| --- | --- |
| `library:itemsAdded` | `{ count, timestamp }` |
| `library:itemsRemoved` | `{ count, timestamp }` |
| `library:itemsModified` | `{ count, timestamp }` |
| `library:initialized` | `{ timestamp }` |
