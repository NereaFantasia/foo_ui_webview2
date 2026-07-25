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

## Supplemental method reference

### getAvailableArtwork(path?)

Signature: `fb.artwork.getAvailableArtwork(path?: string): Promise<ArtworkAvailableArtworkResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Audio path; omitted for the host's current-track behavior |

Returns available-artwork status.

```javascript
const available = await fb.artwork.getAvailableArtwork('E:\\Music\\song.flac');
```

### getAvailableTypes()

Signature: `fb.artwork.getAvailableTypes(): Promise<AlbumArtType[]>`

Returns the artwork types available for the current host context.

```javascript
const types = await fb.artwork.getAvailableTypes();
```

### getFolderImages(directory)

Signature: `fb.artwork.getFolderImages(directory: string): Promise<{ images: string[] }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `directory` | `string` | Yes | Directory path |

Returns recognized image paths in the directory.

```javascript
const images = await fb.artwork.getFolderImages('E:\\Music\\Album');
```

### getLyrics(path)

Signature: `fb.artwork.getLyrics(path: string): Promise<ArtworkLyricsResult>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Audio path |

Returns lyrics data for the path.

```javascript
const lyrics = await fb.artwork.getLyrics('E:\\Music\\song.flac');
```

### getMetadata(path)

Signature: `fb.artwork.getMetadata(path: string): Promise<ArtworkMetadataResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Audio path |

Returns artwork-related metadata.

```javascript
const metadata = await fb.artwork.getMetadata('E:\\Music\\song.flac');
```
