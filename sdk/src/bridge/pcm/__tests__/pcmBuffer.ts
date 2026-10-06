// Test helpers: hand-built PCM buffers and a writer that mirrors the host's
// ring writer (src/api/PcmRing.cpp WriteRingChunk).

import { PCM_HEADER_BYTES_BY_VERSION, PCM_MAGIC, PCM_MODE_RING, PCM_OFFSETS } from '../PcmHeader.js';
import { PCM_SEGMENT_ENTRY_BYTES, PCM_SEGMENT_FLAG_ESTIMATED, PCM_SEGMENT_SLOTS } from '../PcmSegments.js';

export interface PcmBufferFields {
    mode?: number;
    version?: number;
    sampleRate?: number;
    channels?: number;
    capacityFrames?: number;
    seq?: number;
    writeFrames?: number;
    flags?: number;
    hostTimeMs?: number;
    startSeconds?: number;
    epoch?: number;
    writeSlot?: number;
}

// Unknown versions get the version-1 size so the version check, not the size check, rejects them.
export const headerBytesOf = (version: number): number => PCM_HEADER_BYTES_BY_VERSION[version] ?? 64;

export function makePcmBuffer(fields: PcmBufferFields = {}): ArrayBuffer {
    const version = fields.version ?? 1;
    const headerBytes = headerBytesOf(version);
    const channels = fields.channels ?? 2;
    const capacity = fields.capacityFrames ?? 4;
    const buffer = new ArrayBuffer(headerBytes + capacity * channels * 4);
    const view = new DataView(buffer);
    view.setUint32(PCM_OFFSETS.magic, PCM_MAGIC, true);
    view.setUint32(PCM_OFFSETS.version, version, true);
    view.setUint32(PCM_OFFSETS.headerBytes, headerBytes, true);
    view.setUint32(PCM_OFFSETS.mode, fields.mode ?? PCM_MODE_RING, true);
    view.setUint32(PCM_OFFSETS.sampleRate, fields.sampleRate ?? 48000, true);
    view.setUint32(PCM_OFFSETS.channels, channels, true);
    view.setUint32(PCM_OFFSETS.capacityFrames, capacity, true);
    view.setUint32(PCM_OFFSETS.seq, fields.seq ?? 0, true);
    view.setUint32(PCM_OFFSETS.writeFrames, fields.writeFrames ?? 0, true);
    view.setUint32(PCM_OFFSETS.flags, fields.flags ?? 0, true);
    view.setFloat64(PCM_OFFSETS.hostTimeMs, fields.hostTimeMs ?? 0, true);
    view.setFloat64(PCM_OFFSETS.startSeconds, fields.startSeconds ?? 0, true);
    view.setUint32(PCM_OFFSETS.epoch, fields.epoch ?? 1, true);
    view.setUint32(PCM_OFFSETS.writeSlot, fields.writeSlot ?? 0, true);
    return buffer;
}

export interface SegmentFields {
    segment: number;
    startFrame: number;
    reason: number;
    estimated?: boolean;
    startSeconds: number;
}

// Appends an entry to a version-2 segment table the way the host does: the next slot after the head.
export function pushSegment(buffer: ArrayBuffer, entry: SegmentFields): void {
    const view = new DataView(buffer);
    const current = view.getUint32(PCM_OFFSETS.segmentHead, true);
    const empty = view.getUint32(PCM_OFFSETS.segments + current * PCM_SEGMENT_ENTRY_BYTES, true) === 0;
    const head = empty ? current : (current + 1) % PCM_SEGMENT_SLOTS;
    const base = PCM_OFFSETS.segments + head * PCM_SEGMENT_ENTRY_BYTES;
    view.setUint32(base, entry.segment, true);
    view.setUint32(base + 4, entry.startFrame >>> 0, true);
    view.setUint32(base + 8, entry.reason, true);
    view.setUint32(base + 12, entry.estimated ? PCM_SEGMENT_FLAG_ESTIMATED : 0, true);
    view.setFloat64(base + 16, entry.startSeconds, true);
    view.setUint32(PCM_OFFSETS.segmentHead, head, true);
}

// Every channel c of the n-th frame this writer produces holds n + c * 0.25.
export class FakeRingWriter {
    readonly buffer: ArrayBuffer;
    private readonly view: DataView;
    private readonly samples: Float32Array;
    private produced = 0;

    constructor(
        readonly capacity: number,
        readonly channels: number,
        writeFrames = 0,
        writeSlot = 0,
        version = 1,
    ) {
        this.buffer = makePcmBuffer({ capacityFrames: capacity, channels, writeFrames, writeSlot, version });
        this.view = new DataView(this.buffer);
        this.samples = new Float32Array(this.buffer, headerBytesOf(version), capacity * channels);
    }

    /** `writeFrames` of the next frame to be written. */
    get nextFrame(): number {
        return this.view.getUint32(PCM_OFFSETS.writeFrames, true);
    }

    write(frames: number, hostTimeMs = 0): void {
        const seq = this.view.getUint32(PCM_OFFSETS.seq, true);
        this.view.setUint32(PCM_OFFSETS.seq, seq + 1, true);
        let slot = this.view.getUint32(PCM_OFFSETS.writeSlot, true);
        for (let i = 0; i < frames; ++i) {
            const frame = this.produced + i;
            for (let c = 0; c < this.channels; ++c) this.samples[slot * this.channels + c] = frame + c * 0.25;
            slot = (slot + 1) % this.capacity;
        }
        this.produced += frames;
        const writeFrames = (this.view.getUint32(PCM_OFFSETS.writeFrames, true) + frames) >>> 0;
        this.view.setUint32(PCM_OFFSETS.writeFrames, writeFrames, true);
        this.view.setUint32(PCM_OFFSETS.writeSlot, slot, true);
        this.view.setFloat64(PCM_OFFSETS.hostTimeMs, hostTimeMs, true);
        this.view.setUint32(PCM_OFFSETS.seq, seq + 2, true);
    }
}
