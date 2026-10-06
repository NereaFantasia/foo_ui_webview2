# fb.audio Audio Analysis

## subscribeSpectrum(callback, options?)

Subscribes to real-time spectrum data. The SDK starts the host spectrum pipeline automatically and returns an unsubscribe function that also stops the host-side subscription.

| Parameter | Type | Description |
| --- | --- | --- |
| callback | function | Receives this subscription's frames only. Band output passes `AudioSpectrumPayload`: `spectrum` plus the frame fields listed under [`audio.subscribeSpectrum`](../api/audio.md#audio-subscribespectrum) (`subscriptionId`, `bands`, `fftSize`, `scale`, `sampleRate`, `state`, `streamTime`, `hostTime` and more); older hosts send `spectrum` only. With `output: 'bins'` it passes `SpectrumBinsFrame`: `firstBin` plus `spectrum` for `channels: 'mix'`, or `left` and `right` for `'stereo'` |
| options.fftSize | number | FFT size; a power of two from 256 to 65536. Defaults to 1024. 65536 is this component's ceiling and is only worth it when you need raw bin resolution. Bin output uses it as given; band output raises it for 32 or more bands |
| options.bands | number | Number of frequency bands for band output. Defaults to 48 |
| options.fps | number | Refresh rate from 1 to 60 FPS. Defaults to 30 |
| options.scale | `'weighted' \| 'db'` | `'weighted'` (default) is a display curve in `[0, 1]`; `'db'` is band power in dB where a full-scale sine reads 0 dB. Bin output is always in dB: omit it or pass `'db'` |
| options.output | `'bands' \| 'bins'` | `'bins'` delivers the FFT's linear bins as power in dB instead of bands; see [Bin output](../api/audio.md#bin-output). `'bands'` (default) is kept for compatibility |
| options.channels | `'mix' \| 'stereo'` | Bin output only: `'mix'` (default) averages the channels into `spectrum`, `'stereo'` delivers `left` and `right` |
| options.backgroundThrottle | boolean | Defaults to `true`, which limits the subscription to at most 12 FPS, usually 10 to 12, while another application is in the foreground. Pass `false` to keep the full rate |
| options.minFrequency | number | Lower edge of the band range in Hz, at least 1. Defaults to 20 |
| options.maxFrequency | number | Upper edge in Hz, greater than `minFrequency`; capped at half the stream's sample rate, which is also the default |
| options.event | string | Custom event name. Defaults to `audio:spectrum`; distinct names can isolate data across panels |

The returned function has a `ready` property: a promise that settles with the host's answer and never rejects. It resolves to `{ ok: true, subscriptionId, fftSize, bands, fps, scale, output, channels, backgroundThrottle, minFrequency, maxFrequency, streamReady }` once the subscription is registered (`maxFrequency` is `null` when the range follows half the sample rate; a host without bin output reports `output: 'bands'` for a bin request, and the callback then receives nothing), or to `{ ok: false, code, error }`: `code` is `INVALID_PARAMS` for rejected options, `NOT_SUPPORTED` without a host, and `UNKNOWN_ERROR` when the call itself failed.

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

// Linear bins in dB for the left and right channels
const bins = fb.audio.subscribeSpectrum(
    (frame) => {
        if (frame.channels === 'stereo') console.log(frame.firstBin, frame.left.length, frame.right.length);
    },
    { output: 'bins', channels: 'stereo', fftSize: 16384, backgroundThrottle: false }
);
const binsOutcome = await bins.ready;
if (binsOutcome.ok && binsOutcome.output !== 'bins') console.warn('this host has no bin output');

// Detach listeners and release host visualization resources
unsubscribe();
unsubscribeCustom();
bins();
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
| options.subscriptionId | string | Compute the frame with this subscription's parameters, bin output included (`bands`, `scale`, `output`, `channels` and the frequency range are then ignored) |
| options.bands | number | Requested band count for this poll. When omitted or `0`, the largest band count among the subscriptions is used |
| options.scale | `'weighted' \| 'db'` | Scale for this poll when no `subscriptionId` is given. Defaults to `'weighted'` |
| options.minFrequency / options.maxFrequency | number | Band range for this poll when no `subscriptionId` is given, as in `subscribeSpectrum` |
| options.output / options.channels | string | Without `subscriptionId` only `'bands'` and `'mix'` are accepted; bin output needs a subscription of its own |

```javascript
// Start the spectrum pipeline first
const unsubscribe = fb.audio.subscribeSpectrum(() => {}, { bands: 96 });

// Poll the current spectrum once
const result = await fb.audio.getSpectrum({ bands: 96 });
if (result.success === false) throw new Error(result.error);
console.log(result.spectrum);
console.log(result.bands);

unsubscribe();
```

## getWaveform(options?)

Returns a short waveform window from the current playback stream. Call `subscribeSpectrum` first to start the visualization pipeline.

| Parameter | Type | Description |
| --- | --- | --- |
| options.duration | number | Window duration in seconds, greater than 0 and at most 1: the most recent `duration` seconds, ending at the visualization stream's current time. Defaults to `0.05` |
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
if (result.success === false) throw new Error(result.error);
console.log('Waveform:', result.waveform);

// Both channels, 256 samples each
const res = await fb.audio.getWaveform({
  duration: 0.05,
  signed: true,
  channels: 'stereo',
  points: 256,
});
if (res.success === false) throw new Error(res.error);
const { left, right } = res;

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
if (pending.success === false) throw new Error(pending.error);
if (pending.status === 'pending') {
    const res = await fb.audio.cancelFullWaveform(pending.taskId);
    if (res.success === false) throw new Error(res.error);
    const { cancelled } = res;
}
```

## decodePcm(path, options?)

Experimental API; it may change in future releases.

Decodes a track, or a range of it, into float32 PCM and resolves with a `PcmBuffer` that reads the samples in place, the way Web Audio's `decodeAudioData` hands back an `AudioBuffer`. The host posts the samples as a shared buffer; the SDK pairs it with the `audio:pcmReady` event, so do not handle `sharedbufferreceived` for `audio.decodePcm` yourself on a page that uses this method.

| Parameter | Type | Description |
| --- | --- | --- |
| path | string | Track path; a `\|subsong:N` suffix selects a subsong |
| options.cueIndex | number | Cue subsong index, 0-based; takes precedence over a subsong in `path` |
| options.start | number | Start of the range in seconds; defaults to `0` |
| options.end | number | End of the range in seconds; defaults to the end of the track and is cut to the track length |
| options.sampleRate | number | Sample rate to convert to, 8000 to 192000; defaults to the source rate |
| options.mono | boolean | Average all channels into one |
| options.signal | AbortSignal | Aborting cancels the host task and rejects with a `DOMException` named `AbortError` |
| options.timeoutMs | number | Wait limit in milliseconds, queueing included; defaults to `120000`, `0` waits forever |

**Returns:** a promise for a `PcmBuffer`:

| Member | Description |
| --- | --- |
| `sampleRate`, `channels`, `frames` | Format of the samples; `channels` is 1 with `mono` |
| `duration` | `frames / sampleRate`, in seconds |
| `start` | Track time of the first frame, in seconds |
| `truncated` | The audio ran past the size estimated from the track length, or its format changed partway; the samples end at the cut |
| `resampled` | Converted to `sampleRate` with foobar2000's resampler |
| `getChannelView(c)` | Zero-copy `Float32Array` of channel `c`, `frames` samples long |
| `toAudioBuffer()` | Copies the samples into a new `AudioBuffer` |
| `transfer()` | Hands over the underlying `ArrayBuffer`, for `worker.postMessage(buffer, [buffer])` |
| `release()` | Frees the shared memory; safe to call twice |
| `available` | `false` after `release()` or `transfer()` |

::: danger Read-only memory
`getChannelView()` returns views of read-only shared memory. Writing to them crashes the renderer process, which takes the whole foobar2000 window with it. The method is deliberately not called `getChannelData`: Web Audio's method of that name returns an array you may modify. Use `toAudioBuffer()` when you need a copy you can change.
:::

Call `release()` when you are done. After `transfer()` the `PcmBuffer` gives out no more views and `release()` does nothing; views you took earlier keep working until the buffer is actually transferred with `postMessage`, and are detached (`byteLength` 0) from then on. A worker cannot release the buffer, so to free it early, transfer it back to the page and pass it to `chrome.webview.releaseBuffer`. The buffer still starts with its header, which `readPcmHeader()` from the SDK reads.

**Failures** reject with a `PcmDecodeError`, whose `code` is the host's: `INVALID_PARAMS` (also for a range too large for one buffer: 256 MiB in 64-bit foobar2000, 64 MiB in 32-bit), `INVALID_PATH`, `NO_INFO`, `NOT_SUPPORTED` (outside a WebView2 host, on a runtime without shared buffers, or with no resampler for the conversion), `CANCELLED`, `DECODE_FAILED` and the other codes listed under [`audio.decodePcm`](../api/audio.md#audio-decodepcm). A timeout rejects with code `TIMEOUT`. Both an abort and a timeout cancel the host task.

One task decodes at a time and the rest queue; the queue time counts toward `timeoutMs`. Identical requests in flight share one decode.

```javascript
const pcm = await fb.audio.decodePcm('E:\\Music\\song.flac', {
    start: 30,
    end: 60,
    sampleRate: 22050,
    mono: true,
});
try {
    const samples = pcm.getChannelView(0);
    let sum = 0;
    for (const s of samples) sum += s * s;
    console.log('RMS', Math.sqrt(sum / pcm.frames));
} finally {
    pcm.release();
}
```

## cancelDecodePcm(taskId)

Experimental API; it may change in future releases.

Cancels a pending `audio.decodePcm` task by its `taskId`. `decodePcm()` already calls this on abort and on timeout; call it directly only for tasks started with `fb2k.invoke`. `cancelled: false` means the task has already settled, does not exist, or belongs to another page. A cancelled task receives one `audio:pcmFailed` with `code: 'CANCELLED'`.

```javascript
const pending = await fb2k.invoke('audio.decodePcm', { path: 'E:\\Music\\song.flac' });
if (pending.success === false) throw new Error(pending.error);
const res = await fb.audio.cancelDecodePcm(pending.taskId);
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

## subscribeStream(options?)

Experimental API; it may change in future releases.

Signature: `fb.audio.subscribeStream(options?: PcmStreamOptions): PcmStream`

Subscribes to the audio foobar2000 is playing and returns a `PcmStream` handle at once. The host copies every chunk it plays into a ring buffer shared with the page; `read()` copies the frames written since the previous call out of it, one `Float32Array` per channel, together with `dropped`, the number of frames the ring overwrote before they were read. Poll it from `requestAnimationFrame` or a timer: the host cannot notify the page when frames arrive.

- The samples are what the DSP chain outputs, before ReplayGain and the volume control; unlike `audio:spectrum` they do not include ReplayGain.
- `ready` resolves with the host's answer and never rejects: `{ ok: true, subscriptionId }`, or `{ ok: false, code, error }` with `NOT_SUPPORTED` outside a WebView2 host or on a runtime without shared buffers, `OPERATION_FAILED` when the page already has 8 stream subscriptions, `INVALID_PARAMS` for values out of range.
- `format` is `null` until the first chunk of audio arrives, then `{ sampleRate, channels, capacityFrames, epoch }`; `onFormat(listener)` fires for the first buffer and again whenever the sample rate or channel count changes. Frames of the previous format that were not read before the change are discarded with its buffer and counted in the `dropped` of the next `read()` that returns frames.
- `read()` returns `null` while nothing new was written (playback stopped or paused), before the first buffer, and after the stream ended.
- The result also carries `segments`: the frames cut where media time restarts (a start, seek or track change), each run as `{ offset, segment, startSeconds, reason, estimated }` with `offset` its first index in the planes. A host that writes version-1 buffers has no segment table, so there is always one run with `segment: null`.
- `ended` turns `true`, and `onEnded(listener)` fires once, after `unsubscribe()`, when the host refused the subscription, or when the host stopped writing and no new buffer followed within a second.
- `options.interval` asks the core for a callback interval in seconds (0.01 to 0.2). The core rounds it to about 16 ms steps, defaults to 200 ms, and applies the shortest interval any stream subscription in the component asked for. `options.bufferSeconds` (0.1 to 10, default 1) sizes the ring; the first chunk after playback starts can hold up to 0.8 s of audio.
- Call `unsubscribe()` when done: it removes the subscription on the host and releases the page's view of the shared memory.

```javascript
const stream = fb.audio.subscribeStream({ bufferSeconds: 0.5 });
const outcome = await stream.ready;
if (outcome.ok === false) throw new Error(outcome.code);
const tick = () => {
    const chunk = stream.read();
    if (chunk) {
        const left = chunk.planes[0];
        let peak = 0;
        for (const sample of left) peak = Math.max(peak, Math.abs(sample));
        console.log(stream.format?.sampleRate, chunk.frames, chunk.dropped, peak);
    }
    if (!stream.ended) requestAnimationFrame(tick);
};
requestAnimationFrame(tick);
// later
stream.unsubscribe();
```

### Computing a spectrum yourself

Bin output uses foobar2000's FFT, whose Gaussian window is fixed. To choose the window function, zero-pad, combine several FFT sizes or use another transform, compute the spectrum from the live PCM stream:

- Ask for the shortest interval, `interval: 0.016`; at the 200 ms default the spectrum would change five times a second. The interval applies to every stream subscription in the component.
- Chunks then arrive every 16 ms on average but unevenly (3 to 50 ms apart in measurements), so some display frames see no new samples; draw the previous spectrum again or smooth over time.
- The samples are taken before ReplayGain, so levels differ from `audio:spectrum` by the ReplayGain gain. The stream trails what is heard by about as much as the visualization stream does, a little over ten milliseconds in measurements, plus the wait for the next chunk.
- `read()` returns only the frames written since the previous call, so keep the last `N` samples yourself, and bring your own FFT. `segments` marks where media time restarts after a seek or track change; clear the history there if one spectrum must not mix the two.

```javascript
/**
 * A 4096-point spectrum of the left channel, redrawn on every display frame.
 * @param {(samples: Float32Array) => Float32Array} fft magnitudes from your FFT library
 * @param {(spectrum: Float32Array) => void} draw
 */
function startLiveSpectrum(fft, draw) {
    const N = 4096;
    const history = new Float32Array(N); // the last N samples, oldest first
    const hann = Float32Array.from({ length: N }, (_, i) => 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (N - 1)));
    const live = fb.audio.subscribeStream({ interval: 0.016, bufferSeconds: 1 });
    const tick = () => {
        const chunk = live.read();
        if (chunk) {
            const fresh = chunk.planes[0];
            const keep = Math.max(0, N - fresh.length);
            history.copyWithin(0, N - keep); // drop the oldest samples
            history.set(fresh.subarray(Math.max(0, fresh.length - N)), keep);
            draw(fft(history.map((s, i) => s * hann[i])));
        }
        if (!live.ended) requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
    return live;
}
```

## unsubscribeStream(subscriptionId?)

Experimental API; it may change in future releases.

Signature: `fb.audio.unsubscribeStream(subscriptionId?: string): Promise<AudioUnsubscribeStreamResponse>`

Removes this page's stream subscriptions on the host: the one with `subscriptionId`, or all of them when omitted. `PcmStream.unsubscribe()` already does this for its own subscription; call this directly only for subscriptions made with `fb2k.invoke`. Resolves with `removed`, the number of subscriptions removed; subscriptions of other pages are never counted or removed.

```javascript
const res = await fb.audio.unsubscribeStream();
if (res.success === false) throw new Error(res.error);
const { removed } = res;
```

## analyzeBPM(path, options?)

Reads the file's `BPM` tag; the host does not detect tempo. Without a tag holding a number between 0 and 500 (exclusive) it resolves `success: false` with `code: 'NOT_FOUND'`.

| Parameter | Type | Description |
| --- | --- | --- |
| path | string | Audio file path |
| options.forceAnalysis | boolean | Deprecated and ignored |

```javascript
const result = await fb.audio.analyzeBPM('E:\\Music\\song.flac');
if (result.success) {
    console.log(`BPM: ${result.bpm}`);
}
```

## setChannelMode(mode)

Chooses which channels the visualization stream carries. It applies to spectrum frames and `getWaveform`, not to what is heard. Any other `mode` fails with `INVALID_PARAMS`.

| Parameter | Type | Description |
| --- | --- | --- |
| mode | `'default' \| 'mono' \| 'front' \| 'back'` | Channels the visualization stream carries |

```javascript
const res = await fb.audio.setChannelMode('mono'); // mode === 'mono';
if (res.success === false) throw new Error(res.error);
const { mode } = res;
```

## getSpectrumDebugState()

Returns internal spectrum diagnostics, including active subscriptions with their `scale`, `backgroundThrottle` and frequency range, dispatch targets, the largest requested FFT/FPS/band settings, `framesComputed`, stream readiness, and the state of the beat thread that pushes frames (`timerRunning`, `beatSource`, `beatIntervalMs`, `beatsCoalesced`).

```javascript
const debug = await fb.audio.getSpectrumDebugState();
if (debug.success === false) throw new Error(debug.error);
console.log(debug.subscriptions, debug.effectiveFps);
```

## getPcmDebugState()

Experimental API; it may change in future releases.

Returns whether this page can receive shared buffers (`runtime.environment12`, `runtime.webview17`, the installed WebView2 version) and the state of the decode queue (`decode.active`, `decode.queued`, `decode.openBufferBytes`), for tests and diagnostics.

```javascript
const res = await fb.audio.getPcmDebugState();
if (res.success === false) throw new Error(res.error);
const { runtime, decode } = res;
console.log(runtime.version, decode.active);
```

## getOutputInfo()

Signature: `fb.audio.getOutputInfo(): Promise<AudioGetOutputInfoResponse>`

Returns the current native volume in dB and `volumePercent` on a 0-100 scale.

```javascript
const output = await fb.audio.getOutputInfo();
if (output.success === false) throw new Error(output.error);
console.log(output.volume, output.volumePercent);
```

## getStreamInfo()

Signature: `fb.audio.getStreamInfo(): Promise<AudioGetStreamInfoResponse>`

Returns `{ playing }`. While playback is active, it also includes `sampleRate`, `channels`, `bitrate`, `codec`, and `duration` when available.

```javascript
const stream = await fb.audio.getStreamInfo();
if (stream.success === false) throw new Error(stream.error);
console.log(stream.channels, stream.sampleRate);
```

## isVisualizationAvailable()

Signature: `fb.audio.isVisualizationAvailable(): Promise<AudioIsVisualizationAvailableResponse>`

Reports, as `available`, whether foobar2000 offers a visualization stream; spectrum frames and `getWaveform()` need one.

```javascript
const res = await fb.audio.isVisualizationAvailable();
if (res.success === false) throw new Error(res.error);
const { available } = res;
```
