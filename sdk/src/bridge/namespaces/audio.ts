import { bridge } from '../Bridge.js';
import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import { decodePcm, type DecodePcmOptions } from '../pcm/decodePcm.js';
import type { PcmBuffer } from '../pcm/PcmBuffer.js';
import { subscribeStream, type PcmStream, type PcmStreamOptions } from '../pcm/PcmStream.js';
import type {
    FullWaveformOptions,
    FullWaveformResult,
    SpectrumBinsFrame,
    SpectrumSubscribeOutcome,
    SpectrumSubscription,
} from '../../types/responses.js';
import type {
    AudioSpectrumPayload,
    FullWaveformFailedEvent,
    FullWaveformReadyEvent,
} from '../../types/events.js';
import type {
    AudioGenerateFullWaveformParams,
    AudioGetSpectrumParams,
    AudioGetWaveformParams,
    AudioSetChannelModeParams,
    AudioSubscribeSpectrumParams,
    AudioSubscribeStreamParams,
} from '../../types/generated/params.js';
import type {
    AudioSubscribeSpectrumResponse,
} from '../../types/generated/responses.js';

/** @deprecated Use `AudioSubscribeSpectrumParams`. */
export type SubscribeSpectrumOptions = AudioSubscribeSpectrumParams;

type SpectrumCallback = (data: AudioSpectrumPayload) => void;

type SpectrumBinsCallback = (frame: SpectrumBinsFrame) => void;

/**
 * Call signatures of {@link audio.subscribeSpectrum}. Options with
 * `output: 'bins'` pass bin frames to the callback; any other options pass
 * band frames.
 */
export interface SpectrumSubscribeFunction {
    (
        callback: (frame: SpectrumBinsFrame) => void,
        options: AudioSubscribeSpectrumParams & { output: 'bins' },
    ): SpectrumSubscription;
    (callback: (frame: AudioSpectrumPayload) => void, options?: AudioSubscribeSpectrumParams): SpectrumSubscription;
}

/** @deprecated Use `AudioSubscribeStreamParams`. */
export type AudioStreamOptions = AudioSubscribeStreamParams;

/** @deprecated Use `AudioGetSpectrumParams`. */
export type GetSpectrumOptions = AudioGetSpectrumParams;

/** @deprecated Use `AudioGetWaveformParams`. */
export type GetWaveformOptions = AudioGetWaveformParams;

/**
 * Generate a UUID for spectrum subscription identifiers, falling back
 * to a timestamped random suffix when `crypto.randomUUID` is missing
 * (older WebView2 builds).
 */
function newSubscriptionId(): string {
    if (
        typeof globalThis.crypto !== 'undefined' &&
        typeof globalThis.crypto.randomUUID === 'function'
    ) {
        return globalThis.crypto.randomUUID();
    }
    return (
        'spectrum_' +
        Date.now().toString() +
        '_' +
        Math.random().toString(36).slice(2)
    );
}

/**
 * True for a frame tagged with another subscription's id. Untagged frames
 * (hosts that merge subscriptions) are not foreign.
 */
function isForeignFrame(data: unknown, subscriptionId: string): boolean {
    if (typeof data !== 'object' || data === null || !('subscriptionId' in data)) {
        return false;
    }
    const id = data.subscriptionId;
    return typeof id === 'string' && id !== subscriptionId;
}

/** True for a frame of bin output. Hosts without bin output never send one. */
function isBinsFrame(data: unknown): data is SpectrumBinsFrame {
    return typeof data === 'object' && data !== null && 'output' in data && data.output === 'bins';
}

/** What an aborted `signal` rejects `generateFullWaveform` with. */
function abortError(): DOMException {
    return new DOMException('The operation was aborted.', 'AbortError');
}

/** Map the host's answer to `audio.subscribeSpectrum` onto the `ready` outcome. */
function toSubscribeOutcome(
    resp: AudioSubscribeSpectrumResponse,
    requested: { subscriptionId: string; fftSize: number; bands: number; fps: number },
): SpectrumSubscribeOutcome {
    if (typeof resp !== 'object' || resp === null) {
        return { ok: false, code: 'UNKNOWN_ERROR' };
    }
    if (resp.success !== true) {
        return {
            ok: false,
            code: typeof resp.code === 'string' ? resp.code : 'UNKNOWN_ERROR',
            error: typeof resp.error === 'string' ? resp.error : undefined,
        };
    }
    // Hosts that predate scale / backgroundThrottle / streamReady only answer
    // success when the stream is valid, always use the weighted scale and
    // always throttle; hosts without a frequency range always cover
    // 20 Hz to half the stream's sample rate.
    return {
        ok: true,
        subscriptionId: resp.subscriptionId ?? requested.subscriptionId,
        fftSize: resp.fftSize ?? requested.fftSize,
        bands: resp.bands ?? requested.bands,
        fps: resp.fps ?? requested.fps,
        scale: resp.scale === 'db' ? 'db' : 'weighted',
        output: resp.output === 'bins' ? 'bins' : 'bands',
        channels: resp.channels === 'stereo' ? 'stereo' : 'mix',
        backgroundThrottle: resp.backgroundThrottle ?? true,
        minFrequency: typeof resp.minFrequency === 'number' ? resp.minFrequency : 20,
        maxFrequency: typeof resp.maxFrequency === 'number' ? resp.maxFrequency : null,
        streamReady: resp.streamReady ?? true,
    };
}

/**
 * `audio` — real-time audio analysis namespace.
 *
 * Spectrum / waveform subscriptions return an unsubscribe callback;
 * call it to stop receiving callbacks **and** notify the host so the
 * underlying compute pipeline is torn down.
 *
 * {@link audio.generateFullWaveform} resolves either synchronously
 * (cache hit) or asynchronously by listening for the
 * `audio:fullWaveformReady` / `audio:fullWaveformFailed` events with
 * a client-side timeout; a timeout or an aborted `signal` also cancels
 * the host task.
 */
export const audio = {
    /**
     * Subscribe to a real-time spectrum stream. The callback receives this
     * subscription's frames only; other subscriptions on the same event
     * name get their own.
     *
     * With `output: 'bins'` the callback receives the FFT's linear bins as
     * power in dB (see {@link SpectrumOutput}); band output is kept for
     * compatibility. A host without bin output registers band output
     * instead: `ready` then reports `output: 'bands'` and the callback
     * receives nothing.
     *
     * Returns an unsubscribe callback that detaches the listener and removes
     * the subscription on the host. Its `ready` promise settles with the
     * host's answer to the registration (`ok: false` with `INVALID_PARAMS`
     * for rejected parameters) and never rejects.
     *
     * @example
     *   const stop = fb.audio.subscribeSpectrum((frame) => {
     *       const values = frame.channels === 'stereo' ? frame.left : frame.spectrum;
     *       draw(frame.firstBin, values);
     *   }, { output: 'bins', fftSize: 16384, backgroundThrottle: false });
     *   const outcome = await stop.ready;
     *   if (!outcome.ok || outcome.output !== 'bins') console.warn('no bin output');
     */
    subscribeSpectrum: ((
        callback: SpectrumCallback | SpectrumBinsCallback,
        options: AudioSubscribeSpectrumParams = {},
    ): SpectrumSubscription => {
        const subscriptionId = newSubscriptionId();
        const eventName = options.event || 'audio:spectrum';
        const requested = {
            subscriptionId,
            fftSize: options.fftSize || 1024,
            bands: options.bands || 48,
            fps: options.fps || 30,
        };
        const bins = options.output === 'bins';
        const ready = call('audio.subscribeSpectrum', {
                subscriptionId,
                fftSize: requested.fftSize,
                fps: requested.fps,
                // Bin output ignores the band count; leave it to the host default.
                bands: bins ? undefined : requested.bands,
                scale: options.scale,
                backgroundThrottle: options.backgroundThrottle,
                minFrequency: options.minFrequency,
                maxFrequency: options.maxFrequency,
                output: options.output,
                channels: options.channels,
                event: eventName,
            })
            .then((resp) => toSubscribeOutcome(resp, requested))
            .catch(
                (error: unknown): SpectrumSubscribeOutcome => ({
                    ok: false,
                    code: 'UNKNOWN_ERROR',
                    error: error instanceof Error ? error.message : String(error),
                }),
            );
        const unsub = bridge.on(eventName, (data: unknown) => {
            if (isForeignFrame(data, subscriptionId)) return;
            // The overloads pair a bin callback with bin output only, so the
            // callback matches whichever kind of frame passes this check.
            if (isBinsFrame(data)) {
                if (bins) (callback as SpectrumBinsCallback)(data);
            } else if (!bins) {
                (callback as SpectrumCallback)(data as AudioSpectrumPayload);
            }
        });
        const unsubscribe = (): void => {
            unsub();
            // Nothing is left to undo if the host call fails; swallow it so the
            // rejection does not surface as unhandled.
            call('audio.unsubscribeSpectrum', {
                    subscriptionId,
                })
                .catch(() => undefined);
        };
        return Object.assign(unsubscribe, { ready });
    }) as SpectrumSubscribeFunction,

    /**
     * Subscribe to the audio foobar2000 is playing and get a {@link PcmStream}
     * to poll for frames.
     *
     * The samples are what the DSP chain produces, before ReplayGain and the
     * volume control, so they differ from `audio:spectrum` in not including
     * ReplayGain. They live in read-only shared memory; `read()` copies them
     * out per channel and reports frames the ring overwrote before they were
     * read. Nothing arrives while playback is stopped or paused. Poll from
     * `requestAnimationFrame` or a timer and call `unsubscribe()` when done.
     *
     * `opts.interval` asks the core for a callback interval in seconds; the
     * core rounds it to about 16 ms steps, never exceeds 200 ms (its default),
     * and applies the shortest interval any subscription in the component
     * asked for. `opts.bufferSeconds` sizes the ring (default 1 s; the first
     * chunk after playback starts can hold up to 0.8 s of audio).
     *
     * The handle's `ready` resolves with the host's answer and never rejects;
     * `code` is `NOT_SUPPORTED` outside a WebView2 host or on a runtime without
     * shared buffers, `OPERATION_FAILED` when the page already has 8 stream
     * subscriptions.
     *
     * @example
     *   const stream = fb.audio.subscribeStream({ bufferSeconds: 0.5 });
     *   const tick = () => {
     *       const chunk = stream.read();
     *       if (chunk) meter.push(chunk.planes[0], chunk.dropped);
     *       if (!stream.ended) requestAnimationFrame(tick);
     *   };
     *   requestAnimationFrame(tick);
     * @experimental
     */
    subscribeStream: (opts?: PcmStreamOptions): PcmStream => subscribeStream(opts),

    /**
     * Remove this page's stream subscriptions on the host: the one with
     * `subscriptionId`, or all of them when omitted. {@link PcmStream.unsubscribe}
     * does this for its own subscription; call this directly only for
     * subscriptions made with a raw `invoke`. Resolves with the number removed.
     * @experimental
     */
    unsubscribeStream: (subscriptionId?: string) =>
        call(
            'audio.unsubscribeStream',
            subscriptionId === undefined ? {} : { subscriptionId },
        ),

    /**
     * Single-shot poll of the spectrum; requires at least one active
     * {@link audio.subscribeSpectrum} subscription on the host side. With
     * `subscriptionId` the frame uses that subscription's parameters.
     * Paused or stopped playback answers with a silence frame; a stream
     * that has not produced data yet answers `success: false`.
     */
    getSpectrum: (options: AudioGetSpectrumParams = {}) =>
        call('audio.getSpectrum', options),

    /**
     * Short-window waveform of the current playback stream. Accepts
     * either `(opts)` (preferred) or `(path, opts)` (deprecated form;
     * the path is ignored because the host always uses the active
     * stream).
     */
    getWaveform: (
        pathOrOpts?: string | AudioGetWaveformParams,
        opts?: AudioGetWaveformParams,
    ) => {
        if (typeof pathOrOpts === 'string') {
            return call(
                'audio.getWaveform',
                opts || {},
            );
        }
        return call(
            'audio.getWaveform',
            pathOrOpts || {},
        );
    },
    getOutputInfo: () =>
        call('audio.getOutputInfo'),
    getStreamInfo: () =>
        call('audio.getStreamInfo'),
    /**
     * Reads the track's `BPM` tag; the host does not detect tempo. Resolves
     * `success: false` with `code: 'NOT_FOUND'` when the tag is missing or is
     * not a number between 0 and 500 (exclusive).
     */
    analyzeBPM: (path: string) =>
        call('audio.analyzeBPM', { path }),
    isVisualizationAvailable: () =>
        call('audio.isVisualizationAvailable'),
    /**
     * Choose the channels of the visualisation stream behind spectrum frames and
     * `getWaveform`; what is heard does not change.
     */
    setChannelMode: (mode: NonNullable<AudioSetChannelModeParams['mode']>) =>
        call('audio.setChannelMode', { mode }),
    getSpectrumDebugState: () =>
        call('audio.getSpectrumDebugState'),

    /**
     * Generate a full-track waveform. Resolves synchronously on cache
     * hit; otherwise awaits the
     * `audio:fullWaveformReady` / `audio:fullWaveformFailed` events.
     *
     * `opts.timeout` (default 60 s, `0` waits forever) and `opts.signal`
     * end the wait early and cancel the host task through
     * {@link audio.cancelFullWaveform}; neither is sent to the host. An
     * aborted signal rejects with a `DOMException` named `AbortError`, a
     * timeout with `{ success: false, error: 'TIMEOUT' }`. A cache hit
     * resolves whatever the signal does.
     *
     * Failures take three shapes: a request the host refuses up front
     * (bad path, permission, shutdown) resolves with
     * `{ success: false, error, code }`; a decode that fails later rejects
     * with the `audio:fullWaveformFailed` payload; a bridge-level error
     * rejects with an `Error`. The host decodes two tracks at a time and
     * queues the rest; time spent in its queue counts toward the timeout.
     */
    generateFullWaveform: async (
        path: string,
        opts: FullWaveformOptions & { timeout?: number; signal?: AbortSignal } = {},
    ): Promise<FullWaveformResult> => {
        const { timeout, signal, ...params } = opts;
        if (signal?.aborted) throw abortError();

        // path last: an untyped caller's opts.path must not override the argument.
        const request: AudioGenerateFullWaveformParams = { ...params, path };
        const result = await call('audio.generateFullWaveform', request);

        // A ready answer, a refusal, and anything else that is not pending go back as is.
        if (!result || result.success === false || result.status !== 'pending') return result;

        const taskId = result.taskId;
        const cancelHostTask = (): void => {
            if (typeof taskId !== 'string') return;
            // Fire and forget: the host answers cancelled false once the task
            // has settled, and there is nothing to retry either way.
            call('audio.cancelFullWaveform', { taskId })
                .catch(() => undefined);
        };
        if (signal?.aborted) {
            cancelHostTask();
            throw abortError();
        }

        const timeoutMs = timeout ?? 60000;
        return new Promise<FullWaveformResult>((resolve, reject) => {
            let timer: ReturnType<typeof setTimeout> | null = null;
            let settled = false;
            // Detaches everything before any settle, so the CANCELLED event the
            // host sends for our own cancel finds no listener.
            const settle = (): boolean => {
                if (settled) return false;
                settled = true;
                offReady();
                offFail();
                if (timer) clearTimeout(timer);
                signal?.removeEventListener('abort', onAbort);
                return true;
            };
            const offReady = subscribe(
                'audio:fullWaveformReady',
                (e: FullWaveformReadyEvent) => {
                    if (e?.taskId !== taskId || !settle()) return;
                    resolve({ success: true, status: 'ready', ...e });
                },
            );
            const offFail = subscribe(
                'audio:fullWaveformFailed',
                (e: FullWaveformFailedEvent) => {
                    if (e?.taskId !== taskId || !settle()) return;
                    reject(e);
                },
            );
            const onAbort = (): void => {
                if (!settle()) return;
                cancelHostTask();
                reject(abortError());
            };
            signal?.addEventListener('abort', onAbort, { once: true });
            if (timeoutMs > 0) {
                timer = setTimeout(() => {
                    if (!settle()) return;
                    cancelHostTask();
                    reject({
                        success: false,
                        error: 'TIMEOUT',
                        message: 'generateFullWaveform timed out after ' + timeoutMs + 'ms',
                        taskId,
                    });
                }, timeoutMs);
            }
        });
    },

    /**
     * Cancel a pending {@link audio.generateFullWaveform} request by the
     * `taskId` of its `pending` answer. Only the caller that made the
     * request can cancel it: `cancelled: false` means the task has already
     * settled, does not exist or belongs to another caller. A cancelled
     * task receives one `audio:fullWaveformFailed` with
     * `code: 'CANCELLED'`, which may arrive before this answer.
     */
    cancelFullWaveform: (taskId: string) =>
        call('audio.cancelFullWaveform', { taskId }),

    /**
     * Decode a track, or a range of it, into float32 PCM and resolve with a
     * {@link PcmBuffer} that reads the samples in place.
     *
     * The samples live in read-only shared memory: writing through
     * `getChannelView()` crashes the renderer process, so copy them with
     * `toAudioBuffer()` when you need to change them, and call `release()` when
     * done. One task decodes at a time and the rest queue; identical requests
     * in flight share one decode.
     *
     * Rejects with `PcmDecodeError` carrying the host's `code` when the host
     * refuses the request (`INVALID_PARAMS` also covers a range too large for
     * one buffer: 256 MiB in 64-bit foobar2000, 64 MiB in 32-bit) or the task
     * fails later; `NOT_SUPPORTED` outside a WebView2 host. `opts.signal` and
     * `opts.timeoutMs` (default 120 s, `0` waits forever) cancel the host task
     * and reject, with an `AbortError` `DOMException` and code `TIMEOUT`
     * respectively.
     *
     * @example
     *   const pcm = await fb.audio.decodePcm(path, { start: 30, end: 60, sampleRate: 22050, mono: true });
     *   try {
     *       analyse(pcm.getChannelView(0), pcm.sampleRate);
     *   } finally {
     *       pcm.release();
     *   }
     * @experimental
     */
    decodePcm: (path: string, opts?: DecodePcmOptions): Promise<PcmBuffer> => decodePcm(path, opts),

    /**
     * Cancel a pending `audio.decodePcm` task by its `taskId`. The SDK's
     * {@link audio.decodePcm} does this for you on abort and timeout; call it
     * directly only for tasks started with a raw `invoke`. `cancelled: false`
     * means the task has already settled, does not exist or belongs to another
     * page.
     * @experimental
     */
    cancelDecodePcm: (taskId: string) =>
        call('audio.cancelDecodePcm', { taskId }),

    /**
     * Shared-buffer support of this page and the state of the decode queue, for tests and diagnostics.
     * @experimental
     */
    getPcmDebugState: () => call('audio.getPcmDebugState'),
};
