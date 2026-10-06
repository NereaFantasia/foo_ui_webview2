# Audio API

Methods of the `audio` namespace.

## audio

### audio.analyzeBPM

<!-- api-schema:begin audio.analyzeBPM -->
Read the track's `BPM` tag. The host does not detect tempo. foobar2000's cached info is used when complete; otherwise the file itself is read, so tracks that are in no playlist or library work too.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix selects a subsong. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `bpm` | `number` | Value of the `BPM` tag, between 0 and 500 (exclusive); a tag outside that range fails with `NOT_FOUND`. |
| `confidence` | `number` | Always `1`. |
| `source` | `"metadata"` | Always `metadata`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The call fails with `NOT_FOUND` when the track has no `BPM` tag or its value is out of range, with `INVALID_HANDLE` or `NO_INFO` when the file cannot be opened or its info cannot be read (for example because the file does not exist), and with `OPERATION_FAILED` for any other error.

```js
const result = await fb2k.invoke('audio.analyzeBPM', {
    path: 'C:\\Music\\song.flac'
});
if (result.success === true) {
    console.log(`BPM: ${result.bpm}`);
} else if (result.code === 'NOT_FOUND') {
    console.log('No BPM tag');
}
```

### audio.cancelDecodePcm

<!-- api-schema:begin audio.cancelDecodePcm -->
Experimental API; it may change in future releases.

Cancel a pending decodePcm task started by the same page.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `taskId` | `string` | Yes | Task id from the decodePcm answer. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `cancelled` | `boolean` | `false` when the task has already ended, does not exist or belongs to another page; these cases are not told apart. A cancelled task receives one `audio:pcmFailed` with `code: 'CANCELLED'`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

A successful cancel sends that `taskId` exactly one `audio:pcmFailed` with `code: "CANCELLED"`, which may arrive before this answer, and nothing after it. The decode keeps running while an identical request still waits on it; otherwise a queued task is dropped and a running one is aborted.

```js
const pending = await fb2k.invoke('audio.decodePcm', { path: 'C:\\Music\\song.flac' });
if (pending.success === false) throw new Error(pending.error);
await fb2k.invoke('audio.cancelDecodePcm', { taskId: pending.taskId });
```

### audio.cancelFullWaveform

<!-- api-schema:begin audio.cancelFullWaveform -->
Cancel a pending generateFullWaveform request made by the same page.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `taskId` | `string` | Yes | Task id from a `pending` answer of generateFullWaveform. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `cancelled` | `boolean` | `false` when the task has already ended, does not exist or belongs to another page; these cases are not told apart. A cancelled task receives one `audio:fullWaveformFailed` with `code: 'CANCELLED'`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

A successful cancel sends that `taskId` exactly one `audio:fullWaveformFailed` with `code: "CANCELLED"`, which may arrive before this answer, and nothing after it. The decode keeps running while another request for the same track still waits on it; otherwise a queued decode is dropped and a running one is aborted.

```js
const pending = await fb2k.invoke('audio.generateFullWaveform', { path: 'C:\\Music\\song.flac' });
if (pending.success === false) throw new Error(pending.error);
if (pending.status === 'pending') {
    await fb2k.invoke('audio.cancelFullWaveform', { taskId: pending.taskId });
}
```

### audio.decodePcm

<!-- api-schema:begin audio.decodePcm -->
Experimental API; it may change in future releases.

Decode a track, or a range of it, into float32 planar PCM delivered through a shared buffer. Answers with a task id right away; the samples arrive as a `sharedbufferreceived` event on `chrome.webview` together with `audio:pcmReady`, or the task ends with `audio:pcmFailed`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Path of the track to decode; a `\|subsong:N` suffix selects a subsong. Must not be empty. |
| `cueIndex` | `integer` | No | Cue subsong index, 0-based; takes precedence over a subsong in `path`. At least `0`. |
| `start` | `number` | No | Start of the range, in seconds. At least `0`. |
| `end` | `number` | No | End of the range, in seconds; defaults to the end of the track and is cut to the track length. Must be greater than `start`. At least `0`. |
| `sampleRate` | `integer` | No | Sample rate to convert to with foobar2000's resampler; defaults to the source rate. Between `8000` and `192000` inclusive. |
| `mono` | `boolean` | No | Average all channels into one. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `taskId` | `string` | Identifies the task in events, shared-buffer metadata and cancelDecodePcm; `pcm_N`. |
| `status` | `"pending"` | Always `pending`: decoded PCM is never cached. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The samples do not travel through the JSON channel. When decoding finishes, the page receives two messages for the `taskId`, in either order:

- a `sharedbufferreceived` event on `window.chrome.webview`: `e.getBuffer()` returns the samples and `e.additionalData` is `{ purpose: 'audio.decodePcm', taskId, sampleRate, channels, frames, headerBytes: 64 }`;
- an `audio:pcmReady` event: `{ taskId, path, sampleRate, channels, frames, start, end, duration, truncated, resampled }`, where `start` and `end` are the range actually decoded, in seconds.

A task that fails sends one `audio:pcmFailed` (`{ taskId, path, code, error }`) instead. The SDK method `fb.audio.decodePcm` pairs the two messages and returns a `PcmBuffer`; a page that uses it should not also handle these buffers itself.

The buffer starts with a 64-byte little-endian header followed by float32 samples, one run of `capacityFrames` samples per channel: sample `i` of channel `c` is at byte `64 + (c × capacityFrames + i) × 4`. `capacityFrames` is the u32 at byte 24 of the header; only the first `frames` samples of each run are audio (`frames` is also the u32 at byte 32).

- The buffer is **read-only shared memory**. Writing to it crashes the page's renderer process, and with it the whole foobar2000 window. Copy the samples first, for example into an `AudioBuffer`, if you need to change them.
- Every listener of one `sharedbufferreceived` event gets the same `ArrayBuffer`. Call `chrome.webview.releaseBuffer(buffer)` once, after the last use; any access after that throws `TypeError`.
- One buffer holds at most 256 MiB in 64-bit foobar2000 and 64 MiB in 32-bit. The host estimates the size from the track length before decoding, adding one second of slack, and rejects a larger request with `INVALID_PARAMS`, `details.estimatedBytes` and `details.limitBytes`. Five minutes of 44.1 kHz stereo take about 106 MB; lower `sampleRate`, set `mono` or decode shorter ranges to stay under the limit.
- `truncated` is `true` when the audio ran past that estimate (the file is longer than its reported length) or its format changed partway, as in chained Ogg streams; the buffer holds the part before the cut.
- One task decodes at a time and the rest wait in submission order. Identical requests (same file, range, `sampleRate` and `mono`) that arrive while one is in flight share its decode; each still gets its own `taskId`, buffer and events.
- Errors known at request time come back in the answer: `INVALID_PARAMS`, `INVALID_PATH`, `INVALID_HANDLE`, `NO_INFO`, `NOT_SUPPORTED` (the WebView2 runtime cannot share buffers, or the calling page cannot be located) and `OPERATION_FAILED` (foobar2000 is shutting down). Later ones arrive as `audio:pcmFailed`: `CANCELLED`, `INVALID_PARAMS` (`start` is at or past the end of the track), `NOT_SUPPORTED` (no resampler handles the conversion), `DECODER_FAILED`, `DECODE_FAILED`, `ORIGIN_DENIED` (the page's origin is no longer trusted), `OPERATION_FAILED` (the buffer could not be allocated or posted) and `UNKNOWN_ERROR`.
- Nothing is sent to a page that navigated away or whose window or panel closed before the result was ready, nor when foobar2000 exits.

```js
const pending = await fb2k.invoke('audio.decodePcm', {
    path: 'C:\\Music\\song.flac',
    start: 30,
    end: 60,
    sampleRate: 22050,
});
if (pending.success === false) throw new Error(pending.error);
window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
    const info = e.additionalData;
    if (info?.purpose !== 'audio.decodePcm' || info.taskId !== pending.taskId) return;
    const buffer = e.getBuffer();
    const capacityFrames = new DataView(buffer).getUint32(24, true);
    const channel = (c) => new Float32Array(buffer, 64 + c * capacityFrames * 4, info.frames);
    console.log(info.sampleRate, channel(0).length);
    window.chrome.webview.releaseBuffer(buffer);
});
```

### audio.generateFullWaveform

<!-- api-schema:begin audio.generateFullWaveform -->
Compute a waveform of a whole track. A cache hit answers `ready` with the waveform; otherwise the answer is `pending` with a task id, and the result arrives as `audio:fullWaveformReady` or `audio:fullWaveformFailed`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Track path; a `\|subsong:N` suffix selects a subsong. Must not be empty. |
| `cueIndex` | `integer` | No | Subsong index, 0-based; takes precedence over a subsong in `path`. At least `0`. |
| `resolution` | `integer` | No | Number of points, clamped to 64 to 4096. A track is decoded once per resolution; the other options are computed from the cache. Default: `256`. |
| `method` | `"rms" \| "peak"` | No | How each point is taken. Default: `"rms"`. |
| `scale` | `"linear" \| "db"` | No | Scale of the points: `linear`, or `db`, where 60 dB below the track's own maximum maps to 0. Ignored when `signed` is set. Default: `"linear"`. |
| `signed` | `boolean` | No | Keep PCM polarity; points fall in `[-1, 1]`. Default: `false`. |
| `preferCache` | `boolean` | No | Answer from the cache when possible. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `status` | `"ready" \| "pending"` | `ready`: the waveform is in this answer. `pending`: it arrives as an event for `taskId`. |
| `cached` | `boolean` | The answer came from the cache. |
| `taskId` | `string` | Identifies the task in events and cancelFullWaveform; `pending` answers only. |
| `waveform` | `number[]` | `ready` answers: the points, normalized by `maxAmplitude`. |
| `maxAmplitude` | `number` | `ready` answers: the largest value of the selected sequence before normalization, in linear full-scale units. On the `linear` scale `waveform[i] * maxAmplitude` restores the level; on `db`, dBFS is `(v * 60 - 60) + 20 * log10(maxAmplitude)`. |
| `duration` | `number` | `ready` answers: track length in seconds. |
| `sampleRate` | `integer` | `ready` answers: sample rate in Hz. |
| `channels` | `integer` | `ready` answers: channel count. |
| `resolution` | `integer` | Point count in use. |
| `method` | `"rms" \| "peak"` | As requested. |
| `scale` | `"linear" \| "db"` | As requested. |
| `signed` | `boolean` | As requested. |
| `path` | `string` | The requested path. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

On a cache miss, requests for the same track and `resolution` that are already in flight share one decode; each keeps its own `taskId` and gets a ready event in its own shape. At most two tracks decode at once and the rest wait in submission order, which counts toward the caller's own timeout. Cancel a request that is no longer needed with `audio.cancelFullWaveform`. Every `taskId` gets exactly one terminal event, ready or failed (`code: "CANCELLED"` when cancelled); a cache hit answers synchronously without one. Requests from a popup that closes, or still pending when foobar2000 exits, are dropped without an event.

Errors known at request time come back in the answer: `INVALID_PARAMS`, `INVALID_PATH` (the file cannot be found or read) and `OPERATION_FAILED` (foobar2000 is shutting down, or any other error). A decode that fails later ends with `audio:fullWaveformFailed`.

```js
// Minimal call: cache hit returns status 'ready', otherwise 'pending' + taskId
const res = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'C:\\Music\\song.flac'
});
if (res.success === false) throw new Error(res.error);
const { status, taskId } = res;

// Higher-resolution peak waveform on a dB scale
const detailed = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'C:\\Music\\song.flac',
    resolution: 1000,
    method: 'peak',
    scale: 'db'
});
```

### audio.getOutputInfo

<!-- api-schema:begin audio.getOutputInfo -->
Report foobar2000's volume.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `volume` | `number` | Volume in dB, `-100` (silence) to `0` (full). |
| `volumePercent` | `number` | The same volume as a linear percentage, `100 * 10^(volume / 20)`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('audio.getOutputInfo');
```

### audio.getPcmDebugState

<!-- api-schema:begin audio.getPcmDebugState -->
Experimental API; it may change in future releases.

Report the shared-buffer support of the calling page, the decode task queue and the stream subscriptions, for tests and diagnostics.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `runtime` | `PcmRuntimeState` | Shared-buffer support of the calling page; both flags are `false` when the page cannot be located. |
| `runtime.version` | `string` | Version of the WebView2 runtime installed on the machine, such as `153.0.4234.48`; empty when it cannot be read. |
| `runtime.environment12` | `boolean` | The page's environment can create shared buffers (`ICoreWebView2Environment12`). |
| `runtime.webview17` | `boolean` | The page's webview can receive shared buffers (`ICoreWebView2_17`). |
| `decode` | `PcmDecodeState` | The decodePcm task queue. |
| `decode.active` | `integer` | Tasks decoding, including aborted ones whose worker has not returned yet. |
| `decode.queued` | `integer` | Tasks waiting for a free slot. |
| `decode.openBufferBytes` | `integer` | Bytes of decode buffers the host has not closed yet. |
| `stream` | `PcmStreamState` | The stream capture callback and every stream subscription of every page. |
| `stream.callbackRegistered` | `boolean` | The component has a capture callback registered with the core; `false` when no subscription exists. |
| `stream.interval` | `number` | Interval currently requested from the core, in seconds; absent when the core's default is used. |
| `stream.chunkCount` | `integer` | Chunks the callback has received since the component loaded. |
| `stream.chunkCycles` | `integer` | CPU cycles the callback has spent in total, from `QueryThreadCycleTime`. |
| `stream.subscriptions` | `PcmStreamDebugEntry[]` | Every live stream subscription. |
| `stream.subscriptions[].subscriptionId` | `string` | The subscription's id. |
| `stream.subscriptions[].windowId` | `string` | Window id of the subscribing page. |
| `stream.subscriptions[].epoch` | `integer` | Buffer generation, `0` before the first chunk of audio arrived. |
| `stream.subscriptions[].capacityFrames` | `integer` | Frames the current ring holds; `0` before the first chunk. |
| `stream.subscriptions[].writeFrames` | `integer` | Frames written into the current ring so far, wrapping at 2^32. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`decode.openBufferBytes` drops back to `0` once every finished task has been delivered and its buffer closed on the host side; pages keep their own views until they release them.

```js
const res = await fb2k.invoke('audio.getPcmDebugState');
if (res.success === false) throw new Error(res.error);
const { runtime, decode } = res;
```

### audio.getSpectrum

<!-- api-schema:begin audio.getSpectrum -->
Compute one spectrum frame on demand. At least one spectrum subscription must exist.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Compute the frame with this subscription's parameters, bin output included; the other parameters are then ignored. An unknown id fails with `NOT_FOUND`. Must not be empty. |
| `bands` | `integer` | No | Used without `subscriptionId`. `0` takes the largest band count among all subscriptions; the value is capped at `fftSize / 2`. At least `0`. Default: `0`. |
| `scale` | `"weighted" \| "db"` | No | Used without `subscriptionId`; as in subscribeSpectrum. Default: `"weighted"`. |
| `minFrequency` | `number` | No | Used without `subscriptionId`; as in subscribeSpectrum. At least `1`. Default: `20`. |
| `maxFrequency` | `number` | No | Used without `subscriptionId`; as in subscribeSpectrum. |
| `output` | `"bands" \| "bins"` | No | Without `subscriptionId` only `bands` is accepted: the FFT size then follows the other subscriptions, so bin output needs a subscription of its own. Default: `"bands"`. |
| `channels` | `"mix" \| "stereo"` | No | Without `subscriptionId` only `mix` is accepted. Default: `"mix"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `subscriptionId` | `string` | Subscription the frame belongs to; absent when the request named none. |
| `output` | `"bands" \| "bins"` | What the frame carries. |
| `spectrum` | `number[]` | Band frames: one value per band in the frame's `scale`; `weighted` values lie in `[0, 1]`, `db` values have a floor of `-160`. Bin frames with `channels: 'mix'`: one power value in dB per bin. |
| `left` | `number[]` | Bin frames with `channels: 'stereo'`: the first channel, one dB value per bin. |
| `right` | `number[]` | Bin frames with `channels: 'stereo'`: the second channel, or the first again for a mono stream. |
| `bands` | `integer` | Band frames: length of `spectrum`. |
| `firstBin` | `integer` | Bin frames: FFT bin index of the first value; value `i` is centred at `(firstBin + i) * sampleRate / fftSize` Hz. `0`, with empty arrays, when the range holds no bin. |
| `fftSize` | `integer` | FFT size actually used. |
| `scale` | `"weighted" \| "db"` | Scale of the values; always `db` for bin frames. |
| `channels` | `"mix" \| "stereo"` | Bin frames: the subscription's channel layout. |
| `channelCount` | `integer` | Bin frames: channel count of the visualization stream; `0` when unknown. |
| `sampleRate` | `integer` | Sample rate of the visualization stream in Hz; `0` when unknown. |
| `minFrequency` | `number` | Lower edge of the frequency range in Hz. |
| `maxFrequency` | `number` | Upper edge in Hz: the requested one capped at `sampleRate / 2`, or `sampleRate / 2` when none was given. |
| `state` | `"playing" \| "paused" \| "stopped"` | Paused or stopped frames are silence. |
| `streamTime` | `number` | Seconds since the visualization stream last started; the frame is computed from the `fftSize` samples ending at this time. |
| `hostTime` | `number` | Host system time when the frame was computed, Unix epoch milliseconds. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The answer is one frame with the fields listed under [`audio.subscribeSpectrum`](#audio-subscribespectrum). While playback is paused or stopped the answer is a silence frame whose `state` says which. Without any subscription, or while a new stream has not produced data yet (about 0.7 s after the first subscription), the call fails with `OPERATION_FAILED`.

::: tip Pull or push
Both deliver the same frame. Pull when drawing must follow the display frame by frame or needs more than 60 frames per second: subscribe with `fps: 1` to keep the subscription alive, call `audio.getSpectrum` with its `subscriptionId` from `requestAnimationFrame`, wait for each answer before the next call, and skip frames whose `streamTime` has not changed. Each call computes an FFT on the host's main thread, costing about as much as a pushed frame, so limit the calls to the frame rate you need rather than the display refresh rate. For up to 60 frames per second, pushed frames work equally well.
:::

```js
// Pull from requestAnimationFrame: one call at a time, at most about 60 calls per second
const res = await fb2k.invoke('audio.subscribeSpectrum', { fps: 1 });
if (res.success === false) throw new Error(res.error);
const { subscriptionId } = res;
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

<!-- api-schema:begin audio.getSpectrumDebugState -->
Report the spectrum runtime as the host sees it, for tests and diagnostics. The shape is not a stable contract.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `active` | `boolean` | At least one spectrum subscription is registered. |
| `timerRunning` | `boolean` | The beat thread that pushes frames is running. |
| `timerHwnd` | `integer` | Deprecated; always `0`. |
| `beatSource` | `"high-resolution" \| "standard" \| null` | Timer of the beat thread; `null` while it is not running. |
| `beatIntervalMs` | `number \| null` | Current beat in milliseconds, 1000 / `effectiveFps`; `null` while the beat thread is not running. |
| `beatsCoalesced` | `integer` | Beats dropped because the previous one had not reached the main thread yet; cumulative. |
| `effectiveFftSize` | `integer` | Largest FFT size requested among all subscriptions; each still computes with its own. |
| `effectiveFps` | `integer` | Largest frame rate requested among all subscriptions. |
| `effectiveBands` | `integer` | Largest band count requested among all subscriptions. |
| `skipFrames` | `integer` | Ticks the pushed frames will skip because the last one ran over its interval. |
| `framesComputed` | `integer` | FFTs computed since the host started; flat while playback is paused or stopped. |
| `streamReady` | `boolean` | The visualization stream exists. |
| `subscriptionCount` | `integer` | Length of `subscriptions`. |
| `dispatchTargetCount` | `integer` | Length of `dispatchTargets`. |
| `subscriptions` | `SpectrumDebugSubscription[]` | Every spectrum subscription of every page. |
| `subscriptions[].token` | `string` | The subscription's key. |
| `subscriptions[].windowId` | `string` | Window id of the subscribing page. |
| `subscriptions[].ownerHwnd` | `integer` | Handle of the window that owns the subscription, as a number. |
| `subscriptions[].event` | `string` | Event name of the frames. |
| `subscriptions[].fftSize` | `integer` | Requested FFT size. |
| `subscriptions[].fps` | `integer` | Frame rate after clamping. |
| `subscriptions[].bands` | `integer` | Band count after clamping. |
| `subscriptions[].scale` | `"weighted" \| "db"` | Scale in use. |
| `subscriptions[].backgroundThrottle` | `boolean` | As requested. |
| `subscriptions[].minFrequency` | `number` | Lower edge of the range in Hz. |
| `subscriptions[].maxFrequency` | `number \| null` | Requested upper edge in Hz; `null` when the range follows half the stream's sample rate. |
| `subscriptions[].output` | `"bands" \| "bins"` | Output in use. |
| `subscriptions[].channels` | `"mix" \| "stereo"` | Channel layout in use. |
| `dispatchTargets` | `SpectrumDebugTarget[]` | Windows the pushed frames are delivered to. |
| `dispatchTargets[].windowId` | `string` | Window id the frames go to. |
| `dispatchTargets[].ownerHwnd` | `integer` | Handle of that window, as a number. |
| `dispatchTargets[].event` | `string` | Event name of the frames. |
| `instanceCount` | `integer` | WebView2 instances in the component. |
| `callerHwnd` | `integer` | Window handle of the calling page, as a number; `0` when unknown. |
| `callerWindowId` | `string` | Window id of the calling page; empty when unknown. |
| `callerOwnsSubscription` | `boolean` | The calling page owns at least one subscription. |
| `foregroundHwnd` | `integer` | Handle of the foreground window, as a number. |
| `foregroundPid` | `integer` | Process id of the foreground window. |
| `foregroundIsExternal` | `boolean` | The foreground window belongs to another process, so the background frame limit applies. |
| `foregroundTitle` | `string` | Title of the foreground window. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('audio.getSpectrumDebugState');
```

### audio.getStreamInfo

<!-- api-schema:begin audio.getStreamInfo -->
Report the format of the playing track.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `playing` | `boolean` | A track is playing or paused; the other fields are present only then. |
| `sampleRate` | `integer` | Sample rate in Hz. |
| `channels` | `integer` | Channel count. |
| `bitrate` | `integer` | Bitrate in kbps. |
| `codec` | `string` | Codec name as foobar2000 reports it, or `unknown`. |
| `duration` | `number` | Track length in seconds. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('audio.getStreamInfo');
```

### audio.getWaveform

<!-- api-schema:begin audio.getWaveform -->
Return the most recent `duration` seconds of the visualization stream, ending at its current time (`streamTime` in spectrum frames). Needs a spectrum subscription.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `duration` | `number` | No | Window length in seconds, greater than 0. At most `1`. Default: `0.05`. |
| `signed` | `boolean` | No | `true` keeps PCM polarity, clamped to `[-1, 1]`; `false` maps the magnitude from -70 to 0 dB onto `[0, 1]`. Default: `false`. |
| `channels` | `"mix" \| "stereo"` | No | `mix` averages the channels into `waveform`; `stereo` returns the first two channels as `left` and `right`. Default: `"mix"`. |
| `points` | `integer` | No | Thin the window to this many evenly spaced samples, without averaging; every sample is returned when omitted. Between `2` and `65536` inclusive. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `waveform` | `number[]` | `mix` answers: one value per sample. |
| `left` | `number[]` | `stereo` answers: the first channel. |
| `right` | `number[]` | `stereo` answers: the second channel; a copy of `left` when the stream has one channel. |
| `duration` | `number` | Window length in seconds, as requested. |
| `signed` | `boolean` | As requested. |
| `channels` | `"mix" \| "stereo"` | As requested. |
| `sampleRate` | `integer` | Sample rate of the visualization stream in Hz. |
| `channelCount` | `integer` | Channel count of the visualization stream; `1` after setChannelMode with `mono`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

- With one channel `right` equals `left`. Channels beyond the second are ignored, not mixed in. After `audio.setChannelMode` with `'mono'` the stream has one channel.
- Within `duration` seconds of starting playback, a seek or a manual track change, the part of the window before the restart is zeros.
- Without a spectrum subscription, or when the stream cannot supply the window, the call fails with `OPERATION_FAILED`.

```js
const res = await fb2k.invoke('audio.getWaveform', { duration: 0.1 });
if (res.success === false) throw new Error(res.error);
const { waveform } = res;

// Two channels, 256 evenly spaced samples each (a stereo field display)
const res2 = await fb2k.invoke('audio.getWaveform', {
  duration: 0.05,
  signed: true,
  channels: 'stereo',
  points: 256,
});
if (res2.success === false) throw new Error(res2.error);
const { left, right } = res2;
```

### audio.isVisualizationAvailable

<!-- api-schema:begin audio.isVisualizationAvailable -->
Report whether foobar2000 offers a visualization stream. Spectrum frames and getWaveform need one.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `available` | `boolean` | foobar2000 offers a visualization stream. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('audio.isVisualizationAvailable');
```

### audio.setChannelMode

<!-- api-schema:begin audio.setChannelMode -->
Choose which channels the visualization stream carries. Applies to spectrum frames and getWaveform, not to what is heard.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `"default" \| "mono" \| "front" \| "back"` | No | Channels the visualization stream carries. Default: `"default"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `"default" \| "mono" \| "front" \| "back"` | The mode now in effect. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('audio.setChannelMode', { mode: 'mono' });
```

### audio.subscribeSpectrum

<!-- api-schema:begin audio.subscribeSpectrum -->
Subscribe to spectrum frames of the playing audio. Each subscription keeps its own parameters; frames arrive under `event` in the calling window only, at up to `fps` frames per second while playback runs, plus one silence frame when it pauses or stops.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Key of the subscription; subscribing again with the same id replaces that subscription. When omitted, the key is derived from the calling window and `event`. Must not be empty. |
| `fftSize` | `integer` | No | FFT size, a power of two; other values fail with `INVALID_PARAMS`. Band output raises it for 32 or more bands; bin output uses it as given. 65536 is this component's ceiling, not a foobar2000 limit, and costs about 1.5 s of PCM per frame. Between `256` and `65536` inclusive. Default: `1024`. |
| `bands` | `integer` | No | Band output only: number of bands, clamped to 8 to `fftSize / 2`. Default: `48`. |
| `fps` | `integer` | No | Frames per second, clamped to 1 to 60. Default: `30`. |
| `scale` | `"weighted" \| "db"` | No | Band scale, `weighted` when omitted. With `output: 'bins'` omit it or pass `db`; anything else fails with `INVALID_PARAMS`. |
| `output` | `"bands" \| "bins"` | No | What the frames carry; `bands` is ignored for bin output. Default: `"bands"`. |
| `channels` | `"mix" \| "stereo"` | No | Bin output only; `stereo` with band output fails with `INVALID_PARAMS`. Default: `"mix"`. |
| `backgroundThrottle` | `boolean` | No | `true` limits this subscription to at most 12 frames per second, usually 10 to 12, while another application is in the foreground; `false` keeps the full rate. Default: `true`. |
| `minFrequency` | `number` | No | Lower edge of the frequency range in Hz; bands divide the range logarithmically. At least `1`. Default: `20`. |
| `maxFrequency` | `number` | No | Upper edge in Hz, greater than `minFrequency`. Capped at half the stream's sample rate when frames are computed, and follows it when omitted. A range entirely above that frequency yields silence frames. |
| `event` | `string` | No | Event name the frames are delivered under. Must not be empty. Default: `"audio:spectrum"`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `subscriptionId` | `string` | The subscription's key, as given or derived. |
| `fftSize` | `integer` | Requested FFT size; frames report the size actually used. |
| `bands` | `integer` | Band count after clamping. |
| `fps` | `integer` | Frame rate after clamping. |
| `scale` | `"weighted" \| "db"` | Scale in use; always `db` for bin output. |
| `backgroundThrottle` | `boolean` | As requested. |
| `minFrequency` | `number` | Lower edge of the range in Hz, as requested. |
| `maxFrequency` | `number \| null` | Requested upper edge in Hz; `null` when the range follows half the stream's sample rate. |
| `output` | `"bands" \| "bins"` | Output registered. |
| `channels` | `"mix" \| "stereo"` | Channel layout registered. |
| `event` | `string` | Event name the frames arrive under. |
| `streamReady` | `boolean` | `true` once the visualization stream exists, which includes the stopped state; whether audio flows shows in the frames' `state`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Another subscription with a larger FFT size, band count or frame rate does not change the frames this one receives. A registered subscription always answers `success: true`; `streamReady` is `true` once the visualization stream exists, which includes the stopped state. Invalid parameters register nothing and fail with `INVALID_PARAMS`; an `fftSize` that is not a power of two, a `maxFrequency` not above `minFrequency`, `channels: 'stereo'` with band output and a `scale` other than `db` with bin output also carry `details: { param, value }`.

Each frame is delivered to the subscribing window only, under the subscription's `event`:

| Field | Description |
| --- | --- |
| `subscriptionId` | Subscription the frame belongs to. Several subscriptions on the same event name each receive their own frames, so filter on this field. |
| `output` | `bands` or `bins`. Older hosts omit it and send band frames. |
| `spectrum` | Band frames: one value per band in the subscription's `scale`; `weighted` values lie in `[0, 1]`, `db` values have a floor of `-160`. Bin frames with `channels: 'mix'`: one power value in dB per bin. |
| `left`, `right` | Bin frames with `channels: 'stereo'`, in place of `spectrum`: the first two channels of the visualization stream; for a mono stream `right` repeats `left`. |
| `bands` | Band frames: length of `spectrum`. Bin frames do not carry it. |
| `firstBin` | Bin frames: FFT bin index of the first value; value `i` is centred at `(firstBin + i) * sampleRate / fftSize` Hz. `0`, with empty arrays, when the range holds no bin. |
| `fftSize` | FFT size actually used. For band output, band counts of 32 and 64 or more raise it to at least 4096 and 8192; bin output uses the requested size. |
| `scale` | `weighted` or `db`; bin frames always carry `db`. |
| `channels`, `channelCount` | Bin frames: the subscription's `channels` and the channel count of the visualization stream, `0` when unknown. |
| `sampleRate` | Sample rate of the visualization stream in Hz; `0` when unknown. |
| `minFrequency`, `maxFrequency` | Frequency range that bands divide logarithmically and that a bin's centre must lie in: the subscription's `minFrequency` (20 Hz by default) to its `maxFrequency` capped at `sampleRate / 2`, or `sampleRate / 2` when none was given. The answer to the subscription echoes the requested values, with `maxFrequency: null` when it was omitted. |
| `state` | `playing`, `paused` or `stopped`. When playback pauses or stops, each subscription receives one silence frame with the new state and then no frames until playback resumes. |
| `streamTime` | Seconds since the visualization stream last started; the frame is computed from the `fftSize` samples ending at this time. Starting playback, seeking and manual track changes restart it at `0`, where it stays for about 200 ms with silence frames; until `fftSize / sampleRate` seconds have passed, the part of the window before the restart is silence. Natural track transitions do not restart it. The visualization stream runs a little over ten milliseconds behind what is heard (13 ms measured on one setup). |
| `hostTime` | Host system time when the frame was computed, Unix epoch milliseconds. `Date.now() - frame.hostTime` measures delivery latency. |

#### Bin output

With `output: 'bins'` each frame carries the FFT's linear bins without the band processing: no interpolation of narrow low bands, no weighting and no raise of `fftSize`. How to group, interpolate or smooth them is up to the page.

- A value is `10 · log10(p) − 2.75` dB, where `p` is the bin's squared magnitude (`stereo`) or its mean over the channels (`mix`), rounded to 0.01 dB with a floor of `-160`. The powers of the bins a full-scale sine's main lobe covers sum to 0 dB, so adding bins as powers, `10 · log10(Σ 10^(v / 10))`, gives the `db` band value for the same range. The peak bin alone reads lower because foobar2000's FFT uses a Gaussian window, which spreads a sine over neighbouring bins: at 48 kHz and 8192 points a 0 dBFS 1 kHz sine reads -3.18 dB in its peak bin, with six bins within 40 dB of it.
- Bin `k` (from 1; the DC bin is never sent) is included when its centre `k · sampleRate / fftSize` lies in `[minFrequency, maxFrequency)`. Pass a `minFrequency` below 20 to receive the lowest bins as well.
- A silence frame keeps the length and `firstBin` of the subscription's previous frame, filled with `-160`; before any frame it is empty.
- To choose the window function, zero-pad or use another transform, compute the spectrum yourself from the PCM of `audio.subscribeStream`; see [Computing a spectrum yourself](../sdk/audio.md#computing-a-spectrum-yourself).

Frames are JSON, about 7 bytes per value. Estimated sizes and pull round trips at 48 kHz over the default range, from JSON answers of the same size measured on one machine:

| `fftSize` | `channels` | Values | Size | Round trip |
| --- | --- | --- | --- | --- |
| 4096 | `mix` | 2046 | 14 KB | 0.5 ms |
| 16384 | `mix` | 8185 | 56 KB | 1.1 ms |
| 16384 | `stereo` | 16370 | 112 KB | 1.9 ms |
| 65536 | `stereo` | 65480 | 448 KB | 6.6 ms |

Most of this time is spent on foobar2000's main thread, so pick the frame rate by size. When one tick of pushed frames takes longer than the frame interval, every subscription skips the next tick.

::: tip Band output
Band output (`output: 'bands'`) is kept for compatibility and its values do not change. It maps the bins onto log-spaced bands, interpolates bands narrower than one bin from the neighbouring bins, and raises `fftSize` to at least 4096 or 8192 for 32 or 64 bands; `weighted` also applies a tilt, a bass shelf below 200 Hz and a display curve. New code should use bin output.
:::

```js
// Minimal call: all defaults, result delivered on 'audio:spectrum'
const res = await fb2k.invoke('audio.subscribeSpectrum');
if (res.success === false) throw new Error(res.error);
const { subscriptionId } = res;

// Explicit configuration
const custom = await fb2k.invoke('audio.subscribeSpectrum', {
    bands: 64,
    fftSize: 2048,
    fps: 60
});

// Linear bins in dB for the left and right channels
const bins = await fb2k.invoke('audio.subscribeSpectrum', {
    output: 'bins',
    channels: 'stereo',
    fftSize: 16384,
    fps: 60
});
```

### audio.subscribeStream

<!-- api-schema:begin audio.subscribeStream -->
Experimental API; it may change in future releases.

Subscribe to the audio foobar2000 is playing. Every chunk the core plays is copied into a ring buffer shared with the page, one buffer per subscription. The buffer arrives as a `sharedbufferreceived` event on `chrome.webview` with the first chunk of audio, and again with a new `epoch` whenever the sample rate or channel count changes; `audio:stream` then reports the old epoch as ended. Nothing arrives while playback is stopped or paused.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Subscription id chosen by the caller; the host generates one (`pcmstream_N`) when absent. Subscribing again with the same id from the same page replaces the earlier subscription. Must not be empty. |
| `interval` | `number` | No | Callback interval to request from foobar2000, in seconds. The core rounds it to its own tick of about 16 ms and never goes above 200 ms; one subscription asking for a short interval shortens it for every stream subscription in the component. Defaults to the core's 200 ms. Between `0.01` and `0.2` inclusive. |
| `bufferSeconds` | `number` | No | Ring buffer length, in seconds. Reduced when the buffer would exceed 64 MiB. Between `0.1` and `10` inclusive. Default: `1`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `subscriptionId` | `string` | The subscription's id, as given or generated. |
| `interval` | `number` | Interval requested from the core; absent when the core's default is used. |
| `bufferSeconds` | `number` | Ring buffer length registered, in seconds. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The samples do not travel through the JSON channel. Once audio plays, the page receives a `sharedbufferreceived` event on `window.chrome.webview`: `e.getBuffer()` is the ring buffer and `e.additionalData` is `{ purpose: 'audio.subscribeStream', subscriptionId, epoch, sampleRate, channels, capacityFrames, headerBytes: 64 }`. The same event arrives again with `epoch + 1` when the sample rate or channel count changes (a track in another format, or a DSP that changes the channel count); one `audio:stream` event `{ subscriptionId, type: 'ended', epoch, reason: 'format-change' }` then names the epoch that ended, whose buffer also has its `ended` flag set.

The buffer starts with the same 64-byte header as `audio.decodePcm` buffers, followed by **interleaved** float32 frames: the frame in slot `s` has channel `c` at byte `64 + (s × channels + c) × 4`. In the header, `capacityFrames` (u32 at byte 24) is the number of slots, `writeFrames` (u32 at byte 32) counts frames written so far and wraps at 2^32, `writeSlot` (u32 at byte 60) is the slot the next frame goes to, `seq` (u32 at byte 28) is odd while the host writes, and bit 0 of `flags` (u32 at byte 36) is set once the host stops writing. Read like this: load `seq` and try again later if it is odd; copy the frames you want, newest ending at `writeSlot`; load `seq` again and start over if it changed. A reader that falls behind by more than `capacityFrames` has lost the oldest frames: `dropped = writeFrames − readFrames − capacityFrames` when positive. The SDK method `fb.audio.subscribeStream` does all of this and returns planar `Float32Array` chunks; a page that uses it should not also handle these buffers itself.

- The samples are taken after the DSP chain and before ReplayGain and the volume control: what the DSP chain outputs, at full scale. Unlike `audio:spectrum`, they do not include ReplayGain.
- The buffer is **read-only shared memory**. Writing to it crashes the page's renderer process, and with it the whole foobar2000 window. Call `chrome.webview.releaseBuffer(buffer)` once you are done with an epoch: after `audio:stream` reports it ended, and for the last epoch after unsubscribing.
- Chunks arrive about every 200 ms by default and at most about every 16 ms (`interval`). The first chunk after playback starts or resumes can hold up to 0.8 s of audio, so a `bufferSeconds` below that loses frames at every start.
- Nothing arrives while playback is stopped or paused; `writeFrames` stops growing. Seeking and track changes within the same format do not reset the buffer.
- A page may hold up to 8 stream subscriptions; a ninth answers `OPERATION_FAILED`. Subscribing again with the same `subscriptionId` replaces the earlier subscription: its buffer gets the `ended` flag, no event is sent, and the next epoch continues the count.
- Errors known at request time come back in the answer: `INVALID_PARAMS` for values out of range or unknown keys, `NOT_SUPPORTED` when the WebView2 runtime cannot share buffers or the calling page cannot be located, `OPERATION_FAILED` when foobar2000 is shutting down or the page already has 8 subscriptions.
- Subscriptions end without an event when the page navigates away, its window or panel closes, or foobar2000 exits.

```js
const sub = await fb2k.invoke('audio.subscribeStream', { subscriptionId: 'meter', bufferSeconds: 0.5 });
if (sub.success === false) throw new Error(sub.error);
window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
    const info = e.additionalData;
    if (info?.purpose !== 'audio.subscribeStream' || info.subscriptionId !== sub.subscriptionId) return;
    const buffer = e.getBuffer();
    const header = new DataView(buffer);
    console.log(info.epoch, header.getUint32(16, true), header.getUint32(20, true)); // epoch, sampleRate, channels
});
```

### audio.unsubscribeSpectrum

<!-- api-schema:begin audio.unsubscribeSpectrum -->
Remove spectrum subscriptions of the calling page.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Subscription to remove; omit to remove every spectrum subscription of the calling page. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `removed` | `integer` | Subscriptions removed: 0 or 1 with a `subscriptionId`, any number without. Subscriptions of other pages are never counted or removed. |
| `subscriptionId` | `string` | The requested id; empty when none was given. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('audio.unsubscribeSpectrum', { subscriptionId });
```

### audio.unsubscribeStream

<!-- api-schema:begin audio.unsubscribeStream -->
Experimental API; it may change in future releases.

Remove stream subscriptions of the calling page. Their buffers get the `ended` flag and are closed on the host side; no event is sent.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | No | Subscription to remove; omit to remove every stream subscription of the calling page. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `removed` | `integer` | Subscriptions removed: 0 or 1 with a `subscriptionId`, any number without. Subscriptions of other pages are never counted or removed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

The removed subscriptions' buffers get their `ended` flag (bit 0 of the u32 at byte 36) set before the host closes its side; the page's views stay readable until the page releases them. Removing the last stream subscription in the component also unregisters the capture callback from foobar2000.

```js
await fb2k.invoke('audio.unsubscribeStream', { subscriptionId: 'meter' });
```

## Runtime behavior notes

- `audio.subscribeSpectrum` creates a subscription of the calling window, or replaces the one with the same `subscriptionId`. Without `subscriptionId` the key is derived from the calling window and `event`; frames arrive under `event`, which defaults to `audio:spectrum`. The SDK's `fb.audio.subscribeSpectrum()` sets up the same subscription.
- Spectrum subscriptions do not share parameters. A beat thread on a high-resolution timer ticks at the highest requested `fps`, and each subscription gets a frame when its own interval is due, so pushed frames arrive close to the requested rate; subscriptions with the same effective FFT size, band count, `scale` and frequency range share one FFT per tick. Host main-thread time grows with the number of frames, about 0.5 ms each.
- The `db` scale reflects the signal after ReplayGain and DSP processing and before the volume control, so changing the volume does not change the readings.
- `audio.getSpectrum` and `audio.getWaveform` read the visualization stream. Both fail with `OPERATION_FAILED` while no spectrum subscription exists; `audio.getSpectrum` also fails while playback runs and the stream has no data yet, and answers a silence frame while playback is paused or stopped.
- `audio.generateFullWaveform` returns `status: "ready"` with cached data or `status: "pending"` with `taskId`. The caller receives `audio:fullWaveformReady` or `audio:fullWaveformFailed`; `cueIndex` takes precedence over a `path|subsong:N` suffix.
