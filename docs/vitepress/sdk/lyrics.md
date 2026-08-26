# fb.lyrics Lyrics

`fb.lyrics` locates, checks, and saves lyrics for a track. When no path is supplied to `get()`, the host uses the currently playing track.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### exists()

Signature: `fb.lyrics.exists(path: string): Promise<{ exists: boolean }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path to inspect. |

Returns `{ exists }`.

```javascript
const { exists } = await fb.lyrics.exists('E:\\Music\\song.flac');
```

### get()

Signature: `fb.lyrics.get(path?: string, options?: Omit<LyricsGetParams, 'path'>): Promise<LyricsGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Track path; omit it to use the current track. |
| `options.source` | `string` | No | `'embedded'`, `'file'`, or `'any'`. |
| `options.type` | `string` | No | `'synced'`, `'unsynced'`, or `'any'`. |
| `options.format` | `string` | No | `'lrc'`, `'txt'`, or `'any'`. |

Returns `LyricsGetResponse`. Read `available` first; successful matches can include `source`, `sourcePath`, `tagName`, `lyrics`, and `synced`.

```javascript
const result = await fb.lyrics.get(undefined, { type: 'synced' });
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Save Lyrics

`fb.lyrics.save(path, lyricsText, options?)` invokes `lyrics.save`. `options` is `Omit<LyricsSaveParams, 'path' | 'lyrics'>` and can include `filename`, `tagName`, `format`, and `target`.

The SDK return type is `BaseResponse & { results?: Array<{ target: string; success: boolean; error?: string }>; savedTo?: string[] }`.

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
