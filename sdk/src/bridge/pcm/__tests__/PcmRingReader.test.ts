import { describe, expect, it } from 'vitest';

import { PCM_MODE_ONE_SHOT, PCM_OFFSETS, PcmHeaderError } from '../PcmHeader.js';
import { PcmRingReader } from '../PcmRingReader.js';
import { FakeRingWriter, makePcmBuffer, pushSegment } from './pcmBuffer.js';

function values(planes: Float32Array[], channel = 0): number[] {
    return Array.from(planes[channel]);
}

function range(from: number, count: number, offset = 0): number[] {
    return Array.from({ length: count }, (_, i) => from + i + offset);
}

describe('PcmRingReader', () => {
    it('returns exactly the frames written since the previous read', () => {
        const writer = new FakeRingWriter(100, 2);
        const reader = new PcmRingReader(writer.buffer);
        expect(reader.read()).toBeNull();

        writer.write(30, 11);
        let r = reader.read();
        expect(r).not.toBeNull();
        expect(r!.frames).toBe(30);
        expect(r!.dropped).toBe(0);
        expect(r!.hostTimeMs).toBe(11);
        expect(values(r!.planes, 0)).toEqual(range(0, 30));
        expect(values(r!.planes, 1)).toEqual(range(0, 30, 0.25));
        expect(reader.read()).toBeNull();

        writer.write(90);
        r = reader.read();
        expect(r!.frames).toBe(90);
        expect(values(r!.planes)).toEqual(range(30, 90));
    });

    it('reports overflow as dropped and resumes at the oldest frame still in the ring', () => {
        const writer = new FakeRingWriter(100, 1);
        const reader = new PcmRingReader(writer.buffer);
        writer.write(40);
        reader.read();
        // Three writes without a read: 150 new frames into a 100-frame ring.
        writer.write(50);
        writer.write(50);
        writer.write(50);
        const r = reader.read();
        expect(r!.dropped).toBe(50);
        expect(r!.frames).toBe(100);
        expect(values(r!.planes)).toEqual(range(90, 100));
        writer.write(5);
        expect(reader.read()!.dropped).toBe(0);
    });

    it('retries with a halved window when the host wrote during the copy, keeping the newest frames', () => {
        const writer = new FakeRingWriter(100, 1);
        writer.write(80);
        // First attempt: seq 2 before the copy and 4 after it. The second attempt is clean.
        const seqs = [2, 4, 2, 2];
        const reader = new PcmRingReader(writer.buffer, () => seqs.shift() ?? 2);
        const r = reader.read();
        expect(r!.frames).toBe(40);
        expect(r!.dropped).toBe(40);
        expect(values(r!.planes)).toEqual(range(40, 40));
    });

    it('gives up after three torn attempts without consuming anything', () => {
        const writer = new FakeRingWriter(100, 1);
        writer.write(10);
        let torn = true;
        let seq = 0;
        const reader = new PcmRingReader(writer.buffer, () => (torn ? (seq += 2) : 2));
        expect(reader.read()).toBeNull();
        torn = false;
        const r = reader.read();
        expect(r!.frames).toBe(10);
        expect(r!.dropped).toBe(0);
    });

    it('waits while seq is odd', () => {
        const writer = new FakeRingWriter(10, 1);
        writer.write(3);
        const seqs = [3, 3, 3];
        const reader = new PcmRingReader(writer.buffer, () => seqs.shift() ?? 2);
        expect(reader.read()).toBeNull();
        expect(reader.read()!.frames).toBe(3);
    });

    it('addresses frames through writeSlot across the 2^32 wrap of writeFrames', () => {
        const nearWrap = 0xffffffff - 9;
        const writer = new FakeRingWriter(48000, 1, nearWrap, 47990);
        const reader = new PcmRingReader(writer.buffer);
        // The first read catches up with the count the buffer was created with.
        expect(reader.read()!.frames).toBe(48000);
        writer.write(20);
        const view = new DataView(writer.buffer);
        expect(view.getUint32(PCM_OFFSETS.writeFrames, true)).toBe(10);
        expect(view.getUint32(PCM_OFFSETS.writeSlot, true)).toBe(10);
        const r = reader.read();
        expect(r!.frames).toBe(20);
        expect(r!.dropped).toBe(0);
        expect(values(r!.planes)).toEqual(range(0, 20));
    });

    it('reports ended from the header flag', () => {
        const writer = new FakeRingWriter(10, 1);
        const reader = new PcmRingReader(writer.buffer);
        expect(reader.ended).toBe(false);
        new DataView(writer.buffer).setUint32(PCM_OFFSETS.flags, 1, true);
        expect(reader.ended).toBe(true);
    });

    it('rejects one-shot buffers and unknown versions', () => {
        expect(() => new PcmRingReader(makePcmBuffer({ mode: PCM_MODE_ONE_SHOT }))).toThrow(PcmHeaderError);
        expect(() => new PcmRingReader(makePcmBuffer({ version: 3 }))).toThrow(PcmHeaderError);
    });

    it('counts frames written but not yet read, including overwritten ones', () => {
        const writer = new FakeRingWriter(10, 1);
        const reader = new PcmRingReader(writer.buffer);
        writer.write(4);
        reader.read();
        writer.write(15);
        expect(reader.unreadFrames()).toBe(15);
        reader.read();
        expect(reader.unreadFrames()).toBe(0);
    });
});

describe('PcmRingReader segments', () => {
    it('reports one unknown run for a version-1 buffer, which has no segment table', () => {
        const writer = new FakeRingWriter(100, 1);
        const reader = new PcmRingReader(writer.buffer);
        writer.write(30);
        expect(reader.read()!.segments).toEqual([{ offset: 0, segment: null, startSeconds: null, reason: null, estimated: false }]);
    });

    it('reads a version-2 ring after its 192-byte header', () => {
        const writer = new FakeRingWriter(100, 2, 0, 0, 2);
        const reader = new PcmRingReader(writer.buffer);
        writer.write(10);
        const r = reader.read()!;
        expect(values(r.planes, 0)).toEqual(range(0, 10));
        expect(values(r.planes, 1)).toEqual(range(0, 10, 0.25));
    });

    it('carries the start time of the segment the first frame belongs to', () => {
        const writer = new FakeRingWriter(1000, 1, 0, 0, 2);
        pushSegment(writer.buffer, { segment: 1, startFrame: 0, reason: 1, startSeconds: 0 });
        const reader = new PcmRingReader(writer.buffer);
        writer.write(480);
        expect(reader.read()!.segments).toEqual([{ offset: 0, segment: 1, startSeconds: 0, reason: 'start', estimated: false }]);
        writer.write(240);
        expect(reader.read()!.segments).toEqual([{ offset: 0, segment: 1, startSeconds: 0.01, reason: 'start', estimated: false }]);
    });

    it('splits the frames where a segment starts inside them', () => {
        const writer = new FakeRingWriter(1000, 1, 0, 0, 2);
        pushSegment(writer.buffer, { segment: 1, startFrame: 0, reason: 1, startSeconds: 0 });
        const reader = new PcmRingReader(writer.buffer);
        writer.write(100);
        reader.read();
        writer.write(200);
        // A seek landed at frame 250, in the middle of the 200 frames read next.
        pushSegment(writer.buffer, { segment: 2, startFrame: 250, reason: 2, startSeconds: 30 });
        const r = reader.read()!;
        expect(r.frames).toBe(200);
        expect(r.segments).toEqual([
            { offset: 0, segment: 1, startSeconds: 100 / 48000, reason: 'start', estimated: false },
            { offset: 150, segment: 2, startSeconds: 30, reason: 'seek', estimated: false },
        ]);
    });

    it('maps every reason code and the estimated flag', () => {
        const writer = new FakeRingWriter(1000, 1, 0, 0, 2);
        const reader = new PcmRingReader(writer.buffer);
        writer.write(10);
        reader.read();
        writer.write(60);
        for (let reason = 1; reason <= 4; ++reason) {
            pushSegment(writer.buffer, { segment: reason, startFrame: 10 + reason * 10, reason, estimated: reason > 2, startSeconds: reason });
        }
        const r = reader.read()!;
        expect(r.segments.map((s) => [s.offset, s.reason, s.estimated])).toEqual([
            [0, null, false],
            [10, 'start', false],
            [20, 'seek', false],
            [30, 'transition', true],
            [40, 'jump', true],
        ]);
        writer.write(20);
        pushSegment(writer.buffer, { segment: 5, startFrame: 75, reason: 5, estimated: true, startSeconds: 5 });
        writer.write(10);
        pushSegment(writer.buffer, { segment: 6, startFrame: 95, reason: 6, estimated: true, startSeconds: 6 });
        expect(reader.read()!.segments.map((s) => [s.offset, s.segment, s.reason])).toEqual([
            [0, 4, 'jump'],
            [5, 5, 'format'],
            [25, 6, 'join'],
        ]);
    });

    it('leaves the first run unknown when its frames are older than every listed entry', () => {
        const writer = new FakeRingWriter(1000, 1, 0, 0, 2);
        const reader = new PcmRingReader(writer.buffer);
        writer.write(100);
        // Five segments since the previous read: the table keeps the newest four.
        for (let n = 1; n <= 5; ++n) pushSegment(writer.buffer, { segment: n, startFrame: n * 10, reason: 2, startSeconds: n });
        const r = reader.read()!;
        expect(r.segments.map((s) => [s.offset, s.segment])).toEqual([
            [0, null],
            [20, 2],
            [30, 3],
            [40, 4],
            [50, 5],
        ]);
        expect(r.segments[0]).toMatchObject({ startSeconds: null, reason: null, estimated: false });
    });

    it('compares frame positions across the 2^32 wrap of writeFrames', () => {
        const nearWrap = 0xffffffff - 99;
        const writer = new FakeRingWriter(1000, 1, nearWrap, 0, 2);
        const reader = new PcmRingReader(writer.buffer);
        reader.read();
        // The segment starts 50 frames before the wrap; the next read spans the wrap.
        pushSegment(writer.buffer, { segment: 7, startFrame: nearWrap + 50, reason: 2, startSeconds: 60 });
        writer.write(200);
        expect(writer.nextFrame).toBe(100);
        const r = reader.read()!;
        expect(r.segments).toEqual([
            { offset: 0, segment: null, startSeconds: null, reason: null, estimated: false },
            { offset: 50, segment: 7, startSeconds: 60, reason: 'seek', estimated: false },
        ]);
        pushSegment(writer.buffer, { segment: 8, startFrame: 120, reason: 2, startSeconds: 90 });
        writer.write(40);
        expect(reader.read()!.segments).toEqual([
            { offset: 0, segment: 7, startSeconds: 60 + 150 / 48000, reason: 'seek', estimated: false },
            { offset: 20, segment: 8, startSeconds: 90, reason: 'seek', estimated: false },
        ]);
    });

    it('ignores an out-of-range head index', () => {
        const writer = new FakeRingWriter(100, 1, 0, 0, 2);
        pushSegment(writer.buffer, { segment: 1, startFrame: 0, reason: 1, startSeconds: 0 });
        new DataView(writer.buffer).setUint32(PCM_OFFSETS.segmentHead, 4, true);
        const reader = new PcmRingReader(writer.buffer);
        writer.write(10);
        expect(reader.read()!.segments).toEqual([{ offset: 0, segment: null, startSeconds: null, reason: null, estimated: false }]);
    });
});
