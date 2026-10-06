/**
 * Segment table of version-2 ring buffers: where the media time of the stream
 * restarts (start, seek, track change) and what track time each segment
 * begins at, so a reader can map any frame it copies out to a track time.
 */

import { PCM_OFFSETS } from './PcmHeader.js';

/** Entries in the segment table; older segments fall out of it. */
export const PCM_SEGMENT_SLOTS = 4;
/** Bytes per segment table entry. */
export const PCM_SEGMENT_ENTRY_BYTES = 24;
/** Entry flag bit: the start time was estimated rather than taken from a callback. */
export const PCM_SEGMENT_FLAG_ESTIMATED = 1 << 0;

/**
 * Why a segment began:
 * - `'start'` playback started or the track was changed by hand;
 * - `'seek'` a seek;
 * - `'transition'` the next track followed without a stop (gapless or crossfade);
 * - `'jump'` the playback position moved while the stream kept flowing;
 * - `'format'` the stream format changed and the segment continues the previous one;
 * - `'join'` the subscription started while audio was already playing.
 */
export type PcmSegmentReason = 'start' | 'seek' | 'transition' | 'jump' | 'format' | 'join';

/** One run of frames with continuous media time within a {@link PcmRingReader.read} result. */
export interface PcmSegment {
    /** Index in the result's `planes` where this run begins; the first run begins at 0. */
    offset: number;
    /**
     * Segment number, rising by one per segment and kept across a format
     * change. `null` when the frames are older than every entry the buffer
     * still lists, or the buffer carries no segment table (version 1).
     */
    segment: number | null;
    /** Track time in seconds of the frame at `offset`; `null` when the segment is unknown. */
    startSeconds: number | null;
    /** `null` when the segment is unknown. */
    reason: PcmSegmentReason | null;
    /** The start time was derived rather than reported by foobar2000; expect tens of milliseconds of error. */
    estimated: boolean;
}

/** A raw entry of the segment table. */
export interface PcmSegmentEntry {
    /** Segment number, from 1. */
    segment: number;
    /** `writeFrames` value of the segment's first frame, wrapping at 2^32. */
    startFrame: number;
    /** Reason code 1 to 6, in the order of {@link PcmSegmentReason}. */
    reason: number;
    estimated: boolean;
    /** Track time in seconds of the segment's first frame. */
    startSeconds: number;
}

const REASONS: readonly PcmSegmentReason[] = ['start', 'seek', 'transition', 'jump', 'format', 'join'];

const UNKNOWN: Omit<PcmSegment, 'offset'> = { segment: null, startSeconds: null, reason: null, estimated: false };

/**
 * Read the segment table of a version-2 header, newest entry first, empty
 * entries left out. An out-of-range head index yields no entries.
 *
 * @param header - View over at least the 192-byte header.
 */
export function readSegmentEntries(header: DataView): PcmSegmentEntry[] {
    const head = header.getUint32(PCM_OFFSETS.segmentHead, true);
    if (head >= PCM_SEGMENT_SLOTS) return [];
    const entries: PcmSegmentEntry[] = [];
    for (let k = 0; k < PCM_SEGMENT_SLOTS; ++k) {
        const base = PCM_OFFSETS.segments + ((head - k + PCM_SEGMENT_SLOTS) % PCM_SEGMENT_SLOTS) * PCM_SEGMENT_ENTRY_BYTES;
        const segment = header.getUint32(base, true);
        if (segment === 0) continue;
        entries.push({
            segment,
            startFrame: header.getUint32(base + 4, true),
            reason: header.getUint32(base + 8, true),
            estimated: (header.getUint32(base + 12, true) & PCM_SEGMENT_FLAG_ESTIMATED) !== 0,
            startSeconds: header.getFloat64(base + 16, true),
        });
    }
    return entries;
}

/**
 * Cut the newest `frames` frames before `writeFrames` into runs of continuous
 * media time.
 *
 * Frame numbers wrap at 2^32, so entries are compared by how far they lie
 * behind `writeFrames`. The first run belongs to the newest entry that began
 * at or before the first frame, or is unknown when no listed entry did; every
 * entry that begins inside the range starts another run.
 *
 * @param entries - Newest first, as {@link readSegmentEntries} returns them.
 * @param writeFrames - The header's `writeFrames` read in the same seqlock window as the entries.
 * @param frames - Number of frames read, ending right before `writeFrames`.
 * @param sampleRate - Samples per second, used to carry a start time forward to the first frame.
 */
export function splitIntoSegments(
    entries: readonly PcmSegmentEntry[],
    writeFrames: number,
    frames: number,
    sampleRate: number,
): PcmSegment[] {
    const behind = (frame: number): number => (writeFrames - frame) >>> 0;
    let covering: PcmSegmentEntry | undefined;
    const inside = new Map<number, PcmSegmentEntry>();
    for (const entry of entries) {
        const distance = behind(entry.startFrame);
        if (distance >= frames) {
            if (!covering || distance < behind(covering.startFrame)) covering = entry;
        } else if (distance > 0 && !inside.has(distance)) {
            // A distance of 0 would be a segment starting at a frame not yet written.
            inside.set(distance, entry);
        }
    }

    const toSegment = (entry: PcmSegmentEntry, offset: number, startSeconds: number): PcmSegment => ({
        offset,
        segment: entry.segment,
        startSeconds,
        reason: REASONS[entry.reason - 1] ?? null,
        estimated: entry.estimated,
    });

    const runs: PcmSegment[] = [];
    if (covering) {
        const carried = behind(covering.startFrame) - frames;
        runs.push(toSegment(covering, 0, covering.startSeconds + (sampleRate > 0 ? carried / sampleRate : 0)));
    } else {
        runs.push({ offset: 0, ...UNKNOWN });
    }
    const starts = [...inside.keys()].sort((a, b) => b - a);
    for (const distance of starts) {
        const entry = inside.get(distance);
        if (entry) runs.push(toSegment(entry, frames - distance, entry.startSeconds));
    }
    return runs;
}

/** The single unknown run a version-1 buffer, which has no segment table, reports. */
export function unsegmented(): PcmSegment[] {
    return [{ offset: 0, ...UNKNOWN }];
}
