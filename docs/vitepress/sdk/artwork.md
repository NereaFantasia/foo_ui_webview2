# `fb.artwork` album art

Choose an API according to the representation you need:

1. **Direct display:** `getFb2kUrl()` or `getFb2kUrlByPath()` returns a response whose `dataUrl` field contains an `fb2k://` URL suitable for `<img src>`.
2. **Embedded image content:** `getCurrent()`, `getByPath()`, or `getForTrack()` returns an `ArtworkResponse` with renderable URL/data fields when `available` is true.

Read `available` before consuming `dataUrl` or `url`.

These representations are not interchangeable. An `fb2k://` URL is resolved
only inside this component's WebView2 and is not persistable image content.
Direct reads return a standard Data URL; saving it requires extracting the
Base64 payload and adapting it to the destination API's wire contract.

## getFb2kUrl(type?, options?)

Resolves an `fb2k://` artwork URL for the current track.

- `type?`: `'front' | 'back' | 'disc' | 'icon' | 'artist'`
- `options?`: `{ maxSize?: number }`, where `maxSize` is the maximum pixel dimension

```javascript
const res = await fb.artwork.getFb2kUrl('front', { maxSize: 300 });
if (res.success === false) throw new Error(res.error);
if (res.available && res.dataUrl) {
    document.getElementById('cover').src = res.dataUrl;
}
```

## getFb2kUrlByPath(path, type?, options?)

Resolves an `fb2k://` artwork URL for a file path.

```javascript
const res = await fb.artwork.getFb2kUrlByPath(
    'E:\\Music\\song.flac',
    'front',
    { maxSize: 200 },
);
```

## `fb2k://artwork` URL notes

- The URL structure is `fb2k://artwork/<encoded-path>/<type>?maxSize=200`.
- The SDK asks the host to construct the URL; consume the returned `dataUrl` rather than assembling protocol URLs manually.
- `maxSize` is optional and requests host-side downsampling.
- The URL is for immediate rendering in the component WebView only. Do not pass it to `fb.file.write()` or `fb.metadata.embedArtwork()`.

## Saving returned artwork

Direct artwork reads return `data:<mime>;base64,<payload>`. `fb.file.write()`
requires `base64:<payload>` together with `{ encoding: 'binary' }`, while
`fb.metadata.embedArtwork()` requires only the raw `<payload>`.

```javascript
const cover = await fb.artwork.getCurrent('front');
if (cover.success === false) throw new Error(cover.error);
if (cover.available && cover.dataUrl) {
    const comma = cover.dataUrl.indexOf(',');
    const payload = cover.dataUrl.slice(comma + 1);
    await fb.file.write('C:\\Config\\cover.jpg', `base64:${payload}`, {
        encoding: 'binary',
    });
}
```

## getForTrack(path, type?, options?)

Returns embedded artwork information for a track path. `options.maxSize` requests downsampling.

```javascript
const res = await fb.artwork.getForTrack(
    'E:\\Music\\song.flac',
    'front',
    { maxSize: 300 },
);
```

## getCurrent(type?)

Returns an `ArtworkResponse` for the current track.

| Parameter | Type | Description |
| --- | --- | --- |
| `type` | `AlbumArtType?` | Artwork type |

```javascript
const res = await fb.artwork.getCurrent('front');
if (res.success === false) throw new Error(res.error);
if (res.available) img.src = res.dataUrl;
```

## getByPath(path, type?)

Returns an `ArtworkResponse` for a file path.

| Parameter | Type | Description |
| --- | --- | --- |
| `path` | `string` | Audio path |
| `type` | `AlbumArtType?` | Artwork type |

```javascript
const cover = await fb.artwork.getByPath('E:\\Music\\song.flac', 'front');
```

## withMaxSize(url, maxSize?)

Pure helper that appends a `maxSize` query parameter to an artwork URL. It returns the input unchanged when the URL is empty or `maxSize` is missing/non-positive.

```javascript
const url = fb.artwork.withMaxSize('fb2k://artwork/...', 300);
// 'fb2k://artwork/...?maxSize=300'
```

## getBatch(paths)

Fetches artwork for multiple paths. The wrapper accepts `string[]`.

```javascript
const results = await fb.artwork.getBatch([
    'E:\\Music\\a.flac',
    'E:\\Music\\b.mp3',
]);
```

## getByPlaylistItem(playlist, index, type?)

Fetches artwork for a playlist item.

```javascript
const artwork = await fb.artwork.getByPlaylistItem(0, 12, 'front');
```

## getFb2kUrlByPathBatch(items, opts?)

Batch variant of `getFb2kUrlByPath()`. `items` may be `string[]` or `ArtworkBatchItem[]`; `opts` may provide batch-wide `type` and `maxSize` values. It returns the full `ArtworkBatchResponse` envelope.

```javascript
const batch = await fb.artwork.getFb2kUrlByPathBatch(
    ['E:\\Music\\a.flac', 'E:\\Music\\b.flac'],
    { type: 'front', maxSize: 256 },
);
```

## getAvailableArtwork(path)

Signature: `fb.artwork.getAvailableArtwork(path: string): Promise<ArtworkGetAvailableArtworkResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Audio path; a `\|subsong:N` suffix is dropped |

Reports which picture types the file embeds and which cover files sit next to it. `artworks` lists the embedded pictures in probe order and `available` tells whether there is at least one. `sources` holds `embedded` once when any picture is embedded, then `folder:<name>` for each `cover`, `folder`, `front` or `album` file (`.jpg` or `.png`) found beside the file.

```javascript
const available = await fb.artwork.getAvailableArtwork('E:\\Music\\song.flac');
```

## getAvailableTypes(path?)

Signature: `fb.artwork.getAvailableTypes(path?: string): Promise<ArtworkGetAvailableTypesResponse>`

Returns the embedded artwork types of the file at `path`, or of the playing track when `path` is omitted, as the `types` array of the response.

```javascript
const types = await fb.artwork.getAvailableTypes();
```

## getFolderImages(directory)

Signature: `fb.artwork.getFolderImages(directory: string): Promise<ArtworkGetFolderImagesResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `directory` | `string` | Yes | Directory path |

Returns the recognized image files in the directory as `images` rows of `{ name, path, size }`.

```javascript
const images = await fb.artwork.getFolderImages('E:\\Music\\Album');
```

## getLyrics(path?)

Signature: `fb.artwork.getLyrics(path?: string): Promise<ArtworkGetLyricsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Audio path; omitted for the playing track |

Reads the lyrics tag of the track. `LYRICS`, `UNSYNCED LYRICS`, `UNSYNCEDLYRICS`, `SYNCEDLYRICS` and `SYNCED LYRICS` are probed in that order and the first non-empty one wins: `available` tells whether one was found, `tag` names it, `lyrics` holds the text, and `synced` is `true` when the tag name marks the lyrics as synced. The host's cached track info is used when complete; a local file it has not read is read from disk. With `path` omitted and nothing playing the call fails with `NO_ACTIVE_ITEM`; a path no track can be made for fails with `NOT_FOUND`, and a local file that cannot be read with `OPERATION_FAILED`.

```javascript
const lyrics = await fb.artwork.getLyrics('E:\\Music\\song.flac');
```

## getMetadata(path?)

Signature: `fb.artwork.getMetadata(path?: string): Promise<ArtworkGetMetadataResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Audio path; omitted for the playing track |

Reads the album-level tags of the track: `album`, `artist`, `albumArtist`, `title`, `year`, `genre`, `trackNumber` and `discNumber`, each empty when absent, with multi-value tags joined by `, `. `hasEmbedded` tells whether the file embeds any picture and `hasLyrics` whether it carries a lyrics tag, even an empty one. A remote or unrecognised path keeps whatever the host's cache has, which can be blank. Failures are those of `getLyrics()`.

```javascript
const metadata = await fb.artwork.getMetadata('E:\\Music\\song.flac');
```
