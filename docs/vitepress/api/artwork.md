# Artwork API

Methods of the `artwork` namespace.

## artwork

### artwork.getAvailableArtwork

<!-- api-schema:begin artwork.getAvailableArtwork -->
Report which picture types a file embeds and which cover files sit next to it.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix is dropped. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Whether the file embeds at least one picture. |
| `artworks` | `ArtworkAvailableEntry[]` | The embedded pictures, in probe order. |
| `artworks[].type` | `string` | Picture type. |
| `artworks[].source` | `string` | Where it comes from; always `embedded`, since only embedded pictures are listed. |
| `sources` | `string[]` | `embedded` once when any picture is embedded, then `folder:<name>` for each cover file (`cover`, `folder`, `front`, `album` as `.jpg` or `.png`) found next to the file. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getAvailableArtwork', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { artworks, sources } = res;
```

### artwork.getAvailableTypes

<!-- api-schema:begin artwork.getAvailableTypes -->
List the picture types embedded in a file, in the fixed probe order front, back, disc, icon, artist. Without `path` the playing track is used; with nothing playing the call fails with `NO_ACTIVE_ITEM`. A file that is missing, cannot be read or has a format without embedded pictures is not an error: it lists no types.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Track path; omitted for the playing track. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `types` | `string[]` | The embedded picture types, in probe order. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getAvailableTypes', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { types } = res;
```

### artwork.getBatch

<!-- api-schema:begin artwork.getBatch -->
Read one picture type from several files with the album art extractor, one row per path in the given order; a file without the picture gets a row with `available: false`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths; a `\|subsong:N` suffix is dropped. |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type for every path. Default: `"front"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `artworks` | `ArtworkBatchRow[]` | One row per path, in the given order. |
| `artworks[].path` | `string` | The path as given. |
| `artworks[].available` | `boolean` | Whether the file has the picture. |
| `artworks[].mimeType` | `string` | MIME type detected from the bytes. |
| `artworks[].size` | `integer` | Picture size in bytes. |
| `artworks[].dataUrl` | `string` | `data:<mime>;base64,...` URL of the picture. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getBatch', {
	paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'],
});
if (res.success === false) throw new Error(res.error);
const { artworks } = res;
```

### artwork.getByPath

<!-- api-schema:begin artwork.getByPath -->
Read a picture straight from the file at `path` with the album art extractor. The `|subsong:N` suffix is dropped, since pictures belong to the file; a `file-relative://` path is refused with `INVALID_PATH`, because only a playlist row can resolve it.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path: native, `file://`, or with a `\|subsong:N` suffix. Must not be empty. |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type. Default: `"front"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Whether the file has the picture. |
| `type` | `string` | The requested type. |
| `path` | `string` | The path as given. |
| `mimeType` | `string` | MIME type detected from the bytes. |
| `size` | `integer` | Picture size in bytes. |
| `dataUrl` | `string` | `data:<mime>;base64,...` URL of the picture. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getByPath', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { available, dataUrl } = res;
```

### artwork.getByPlaylistItem

<!-- api-schema:begin artwork.getByPlaylistItem -->
Read the picture of a playlist row through the album art manager. A negative `playlist` means the active playlist and a negative `index` means the first row. A row past the end, an empty playlist included, fails with `NOT_FOUND`. A playlist index past the last playlist fails with `INVALID_INDEX`, a `playlistGuid` no playlist has with `NOT_FOUND`, and no active playlist when the active one is meant with `NO_ACTIVE_ITEM`. A row without the picture is a success with `available` `false`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `playlist` | `integer` | No | Playlist index; negative for the active playlist. An index past the last playlist fails with `INVALID_INDEX`. Default: `-1`. |
| `playlistGuid` | `string` | No | A playlist's GUID instead of its index, written with braces as `playlist.getAll` and every result or event that names a playlist report it; either hex case is accepted. It keeps naming the same playlist while other playlists are added, removed or reordered, which an index does not. Giving both this and the index, or a malformed GUID, fails with `INVALID_PARAMS`. A playlist that no longer exists fails with `NOT_FOUND`; the call never falls back to another playlist. A malformed GUID and a missing playlist carry the GUID given as `details.playlistGuid`. |
| `index` | `integer` | No | Row index; negative for the first row. Default: `-1`. |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type. Default: `"front"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Whether a picture was found. |
| `type` | `string` | The requested type. |
| `playlist` | `integer` | The playlist actually read. |
| `playlistGuid` | `string` | The playlist's GUID, written as `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`. Pass it as `playlistGuid` to address this playlist even after the playlist list has changed. |
| `index` | `integer` | The row actually read. |
| `mimeType` | `string` | MIME type detected from the bytes. |
| `size` | `integer` | Picture size in bytes. |
| `dataUrl` | `string` | `data:<mime>;base64,...` URL of the picture. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getByPlaylistItem', {
	index: 3,
});
if (res.success === false) throw new Error(res.error);
const { available, dataUrl } = res;
```

### artwork.getCurrent

<!-- api-schema:begin artwork.getCurrent -->
Read the picture of the playing track. Three lookups are tried in order and `source` names the one that answered; `available: false` with `reason: "no_track"` when nothing is playing and `reason: "not_found"` when none of them had a picture.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type. Default: `"front"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Whether a picture was found. |
| `type` | `string` | The requested type. |
| `source` | `"now_playing_manager" \| "album_art_manager_v2" \| "extractor"` | Lookup that answered: the now-playing cache, the album art manager, or the extractor. |
| `mimeType` | `string` | MIME type detected from the bytes. |
| `size` | `integer` | Picture size in bytes. |
| `dataUrl` | `string` | `data:<mime>;base64,...` URL of the picture. |
| `reason` | `"no_track" \| "not_found"` | Why no picture was found. |
| `path` | `string` | Path of the playing track; reported when no picture was found for it. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

| `source` value | Meaning |
| --- | --- |
| `now_playing_manager` | Cached current front-cover artwork. |
| `album_art_manager_v2` | Artwork resolved by the album-art manager fallback. |
| `extractor` | Artwork resolved directly by the file extractor fallback. |

```js
const res = await fb2k.invoke('artwork.getCurrent');
if (res.success === false) throw new Error(res.error);
const { available, dataUrl, source } = res;
```

### artwork.getFb2kUrl

<!-- api-schema:begin artwork.getFb2kUrl -->
Build the `fb2k://artwork/` URL of the playing track's picture; the resource handler reads and scales the picture when the page loads the URL. `available: false` with `reason: "no_track"` when nothing is playing.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type. Default: `"front"`. |
| `maxSize` | `integer` | No | Longest side in pixels the handler scales the picture down to; omitted or `0` keeps the original size. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Whether a track is playing. |
| `type` | `string` | The type the URL carries: `front` or `back` for their `cover_` spellings. |
| `dataUrl` | `string` | The `fb2k://artwork/?path=...` URL; not a data URL, only the component's WebView2 resource handler resolves it. |
| `reason` | `"no_track" \| "not_found"` | Why there is no URL; only `no_track` occurs here. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getFb2kUrl', {
	maxSize: 300,
});
if (res.success === false) throw new Error(res.error);
const { available, dataUrl } = res;
```

### artwork.getFb2kUrlByPath

<!-- api-schema:begin artwork.getFb2kUrlByPath -->
Build the `fb2k://artwork/` URL for a path. Only a string is built: the file is not opened, so a path with no file behind it is still reported available.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. Must not be empty. |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type. Default: `"front"`. |
| `maxSize` | `integer` | No | Longest side in pixels the handler scales the picture down to; omitted or `0` keeps the original size. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Always `true`: the URL is built without opening the file. |
| `type` | `string` | The type the URL carries: `front` or `back` for their `cover_` spellings. |
| `path` | `string` | The path as given. |
| `dataUrl` | `string` | The `fb2k://artwork/?path=...` URL; not a data URL, only the component's WebView2 resource handler resolves it. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getFb2kUrlByPath', {
	path: 'C:\\Music\\song.flac',
	maxSize: 300,
});
if (res.success === false) throw new Error(res.error);
const { available, dataUrl } = res;
```

### artwork.getFb2kUrlByPathBatch

<!-- api-schema:begin artwork.getFb2kUrlByPathBatch -->
Build `fb2k://artwork/` URLs for up to 100 entries given as exactly one of `paths` or `items`; giving both or neither fails with `INVALID_PARAMS`. Each row reports its own outcome, so a bad entry does not fail the call.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | No | Track paths; exactly one of `paths` and `items`. |
| `items` | `ArtworkBatchItem[]` | No | Entries with their own type or scale limit; exactly one of `paths` and `items`. |
| `items[].path` | `string` | Yes | Track path. |
| `items[].type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type for this entry; the batch-wide `type` when omitted. |
| `items[].maxSize` | `integer` | No | Scale limit for this entry; the batch-wide `maxSize` when omitted. |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type for entries that do not name their own. Default: `"front"`. |
| `maxSize` | `integer` | No | Longest side in pixels for entries that do not name their own; omitted or `0` keeps the original size. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `artworks` | `ArtworkUrlRow[]` | One row per entry, in the given order. |
| `artworks[].path` | `string` | The entry's path. |
| `artworks[].available` | `boolean` | Whether a URL was built. |
| `artworks[].type` | `string` | The type the URL carries. |
| `artworks[].dataUrl` | `string` | The `fb2k://artwork/?path=...` URL. |
| `artworks[].error` | `string` | Why no URL was built, as the URL builder's error name such as `invalid_type`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('artwork.getFb2kUrlByPathBatch', {
    paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'],
    type: 'front',
});
```

### artwork.getFolderImages

<!-- api-schema:begin artwork.getFolderImages -->
List the image files (`.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, `.webp`) directly inside a directory. A directory that is missing, is a file or cannot be listed is not an error: it lists no images.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `directory` | `string` | Yes | Directory to list. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `images` | `ArtworkFolderImage[]` | The image files, in directory order. |
| `images[].name` | `string` | File name. |
| `images[].path` | `string` | Full path. |
| `images[].size` | `integer` | File size in bytes. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getFolderImages', {
	directory: 'C:\\Music\\Album',
});
if (res.success === false) throw new Error(res.error);
const { images } = res;
```

### artwork.getForTrack

<!-- api-schema:begin artwork.getForTrack -->
Read a picture through the album art manager, which also finds folder covers, and measure it. `width` and `height` are read from the PNG header and are `0` for other formats. A `file-relative://` path is refused with `INVALID_PATH`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path: native, `file://`, or with a `\|subsong:N` suffix. Must not be empty. |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type. Default: `"front"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Whether a picture was found. |
| `type` | `string` | The requested type. |
| `path` | `string` | The path as given. |
| `mimeType` | `string` | MIME type detected from the bytes. |
| `width` | `integer` | Width in pixels from the PNG header; `0` for other formats. |
| `height` | `integer` | Height in pixels from the PNG header; `0` for other formats. |
| `size` | `integer` | Picture size in bytes. |
| `dataUrl` | `string` | `data:<mime>;base64,...` URL of the picture. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getForTrack', {
	path: 'C:\\Music\\song.flac',
	type: 'back',
});
if (res.success === false) throw new Error(res.error);
const { available, dataUrl } = res;
```

### artwork.getLyrics

<!-- api-schema:begin artwork.getLyrics -->
Read the lyrics tag of a track. The host's cached track info is used when complete; a local file it has not read is read from disk, and a remote or unrecognised path keeps whatever the cache has. The tags `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS` and `SYNCED LYRICS` are probed in that order and the first non-empty one wins. Without `path` the playing track is used; with nothing playing the call fails with `NO_ACTIVE_ITEM`. A path no track can be made for fails with `NOT_FOUND`, a local file that cannot be read with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Track path; omitted for the playing track. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Whether a non-empty lyrics tag was found. |
| `tag` | `string` | The tag that supplied the lyrics. |
| `lyrics` | `string` | The lyrics text. |
| `synced` | `boolean` | `true` when the tag name marks the lyrics as synced. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('artwork.getLyrics');
if (res.success === false) throw new Error(res.error);
const { available, lyrics } = res;
```

### artwork.getMetadata

<!-- api-schema:begin artwork.getMetadata -->
Read the album-level tags of a track, plus whether the file embeds any picture and whether it carries a lyrics tag. The tags come from the host's cached track info when complete; a local file it has not read is read from disk, and a remote or unrecognised path keeps whatever the cache has, which can be blank. Without `path` the playing track is used; with nothing playing the call fails with `NO_ACTIVE_ITEM`. A path no track can be made for fails with `NOT_FOUND`, a local file that cannot be read with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Track path; omitted for the playing track. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | Always `true`: the track was resolved, whether or not any of its tags could be read. |
| `album` | `string` | `ALBUM` tag; empty when absent. |
| `artist` | `string` | `ARTIST` values joined with `, ` in tag order. |
| `albumArtist` | `string` | `ALBUM ARTIST` values joined with `, ` in tag order. |
| `title` | `string` | `TITLE` tag; empty when absent. |
| `year` | `string` | `DATE` tag as written; empty when absent. |
| `genre` | `string` | `GENRE` values joined with `, ` in tag order. |
| `trackNumber` | `string` | `TRACKNUMBER` tag as written, such as `3` or `3/12`; empty when absent. |
| `discNumber` | `string` | `DISCNUMBER` tag as written; empty when absent. |
| `hasEmbedded` | `boolean` | Whether the file embeds any of the five picture types. |
| `hasLyrics` | `boolean` | Whether any of the five tags recognized by `artwork.getLyrics` exists, even an empty one. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

> Multi-value tags in `artist` / `albumArtist` / `genre` / `composer` (only the fields this API actually returns) are joined with `, ` in their original order, without de-duplication.

```js
const res = await fb2k.invoke('artwork.getMetadata', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { title, album, hasEmbedded } = res;
```

## Usage notes

- Valid artwork `type` values are `front` (also `cover_front`), `back` (also `cover_back`), `disc`, `icon`, and `artist`. Omitted `type` means `front`; an unknown value returns `INVALID_PARAMS`.
- `artwork.getByPath` and `artwork.getForTrack` accept native paths, `file://` paths, and `path|subsong:N`. They reject `file-relative://` because an extractor has no playlist context; use `artwork.getByPlaylistItem` for those items.
- Direct artwork reads return a standard `data:image/...;base64,...` URL. `artwork.getFb2kUrl` and its path variants instead return a `fb2k://artwork/` URL in the `dataUrl` field. Despite the field name, that value is not a Data URL or image bytes: it is resolved only by this component's WebView2 resource handler and is intended for immediate `<img src>` rendering. Do not persist it, pass it to `file.write`, or treat it as a system-wide URL. `maxSize` is applied only when it is greater than `0`.
- To save a direct-read Data URL with `file.write`, split it at the first comma, keep the Base64 payload after the comma, and write `content: 'base64:' + payload` with `encoding: 'binary'`. To pass the same artwork to `metadata.embedArtwork`, pass only the raw Base64 payload without the Data URL header and without the `base64:` marker.
- `artwork.getFb2kUrlByPathBatch` requires exactly one array input named `paths` or `items`. Array entries may be strings or objects with a `path` member. It has no top-level `path` parameter. The result is `{ success, artworks }`, with one `available`/`error` result for each supplied entry.
- `artwork.getAvailableArtwork` reports embedded items and external source labels such as `folder:cover.jpg`. The implementation opens files through `album_art_extractor`; absence of artwork is represented by `available: false`, not necessarily an error.
- `artwork.getFolderImages` reads a directory and returns matching `.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, and `.webp` files. Its `directory` argument is subject to the runtime `Read` security level.
