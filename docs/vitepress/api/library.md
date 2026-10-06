# Library API

English API reference for the `library` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## library

### library.addToPlaylist


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | File path list to insert. An empty array is rejected with `No paths specified`. |
| `playlist` | `integer` | No | Target playlist index; defaults to the active playlist. |

**Returns**: `{"added":"...","error":"...","success":true}`

```js
// Minimal call: append to the active playlist
const { added } = await fb2k.invoke('library.addToPlaylist', {
    paths: ['C:\\Music\\song.flac', 'C:\\Music\\other.mp3']
});

// Target a specific playlist by index
await fb2k.invoke('library.addToPlaylist', {
    paths: ['C:\\Music\\song.flac'],
    playlist: 0
});
```

### library.browseDirectory


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | No | — | Case-insensitive path prefix. Omit to list top-level directories. |
| `includeFiles` | `boolean` | No | `true` | Set false to return directories only. |

**Returns**: `{"directories":"...","error":"...","files":"...","items":"...","success":true}`

```js
// Top-level directories only
const { directories } = await fb2k.invoke('library.browseDirectory', {
    includeFiles: false
});

// Descend into one directory, including its tracks
const result = await fb2k.invoke('library.browseDirectory', {
    path: 'C:\\Music'
});
```

### library.browseTree


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `rootId` | `string` | Yes | — | Root id from `library.getRoots`. |
| `pathId` | `string` | No | — | Directory id relative to the root. Omit for the root directory. |
| `includeFiles` | `boolean` | No | `false` | When false, `recursiveFiles` is ignored. |
| `recursiveFiles` | `boolean` | No | `false` | Requires `includeFiles: true`. |

**Returns**: `{ "success": true, "root": { ... }, "pathId": "...", "absolutePath": "...", "directories": [...], "files": [...], "fromCache": true }`. Each `directories` entry carries `id`, `rootId`, `pathId`, `parentPathId`, `name`, `displayName`, `rawPath`, `absolutePath`, `relativePath`, `depth`, `trackCount`, `childDirectoryCount`, `hasChildren`. `files` holds standard track objects (same shape as `library.getAll`) when `includeFiles` is set. Failure branches: `rootId is required`, `Unknown rootId`, `Path not found`.

```js
const { roots } = await fb2k.invoke('library.getRoots');

// Minimal call: directory structure of a root, no files
const tree = await fb2k.invoke('library.browseTree', {
    rootId: roots[0].id
});

// Descend into a subdirectory and include its tracks
const withFiles = await fb2k.invoke('library.browseTree', {
    rootId: roots[0].id,
    pathId: tree.directories[0].pathId,
    includeFiles: true
});
```

### library.getAlbumTracks


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `album` | `string` | No | Album name, compared byte for byte, so it is case-sensitive. Omitting it returns an empty result set. |
| `artist` | `string` | No | Narrows the match to this album artist or track artist, compared the same way. |

**Returns**: `{"album":"...","artist":"...","items":"...","success":true,"total":"...","tracks":"..."}`

> Grouping is by album name alone, without the album artist, so identically titled albums by different artists come back as one track list whose length is the sum of the matching `library.getAlbums` rows. Pass `artist` to tell them apart.

```js
// Tracks are returned sorted by track number
const { items } = await fb2k.invoke('library.getAlbumTracks', {
    album: 'Abbey Road'
});

// Disambiguate same-named albums by artist
const scoped = await fb2k.invoke('library.getAlbumTracks', {
    album: 'Greatest Hits',
    artist: 'Queen'
});
```

### library.getAlbums


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `query` | `string` | No | — | Case-insensitive substring matched against album name and album artist. |
| `sort` | `string` | No | `name` | Accepts `name`, `artist`, `year`, `trackCount`. |
| `offset` | `integer` | No | `0` | Page offset. |
| `limit` | `integer` | No | `100` | Page size. |
| `includeTracks` | `boolean` | No | `false` | Adds a per-album `tracks` array and bypasses the cache. |
| `includeCover` | `boolean` | No | `false` | Adds `coverDataUrl` when a cover exists. |
| `coverMaxSize` | `integer` | No | `500` | Cover size cap in KB; larger covers omit `coverDataUrl`. Only used with `includeCover`. |
| `useCache` | `boolean` | No | `true` | Serves cached results until the library changes. |

**Returns**: `{"albums":[],"fromCache":"...","hasMore":true,"includeCover":"...","limit":"...","offset":"...","success":true,"total":"..."}`

> Albums are grouped by album name plus the first value of `album artist`, falling back to the first value of `artist` when that tag is absent. A row's `artist` and `albumArtist` both carry whichever of the two the host resolved, so on an album with no `album artist` tag both report the first credited artist of the first track seen. Neither key is a per-track credit, and a multi-value `album artist` contributes only its first value. To list the albums one artist appears on, use `library.getArtistAlbums`.

```js
// Minimal call: first 100 albums sorted by name
const { albums, total, hasMore } = await fb2k.invoke('library.getAlbums');

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `offset` | `integer` | No | `0` | Page offset. |
| `limit` | `integer` | No | `100` | Page size. |
| `start` | `integer` | No | `0` | Legacy alias of `offset`; takes precedence when both are present. |
| `count` | `integer` | No | `100` | Legacy alias of `limit`; takes precedence when both are present. |
| `useCache` | `boolean` | No | `true` | Serves cached results until the library changes. |
| `asyncResult` | `boolean` | No | `false` | Full-library requests return `{ pending, requestId }` and deliver the result via `library:getAllResult`. |

**Returns**: `{"error":"...","fromCache":"...","items":[],"limit":"...","offset":"...","pending":"...","requestId":"...","total":"...","tracks":[]}`

> `artists` holds the atomic values behind `artist`: `artists.join(', ')` is exactly `artist`, so a multi-value tag can be recovered from `artists` and not from `artist`. It is returned by the media-library track APIs — `library.getAll`, `library.query`, `library.search` and `library.getByPath` among them; the track objects returned by other namespaces (`playlist.getTracks`, `playback.getCurrentTrack`, `queue.get`, artwork payloads, event payloads) do not carry it.

```js
// Minimal call: first 100 tracks
const { items, total } = await fb2k.invoke('library.getAll');

// Explicit page
const page2 = await fb2k.invoke('library.getAll', { limit: 50, offset: 100 });
```

### library.getArtistAlbums


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `artist` | `string` | Yes | — | Artist name. Compared byte for byte against each atomic tag value on a track, so it is case-sensitive; pass a name straight from `library.getArtists`. |
| `limit` | `integer` | No | `100` | Album cap, applied after grouping. There is no `offset`. |
| `sort` | `string` | No | `name` | Accepts `name`, `artist`, `year`, `trackCount`, as in `library.getAlbums`. An unrecognised value falls back to `name`. `artist` sorts on the album artist, which is often one value for every row here. |
| `match` | `string` | No | `exact` | `exact` requires the whole atomic value to be byte-identical; `substring` matches on containment and is looser about case (see below). Any other value is refused — unlike `sort`, which falls back. |

**Returns**: `{"albums":[],"artist":"...","error":"...","hasMore":true,"success":true,"total":"..."}`

> A row carries the same keys as a `library.getAlbums` row except `coverDataUrl` and `tracks`: `name`, `artist`, `albumArtist`, `trackCount`, `discCount`, `duration`, `year`, `genre`, `label`, `firstTrackPath` and `firstTrackAbsolutePath`. For cover art, pass `firstTrackAbsolutePath` to `artwork.getForTrack` — `firstTrackPath` can be a `file-relative://` URI, which that endpoint refuses. Every response carries `albums`, including the failures.

> **`trackCount`, `duration` and `discCount` count only the tracks this artist appears on**, not the whole album. `library.getAlbums` returns the album-wide figures for the same album, so the two disagree on any album the artist appears on only partly — a one-track guest spot on a 20-track compilation reports `trackCount: 1`. Rendering a row here as an album card will understate it.

> `total` counts albums before `limit` truncation. There is no `offset`: when `hasMore` is true, the only way to see the rest is a larger `limit`.

> A name from `library.getArtists` matches here even when it is not the first of several credited artists, because the host compares each atomic tag value. The joined `artist` string on a track object is not a valid key — pass one artist name. Under `substring`, a short name also returns albums by other artists whose name contains it, and its case rule is asymmetric: a lowercase letter in the query matches either case in the tag, while an uppercase letter demands uppercase in the same position — `camellia` finds `Camellia`, `CAMELLIA` does not. `exact` is free of this.

> Rows are grouped by album name alone, so identically titled albums by different artists collapse into one row. Tracks whose `album` tag is missing are grouped under `(Unknown Album)`; a tag present but empty forms its own group with an empty name. `library.getAlbums` differs on both counts: it keys on album plus album artist, and skips a track whose album is missing or empty.

```js
const { albums } = await fb2k.invoke('library.getArtistAlbums', {
    artist: 'The Beatles'
});

// Discography newest first, capped at 20 albums
const recent = await fb2k.invoke('library.getArtistAlbums', {
    artist: 'The Beatles',
    sort: 'year',
    limit: 20
});
```

### library.getArtistTracks


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `artist` | `string` | No | — | Artist name. Compared byte for byte against each atomic tag value on a track, so it is case-sensitive. Omitting it returns an empty result set. |
| `limit` | `integer` | No | `500` | Result cap. |

**Returns**: `{"artist":"...","count":"...","items":"...","success":true,"total":"...","tracks":"..."}`

> Matching works exactly as `library.getArtistAlbums` does under `match: 'exact'`: a name from `library.getArtists` matches even when it is not the first of several credited artists, and an artist differing only in case is not pulled in.

```js
const { items } = await fb2k.invoke('library.getArtistTracks', {
    artist: 'The Beatles'
});
```

### library.getArtists


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `sort` | `string` | No | `name` | Accepts `name`, `trackCount`, `albumCount`. |
| `limit` | `integer` | No | `1000` | Result cap. Caps the artist entries only, never the `albums` inside them. |
| `includeAlbums` | `boolean` | No | `false` | When `true`, every entry carries an `albums` array listing the albums the artist is credited on. |

**Returns**: `{"count":"...","error":"...","items":"...","success":true}`

Each `items` entry is `{ name, albumCount, trackCount, totalDuration }`, plus `albums: [{ name, artist }]` when `includeAlbums` is `true`.

> Every credited artist gets its own entry, so a track tagged with several artists is counted under each of them. `trackCount` is a participation count and the entries add up to more than the total number of tracks; `albumCount` and `totalDuration` are counted per artist the same way.

> An entry `name` is a single atomic value and can be passed straight to `library.getArtistAlbums` or `library.getArtistTracks`. The joined `artist` string on a track object cannot: the host compares each atomic value, so on a multi-value track the joined string matches nothing. On a single-value track it happens to equal the atomic value, which makes it look usable — do not rely on it.

> `albums` gives the "artist → albums" mapping for the whole library in the one scan this endpoint already performs, where calling `library.getArtistAlbums` per artist would scan the library once per artist. Each element's `(name, artist)` is the identity `library.getAlbums` groups by — `artist` is the first `album artist` value, falling back to the first `artist` value — so it matches exactly one `library.getAlbums` row, whose `firstTrackAbsolutePath` then leads to the cover. A track without an `album` tag contributes nothing here, as in `library.getAlbums`. Elements are de-duplicated by `(name, artist)` and sorted by `name`, then `artist`, in byte order. `albumCount` keeps its own rule and de-duplicates by album name only: two same-named albums by different album artists count as 1 in `albumCount` and appear as 2 elements in `albums`. Without `includeAlbums` the response is unchanged from earlier releases — the key is not present.

```js
// Minimal call: up to 1000 artists sorted by name
const { items } = await fb2k.invoke('library.getArtists');

// Top 50 artists by track count
const top = await fb2k.invoke('library.getArtists', {
    limit: 50,
    sort: 'trackCount'
});

// Artist -> albums mapping in one call (raise limit above the artist count)
const { items: credited } = await fb2k.invoke('library.getArtists', {
    includeAlbums: true,
    limit: 100000
});
for (const artist of credited) {
    for (const album of artist.albums) {
        // album.name / album.artist pair up with a library.getAlbums row
    }
}
```

### library.getByPath


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File path to look up. Returns `found: false` when the track is not in the library. |

**Returns**: `{"absolutePath":"...","album":"...","artist":"...","artists":[],"date":"...","duration":"...","error":"...","found":"...","genre":"...","path":"...","success":true,"title":"...","trackNumber":"..."}`

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

> `artists` holds the atomic values behind `artist`: `artists.join(', ')` is exactly `artist`, so a multi-value tag can be recovered from `artists` and not from `artist`. It is returned by the media-library track APIs — `library.getAll`, `library.query`, `library.search` and `library.getByPath` among them; the track objects returned by other namespaces (`playlist.getTracks`, `playback.getCurrentTrack`, `queue.get`, artwork payloads, event payloads) do not carry it. This API is a flat object, so `artists` is the only array key it gains.

```js
const { found, title } = await fb2k.invoke('library.getByPath', {
    path: 'C:\\Music\\song.flac'
});
```

### library.getCacheStats


_No parameters._

**Returns**: cache/tree stats object with keys `valid`, `lastModified`, `albumsCacheEntries`, `tracksCached`, `artistsCached`, `genresCached`, `statsCached`, `coversCached`, `coverCacheBytes`, `coverCacheMB`, `cacheHits`, `cacheMisses`, `treeIndexValid`, `rootsCached`, `treeIndexedTracks`, `treeSkippedTracks`, `treeLastBuilt`.

```js
const result = await fb2k.invoke('library.getCacheStats');
```

### library.getCount


_No parameters._

**Returns**: `{"count":"...","success":true}`

```js
const result = await fb2k.invoke('library.getCount');
```

### library.getFieldValues


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `field` | `string` | Yes | — | Metadata field name to enumerate, for example `genre`. |
| `separator` | `string` | No | — | Splits a single field value into multiple values, for example `;`. |
| `limit` | `integer` | No | `5000` | Result cap. |

**Returns**: `{"error":"...","field":"...","success":true,"total":"...","values":"..."}`

```js
// Values are returned sorted by descending trackCount
const { values } = await fb2k.invoke('library.getFieldValues', { field: 'genre' });

// Split multi-value fields and cap the result
const artists = await fb2k.invoke('library.getFieldValues', {
    field: 'artist',
    separator: ';',
    limit: 50
});
```

### library.getGenres


_No parameters._

**Returns**: `{"error":"...","genres":"...","success":true}`

> Every value of a multi-value `genre` gets its own entry, and `trackCount` is a participation count: a track tagged with several genres is counted under each of them.

```js
const result = await fb2k.invoke('library.getGenres');
```

### library.getRandomTracks


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `count` | `integer` | No | `10` | Capped at the library size. |

**Returns**: `{"count":"...","success":true,"tracks":"..."}`

```js
const { tracks } = await fb2k.invoke('library.getRandomTracks', { count: 50 });
```

### library.getRecentlyAdded


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `limit` | `integer` | No | `50` | Result cap. |
| `sortBy` | `string` | No | `added` | Accepts `added` (requires foo_playcount, falls back to `modified`) or `modified`. |

**Returns**: `{"fallback":"...","limit":"...","sortBy":"...","success":true,"total":"...","tracks":"..."}`

```js
// Minimal call: 50 most recently added tracks
const { tracks, fallback } = await fb2k.invoke('library.getRecentlyAdded');

// Sort by file modification time instead
const byMtime = await fb2k.invoke('library.getRecentlyAdded', {
    limit: 50,
    sortBy: 'modified'
});
```

### library.getRoots


_No parameters._

**Returns**: `{ "success": true, "enabled": true, "roots": [{ "id": "...", "displayName": "...", "rawPath": "...", "absolutePath": "...", "trackCount": 0 }], "total": 3, "indexedTracks": 14930, "skippedTracks": 12, "fromCache": false }`

```js
const result = await fb2k.invoke('library.getRoots');
```

### library.getStats


_No parameters._

**Returns**: `{"cacheValid":"...","lastModified":"...","totalAlbums":"...","totalArtists":"...","totalDuration":"...","totalSize":"...","totalTracks":"..."}`

> `totalArtists` counts credited artists — a track tagged with several artists contributes to each of them — so it matches the entry count of `library.getArtists`.

```js
const result = await fb2k.invoke('library.getStats');
```

### library.getStatus


_No parameters._

**Returns**: `{"count":0,"enabled":true,"initialized":"...","itemCount":"...","scanning":"..."}`

```js
const result = await fb2k.invoke('library.getStatus');
```

### library.invalidateCache


_No parameters._

**Returns**: `{"success":true,"timestamp":"..."}`

```js
const result = await fb2k.invoke('library.invalidateCache');
```

### library.isEnabled


_No parameters._

**Returns**: `{"enabled":"...","success":true}`

```js
const result = await fb2k.invoke('library.isEnabled');
```

### library.query


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `query` | `string` | Yes | — | foobar2000 query expression. |
| `sort` | `string` | No | — | Titleformat pattern used to sort the matches. |
| `limit` | `integer` | No | `100` | Result cap. |
| `fields` | `string[]` | No | all 20 keys | Track keys to project. See [Field projection](#field-projection). |

**Returns**: `{"error":"...","success":true,"total":"...","tracks":"..."}`

```js
// Minimal call
const { tracks } = await fb2k.invoke('library.query', {
    query: '%rating% GREATER 3'
});

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


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('library.refresh');
```

### library.rescan


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('library.rescan');
```

### library.search


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `query` | `string` | No | — | foobar2000 query expression. An empty query returns an empty result set with `success: true`. |
| `offset` | `integer` | No | `0` | Page offset. |
| `limit` | `integer` | No | `100` | Page size. |
| `fields` | `string[]` | No | all 20 keys | Track keys to project. See [Field projection](#field-projection). |

**Returns**: `{"error":"...","hasMore":"...","limit":"...","offset":"...","success":true,"total":"...","tracks":"..."}`

```js
// Minimal call
const { tracks, total, hasMore } = await fb2k.invoke('library.search', {
    query: 'artist HAS Beatles'
});

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
restricts which track keys each returned row carries. Omitting `fields` keeps
the current behaviour: every row carries all 20 keys.

When `fields` is present, each row holds **exactly** the requested keys and no
others — including rows whose metadata container could not be read, where the
requested keys are filled with type defaults (empty string, zero) so that a
requested key is never missing. The response envelope itself is unchanged:
`library.query` still returns `success` / `tracks` / `total`, and
`library.search` still returns `success` / `tracks` / `total` / `offset` /
`limit` / `hasMore`.

**Accepted key names** (exact match, case-sensitive):

`index`, `title`, `artist`, `artists`, `album`, `albumArtist`, `genre`, `date`,
`trackNumber`, `discNumber`, `duration`, `path`, `absolutePath`, `fileSize`,
`bitrate`, `sampleRate`, `channels`, `codec`, `subsong`, `rating`

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

> `artists` holds the atomic values behind `artist`: `artists.join(', ')` is exactly `artist`, so a multi-value tag can be recovered from `artists` and not from `artist`. It is returned by the media-library track APIs — `library.getAll`, `library.query`, `library.search` and `library.getByPath` among them; the track objects returned by other namespaces (`playlist.getTracks`, `playback.getCurrentTrack`, `queue.get`, artwork payloads, event payloads) do not carry it. Projecting `artists` without `artist` (or the other way round) is allowed; both are read in one pass. On a row whose metadata container could not be read, a requested `artists` comes back as `[]`.

Duplicate names are de-duplicated. `rating` is only computed when it is
requested (or when `fields` is omitted), which is where most of the saving on
tag-free projections comes from.

**Validation** is fail-closed and always *resolves* — it never rejects the
promise. A non-array (including an explicit `null`), an empty array, a
non-string element, or any name outside the whitelist produces:

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

`details.unknownFields` is only present for the unknown-name case; the other
malformed shapes resolve with `success` / `error` / `code` alone.

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

- `library.getAll` accepts either `start` or `offset`, and either `count` or `limit`; when both members of a pair are present, `start` and `count` take precedence. The defaults are `0`, `0`, `100`, and `100` respectively. `useCache` defaults to `true`.
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
