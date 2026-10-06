# ReplayGain API

Methods of the `replaygain` namespace.

## replaygain

### replaygain.clear

<!-- api-schema:begin replaygain.clear -->
Remove the ReplayGain values from files. The tags are rewritten in the background; read the files again to see the values gone.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Tracks whose ReplayGain values are removed. A `\|subsong:N` suffix selects a subsong, such as one track of a CUE sheet; a path without it means the first track. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `clearedCount` | `integer` | Number of files handed to the tag writer. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('replaygain.clear', {
    paths: ['C:\\Music\\song.flac']
});
```

### replaygain.get

<!-- api-schema:begin replaygain.get -->
Read the ReplayGain values stored in files, one row per path. A file the host cannot read is a failed row, not a failed call.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Tracks to read. A `\|subsong:N` suffix selects a subsong, such as one track of a CUE sheet; a path without it reads the first track. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `count` | `integer` | Number of rows in `results`. |
| `results` | `ReplayGainTrackInfo[]` | One row per requested path, in request order. |
| `results[].path` | `string` | The path as requested, including any `\|subsong:N` suffix. |
| `results[].success` | `boolean` | Whether the file could be read. |
| `results[].error` | `string` | Why the file could not be read; present when `success` is `false`. |
| `results[].hasReplayGain` | `boolean` | Whether the file stores a track gain or an album gain. Present when `success` is `true`. |
| `results[].trackGain` | `string` | Track gain formatted with two decimals and a `dB` suffix, such as `-7.25 dB`. |
| `results[].trackGainRaw` | `number` | Track gain in dB. |
| `results[].trackPeak` | `string` | Track peak formatted with six decimals. |
| `results[].trackPeakRaw` | `number` | Track peak as a linear amplitude. |
| `results[].albumGain` | `string` | Album gain formatted with two decimals and a `dB` suffix. |
| `results[].albumGainRaw` | `number` | Album gain in dB. |
| `results[].albumPeak` | `string` | Album peak formatted with six decimals. |
| `results[].albumPeakRaw` | `number` | Album peak as a linear amplitude. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('replaygain.get', {
    paths: ['C:\\Music\\song.flac']
});
if (res.success === false) throw new Error(res.error);
const { results } = res;
```

### replaygain.getMode

<!-- api-schema:begin replaygain.getMode -->
Read the source mode and the processing mode.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | Where the gain applied during playback comes from: `none` applies no gain, `track` and `album` use the respective stored value, `auto` follows the playback order (track gain in shuffle, album gain otherwise). |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('replaygain.getMode');
```

### replaygain.getPreamp

<!-- api-schema:begin replaygain.getPreamp -->
Read the two preamps, in dB.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `withRg` | `number` | Preamp applied to tracks that carry ReplayGain, in dB. |
| `withoutRg` | `number` | Preamp applied to tracks without ReplayGain, in dB. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('replaygain.getPreamp');
```

### replaygain.getSettings

<!-- api-schema:begin replaygain.getSettings -->
Read the whole ReplayGain configuration: source mode, processing mode, both preamps and whether ReplayGain is in effect.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | Where the gain applied during playback comes from: `none` applies no gain, `track` and `album` use the respective stored value, `auto` follows the playback order (track gain in shuffle, album gain otherwise). |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing. |
| `preampWithRg` | `number` | Preamp applied to tracks that carry ReplayGain, in dB. |
| `preampWithoutRg` | `number` | Preamp applied to tracks without ReplayGain, in dB. |
| `active` | `boolean` | Whether ReplayGain changes the output: `false` when the source mode is `none` or `auto`, or the processing mode is `none`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('replaygain.getSettings');
```

### replaygain.scan

<!-- api-schema:begin replaygain.scan -->
Start foobar2000's own ReplayGain scanner on the files through its context menu command. The call returns when the scan has been started; results are written to the files by the scanner.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `string[]` | Yes | Tracks to scan. A `\|subsong:N` suffix selects a subsong, such as one track of a CUE sheet; a path without it means the first track. Must not be empty. |
| `mode` | `"track" \| "album"` | No | `track` scans each file's own gain; `album` scans the files as one album. Default: `"track"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `scannedCount` | `integer` | Number of files handed to the scanner. |
| `mode` | `"track" \| "album"` | The mode the scan runs in. |
| `note` | `string` | A fixed English note saying the scan runs in the background. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
// Per-file track gain (default)
await fb2k.invoke('replaygain.scan', { paths: ['C:\\Music\\song.flac'] });

// Scan the selection as a single album
await fb2k.invoke('replaygain.scan', {
    paths: ['C:\\Music\\song.flac', 'C:\\Music\\other.flac'],
    mode: 'album'
});
```

### replaygain.setMode

<!-- api-schema:begin replaygain.setMode -->
Change the source mode, the processing mode, or both; a key left out keeps its value. The response reports the modes in effect afterwards.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto" \| "byPlaybackOrder"` | No | A source mode to set; `byPlaybackOrder` is the older spelling of `auto`. |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | No | What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `sourceMode` | `"none" \| "track" \| "album" \| "auto"` | Where the gain applied during playback comes from: `none` applies no gain, `track` and `album` use the respective stored value, `auto` follows the playback order (track gain in shuffle, album gain otherwise). |
| `processingMode` | `"none" \| "gain" \| "gain_and_peak" \| "peak"` | What is applied: `gain` the gain only, `peak` peak limiting only, `gain_and_peak` both, `none` nothing. |
| `changed` | `boolean` | Whether the call changed anything. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('replaygain.setMode', { sourceMode: 'album' });
```

### replaygain.setPreamp

<!-- api-schema:begin replaygain.setPreamp -->
Change one or both preamps; a key left out keeps its value. The response reports the values in effect afterwards.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `withRg` | `number` | No | Preamp for tracks that carry ReplayGain, in dB. Between `-24` and `24` inclusive. |
| `withoutRg` | `number` | No | Preamp for tracks without ReplayGain, in dB. Between `-24` and `24` inclusive. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `withRg` | `number` | Preamp applied to tracks that carry ReplayGain, in dB. |
| `withoutRg` | `number` | Preamp applied to tracks without ReplayGain, in dB. |
| `changed` | `boolean` | Whether the call changed anything. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('replaygain.setPreamp', { withRg: -3 });
```

## Runtime behavior notes

- `replaygain.get` reads each supplied media path; `replaygain.clear` writes ReplayGain metadata asynchronously through foobar2000. `replaygain.scan` requests the host scanner and is not a synchronous analysis result.
