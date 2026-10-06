/**
 * Reader for the ring buffers of `audio.subscribeStream`.
 *
 * The host writes interleaved float32 frames under a seqlock: `seq` turns
 * odd, the samples and counters change, `seq` turns even again. The buffer
 * is posted read-only, so the host never learns how far the page has read;
 * when the page falls behind, the oldest frames are overwritten and the
 * reader reports them as dropped.
 */

import { PCM_FLAG_ENDED, PCM_MODE_RING, PCM_OFFSETS, PcmHeaderError, readPcmHeader } from './PcmHeader.js';
import { readSegmentEntries, splitIntoSegments, unsegmented, type PcmSegment } from './PcmSegments.js';

/** Frames returned by one {@link PcmRingReader.read} call. */
export interface PcmRingRead {
    /** One array per channel, `frames` samples each, oldest first. */
    planes: Float32Array[];
    frames: number;
    /** Frames skipped since the previous read because the ring overwrote them or a retry shrank the window. */
    dropped: number;
    /** Host Unix time in milliseconds of the write that produced the newest frame. */
    hostTimeMs: number;
    /**
     * The frames cut into runs of continuous media time, in `offset` order,
     * the first at offset 0. A version-1 buffer has no segment table and
     * always reports one unknown run.
     */
    segments: PcmSegment[];
}

/** Test hook: reads the seqlock counter; defaults to `Atomics.load`. */
export type SeqLoader = (header: Uint32Array) => number;

const SEQ_INDEX = PCM_OFFSETS.seq / 4;
const WRITE_FRAMES_INDEX = PCM_OFFSETS.writeFrames / 4;
const FLAGS_INDEX = PCM_OFFSETS.flags / 4;
const WRITE_SLOT_INDEX = PCM_OFFSETS.writeSlot / 4;
const MAX_ATTEMPTS = 3;

const atomicSeq: SeqLoader = (header) => Atomics.load(header, SEQ_INDEX);

/**
 * Pull the frames written since the previous read from one ring buffer.
 *
 * Construct one reader per buffer (a new epoch comes with a new buffer).
 * Reading starts at frame 0 of the buffer, so the first {@link read} also
 * returns what was written before the page received it, up to the capacity.
 * @experimental
 */
export class PcmRingReader {
    /** Samples per second per channel. */
    readonly sampleRate: number;
    readonly channels: number;
    /** Frames the ring holds. */
    readonly capacityFrames: number;
    /** Generation of this buffer; a format change arrives as a new buffer with a higher epoch. */
    readonly epoch: number;

    private readonly version: number;
    private readonly header: Uint32Array;
    private readonly view: DataView;
    private readonly samples: Float32Array;
    private readonly loadSeq: SeqLoader;
    private readFrames = 0;

    /**
     * @param buffer - Ring buffer from `sharedbufferreceived`.
     * @param loadSeq - Replaces `Atomics.load` for the seqlock counter; meant for tests.
     * @throws {@link PcmHeaderError} when the buffer is not a ring buffer of a version this reader supports.
     */
    constructor(buffer: ArrayBuffer, loadSeq: SeqLoader = atomicSeq) {
        const header = readPcmHeader(buffer);
        if (header.mode !== PCM_MODE_RING) {
            throw new PcmHeaderError('bad-mode', 'Expected a ring buffer');
        }
        this.sampleRate = header.sampleRate;
        this.channels = header.channels;
        this.capacityFrames = header.capacityFrames;
        this.epoch = header.epoch;
        this.version = header.version;
        this.header = new Uint32Array(buffer, 0, header.headerBytes / 4);
        this.view = new DataView(buffer, 0, header.headerBytes);
        this.samples = new Float32Array(buffer, header.headerBytes, header.capacityFrames * header.channels);
        this.loadSeq = loadSeq;
    }

    /** `true` once the host has stopped writing (unsubscribed, format change, page gone or host exit). */
    get ended(): boolean {
        return (Atomics.load(this.header, FLAGS_INDEX) & PCM_FLAG_ENDED) !== 0;
    }

    /** Frames written that no {@link read} has returned yet, including any the ring already overwrote. */
    unreadFrames(): number {
        return (Atomics.load(this.header, WRITE_FRAMES_INDEX) - this.readFrames) >>> 0;
    }

    /**
     * Copy out the frames written since the previous call, de-interleaved.
     *
     * Returns `null` when nothing new was written, or when the host kept
     * writing during all three attempts; in that case nothing is consumed
     * and the next call tries again. Each retry halves the window and keeps
     * only the newest frames, so a slow copy of a large backlog still
     * completes; the frames given up that way count in `dropped`.
     */
    read(): PcmRingRead | null {
        const capacity = this.capacityFrames;
        let limit = Number.POSITIVE_INFINITY;
        for (let attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
            const before = this.loadSeq(this.header);
            if ((before & 1) !== 0) continue;

            const writeFrames = this.header[WRITE_FRAMES_INDEX];
            const writeSlot = this.header[WRITE_SLOT_INDEX];
            const hostTimeMs = this.view.getFloat64(PCM_OFFSETS.hostTimeMs, true);
            const available = (writeFrames - this.readFrames) >>> 0;
            if (available === 0) return null;
            // The segment table is written in the same seqlock window as the samples.
            const entries = this.version >= 2 ? readSegmentEntries(this.view) : null;

            const inRing = Math.min(available, capacity);
            const frames = Math.max(1, Math.min(inRing, limit));
            const planes = this.copyNewest(frames, writeSlot);

            if (this.loadSeq(this.header) !== before) {
                limit = Math.max(1, Math.floor(frames / 2));
                continue;
            }
            this.readFrames = writeFrames;
            const segments = entries ? splitIntoSegments(entries, writeFrames, frames, this.sampleRate) : unsegmented();
            return { planes, frames, dropped: available - frames, hostTimeMs, segments };
        }
        return null;
    }

    // The newest `frames` frames end right before writeSlot and may wrap past slot 0.
    private copyNewest(frames: number, writeSlot: number): Float32Array[] {
        const capacity = this.capacityFrames;
        const channels = this.channels;
        const planes: Float32Array[] = [];
        for (let c = 0; c < channels; ++c) planes.push(new Float32Array(frames));
        let slot = (writeSlot + capacity - frames) % capacity;
        for (let i = 0; i < frames; ++i) {
            const base = slot * channels;
            for (let c = 0; c < channels; ++c) planes[c][i] = this.samples[base + c];
            slot = slot + 1 === capacity ? 0 : slot + 1;
        }
        return planes;
    }
}
