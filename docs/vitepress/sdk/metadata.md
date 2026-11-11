# fb.metadata Metadata and Artwork Writes

`fb.metadata` reads and writes track tags, performs raw file reads, and manages embedded or sidecar artwork. Tag-write methods dispatch asynchronously and report final completion through `metadata:writeComplete`.

## read(path, opts?)

Signature: `fb.metadata.read(path: string, opts?: Omit<MetadataReadParams, 'path'>): Promise<MetadataReadResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image. Equivalent to a `\|subsong:<n>` path suffix; the option wins when both are given |

Returns `{ success, path, tags, info }`. `tags` preserves upstream key casing and each value is a `string` or `string[]`. `info` carries `duration`, `bitrate`, `sampleRate`, `channels`, and `codec`.

The host inspects cached metadata first; if missing or lacking a title tag, it reads the audio file directly from disk. Tracks that cannot be opened or parsed fail with `OPERATION_FAILED`.

```javascript
const result = await fb.metadata.read('E:\\Music\\song.flac');
if (result.success === false) throw new Error(result.error);
console.log(result.tags, result.info.sampleRate);

// Track 3 of a CUE sheet
const track3 = await fb.metadata.read('E:\\Music\\album.cue', { cueIndex: 3 });
```

## readBatch(paths)

Signature: `fb.metadata.readBatch(paths: string[]): Promise<MetadataReadBatchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths to read |

Returns `results`, one envelope per requested path, plus the aggregate counters `total`, `successCount`, and `errorCount`.

Each path is resolved independently and may include a `|subsong:N` suffix; batch-level `cueIndex` is not supported. Successful entries provide a flat metadata map in `tags` with uppercase keys (matching `readByPath()`, but omitting `path` and without inferring track numbers from filenames). Paths that fail to read are returned with `success: false` and individual `error` details without failing the batch request. Reads execute synchronously on the host main thread; use `probeBatchAsync()` for large numbers of files.

```javascript
const result = await fb.metadata.readBatch([
	'E:\\Music\\one.flac',
	'E:\\Music\\two.flac',
]);
```

## readByPath(path, opts?)

Signature: `fb.metadata.readByPath(path: string, opts?: Omit<MetadataReadByPathParams, 'path'>): Promise<MetadataReadByPathResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image, as in `read()` |

Returns the flat `metadata.readByPath` object. Tag keys become top-level fields alongside host status and path fields. This method does not invoke `metadata.readRaw`.

All tag and technical info fields use uppercase keys (such as `TITLE` and `SAMPLERATE`). Multi-value tags are returned as `string[]`; technical info fields are strings. `DURATION` is always present, formatted in seconds with three decimal places (e.g. `215.400`). `FILESIZE` is included only when file size is known to the host; unindexed media files may omit this field. When a file lacks a `TRACKNUMBER` tag, the host attempts to parse a leading numeric prefix of up to three digits from the filename (e.g. `07 - Song.flac`), omitting the field if no number is found. Read logic and failure cases match `read()`; `canonicalPath` appears only in failure envelopes when files cannot be opened.

```javascript
const fields = await fb.metadata.readByPath('E:\\Music\\song.flac');
if (fields.success === false) throw new Error(fields.error);
console.log(fields.TITLE, fields.DURATION);
```

## removeField(path, field, opts?)

Signature: `fb.metadata.removeField(path: string, field: string, opts?: Omit<MetadataRemoveFieldParams, 'path' | 'tags'>): Promise<MetadataRemoveFieldResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path |
| `field` | `string` | Yes | Single tag name to remove |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; targets a single contained track instead of the container |

Dispatches `metadata.removeField` with `tags: [field]`. The receipt can contain `dispatched`, `subsong`, `removedTags`, `removedCount`, and `note`; final completion is reported by `metadata:writeComplete` with `operation: 'removeTag'`.

```javascript
const receipt = await fb.metadata.removeField(
	'E:\\Music\\song.flac',
	'COMMENT',
);
```

## removeTag(path, tags, opts?)

Signature: `fb.metadata.removeTag(path: string, tags: string[], opts?: Omit<MetadataRemoveTagParams, 'path' | 'tags'>): Promise<MetadataRemoveTagResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path |
| `tags` | `string[]` | Yes | Tag names to remove |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; targets a single contained track instead of the container |

Dispatches an asynchronous tag deletion task and returns a receipt. Tag names are normalized to uppercase, and `removedTags` preserves input order; if `tags` is empty, the method succeeds immediately without dispatching file operations. Final execution status is reported via `metadata:writeComplete`.

`removeField()` and `removeTag()` share one host handler, so both accept the same `cueIndex` option.

```javascript
await fb.metadata.removeTag('E:\\Music\\song.flac', ['COMMENT', 'GROUPING']);

// Clear a tag on track 3 of a CUE sheet only
await fb.metadata.removeTag('E:\\Music\\album.cue', ['COMMENT'], {
	cueIndex: 3,
});
```

## write(path, tags, opts?)

Signature: `fb.metadata.write(path: string, tags: JsonObject, opts?: Omit<MetadataWriteParams, 'path' | 'tags'>): Promise<MetadataWriteResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path |
| `tags` | `JsonObject` | Yes | Tag updates; `null`, an empty string or an empty array removes the corresponding tag |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; writes tags to that single contained track instead of the container |

Tag names are normalized to uppercase; keys differing only in case refer to the same tag. String values are written verbatim; numeric values are formatted as decimal strings (e.g. `2.5` becomes `2.500000`); booleans and objects are ignored.

A non-empty string array replaces all values for that tag, preserving element ordering, duplicates, and whitespace without treating commas or semicolons as delimiters. An empty array removes the tag. If an array contains non-string elements, empty strings, or NUL characters, the track fails immediately with `INVALID_PARAMS` without dispatching any tag modifications; error payloads include the tag name and 0-based element index.

For array updates, `tagsApplied` echoes the written array; deletions (including empty arrays) are reported as `null`. `tagsSet` and `tagsRemoved` track affected fields rather than individual value counts. Multivalue tag capabilities depend on the underlying audio container and tag writer; inspect the completion event for final write results.

On successful dispatch, the call returns an operation receipt containing `dispatched`, `handlePath`, `subsong`, `tagsApplied`, and field counters; if no modifications are required, the receipt returns `note: "No tags to update"`. `canonicalPath` appears only in failure envelopes when files cannot be opened. Receipts indicate task acceptance only, not completed disk writes.

```javascript
await fb.metadata.write('E:\\Music\\song.flac', {
	TITLE: 'New title',
	ARTIST: ['First artist', 'Second artist'],
	COMMENT: null,
	GENRE: [],
});

// Tag track 3 of a CUE sheet
await fb.metadata.write('E:\\Music\\album.cue', { TITLE: 'Track three' }, {
	cueIndex: 3,
});
```

## writeBatch(items)

Signature: `fb.metadata.writeBatch(items: MetadataWriteBatchItem[]): Promise<MetadataWriteBatchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `MetadataWriteBatchItem[]` | Yes | Per-track tag updates: `path`, `tags`, and an optional `cueIndex` |

Invokes `metadata.writeBatch` and returns `successCount`, `failCount`, and per-path `errors`. When any entry fails, the call resolves as a failure envelope that still carries all three, and the other entries are dispatched all the same. It does not call either artwork endpoint.

Tag values follow the same rules as `write()`. An invalid array prevents all tag changes for that entry, while valid entries are still queued; the batch failure uses `OPERATION_FAILED` and lists the affected paths in `errors`.

```javascript
const result = await fb.metadata.writeBatch([
	{ path: 'E:\\Music\\one.flac', tags: { GENRE: ['Ambient', 'Electronic'] } },
	{ path: 'E:\\Music\\two.flac', tags: { GENRE: 'Ambient' } },
]);
```

## Raw File Read

### readRaw(path, opts?)

Signature: `fb.metadata.readRaw(path: string, opts?: Omit<MetadataReadRawParams, 'path'>): Promise<MetadataReadRawResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image, as in `read()` |

`fb.metadata.readRaw(path, options?)` bypasses the metadb cache and reads the file directly. `options` is `Omit<MetadataReadRawParams, 'path'>` and may contain `cueIndex`. The typed result is `MetadataReadRawResponse`: the fields of `read()` plus `source`, which is always `'file'`. A file that cannot be read fails with `OPERATION_FAILED`.

```javascript
const raw = await fb.metadata.readRaw('E:\\Music\\album.flac', {
	cueIndex: 2,
});
```

## Cancellable Batch Probe

`readBatch()` parses metadata synchronously on the host main thread, which can cause UI latency when processing unindexed files and cannot be aborted in progress. `probeBatchAsync()` performs disk inspection asynchronously on worker threads, supports mid-flight cancellation, and classifies specific failure reasons.

`read()`, `readBatch()`, `readRaw()`, and `readByPath()` likewise report accurate `duration`, `bitrate`, and `sampleRate` for unindexed files; `probeBatchAsync()` is designed for high-volume scenarios where main thread blocking must be avoided.

### probeBatchAsync(paths, options?)

Returns immediately with `{ success, operationId, totalCount }`. Results arrive on `metadata:probeProgress`, followed by exactly one `metadata:probeComplete`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Paths to probe; must not be empty. A `\|subsong:N` suffix is honoured per entry |
| `options.includeTags` | `boolean` | No | Default `true`. Attaches the flat tag map to each successful result |

Each result entry is a `MetadataProbeResultItem`:

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | Echoed verbatim, `\|subsong:N` included, so it works as a lookup key |
| `success` | `boolean` | Whether info was obtained |
| `infoSource` | `'cached' \| 'direct' \| 'none'` | `cached` is a metadb hit, `direct` is a fresh disk read, `none` accompanies a failure |
| `failure` | `'not-found' \| 'unsupported-format' \| 'read-error'` | Present only when `success` is `false` |
| `info` | `TrackTechnicalInfo` | `duration`, `bitrate`, `sampleRate`, `channels`, `codec` |
| `tags` | `Record<string, string \| string[]>` | Flat map with upper-cased keys, as in `readBatch()`. Omitted when `includeTags` is `false` |

```javascript
const off = fb.on('metadata:probeProgress', (event) => {
	console.log(`${event.done} / ${event.total}`);
	for (const item of event.results) {
		if (item.success) {
			console.log(item.path, item.infoSource, item.info.bitrate);
		} else {
			console.warn(item.path, item.failure);
		}
	}
});

fb.on('metadata:probeComplete', (event) => {
	console.log('done', event.successCount, event.failureCount, event.cancelled);
	off();
});

const receipt = await fb.metadata.probeBatchAsync(droppedPaths, {
	includeTags: false,
});
```

### cancelProbe(operationId)

```javascript
const res = await fb.metadata.cancelProbe(receipt.operationId);
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

Cancelling aborts active disk reads immediately rather than waiting for task completion. The `metadata:probeComplete` event is still emitted with `cancelled: true`; unreached paths are omitted from subsequent results, and the interrupted track is omitted from both success and failure counters. A `cancelled` value of `false` indicates that the operation had already finished or the `operationId` was not found.

### Batching and event volume

Progress events are dispatched in batched intervals: a batch is emitted whenever 64 items accumulate or 100ms elapse since the previous batch, whichever comes first. For example, 10,000 fully cached tracks consolidate into roughly `ceil(10000 / 64)` events. Disk-heavy runs use the 100ms time boundary to provide steady progress updates. The final partial batch always arrives before `metadata:probeComplete`.

### Batch Path Validation Policy {#path-validation-is-all-or-nothing}

All paths in the input list undergo host media-read security checks before task execution. If any single path fails validation, the entire call is rejected with `PERMISSION_DENIED`, generating no `operationId`, executing no partial reads, and reporting no per-entry `invalid-path` result. When processing mixed or unverified sources, filter paths before invoking the API.

If `paths` is not an array or contains non-string elements, validation returns `INVALID_PARAMS` instead of `PERMISSION_DENIED`. Applications should branch on `code` accordingly.

### Known boundaries

- The batch surface does not honour the legacy `#N` subsong spelling that `read()` still accepts. `#N` would mis-split an extensionless filename ending in `#<digits>`, and this endpoint's input is arbitrary user-dropped filenames. Use `|subsong:N`.
- `read()` / `readBatch()` / `readRaw()` / `readByPath()` decide whether to re-read from disk by looking for a `title` tag, so a cached entry that has a title but no `bitrate` is not re-read. `probeBatchAsync()` uses the host's own partial-info flag instead and does not have this gap; the older four are unchanged.

## Artwork

### Byte and Data URL helpers

Prefer `embedArtworkBytes(path, bytes, options?)` when the image is already an
`ArrayBuffer` or `Uint8Array`. Use
`embedArtworkFromDataUrl(path, dataUrl, options?)` for a canonical Base64
`data:image/*` URL. The latter rejects non-image or malformed Data URLs before
invoking the Host.

Both helpers encode or extract a raw Base64 `imageData` payload and call the
existing `metadata.embedArtwork` endpoint; the raw facade remains unchanged.

```javascript
await fb.metadata.embedArtworkBytes(
	'E:\\Music\\song.flac',
	coverBytes,
	{ type: 'front', target: 'embedded' },
);

await fb.metadata.embedArtworkFromDataUrl(
	'E:\\Music\\song.flac',
	coverDataUrl,
	{ type: 'front', target: ['embedded', 'file'] },
);
```

### embedArtwork(path, options?)

`fb.metadata.embedArtwork()` writes an image into the file, to a sibling image file, or to both destinations. `MetadataEmbedArtworkParams` includes `imageData`, `type`, `filename`, and `target`. `target` also accepts a single string, which the SDK sends as a one-element array.

`imageData` is the raw Base64 payload only. It must not contain a
`data:image/...;base64,` header, the `file.write`-specific `base64:` marker, or
an `fb2k://` URL.

- When `target` is omitted, artwork is written exclusively as embedded picture.
- `'embedded'` writes to the audio file embedded tag container; certain formats (such as CUE sheets) may not support embedded artwork.
- `'file'` writes a sidecar image in the same directory (e.g. `cover.<ext>`), with the file extension inferred from image bytes. Any `|subsong:N` suffix is automatically stripped, ensuring all tracks belonging to a CUE sheet share the same image file.
- `['embedded', 'file']` or `'all'` targets both embedded and sidecar destinations. The response reports results under `results.embedded` and `results.file` respectively; the operation fails with `OPERATION_FAILED` only when both destinations fail.
- `filename` applies only to sidecar file output; path separators and `..` are rejected.

`type` defaults to `'front'`. Image data that decodes to no bytes fails with `INVALID_PARAMS`.

```javascript
const comma = coverDataUrl.indexOf(',');
const coverBase64 = coverDataUrl.slice(comma + 1);
const result = await fb.metadata.embedArtwork(
	'E:\\Music\\song.flac',
	{
		imageData: coverBase64,
		type: 'front',
		target: ['embedded', 'file'],
	},
);
```

### removeEmbeddedArt(path, options?)

`fb.metadata.removeEmbeddedArt()` accepts `type` and `removeAll` parameters. Omitting `type` or specifying `removeAll: true` removes all embedded pictures. Successful responses include `removedTypes`: returning `['all']` when removing all pictures, or listing the specific removed picture types. Audio file formats unsupported by the album art editor fail with `NOT_SUPPORTED`.

```javascript
await fb.metadata.removeEmbeddedArt('E:\\Music\\song.flac', {
	type: 'front',
});
```

## Asynchronous Completion and Default Logging

`metadata.write`, `metadata.writeBatch`, `metadata.removeField`, and `metadata.removeTag` dispatch work before the file operation finishes. Subscribe to `metadata:writeComplete` for the final `MetadataWriteCompletePayload`: `operation`, `path`, `subsong`, `code`, `success`, and `status`.

`operation` is `'write'` for `write()` and `writeBatch()`, and `'removeTag'` for `removeTag()` and `removeField()`. `code` is foobar2000's completion code (`0` success, `1` aborted, `2` errors), `status` spells it as `'success'`, `'aborted'`, or `'error'`, and `success` is `true` when `code` is `0`. The event is broadcast to every window.

### disableDefaultLogger()

Signature: `fb.metadata.disableDefaultLogger(): void`

The SDK installs a default listener that logs failed completions to the JavaScript console. Call `fb.metadata.disableDefaultLogger()` to detach it before installing custom UI handling; the operation is idempotent.

```javascript
fb.metadata.disableDefaultLogger();

const off = fb.on('metadata:writeComplete', (event) => {
	if (!event.success) {
		console.error(event.operation, event.path, event.status, event.code);
	}
});
```
