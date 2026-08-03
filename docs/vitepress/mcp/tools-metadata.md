# Metadata and Rating Tools

Twelve tools read and write metadata, manage artwork, remove fields, and get or set ratings.

## Reading

### fb2k_metadata_read 

Reads file metadata in a structured result with separate `tags` and `info` objects.

- **Bridge method**: `metadata.read`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio file path; supports `|subsong:N` or `#N` |
| `cueIndex` | integer | No | CUE sub-track index; overrides the subsong marker in `path` |

**Example result:**

```json
{
  "success": true,
  "path": "D:\\Music\\track.flac",
  "tags": {
    "TITLE": "Redo",
    "ARTIST": "Mili",
    "ALBUM": "Millennium Mother",
    "GENRE": ["Alternative", "Electronic"]
  },
  "info": {
    "duration": 263.5,
    "bitrate": 876,
    "sampleRate": 44100,
    "channels": 2,
    "codec": "FLAC"
  }
}
```

::: tip Multi-value tags
A tag with multiple values is returned as a string array rather than a single string.
:::

### fb2k_metadata_read_by_path 

Reads metadata by path in a flat result.

- **Bridge method**: `metadata.readByPath`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio file path; supports `|subsong:N` or `#N` |
| `cueIndex` | integer | No | CUE sub-track index; overrides the subsong marker in `path` |

### fb2k_metadata_read_raw 

Reads metadata directly from the file, bypassing the metadb cache.

- **Bridge method**: `metadata.readRaw`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio file path; supports `|subsong:N` or `#N` |
| `cueIndex` | integer | No | CUE sub-track index; overrides the subsong marker in `path` |

::: tip Compared with `metadata.read`
`readRaw` reads from the file instead of the metadb cache. It is useful when a caller needs to inspect the file-level state after a write.
:::

### fb2k_metadata_read_batch 

Reads metadata for multiple files in one call.

- **Bridge method**: `metadata.readBatch`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | string[] | Yes | Audio file paths |

## Writing

### fb2k_metadata_write 

Writes metadata tags. A `null` or empty-string value removes the tag.

- **Bridge method**: `metadata.write`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio file path |
| `tags` | object | Yes | Tag key-value pairs, for example `{ "TITLE": "New title" }` |

::: warning File modification
Metadata writes modify the target file. Ensure the file is writable and preserve a backup when appropriate.
:::

### fb2k_metadata_write_batch 

Writes metadata for multiple files.

- **Bridge method**: `metadata.writeBatch`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | object[] | Yes | Write items containing `path` and `tags` |

## Artwork writing

### fb2k_metadata_embed_artwork 

Writes artwork into an audio file's tag container, to a sibling file, or to both targets.

- **Bridge method**: `metadata.embedArtwork`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio file path; supports `|subsong:N` |
| `imageData` | string | Yes | Base64-encoded JPEG, PNG, WebP, GIF, or BMP data |
| `type` | string | No | `front`, `back`, `disc`, `icon`, or `artist`; default `front` |
| `target` | string | No | `embedded`, `file`, or `all`; default `embedded` |
| `filename` | string | No | Plain filename for a file target; path separators and `..` are rejected by the Bridge |

### fb2k_metadata_remove_embedded_art 

Removes embedded artwork from an audio file.

- **Bridge method**: `metadata.removeEmbeddedArt`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio file path |
| `type` | string | No | `front`, `back`, `disc`, `icon`, or `artist` |
| `removeAll` | boolean | No | Remove every artwork type; default `false` |

## Field removal

### fb2k_metadata_remove_tag 

Removes specified metadata tags.

- **Bridge method**: `metadata.removeTag`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio file path |
| `tags` | string[] | Yes | Tag names to remove |

### fb2k_metadata_remove_field 

Removes specified metadata fields. This maps to the Bridge alias `metadata.removeField`.

- **Bridge method**: `metadata.removeField`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Audio file path |
| `tags` | string[] | Yes | Field names to remove |

## Ratings

### fb2k_rating_set 

Sets a track rating. The Bridge first uses the foo_playcount context-menu path and falls back to the `RATING` tag.

- **Bridge method**: `rating.set`

| Parameter | Type | Required | Constraints |
| --- | --- | --- | --- |
| `rating` | integer | Yes | `0` to `5`; `0` means unrated |
| `path` | string | No | Track path; when omitted, the Bridge resolves the current or selected track |
| `cueIndex` | integer | No | CUE sub-track index |

### fb2k_rating_get 

Gets a track rating, reading foo_playcount statistics first and falling back to the `RATING` tag.

- **Bridge method**: `rating.get`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | string | Yes | Track file path |
| `cueIndex` | integer | No | CUE sub-track index |

**Example result:**

```json
{
  "success": true,
  "path": "D:\\Music\\track.flac",
  "rating": 4,
  "storage": "stats"
}
```

| Field | Type | Description |
| --- | --- | --- |
| `rating` | integer | `0` for unrated or `1` to `5` |
| `storage` | string | `stats` for foo_playcount or `file` for the `RATING` tag |
