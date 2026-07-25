# fb.metadata Metadata and Artwork Writes

`fb.metadata` reads and writes track tags, performs raw file reads, and manages embedded or sidecar artwork. Tag-write methods dispatch asynchronously and report final completion through `metadata:writeComplete`.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK Method Stubs

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### read()

Signature: `fb.metadata.read(path: string): Promise<MetadataReadResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |

Returns `{ success, path?, tags?, info? }`. `tags` preserves upstream key casing and each value is a `string` or `string[]`.

```javascript
const result = await fb.metadata.read('E:\\Music\\song.flac');
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

Signature: `fb.metadata.readByPath(path: string): Promise<JsonObject>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |

Returns the flat `metadata.readByPath` object. Tag keys become top-level fields alongside host status and path fields. This method does not invoke `metadata.readRaw`.

```javascript
const fields = await fb.metadata.readByPath('E:\\Music\\song.flac');
```

### removeField()

Signature: `fb.metadata.removeField(path: string, field: string): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `field` | `string` | Yes | Single tag name to remove. |

Dispatches `metadata.removeField` with `tags: [field]`. The receipt can contain `dispatched`, `subsong`, `removedTags`, `removedCount`, and `note`; final completion is reported by `metadata:writeComplete`.

```javascript
const receipt = await fb.metadata.removeField(
	'E:\\Music\\song.flac',
	'COMMENT',
);
```

### removeTag()

Signature: `fb.metadata.removeTag(path: string, tags: string[]): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `tags` | `string[]` | Yes | Tag names to remove. |

Dispatches an asynchronous removal and returns its receipt. Observe `metadata:writeComplete` for the final outcome.

```javascript
await fb.metadata.removeTag('E:\\Music\\song.flac', ['COMMENT', 'GROUPING']);
```

### write()

Signature: `fb.metadata.write(path: string, tags: JsonObject): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path. |
| `tags` | `JsonObject` | Yes | Tag updates; a `null` or empty value removes the corresponding tag. |

Dispatches the write and returns a receipt that can include `canonicalPath`, `handlePath`, `subsong`, and tag counters. The receipt is not the final write result.

```javascript
await fb.metadata.write('E:\\Music\\song.flac', {
	TITLE: 'New title',
	COMMENT: null,
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
