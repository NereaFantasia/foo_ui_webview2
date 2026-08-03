# Library API

English API reference for the `library` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## library

### library.addToPlaylist


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | File path list to insert. An empty array is rejected with `No paths specified`. |
| `playlist` | `integer` | No | Optional; target playlist index. Defaults to the active playlist. |

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeFiles` | `boolean` | No | Optional; default true. Set false to return directories only. |
| `path` | `string` | No | Optional case-insensitive path prefix. Omit to list top-level directories. |

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeFiles` | `boolean` | No | Optional; default false. When false, `recursiveFiles` is ignored. |
| `pathId` | `string` | No | Optional directory id relative to the root. Omit for the root directory. |
| `recursiveFiles` | `boolean` | No | Optional; default false. Requires `includeFiles: true`. |
| `rootId` | `string` | Yes | Root id from `library.getRoots`. |

**Returns**: `{"files":[]}`

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
| `album` | `string` | No | Album name, matched exactly. Omitting it returns an empty result set. |
| `artist` | `string` | No | Optional. Narrows the match to this album artist or track artist. |

**Returns**: `{"album":"...","artist":"...","items":"...","success":true,"total":"...","tracks":"..."}`

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `coverMaxSize` | `integer` | No | Optional; default 500. Longest cover edge in pixels; only used with `includeCover`. |
| `includeCover` | `boolean` | No | Optional; default false. Adds `coverDataUrl` when a cover exists. |
| `includeTracks` | `boolean` | No | Optional; default false. Adds a per-album `tracks` array and bypasses the cache. |
| `limit` | `integer` | No | Optional; default 100. |
| `offset` | `integer` | No | Optional; default 0. |
| `query` | `string` | No | Optional case-insensitive substring matched against album name and album artist. |
| `sort` | `string` | No | Optional; default name. Accepts `name`, `artist`, `year`, `trackCount`. |
| `useCache` | `boolean` | No | Optional; default true. |

**Returns**: `{"albums":[],"fromCache":"...","hasMore":true,"includeCover":"...","limit":"...","offset":"...","success":true,"total":"..."}`

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `asyncResult` | `boolean` | No | Optional; default false. Full-library requests return `{ pending, requestId }` and deliver the result via `library:getAllResult`. |
| `count` | `integer` | No | Legacy alias of `limit`; takes precedence when both are present. |
| `limit` | `integer` | No | Optional; default 100. |
| `offset` | `integer` | No | Optional; default 0. |
| `start` | `integer` | No | Legacy alias of `offset`; takes precedence when both are present. |
| `useCache` | `boolean` | No | Optional; default true. |

**Returns**: `{"error":"...","fromCache":"...","items":[],"limit":"...","offset":"...","pending":"...","requestId":"...","total":"...","tracks":[]}`

```js
// Minimal call: first 100 tracks
const { items, total } = await fb2k.invoke('library.getAll');

// Explicit page
const page2 = await fb2k.invoke('library.getAll', { limit: 50, offset: 100 });
```

### library.getArtistAlbums


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `artist` | `string` | Yes | Artist name, matched as a substring. |
| `limit` | `integer` | No | Optional; default 100. |

**Returns**: `{"albums":"...","error":"...","success":true}`

```js
const { albums } = await fb2k.invoke('library.getArtistAlbums', {
    artist: 'The Beatles'
});
```

### library.getArtistTracks


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `artist` | `string` | No | Artist name, matched exactly. Omitting it returns an empty result set. |
| `limit` | `integer` | No | Optional; default 500. |

**Returns**: `{"artist":"...","count":"...","items":"...","success":true,"total":"...","tracks":"..."}`

```js
const { items } = await fb2k.invoke('library.getArtistTracks', {
    artist: 'The Beatles'
});
```

### library.getArtists


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `limit` | `integer` | No | Optional; default 1000. |
| `sort` | `string` | No | Optional; default name. Accepts `name`, `trackCount`, `albumCount`. |

**Returns**: `{"count":"...","error":"...","items":"...","success":true}`

```js
// Minimal call: up to 1000 artists sorted by name
const { items } = await fb2k.invoke('library.getArtists');

// Top 50 artists by track count
const top = await fb2k.invoke('library.getArtists', {
    limit: 50,
    sort: 'trackCount'
});
```

### library.getByPath


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | File path to look up. Returns `found: false` when the track is not in the library. |

**Returns**: `{"absolutePath":"...","album":"...","artist":"...","date":"...","duration":"...","error":"...","found":"...","genre":"...","path":"...","success":true,"title":"...","trackNumber":"..."}`

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `field` | `string` | Yes | Metadata field name to enumerate, for example `genre`. |
| `limit` | `integer` | No | Optional; default 5000. |
| `separator` | `string` | No | Optional. Splits a single field value into multiple values, for example `;`. |

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

```js
const result = await fb2k.invoke('library.getGenres');
```

### library.getRandomTracks


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `integer` | No | Optional; default 10. Capped at the library size. |

**Returns**: `{"count":"...","success":true,"tracks":"..."}`

```js
const { tracks } = await fb2k.invoke('library.getRandomTracks', { count: 50 });
```

### library.getRecentlyAdded


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `limit` | `integer` | No | Optional; default 50. |
| `sortBy` | `string` | No | Optional; default added. Accepts `added` (requires foo_playcount, falls back to `modified`) or `modified`. |

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

**Returns**: `{"fromCache":"..."}`

```js
const result = await fb2k.invoke('library.getRoots');
```

### library.getStats


_No parameters._

**Returns**: `{"cacheValid":"...","lastModified":"...","totalAlbums":"...","totalArtists":"...","totalDuration":"...","totalSize":"...","totalTracks":"..."}`

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `limit` | `integer` | No | Optional; default 100. |
| `query` | `string` | Yes | foobar2000 query expression. |
| `sort` | `string` | No | Optional titleformat pattern used to sort the matches. |

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `limit` | `integer` | No | Optional; default 100. |
| `offset` | `integer` | No | Optional; default 0. |
| `query` | `string` | No | foobar2000 query expression. An empty query returns an empty result set with `success: true`. |

**Returns**: `{"error":"...","hasMore":"...","items":"...","limit":"...","offset":"...","success":true,"total":"...","tracks":"..."}`

```js
// Minimal call
const { items, total, hasMore } = await fb2k.invoke('library.search', {
    query: 'artist HAS Beatles'
});

// Second page
const page2 = await fb2k.invoke('library.search', {
    query: '%rating% GREATER 3',
    limit: 50,
    offset: 100
});
```

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
