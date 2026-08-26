# Metadata API

English API reference for the `metadata`, `rating` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## Addressing a track inside a container {#subsong-addressing}

A CUE sheet, ISO image, or multi-track file holds several tracks behind one
file path. Every read method addresses an individual track the same way:

- Append `|subsong:N` to the path, e.g. `D:\album.cue|subsong:2`.
- Or pass `cueIndex: N` alongside the plain path. When both are supplied,
  `cueIndex` wins.

Track numbering follows foobar2000, which is **1-based for CUE sheets**:
`|subsong:1` is the first track. A bare container path (or `|subsong:0`)
addresses subsong 0, which does not exist in a CUE sheet and therefore fails
with `Failed to get track info` — that is the host's numbering, not an error in
the request.

Reading a plain single-track file needs no suffix; `|subsong:0` is equivalent
to omitting it.

## metadata

### metadata.cancelProbe


Cancels an in-flight `metadata.probeBatchAsync` operation.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `operationId` | `string` | Yes | Operation ID from the `metadata.probeBatchAsync` receipt; a missing or empty value fails with `operationId is required`. |

**Returns**: `{"cancelled":true,"success":true}`

`cancelled: false` means the operation already finished or never existed; the two are intentionally indistinguishable. A cancelled batch still ends with a final `metadata:probeComplete` event carrying `cancelled: true`.

```js
const { cancelled } = await fb2k.invoke('metadata.cancelProbe', { operationId });
```

### metadata.embedArtwork


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | The `embedded` target requires a format supported by `album_art_editor`. |
| `imageData` | `string` | Yes | — | Raw Base64 image bytes, without a Data URL header or `base64:` marker. |
| `type` | `string` | No | `front` | Artwork type: `front` / `back` / `disc` / `icon` / `artist`. |
| `target` | `string \| string[]` | No | `embedded` | `embedded` (write into file tags), `file` (write a sidecar image next to the file), `all` (both), or an array of `embedded`/`file`. |
| `filename` | `string` | No | — | Only used by the `file` target to name the sidecar image; empty picks a type-based name (`cover.jpg`, …). Path separators are rejected. |

**Returns**: `{"error":"...","path":"...","results":"...","success":true,"type":"..."}`

```js
await fb2k.invoke('metadata.embedArtwork', {
	path: 'C:\\Music\\song.flac',
	imageData: base64Jpeg,
});
```

### metadata.probeBatchAsync


Cancellable batch metadata probe; disk reads run on a worker thread.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `paths` | `array<string>` | Yes | — | Paths to probe; a missing or empty array fails with `INVALID_PARAMS`, as does a non-array value or a non-string entry. Entries may use `path\|subsong:N`. Per-item `MediaRead` validation is fail-fast: one rejected path fails the whole batch with `PERMISSION_DENIED` and no `operationId` is issued. |
| `includeTags` | `boolean` | No | `true` | Attach a flat tag map to each successful result; pass `false` for technical info only. |

**Returns**: `{"operationId":"probe_...","success":true,"totalCount":42}`

The return value is only a dispatch receipt; results arrive via `metadata:probeProgress` (batched) and a final `metadata:probeComplete`. Each result carries `infoSource` (`cached` / `direct` / `none`); failed items carry `failure` (`not-found` / `unsupported-format` / `read-error`).

```js
const receipt = await fb2k.invoke('metadata.probeBatchAsync', { paths });
```

### metadata.read


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Accepts `path\|subsong:N`. |
| `cueIndex` | `integer` | No | `-1` | Subsong index override, wins over a `\|subsong:N` suffix in `path`. |

**Returns**: `{"error":"...","info":"...","path":"...","success":true,"tags":"..."}`

```js
const { tags, info } = await fb2k.invoke('metadata.read', {
	path: 'C:\\Music\\song.flac',
});
```

See [Addressing a track inside a container](#subsong-addressing) for CUE sheets, ISO images, and other multi-track files.

### metadata.readBatch


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | Each entry may carry a `\|subsong:N` suffix and is resolved independently. |

**Returns**: `{"error":"...","errorCount":"...","results":"...","success":true,"successCount":"...","total":"..."}`

```js
const { results } = await fb2k.invoke('metadata.readBatch', {
	paths: ['C:\\Music\\song.flac', 'D:\\album.cue|subsong:2'],
});
```

Each entry is resolved independently, so a batch may mix plain file paths and `container|subsong:N` references. There is no batch-wide `cueIndex`; put the index in each path.

### metadata.readByPath


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Accepts `path\|subsong:N`. |
| `cueIndex` | `integer` | No | `-1` | Subsong index override, wins over a `\|subsong:N` suffix in `path`. |

**Returns**: `{"TRACKNUMBER":"...","canonicalPath":"...","error":"...","path":"...","success":true}`

```js
const result = await fb2k.invoke('metadata.readByPath', {
	path: 'C:\\Music\\song.flac',
});
```

### metadata.readRaw


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Accepts `path\|subsong:N`. |
| `cueIndex` | `integer` | No | `-1` | Subsong index override, wins over a `\|subsong:N` suffix in `path`. |

**Returns**: `{"error":"...","info":"...","path":"...","source":"...","success":true,"tags":"..."}`

```js
const { tags } = await fb2k.invoke('metadata.readRaw', {
	path: 'C:\\Music\\song.flac',
});
```

### metadata.removeEmbeddedArt


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Requires a format supported by `album_art_editor`. |
| `type` | `string` | No | — | Artwork type to remove; empty removes every type. |
| `removeAll` | `boolean` | No | `false` | When `true`, removes all artwork types and ignores `type`. |

**Returns**: `{"error":"...","path":"...","removedTypes":"...","success":true}`

```js
await fb2k.invoke('metadata.removeEmbeddedArt', {
	path: 'C:\\Music\\song.flac',
	type: 'back',
});
```

### metadata.removeField


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Accepts `path\|subsong:N`. |
| `tags` | `array` | Yes | — | Tag names to remove. |
| `cueIndex` | `integer` | No | `-1` | Subsong index override, wins over a `\|subsong:N` suffix in `path`. |

**Returns**: `{"dispatched":"...","error":"...","note":"...","path":"...","removedCount":"...","removedTags":"...","subsong":"...","success":true}`

```js
await fb2k.invoke('metadata.removeField', {
	path: 'C:\\Music\\song.flac',
	tags: ['COMMENT'],
});
```

### metadata.removeTag


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Accepts `path\|subsong:N`. |
| `tags` | `array` | Yes | — | Tag names to remove. |
| `cueIndex` | `integer` | No | `-1` | Subsong index override, wins over a `\|subsong:N` suffix in `path`. |

**Returns**: `{"dispatched":"...","error":"...","note":"...","path":"...","removedCount":"...","removedTags":"...","subsong":"...","success":true}`

```js
await fb2k.invoke('metadata.removeTag', {
	path: 'C:\\Music\\song.flac',
	tags: ['COMMENT', 'LYRICS'],
});
```

### metadata.write


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Accepts `path\|subsong:N`. |
| `tags` | `object` | Yes | — | A `null` or empty-string value removes that tag. |
| `cueIndex` | `integer` | No | `-1` | Subsong index override, wins over a `\|subsong:N` suffix in `path`. |

**Returns**: `{"canonicalPath":"...","dispatched":"...","error":"...","handlePath":"...","note":"...","path":"...","subsong":"...","success":true,"tagsApplied":"...","tagsRemoved":"...","tagsSet":"..."}`

```js
await fb2k.invoke('metadata.write', {
	path: 'C:\\Music\\song.flac',
	tags: { TITLE: 'New Title', ARTIST: 'New Artist' },
});
```

### metadata.writeBatch


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `array` | Yes | Each item is `{ path, tags }`. |

**Returns**: `{"error":"...","errors":"...","failCount":"...","success":true,"successCount":"..."}`

```js
const { successCount, failCount } = await fb2k.invoke('metadata.writeBatch', {
	items: [
		{ path: 'C:\\Music\\song.flac', tags: { GENRE: 'Ambient' } },
		{ path: 'C:\\Music\\other.flac', tags: { GENRE: 'Ambient' } },
	],
});
```

## rating

### rating.get


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Accepts `path\|subsong:N`. |
| `cueIndex` | `integer` | No | `-1` | Subsong index override, wins over a `\|subsong:N` suffix in `path`. |

**Returns**: `{"error":"...","path":"...","rating":"...","storage":"...","success":true}`

```js
const { rating, storage } = await fb2k.invoke('rating.get', {
	path: 'C:\\Music\\song.flac',
});
```

### rating.set


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | No | — | Omitting it falls back to the now-playing track, then the active playlist selection; an empty string is rejected by path security. |
| `rating` | `integer` | Yes | — | `0`–`5`; `0` clears the rating. |
| `cueIndex` | `integer` | No | `-1` | Subsong index override, wins over a `\|subsong:N` suffix in `path`. |

**Returns**: `{"(current)":"...","error":"...","menuPath":"...","note":"...","path":"...","rating":"...","storage":"...","success":true}`

```js
await fb2k.invoke('rating.set', {
	path: 'C:\\Music\\song.flac',
	rating: 5,
});
```

## Usage notes

- `metadata.read`, `metadata.readByPath`, and `metadata.readRaw` require `path`. `readRaw` bypasses the metadb cache and accepts `cueIndex` with default `-1`; a `path|subsong:N` value selects a container subsong. Its successful result adds `source: "file"` to the structured `{ success, path, tags, info }` shape.
- `metadata.write`, `metadata.removeTag`, and the compatibility endpoint `metadata.removeField` dispatch an asynchronous update. A successful dispatch is not final persistence confirmation: listen for the broadcast `metadata:writeComplete` payload `{ operation, path, subsong, code, success, status }`.
- `metadata.embedArtwork` requires non-empty `path` and **raw Base64 image bytes** in `imageData`. Do not pass a `data:image/...;base64,` header, a `base64:` marker, or an `fb2k://` URL; strip a standard Data URL at its first comma before invoking this endpoint. `type` defaults to `front`; `cover_front` is accepted as its equivalent, and `cover_back` is accepted as the equivalent of `back`. `filename` defaults to an empty string. `target` defaults to `embedded` and accepts `embedded`, `file`, `all`, or an array of `embedded` and `file`. For multiple targets, `{ success, path, type, results }` succeeds when any target succeeds.
- `metadata.removeEmbeddedArt` accepts `removeAll` and an optional `type`; an empty `type` also requests removal of all artwork. It requires a format that supports the `album_art_editor` workflow.
- `rating.set` accepts values from `0` through `5`; `0` removes the rating. It uses foo_playcount when a matching context-menu command is available and otherwise writes the `RATING` file tag. `rating.get` reports its source through `storage` as either `stats` or `file`.
