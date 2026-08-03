# `fb.library` media library

> For full-library enumeration, prefer `getCount()` plus paged `getAll(start, count)`, or use `enumerateTracks()`. Use `getRoots()` for real library roots, `browseTree()` for typed directory browsing, and `enumerateTree()` for high-level traversal. `browseDirectory()` and `enumerateDirectories()` are deprecated legacy projection APIs.

## search(query, limit?) 

Searches the media library and returns a `LibrarySearchResponse`.

| Parameter | Type | Description |
| --- | --- | --- |
| `query` | `string` | foobar2000 query expression |
| `limit` | `number?` | Maximum result count |
| `options` | `Omit<LibrarySearchParams, 'query' \| 'limit'>?` | Additional native search options |

`tracks` and the compatibility alias `items` contain track rows. `hasMore` indicates that more matching rows exist.

```javascript
const results = await fb.library.search('artist HAS Beatles', 100);
console.log(`Found ${results.total} tracks`);
if (results.hasMore) console.log('More results are available');
```

## getAlbums(limit?) 

Returns album aggregates. Pass either a numeric limit or a `LibraryGetAlbumsParams` object.

```javascript
const albums = await fb.library.getAlbums(50);
// { albums: [{ name, artist, trackCount, duration, ... }], total, offset, limit, hasMore }
```

## getArtists(limit?) 

Returns artist aggregates.

| Parameter | Type | Description |
| --- | --- | --- |
| `limit` | `number?` | Maximum result count |
| `options` | `Omit<LibraryGetArtistsParams, 'limit'>?` | Additional native options |

```javascript
const artists = await fb.library.getArtists(100);
```

## getStats() 

Returns aggregate statistics such as `totalTracks`, `totalAlbums`, `totalArtists`, `totalDuration`, and `totalSize`.

```javascript
const stats = await fb.library.getStats();
console.log(`${stats.totalTracks} tracks, ${stats.totalDuration} seconds`);
```

## getGenres() 

Returns `{ success, genres: [{ name, trackCount }] }`.

```javascript
const r = await fb.library.getGenres();
// {genres: [{name: 'Rock', trackCount: 5000}, ...]}
```

## getStatus() 

Returns media-library state, including `initialized` and optional `enabled`, `scanning`, `itemCount`, and `count` fields.

```javascript
const s = await fb.library.getStatus();
if (s.initialized) console.log('The library is initialized');
```

## getCount() 

Returns the media-library item count as `{ count }`.

```javascript
const { count } = await fb.library.getCount();
```

## getAll(start, count) 

Returns paged tracks as `LibraryPagedTracksResponse`. When the host offloads a full-library request to a background worker, the wrapper waits for the matching `library:getAllResult` event and still resolves to the same final shape.

| Parameter | Type | Description |
| --- | --- | --- |
| `start` | `number?` | Start offset |
| `count` | `number?` | Maximum result count |
| `opts.timeout` | `number?` | Client-side timeout in milliseconds; defaults to `60000`, and `0` disables it |

```javascript
const r = await fb.library.getAll(0, 100);
console.log(`${r.total} total tracks; received ${r.tracks.length}`);
```

`items` is a compatibility alias that normally contains the same rows as `tracks`.

## enumerateTracks(options?)

This high-level async generator pages through the library and supports `pageSize`, `start`, `useCache`, `signal`, and `onProgress`.

```javascript
for await (const page of fb.library.enumerateTracks({ pageSize: 500 })) {
  console.log(page.fetched, '/', page.total);
}
```

## refresh() 

Requests a library refresh and returns a `BaseResponse`.

## getByPath(path) 

Looks up a library item by path. Metadata fields are returned at the top level when `found` is true.

```javascript
const r = await fb.library.getByPath('E:\\Music\\song.flac');
if (r.found) console.log(r.title, r.artist);
```

## getRoots()

Returns resolved media-library roots.

```javascript
const { roots, total, indexedTracks } = await fb.library.getRoots();
for (const root of roots) {
  console.log(root.displayName, root.absolutePath, root.trackCount);
}
```

| Response field | Type | Description |
| --- | --- | --- |
| `roots` | `LibraryRootInfo[]` | Resolved root descriptors |
| `total` | `number` | Root count |
| `indexedTracks` | `number` | Successfully indexed items |
| `skippedTracks` | `number` | Items not assigned to a stable local root |
| `enabled` | `boolean` | Whether the media library is enabled |
| `fromCache` | `boolean` | Whether the result came from cache |

| Root field | Type | Description |
| --- | --- | --- |
| `id` | `string` | Stable identifier; currently the canonical `absolutePath` |
| `displayName` | `string` | Human-readable root name |
| `rawPath` | `string` | Currently identical to `absolutePath` |
| `absolutePath` | `string` | Canonical local absolute path |
| `trackCount` | `number` | Tracks below this root |

Only items that resolve to stable local absolute paths contribute roots. Protocol-backed items such as `http://`, `file-relative://`, and `unpack://` contribute to `skippedTracks`. The first call builds the index; later calls may use the cache, which is invalidated by library changes or `invalidateCache()`.

## browseTree(params)

Browses the typed directory tree using `rootId` and optional `pathId`. Call `getRoots()` first to obtain a valid root ID.

```javascript
const { roots } = await fb.library.getRoots();
const tree = await fb.library.browseTree({ rootId: roots[0].id });
for (const dir of tree.directories) {
  console.log(dir.name, dir.trackCount, dir.hasChildren);
}
// Expand a child and include its direct files.
const sub = await fb.library.browseTree({
  rootId: roots[0].id,
  pathId: tree.directories[0].pathId,
  includeFiles: true
});
```

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `rootId` | `string` | Yes | Root ID from `getRoots().roots[].id` |
| `pathId` | `string` | No | Slash-separated path below the root; defaults to `""` |
| `includeFiles` | `boolean` | No | Include files; defaults to `false` |
| `recursiveFiles` | `boolean` | No | Include descendant files recursively when files are enabled |

| Response field | Type | Description |
| --- | --- | --- |
| `root` | `LibraryRootInfo` | Owning root |
| `pathId` | `string` | Requested path ID |
| `absolutePath` | `string` | Current absolute directory path |
| `directories` | `LibraryDirectoryNodeInfo[]` | Immediate child directories |
| `files` | `TrackInfo[]` | Files; empty when `includeFiles` is false |
| `fromCache` | `boolean` | Whether the result came from cache |

The host reports `"rootId is required"`, `"Unknown rootId"`, or `"Path not found"` for the corresponding invalid requests.

## enumerateTree(options)

Root-aware async generator that performs breadth-first or depth-first traversal through `browseTree()`.

```javascript
for await (const batch of fb.library.enumerateTree({
  rootId: roots[0].id,
  strategy: 'bfs',
  includeFiles: true
})) {
  console.log(batch.pathId, batch.directories.length, batch.files.length);
  console.log(`${batch.visited} visited, ${batch.pending} pending`);
}
```

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `rootId` | `string` | Yes | Root ID from `getRoots()` |
| `pathId` | `string` | No | Starting path; defaults to the root |
| `includeFiles` | `boolean` | No | Include direct files; defaults to `false` |
| `strategy` | `'bfs' \| 'dfs'` | No | Traversal strategy; defaults to `'bfs'` |
| `signal` | `AbortSignal` | No | Cooperative cancellation signal |
| `onProgress` | `Function` | No | Receives `{ rootId, pathId, absolutePath, visited, pending }` |

| Yielded field | Type | Description |
| --- | --- | --- |
| `...browseTreeResponse` | - | Includes `root`, `pathId`, `absolutePath`, `directories`, `files`, and `fromCache` |
| `visited` | `number` | Nodes visited |
| `pending` | `number` | Nodes still queued |

| Final return field | Type | Description |
| --- | --- | --- |
| `rootId` | `string` | Traversed root ID |
| `visited` | `number` | Total nodes visited |
| `aborted` | `boolean` | Whether the signal aborted traversal |

Each yielded batch corresponds to a `browseTree({ recursiveFiles: false })` call, so `files` contains only the current node's direct files.

## browseDirectory(path, includeFiles?)

> Deprecated legacy projection API. Prefer `getRoots()`, `browseTree()`, and `enumerateTree()`.

Returns legacy directory strings and optional track rows.

An empty path means the projected top-level view; it is not the configured foobar2000 root list.

```javascript
const root = await fb.library.browseDirectory('', false);
```

## enumerateDirectories(options?)

> Deprecated legacy async generator built on `browseDirectory()`. Use the typed root APIs for real roots.

Supports breadth-first and depth-first traversal of the legacy projection.

```javascript
for await (const node of fb.library.enumerateDirectories({ rootPath: '', strategy: 'bfs' })) {
  console.log(node.path, node.directories.length);
}
```

## getAlbumTracks(album, artist?)

Returns matching album tracks.

```javascript
const tracks = await fb.library.getAlbumTracks('Abbey Road', 'The Beatles');
```

## getFieldValues(field, limit?, separator?)

Returns distinct values and track counts for a metadata field. `enumerateFieldValues(field, options?)` is a semantic alias that accepts `{ limit?, separator? }`.

```javascript
const years = await fb.library.getFieldValues('date', 50);
const moreYears = await fb.library.enumerateFieldValues('date', { limit: 50 });
```

## query(query, sort?, limit?)

Runs a foobar2000 query with an optional Title Formatting sort expression.

```javascript
const r = await fb.library.query('%rating% GREATER 3', '%rating%', 100);
```

## Supplemental method reference

### addToPlaylist(paths, playlistIndex?)

Signature: `fb.library.addToPlaylist(paths: string[], playlistIndex?: number): Promise<LibraryAddToPlaylistResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths to append |
| `playlistIndex` | `number` | No | Target playlist; omitted for the host default |

Returns the append result, including optional `added` metadata.

```javascript
await fb.library.addToPlaylist(['E:\\Music\\song.flac'], 0);
```

### getArtistAlbums(artist, limit?)

Signature: `fb.library.getArtistAlbums(artist: string, limit?: number): Promise<{ albums: AlbumInfo[] }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `artist` | `string` | Yes | Artist name |
| `limit` | `number` | No | Maximum result count |

Returns the artist's album aggregates.

```javascript
const albums = await fb.library.getArtistAlbums('The Beatles', 50);
```

### getArtistTracks(artist, limit?)

Signature: `fb.library.getArtistTracks(artist: string, limit?: number): Promise<LibraryArtistTracksResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `artist` | `string` | Yes | Artist name |
| `limit` | `number` | No | Maximum result count |

Returns matching tracks plus artist and count metadata.

```javascript
const tracks = await fb.library.getArtistTracks('The Beatles', 100);
```

### getCacheStats()

Signature: `fb.library.getCacheStats(): Promise<LibraryCacheStatsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the extensible library-cache statistics envelope.

```javascript
const cache = await fb.library.getCacheStats();
```

### getRandomTracks(count?)

Signature: `fb.library.getRandomTracks(count?: number): Promise<LibraryRandomTracksResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `number` | No | Number of random tracks |

Returns `{ tracks, count }` plus the base response fields.

```javascript
const random = await fb.library.getRandomTracks(25);
```

### getRecentlyAdded(limit?)

Signature: `fb.library.getRecentlyAdded(limit?: number, sortBy?: string): Promise<LibraryRecentlyAddedResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `limit` | `number` | No | Maximum result count |
| `sortBy` | `string` | No | Host sort selector |

Returns recently added tracks plus `total`, `limit`, `sortBy`, and `fallback` metadata.

```javascript
const recent = await fb.library.getRecentlyAdded(50);
```

### invalidateCache()

Signature: `fb.library.invalidateCache(): Promise<LibraryInvalidateCacheResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the invalidation result and optional timestamp.

```javascript
await fb.library.invalidateCache();
```

### isEnabled()

Signature: `fb.library.isEnabled(): Promise<{ enabled: boolean }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns whether the media library is enabled.

```javascript
const { enabled } = await fb.library.isEnabled();
```

### refresh()

Signature: `fb.library.refresh(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the refresh operation result.

```javascript
await fb.library.refresh();
```

### rescan()

Signature: `fb.library.rescan(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the host rescan operation result.

```javascript
await fb.library.rescan();
```
