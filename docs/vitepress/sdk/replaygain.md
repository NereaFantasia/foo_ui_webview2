# fb.replaygain ReplayGain

`fb.replaygain` reads ReplayGain metadata, controls playback processing settings, and starts host-side scan or clear operations.

## clear(paths)

Signature: `fb.replaygain.clear(paths: string[]): Promise<ReplaygainClearResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string[] | Yes | File paths whose ReplayGain metadata should be cleared; at least one. A `\|subsong:N` suffix selects a subsong, such as one track of a CUE sheet |

Removes ReplayGain metadata from specified files. The response carries `clearedCount` indicating how many files were submitted to the tag writer. Because tags are updated asynchronously in the background, invoke `get()` subsequently to verify tag removal.

```javascript
const result = await fb.replaygain.clear(['E:\\Music\\song.flac']);
```

## get(paths)

Signature: `fb.replaygain.get(paths: string | string[]): Promise<ReplaygainGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string \| string[] | Yes | One file path or an array of paths; the SDK always sends `{ paths: string[] }` |

Returns per-track results with `count` and `results` matching the input paths in order. Each row (`ReplayGainTrackInfo`) echoes the requested `path` with its `success` status; unreadable files return per-item failure rows containing `error` rather than rejecting the overall call.

Successfully read rows report `hasReplayGain` alongside stored values: formatted text in `trackGain` and `albumGain` (such as `-7.25 dB`), formatted peaks in `trackPeak` and `albumPeak` (with six decimal places), and raw numeric values in `trackGainRaw`, `albumGainRaw` (dB), `trackPeakRaw`, and `albumPeakRaw` (linear amplitude). A `|subsong:N` suffix selects a specific subsong (e.g. from a CUE sheet); omitting the suffix targets the primary subsong.

```javascript
const result = await fb.replaygain.get('E:\\Music\\song.flac');

const res = await fb.replaygain.get(['E:\\Music\\one.flac', 'E:\\Music\\two.flac']);
if (res.success === false) throw new Error(res.error);
for (const row of res.results) {
  if (row.success) console.log(row.path, row.trackGain ?? 'no track gain');
}
```

## getMode()

Signature: `fb.replaygain.getMode(): Promise<ReplaygainGetModeResponse>`

Returns `sourceMode` and `processingMode`. `sourceMode` defines the gain source applied during playback: `none` applies no gain, `track` and `album` use their respective tags, and `auto` follows the active playback order (track gain during shuffle, album gain otherwise). `processingMode` specifies the processing behavior: `gain` applies volume adjustments only, `peak` applies peak limiting only, `gain_and_peak` applies both, and `none` disables ReplayGain processing.

```javascript
const result = await fb.replaygain.getMode();
```

## getPreamp()

Signature: `fb.replaygain.getPreamp(): Promise<ReplaygainGetPreampResponse>`

Returns `withRg` and `withoutRg` preamp values in dB: `withRg` applies to tracks that carry ReplayGain, `withoutRg` to tracks without it.

```javascript
const result = await fb.replaygain.getPreamp();
```

## getSettings()

Signature: `fb.replaygain.getSettings(): Promise<ReplaygainGetSettingsResponse>`

Returns the source mode, processing mode, both preamp values, and active state in one snapshot: `sourceMode`, `processingMode`, `preampWithRg`, `preampWithoutRg` (dB), and `active`. `active` is `false` when the source mode is `none` or `auto`, or the processing mode is `none`. To start analysis, call `scan(paths, { mode? })`.

```javascript
const result = await fb.replaygain.getSettings();
```

## scan(paths, opts?)

Signature: `fb.replaygain.scan(paths: string[], opts?: { mode?: 'track' | 'album' }): Promise<ReplaygainScanResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string[] | Yes | Files to scan; an empty list fails with `INVALID_PARAMS`. A `\|subsong:N` suffix selects a subsong, such as one track of a CUE sheet |
| opts | `{ mode?: 'track' \| 'album' }` | No | `mode` is `'track'` by default, which scans each file's own track gain; `'album'` treats the selection as one album |

Invokes foobar2000's native ReplayGain scanner via context-menu integration. The promise resolves once background scanning is dispatched, with results written to files upon completion. The response returns `scannedCount`, the active scan `mode`, and an explanatory `note`.

```javascript
await fb.replaygain.scan(['E:\\Music\\one.flac'], { mode: 'track' });

const res = await fb.replaygain.scan(['E:\\Music\\one.flac', 'E:\\Music\\two.flac'], { mode: 'album' });
if (res.success === false) throw new Error(res.error);
console.log(`Scanning ${res.scannedCount} files in ${res.mode} mode`);
```

## setMode(sourceMode, processingMode?)

Signature: `fb.replaygain.setMode(sourceMode: NonNullable<ReplaygainSetModeParams['sourceMode']>, processingMode?: ReplaygainSetModeParams['processingMode']): Promise<ReplaygainSetModeResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| sourceMode | string | Yes | ReplayGain source mode: `none`, `track`, `album`, or `auto`; `byPlaybackOrder` is accepted as the older spelling of `auto` |
| processingMode | string | No | ReplayGain processing mode: `none`, `gain`, `gain_and_peak`, or `peak`; omitted, it keeps its value |

Updates the source mode, and optionally the processing mode when specified. The response returns the active `sourceMode`, `processingMode`, and a `changed` boolean indicating whether settings were modified.

```javascript
const result = await fb.replaygain.setMode('track');
```

## setPreamp(withRg?, withoutRg?)

Signature: `fb.replaygain.setPreamp(withRg?: number, withoutRg?: number): Promise<ReplaygainSetPreampResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| withRg | number | No | Preamp in dB for tracks with ReplayGain metadata, from -24 to 24 |
| withoutRg | number | No | Preamp in dB for tracks without ReplayGain metadata, from -24 to 24 |

Updates preamp values; omitted parameters remain unchanged. The response returns the active `withRg`, `withoutRg`, and a `changed` boolean.

```javascript
const result = await fb.replaygain.setPreamp(6, 0);
```
