# fb.lyrics Lyrics

`fb.lyrics` locates, checks, and saves lyrics for a track. When no path is supplied to `get()`, the host uses the currently playing track.

## exists(path)

Signature: `fb.lyrics.exists(path: string, options?: Omit<LyricsExistsParams, 'path'>): Promise<LyricsExistsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path to inspect; a `\|subsong:N` suffix selects a subsong. |
| `options.filename` | `string` | No | Exact file name beside the audio, including its extension; empty or omitted uses automatic lookup. |

Lists existing known lyrics tags and sidecar files, including empty ones. File candidates and access checks match `get()`; an explicit filename restricts only the file candidates, and embedded tags are still checked.

Returns `{ exists, sources }`; `sources` lists every source found, such as `embedded` or `file:song.lrc`.

```javascript
const res = await fb.lyrics.exists('E:\\Music\\song.flac');
if (res.success === false) throw new Error(res.error);
const { exists } = res;
```

## get(path?, options?)

Signature: `fb.lyrics.get(path?: string, options?: Omit<LyricsGetParams, 'path'>): Promise<LyricsGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Track path; omit it to use the current track. With nothing playing the call then fails with `NO_ACTIVE_ITEM`. |
| `options.source` | `string` | No | `'embedded'`, `'file'`, or `'any'`; defaults to `'any'`, which checks the embedded tags first, then the sidecar files. |
| `options.type` | `string` | No | `'synced'`, `'unsynced'`, or `'any'`; defaults to `'any'`. |
| `options.format` | `string` | No | `'lrc'`, `'txt'`, or `'any'`; defaults to `'any'`, which tries `.lrc` before `.txt`. Applies to the sidecar lookup only. |
| `options.filename` | `string` | No | Exact file name beside the audio; a non-empty value replaces automatic candidates and ignores `format`. `source` and `type` still apply. |

Returns `LyricsGetResponse`. Check `success`, then `available`; matches can include `source`, `sourcePath`, `tagName`, `lyrics`, and `synced`. Automatic lookup uses the audio stem only for verified single-track files, then the first-artist/title name; subsongs use only the latter. See [file lookup and naming](/api/lyrics#sidecar-lookup).

```javascript
const path = 'E:\\Music\\song.flac';
const current = await fb.lyrics.get();
const embedded = await fb.lyrics.get(path, { source: 'embedded' });
const result = await fb.lyrics.get(undefined, { type: 'synced' });
```

## Save Lyrics

`fb.lyrics.save(path, lyricsText, options?)` invokes `lyrics.save`. `options` is `LyricsSaveOptions` and can include `filename`, `tagName`, `format`, and `target`; a single-string `target` is sent to the host as a one-element array.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path the lyrics belong to. |
| `lyricsText` | `string` | Yes | Lyrics text to save. |
| `options.target` | `LyricsSaveTarget \| LyricsSaveTarget[]` | No | `'file'` (a sidecar next to the audio file, the default), `'embedded'` (a tag in the file), or `'all'` for both. |
| `options.filename` | `string` | No | Exact file name, including its extension, for the `file` target. A non-empty value overrides the default name and `format`. |
| `options.tagName` | `string` | No | Tag the `embedded` target writes; defaults to `'LYRICS'`. |
| `options.format` | `string` | No | `'lrc'` or `'txt'`, the extension for a default sidecar name; defaults to `'lrc'`. Ignored for a non-empty `filename`. |

The SDK return type is `LyricsSaveResponse`. With one target it describes that write (`savedTo` for a file, the tag-write receipt for `embedded`); with several targets each outcome is keyed under `results`, and the call fails with `OPERATION_FAILED` only when every target failed. The `embedded` target only queues the tag write: `dispatched: true` in its receipt means it was queued, and the final outcome arrives as the `metadata:writeComplete` event.

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
const all = await fb.lyrics.save(path, text, { target: 'all' }); // file and tag
if (all.success === false) throw new Error(all.error);
```

All three methods apply the same Windows filename validation. Missing tags prevent deriving a subsong's default file name; specify `filename` in that case. File saves invalidate cached reads before returning, while embedded writes invalidate them when metadata changes. The removed `config` target and older subsong names are covered in the [migration notes](/api/lyrics#migrating-older-callers).
