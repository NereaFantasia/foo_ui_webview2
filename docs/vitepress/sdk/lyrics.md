# fb.lyrics Lyrics

`fb.lyrics` provides lookup, existence inspection, and persistence operations for track lyrics. When the `path` parameter is omitted in `get()`, the host automatically targets the currently playing track.

## exists(path)

Signature: `fb.lyrics.exists(path: string, options?: Omit<LyricsExistsParams, 'path'>): Promise<LyricsExistsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track file path to inspect; supports appending a `\|subsong:N` suffix to target a specific subsong index |
| `options.filename` | `string` | No | Exact filename for the sidecar lyrics file beside the audio (including extension); empty or omitted applies default automatic lookup rules |

Inspects and returns existing known lyrics metadata tags and companion sidecar files (including zero-byte empty files). File candidate resolution and filesystem access policies are identical to `get()`; supplying an explicit filename constrains only the sidecar candidate list while embedded tag inspection remains active.

Resolves with `{ exists, sources }`, where `sources` is an array of matched source identifiers (such as `embedded` or `file:song.lrc`).

```javascript
const res = await fb.lyrics.exists('E:\\Music\\song.flac');
if (res.success === false) throw new Error(res.error);
const { exists } = res;
```

## get(path?, options?)

Signature: `fb.lyrics.get(path?: string, options?: Omit<LyricsGetParams, 'path'>): Promise<LyricsGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Track file path; omitted to target the currently active track. With nothing playing, the call fails with `NO_ACTIVE_ITEM` |
| `options.source` | `string` | No | Source filter; accepted values are `'embedded'`, `'file'`, or `'any'`. Defaults to `'any'` (inspects embedded tags first, falling back to sidecar files) |
| `options.type` | `string` | No | Lyrics type filter; accepted values are `'synced'` (timestamped), `'unsynced'` (plain text), or `'any'`. Defaults to `'any'` |
| `options.format` | `string` | No | Sidecar file format filter; accepted values are `'lrc'`, `'txt'`, or `'any'`. Defaults to `'any'` (tries `.lrc` before `.txt`). Applies to sidecar file lookups only |
| `options.filename` | `string` | No | Exact sidecar filename beside the audio file; a non-empty string overrides candidate generation rules and bypasses `format`, while `source` and `type` filters remain enforced |

Resolves with a `LyricsGetResponse` structure. Callers must evaluate `success`, then inspect `available` to determine whether lyrics were located; matched results include `source`, `sourcePath`, `tagName`, `lyrics`, and `synced`. Automatic resolution policy: verified single-track audio files check for an identical base filename before trying `<First Artist> - <Title>`; subsong tracks only attempt the artist/title pattern. See [Sidecar lookup and naming](/api/lyrics#sidecar-lookup).

```javascript
const path = 'E:\\Music\\song.flac';
const current = await fb.lyrics.get();
const embedded = await fb.lyrics.get(path, { source: 'embedded' });
const result = await fb.lyrics.get(undefined, { type: 'synced' });
```

## save(path, lyricsText, options?) {#save-lyrics}

Signature: `fb.lyrics.save(path: string, lyricsText: string, options?: LyricsSaveOptions): Promise<LyricsSaveResponse>`

Invokes the underlying `lyrics.save` endpoint to persist lyrics text. The `options` parameter accepts `target`, `filename`, `tagName`, and `format`; if `target` is passed as a single string, the SDK normalizes it into a single-element array before sending to the host.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path the lyrics belong to |
| `lyricsText` | `string` | Yes | Lyrics text to save |
| `options.target` | `LyricsSaveTarget \| LyricsSaveTarget[]` | No | Save destination; accepted values are `'file'` (sidecar file beside the audio, default), `'embedded'` (metadata tag within the audio container), or `'all'` (persists to both) |
| `options.filename` | `string` | No | Exact filename (including extension) for the `file` target. Supplying a non-empty string overrides default naming rules and ignores `format` |
| `options.tagName` | `string` | No | Metadata tag field name for `embedded` target writes; defaults to `'LYRICS'` |
| `options.format` | `string` | No | Sidecar file format extension (`'lrc'` or `'txt'`); defaults to `'lrc'`. Ignored when a non-empty `filename` is provided |

Resolves with a `LyricsSaveResponse` structure. When targeting a single destination, the response describes that specific operation (`savedTo` absolute path for `file`, asynchronous receipt for `embedded`). When targeting multiple destinations, individual outcomes are keyed under `results`, and the overall operation fails with `OPERATION_FAILED` only when all targets fail. Note that the `embedded` target only submits an asynchronous tag write to the host queue: `dispatched: true` in the receipt confirms queue ingestion, with final persistence status delivered via the `metadata:writeComplete` event.

```javascript
const result = await fb.lyrics.save(
	'E:\\Music\\song.flac',
	'[00:00.00]Lyrics...',
	{
		target: ['file', 'embedded'],
		format: 'lrc',
		tagName: 'SYNCEDLYRICS',
	},
);
```

```javascript
const path = 'E:\\Music\\song.flac';
const text = '[00:00.00]Lyrics...';
await fb.lyrics.save(path, text); // sidecar file only
await fb.lyrics.save(path, text, { filename: 'chosen.lrc' });
const chosen = await fb.lyrics.get(path, { source: 'file', filename: 'chosen.lrc' });
const found = await fb.lyrics.exists(path, { filename: 'chosen.lrc' });
const all = await fb.lyrics.save(path, text, { target: 'all' }); // both sidecar file and embedded metadata
if (all.success === false) throw new Error(all.error);
```

All three methods enforce standardized Windows filename validation rules. When track metadata tags are missing, the host cannot automatically derive a subsong's sidecar filename; an explicit `filename` must be provided. Sidecar file writes invalidate read caches prior to returning; embedded writes invalidate caches after underlying metadata modifications complete on disk. For details regarding the removed `config` target and legacy subsong naming compatibility, refer to [Migration Notes](/api/lyrics#migrating-older-callers).
