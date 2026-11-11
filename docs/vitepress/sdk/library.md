# `fb.library` media library

> For full-library enumeration, prefer `getCount()` plus paged `getAll(start, count)`, or use `enumerateTracks()`. Use `getRoots()` for real library roots, `browseTree()` for typed directory browsing, and `enumerateTree()` for high-level traversal. `browseDirectory()` and `enumerateDirectories()` are deprecated legacy projection APIs.

## search(query, limit?)

Searches the media library and returns a `LibrarySearchResponse`.

| Parameter | Type | Description |
| --- | --- | --- |
| `query` | `string` | foobar2000 query expression |
| `limit` | `number?` | Maximum result count |
| `options` | `Omit<LibrarySearchParams, 'query' \| 'limit'>?` | Additional native search options (`offset`, `fields`) |
| `options.fields` | `string[]?` | Track keys to project; see [Field projection](#field-projection) |

`tracks` contains the track rows. `hasMore` indicates that more matching rows exist.

```javascript
const results = await fb.library.search('artist HAS Beatles', 100);
if (results.success === false) throw new Error(results.error);
console.log(`Found ${results.total} tracks`);
if (results.hasMore) console.log('More results are available');

// Narrow the projection: each row of `tracks` carries only these keys
const narrow = await fb.library.search('artist HAS Beatles', 500, {
    fields: ['absolutePath', 'album'],
});
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
| `limit` | `number?` | Maximum result count. Caps the artist rows only, never the `albums` inside them |
| `options` | `Omit<LibraryGetArtistsParams, 'limit'>?` | Additional native options: `sort` (`name` / `trackCount` / `albumCount`) and `includeAlbums` |

Resolves to `LibraryArtistsResponse`; each `items` row is an `ArtistInfo` — `{ name, albumCount, trackCount, totalDuration }`, plus `albums?: ArtistAlbumRef[]` (`{ name, artist }`) when `options.includeAlbums` is `true`.

> Every credited artist gets its own entry, so a track tagged with several artists is counted under each of them. `trackCount` is a participation count and the entries add up to more than the total number of tracks; `albumCount` and `totalDuration` are counted per artist the same way.

> `includeAlbums` produces the full artist-to-albums mapping from a single library scan without separate `getArtistAlbums` calls. Each `albums` element's `(name, artist)` matches the identity `getAlbums` groups by (`artist` prioritizes the first `album artist` value, falling back to the first `artist` value). When `albumId` is absent, this pair corresponds to one `getAlbums` row; when present, match by `albumId` rather than display names. Elements are de-duplicated by this identity and sorted by `name`, then `artist`; `albumCount` de-duplicates by album name only, so two same-named albums by different album artists count once in `albumCount` and twice in `albums`.

```javascript
const artists = await fb.library.getArtists(100);

// Artist -> albums mapping for a browser section
const res = await fb.library.getArtists(100000, { includeAlbums: true });
if (res.success === false) throw new Error(res.error);
const { items } = res;
const albumsByArtist = new Map(items.map((a) => [a.name, a.albums]));
```

## getStats()

Returns aggregate statistics such as `totalTracks`, `totalAlbums`, `totalArtists`, `totalDuration`, and `totalSize`.

> `totalArtists` counts credited artists — a track tagged with several artists contributes to each of them — so it matches the entry count of `getArtists()`.

```javascript
const stats = await fb.library.getStats();
if (stats.success === false) throw new Error(stats.error);
console.log(`${stats.totalTracks} tracks, ${stats.totalDuration} seconds`);
```

## getGenres()

Returns `{ success, genres: [{ name, trackCount }] }`.

> Every value of a multi-value `genre` gets its own entry, and `trackCount` is a participation count: a track tagged with several genres is counted under each of them.

```javascript
const r = await fb.library.getGenres();
// {genres: [{name: 'Rock', trackCount: 5000}, ...]}
```

## getStatus()

Returns media-library state, including `initialized` and optional `enabled`, `scanning`, `itemCount`, and `count` fields.

```javascript
const s = await fb.library.getStatus();
if (s.success === false) throw new Error(s.error);
if (s.initialized) console.log('The library is initialized');
```

## getCount()

Returns the media-library item count as `{ count }`.

```javascript
const res = await fb.library.getCount();
if (res.success === false) throw new Error(res.error);
const { count } = res;
```

## getAll(start, count, opts?)

Fetches paged library tracks as `LibraryPagedTracksResponse`, whose rows are `LibraryTrack` objects (containing shared track fields and the library `index`). When the host constructs a full-library list on a background worker thread, the SDK automatically listens for the matching `library:getAllResult` event and resolves to the same consistent structure. Both list construction failures and timeouts resolve with `OPERATION_FAILED` envelopes rather than rejecting; `details.requestId` identifies the request, and timeouts carry `details.timeoutMs`.

| Parameter | Type | Description |
| --- | --- | --- |
| `start` | `number?` | Position of the first track, sent as the host's `offset`; defaults to `0` |
| `count` | `number?` | Maximum number of tracks to return, sent as the host's `limit`; defaults to `100` |
| `opts.useCache` | `boolean?` | When reading from `start: 0`, prefers the host-cached snapshot. Sent only when explicitly specified; host defaults to `true`. Full requests from index 0 are automatically cached by the host |
| `opts.asyncResult` | `boolean?` | Directs the host to build full requests on a background worker thread; defaults to `true` in the SDK (the host native default is `false`). Takes effect only when `useCache` is enabled and cache misses. Set to `false` to build synchronously on the host main thread |
| `opts.timeout` | `number?` | Client-side timeout in milliseconds for the `library:getAllResult` event; defaults to `60000`, and `0` disables it |

When the host builds data off the main thread, it initially returns a `{ pending: true, requestId }` receipt, and the full page payload is delivered through the `library:getAllResult` event. The SDK sets up the listener prior to dispatching the request so early events are never lost, and unbinds the listener automatically on completion, error, or timeout. Callers do not need to subscribe manually. A failure envelope from the call resolves as it is, and the promise rejects only when the call itself rejects.

```javascript
const r = await fb.library.getAll(0, 100);
if (r.success === false) throw new Error(r.error);
console.log(`${r.total} total tracks; received ${r.tracks.length}`);

// Rebuild the whole library list, skipping the kept copy
const all = await fb.library.getAll(0, 1_000_000, { useCache: false, asyncResult: false });
if (all.success === false) {
    console.warn('library.getAll failed or timed out:', all.error, all.details);
} else {
    console.log(all.fromCache, all.tracks.length);
}
```

`items` is a compatibility alias that normally contains the same rows as `tracks`.

> `artists` contains the raw array of atomic tag values behind `artist`: `artists.join(', ')` equals `artist`, so a multi-value tag can be recovered from `artists` but not from `artist`. It is included on every shared `Track` row (the library track rows, `fb.playlist.getTracks`, `fb.player.getCurrentTrack`, `fb.queue.get`, and track events), as well as on the flat result of `getByPath()`; artwork payloads do not carry it. Across all endpoints returning `Track` rows, `albumArtists` follows the same convention for `albumArtist`: its first element (or `artists[0]` when empty) is the album artist that `getAlbums()` groups the track under.

## enumerateTracks(options?)

This high-level async generator pages through the library and supports `pageSize`, `start`, `useCache`, `signal`, and `onProgress`.

```javascript
for await (const page of fb.library.enumerateTracks({ pageSize: 500 })) {
  console.log(page.fetched, '/', page.total);
}
```

## refresh()

Signature: `fb.library.refresh(): Promise<LibraryRefreshResponse>`

Requests foobar2000 to rescan configured library folders. This is equivalent to `rescan()`, invoking a legacy SDK method marked obsolete in the foobar2000 SDK. The library follows file changes on its own, so explicit rescans are rarely needed.

```javascript
await fb.library.refresh();
```

## getByPath(path)

Looks up a library item by path. Metadata fields are returned at the top level when `found` is true.

```javascript
const r = await fb.library.getByPath('E:\\Music\\song.flac');
if (r.success === false) throw new Error(r.error);
if (r.found) console.log(r.title, r.artist);
```

> The flat result carries `artists` alongside `artist`, with the same meaning as on `getAll()` rows: `artists.join(', ')` is exactly `artist`. It has no `albumArtist` / `composer`, so `artists` is the only array key it gains.

## getChanges(params?)

Returns the library `epoch`, `revision`, and handles changed after `since` for local incremental sync. When parameters are omitted or no longer match the host log, returns `resync: true` with full handles. Fails with `OPERATION_PENDING` while the library is still loading at startup. See [Keeping a local copy](../api/library.md#keeping-a-local-copy).

```javascript @ts-nocheck
const r = await fb.library.getChanges({ epoch: mark.epoch, since: mark.revision });
if (r.success === false) throw new Error(r.error);
const changed = r.resync ? [] : r.changed;
```

## getTracks(handles, fields?)

Returns library track rows for up to 5000 handles in the given order. Handles not in the library (or when disabled) are reported in `missing`. Optional `fields` restricts returned keys, as in `query()`. Each handle is resolved on the main thread, so use this for small sets of changed rows; to rebuild the whole copy, page through `search()` as described in [Keeping a local copy](../api/library.md#keeping-a-local-copy).

```javascript @ts-nocheck
const r = await fb.library.getTracks(changed.slice(0, 5000), ['handle', 'title', 'album']);
if (r.success === false) throw new Error(r.error);
console.log(r.tracks.length, r.missing.length);
```

## getRoots()

Returns resolved media-library roots.

```javascript
const res = await fb.library.getRoots();
if (res.success === false) throw new Error(res.error);
const { roots, total, indexedTracks } = res;
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
const res = await fb.library.getRoots();
if (res.success === false) throw new Error(res.error);
const { roots } = res;
const tree = await fb.library.browseTree({ rootId: roots[0].id });
if (tree.success === false) throw new Error(tree.error);
for (const dir of tree.directories) {
  console.log(dir.name, dir.trackCount, dir.hasChildren);
}
// Expand a child and include its direct files
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
| `files` | `LibraryTrack[]` | Files; empty when `includeFiles` is false |
| `fromCache` | `boolean` | Whether the result came from cache |

An empty `rootId` fails with `INVALID_PARAMS`; an unknown `rootId` (`"Unknown rootId"`) or `pathId` (`"Path not found"`) fails with `NOT_FOUND`.

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

## getAlbumTracks(album, albumArtist)

Returns the tracks of an album from `getAlbums`, sorted by disc number, track number, and original library order. Pass the album's `name` and `albumArtist` unchanged, and provide `{ albumId: album.albumId }` as the third argument when available. Without `albumId`, names are compared byte for byte; with an ID, the host resolves tracks by unique identity to prevent name collisions. `total` matches the album's `trackCount`. The row's `artist` is a different key and cannot replace `albumArtist`. If the album artist is empty, pass `albumArtist: ""`. The host caches grouping results until the library changes, avoiding redundant rescans. Empty or malformed identifiers fail with `INVALID_PARAMS` rather than falling back to name matching.

```javascript
const page = await fb.library.getAlbums({ limit: 1 });
const album = page.success ? page.albums[0] : undefined;
if (album) {
    const res = await fb.library.getAlbumTracks(album.name, album.albumArtist, album);
}
```

## getFieldValues(field, limit?, separator?)

Returns distinct values and track counts for a metadata field. `enumerateFieldValues(field, options?)` is a semantic alias that accepts `{ limit?, separator? }`.

```javascript
const years = await fb.library.getFieldValues('date', 50);
const moreYears = await fb.library.enumerateFieldValues('date', { limit: 50 });
```

## query(query, sort?, limit?, fields?)

Runs a foobar2000 query with an optional Title Formatting sort expression. Sorting is applied before `limit` truncation, and `total` reports the untruncated hit count.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | Yes | foobar2000 query expression; an empty value resolves with `success: false` |
| `sort` | `string` | No | Title Formatting sort expression; omit to keep library order |
| `limit` | `number` | No | Result cap (host default `100`) |
| `fields` | `string[]` | No | Track keys to project; see [Field projection](#field-projection) |

```javascript
const r = await fb.library.query('%rating% GREATER 3', '%rating%', 100);

// Path-only projection over a large hit set
const paths = await fb.library.query('%codec% IS FLAC', undefined, 100000, [
    'absolutePath',
]);
```

## Field projection

`query(..., fields)` and `search(query, limit, { fields })` accept an optional list of track keys. Omitting it returns every key of a library track row.

When `fields` is specified, each returned row contains only the requested keys, so rows are typed as `LibraryTrackPartial`. Tracks with unreadable metadata populate non-optional fields with type defaults (empty string or 0). The optional `albumId` is omitted if no album identifier exists, even when it was requested.

**Accepted key names** (exact match, case-sensitive):

`index`, `handle`, `title`, `artist`, `artists`, `album`, `albumId`, `albumArtist`, `albumArtists`, `genre`, `date`, `trackNumber`, `discNumber`, `duration`, `path`, `absolutePath`, `fileSize`, `bitrate`, `sampleRate`, `channels`, `codec`, `subsong`, `rating`

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

> `artists` holds the atomic values behind `artist`: `artists.join(', ')` is exactly `artist`. It is on every shared `Track` row — the library track rows, `fb.playlist.getTracks`, `fb.player.getCurrentTrack`, `fb.queue.get` and the track events — and on the flat answer of `getByPath()`; artwork payloads do not carry it. `albumArtists` does the same for `albumArtist` wherever a `Track` row is returned: its first value, or `artists[0]` when it is empty, is the album artist `getAlbums()` groups the track under. An array may be projected independently of its joined string. For tracks with unreadable metadata, a requested `artists` or `albumArtists` returns `[]`.

Duplicates are de-duplicated. `rating` is only computed when requested (or when the list is omitted).

**Validation** always resolves — the promise is never rejected. An unknown name produces:

```javascript
const bad = await fb.library.query('artist HAS Beatles', undefined, 100, [
    'absolutepath',
    'Rating',
]); // wrong case
// {
//   success: false,
//   error: 'fields contains unknown field names',
//   code: 'INVALID_PARAMS',
//   details: { unknownFields: ['absolutepath', 'Rating'] }
// }
```

`details.unknownFields` appears only for the unknown-name case; the other malformed shapes resolve with `success` / `error` / `code` alone.

**When to use it**

| Scenario | Suggested list |
| --- | --- |
| Filtering tens of thousands of hits and only paths are needed | `['absolutePath']` |
| Search results shown in a UI (hundreds of rows, all columns) | omit the argument |
| Filtering plus per-album grouping | `['absolutePath', 'album']` |

For an 80,000-row result set the single-key projection measured 45.1 MB down to 8.4 MB on the wire and `JSON.parse` in the page from 147 ms down to 37 ms.

### Large result sets

**The host's main thread is occupied in proportion to the response size.** Rows are built on a worker thread; the cost is in handing the finished response to the page, measured at 15–27 ms per MiB. Controlling how many bytes one call returns is the only effective lever, and there are two: projection (`fields`, roughly 4.6× less payload going from full-field to `['absolutePath']`) and paging (`offset` / `limit`, which scales occupancy with the rows in that page).

**Supported access pattern**: up to about 20,000 rows per call after paging and/or projection, which keeps a single call's main-thread occupancy under 100 ms. Past that, page it.

::: warning 32-bit (x86) hosts must avoid large result sets
A 32-bit process has roughly 2–4 GB of user address space, and a whole-library full-field response is resident in several forms at once while it is parsed. Estimated instantaneous peaks: 80,000 rows ≈ 178 MB full-field / ≈ 39 MB projected to `['absolutePath']`; 165,306 rows ≈ 367 MB / ≈ 80 MB. Those stack on the host's existing usage, and `search()` doubles them again. On a 32-bit host, project or page when a query can match tens of thousands of rows; do not issue a full-field request for a whole library. A 64-bit host has no such limit, but the main-thread occupancy applies equally.

The peaks are analytic upper bounds from the parse model, not measured working sets — guidance, not a budget.
:::

## addToPlaylist(paths, playlistIndex?)

Signature: `fb.library.addToPlaylist(paths: string[], playlistIndex?: PlaylistRef): Promise<LibraryAddToPlaylistResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths to append, in this order; a `\|subsong:N` suffix selects a subsong |
| `playlistIndex` | `number \| string` | No | Target playlist: numeric index or `guid` from `fb.playlist.getAll()`; omitted, the active playlist. Supplying a `guid` is recommended when targeting across asynchronous workflows to avoid race conditions with concurrent playlist insertions or deletions |

Appends tracks to a playlist by file path. Target paths need not exist in the library, and missing files are still appended. `added` reports the number of tracks successfully added. A locked playlist fails with `LOCKED` without adding tracks; an out-of-range index fails with `INVALID_INDEX`, and an omitted `playlistIndex` with no active playlist fails with `NO_ACTIVE_ITEM`.

```javascript
await fb.library.addToPlaylist(['E:\\Music\\song.flac'], 0);
```

## getArtistAlbums(artist, limit?, options?)

Signature: `fb.library.getArtistAlbums(artist: string, limit?: number, options?: Omit<LibraryGetArtistAlbumsParams, 'artist' | 'limit'>): Promise<LibraryGetArtistAlbumsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `artist` | `string` | Yes | Artist name, compared byte for byte against each atomic tag value, so it is case-sensitive |
| `limit` | `number` | No | Album cap, applied after grouping (default `100`); there is no `offset` |
| `options.sort` | `string` | No | `name` (default), `artist`, `year` or `trackCount`; an unrecognised value falls back to `name` |
| `options.match` | `'exact' \| 'substring'` | No | `exact` (default) requires the whole atomic value to be byte-identical; `substring` matches on containment and is looser about case; an unrecognised value is refused rather than falling back |

Returns albums featuring the specified artist. Rows carry the same keys as `getAlbums` except `coverDataUrl` and canonical `albumId`, and `tracks` lists the tracks credited to this artist in the group. For cover art, pass `firstTrackAbsolutePath` to `artwork.getForTrack` (`firstTrackPath` may contain an unsupported `file-relative://` URI).

`trackCount`, `duration`, and `discCount` aggregate only the tracks featuring this artist rather than the complete album; these values differ from `getAlbums` when the artist participates on only part of the album. The `tracks` array identifies each participating track member by its `path` and `subsong`. Rows omit `albumId` because the grouping scope differs from `getAlbums`; see [`library.getArtistAlbums`](../api/library.md#library-getartistalbums) for detailed grouping rules.

```javascript
const res = await fb.library.getArtistAlbums('The Beatles', 50);
if (res.success === false) throw new Error(res.error);
const { albums } = res;

// Newest first
const recent = await fb.library.getArtistAlbums('The Beatles', 20, {
    sort: 'year',
});
```

## getArtistTracks(artist, limit?)

Signature: `fb.library.getArtistTracks(artist: string, limit?: number): Promise<LibraryGetArtistTracksResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `artist` | `string` | Yes | Artist name, matched as `getArtistAlbums()` matches it under `match: 'exact'` |
| `limit` | `number` | No | Most tracks to return, the first ones in library order (default `500`) |

Returns tracks featuring the specified artist in library order as `tracks` (`items` is the same list). `count` indicates the returned count, matching `total`. Reaching the `limit` threshold indicates that additional matching tracks may exist. When `artist` is an empty string or the library is disabled, the call succeeds with an empty array.

```javascript
const res = await fb.library.getArtistTracks('The Beatles', 100);
if (res.success === false) throw new Error(res.error);
const { tracks } = res;
```

## getCacheStats()

Signature: `fb.library.getCacheStats(): Promise<LibraryGetCacheStatsResponse>`

Counts of the host's library cache and directory tree index, for diagnostics: whether results are kept (`valid`, `tracksCached`, `artistsCached`, `statsCached`, `albumsCacheEntries`), `cacheHits` and `cacheMisses` since the host started, and the tree index state (`treeIndexValid`, `rootsCached`, `treeIndexedTracks`, `treeSkippedTracks`, `treeLastBuilt`). Times are milliseconds since the Unix epoch. Genres and covers are never kept, so `genresCached` is always `false` and the cover counts are always `0`.

```javascript
const cache = await fb.library.getCacheStats();
if (cache.success === false) throw new Error(cache.error);
console.log(cache.cacheHits, cache.cacheMisses);
```

## getRandomTracks(count?)

Signature: `fb.library.getRandomTracks(count?: number): Promise<LibraryGetRandomTracksResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `count` | `number` | No | Tracks to draw (default `10`); at most the library size is returned |

Tracks drawn at random from the whole library without repeats, a new draw on every call, as `tracks` with their `count`. A disabled or empty library succeeds with no tracks.

```javascript
const res = await fb.library.getRandomTracks(25);
if (res.success === false) throw new Error(res.error);
const { tracks } = res;
```

## getRecentlyAdded(limit?, sortBy?)

Signature: `fb.library.getRecentlyAdded(limit?: number, sortBy?: LibraryGetRecentlyAddedParams['sortBy']): Promise<LibraryGetRecentlyAddedResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `limit` | `number` | No | Most tracks to return (default `50`) |
| `sortBy` | `'added' \| 'modified'` | No | `added` (the default) orders by foo_playcount's `%added%`, with tracks that lack it last; `modified` orders by file modification time |

Returns the newest tracks in the library in descending chronological order. When `added` is requested but no track has `%added%`, the call falls back to sorting by file modification time; the response then reports `sortBy: 'modified'` and `fallback: true`. `total` reflects the current total track count of the library.

```javascript
const res = await fb.library.getRecentlyAdded(50);
if (res.success === false) throw new Error(res.error);
const { tracks, fallback } = res;
```

## invalidateCache()

Signature: `fb.library.invalidateCache(): Promise<LibraryInvalidateCacheResponse>`

Clears cached library query results and directory tree indices from host memory, prompting endpoints to rebuild data on subsequent calls. The host also invalidates this cache automatically upon detected library changes. `timestamp` indicates the Unix millisecond timestamp when the cache was cleared.

```javascript
await fb.library.invalidateCache();
```

## isEnabled()

Signature: `fb.library.isEnabled(): Promise<LibraryIsEnabledResponse>`

Whether the foobar2000 media library is enabled, that is, whether any library folder is configured.

```javascript
const res = await fb.library.isEnabled();
if (res.success === false) throw new Error(res.error);
const { enabled } = res;
```

## rescan()

Signature: `fb.library.rescan(): Promise<LibraryRescanResponse>`

Requests foobar2000 to rescan library folders via `library_manager::rescan()`, an interface marked obsolete in the foobar2000 SDK. The library monitors file modifications automatically, making this call rarely necessary. `refresh()` is an equivalent alias.

```javascript
await fb.library.rescan();
```
