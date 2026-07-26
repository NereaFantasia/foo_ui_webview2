# Audio API

English API reference for the `audio`, `dsp`, `output`, `replaygain` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## audio

### audio.analyzeBPM

Public API method. Runtime authority: `src/api/AudioApi.cpp:1744`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `forceAnalysis` | `boolean` | No | Optional; default false. |
| `path` | `string` | No | Optional; default . |

**Returns**: `{"bpm":"...","confidence":"...","error":"...","source":"...","success":true}`

```js
const result = await fb2k.invoke('audio.analyzeBPM', { forceAnalysis: /* value */, path: /* value */ });
```

### audio.generateFullWaveform

Public API method. Runtime authority: `src/api/AudioApi.cpp:1746`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `cueIndex` | `integer` | No | Optional; default -1. |
| `method` | `string` | No | Optional; default rms. |
| `path` | `string` | No | Optional; default . |
| `preferCache` | `boolean` | No | Optional; default true. |
| `resolution` | `integer` | No | Optional; default 256. |
| `scale` | `string` | No | Optional; default linear. |
| `signed` | `boolean` | No | Optional; default false. |

**Returns**: `{"cached":"...","channels":"...","duration":"...","method":"...","path":"...","resolution":"...","sampleRate":"...","scale":"...","signed":"...","status":"...","success":true,"taskId":"...","waveform":"..."}`

```js
const result = await fb2k.invoke('audio.generateFullWaveform', { cueIndex: /* value */, method: /* value */, path: /* value */, preferCache: /* value */, resolution: /* value */, scale: /* value */, signed: /* value */ });
```

### audio.generateWaveform

Public API method. Runtime authority: `src/api/AudioApi.cpp:1745`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |
| `resolution` | `integer` | No | Optional; default 800. |

**Returns**: `{"channels":"...","duration":"...","error":"...","requestedResolution":"...","sampleRate":"...","success":true}`

```js
const result = await fb2k.invoke('audio.generateWaveform', { path: /* value */, resolution: /* value */ });
```

### audio.getOutputInfo

Public API method. Runtime authority: `src/api/AudioApi.cpp:1749`.

_No parameters._

**Returns**: `{"error":"...","success":true,"volume":"...","volumePercent":"..."}`

```js
const result = await fb2k.invoke('audio.getOutputInfo');
```

### audio.getSpectrum

Public API method. Runtime authority: `src/api/AudioApi.cpp:1734`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `bands` | `integer` | No | Optional; default 0. |

**Returns**: `{"bands":"...","error":"...","fftSize":"...","spectrum":"...","success":true}`

```js
const result = await fb2k.invoke('audio.getSpectrum', { bands: /* value */ });
```

### audio.getSpectrumDebugState

Public API method. Runtime authority: `src/api/AudioApi.cpp:1735`.

_No parameters._

**Returns**: `{"active":"...","callerHwnd":"...","callerOwnsSubscription":"...","callerWindowId":"...","dispatchTargetCount":"...","dispatchTargets":"...","effectiveBands":"...","effectiveFftSize":"...","effectiveFps":"...","foregroundHwnd":"...","foregroundIsExternal":"...","foregroundPid":"...","foregroundTitle":"...","instanceCount":"...","skipFrames":"...","streamReady":"...","subscriptionCount":"...","subscriptions":"...","success":true,"timerHwnd":"...","timerRunning":"..."}`

```js
const result = await fb2k.invoke('audio.getSpectrumDebugState');
```

### audio.getStreamInfo

Public API method. Runtime authority: `src/api/AudioApi.cpp:1750`.

_No parameters._

**Returns**: `{"bitrate":"...","channels":"...","codec":"...","duration":"...","error":"...","playing":"...","sampleRate":"...","success":true}`

```js
const result = await fb2k.invoke('audio.getStreamInfo');
```

### audio.getWaveform

Public API method. Runtime authority: `src/api/AudioApi.cpp:1736`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `duration` | `number` | No | Optional; default 0.05. |
| `signed` | `boolean` | No | Optional; default false. |

**Returns**: `{"duration":"...","error":"...","signed":"...","success":true,"waveform":"..."}`

```js
const result = await fb2k.invoke('audio.getWaveform', { duration: /* value */, signed: /* value */ });
```

### audio.isVisualizationAvailable

Public API method. Runtime authority: `src/api/AudioApi.cpp:1751`.

_No parameters._

**Returns**: `{"available":"...","success":true}`

```js
const result = await fb2k.invoke('audio.isVisualizationAvailable');
```

### audio.setChannelMode

Public API method. Runtime authority: `src/api/AudioApi.cpp:1737`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | Optional; default default. |

**Returns**: `{"mode":"...","success":true}`

```js
const result = await fb2k.invoke('audio.setChannelMode', { mode: /* value */ });
```

### audio.subscribeSpectrum

Public API method. Runtime authority: `src/api/AudioApi.cpp:1732`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Optional; default . |
| `fftSize` | `integer` | No | Optional; default 1024. |
| `event` | `string` | No | Optional; default audio:spectrum. |
| `fps` | `integer` | No | Optional; default 30. |
| `bands` | `integer` | No | Optional; default 48. |

**Returns**: `{"bands":"...","error":"...","event":"...","fftSize":"...","fps":"...","subscriptionId":"...","success":true}`

```js
const result = await fb2k.invoke('audio.subscribeSpectrum', { bands: /* value */, event: /* value */, fftSize: /* value */, fps: /* value */, subscriptionId: /* value */ });
```

### audio.subscribeStream

Public API method. Runtime authority: `src/api/AudioApi.cpp:1740`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `event` | `string` | No | Optional; default audio:stream. |
| `interval` | `number` | No | Optional; default 0.05. |

**Returns**: `{"error":"...","event":"...","interval":"...","success":true}`

```js
const result = await fb2k.invoke('audio.subscribeStream', { event: /* value */, interval: /* value */ });
```

### audio.unsubscribeSpectrum

Public API method. Runtime authority: `src/api/AudioApi.cpp:1733`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Optional; default . |

**Returns**: `{"removed":"...","subscriptionId":"...","success":true}`

```js
const result = await fb2k.invoke('audio.unsubscribeSpectrum', { subscriptionId: /* value */ });
```

### audio.unsubscribeStream

Public API method. Runtime authority: `src/api/AudioApi.cpp:1741`.

_No parameters._

**Returns**: `{"success":true}`

```js
const result = await fb2k.invoke('audio.unsubscribeStream');
```

## dsp

> Note: `dsp.getActivePreset` / `dsp.setActivePreset` are not registered on the C++ side — use `config.getActiveDspPreset` / `config.setActiveDspPreset` instead.

### dsp.addDsp

Public API method. Runtime authority: `src/api/DspApi.cpp:418`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Required. |
| `position` | `integer` | No | Optional; default -1 (append to the end). |

**Returns**: `{"addedDsp":"...","error":"...","position":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.addDsp', { guid: /* value */, position: /* value */ });
```

### dsp.applyPreset

Public API method. Runtime authority: `src/api/DspApi.cpp:416`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | No | Optional; default omitted. |
| `name` | `string` | No | Optional; default omitted. |

**Returns**: `{"appliedIndex":"...","appliedPreset":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.applyPreset', { index: /* value */, name: /* value */ });
```

Supply either `index` or `name`; at least one is required, and `index` wins when
both are present. Both address the same presets, and the response echoes
`appliedPreset` / `appliedIndex` either way. Applying a preset replaces the whole
active chain; it never writes back to the stored preset, so the files under
`profile\dsp-presets\<name>.fb2k-dsp` are left untouched.

### dsp.getAvailable

Public API method. Runtime authority: `src/api/DspApi.cpp:417`.

_No parameters._

**Returns**: `{"count":"...","dsps":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.getAvailable');
```

### dsp.getChain

Public API method. Runtime authority: `src/api/DspApi.cpp:414`.

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

Public API method. Runtime authority: `src/api/DspApi.cpp:415`.

_No parameters._

**Returns**: `{"count":"...","error":"...","presets":"...","selectedIndex":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.getPresets');
```

`selectedIndex` is `-1` when no preset is selected. Presets live in
`profile\dsp-presets\<name>.fb2k-dsp`.

### dsp.moveDsp

Public API method. Runtime authority: `src/api/DspApi.cpp:420`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `from` | `integer` | Yes | Required. |
| `to` | `integer` | Yes | Required. |

**Returns**: `{"error":"...","from":"...","message":"...","movedDsp":"...","success":true,"to":"..."}`

```js
const result = await fb2k.invoke('dsp.moveDsp', { from: /* value */, to: /* value */ });
```

`to` is the final index in the reordered chain and matches the value you passed,
in both directions. When `from === to` nothing moves and the response carries
`message: "No change needed"`. Use this — not `getChain` fed back into
`setChain` — to reorder a chain, because it preserves each DSP's configuration.

### dsp.removeDsp

Public API method. Runtime authority: `src/api/DspApi.cpp:419`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `index` | `integer` | Yes | Required. |

**Returns**: `{"error":"...","removedDsp":"...","removedIndex":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.removeDsp', { index: /* value */ });
```

### dsp.setChain

Public API method. Runtime authority: `src/api/DspApi.cpp:421`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `dsps` | `array` | Yes | Required. |

**Returns**: `{"count":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('dsp.setChain', { dsps: /* value */ });
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

Public API method. Runtime authority: `src/api/OutputApi.cpp:144`.

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

Public API method. Runtime authority: `src/api/OutputApi.cpp:147`.

_No parameters._

**Returns**: `{"count":"...","entries":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('output.getEntries');
```

### output.getSettings

Public API method. Runtime authority: `src/api/OutputApi.cpp:150`.

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

Public API method. Runtime authority: `src/api/ReplayGainApi.cpp:538`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | Required. |

**Returns**: `{"clearedCount":"...","error":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.clear', { paths: /* value */ });
```

### replaygain.get

Public API method. Runtime authority: `src/api/ReplayGainApi.cpp:535`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `paths` | `array` | Yes | Required. |

**Returns**: `{"count":"...","error":"...","results":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.get', { paths: /* value */ });
```

### replaygain.getMode

Public API method. Runtime authority: `src/api/ReplayGainApi.cpp:523`.

_No parameters._

**Returns**: `{"error":"...","processingMode":"...","sourceMode":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.getMode');
```

### replaygain.getPreamp

Public API method. Runtime authority: `src/api/ReplayGainApi.cpp:529`.

_No parameters._

**Returns**: `{"error":"...","success":true,"withRg":"...","withoutRg":"..."}`

```js
const result = await fb2k.invoke('replaygain.getPreamp');
```

### replaygain.getSettings

Public API method. Runtime authority: `src/api/ReplayGainApi.cpp:520`.

_No parameters._

**Returns**: `{"active":"...","error":"...","preampWithRg":"...","preampWithoutRg":"...","processingMode":"...","sourceMode":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.getSettings');
```

### replaygain.scan

Public API method. Runtime authority: `src/api/ReplayGainApi.cpp:541`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | Optional; default track. |
| `paths` | `array` | Yes | Required. |

**Returns**: `{"error":"...","mode":"...","note":"...","scannedCount":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.scan', { mode: /* value */, paths: /* value */ });
```

### replaygain.setMode

Public API method. Runtime authority: `src/api/ReplayGainApi.cpp:526`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `processingMode` | `string` | No | Optional; default omitted. |
| `sourceMode` | `string` | No | Optional; default omitted. |

**Returns**: `{"changed":"...","error":"...","processingMode":"...","sourceMode":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.setMode', { processingMode: /* value */, sourceMode: /* value */ });
```

### replaygain.setPreamp

Public API method. Runtime authority: `src/api/ReplayGainApi.cpp:532`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `withoutRg` | `number` | No | Optional; default omitted. |
| `withRg` | `number` | No | Optional; default omitted. |

**Returns**: `{"changed":"...","error":"...","success":true,"withRg":"...","withoutRg":"..."}`

```js
const result = await fb2k.invoke('replaygain.setPreamp', { withRg: /* value */, withoutRg: /* value */ });
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

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:audio.subscribeSpectrum -->
### Contract supplement: `audio.subscribeSpectrum`

Verified contract supplement. Runtime authority: `src/api/AudioApi.cpp:719-762`.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `subscriptionId` | `string` | No | `` | Optional; default . |
| `fftSize` | `integer` | No | `1024` | Optional; default 1024. |
| `event` | `string` | No | `audio:spectrum` | Optional; default audio:spectrum. |
| `fps` | `integer` | No | `30` | Optional; default 30. |
| `bands` | `integer` | No | `48` | Optional; default 48. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `bands` | `json` | No |
| `event` | `json` | No |
| `fftSize` | `json` | No |
| `fps` | `json` | No |
| `subscriptionId` | `json` | No |

Semantics: omitted optional parameters use handler defaults; failure branches and error fields are defined by this source file.

```js
const result = await fb2k.invoke('audio.subscribeSpectrum', { subscriptionId: /* value */, fftSize: /* value */, event: /* value */, fps: /* value */, bands: /* value */ });
```
