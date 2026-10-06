/**
 * Hand-written parameter and response shapes for the `audio.*` namespace.
 */

import type { JsonObject } from '../json.js';

/**
 * Band value scale of a spectrum subscription.
 *
 * - `'weighted'` — display curve in `[0, 1]`: triangular band filter,
 *   tilt and bass shelf, mapped from -50..0 dB with gamma 0.8.
 * - `'db'` — band power in dB: the sum of the bin powers in the band, with
 *   a full-scale sine reading 0 dB and a floor of `-160`. The total power of
 *   a signal does not depend on the band count, except in low bands that
 *   are narrower than one FFT bin.
 */
export type SpectrumScale = 'weighted' | 'db';

/** Playback state stamped into a spectrum frame. */
export type SpectrumFrameState = 'playing' | 'paused' | 'stopped';

/**
 * Frame fields shared by `audio:spectrum` events and the `audio.getSpectrum`
 * response, apart from `spectrum` itself. All optional: older hosts send
 * `spectrum` only.
 */
export interface SpectrumFrameInfo {
    /** Subscription the frame belongs to; absent in `getSpectrum` answers without one. */
    subscriptionId?: string;
    /** Length of `spectrum`. */
    bands?: number;
    /** FFT size actually used, after the automatic raise for large band counts. */
    fftSize?: number;
    scale?: SpectrumScale;
    /** Sample rate of the visualisation stream in Hz; `0` when unknown. */
    sampleRate?: number;
    /** Lower edge of the lowest band in Hz: the subscription's `minFrequency`, `20` by default. */
    minFrequency?: number;
    /**
     * Upper edge of the highest band in Hz: the subscription's `maxFrequency`
     * capped at `sampleRate / 2`, or `sampleRate / 2` when none was given;
     * `0` when the sample rate is unknown.
     */
    maxFrequency?: number;
    /**
     * `'paused'` / `'stopped'` frames are single silence frames sent when
     * playback leaves the playing state; no frames follow until playback
     * resumes.
     */
    state?: SpectrumFrameState;
    /**
     * Seconds since the visualisation stream last started. Starting playback,
     * seeking and manual track changes restart it at `0`, where it stays for
     * about 200 ms (those frames repeat the same audio); natural track
     * transitions do not. A frame is new when this value changed.
     */
    streamTime?: number;
    /**
     * Host system time when the frame was computed, Unix epoch milliseconds
     * with a fractional part. Compare with `Date.now()`; the page's
     * `performance.timeOrigin + performance.now()` drifts away from system
     * time the longer the page stays open.
     */
    hostTime?: number;
}

/**
 * Parameters for `audio.subscribeSpectrum`. Every subscription keeps its own
 * values; frames report the values actually used.
 *
 * @codegen-override params:audio.subscribeSpectrum
 * @codegen-snapshot backgroundThrottle:primitive,bands:primitive,event:primitive,fftSize:primitive,fps:primitive,maxFrequency:unknown,minFrequency:unknown,scale:primitive,subscriptionId:primitive
 */
export interface AudioSubscribeSpectrumParams {
    /** Power of two in `[256, 65536]`; other values fail with `INVALID_PARAMS`. @default 1024 */
    fftSize?: number;
    /** Event name the frames are sent under. @default "audio:spectrum" */
    event?: string;
    /** Frames per second, clamped to `[1, 60]`. @default 30 */
    fps?: number;
    /** @default "weighted" */
    scale?: SpectrumScale;
    /**
     * `false` keeps the full frame rate while another application is in the
     * foreground; `true` limits the subscription to at most 12 frames per
     * second then, usually 10 to 12. @default true
     */
    backgroundThrottle?: boolean;
    /** Clamped to `[8, fftSize / 2]`. @default 48 */
    bands?: number;
    /**
     * Lower edge of the band range in Hz; bands divide the range
     * logarithmically. A finite number of at least 1, otherwise
     * `INVALID_PARAMS`. @default 20
     */
    minFrequency?: number;
    /**
     * Upper edge of the band range in Hz, greater than `minFrequency`
     * (otherwise `INVALID_PARAMS`) and capped at half the stream's sample
     * rate when frames are computed. Follows half the sample rate when
     * omitted. A range entirely above that frequency yields silence frames.
     */
    maxFrequency?: number;
    /**
     * Key of the subscription; subscribing again with the same key replaces
     * it. Derived from the calling window and `event` when omitted.
     * `audio.subscribeSpectrum` always sends a fresh one.
     */
    subscriptionId?: string;
}

/**
 * Response from `audio.subscribeSpectrum`: the registered values, echoing the
 * request. Rejected parameters register nothing and carry `code` and
 * `details` instead.
 *
 * @codegen-override response:audio.subscribeSpectrum
 * @codegen-snapshot backgroundThrottle:primitive,bands:primitive,code:unknown,details:object,error:unknown,event:primitive,fftSize:primitive,fps:primitive,maxFrequency:object,minFrequency:primitive,scale:primitive,streamReady:primitive,subscriptionId:primitive,success:primitive
 */
export interface AudioSubscribeSpectrumResponse {
    success: boolean;
    error?: string;
    /** `INVALID_PARAMS` for rejected parameters. */
    code?: string;
    /** The offending parameter and value. */
    details?: JsonObject;
    subscriptionId?: string;
    /** Requested FFT size; frames report the size actually used. */
    fftSize?: number;
    bands?: number;
    fps?: number;
    scale?: SpectrumScale;
    backgroundThrottle?: boolean;
    minFrequency?: number;
    /** `null` when the range follows half the stream's sample rate. */
    maxFrequency?: number | null;
    event?: string;
    /** `true` once the visualisation stream exists, also while stopped. */
    streamReady?: boolean;
}

/**
 * Parameters for `audio.getSpectrum`.
 *
 * @codegen-override params:audio.getSpectrum
 * @codegen-snapshot bands:primitive,maxFrequency:unknown,minFrequency:unknown,scale:primitive,subscriptionId:primitive
 */
export interface AudioGetSpectrumParams {
    /**
     * Compute the frame with this subscription's parameters; `bands`,
     * `scale` and the frequency range are then ignored. Unknown ids fail
     * with `NOT_FOUND`.
     */
    subscriptionId?: string;
    /**
     * Band count when no `subscriptionId` is given; `0` takes the largest
     * among all subscriptions. Capped at `fftSize / 2`. @default 0
     */
    bands?: number;
    /** Scale when no `subscriptionId` is given. @default "weighted" */
    scale?: SpectrumScale;
    /** Band range when no `subscriptionId` is given; same rules as `audio.subscribeSpectrum`. @default 20 */
    minFrequency?: number;
    /** Band range when no `subscriptionId` is given; same rules as `audio.subscribeSpectrum`. */
    maxFrequency?: number;
}

/**
 * Response from `audio.getSpectrum`: one spectrum frame plus `success`.
 *
 * Paused or stopped playback answers with a silence frame. Failures (no
 * subscription, stream not producing data yet) carry `error` instead of a
 * frame; parameter errors add `code` and `details`. Older hosts answer with
 * `spectrum`, `fftSize` and `bands` only.
 *
 * @codegen-override response:audio.getSpectrum
 * @codegen-snapshot bands:callexpr,code:unknown,details:object,error:primitive,fftSize:callexpr,hostTime:callexpr,maxFrequency:callexpr,minFrequency:callexpr,sampleRate:callexpr,scale:callexpr,spectrum:callexpr,state:callexpr,streamTime:callexpr,subscriptionId:callexpr,success:primitive
 */
export interface AudioGetSpectrumResponse extends SpectrumFrameInfo {
    success: boolean;
    error?: string;
    /** `INVALID_PARAMS` or `NOT_FOUND` (unknown `subscriptionId`). */
    code?: string;
    /** The offending parameter and value. */
    details?: JsonObject;
    /** One value per band, in the frame's `scale`; floor values while paused or stopped. */
    spectrum?: number[];
}

/** Channel layout of an `audio.getWaveform` answer. */
export type WaveformChannels = 'mix' | 'stereo';

/**
 * Parameters for `audio.getWaveform`. The window starts at the current
 * playback position and covers audio already buffered for output.
 *
 * @codegen-override params:audio.getWaveform
 * @codegen-snapshot channels:primitive,duration:primitive,points:primitive,signed:primitive
 */
export interface AudioGetWaveformParams {
    /**
     * Window length in seconds, greater than 0 and at most 1; other values
     * fail with `INVALID_PARAMS`. A window longer than the audio buffered for
     * output fails like a missing stream. @default 0.05
     */
    duration?: number;
    /**
     * `true` keeps the sign, clamped to `[-1, 1]`; `false` maps the magnitude
     * from -70..0 dB onto `[0, 1]`. @default false
     */
    signed?: boolean;
    /**
     * `'mix'` averages the channels into `waveform`; `'stereo'` returns the
     * first two channels as `left` / `right`. @default "mix"
     */
    channels?: WaveformChannels;
    /**
     * Thin the window to this many evenly spaced samples, without averaging;
     * an integer in `[2, 65536]`. Every sample is returned when omitted.
     */
    points?: number;
}

/**
 * Response from `audio.getWaveform`. Without a spectrum subscription, or
 * when the stream has no data for the window, `success` is false with
 * `error`; rejected parameters add `code` and `details`. Older hosts answer
 * with `waveform`, `duration` and `signed` only.
 *
 * @codegen-override response:audio.getWaveform
 * @codegen-snapshot channelCount:unknown,channels:primitive,code:unknown,details:object,duration:primitive,error:unknown,left:array,right:array,sampleRate:unknown,signed:primitive,success:primitive,waveform:array
 */
export interface AudioGetWaveformResponse {
    success: boolean;
    error?: string;
    /** `INVALID_PARAMS` for rejected parameters. */
    code?: string;
    /** The offending parameter and value. */
    details?: JsonObject;
    /** `'mix'` answers: one value per sample. */
    waveform?: number[];
    /** `'stereo'` answers: the first channel. */
    left?: number[];
    /** `'stereo'` answers: the second channel; a copy of `left` when the stream has one channel. */
    right?: number[];
    /** Window length in seconds, as requested. */
    duration?: number;
    signed?: boolean;
    channels?: WaveformChannels;
    /** Sample rate of the visualisation stream in Hz. */
    sampleRate?: number;
    /** Channel count of the visualisation stream; `1` after `audio.setChannelMode('mono')`. */
    channelCount?: number;
}

/** Timer behind pushed spectrum frames: a high-resolution waitable timer, or the standard one where that is unavailable. */
export type SpectrumBeatSource = 'high-resolution' | 'standard';

/**
 * Response from `audio.getSpectrumDebugState`: the spectrum runtime as the
 * host sees it. Diagnostic only; the shape is not a stable contract.
 *
 * @codegen-override response:audio.getSpectrumDebugState
 * @codegen-snapshot active:primitive,beatIntervalMs:object,beatsCoalesced:callexpr,beatSource:object,callerHwnd:primitive,callerOwnsSubscription:primitive,callerWindowId:primitive,dispatchTargetCount:callexpr,dispatchTargets:object,effectiveBands:primitive,effectiveFftSize:primitive,effectiveFps:primitive,foregroundHwnd:primitive,foregroundIsExternal:primitive,foregroundPid:unknown,foregroundTitle:primitive,framesComputed:callexpr,instanceCount:primitive,skipFrames:primitive,streamReady:primitive,subscriptionCount:callexpr,subscriptions:object,success:primitive,timerHwnd:primitive,timerRunning:primitive
 */
export interface AudioGetSpectrumDebugStateResponse {
    success: boolean;
    active: boolean;
    /** `true` while the beat thread that pushes frames is running. */
    timerRunning: boolean;
    /** @deprecated Always `0`: frames are no longer driven by a window timer. */
    timerHwnd: number;
    /** `null` while the beat thread is not running. */
    beatSource: SpectrumBeatSource | null;
    /** Current beat length in milliseconds (1000 / the highest `fps`); `null` while the beat thread is not running. */
    beatIntervalMs: number | null;
    /** Beats dropped because the previous one had not reached the main thread yet; cumulative. */
    beatsCoalesced: number;
    effectiveFftSize: number;
    effectiveFps: number;
    effectiveBands: number;
    skipFrames: number;
    framesComputed: number;
    streamReady: boolean;
    subscriptionCount: number;
    dispatchTargetCount: number;
    subscriptions: JsonObject[];
    dispatchTargets: JsonObject[];
    instanceCount: number;
    callerHwnd: number;
    callerWindowId: string;
    callerOwnsSubscription: boolean;
    foregroundHwnd: number;
    foregroundPid: number;
    foregroundIsExternal: boolean;
    foregroundTitle: string;
}
