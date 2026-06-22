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
 * a client-side timeout.
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
} from '../../types/responses.js';
import type {
    AudioSpectrumPayload,
    FullWaveformFailedEvent,
    FullWaveformReadyEvent,
} from '../../types/events.js';
import type {
    AudioAnalyzeBPMParams,
    AudioGenerateWaveformParams,
    AudioGetSpectrumParams,
    AudioGetWaveformParams,
    AudioSubscribeSpectrumParams,
    AudioSubscribeStreamParams,
} from '../../types/generated/params.js';

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

export const audio = {
    /**
     * Subscribe to a real-time spectrum stream. Returns an unsubscribe
     * callback that detaches the listener and tells the host to tear
     * down the underlying compute pipeline.
     */
    subscribeSpectrum: (
        callback: SpectrumCallback,
        options: AudioSubscribeSpectrumParams = {},
    ): (() => void) => {
        const subscriptionId = newSubscriptionId();
        const eventName = options.event || 'audio:spectrum';
        bridge.invoke('audio.subscribeSpectrum', {
            subscriptionId,
            fftSize: options.fftSize || 1024,
            fps: options.fps || 30,
            bands: options.bands || 48,
            event: eventName,
        });
        const unsub = bridge.on(eventName, callback as (data: unknown) => void);
        return () => {
            unsub();
            bridge.invoke('audio.unsubscribeSpectrum', { subscriptionId });
        };
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
            bridge.invoke('audio.unsubscribeStream');
        };
    },
    /** @deprecated Pairs with the deprecated {@link audio.subscribeStream}. */
    unsubscribeStream: () =>
        bridge.invoke<BaseResponse>('audio.unsubscribeStream'),

    /**
     * Single-shot poll of the spectrum buffer; requires an active
     * {@link audio.subscribeSpectrum} subscription on the host side.
     */
    getSpectrum: (options: AudioGetSpectrumParams = {}) =>
        bridge.invoke<AudioSpectrumPayload>('audio.getSpectrum', options),

    /**
     * Short-window waveform of the current playback stream. Accepts
     * either `(opts)` (preferred) or `(path, opts)` (legacy; the path
     * is ignored because the host always uses the active stream).
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
     * `audio:fullWaveformReady` / `audio:fullWaveformFailed` events
     * with a client-side timeout (default 60 s, override via
     * `opts.timeout`).
     */
    generateFullWaveform: async (
        path: string,
        opts: FullWaveformOptions & { timeout?: number } = {},
    ): Promise<FullWaveformResult> => {
        const result = await bridge.invoke<FullWaveformResult>(
            'audio.generateFullWaveform',
            { path, ...opts },
        );

        if (result?.status === 'ready') return result;
        if (result?.success === false) return result;

        if (result?.status === 'pending') {
            const timeoutMs = opts.timeout ?? 60000;
            return new Promise<FullWaveformResult>((resolve, reject) => {
                const taskId = result.taskId;
                let timer: ReturnType<typeof setTimeout> | null = null;
                const cleanup = (): void => {
                    offReady();
                    offFail();
                    if (timer) {
                        clearTimeout(timer);
                        timer = null;
                    }
                };
                const offReady = bridge.on(
                    'audio:fullWaveformReady',
                    (e: FullWaveformReadyEvent) => {
                        if (e?.taskId === taskId) {
                            cleanup();
                            resolve({
                                success: true,
                                status: 'ready',
                                ...e,
                            });
                        }
                    },
                );
                const offFail = bridge.on(
                    'audio:fullWaveformFailed',
                    (e: FullWaveformFailedEvent) => {
                        if (e?.taskId === taskId) {
                            cleanup();
                            reject(e);
                        }
                    },
                );
                if (timeoutMs > 0) {
                    timer = setTimeout(() => {
                        cleanup();
                        reject({
                            success: false,
                            error: 'TIMEOUT',
                            message:
                                'generateFullWaveform timed out after ' +
                                timeoutMs +
                                'ms',
                            taskId,
                        });
                    }, timeoutMs);
                }
            });
        }

        return result;
    },
};
