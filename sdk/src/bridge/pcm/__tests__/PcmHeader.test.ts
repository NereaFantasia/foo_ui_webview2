import { describe, expect, it } from 'vitest';

import {
    PCM_FLAG_ENDED,
    PCM_MODE_ONE_SHOT,
    PCM_OFFSETS,
    PcmHeaderError,
    readPcmHeader,
} from '../PcmHeader.js';
import { makePcmBuffer } from './pcmBuffer.js';

function reasonOf(fn: () => unknown): string | undefined {
    try {
        fn();
    } catch (error) {
        return error instanceof PcmHeaderError ? error.reason : 'other';
    }
    return undefined;
}

describe('readPcmHeader', () => {
    it('reads every field of a one-shot header', () => {
        const buffer = makePcmBuffer({
            mode: PCM_MODE_ONE_SHOT,
            sampleRate: 44100,
            channels: 1,
            capacityFrames: 10,
            seq: 2,
            writeFrames: 7,
            flags: PCM_FLAG_ENDED,
            hostTimeMs: 1758800000123.5,
            startSeconds: 5.25,
        });
        expect(readPcmHeader(buffer)).toEqual({
            version: 1,
            headerBytes: 64,
            mode: PCM_MODE_ONE_SHOT,
            sampleRate: 44100,
            channels: 1,
            capacityFrames: 10,
            seq: 2,
            writeFrames: 7,
            flags: PCM_FLAG_ENDED,
            hostTimeMs: 1758800000123.5,
            startSeconds: 5.25,
            epoch: 1,
            writeSlot: 0,
        });
    });

    it('starts with the bytes "FPCM"', () => {
        const bytes = new Uint8Array(makePcmBuffer(), 0, 4);
        expect(String.fromCharCode(...bytes)).toBe('FPCM');
    });

    it('reads a version-2 header, whose samples start at byte 192', () => {
        const header = readPcmHeader(makePcmBuffer({ version: 2, channels: 1, capacityFrames: 3 }));
        expect(header.version).toBe(2);
        expect(header.headerBytes).toBe(192);
        expect(header.capacityFrames).toBe(3);
    });

    it('rejects a header size that does not match the version', () => {
        const v2 = makePcmBuffer({ version: 2 });
        new DataView(v2).setUint32(PCM_OFFSETS.headerBytes, 64, true);
        expect(reasonOf(() => readPcmHeader(v2))).toBe('bad-header-size');
        const v1 = makePcmBuffer({ version: 1 });
        new DataView(v1).setUint32(PCM_OFFSETS.headerBytes, 192, true);
        expect(reasonOf(() => readPcmHeader(v1))).toBe('bad-header-size');
        const shortV2 = makePcmBuffer({ version: 2 }).slice(0, 128);
        expect(reasonOf(() => readPcmHeader(shortV2))).toBe('too-small');
    });

    it('rejects unknown versions, foreign data and a data region that does not fit', () => {
        expect(reasonOf(() => readPcmHeader(makePcmBuffer({ version: 3 })))).toBe('unsupported-version');
        expect(reasonOf(() => readPcmHeader(makePcmBuffer({ version: 0 })))).toBe('unsupported-version');
        expect(reasonOf(() => readPcmHeader(new ArrayBuffer(32)))).toBe('too-small');
        expect(reasonOf(() => readPcmHeader(new ArrayBuffer(128)))).toBe('bad-magic');
        expect(reasonOf(() => readPcmHeader(makePcmBuffer({ mode: 3 })))).toBe('bad-mode');

        const short = makePcmBuffer({ capacityFrames: 4 });
        new DataView(short).setUint32(PCM_OFFSETS.capacityFrames, 5, true);
        expect(reasonOf(() => readPcmHeader(short))).toBe('bad-size');

        const size = makePcmBuffer();
        new DataView(size).setUint32(PCM_OFFSETS.headerBytes, 128, true);
        expect(reasonOf(() => readPcmHeader(size))).toBe('bad-header-size');
    });
});
