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

### metadata.embedArtwork

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1675`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `filename` | `string` | No | Optional; default . |
| `imageData` | `string` | No | Optional; default . |
| `path` | `string` | Yes | Required. |
| `target` | `array` | No | Optional; default embedded. |
| `type` | `string` | No | Optional; default front. |

**Returns**: `{"error":"...","path":"...","results":"...","success":true,"type":"..."}`

```js
const result = await fb2k.invoke('metadata.embedArtwork', { filename: /* value */, imageData: /* value */, path: /* value */, target: /* value */, type: /* value */ });
```

### metadata.read

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1657`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Required. |
| `cueIndex` | `integer` | No | Optional; default -1. Subsong index override, wins over a `|subsong:N` suffix in `path`. |

**Returns**: `{"error":"...","info":"...","path":"...","success":true,"tags":"..."}`

```js
const result = await fb2k.invoke('metadata.read', { path: /* value */ });
```

See [Addressing a track inside a container](#subsong-addressing) for CUE sheets, ISO images, and other multi-track files.

### metadata.readBatch

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1666`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | Required. |

**Returns**: `{"error":"...","errorCount":"...","results":"...","success":true,"successCount":"...","total":"..."}`

```js
const result = await fb2k.invoke('metadata.readBatch', { paths: /* value */ });
```

Each entry is resolved independently, so a batch may mix plain file paths and `container|subsong:N` references. There is no batch-wide `cueIndex`; put the index in each path.

### metadata.readByPath

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1660`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Required. |
| `cueIndex` | `integer` | No | Optional; default -1. Subsong index override, wins over a `|subsong:N` suffix in `path`. |

**Returns**: `{"TRACKNUMBER":"...","canonicalPath":"...","error":"...","path":"...","success":true}`

```js
const result = await fb2k.invoke('metadata.readByPath', { path: /* value */ });
```

### metadata.readRaw

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1663`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | No | Optional; default -1. |
| `path` | `string` | Yes | Required. |

**Returns**: `{"error":"...","info":"...","path":"...","source":"...","success":true,"tags":"..."}`

```js
const result = await fb2k.invoke('metadata.readRaw', { cueIndex: /* value */, path: /* value */ });
```

### metadata.removeEmbeddedArt

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1678`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Required. |
| `removeAll` | `boolean` | No | Optional; default false. |
| `type` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","path":"...","removedTypes":"...","success":true}`

```js
const result = await fb2k.invoke('metadata.removeEmbeddedArt', { path: /* value */, removeAll: /* value */, type: /* value */ });
```

### metadata.removeField

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1684`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | No | Optional; default -1. |
| `path` | `string` | Yes | Required. |
| `tags` | `array` | Yes | Required. |

**Returns**: `{"dispatched":"...","error":"...","note":"...","path":"...","removedCount":"...","removedTags":"...","subsong":"...","success":true}`

```js
const result = await fb2k.invoke('metadata.removeField', { cueIndex: /* value */, path: /* value */, tags: /* value */ });
```

### metadata.removeTag

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1681`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | No | Optional; default -1. |
| `path` | `string` | Yes | Required. |
| `tags` | `array` | Yes | Required. |

**Returns**: `{"dispatched":"...","error":"...","note":"...","path":"...","removedCount":"...","removedTags":"...","subsong":"...","success":true}`

```js
const result = await fb2k.invoke('metadata.removeTag', { cueIndex: /* value */, path: /* value */, tags: /* value */ });
```

### metadata.write

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1669`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | No | Optional; default -1. |
| `path` | `string` | Yes | Required. |
| `tags` | `object` | Yes | Required. |

**Returns**: `{"canonicalPath":"...","dispatched":"...","error":"...","handlePath":"...","note":"...","path":"...","subsong":"...","success":true,"tagsApplied":"...","tagsRemoved":"...","tagsSet":"..."}`

```js
const result = await fb2k.invoke('metadata.write', { cueIndex: /* value */, path: /* value */, tags: /* value */ });
```

### metadata.writeBatch

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1672`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `array` | Yes | Required. |

**Returns**: `{"error":"...","errors":"...","failCount":"...","success":true,"successCount":"..."}`

```js
const result = await fb2k.invoke('metadata.writeBatch', { items: /* value */ });
```

## rating

### rating.get

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1690`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | No | Optional; default -1. |
| `path` | `string` | Yes | Required. |

**Returns**: `{"error":"...","path":"...","rating":"...","storage":"...","success":true}`

```js
const result = await fb2k.invoke('rating.get', { cueIndex: /* value */, path: /* value */ });
```

### rating.set

Public API method. Runtime authority: `src/api/MetadataApi.cpp:1687`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | No | Optional; default -1. |
| `path` | `string` | Yes | Required. |
| `rating` | `integer` | No | Optional; default -1. |

**Returns**: `{"(current)":"...","error":"...","menuPath":"...","note":"...","path":"...","rating":"...","storage":"...","success":true}`

```js
const result = await fb2k.invoke('rating.set', { cueIndex: /* value */, path: /* value */, rating: /* value */ });
```

## Contract notes

- `metadata.read`, `metadata.readByPath`, and `metadata.readRaw` require `path`. `readRaw` bypasses the metadb cache and accepts `cueIndex` with default `-1`; a `path|subsong:N` value selects a container subsong. Its successful result adds `source: "file"` to the structured `{ success, path, tags, info }` shape.
- `metadata.write`, `metadata.removeTag`, and the compatibility endpoint `metadata.removeField` dispatch an asynchronous update. A successful dispatch is not final persistence confirmation: listen for the broadcast `metadata:writeComplete` payload `{ operation, path, subsong, code, success, status }`.
- `metadata.embedArtwork` requires non-empty `path` and **raw Base64 image bytes** in `imageData`. Do not pass a `data:image/...;base64,` header, a `base64:` marker, or an `fb2k://` URL; strip a standard Data URL at its first comma before invoking this endpoint. `type` defaults to `front`; `cover_front` is accepted as its equivalent, and `cover_back` is accepted as the equivalent of `back`. `filename` defaults to an empty string. `target` defaults to `embedded` and accepts `embedded`, `file`, `all`, or an array of `embedded` and `file`. For multiple targets, `{ success, path, type, results }` succeeds when any target succeeds.
- `metadata.removeEmbeddedArt` accepts `removeAll` and an optional `type`; an empty `type` also requests removal of all artwork. It requires a format that supports the `album_art_editor` workflow.
- `rating.set` accepts values from `0` through `5`; `0` removes the rating. It uses foo_playcount when a matching context-menu command is available and otherwise writes the `RATING` file tag. `rating.get` reports its source through `storage` as either `stats` or `file`.
