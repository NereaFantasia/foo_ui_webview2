/**
 * Spectrum frame shapes of the `audio:spectrum` event and the value types they share with the
 * `audio.*` parameters.
 */

/**
 * Band value scale of a spectrum subscription. Band output is kept for
 * compatibility; bin output (see {@link SpectrumOutput}) is always `'db'`.
 *
 * - `'weighted'` — display curve in `[0, 1]`: triangular band filter,
 *   tilt and bass shelf, mapped from -50..0 dB with gamma 0.8.
 * - `'db'` — band power in dB: the sum of the bin powers in the band, with
 *   a full-scale sine reading 0 dB and a floor of `-160`. The total power of
 *   a signal does not depend on the band count, except in low bands that
 *   are narrower than one FFT bin.
 */
export type SpectrumScale = 'weighted' | 'db';

/**
 * What a spectrum subscription delivers.
 *
 * - `'bands'` — log-spaced bands in the subscription's `scale`, with the
 *   host's interpolation of narrow low bands and its automatic FFT-size
 *   raise for large band counts. Kept for compatibility.
 * - `'bins'` — the FFT's linear bins as power in dB, at the requested FFT
 *   size, rounded to 0.01 dB, floor `-160`. The powers of the bins a
 *   full-scale sine's main lobe covers sum to 0 dB, so summing bins as
 *   powers reproduces the `'db'` band values. Value `i` of a frame is bin
 *   `firstBin + i`, centred at `(firstBin + i) * sampleRate / fftSize` Hz.
 */
export type SpectrumOutput = 'bands' | 'bins';

/**
 * Channel layout of bin output. `'mix'` averages the power of all channels
 * into `spectrum`; `'stereo'` delivers the stream's first two channels as
 * `left` and `right`, with `right` repeating `left` for a mono stream.
 */
export type SpectrumChannels = 'mix' | 'stereo';

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
    /** What the frame carries. Older hosts send band frames without this field. */
    output?: SpectrumOutput;
    /** Length of `spectrum` in a band frame; bin frames do not carry it. */
    bands?: number;
    /**
     * FFT size actually used: for band frames after the automatic raise for
     * large band counts, for bin frames the requested size.
     */
    fftSize?: number;
    /** Bin frames always report `'db'`. */
    scale?: SpectrumScale;
    /** Channel layout of a bin frame. */
    channels?: SpectrumChannels;
    /**
     * Channel count of the visualisation stream, sent with bin frames; `0`
     * when unknown. With `channels: 'stereo'` and a count of 1, `right`
     * repeats `left`.
     */
    channelCount?: number;
    /** Bin index of the first value in a bin frame; `0` when the range holds no bin. */
    firstBin?: number;
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
     * Seconds since the visualisation stream last started; the frame is
     * computed from the `fftSize` samples ending at this time. Starting
     * playback, seeking and manual track changes restart it at `0`, where it
     * stays for about 200 ms (silence frames); until `fftSize / sampleRate`
     * seconds have passed, the part of the window before the restart is
     * silence. Natural track transitions do not restart it. A frame is new
     * when this value changed.
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
 * Frame of a subscription with `output: 'bins'` and `channels: 'mix'`,
 * delivered on the subscription's event and answered by `audio.getSpectrum`
 * for it. A silence frame keeps the previous frame's length, filled with
 * `-160`, and is empty when no frame came before it.
 */
export interface SpectrumMixBinsFrame extends SpectrumFrameInfo {
    output: 'bins';
    channels: 'mix';
    firstBin: number;
    /** Power of all channels in dB, one value per bin from bin `firstBin` on. */
    spectrum: number[];
}

/** Frame of a subscription with `output: 'bins'` and `channels: 'stereo'`; see {@link SpectrumMixBinsFrame}. */
export interface SpectrumStereoBinsFrame extends SpectrumFrameInfo {
    output: 'bins';
    channels: 'stereo';
    firstBin: number;
    /** First channel in dB, one value per bin from bin `firstBin` on. */
    left: number[];
    /** Second channel in dB, or the first again when `channelCount` is 1. */
    right: number[];
}

/** Frame of a bin subscription; `channels` tells the two layouts apart. */
export type SpectrumBinsFrame = SpectrumMixBinsFrame | SpectrumStereoBinsFrame;

/** Channel layout of an `audio.getWaveform` answer. */
export type WaveformChannels = 'mix' | 'stereo';

/** Timer behind pushed spectrum frames: a high-resolution waitable timer, or the standard one where that is unavailable. */
export type SpectrumBeatSource = 'high-resolution' | 'standard';
