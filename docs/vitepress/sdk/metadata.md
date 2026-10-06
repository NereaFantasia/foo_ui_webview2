# fb.metadata Metadata and Artwork Writes

`fb.metadata` reads and writes track tags, performs raw file reads, and manages embedded or sidecar artwork. Tag-write methods dispatch asynchronously and report final completion through `metadata:writeComplete`.

## read(path, opts?)

Signature: `fb.metadata.read(path: string, opts?: Omit<MetadataReadParams, 'path'>): Promise<MetadataReadResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image. Equivalent to a `\|subsong:<n>` path suffix; the option wins when both are given. |

Returns `{ success, path, tags, info }`. `tags` preserves upstream key casing and each value is a `string` or `string[]`. `info` carries `duration`, `bitrate`, `sampleRate`, `channels`, and `codec`.

The host reads its cached info first and reads the file itself when that info is missing or has no title tag. A track that cannot be opened or read fails with `OPERATION_FAILED`.

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
| `paths` | `string[]` | Yes | Track paths to read. |

Returns `results`, one envelope per requested path, plus the aggregate counters `total`, `successCount`, and `errorCount`.

Each path is resolved on its own and may carry its own `|subsong:N` suffix; there is no batch-wide `cueIndex`. A successful row holds the flat, upper-cased field map in `tags`, as `readByPath()` returns it but without `path` and without a track number taken from the file name. A path that cannot be read gets a row with `success: false` and its own `error` instead of failing the call. The reads run on the host's main thread; for many files use `probeBatchAsync()`.

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
| `path` | `string` | Yes | Track path. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image, as in `read()`. |

Returns the flat `metadata.readByPath` object. Tag keys become top-level fields alongside host status and path fields. This method does not invoke `metadata.readRaw`.

Every tag and technical-info field is keyed by its upper-case name, such as `TITLE` or `SAMPLERATE`. A tag with several values is a `string[]`; technical-info fields are strings. `DURATION` is always present, in seconds with three decimals (`215.400`). `FILESIZE` appears only when the host knows the size, so a file the library has not indexed has none. When the file has no `TRACKNUMBER` tag, it is taken from a leading number of up to three digits in the file name (`07 - Song.flac`), and is absent when there is none. The track is read as by `read()`, with the same failures; `canonicalPath` appears only on the file-open failure envelope.

```javascript
const fields = await fb.metadata.readByPath('E:\\Music\\song.flac');
if (fields.success === false) throw new Error(fields.error);
console.log(fields.TITLE, fields.DURATION);
```

## removeField(path, field, opts?)

Signature: `fb.metadata.removeField(path: string, field: string, opts?: Omit<MetadataRemoveFieldParams, 'path' | 'tags'>): Promise<MetadataRemoveFieldResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `field` | `string` | Yes | Single tag name to remove. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; targets a single contained track instead of the container. |

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
| `path` | `string` | Yes | Track path. |
| `tags` | `string[]` | Yes | Tag names to remove. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; targets a single contained track instead of the container. |

Dispatches an asynchronous removal and returns its receipt. Tag names are upper-cased, and `removedTags` lists them in the order given; an empty `tags` removes nothing and succeeds without dispatching. Observe `metadata:writeComplete` for the final outcome.

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
| `path` | `string` | Yes | Track path. |
| `tags` | `JsonObject` | Yes | Tag updates; `null`, an empty string or an empty array removes the corresponding tag. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; writes tags to that single contained track instead of the container. |

Tag names are upper-cased, so keys that differ only in case name the same tag. A string is written as it is, an integer or a fraction as its decimal text (`2.5` becomes `2.500000`), and a boolean or object is ignored.

A non-empty string array replaces all values of a tag. Order, duplicates and whitespace are preserved; commas and semicolons inside a value are not separators. An empty array removes the tag. A non-string, empty or NUL-containing array element fails the track with `INVALID_PARAMS` before any of its tags are queued; the error identifies the tag and zero-based element index.

For array writes, `tagsApplied` contains the array; removals, including empty arrays, are reported as `null`. `tagsSet` and `tagsRemoved` count fields, not values. A file format or tag writer may limit multivalue support; check the completion event for the final write result.

Dispatches the write and returns a receipt with `dispatched`, `handlePath`, `subsong`, `tagsApplied`, and the tag counters, or only `note: "No tags to update"` when nothing was left to write. `canonicalPath` appears only on the file-open failure envelope. The receipt is not the final write result.

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
| `items` | `MetadataWriteBatchItem[]` | Yes | Per-track tag updates: `path`, `tags`, and an optional `cueIndex`. |

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
| `path` | `string` | Yes | Track path. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image, as in `read()`. |

`fb.metadata.readRaw(path, options?)` bypasses the metadb cache and reads the file directly. `options` is `Omit<MetadataReadRawParams, 'path'>` and may contain `cueIndex`. The typed result is `MetadataReadRawResponse`: the fields of `read()` plus `source`, which is always `'file'`. A file that cannot be read fails with `OPERATION_FAILED`.

```javascript
const raw = await fb.metadata.readRaw('E:\\Music\\album.flac', {
	cueIndex: 2,
});
```

## Cancellable Batch Probe

`readBatch()` reads every path on the host's main thread, so a few hundred files that are not in the library will freeze the UI until it finishes, and there is no way to stop it. `probeBatchAsync()` covers the same ground without either problem: reads run on a worker thread, the call can be cancelled, and each failure is classified instead of collapsing into one generic message.

`read()`, `readBatch()`, `readRaw()` and `readByPath()` also return real `duration` / `bitrate` / `sampleRate` for files the library has never seen; `probeBatchAsync()` is for batches large enough that blocking the main thread matters.

### probeBatchAsync(paths, options?)

Returns immediately with `{ success, operationId, totalCount }`. Results arrive on `metadata:probeProgress`, followed by exactly one `metadata:probeComplete`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Paths to probe; must not be empty. A `\|subsong:N` suffix is honoured per entry. |
| `options.includeTags` | `boolean` | No | Default `true`. Attaches the flat tag map to each successful result. |

Each result entry is a `MetadataProbeResultItem`:

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | Echoed verbatim, `\|subsong:N` included, so it works as a lookup key. |
| `success` | `boolean` | Whether info was obtained. |
| `infoSource` | `'cached' \| 'direct' \| 'none'` | `cached` is a metadb hit, `direct` is a fresh disk read, `none` accompanies a failure. |
| `failure` | `'not-found' \| 'unsupported-format' \| 'read-error'` | Present only when `success` is `false`. |
| `info` | `TrackTechnicalInfo` | `duration`, `bitrate`, `sampleRate`, `channels`, `codec`. |
| `tags` | `Record<string, string \| string[]>` | Flat map with upper-cased keys, as in `readBatch()`. Omitted when `includeTags` is `false`. |

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

Cancellation interrupts the disk read in progress rather than waiting for it. `metadata:probeComplete` still arrives, carrying `cancelled: true`; paths not yet reached are never reported, and the interrupted path is reported as neither a success nor a failure. `cancelled` is `false` when the operation had already finished or never existed — the two cases are deliberately indistinguishable.

### Batching and event volume

Progress events are batched, never one per path: results accumulate until 64 are pending or 100ms have passed since the previous batch, whichever comes first. A fully cached batch of 10000 paths therefore collapses to roughly `ceil(10000 / 64)` events. A disk-bound batch trades extra events for a progress signal that keeps moving — that is what the 100ms bound buys. The final partial batch always arrives before `metadata:probeComplete`.

### Path validation is all-or-nothing

Every entry in `paths` is checked against the host's media-read policy **before** the handler runs. If any single entry fails, the whole call is rejected with `PERMISSION_DENIED` and no `operationId` is produced — there is no partial run and no per-entry `invalid-path` result. Filter paths on the page side if a mixed batch has to be tolerated.

The same pre-handler stage also rejects a `paths` that is not an array, or an entry that is not a string, but those come back as `INVALID_PARAMS` rather than `PERMISSION_DENIED`. Branch on `code` if the two need different handling.

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

- Omitting `target` writes the embedded picture only.
- `'embedded'` writes through the host's tag container and may fail for formats such as CUE.
- `'file'` writes a sidecar such as `cover.<ext>`; the extension is inferred from the image bytes. A `|subsong:N` suffix is dropped, so every track of a CUE sheet shares one image.
- `['embedded', 'file']` or `'all'` runs both targets. The result then carries each target's own outcome under `results.embedded` and `results.file`, and the call fails with `OPERATION_FAILED` only when both failed.
- `filename` applies only to file output; path separators and `..` are rejected.

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

`fb.metadata.removeEmbeddedArt()` accepts `type` and `removeAll` through `MetadataRemoveEmbeddedArtParams`. Omitting `type`, or setting `removeAll: true`, removes every picture. The response may include `removedTypes`: `['all']` when every picture went in one step, otherwise the types removed. A format the album art editor does not support fails with `NOT_SUPPORTED`.

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
