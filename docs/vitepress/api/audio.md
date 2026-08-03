# Audio API

English API reference for the `audio`, `dsp`, `output`, `replaygain` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## audio

### audio.analyzeBPM


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `forceAnalysis` | `boolean` | No | Optional; default false. Set true to skip the existing `BPM` tag and fall through to genre estimation. |
| `path` | `string` | Yes | Track path to analyze. |

**Returns**: `{"bpm":"...","confidence":"...","error":"...","source":"...","success":true}`

```js
const { bpm, source } = await fb2k.invoke('audio.analyzeBPM', {
    path: 'C:\\Music\\song.flac'
});
```

### audio.generateFullWaveform


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | No | Optional; default -1. Subsong index; values `>= 0` override the `|subsong:N` suffix in `path`. |
| `method` | `string` | No | Optional; default rms. Accepts `rms` or `peak`. |
| `path` | `string` | Yes | Track path to decode. |
| `preferCache` | `boolean` | No | Optional; default true. |
| `resolution` | `integer` | No | Optional; default 256. Clamped to 64-4096. |
| `scale` | `string` | No | Optional; default linear. Accepts `linear` or `db`. |
| `signed` | `boolean` | No | Optional; default false. |

**Returns**: `{"cached":"...","channels":"...","duration":"...","method":"...","path":"...","resolution":"...","sampleRate":"...","scale":"...","signed":"...","status":"...","success":true,"taskId":"...","waveform":"..."}`

```js
// Minimal call: cache hit returns status 'ready', otherwise 'pending' + taskId
const { status, taskId } = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'C:\\Music\\song.flac'
});

// Higher-resolution peak waveform on a dB scale
const detailed = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'C:\\Music\\song.flac',
    resolution: 1000,
    method: 'peak',
    scale: 'db'
});
```

### audio.generateWaveform


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path to inspect. |
| `resolution` | `integer` | No | Optional; default 800. Clamped to 50-4000. |

**Returns**: `{"channels":"...","duration":"...","error":"...","requestedResolution":"...","sampleRate":"...","success":true}`

```js
const result = await fb2k.invoke('audio.generateWaveform', {
    path: 'C:\\Music\\song.flac'
});
```

### audio.getOutputInfo


_No parameters._

**Returns**: `{"error":"...","success":true,"volume":"...","volumePercent":"..."}`

```js
const result = await fb2k.invoke('audio.getOutputInfo');
```

### audio.getSpectrum


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `bands` | `integer` | No | Optional; default 0, meaning the band count of the active subscription. |

**Returns**: `{"bands":"...","error":"...","fftSize":"...","spectrum":"...","success":true}`

```js
const { spectrum } = await fb2k.invoke('audio.getSpectrum', { bands: 48 });
```

### audio.getSpectrumDebugState


_No parameters._

**Returns**: `{"active":"...","callerHwnd":"...","callerOwnsSubscription":"...","callerWindowId":"...","dispatchTargetCount":"...","dispatchTargets":"...","effectiveBands":"...","effectiveFftSize":"...","effectiveFps":"...","foregroundHwnd":"...","foregroundIsExternal":"...","foregroundPid":"...","foregroundTitle":"...","instanceCount":"...","skipFrames":"...","streamReady":"...","subscriptionCount":"...","subscriptions":"...","success":true,"timerHwnd":"...","timerRunning":"..."}`

```js
const result = await fb2k.invoke('audio.getSpectrumDebugState');
```

### audio.getStreamInfo


_No parameters._

**Returns**: `{"bitrate":"...","channels":"...","codec":"...","duration":"...","error":"...","playing":"...","sampleRate":"...","success":true}`

```js
const result = await fb2k.invoke('audio.getStreamInfo');
```

### audio.getWaveform


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `duration` | `number` | No | Optional; default 0.05. Window length in seconds. |
| `signed` | `boolean` | No | Optional; default false. |

**Returns**: `{"duration":"...","error":"...","signed":"...","success":true,"waveform":"..."}`

```js
const { waveform } = await fb2k.invoke('audio.getWaveform', { duration: 0.1 });
```

### audio.isVisualizationAvailable


_No parameters._

**Returns**: `{"available":"...","success":true}`

```js
const result = await fb2k.invoke('audio.isVisualizationAvailable');
```

### audio.setChannelMode


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | One of `default`, `mono`, `front`, `back`. Defaults to `default`, which is also used for any other value. |

**Returns**: `{"mode":"...","success":true}`

```js
await fb2k.invoke('audio.setChannelMode', { mode: 'mono' });
```

### audio.subscribeSpectrum


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Optional. Omit to reuse the per-window legacy token derived from `event`. |
| `fftSize` | `integer` | No | Optional; default 1024. Must be a power of two between 256 and 16384. |
| `event` | `string` | No | Optional; default audio:spectrum. |
| `fps` | `integer` | No | Optional; default 30. Clamped to 1-60. |
| `bands` | `integer` | No | Optional; default 48. Clamped to 8-`fftSize / 2`. |

**Returns**: `{"bands":"...","error":"...","event":"...","fftSize":"...","fps":"...","subscriptionId":"...","success":true}`

```js
// Minimal call: all defaults, result delivered on 'audio:spectrum'
const { subscriptionId } = await fb2k.invoke('audio.subscribeSpectrum');

// Explicit configuration
const custom = await fb2k.invoke('audio.subscribeSpectrum', {
    bands: 64,
    fftSize: 2048,
    fps: 60
});
```

### audio.subscribeStream


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `event` | `string` | No | Optional; default audio:stream. |
| `interval` | `number` | No | Optional; default 0.05. Interval in seconds. |

**Returns**: `{"error":"...","event":"...","interval":"...","success":true}`

```js
const result = await fb2k.invoke('audio.subscribeStream', { interval: 0.1 });
```

### audio.unsubscribeSpectrum


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Optional. Omit to remove the per-window legacy subscription. |

**Returns**: `{"removed":"...","subscriptionId":"...","success":true}`

```js
await fb2k.invoke('audio.unsubscribeSpectrum', { subscriptionId });
```

### audio.unsubscribeStream


_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('audio.unsubscribeStream');
```

## dsp

> Note: `dsp.getActivePreset` / `dsp.setActivePreset` are not registered on the C++ side — use `config.getActiveDspPreset` / `config.setActiveDspPreset` instead.

### dsp.addDsp


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | GUID of an installed DSP, as reported by `dsp.getAvailable`. |
| `position` | `integer` | No | Optional; default -1 (append to the end). |

**Returns**: `{"addedDsp":"...","error":"...","position":"...","success":true}`

```js
const { dsps } = await fb2k.invoke('dsp.getAvailable');
const eq = dsps.find(d => d.name === 'Equalizer');
if (eq) {
    await fb2k.invoke('dsp.addDsp', { guid: eq.guid });
}
```

### dsp.applyPreset


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | Optional; omitted by default. Preset index as reported by `dsp.getPresets`. |
| `name` | `string` | No | Optional; omitted by default. Preset name as reported by `dsp.getPresets`. |

**Returns**: `{"appliedIndex":"...","appliedPreset":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.applyPreset', { name: 'Headphones' });
```

Supply either `index` or `name`; at least one is required, and `index` wins when
both are present. Both address the same presets, and the response echoes
`appliedPreset` / `appliedIndex` either way. Applying a preset replaces the whole
active chain; it never writes back to the stored preset, so the files under
`profile\dsp-presets\<name>.fb2k-dsp` are left untouched.

### dsp.getAvailable


_No parameters._

**Returns**: `{"count":"...","dsps":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.getAvailable');
```

### dsp.getChain


_No parameters._

**Returns**: `{"activePreset":"...","activePresetIndex":"...","dsps":"..."}`

```js
const result = await fb2k.invoke('dsp.getChain');
```

`activePreset` and `activePresetIndex` are always present. When no preset is
selected — including right after `dsp.setChain`, `addDsp`, `removeDsp` or
`moveDsp` edit the chain by hand — they report `null` and `-1` respectively
rather than being omitted.

### dsp.getPresets


_No parameters._

**Returns**: `{"count":"...","error":"...","presets":"...","selectedIndex":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.getPresets');
```

`selectedIndex` is `-1` when no preset is selected. Presets live in
`profile\dsp-presets\<name>.fb2k-dsp`.

### dsp.moveDsp


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `from` | `integer` | Yes | Current index of the DSP to move. |
| `to` | `integer` | Yes | Target index in the reordered chain. |

**Returns**: `{"error":"...","from":"...","message":"...","movedDsp":"...","success":true,"to":"..."}`

```js
const result = await fb2k.invoke('dsp.moveDsp', { from: 2, to: 0 });
```

`to` is the final index in the reordered chain and matches the value you passed,
in both directions. When `from === to` nothing moves and the response carries
`message: "No change needed"`. Use this — not `getChain` fed back into
`setChain` — to reorder a chain, because it preserves each DSP's configuration.

### dsp.removeDsp


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | Yes | Index of the DSP to remove from the active chain. |

**Returns**: `{"error":"...","removedDsp":"...","removedIndex":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.removeDsp', { index: 0 });
```

### dsp.setChain


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `dsps` | `array` | Yes | Ordered chain entries; each element is an object carrying a `guid`. |

**Returns**: `{"count":"...","error":"...","success":true}`

```js
const { dsps } = await fb2k.invoke('dsp.getAvailable');
const eq = dsps.find(d => d.name === 'Equalizer');
await fb2k.invoke('dsp.setChain', { dsps: [{ guid: eq.guid }] });

// Clear the chain
await fb2k.invoke('dsp.setChain', { dsps: [] });
```

Replaces the entire chain. Passing `dsps: []` clears it. Each element must be an
object carrying a `guid`; entries are applied in array order.

Every entry must resolve to an installed DSP — the call is rejected as a whole,
without touching the current chain, and the error names the offending index:

| Condition | Error |
| --- | --- |
| `dsps` absent or not an array | `dsps array is required` |
| Element is not an object | `dsps[0] must be an object` |
| `guid` missing, empty, or not a string | `dsps[0]: guid is required` |
| `guid` malformed | `dsps[0]: Invalid GUID format: <value>` |
| `guid` well-formed but DSP not installed | `dsps[0]: DSP not found or no default preset: <guid>` |

Because entries only carry a `guid`, each DSP is added using its default preset.
**Whether that keeps the DSP's current settings depends on the DSP itself** —
many foobar2000 DSPs store configuration globally, so their settings survive, but
a DSP that keeps configuration per preset instance (VST wrappers, some
third-party DSPs) will fall back to factory values. Do not rely on `setChain` to
preserve configuration; use `moveDsp` when you only need to reorder.

Editing the chain this way detaches it from any preset, so `getChain` afterwards
reports `activePreset: null` and `getPresets` reports `selectedIndex: -1`.

## output

### output.getDevices


_No parameters._

**Returns**: `{"count":"...","devices":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('output.getDevices');
```

**`guid` is not unique within this response.** foobar2000 reports an output
module's "default device" using the all-zero GUID
`{00000000-0000-0000-0000-000000000000}`, so it appears once per module. Key
devices by the `(entryGuid, guid)` pair rather than by `guid` alone.

### output.getEntries


_No parameters._

**Returns**: `{"count":"...","entries":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('output.getEntries');
```

### output.getSettings


_No parameters._

**Returns**: `{"availableOutputs":"...","note":"..."}`

```js
const result = await fb2k.invoke('output.getSettings');
```

Informational only — output configuration is owned by foobar2000 Preferences, and
`config.setOutputDevice` is the way to switch devices.

**Avoid `availableOutputs` in new code.** It is a bare list of display names with
two observed problems: modules that share a display name are indistinguishable,
and some modules report an empty name. Its order comes from service enumeration
and is **not stable between calls**, so array indices are not usable as
identifiers. Use `output.getEntries`, which pairs each name with its GUID.

## replaygain

### replaygain.clear


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | File path list whose ReplayGain metadata should be removed. |

**Returns**: `{"clearedCount":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.clear', {
    paths: ['C:\\Music\\song.flac']
});
```

### replaygain.get


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | File path list to read ReplayGain metadata from. |

**Returns**: `{"count":"...","error":"...","results":"...","success":true}`

```js
const { results } = await fb2k.invoke('replaygain.get', {
    paths: ['C:\\Music\\song.flac']
});
```

### replaygain.getMode


_No parameters._

**Returns**: `{"error":"...","processingMode":"...","sourceMode":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.getMode');
```

### replaygain.getPreamp


_No parameters._

**Returns**: `{"error":"...","success":true,"withRg":"...","withoutRg":"..."}`

```js
const result = await fb2k.invoke('replaygain.getPreamp');
```

### replaygain.getSettings


_No parameters._

**Returns**: `{"active":"...","error":"...","preampWithRg":"...","preampWithoutRg":"...","processingMode":"...","sourceMode":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.getSettings');
```

### replaygain.scan


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | Optional; default track. Accepts `track` or `album`. |
| `paths` | `array` | Yes | File path list to scan. |

**Returns**: `{"error":"...","mode":"...","note":"...","scannedCount":"...","success":true}`

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


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `processingMode` | `string` | No | Optional; omitted by default. Accepts `none`, `gain`, `gain_and_peak`, `peak`. |
| `sourceMode` | `string` | No | Optional; omitted by default. Accepts `none`, `track`, `album`, `auto` (alias `byPlaybackOrder`). |

**Returns**: `{"changed":"...","error":"...","processingMode":"...","sourceMode":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.setMode', { sourceMode: 'album' });
```

### replaygain.setPreamp


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `withoutRg` | `number` | No | Optional; omitted by default. Preamp in dB for tracks without ReplayGain info; clamped to -24..+24. |
| `withRg` | `number` | No | Optional; omitted by default. Preamp in dB for tracks with ReplayGain info; clamped to -24..+24. |

**Returns**: `{"changed":"...","error":"...","success":true,"withRg":"...","withoutRg":"..."}`

```js
const result = await fb2k.invoke('replaygain.setPreamp', { withRg: -3 });
```

## Runtime behavior notes

- `audio.subscribeSpectrum` creates or updates a caller-owned subscription. Omit `subscriptionId` to use the runtime's caller-scoped legacy identifier; listen for the configured `event`, which defaults to `audio:spectrum`.
- The SDK convenience call `fb.audio.subscribeSpectrum(` configures the same underlying subscription. The unimplemented stream-capture stub keeps its default event token `audio:stream`.
- `audio.getSpectrum` and `audio.getWaveform` consume the visualization stream. They return an error until a spectrum subscription exists and audio data is available.
- `audio.generateWaveform` currently returns file metadata plus a failure explaining that decoder-backed waveform generation is not implemented. Use `audio.generateFullWaveform` for the asynchronous cache-backed workflow.
- `audio.generateFullWaveform` returns `status: "ready"` with cached data or `status: "pending"` with `taskId`. The caller receives `audio:fullWaveformReady` or `audio:fullWaveformFailed`; `cueIndex`, when non-negative, takes precedence over a `path|subsong:N` suffix.
- `audio.subscribeStream` is a capability stub: it returns `success: false` until `playback_stream_capture` is integrated. `audio.unsubscribeStream` remains safe to call.
- DSP registrations are present in every build. When the foobar2000 DSP SDK surface is unavailable, all `dsp.*` methods return the runtime's "DSP API not available in this build" failure instead of emulating a chain.
- `output.getSettings` is read-only discovery information. Output configuration is managed by foobar2000 Preferences rather than this API.
- `replaygain.get` reads each supplied media path; `replaygain.clear` writes ReplayGain metadata asynchronously through foobar2000. `replaygain.scan` requests the host scanner and is not a synchronous analysis result.
