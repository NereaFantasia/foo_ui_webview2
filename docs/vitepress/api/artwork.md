# Artwork API

English API reference for the `artwork` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## artwork

### artwork.getAvailableArtwork


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. Accepts `path|subsong:N`. |

**Returns**: `{"artworks":"...","available":"...","error":"...","sources":"...","success":true}`

```js
const { artworks, sources } = await fb2k.invoke('artwork.getAvailableArtwork', {
	path: 'C:\\Music\\song.flac',
});
```

### artwork.getAvailableTypes


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; falls back to the now-playing track. |

**Returns**: `{"error":"...","success":true,"types":"..."}`

```js
const { types } = await fb2k.invoke('artwork.getAvailableTypes', {
	path: 'C:\\Music\\song.flac',
});
```

### artwork.getBatch


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | Required. |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"artworks":"...","error":"...","success":true}`

```js
const { artworks } = await fb2k.invoke('artwork.getBatch', {
	paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'],
});
```

### artwork.getByPath


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. Accepts native paths, `file://`, and `path|subsong:N`. |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"available":"...","dataUrl":"...","error":"...","mimeType":"...","path":"...","size":"...","type":"..."}`

```js
const { available, dataUrl } = await fb2k.invoke('artwork.getByPath', {
	path: 'C:\\Music\\song.flac',
});
```

### artwork.getByPlaylistItem


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | Optional; default -1, which selects item 0. |
| `playlist` | `integer` | No | Optional; default -1, which selects the active playlist. |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"available":"...","dataUrl":"...","error":"...","index":"...","mimeType":"...","playlist":"...","size":"...","type":"..."}`

```js
const { available, dataUrl } = await fb2k.invoke('artwork.getByPlaylistItem', {
	index: 3,
});
```

### artwork.getCurrent


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"available":"...","dataUrl":"...","error":"...","mimeType":"...","path":"...","reason":"...","size":"...","source":"...","type":"..."}`

| `source` value | Meaning |
| --- | --- |
| `now_playing_manager` | Cached current front-cover artwork. |
| `album_art_manager_v2` | Artwork resolved by the album-art manager fallback. |
| `extractor` | Artwork resolved directly by the file extractor fallback. |

```js
const { available, dataUrl, source } = await fb2k.invoke('artwork.getCurrent');
```

### artwork.getFb2kUrl


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `maxSize` | `integer` | No | Optional; default 0. |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"available":"...","dataUrl":"...","error":"...","reason":"...","type":"..."}`

```js
const { available, dataUrl } = await fb2k.invoke('artwork.getFb2kUrl', {
	maxSize: 300,
});
```

### artwork.getFb2kUrlByPath


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `maxSize` | `integer` | No | Optional; default 0, which means no downscaling. |
| `path` | `string` | Yes | Track path. |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"available":"...","dataUrl":"...","error":"...","path":"...","type":"..."}`

```js
const { available, dataUrl } = await fb2k.invoke('artwork.getFb2kUrlByPath', {
	path: 'C:\\Music\\song.flac',
	maxSize: 300,
});
```

### artwork.getFb2kUrlByPathBatch


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `array` | No | Optional; omitted by default. |
| `maxSize` | `integer` | No | Optional; default 0. |
| `paths` | `array` | No | Optional; omitted by default. |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"artworks":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('artwork.getFb2kUrlByPathBatch', {
    paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'],
    type: 'front',
});
```

### artwork.getFolderImages


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `directory` | `string` | Yes | Directory to scan for image files. |

**Returns**: `{"error":"...","images":"...","success":true}`

```js
const { images } = await fb2k.invoke('artwork.getFolderImages', {
	directory: 'C:\\Music\\Album',
});
```

### artwork.getForTrack


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. Accepts native paths, `file://`, and `path|subsong:N`. |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"available":"...","dataUrl":"...","error":"...","height":"...","mimeType":"...","path":"...","size":"...","type":"...","width":"..."}`

```js
const { available, dataUrl } = await fb2k.invoke('artwork.getForTrack', {
	path: 'C:\\Music\\song.flac',
	type: 'back',
});
```

### artwork.getLyrics


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; falls back to the now-playing track. |

**Returns**: `{"available":"...","error":"...","lyrics":"...","synced":"...","tag":"..."}`

```js
const { available, lyrics } = await fb2k.invoke('artwork.getLyrics');
```

### artwork.getMetadata


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; falls back to the now-playing track. |

**Returns**: `{"album":"...","albumArtist":"...","artist":"...","available":true,"discNumber":"...","error":"...","genre":"...","hasEmbedded":true,"hasLyrics":true,"title":"...","trackNumber":"...","year":"..."}`

```js
const { title, album, hasEmbedded } = await fb2k.invoke('artwork.getMetadata', {
	path: 'C:\\Music\\song.flac',
});
```

## Usage notes

- Valid artwork `type` values are `front` (also `cover_front`), `back` (also `cover_back`), `disc`, `icon`, and `artist`. Omitted `type` means `front`; an unknown value returns `INVALID_PARAMS`.
- `artwork.getByPath` and `artwork.getForTrack` accept native paths, `file://` paths, and `path|subsong:N`. They reject `file-relative://` because an extractor has no playlist context; use `artwork.getByPlaylistItem` for those items.
- Direct artwork reads return a standard `data:image/...;base64,...` URL. `artwork.getFb2kUrl` and its path variants instead return a `fb2k://artwork/` URL in the `dataUrl` field. Despite the field name, that value is not a Data URL or image bytes: it is resolved only by this component's WebView2 resource handler and is intended for immediate `<img src>` rendering. Do not persist it, pass it to `file.write`, or treat it as a system-wide URL. `maxSize` is applied only when it is greater than `0`.
- To save a direct-read Data URL with `file.write`, split it at the first comma, keep the Base64 payload after the comma, and write `content: 'base64:' + payload` with `encoding: 'binary'`. To pass the same artwork to `metadata.embedArtwork`, pass only the raw Base64 payload without the Data URL header and without the `base64:` marker.
- `artwork.getFb2kUrlByPathBatch` requires exactly one array input named `paths` or `items`. Array entries may be strings or objects with a `path` member. It has no top-level `path` parameter. The result is `{ success, artworks }`, with one `available`/`error` result for each supplied entry.
- `artwork.getAvailableArtwork` reports embedded items and external source labels such as `folder:cover.jpg`. The implementation opens files through `album_art_extractor`; absence of artwork is represented by `available: false`, not necessarily an error.
- `artwork.getFolderImages` reads a directory and returns matching `.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`, and `.webp` files. Its `directory` argument is subject to the runtime `Read` security level.
