import { afterEach, describe, expect, it, vi } from 'vitest';

import { PcmBuffer } from '../PcmBuffer.js';
import { PCM_FLAG_ENDED, PCM_FLAG_RESAMPLED, PCM_FLAG_TRUNCATED, PCM_MODE_ONE_SHOT, PcmHeaderError } from '../PcmHeader.js';
import { headerBytesOf, makePcmBuffer } from './pcmBuffer.js';

// A one-shot buffer whose channel c holds c * 10 + i at frame i.
function makeOneShot(channels: number, capacity: number, frames: number, flags = PCM_FLAG_ENDED, version = 1): ArrayBuffer {
    const buffer = makePcmBuffer({
        version,
        mode: PCM_MODE_ONE_SHOT,
        channels,
        capacityFrames: capacity,
        writeFrames: frames,
        seq: 2,
        flags,
        sampleRate: 8000,
        startSeconds: 1.5,
    });
    for (let c = 0; c < channels; ++c) {
        const run = new Float32Array(buffer, headerBytesOf(version) + c * capacity * 4, capacity);
        for (let i = 0; i < frames; ++i) run[i] = c * 10 + i;
    }
    return buffer;
}

class FakeAudioBuffer {
    readonly numberOfChannels: number;
    readonly length: number;
    readonly sampleRate: number;
    private readonly data: Float32Array[];

    constructor(options: { numberOfChannels: number; length: number; sampleRate: number }) {
        this.numberOfChannels = options.numberOfChannels;
        this.length = options.length;
        this.sampleRate = options.sampleRate;
        this.data = Array.from({ length: options.numberOfChannels }, () => new Float32Array(options.length));
    }

    copyToChannel(source: Float32Array, channel: number): void {
        this.data[channel].set(source);
    }

    getChannelData(channel: number): Float32Array {
        return this.data[channel];
    }
}

afterEach(() => {
    vi.unstubAllGlobals();
});

describe('PcmBuffer', () => {
    it('reads its format from the header', () => {
        const pcm = new PcmBuffer(makeOneShot(2, 6, 4, PCM_FLAG_ENDED | PCM_FLAG_TRUNCATED | PCM_FLAG_RESAMPLED), () => {});
        expect(pcm.sampleRate).toBe(8000);
        expect(pcm.channels).toBe(2);
        expect(pcm.frames).toBe(4);
        expect(pcm.duration).toBe(4 / 8000);
        expect(pcm.start).toBe(1.5);
        expect(pcm.truncated).toBe(true);
        expect(pcm.resampled).toBe(true);
        expect(pcm.available).toBe(true);
    });

    it('gives zero-copy planar views of `frames` samples, skipping the unused capacity', () => {
        const buffer = makeOneShot(2, 6, 4);
        const pcm = new PcmBuffer(buffer, () => {});
        const left = pcm.getChannelView(0);
        const right = pcm.getChannelView(1);
        expect(Array.from(left)).toEqual([0, 1, 2, 3]);
        expect(Array.from(right)).toEqual([10, 11, 12, 13]);
        expect(left.buffer).toBe(buffer);
        expect(right.byteOffset).toBe(64 + 6 * 4);
    });

    it('finds the samples of a version-2 buffer after its 192-byte header', () => {
        const pcm = new PcmBuffer(makeOneShot(2, 6, 4, PCM_FLAG_ENDED, 2), () => {});
        expect(Array.from(pcm.getChannelView(0))).toEqual([0, 1, 2, 3]);
        expect(Array.from(pcm.getChannelView(1))).toEqual([10, 11, 12, 13]);
        expect(pcm.getChannelView(1).byteOffset).toBe(192 + 6 * 4);
    });

    it('rejects channel indexes outside the buffer', () => {
        const pcm = new PcmBuffer(makeOneShot(2, 4, 4), () => {});
        expect(() => pcm.getChannelView(2)).toThrow(RangeError);
        expect(() => pcm.getChannelView(-1)).toThrow(RangeError);
        expect(() => pcm.getChannelView(0.5)).toThrow(RangeError);
    });

    it('copies into an AudioBuffer that does not share memory with the views', () => {
        vi.stubGlobal('AudioBuffer', FakeAudioBuffer);
        const pcm = new PcmBuffer(makeOneShot(2, 6, 4), () => {});
        const copy = pcm.toAudioBuffer();
        expect(copy.numberOfChannels).toBe(2);
        expect(copy.length).toBe(4);
        expect(copy.sampleRate).toBe(8000);
        for (let c = 0; c < 2; ++c) {
            const view = pcm.getChannelView(c);
            const data = copy.getChannelData(c);
            expect(Array.from(data)).toEqual(Array.from(view));
            expect(data.buffer).not.toBe(view.buffer);
        }
    });

    it('releases once, after which views and copies throw', () => {
        const release = vi.fn();
        const buffer = makeOneShot(1, 4, 4);
        const pcm = new PcmBuffer(buffer, release);
        pcm.release();
        pcm.release();
        expect(release).toHaveBeenCalledTimes(1);
        expect(release).toHaveBeenCalledWith(buffer);
        expect(pcm.available).toBe(false);
        expect(() => pcm.getChannelView(0)).toThrow(Error);
        expect(() => pcm.toAudioBuffer()).toThrow(Error);
    });

    it('hands the buffer over on transfer and no longer releases it', () => {
        const release = vi.fn();
        const buffer = makeOneShot(1, 4, 4);
        const pcm = new PcmBuffer(buffer, release);
        expect(pcm.transfer()).toBe(buffer);
        expect(() => pcm.getChannelView(0)).toThrow(Error);
        expect(() => pcm.transfer()).toThrow(Error);
        pcm.release();
        expect(release).not.toHaveBeenCalled();
    });

    it('keeps views taken before transfer readable until the buffer is actually transferred', () => {
        const buffer = makeOneShot(1, 4, 4);
        const pcm = new PcmBuffer(buffer, () => {});
        const view = pcm.getChannelView(0);
        const handedOver = pcm.transfer();
        expect(view.byteLength).toBe(16);
        structuredClone(handedOver, { transfer: [handedOver] });
        expect(view.byteLength).toBe(0);
    });

    it('refuses ring buffers and frame counts beyond the capacity', () => {
        expect(() => new PcmBuffer(makePcmBuffer({ channels: 1, capacityFrames: 4 }), () => {})).toThrow(PcmHeaderError);
        const overfull = makePcmBuffer({ mode: PCM_MODE_ONE_SHOT, channels: 1, capacityFrames: 4, writeFrames: 5 });
        expect(() => new PcmBuffer(overfull, () => {})).toThrow(PcmHeaderError);
    });
});
