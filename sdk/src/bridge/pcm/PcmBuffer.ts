/**
 * Decoded PCM that `audio.decodePcm` hands to the page: float32 samples in
 * read-only shared memory, one run per channel.
 */

import {
    PCM_FLAG_RESAMPLED,
    PCM_FLAG_TRUNCATED,
    PCM_MODE_ONE_SHOT,
    PcmHeaderError,
    readPcmHeader,
} from './PcmHeader.js';

/**
 * Samples of one `audio.decodePcm` task, backed by the shared buffer the host
 * posted. Nothing is copied until you call {@link PcmBuffer.toAudioBuffer}.
 *
 * The memory is **read-only**: writing through a view from
 * {@link PcmBuffer.getChannelView} crashes the page's renderer process, and
 * with it the whole foobar2000 window. Call {@link PcmBuffer.release} when you
 * are done so the shared memory can be reclaimed.
 * @experimental
 */
export class PcmBuffer {
    /** Samples per second per channel. */
    readonly sampleRate: number;
    /** 1 when the request set `mono`. */
    readonly channels: number;
    /** Frames per channel. */
    readonly frames: number;
    /** `frames / sampleRate`, in seconds. */
    readonly duration: number;
    /** Track time of the first frame, in seconds. */
    readonly start: number;
    /**
     * The audio ran past the size estimated from the track length, or its
     * format changed partway; the samples end at the cut.
     */
    readonly truncated: boolean;
    /** Converted to `sampleRate` with foobar2000's resampler. */
    readonly resampled: boolean;

    private buffer: ArrayBuffer | null;
    private readonly capacityFrames: number;
    private readonly headerBytes: number;
    private readonly releaseBuffer: (buffer: ArrayBuffer) => void;

    /**
     * @param buffer - The one-shot buffer from a `sharedbufferreceived` event.
     * @param releaseBuffer - Frees the page's view, usually `chrome.webview.releaseBuffer`.
     * @throws {@link PcmHeaderError} when the buffer is not a one-shot PCM buffer this SDK can read.
     */
    constructor(buffer: ArrayBuffer, releaseBuffer: (buffer: ArrayBuffer) => void) {
        const header = readPcmHeader(buffer);
        if (header.mode !== PCM_MODE_ONE_SHOT) {
            throw new PcmHeaderError('bad-mode', 'Expected a one-shot PCM buffer');
        }
        if (header.writeFrames > header.capacityFrames || header.sampleRate === 0) {
            throw new PcmHeaderError('bad-size', 'PCM frame count exceeds the buffer capacity');
        }
        this.buffer = buffer;
        this.releaseBuffer = releaseBuffer;
        this.capacityFrames = header.capacityFrames;
        this.headerBytes = header.headerBytes;
        this.sampleRate = header.sampleRate;
        this.channels = header.channels;
        this.frames = header.writeFrames;
        this.duration = header.writeFrames / header.sampleRate;
        this.start = header.startSeconds;
        this.truncated = (header.flags & PCM_FLAG_TRUNCATED) !== 0;
        this.resampled = (header.flags & PCM_FLAG_RESAMPLED) !== 0;
    }

    /** `false` after {@link PcmBuffer.release} or {@link PcmBuffer.transfer}. */
    get available(): boolean {
        return this.buffer !== null;
    }

    /**
     * Zero-copy view of one channel, `frames` samples long.
     *
     * Treat it as read-only: writing to it crashes the renderer process. It is
     * deliberately not called `getChannelData`, whose Web Audio namesake returns
     * an array you may modify. Views stop working after
     * {@link PcmBuffer.release}, or once the buffer handed out by
     * {@link PcmBuffer.transfer} has actually been transferred.
     *
     * @param channel - 0-based channel index.
     * @throws `RangeError` for a channel outside `0` to `channels - 1`; `Error` once released or transferred.
     */
    getChannelView(channel: number): Float32Array<ArrayBuffer> {
        const buffer = this.requireBuffer();
        if (!Number.isInteger(channel) || channel < 0 || channel >= this.channels) {
            throw new RangeError(`Channel ${channel} is out of range for ${this.channels} channel(s)`);
        }
        return new Float32Array(buffer, this.headerBytes + channel * this.capacityFrames * 4, this.frames);
    }

    /**
     * Copy the samples into a new `AudioBuffer`, which you may modify and play.
     *
     * Needs a page with Web Audio; `sampleRate` must be one `AudioBuffer`
     * accepts (Chromium takes 3000 to 768000 Hz).
     *
     * @throws `Error` once released or transferred.
     */
    toAudioBuffer(): AudioBuffer {
        this.requireBuffer();
        const copy = new AudioBuffer({
            numberOfChannels: this.channels,
            length: this.frames,
            sampleRate: this.sampleRate,
        });
        for (let channel = 0; channel < this.channels; ++channel) {
            copy.copyToChannel(this.getChannelView(channel), channel);
        }
        return copy;
    }

    /**
     * Hand over the underlying `ArrayBuffer`, for example to
     * `worker.postMessage(buffer, [buffer])`.
     *
     * After this call the object gives out no more views and
     * {@link PcmBuffer.release} does nothing. Views taken earlier keep working
     * until the buffer is actually transferred with `postMessage`; from then on
     * they are detached (`byteLength` 0). A transferred buffer can no longer be
     * released on this page; to free it early, transfer it back to the page and
     * pass it to `chrome.webview.releaseBuffer`. The header stays at the start
     * of the buffer, so the receiver can read it with `readPcmHeader`.
     *
     * @throws `Error` once released or transferred.
     */
    transfer(): ArrayBuffer {
        const buffer = this.requireBuffer();
        this.buffer = null;
        return buffer;
    }

    /** Free the page's view of the shared memory. Safe to call more than once. */
    release(): void {
        const buffer = this.buffer;
        if (!buffer) return;
        this.buffer = null;
        this.releaseBuffer(buffer);
    }

    private requireBuffer(): ArrayBuffer {
        if (!this.buffer) throw new Error('This PcmBuffer has been released or transferred');
        return this.buffer;
    }
}
