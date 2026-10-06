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

import { bridge } from '../Bridge.js';
import type {
    AudioGenerateWaveformResponse,
    AudioGetWaveformResponse,
    AudioOutputInfoResponse,
    AudioStreamInfoResponse,
    BaseResponse,
    FullWaveformOptions,
    FullWaveformResult,
    SpectrumDebugState,
    SpectrumSubscribeOutcome,
    SpectrumSubscription,
} from '../../types/responses.js';
import type {
    AudioSpectrumPayload,
    FullWaveformFailedEvent,
    FullWaveformReadyEvent,
} from '../../types/events.js';
import type {
    AudioAnalyzeBPMParams,
    AudioGenerateFullWaveformParams,
    AudioGenerateWaveformParams,
    AudioGetSpectrumParams,
    AudioGetWaveformParams,
    AudioSubscribeSpectrumParams,
    AudioSubscribeStreamParams,
} from '../../types/generated/params.js';
import type {
    AudioCancelFullWaveformResponse,
    AudioGetSpectrumResponse,
    AudioSubscribeSpectrumResponse,
    AudioUnsubscribeSpectrumResponse,
    AudioUnsubscribeStreamResponse,
} from '../../types/generated/responses.js';

/** @deprecated Use `AudioSubscribeSpectrumParams`. */
export type SubscribeSpectrumOptions = AudioSubscribeSpectrumParams;

type SpectrumCallback = (data: AudioSpectrumPayload) => void;
type StreamCallback = (data: unknown) => void;

/** @deprecated Use `AudioSubscribeStreamParams`. */
export type AudioStreamOptions = AudioSubscribeStreamParams;

/** @deprecated Use `AudioGetSpectrumParams`. */
export type GetSpectrumOptions = AudioGetSpectrumParams;

/** @deprecated Use `AudioGetWaveformParams`. */
export type GetWaveformOptions = AudioGetWaveformParams;

/** @deprecated Use `Omit<AudioAnalyzeBPMParams, 'path'>`. */
export type AnalyzeBpmOptions = Omit<AudioAnalyzeBPMParams, 'path'>;

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
    if ('mock' in resp && resp.mock === true) {
        return { ok: false, code: 'NOT_SUPPORTED', error: 'No foobar2000 host is available' };
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
        backgroundThrottle: resp.backgroundThrottle ?? true,
        minFrequency: typeof resp.minFrequency === 'number' ? resp.minFrequency : 20,
        maxFrequency: typeof resp.maxFrequency === 'number' ? resp.maxFrequency : null,
        streamReady: resp.streamReady ?? true,
    };
}

export const audio = {
    /**
     * Subscribe to a real-time spectrum stream. The callback receives this
     * subscription's frames only; other subscriptions on the same event
     * name get their own.
     *
     * Returns an unsubscribe callback that detaches the listener and removes
     * the subscription on the host. Its `ready` promise settles with the
     * host's answer to the registration (`ok: false` with `INVALID_PARAMS`
     * for rejected parameters) and never rejects.
     *
     * @example
     *   const stop = fb.audio.subscribeSpectrum((frame) => draw(frame.spectrum), {
     *       bands: 64, scale: 'db', backgroundThrottle: false,
     *   });
     *   const outcome = await stop.ready;
     *   if (!outcome.ok) console.warn(outcome.code, outcome.error);
     */
    subscribeSpectrum: (
        callback: SpectrumCallback,
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
        const ready = bridge
            .invoke<AudioSubscribeSpectrumResponse>('audio.subscribeSpectrum', {
                subscriptionId,
                fftSize: requested.fftSize,
                fps: requested.fps,
                bands: requested.bands,
                scale: options.scale,
                backgroundThrottle: options.backgroundThrottle,
                minFrequency: options.minFrequency,
                maxFrequency: options.maxFrequency,
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
            callback(data as AudioSpectrumPayload);
        });
        const unsubscribe = (): void => {
            unsub();
            // Nothing is left to undo if the host call fails; swallow it so the
            // rejection does not surface as unhandled.
            bridge
                .invoke<AudioUnsubscribeSpectrumResponse>('audio.unsubscribeSpectrum', {
                    subscriptionId,
                })
                .catch(() => undefined);
        };
        return Object.assign(unsubscribe, { ready });
    },

    /**
     * Subscribe to the raw audio-stream callback channel.
     *
     * @deprecated Host-side stream capture is **not implemented yet**:
     * the C++ handler at `src/api/AudioApi.cpp:AudioSubscribeStream`
     * currently returns
     * `{success: false, error: "Stream capture requires
     * playback_stream_capture integration"}` without ever emitting an
     * `audio:stream` event. The returned unsubscribe is still wired
     * up so callers can fail gracefully, but the supplied `callback`
     * **will never fire** until the host integration lands. The SDK
     * surfaces a one-shot `console.warn` from the underlying
     * `bridge.invoke` resolution to make this state visible.
     *
     * Track the integration status before re-enabling consumer code.
     */
    subscribeStream: (
        callback: StreamCallback,
        options: AudioSubscribeStreamParams = {},
    ): (() => void) => {
        bridge
            .invoke<BaseResponse>('audio.subscribeStream', options)
            .then((resp) => {
                if (resp && resp.success === false) {
                    const detail = resp.error
                        ? ` (${resp.error})`
                        : '';
                    // eslint-disable-next-line no-console
                    console.warn(
                        '[fb-sdk] audio.subscribeStream: host returned ' +
                            'success=false; the callback will never fire' +
                            detail +
                            '.',
                    );
                }
            })
            .catch(() => {
                /* swallow — bridge layer already handles transport errors */
            });
        const unsub = bridge.on('audio:stream', callback);
        return () => {
            unsub();
            bridge
                .invoke<AudioUnsubscribeStreamResponse>('audio.unsubscribeStream')
                .catch(() => undefined);
        };
    },
    /** @deprecated Pairs with the deprecated {@link audio.subscribeStream}. */
    unsubscribeStream: () =>
        bridge.invoke<BaseResponse>('audio.unsubscribeStream'),

    /**
     * Single-shot poll of the spectrum; requires at least one active
     * {@link audio.subscribeSpectrum} subscription on the host side. With
     * `subscriptionId` the frame uses that subscription's parameters.
     * Paused or stopped playback answers with a silence frame; a stream
     * that has not produced data yet answers `success: false`.
     */
    getSpectrum: (options: AudioGetSpectrumParams = {}) =>
        bridge.invoke<AudioGetSpectrumResponse>('audio.getSpectrum', options),

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
            return bridge.invoke<AudioGetWaveformResponse>(
                'audio.getWaveform',
                opts || {},
            );
        }
        return bridge.invoke<AudioGetWaveformResponse>(
            'audio.getWaveform',
            pathOrOpts || {},
        );
    },
    getOutputInfo: () =>
        bridge.invoke<AudioOutputInfoResponse>('audio.getOutputInfo'),
    getStreamInfo: () =>
        bridge.invoke<AudioStreamInfoResponse>('audio.getStreamInfo'),
    analyzeBPM: (
        path: string,
        opts: Omit<AudioAnalyzeBPMParams, 'path'> = {},
    ) =>
        bridge.invoke<{ bpm: number }>('audio.analyzeBPM', { path, ...opts }),
    isVisualizationAvailable: () =>
        bridge.invoke<{ available: boolean }>('audio.isVisualizationAvailable'),
    setChannelMode: (mode: string) =>
        bridge.invoke<BaseResponse>('audio.setChannelMode', { mode }),

    /** @deprecated Use {@link audio.generateFullWaveform}. */
    generateWaveform: (
        path: string,
        opts?: Omit<AudioGenerateWaveformParams, 'path'>,
    ) =>
        bridge.invoke<AudioGenerateWaveformResponse>(
            'audio.generateWaveform',
            { path, ...(opts || {}) },
        ),
    getSpectrumDebugState: () =>
        bridge.invoke<SpectrumDebugState>('audio.getSpectrumDebugState'),

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
        const result = await bridge.invoke<FullWaveformResult>(
            'audio.generateFullWaveform',
            request,
        );

        if (result?.status === 'ready') return result;
        if (result?.success === false) return result;
        if (result?.status !== 'pending') return result;

        const taskId = result.taskId;
        const cancelHostTask = (): void => {
            if (typeof taskId !== 'string') return;
            // Fire and forget: the host answers cancelled false once the task
            // has settled, and there is nothing to retry either way.
            bridge
                .invoke<AudioCancelFullWaveformResponse>('audio.cancelFullWaveform', { taskId })
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
            const offReady = bridge.on(
                'audio:fullWaveformReady',
                (e: FullWaveformReadyEvent) => {
                    if (e?.taskId !== taskId || !settle()) return;
                    resolve({ success: true, status: 'ready', ...e });
                },
            );
            const offFail = bridge.on(
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
        bridge.invoke<AudioCancelFullWaveformResponse>('audio.cancelFullWaveform', { taskId }),
};
