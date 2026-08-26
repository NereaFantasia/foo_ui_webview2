# fb.metadata Metadata and Artwork Writes

`fb.metadata` reads and writes track tags, performs raw file reads, and manages embedded or sidecar artwork. Tag-write methods dispatch asynchronously and report final completion through `metadata:writeComplete`.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### read()

Signature: `fb.metadata.read(path: string, opts?: { cueIndex?: number }): Promise<MetadataReadResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image. Equivalent to a `\|subsong:<n>` path suffix; the option wins when both are given. |

Returns `{ success, path?, tags?, info? }`. `tags` preserves upstream key casing and each value is a `string` or `string[]`.

```javascript
const result = await fb.metadata.read('E:\\Music\\song.flac');

// Track 3 of a CUE sheet
const track3 = await fb.metadata.read('E:\\Music\\album.cue', { cueIndex: 3 });
```

### readBatch()

Signature: `fb.metadata.readBatch(paths: string[]): Promise<MetadataReadBatchResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Track paths to read. |

Returns `results`, one envelope per requested path, plus optional aggregate counters.

```javascript
const result = await fb.metadata.readBatch([
	'E:\\Music\\one.flac',
	'E:\\Music\\two.flac',
]);
```

### readByPath()

Signature: `fb.metadata.readByPath(path: string, opts?: { cueIndex?: number }): Promise<MetadataReadByPathResponse & JsonObject>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image, as in `read()`. |

Returns the flat `metadata.readByPath` object. Tag keys become top-level fields alongside host status and path fields. This method does not invoke `metadata.readRaw`.

```javascript
const fields = await fb.metadata.readByPath('E:\\Music\\song.flac');
```

### removeField()

Signature: `fb.metadata.removeField(path: string, field: string, opts?: Omit<MetadataRemoveFieldParams, 'path' | 'tags'>): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `field` | `string` | Yes | Single tag name to remove. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; targets a single contained track instead of the container. |

Dispatches `metadata.removeField` with `tags: [field]`. The receipt can contain `dispatched`, `subsong`, `removedTags`, `removedCount`, and `note`; final completion is reported by `metadata:writeComplete`.

```javascript
const receipt = await fb.metadata.removeField(
	'E:\\Music\\song.flac',
	'COMMENT',
);
```

### removeTag()

Signature: `fb.metadata.removeTag(path: string, tags: string[], opts?: Omit<MetadataRemoveTagParams, 'path' | 'tags'>): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `tags` | `string[]` | Yes | Tag names to remove. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; targets a single contained track instead of the container. |

Dispatches an asynchronous removal and returns its receipt. Observe `metadata:writeComplete` for the final outcome.

`removeField()` and `removeTag()` share one host handler, so both accept the same `cueIndex` option.

```javascript
await fb.metadata.removeTag('E:\\Music\\song.flac', ['COMMENT', 'GROUPING']);

// Clear a tag on track 3 of a CUE sheet only
await fb.metadata.removeTag('E:\\Music\\album.cue', ['COMMENT'], {
	cueIndex: 3,
});
```

### write()

Signature: `fb.metadata.write(path: string, tags: JsonObject, opts?: Omit<MetadataWriteParams, 'path' | 'tags'>): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `tags` | `JsonObject` | Yes | Tag updates; a `null` or empty value removes the corresponding tag. |
| `opts.cueIndex` | `number` | No | 1-based track index inside a CUE sheet or image; writes tags to that single contained track instead of the container. |

Dispatches the write and returns a receipt that can include `canonicalPath`, `handlePath`, `subsong`, and tag counters. The receipt is not the final write result.

```javascript
await fb.metadata.write('E:\\Music\\song.flac', {
	TITLE: 'New title',
	COMMENT: null,
});

// Tag track 3 of a CUE sheet
await fb.metadata.write('E:\\Music\\album.cue', { TITLE: 'Track three' }, {
	cueIndex: 3,
});
```

### writeBatch()

Signature: `fb.metadata.writeBatch(items: Array<{ path: string; tags: JsonObject }>): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `Array<{ path: string; tags: JsonObject }>` | Yes | Per-track tag updates. |

Invokes `metadata.writeBatch` and can return `successCount`, `failCount`, and per-path `errors`. It does not call either artwork endpoint.

```javascript
const result = await fb.metadata.writeBatch([
	{ path: 'E:\\Music\\one.flac', tags: { GENRE: 'Ambient' } },
	{ path: 'E:\\Music\\two.flac', tags: { GENRE: 'Ambient' } },
]);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Raw File Read

`fb.metadata.readRaw(path, options?)` bypasses the metadb cache and reads the file directly. `options` is `Omit<MetadataReadRawParams, 'path'>` and may contain `cueIndex`. The typed result is `MetadataReadRawResponse`, whose `source` is `'file'` when present.

```javascript
const raw = await fb.metadata.readRaw('E:\\Music\\album.flac', {
	cueIndex: 2,
});
```

## Cancellable Batch Probe

`readBatch()` reads every path on the host's main thread, so a few hundred files that are not in the library will freeze the UI until it finishes, and there is no way to stop it. `probeBatchAsync()` covers the same ground without either problem: reads run on a worker thread, the call can be cancelled, and each failure is classified instead of collapsing into one generic message.

It is an addition, not a replacement — `read()`, `readBatch()`, `readRaw()` and `readByPath()` are unchanged, and all four already return real `duration` / `bitrate` / `sampleRate` for files the library has never seen.

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
const { cancelled } = await fb.metadata.cancelProbe(receipt.operationId);
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

`fb.metadata.embedArtwork()` writes an image into the file, to a sibling image file, or to both destinations. `MetadataEmbedArtworkParams` includes `imageData`, `type`, `filename`, and `target`.

`imageData` is the raw Base64 payload only. It must not contain a
`data:image/...;base64,` header, the `file.write`-specific `base64:` marker, or
an `fb2k://` URL.

- `'embedded'` writes through the host's tag container and may fail for formats such as CUE.
- `'file'` writes a sidecar such as `cover.<ext>`; the extension is inferred from the image bytes.
- `['embedded', 'file']` runs both targets through the declared SDK type.
- `filename` applies only to file output; path separators and `..` are rejected.

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

`fb.metadata.removeEmbeddedArt()` accepts `type` and `removeAll` through `MetadataRemoveEmbeddedArtParams`. The response may include `removedTypes`.

```javascript
await fb.metadata.removeEmbeddedArt('E:\\Music\\song.flac', {
	type: 'front',
});
```

## Asynchronous Completion and Default Logging

`metadata.write`, `metadata.removeField`, `metadata.removeTag`, and batch variants dispatch work before the file operation finishes. Subscribe to `metadata:writeComplete` for the final `MetadataWriteCompletePayload`: `operation`, `path`, `subsong`, `code`, `success`, and `status`.

The SDK installs a default listener that logs failed completions to the JavaScript console. Call `fb.metadata.disableDefaultLogger()` to detach it before installing custom UI handling; the operation is idempotent.

```javascript
fb.metadata.disableDefaultLogger();

const off = fb.on('metadata:writeComplete', (event) => {
	if (!event.success) {
		console.error(event.operation, event.path, event.status, event.code);
	}
});
```
