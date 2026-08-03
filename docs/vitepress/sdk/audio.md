# fb.audio Audio Analysis

## subscribeSpectrum(callback, options?)

Subscribes to real-time spectrum data. The SDK starts the host spectrum pipeline automatically and returns an unsubscribe function that also stops the host-side subscription.

| Parameter | Type | Description |
| --- | --- | --- |
| callback | function | Receives payloads shaped as `{ spectrum: number[], fftSize?: number, bands?: number }` |
| options.fftSize | number | FFT size; a power of two from 256 to 16384. Defaults to 1024 |
| options.bands | number | Number of output frequency bands. Defaults to 48 |
| options.fps | number | Refresh rate from 1 to 60 FPS. Defaults to 30 |
| options.event | string | Custom event name. Defaults to `audio:spectrum`; distinct names can isolate data across panels |

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

// Detach listeners and release host visualization resources
unsubscribe();
unsubscribeCustom();
```

::: warning Important
The returned function calls `audio.unsubscribeSpectrum` to release host visualization resources. Always call it when spectrum data is no longer needed.
:::

::: tip Low-level control
`fb.audio.subscribeSpectrum()` generates and manages its own `subscriptionId`. Use low-level `fb2k.invoke('audio.subscribeSpectrum', ...)` when explicit subscription IDs, replacement of a same-ID subscription, or caller-level diagnostics are required.
:::

## getSpectrum(options?)

Polls the current spectrum buffer once. An active `subscribeSpectrum` host subscription is still required.

| Parameter | Type | Description |
| --- | --- | --- |
| options.bands | number | Requested band count for this poll. When omitted or `0`, the active subscription configuration is used |

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
| options.duration | number | Window duration in seconds. Defaults to `0.05` |
| options.signed | boolean | Preserve signed PCM polarity. Defaults to `false` |

::: warning Important
This method reads a real-time window from the **current playback stream**, not an offline file waveform. Use `generateFullWaveform` for a complete file waveform. The legacy `(path, options)` overload ignores `path`.
:::

```javascript
// Subscribe first to start the visualization pipeline
const unsubscribe = fb.audio.subscribeSpectrum(() => {});

// Get a 0.1-second waveform window
const result = await fb.audio.getWaveform({ duration: 0.1 });
console.log('Waveform:', result.waveform);

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

> The SDK forwards generation options to `audio.generateFullWaveform`, except `timeout`, which is consumed by the SDK. The host parses `path|subsong:N`, canonicalizes the path, and attempts a direct file read when cached metadata lacks the required technical fields.

**Returns:** A promise for `FullWaveformResult`. A cache hit resolves immediately with `status: 'ready'`; on a cache miss, the SDK waits for the matching completion event.

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
```

**Aggregation methods:**

- `peak`: Uses the largest absolute sample across channels in each window; suitable for waveform overviews and seek bars.
- `rms`: Uses the root mean square across each window to produce a smoother energy envelope.

**Cache behavior:**

- The cache key includes the path, subsong, resolution, method, file size, and modification time.
- At most 50 entries are retained using LRU eviction.
- Changing the file invalidates its cached waveform.

::: tip Asynchronous completion
The SDK handles both paths automatically:
- Cache hit: resolves immediately.
- Cache miss: waits for `audio:fullWaveformReady` or rejects on `audio:fullWaveformFailed` or timeout.

Callers can simply `await` the method and do not need to subscribe to these events manually.
:::

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

Returns internal spectrum diagnostics, including active subscriptions, dispatch targets, effective FFT/FPS/band settings, stream readiness, and timer state.

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
