# fb.audio Audio Analysis

## subscribeSpectrum(callback, options?)

Subscribes to real-time spectrum data. The SDK starts the host spectrum pipeline automatically and returns an unsubscribe function that also stops the host-side subscription.

| Parameter | Type | Description |
| --- | --- | --- |
| callback | function | Receives this subscription's frames only, typed `AudioSpectrumPayload`: `spectrum` plus the frame fields listed under [`audio.subscribeSpectrum`](../api/audio.md#audio-subscribespectrum) (`subscriptionId`, `bands`, `fftSize`, `scale`, `sampleRate`, `state`, `streamTime`, `hostTime` and more). Older hosts send `spectrum` only |
| options.fftSize | number | FFT size; a power of two from 256 to 65536. Defaults to 1024. 65536 is this component's ceiling and is only worth it when you need raw bin resolution |
| options.bands | number | Number of output frequency bands. Defaults to 48 |
| options.fps | number | Refresh rate from 1 to 60 FPS. Defaults to 30 |
| options.scale | `'weighted' \| 'db'` | `'weighted'` (default) is a display curve in `[0, 1]`; `'db'` is band power in dB where a full-scale sine reads 0 dB |
| options.backgroundThrottle | boolean | Defaults to `true`, which limits the subscription to at most 12 FPS, usually 10 to 12, while another application is in the foreground. Pass `false` to keep the full rate |
| options.minFrequency | number | Lower edge of the band range in Hz, at least 1. Defaults to 20 |
| options.maxFrequency | number | Upper edge in Hz, greater than `minFrequency`; capped at half the stream's sample rate, which is also the default |
| options.event | string | Custom event name. Defaults to `audio:spectrum`; distinct names can isolate data across panels |

The returned function has a `ready` property: a promise that settles with the host's answer and never rejects. It resolves to `{ ok: true, subscriptionId, fftSize, bands, fps, scale, backgroundThrottle, minFrequency, maxFrequency, streamReady }` once the subscription is registered (`maxFrequency` is `null` when the range follows half the sample rate), or to `{ ok: false, code, error }`: `code` is `INVALID_PARAMS` for rejected options, `NOT_SUPPORTED` without a host, and `UNKNOWN_ERROR` when the call itself failed.

```javascript
// Basic usage
fb.audio.subscribeSpectrum((data) => {
    renderVisualizer(data.spectrum);
});

// Custom options
const unsubscribe = fb.audio.subscribeSpectrum(
    (data) => renderVisualizer(data.spectrum),
    { fftSize: 2048, bands: 96, fps: 60 }
);

// Custom event name
const unsubscribeCustom = fb.audio.subscribeSpectrum(
    (data) => renderVisualizer(data.spectrum),
    { fftSize: 1024, bands: 48, fps: 30, event: 'audio:panelSpectrum' }
);

// Band power in dB at the full frame rate; check the registration
const meter = fb.audio.subscribeSpectrum(
    (frame) => drawMeter(frame.spectrum, frame.state),
    { bands: 64, fftSize: 8192, scale: 'db', backgroundThrottle: false }
);
const outcome = await meter.ready;
if (!outcome.ok) console.warn('spectrum subscription failed', outcome.code, outcome.error);

// Detach listeners and release host visualization resources
unsubscribe();
unsubscribeCustom();
```

::: warning Important
The returned function calls `audio.unsubscribeSpectrum` to release host visualization resources. Always call it when spectrum data is no longer needed.
:::

::: tip Low-level control
`fb.audio.subscribeSpectrum()` generates and manages its own `subscriptionId`; read it from `ready` when you need it, for example to poll that subscription with `getSpectrum`. Use low-level `fb2k.invoke('audio.subscribeSpectrum', ...)` when you need to choose the subscription ID yourself, replace a same-ID subscription, or run caller-level diagnostics.
:::

## getSpectrum(options?)

Polls the current spectrum buffer once. An active `subscribeSpectrum` host subscription is still required. Resolves with `AudioGetSpectrumResponse`: one frame plus `success`, a silence frame while playback is paused or stopped, or `success: false` when no data is available yet. To draw in step with the display, pass the `subscriptionId` from `ready` and call this from `requestAnimationFrame`, one call at a time and no more often than the frame rate you need; see [`audio.getSpectrum`](../api/audio.md#audio-getspectrum).

| Parameter | Type | Description |
| --- | --- | --- |
| options.subscriptionId | string | Compute the frame with this subscription's parameters (`bands`, `scale` and the frequency range are then ignored) |
| options.bands | number | Requested band count for this poll. When omitted or `0`, the largest band count among the subscriptions is used |
| options.scale | `'weighted' \| 'db'` | Scale for this poll when no `subscriptionId` is given. Defaults to `'weighted'` |
| options.minFrequency / options.maxFrequency | number | Band range for this poll when no `subscriptionId` is given, as in `subscribeSpectrum` |

```javascript
// Start the spectrum pipeline first
const unsubscribe = fb.audio.subscribeSpectrum(() => {}, { bands: 96 });

// Poll the current spectrum once
const result = await fb.audio.getSpectrum({ bands: 96 });
console.log(result.spectrum);
console.log(result.bands);

unsubscribe();
```

## getWaveform(options?)

Returns a short waveform window from the current playback stream. Call `subscribeSpectrum` first to start the visualization pipeline.

| Parameter | Type | Description |
| --- | --- | --- |
| options.duration | number | Window duration in seconds, greater than 0 and at most 1. Defaults to `0.05` |
| options.signed | boolean | Preserve signed PCM polarity, clamped to `[-1, 1]`. Defaults to `false` |
| options.channels | `'mix' \| 'stereo'` | `'mix'` (default) returns one averaged `waveform`; `'stereo'` returns the first two channels as `left` and `right`, with `right` equal to `left` on a mono stream |
| options.points | number | Thin the window to this many evenly spaced samples, without averaging; an integer in `[2, 65536]`. Every sample is returned when omitted |

The answer also carries `channels`, `sampleRate` and `channelCount` of the visualization stream. Invalid options fail with `code: 'INVALID_PARAMS'`.

::: warning Important
This method reads a real-time window from the **current playback stream**, not an offline file waveform. Use `generateFullWaveform` for a complete file waveform. The legacy `(path, options)` overload ignores `path`.
:::

```javascript
// Subscribe first to start the visualization pipeline
const unsubscribe = fb.audio.subscribeSpectrum(() => {});

// Get a 0.1-second waveform window
const result = await fb.audio.getWaveform({ duration: 0.1 });
console.log('Waveform:', result.waveform);

// Both channels, 256 samples each
const { left, right } = await fb.audio.getWaveform({
  duration: 0.05,
  signed: true,
  channels: 'stereo',
  points: 256,
});

unsubscribe();
```

## generateFullWaveform(path, options?)

Generates a complete file waveform with background decoding, caching, and asynchronous completion events. It is suitable for seek-bar overviews, waveform cards, and chapter previews.

| Parameter | Type | Description |
| --- | --- | --- |
| path | string | Audio file path; supports the `path\|subsong:N` form |
| options.resolution | number | Number of waveform points, from 64 to 4096. Defaults to 256 |
| options.method | `'peak' \| 'rms'` | Aggregation method. Defaults to `'rms'` |
| options.scale | `'linear' \| 'db'` | Amplitude scale. Defaults to `'linear'` |
| options.signed | boolean | Preserve signed polarity. Defaults to `false` |
| options.preferCache | boolean | Return a cached result when available. Defaults to `true` |
| options.cueIndex | number | Explicit CUE subsong index; takes precedence over `path\|subsong:N` |
| options.timeout | number | SDK wait timeout in milliseconds for a pending task. Defaults to 60000; values `<= 0` disable the timeout |
| options.signal | AbortSignal | Ends the wait early; see below |

> The SDK forwards generation options to `audio.generateFullWaveform`, except `timeout` and `signal`, which the SDK consumes. The host parses `path|subsong:N`, canonicalizes the path, and attempts a direct file read when cached metadata lacks the required technical fields.

**Returns:** A promise for `FullWaveformResult`. A cache hit resolves immediately with `status: 'ready'`; on a cache miss, the SDK waits for the matching completion event. `result.maxAmplitude` is the largest value of the selected sequence before normalization, in linear full-scale units: on the `linear` scale `waveform[i] * maxAmplitude` restores the level, on `db` the dBFS value is `(v * 60 - 60) + 20 * log10(maxAmplitude)`.

**Ending the wait early:** an aborted `signal` rejects with a `DOMException` named `AbortError`, the timeout rejects with `{ success: false, error: 'TIMEOUT' }`, and both cancel the host task through `cancelFullWaveform`; a queued decode is dropped and a running one gives up its decode slot as soon as the decoder notices. A signal that is already aborted rejects without calling the host. A cache hit resolves whatever the signal does.

**Failures:** a request the host refuses up front (missing or invalid path, permission, shutdown) resolves with `{ success: false, error, code }`, so check `success`; a decode that fails later rejects with the `audio:fullWaveformFailed` payload; a bridge-level error rejects with an `Error`.

```javascript
// Basic usage; resolves immediately on a cache hit
const result = await fb.audio.generateFullWaveform('E:\\Music\\song.flac', {
    resolution: 256,
    method: 'peak'
});

console.log('Waveform:', result.waveform);
console.log('Duration:', result.duration);
console.log('From cache:', result.cached);

// Subsong path form
const result2 = await fb.audio.generateFullWaveform('E:\\Music\\disc.flac|subsong:2', {
    resolution: 512
});

// cueIndex takes precedence over a subsong suffix
const result3 = await fb.audio.generateFullWaveform('E:\\Music\\album.cue', {
    cueIndex: 3,
    resolution: 256
});

// RMS mode produces a smoother energy envelope
const result4 = await fb.audio.generateFullWaveform('E:\\Music\\song.flac', {
    resolution: 512,
    method: 'rms'
});

// Drop the request when the user moves on to another track
const controller = new AbortController();
fb.audio.generateFullWaveform('E:\\Music\\song.flac', { signal: controller.signal })
    .catch((err) => {
        if (err.name !== 'AbortError') throw err;
    });
controller.abort();
```

**Aggregation methods:**

- `peak`: Uses the largest absolute sample across channels in each window; suitable for waveform overviews and seek bars.
- `rms`: Uses the root mean square across each window to produce a smoother energy envelope.

**Cache behavior:**

- The cache key includes the canonical path, subsong, resolution, file size, and modification time. An entry holds the raw values of one decode, so every `method` / `signed` / `scale` is computed from it.
- At most 50 entries are retained using LRU eviction.
- Changing the file invalidates its cached waveform.

**Queue behavior:**

- On a cache miss, requests for the same track and resolution that are already in flight share one decode; each gets its own result.
- The host decodes two tracks at a time and queues the rest in submission order. Time spent in the queue counts toward `timeout`, so cancel requests you no longer need.

::: tip Asynchronous completion
The SDK handles both paths automatically:
- Cache hit: resolves immediately.
- Cache miss: waits for `audio:fullWaveformReady` or rejects on `audio:fullWaveformFailed` or timeout.

Callers can simply `await` the method and do not need to subscribe to these events manually.
:::

## cancelFullWaveform(taskId)

Cancels a pending `generateFullWaveform` request by the `taskId` of its `pending` answer. Only the caller that made the request can cancel it. `generateFullWaveform` already calls this on `signal` abort and on timeout; call it directly when you hold a `taskId` from `fb2k.invoke`.

| Parameter | Type | Description |
| --- | --- | --- |
| taskId | string | The `taskId` of a `pending` answer |

**Returns:** A promise for `{ success: true, cancelled: boolean }`. `cancelled: false` means the task has already finished, does not exist, or belongs to another caller. A cancelled task receives one `audio:fullWaveformFailed` with `code: 'CANCELLED'`, which may arrive before this answer. Hosts before 1.14 do not have this endpoint and reject the call with `Method not found`.

```javascript
const pending = await fb2k.invoke('audio.generateFullWaveform', { path: 'E:\\Music\\song.flac' });
if (pending.status === 'pending') {
    const { cancelled } = await fb.audio.cancelFullWaveform(pending.taskId);
}
```

## analyzeBPM(path, options?)

Analyzes a file's BPM and returns `{ bpm }`.

| Parameter | Type | Description |
| --- | --- | --- |
| path | string | Audio file path |
| options.forceAnalysis | boolean | Force analysis instead of using an existing BPM tag. Defaults to `false` |

```javascript
const result = await fb.audio.analyzeBPM('E:\\Music\\song.flac');
console.log(`BPM: ${result.bpm}`);

// Force a fresh analysis
const result2 = await fb.audio.analyzeBPM('E:\\Music\\song.flac', { forceAnalysis: true });
```

## setChannelMode(mode)

Sets the channel mode. An unsupported `mode` is normalized to `'default'`.

| Parameter | Type | Description |
| --- | --- | --- |
| mode | string | `'default'`, `'mono'`, `'front'`, or `'back'` |

```javascript
await fb.audio.setChannelMode('mono');
// Unsupported values fall back to the default mode
const result = await fb.audio.setChannelMode('invalid'); // result.mode === "default"
```

## getSpectrumDebugState()

Returns internal spectrum diagnostics, including active subscriptions with their `scale`, `backgroundThrottle` and frequency range, dispatch targets, the largest requested FFT/FPS/band settings, `framesComputed`, stream readiness, and the state of the beat thread that pushes frames (`timerRunning`, `beatSource`, `beatIntervalMs`, `beatsCoalesced`).

```javascript
const debug = await fb.audio.getSpectrumDebugState();
console.log(debug.subscriptions, debug.effectiveFps);
```

## Additional Methods

### getOutputInfo()

Signature: `fb.audio.getOutputInfo(): Promise<AudioOutputInfoResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the current native volume in dB and `volumePercent` on a 0-100 scale.

```javascript
const output = await fb.audio.getOutputInfo();
console.log(output.volume, output.volumePercent);
```

### getStreamInfo()

Signature: `fb.audio.getStreamInfo(): Promise<AudioStreamInfoResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns `{ playing }`. While playback is active, it also includes `sampleRate`, `channels`, `bitrate`, `codec`, and `duration` when available.

```javascript
const stream = await fb.audio.getStreamInfo();
console.log(stream.channels, stream.sampleRate);
```

### isVisualizationAvailable()

Signature: `fb.audio.isVisualizationAvailable(): Promise<{ available: boolean }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns whether the host can provide visualization data.

```javascript
const { available } = await fb.audio.isVisualizationAvailable();
```

### subscribeStream(callback, options?)

Signature: `fb.audio.subscribeStream(callback: StreamCallback, options?: AudioSubscribeStreamParams): () => void`

Starts the deprecated raw-stream subscription and returns an unsubscribe function. The current host rejects `audio.subscribeStream`, so the callback cannot fire until host stream-capture integration is implemented.

```javascript
const unsubscribe = fb.audio.subscribeStream((chunk) => {
    console.log(chunk);
});
unsubscribe();
```

### generateWaveform(path, options?)

Signature: `fb.audio.generateWaveform(path: string, options?: Omit<AudioGenerateWaveformParams, 'path'>): Promise<AudioGenerateWaveformResponse>`

Deprecated alias for the legacy `audio.generateWaveform` endpoint. Use `fb.audio.generateFullWaveform()` for full-track waveform generation.

```javascript
const result = await fb.audio.generateWaveform('C:\\Music\\song.flac');
```

### unsubscribeStream()

Signature: `fb.audio.unsubscribeStream(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| - | - | - | No parameters |

Returns the result of stopping the raw audio-stream subscription.

::: warning Deprecated and not implemented by the host
`subscribeStream()` and `unsubscribeStream()` are deprecated. The current host returns `success: false` from `audio.subscribeStream` and never emits `audio:stream`, so a subscription callback cannot fire until host stream-capture integration is implemented.
:::

```javascript
await fb.audio.unsubscribeStream();
```
