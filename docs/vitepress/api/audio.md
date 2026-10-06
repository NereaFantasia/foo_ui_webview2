# Audio API

English API reference for the `audio`, `dsp`, `output`, `replaygain` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## audio

### audio.analyzeBPM


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Track path to analyze. |
| `forceAnalysis` | `boolean` | No | `false` | Skips the existing `BPM` tag and falls through to genre estimation. |

**Returns**: `{"bpm":"...","confidence":"...","error":"...","source":"...","success":true}`

```js
const { bpm, source } = await fb2k.invoke('audio.analyzeBPM', {
    path: 'C:\\Music\\song.flac'
});
```

### audio.cancelFullWaveform


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `taskId` | `string` | Yes | — | The `taskId` from a `pending` answer of `audio.generateFullWaveform`. |

**Returns**: `{"cancelled":"...","success":true}`

Only the caller that made the request can cancel it. `cancelled: false` means the task has already finished, does not exist, or belongs to another caller; the three are deliberately indistinguishable. A successful cancel sends that `taskId` exactly one `audio:fullWaveformFailed` with `code: "CANCELLED"` (it may arrive before this answer) and nothing after it. The decode keeps running while another request for the same track still waits on it; otherwise a queued decode is dropped and a running one is aborted. A missing `taskId` answers `REQUIRED_PARAM`; a non-string or empty one answers `INVALID_PARAMS`.

```js
const pending = await fb2k.invoke('audio.generateFullWaveform', { path: 'C:\\Music\\song.flac' });
if (pending.status === 'pending') {
    await fb2k.invoke('audio.cancelFullWaveform', { taskId: pending.taskId });
}
```

### audio.generateFullWaveform


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Track path to decode; accepts `path\|subsong:N`. |
| `cueIndex` | `integer` | No | `-1` | Subsong index; values `>= 0` override the `\|subsong:N` suffix in `path`. |
| `resolution` | `integer` | No | `256` | Clamped to 64-4096. |
| `method` | `string` | No | `rms` | Accepts `rms` or `peak`. |
| `scale` | `string` | No | `linear` | Accepts `linear` or `db`; ignored in `signed` mode. |
| `signed` | `boolean` | No | `false` | Keeps PCM polarity; output normalized to `[-1, 1]`. |
| `preferCache` | `boolean` | No | `true` |  |

**Returns**: `{"cached":"...","channels":"...","duration":"...","maxAmplitude":"...","method":"...","path":"...","resolution":"...","sampleRate":"...","scale":"...","signed":"...","status":"...","success":true,"taskId":"...","waveform":"..."}`

`waveform` is normalized by the largest value of the selected sequence (window RMS for `rms`, peak for `peak`, absolute value for `signed`); that value is returned as `maxAmplitude` in linear full-scale units. On the `linear` scale `waveform[i] * maxAmplitude` restores the absolute level; on `db`, dBFS = `(v·60 − 60) + 20·log10(maxAmplitude)`. A track is decoded once per `resolution`; other `method` / `signed` / `scale` requests are computed from the cache.

On a cache miss, requests for the same track and `resolution` that are already in flight share one decode; each keeps its own `taskId` and gets a ready event in its own shape. At most two tracks decode at once and the rest wait in submission order, which counts toward the caller's own timeout. Cancel a request that is no longer needed with `audio.cancelFullWaveform`. Every `taskId` gets exactly one terminal event, ready or failed (`code: "CANCELLED"` when cancelled); a cache hit answers synchronously without one. Requests from a popup that closes, or still pending when foobar2000 exits, are dropped without an event.

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `path` | `string` | Yes | — | Track path to inspect. |
| `resolution` | `integer` | No | `800` | Clamped to 50-4000. |

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

Computes one spectrum frame on demand. At least one spectrum subscription must exist.

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `subscriptionId` | `string` | No | — | Compute the frame with this subscription's parameters; `bands`, `scale` and the frequency range are then ignored. An unknown id fails with `NOT_FOUND`. |
| `bands` | `integer` | No | `0` | Used without `subscriptionId`. `0` takes the largest band count among all subscriptions; the value is capped at `fftSize / 2`. |
| `scale` | `string` | No | `weighted` | Used without `subscriptionId`. `weighted` or `db`, as in `audio.subscribeSpectrum`. |
| `minFrequency` | `number` | No | `20` | Used without `subscriptionId`. Band range as in `audio.subscribeSpectrum`. |
| `maxFrequency` | `number` | No | `sampleRate / 2` | Used without `subscriptionId`. Band range as in `audio.subscribeSpectrum`. |

**Returns**: `{"bands":"...","code":"...","details":"...","error":"...","fftSize":"...","hostTime":"...","maxFrequency":"...","minFrequency":"...","sampleRate":"...","scale":"...","spectrum":"...","state":"...","streamTime":"...","subscriptionId":"...","success":true}`

The answer is one frame with the fields listed under [`audio.subscribeSpectrum`](#audio-subscribespectrum), plus `success`. `fftSize` is the FFT size actually used. While playback is paused or stopped the answer is a silence frame whose `state` says which. Without any subscription, or while a new stream has not produced data yet (about 0.7 s after the first subscription), the answer is `success: false` with an `error` message.

::: tip Pull or push
Both deliver the same frame. Pull when drawing must follow the display frame by frame or needs more than 60 frames per second: subscribe with `fps: 1` to keep the subscription alive, call `audio.getSpectrum` with its `subscriptionId` from `requestAnimationFrame`, wait for each answer before the next call, and skip frames whose `streamTime` has not changed. Each call computes an FFT on the host's main thread, costing about as much as a pushed frame, so limit the calls to the frame rate you need rather than the display refresh rate. For up to 60 frames per second, pushed frames work equally well.
:::

```js
// Pull from requestAnimationFrame: one call at a time, at most about 60 calls per second
const { subscriptionId } = await fb2k.invoke('audio.subscribeSpectrum', { fps: 1 });
let pending = false;
let lastCall = 0;
let lastStreamTime;
function tick(now) {
    requestAnimationFrame(tick);
    if (pending || now - lastCall < 15) return;
    pending = true;
    lastCall = now;
    fb2k.invoke('audio.getSpectrum', { subscriptionId })
        .then((frame) => {
            if (frame.success && frame.state === 'playing' && frame.streamTime !== lastStreamTime) {
                lastStreamTime = frame.streamTime;
                draw(frame.spectrum);
            }
        })
        .finally(() => {
            pending = false;
        });
}
requestAnimationFrame(tick);
```

### audio.getSpectrumDebugState


_No parameters._

**Returns**: `{"active":"...","beatIntervalMs":"...","beatSource":"...","beatsCoalesced":"...","callerHwnd":"...","callerOwnsSubscription":"...","callerWindowId":"...","dispatchTargetCount":"...","dispatchTargets":"...","effectiveBands":"...","effectiveFftSize":"...","effectiveFps":"...","foregroundHwnd":"...","foregroundIsExternal":"...","foregroundPid":"...","foregroundTitle":"...","framesComputed":"...","instanceCount":"...","skipFrames":"...","streamReady":"...","subscriptionCount":"...","subscriptions":"...","success":true,"timerHwnd":"...","timerRunning":"..."}`

`effectiveFftSize`, `effectiveBands` and `effectiveFps` are the largest requested values among all subscriptions; each subscription still computes with its own. `subscriptions[]` entries carry each subscription's `scale`, `backgroundThrottle`, `minFrequency` and `maxFrequency` (`null` when the range follows `sampleRate / 2`). `framesComputed` counts FFTs since the host started and stays flat while playback is paused or stopped. `foregroundIsExternal` shows whether the external-foreground frame limit currently applies. `timerRunning` is `true` while the beat thread that pushes frames runs. `beatSource` names its timer: `high-resolution`, or `standard` on Windows 10 releases before 1803; `beatIntervalMs` is the current beat, 1000 / `effectiveFps` milliseconds. Both are `null` while the beat thread is not running. `beatsCoalesced` counts beats dropped because the previous one had not reached the host's main thread yet. `timerHwnd` is deprecated and always `0`.

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `duration` | `number` | No | `0.05` | Window length in seconds, greater than 0 and at most 1. The window starts at the current playback position and covers audio already buffered for output. |
| `signed` | `boolean` | No | `false` | `true` keeps PCM polarity, clamped to `[-1, 1]`; `false` maps the magnitude from -70..0 dB onto `[0, 1]`. |
| `channels` | `string` | No | `'mix'` | `'mix'` averages the channels into `waveform`; `'stereo'` returns the first two channels as `left` and `right`. |
| `points` | `integer` | No | — | Thin the window to this many evenly spaced samples, without averaging. Integer in `[2, 65536]`; every sample is returned when omitted. |

**Returns**: `{"channelCount":"...","channels":"...","duration":"...","left":"...","right":"...","sampleRate":"...","signed":"...","success":true,"waveform":"..."}`

- `'mix'` answers carry `waveform`; `'stereo'` answers carry `left` and `right` instead. Both carry `channels`, `sampleRate` (of the visualization stream) and `channelCount`.
- With one channel `right` equals `left`. Channels beyond the second are ignored, not mixed in. After `audio.setChannelMode` with `'mono'` the stream has one channel.
- Invalid `duration`, `channels` or `points` fail with `INVALID_PARAMS`. A window longer than the audio buffered for output fails with `No waveform data available`, as does calling it without a spectrum subscription.

```js
const { waveform } = await fb2k.invoke('audio.getWaveform', { duration: 0.1 });

// Two channels, 256 evenly spaced samples each (a stereo field display)
const { left, right } = await fb2k.invoke('audio.getWaveform', {
  duration: 0.05,
  signed: true,
  channels: 'stereo',
  points: 256,
});
```

### audio.isVisualizationAvailable


_No parameters._

**Returns**: `{"available":"...","success":true}`

```js
const result = await fb2k.invoke('audio.isVisualizationAvailable');
```

### audio.setChannelMode


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `mode` | `string` | No | `default` | One of `default`, `mono`, `front`, `back`; any other value maps to `default`. |

**Returns**: `{"mode":"...","success":true}`

```js
await fb2k.invoke('audio.setChannelMode', { mode: 'mono' });
```

### audio.subscribeSpectrum


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `subscriptionId` | `string` | No | — | Omit to reuse the per-window legacy token derived from `event`. Subscribing again with the same id replaces that subscription. |
| `fftSize` | `integer` | No | `1024` | Must be a power of two between 256 and 65536. 65536 is this component's ceiling, not a foobar2000 limit, and costs about 1.5 s of PCM per frame. |
| `bands` | `integer` | No | `48` | Clamped to 8-`fftSize / 2`. |
| `fps` | `integer` | No | `30` | Clamped to 1-60. |
| `scale` | `string` | No | `weighted` | `weighted`: display curve in `[0, 1]`. `db`: band power in dB, summed over the FFT bins of the band, where a full-scale sine reads 0 dB. |
| `backgroundThrottle` | `boolean` | No | `true` | `true` limits this subscription to at most 12 frames per second, usually 10 to 12, while another application is in the foreground; `false` keeps the full rate. |
| `minFrequency` | `number` | No | `20` | Lower edge of the band range in Hz; bands divide the range logarithmically. A finite number of at least 1. |
| `maxFrequency` | `number` | No | `sampleRate / 2` | Upper edge in Hz, greater than `minFrequency`. Capped at half the stream's sample rate when frames are computed; follows it when omitted. A range entirely above that frequency yields silence frames. |
| `event` | `string` | No | `audio:spectrum` | Event name the spectrum data is delivered on. |

**Returns**: `{"backgroundThrottle":"...","bands":"...","code":"...","details":"...","error":"...","event":"...","fftSize":"...","fps":"...","maxFrequency":"...","minFrequency":"...","scale":"...","streamReady":"...","subscriptionId":"...","success":true}`

Each subscription keeps its own parameters: another subscription with a larger FFT size, band count or frame rate does not change the frames this one receives. A registered subscription always answers `success: true`; `streamReady` is `true` once the visualization stream exists, which includes the stopped state. An `fftSize` that is not a power of two in range, a `scale` other than `weighted` / `db`, a non-boolean `backgroundThrottle`, a `minFrequency` below 1 or a `maxFrequency` not above `minFrequency` registers nothing and answers `success: false` with `code: "INVALID_PARAMS"` and `details: { param, value }`.

Each frame is delivered to the subscribing window only, under the subscription's `event`:

| Field | Description |
| --- | --- |
| `subscriptionId` | Subscription the frame belongs to. Several subscriptions on the same event name each receive their own frames, so filter on this field. |
| `spectrum` | One value per band in the subscription's `scale`. `weighted` values lie in `[0, 1]`; `db` values have a floor of `-160`. |
| `bands` | Length of `spectrum`. |
| `fftSize` | FFT size actually used. Band counts of 32 and 64 or more raise it to at least 4096 and 8192. |
| `scale` | `weighted` or `db`. |
| `sampleRate` | Sample rate of the visualization stream in Hz; `0` when unknown. |
| `minFrequency`, `maxFrequency` | Frequency range the bands divide logarithmically: the subscription's `minFrequency` (20 Hz by default) to its `maxFrequency` capped at `sampleRate / 2`, or `sampleRate / 2` when none was given. The answer to the subscription echoes the requested values, with `maxFrequency: null` when it was omitted. |
| `state` | `playing`, `paused` or `stopped`. When playback pauses or stops, each subscription receives one silence frame with the new state and then no frames until playback resumes. |
| `streamTime` | Seconds since the visualization stream last started. Starting playback, seeking and manual track changes restart it at `0`, where it stays for about 200 ms while repeating the same audio; natural track transitions do not restart it. |
| `hostTime` | Host system time when the frame was computed, Unix epoch milliseconds. `Date.now() - frame.hostTime` measures delivery latency. |

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `event` | `string` | No | `audio:stream` | Event name the stream data is delivered on. |
| `interval` | `number` | No | `0.05` | Interval in seconds. |

**Returns**: `{"error":"...","event":"...","interval":"...","success":true}`

```js
const result = await fb2k.invoke('audio.subscribeStream', { interval: 0.1 });
```

### audio.unsubscribeSpectrum


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Omit to remove every spectrum subscription owned by the calling window; a caller without a window removes every subscription of every caller. |

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `guid` | `string` | Yes | — | GUID of an installed DSP, as reported by `dsp.getAvailable`. |
| `position` | `integer` | No | `-1` | `-1` appends to the end. |

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
| `index` | `integer` | No | Preset index as reported by `dsp.getPresets`; wins when both are present. |
| `name` | `string` | No | Preset name as reported by `dsp.getPresets`. |

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


| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `paths` | `array` | Yes | — | File path list to scan. |
| `mode` | `string` | No | `track` | Accepts `track` or `album`. |

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
| `sourceMode` | `string` | No | Accepts `none`, `track`, `album`, `auto` (alias `byPlaybackOrder`). |
| `processingMode` | `string` | No | Accepts `none`, `gain`, `gain_and_peak`, `peak`. |

**Returns**: `{"changed":"...","error":"...","processingMode":"...","sourceMode":"...","success":true}`

```js
const result = await fb2k.invoke('replaygain.setMode', { sourceMode: 'album' });
```

### replaygain.setPreamp


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `withRg` | `number` | No | Preamp in dB for tracks with ReplayGain info; clamped to -24..+24. |
| `withoutRg` | `number` | No | Preamp in dB for tracks without ReplayGain info; clamped to -24..+24. |

**Returns**: `{"changed":"...","error":"...","success":true,"withRg":"...","withoutRg":"..."}`

```js
const result = await fb2k.invoke('replaygain.setPreamp', { withRg: -3 });
```

## Runtime behavior notes

- `audio.subscribeSpectrum` creates or updates a caller-owned subscription. Omit `subscriptionId` to use the runtime's caller-scoped legacy identifier; listen for the configured `event`, which defaults to `audio:spectrum`.
- Spectrum subscriptions do not share parameters. A beat thread on a high-resolution timer ticks at the highest requested `fps`, and each subscription gets a frame when its own interval is due, so pushed frames arrive close to the requested rate; subscriptions with the same effective FFT size, band count, `scale` and frequency range share one FFT per tick. Host main-thread time grows with the number of frames, about 0.5 ms each.
- The `db` scale reflects the signal after ReplayGain and DSP processing and before the volume control, so changing the volume does not change the readings.
- The SDK convenience call `fb.audio.subscribeSpectrum(` configures the same underlying subscription. The unimplemented stream-capture stub keeps its default event token `audio:stream`.
- `audio.getSpectrum` and `audio.getWaveform` consume the visualization stream. They return an error until a spectrum subscription exists and audio data is available.
- `audio.generateWaveform` currently returns file metadata plus a failure explaining that decoder-backed waveform generation is not implemented. Use `audio.generateFullWaveform` for the asynchronous cache-backed workflow.
- `audio.generateFullWaveform` returns `status: "ready"` with cached data or `status: "pending"` with `taskId`. The caller receives `audio:fullWaveformReady` or `audio:fullWaveformFailed`; `cueIndex`, when non-negative, takes precedence over a `path|subsong:N` suffix.
- `audio.subscribeStream` is a capability stub: it returns `success: false` until `playback_stream_capture` is integrated. `audio.unsubscribeStream` remains safe to call.
- DSP registrations are present in every build. When the foobar2000 DSP SDK surface is unavailable, all `dsp.*` methods return the runtime's "DSP API not available in this build" failure instead of emulating a chain.
- `output.getSettings` is read-only discovery information. Output configuration is managed by foobar2000 Preferences rather than this API.
- `replaygain.get` reads each supplied media path; `replaygain.clear` writes ReplayGain metadata asynchronously through foobar2000. `replaygain.scan` requests the host scanner and is not a synchronous analysis result.
