# Metadata API

Methods of the `metadata` namespace.

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

<!-- api-schema:begin metadata.cancelProbe -->
Stop a probe started by `metadata.probeBatchAsync`. The read in progress is interrupted, the paths not yet reached are never reported, and the run still ends with `metadata:probeComplete` carrying `cancelled: true`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `operationId` | `string` | Yes | Id from the `metadata.probeBatchAsync` receipt. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `cancelled` | `boolean` | `true` when the probe was still running and has been told to stop; `false` when it had already finished or never existed, which are deliberately indistinguishable. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`cancelled: false` means the operation already finished or never existed; the two are intentionally indistinguishable. A cancelled batch still ends with a final `metadata:probeComplete` event carrying `cancelled: true`.

```js
const res = await fb2k.invoke('metadata.cancelProbe', { operationId });
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

### metadata.embedArtwork

<!-- api-schema:begin metadata.embedArtwork -->
Write a picture into a track's tags, into an image file next to it, or both. With one target the result describes that write; with both, each target's outcome is reported under `results` and the call fails with `OPERATION_FAILED` only when both failed.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. The `embedded` target writes into this file, which needs a format the album art editor supports; the `file` target writes into its directory with a `\|subsong:N` suffix dropped, so every track of a CUE sheet shares one image. Must not be empty. |
| `imageData` | `string` | Yes | The image as plain Base64, without a Data URL header or a `base64:` marker. Characters outside the Base64 alphabet are skipped and decoding stops at the first `=`; input that decodes to no bytes fails with `INVALID_PARAMS`. Must not be empty. |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type. Default: `"front"`. |
| `target` | `string[]` | No | Where to write: `embedded` (into the file's tags), `file` (an image next to the audio file, named after `type`, `cover` for the front picture, with the extension read from the image bytes), or `all` for both; several entries write to each. Omitted, it writes `embedded`. Any other value fails with `INVALID_PARAMS`. Must not be empty. |
| `filename` | `string` | No | File name for the `file` target, used as it is instead of the derived one (no extension is added); must be a plain name without separators or traversal. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given. |
| `type` | `string` | The requested type, as given. |
| `size` | `integer` | Picture size in bytes after decoding; present for a single target. |
| `savedTo` | `string` | Full path of the image file written; present for a single `file` target. |
| `results` | `Record<string, any>` | With both targets: each target's own envelope (`{ success, path, type, size }`, plus `savedTo` for `file`, or `{ success: false, error, code }`) under `embedded` and `file`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('metadata.embedArtwork', {
	path: 'C:\\Music\\song.flac',
	imageData: base64Jpeg,
});
```

### metadata.probeBatchAsync

Cancellable batch metadata probe; disk reads run on a worker thread.

<!-- api-schema:begin metadata.probeBatchAsync -->
Start a cancellable batch probe on a worker thread and return a receipt at once. The results arrive in batches on `metadata:probeProgress`, every 64 results or 100 ms, followed by one `metadata:probeComplete`. Each result says whether its info came from the cache or the disk, and a failure is classified as `not-found`, `unsupported-format` or `read-error`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Paths to probe, reported one result each, in this order. A `\|subsong:N` suffix selects a subsong; the `#N` spelling is not recognized, since it would split a file name that ends in `#<digits>`. Every path is checked before anything starts; one refused path fails the whole call with `PERMISSION_DENIED` and no `operationId`. Must not be empty. |
| `includeTags` | `boolean` | No | Attach the flat fields, as in `metadata.readByPath` without `path` and without the `TRACKNUMBER` taken from the file name, to each successful result; `false` reports technical info only. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `operationId` | `string` | Id of the operation, starting with `probe_`. The events of the operation carry it, and `metadata.cancelProbe` takes it. |
| `totalCount` | `integer` | Number of paths accepted; the events report it as `total`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The return value is only a dispatch receipt; results arrive via `metadata:probeProgress` (batched) and a final `metadata:probeComplete`. Each result carries `infoSource` (`cached` / `direct` / `none`); failed items carry `failure` (`not-found` / `unsupported-format` / `read-error`).

```js
const receipt = await fb2k.invoke('metadata.probeBatchAsync', { paths });
```

### metadata.read

<!-- api-schema:begin metadata.read -->
Read the tags and technical info of one track. The host reads its cached info and falls back to reading the file when that info cannot be had or has no title tag; a track that cannot be opened or read fails with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix or a trailing `#N` selects a subsong. Must not be empty. |
| `cueIndex` | `integer` | No | Subsong index that wins over a suffix in `path`; negative values are ignored. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given, suffix included. |
| `tags` | `Record<string, any>` | Every tag of the track, keyed by its name as the file spells it (case kept): a tag with one value is a string, one with several values a string array. |
| `info` | `TrackTechnicalInfo` | Technical info of the track. |
| `info.duration` | `number` | Length in seconds. |
| `info.bitrate` | `integer` | Bitrate in kbit/s as the decoder reports it; `0` when unknown. |
| `info.sampleRate` | `integer` | Sample rate in Hz; `0` when unknown. |
| `info.channels` | `integer` | Channel count; `0` when unknown. |
| `info.codec` | `string` | Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('metadata.read', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { tags, info } = res;
```

See [Addressing a track inside a container](#subsong-addressing) for CUE sheets, ISO images, and other multi-track files.

### metadata.readBatch

<!-- api-schema:begin metadata.readBatch -->
Read the flat fields of several tracks, one row per path. A path that cannot be read gets a row with its own error and does not fail the call. The reads run on the main thread; prefer `metadata.probeBatchAsync` for many files.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths, each resolved on its own; a `\|subsong:N` suffix or a trailing `#N` selects a subsong, and there is no batch-wide `cueIndex`. An empty list is an empty success. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `total` | `integer` | Number of paths received. |
| `successCount` | `integer` | Rows that were read. |
| `errorCount` | `integer` | Rows that failed. |
| `results` | `MetadataReadBatchItem[]` | One row per path, in request order. |
| `results[].path` | `string` | The path of this row, as given. |
| `results[].success` | `boolean` | Whether this row was read. |
| `results[].tags` | `Record<string, any>` | The flat fields of the track, as in `metadata.readByPath` but without `path` and without a `TRACKNUMBER` taken from the file name; present when `success` is `true`. |
| `results[].error` | `string` | Why this row failed, such as `Failed to get track info`; present when `success` is `false`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('metadata.readBatch', {
	paths: ['C:\\Music\\song.flac', 'D:\\album.cue|subsong:2'],
});
if (res.success === false) throw new Error(res.error);
const { results } = res;
```

Each entry is resolved independently, so a batch may mix plain file paths and `container|subsong:N` references. There is no batch-wide `cueIndex`; put the index in each path.

### metadata.readByPath

<!-- api-schema:begin metadata.readByPath -->
Read one track as a flat object: every tag and technical-info field becomes a top-level key under its upper-case name, beside `path`. The track is read the same way as by `metadata.read`, with the same failures.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix or a trailing `#N` selects a subsong. Must not be empty. |
| `cueIndex` | `integer` | No | Subsong index that wins over a suffix in `path`; negative values are ignored. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given, suffix included. |
| `[each key]` | `any` | One tag or technical-info field of the track under its upper-case name. A tag with one value is a string and one with several values a string array; technical-info fields are strings. `DURATION` is always present: the length in seconds with three decimals, such as `215.400`. `FILESIZE` is the size in bytes and appears only when the host knows it, so a file the library has not indexed yet has none. `TRACKNUMBER`, when the file has no such tag, is taken from a leading number of up to three digits in the file name (`07 - Song.flac`, `(07) Song.flac`, `[07] Song.flac`) and is absent when there is none. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('metadata.readByPath', {
	path: 'C:\\Music\\song.flac',
});
```

### metadata.readRaw

<!-- api-schema:begin metadata.readRaw -->
Read the tags and technical info of one track straight from the file, bypassing the host's cache; the result is that of `metadata.read` plus `source`. A file that cannot be read fails with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix or a trailing `#N` selects a subsong. Must not be empty. |
| `cueIndex` | `integer` | No | Subsong index that wins over a suffix in `path`; negative values are ignored. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given, suffix included. |
| `tags` | `Record<string, any>` | Every tag of the track, as in `metadata.read`. |
| `info` | `TrackTechnicalInfo` | Technical info of the track. |
| `info.duration` | `number` | Length in seconds. |
| `info.bitrate` | `integer` | Bitrate in kbit/s as the decoder reports it; `0` when unknown. |
| `info.sampleRate` | `integer` | Sample rate in Hz; `0` when unknown. |
| `info.channels` | `integer` | Channel count; `0` when unknown. |
| `info.codec` | `string` | Codec name as the decoder reports it, such as `FLAC` or `MP3`; empty when unknown. |
| `source` | `string` | Always `file`: the tags were read from the file itself, not from the host's cache. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('metadata.readRaw', {
	path: 'C:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
const { tags } = res;
```

### metadata.removeEmbeddedArt

<!-- api-schema:begin metadata.removeEmbeddedArt -->
Remove embedded pictures from a file: one type, or every picture when `type` is omitted or `removeAll` is set. A format the album art editor does not support fails with `NOT_SUPPORTED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; the file needs a format the album art editor supports. Must not be empty. |
| `type` | `"front" \| "cover_front" \| "back" \| "cover_back" \| "disc" \| "icon" \| "artist"` | No | Picture type to remove; omitted, every picture is removed. |
| `removeAll` | `boolean` | No | Remove every picture whatever `type` says. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given. |
| `removedTypes` | `string[]` | What was removed: `["all"]` when every picture went in one step, otherwise the types removed one by one (`front`, `back`, `disc`, `icon`, `artist`), or the single requested `type` as given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('metadata.removeEmbeddedArt', {
	path: 'C:\\Music\\song.flac',
	type: 'back',
});
```

### metadata.removeField

<!-- api-schema:begin metadata.removeField -->
Same as `metadata.removeTag`, under the name older callers use; its completion event says `removeTag` as well.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix or a trailing `#N` selects a subsong. Must not be empty. |
| `tags` | `string[]` | Yes | Tag names to remove; they are upper-cased. An empty list removes nothing and succeeds without dispatching. |
| `cueIndex` | `integer` | No | Subsong index that wins over a suffix in `path`; negative values are ignored. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given. |
| `removedTags` | `string[]` | The tag names queued for removal, upper-cased, in the order given. |
| `removedCount` | `integer` | Number of names in `removedTags`. |
| `dispatched` | `boolean` | `true` when the removal was dispatched; absent for an empty `tags`. |
| `subsong` | `integer` | Subsong the removal targets; present when dispatched. |
| `note` | `string` | A reminder that the outcome arrives as `metadata:writeComplete`; present when dispatched. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('metadata.removeField', {
	path: 'C:\\Music\\song.flac',
	tags: ['COMMENT'],
});
```

### metadata.removeTag

<!-- api-schema:begin metadata.removeTag -->
Queue the removal of tags from one track and return at once. The outcome arrives as `metadata:writeComplete` with `operation: "removeTag"`, broadcast to every window.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix or a trailing `#N` selects a subsong. Must not be empty. |
| `tags` | `string[]` | Yes | Tag names to remove; they are upper-cased. An empty list removes nothing and succeeds without dispatching. |
| `cueIndex` | `integer` | No | Subsong index that wins over a suffix in `path`; negative values are ignored. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given. |
| `removedTags` | `string[]` | The tag names queued for removal, upper-cased, in the order given. |
| `removedCount` | `integer` | Number of names in `removedTags`. |
| `dispatched` | `boolean` | `true` when the removal was dispatched; absent for an empty `tags`. |
| `subsong` | `integer` | Subsong the removal targets; present when dispatched. |
| `note` | `string` | A reminder that the outcome arrives as `metadata:writeComplete`; present when dispatched. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('metadata.removeTag', {
	path: 'C:\\Music\\song.flac',
	tags: ['COMMENT', 'LYRICS'],
});
```

### metadata.write

<!-- api-schema:begin metadata.write -->
Queue a tag write to one track and return at once. foobar2000 writes the file in the background and reports the outcome as `metadata:writeComplete`, broadcast to every window. A request that leaves nothing to write succeeds without dispatching.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix or a trailing `#N` selects a subsong. Must not be empty. |
| `tags` | `Record<string, any>` | Yes | Tags to change, keyed by name. Names are upper-cased, so keys that differ only in case name the same tag. `null`, an empty string or an empty array removes the tag. A non-empty string array replaces all values of the tag, preserving order, duplicates and whitespace without splitting commas or semicolons. Every array element must be a non-empty string without NUL; an invalid element fails this track with `INVALID_PARAMS` before any of its tags are queued, with the tag name and zero-based element index in the error. A scalar string is written as it is; an integer or a fraction is written as its decimal text (`2.5` becomes `2.500000`). Boolean and object values are ignored. File formats and tag writers may limit multivalue support; the completion event reports the final write result. |
| `cueIndex` | `integer` | No | Subsong index that wins over a suffix in `path`; negative values are ignored. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given. |
| `note` | `string` | `No tags to update` when nothing was left to write; otherwise a reminder that the outcome arrives as `metadata:writeComplete`. |
| `dispatched` | `boolean` | `true` when the write was dispatched; absent when there was nothing to write. |
| `handlePath` | `string` | Path of the handle the write targets, as foobar2000 stores it; present when dispatched. |
| `subsong` | `integer` | Subsong the write targets; present when dispatched. |
| `tagsApplied` | `Record<string, any>` | Every tag queued, keyed by its upper-case name: the string, number or non-empty string array given for a write, `null` for a removal (including an empty array). Present when dispatched. |
| `tagsSet` | `integer` | Number of tag fields set, not the number of values; present when dispatched. |
| `tagsRemoved` | `integer` | Number of tag fields removed, including those given as empty arrays; present when dispatched. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('metadata.write', {
	path: 'C:\\Music\\song.flac',
	tags: { TITLE: 'New Title', ARTIST: 'New Artist' },
});
```

### metadata.writeBatch

<!-- api-schema:begin metadata.writeBatch -->
Queue one `metadata.write` per entry and report how many went through. A dispatched write reports its own outcome later as `metadata:writeComplete`, so a file that does not exist still counts here. When any entry fails, the call fails with `OPERATION_FAILED` and the failure carries the same `successCount`, `failCount` and `errors`; the other entries were queued all the same.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `MetadataWriteBatchItem[]` | Yes | Writes to queue, one per entry, in this order. An entry that is not an object, holds a member other than `path`, `tags` and `cueIndex`, or has no string `path` fails the call with `INVALID_PARAMS`. Every path is checked before anything starts; one refused path, an empty string included, fails the whole call with `PERMISSION_DENIED`. An empty list is an empty success. |
| `items[].path` | `string` | Yes | Track path, as in `metadata.write`. Must not be empty. |
| `items[].tags` | `any` | No | Tags to change, as in `metadata.write`. An entry whose `tags` is missing or not an object is reported in `errors` as `Missing tags` instead of refusing the call. An invalid array value also fails only this entry before any of its tags are queued; other entries still run. |
| `items[].cueIndex` | `integer` | No | Subsong index for this entry, as in `metadata.write`. Default: `-1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `successCount` | `integer` | Entries whose write was dispatched or had nothing to write; each dispatched write reports its outcome as `metadata:writeComplete`. |
| `failCount` | `integer` | Entries that failed; always `0` on success, since a failed entry fails the call. |
| `errors` | `MetadataWriteBatchError[]` | The failed entries; always empty on success, since a failed entry fails the call. |
| `errors[].path` | `string` | The entry's path. |
| `errors[].error` | `string` | Why it failed: `Missing tags`, or the error `metadata.write` gives for that path. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('metadata.writeBatch', {
	items: [
		{ path: 'C:\\Music\\song.flac', tags: { GENRE: 'Ambient' } },
		{ path: 'C:\\Music\\other.flac', tags: { GENRE: 'Ambient' } },
	],
});
if (res.success === false) throw new Error(res.error);
const { successCount, failCount } = res;
```

## Usage notes

- `metadata.read`, `metadata.readByPath`, and `metadata.readRaw` all take `path` and an optional `cueIndex` (default `-1`); a `path|subsong:N` value or `cueIndex` selects a container subsong. `readRaw` bypasses the metadb cache, and its successful result adds `source: "file"` to the structured `{ success, path, tags, info }` shape of `metadata.read`.
- `metadata.write`, `metadata.removeTag`, and the compatibility endpoint `metadata.removeField` dispatch an asynchronous update. A successful dispatch is not final persistence confirmation: listen for the broadcast `metadata:writeComplete` payload `{ operation, path, subsong, code, success, status }`.
- `metadata.embedArtwork` requires non-empty `path` and **raw Base64 image bytes** in `imageData`. Do not pass a `data:image/...;base64,` header, a `base64:` marker, or an `fb2k://` URL; strip a standard Data URL at its first comma before invoking this endpoint. `type` defaults to `front`; `cover_front` and `cover_back` are accepted as `front` and `back`, and any other value fails with `INVALID_PARAMS`. `target` is an array: omitted it writes `embedded`, and it takes `embedded`, `file`, and `all` (both) in any combination, while a plain string such as `"file"` fails with `INVALID_PARAMS` (the SDK helpers send a single target as a one-element array). With both targets each outcome is reported under `results`; the call succeeds when either target succeeded, and fails with `OPERATION_FAILED`, still carrying `results`, when both failed.
- `metadata.removeEmbeddedArt` removes one picture `type`, or every picture when `type` is omitted or `removeAll` is `true`. It requires a format that supports the `album_art_editor` workflow.
