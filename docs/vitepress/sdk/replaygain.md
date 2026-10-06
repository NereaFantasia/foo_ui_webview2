# fb.replaygain ReplayGain

`fb.replaygain` reads ReplayGain metadata, controls playback processing settings, and starts host-side scan or clear operations.

## clear(paths)

Signature: `fb.replaygain.clear(paths: string[]): Promise<ReplaygainClearResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string[] | Yes | File paths whose ReplayGain metadata should be cleared; at least one. A `\|subsong:N` suffix selects a subsong, such as one track of a CUE sheet. |

Removes the ReplayGain values from the files. The response carries `clearedCount`, the number of files handed to the tag writer. The tags are rewritten in the background, so read the files again with `get()` to see the values gone.

```javascript
const result = await fb.replaygain.clear(['E:\\Music\\song.flac']);
```

## get(paths)

Signature: `fb.replaygain.get(paths: string | string[]): Promise<ReplaygainGetResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| paths | string \| string[] | Yes | One file path or an array of paths; the SDK always sends `{ paths: string[] }` |

Returns per-track results with optional track/album gain and peak fields. The response carries `count` and `results`, one row per requested path in request order. Each row (`ReplayGainTrackInfo`) echoes the requested `path` and has `success`; a file the host cannot read is a failed row with an `error`, not a failed call.

A readable row has `hasReplayGain` and whichever values the file stores: `trackGain` and `albumGain` as text such as `-7.25 dB`, `trackPeak` and `albumPeak` as text with six decimals, and the same values as numbers in `trackGainRaw`, `albumGainRaw` (dB), `trackPeakRaw`, and `albumPeakRaw` (linear amplitude). A `|subsong:N` suffix on a path selects a subsong, such as one track of a CUE sheet; without it the first track is read.

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

Returns `sourceMode` and `processingMode`. `sourceMode` says where the gain applied during playback comes from: `none` applies no gain, `track` and `album` use the respective stored value, and `auto` follows the playback order (track gain in shuffle, album gain otherwise). `processingMode` says what is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing.

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
| paths | string[] | Yes | Files to scan; an empty list fails with `INVALID_PARAMS`. A `\|subsong:N` suffix selects a subsong, such as one track of a CUE sheet. |
| opts | `{ mode?: 'track' \| 'album' }` | No | `mode` is `'track'` by default, which scans each file's own track gain; `'album'` treats the selection as one album |

Starts foobar2000's own ReplayGain scanner on the files through its context-menu command. The promise resolves once the scan has started; the scanner writes the results to the files afterwards. The response carries `scannedCount`, the `mode` the scan runs in, and `note`, a fixed English sentence saying the scan runs in the background.

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

Changes the source mode and, when given, the processing mode. The response reports the `sourceMode` and `processingMode` in effect afterwards and `changed`, whether the call changed anything.

```javascript
const result = await fb.replaygain.setMode('track');
```

## setPreamp(withRg?, withoutRg?)

Signature: `fb.replaygain.setPreamp(withRg?: number, withoutRg?: number): Promise<ReplaygainSetPreampResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| withRg | number | No | Preamp in dB for tracks with ReplayGain metadata, from -24 to 24 |
| withoutRg | number | No | Preamp in dB for tracks without ReplayGain metadata, from -24 to 24 |

Only defined arguments are sent, and a preamp left out keeps its value. The response reports `withRg` and `withoutRg` in effect afterwards and `changed`, whether the call changed anything.

```javascript
const result = await fb.replaygain.setPreamp(6, 0);
```
