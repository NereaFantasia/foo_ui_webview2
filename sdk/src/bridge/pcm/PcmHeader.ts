/**
 * Layout of the shared PCM buffers the host posts through
 * `chrome.webview` `sharedbufferreceived`.
 *
 * Every buffer starts with a little-endian header followed by float32
 * samples: planar in one-shot buffers (`audio.decodePcm`), interleaved in
 * ring buffers (`audio.subscribeStream`). Version 1 has a 64-byte header;
 * version 2 keeps those 64 bytes and adds a segment table, for 192 bytes.
 * Samples start at `headerBytes`. The header describes the buffer
 * completely, so a reader does not depend on the JSON messages that
 * accompany it.
 */

/** Magic number at byte 0: the bytes `"FPCM"`. */
export const PCM_MAGIC = 0x4d435046;
/** Header size in bytes of each layout version this reader understands; other versions are rejected. */
export const PCM_HEADER_BYTES_BY_VERSION: Readonly<Record<number, number>> = { 1: 64, 2: 192 };
/** Size of the version-1 header, the part every version shares. */
export const PCM_HEADER_BYTES_V1 = 64;

/** Buffer written once, planar, posted after the last sample. */
export const PCM_MODE_ONE_SHOT = 1;
/** Buffer written continuously by the host, interleaved. */
export const PCM_MODE_RING = 2;

/** Flag bit: the host no longer writes to this buffer. */
export const PCM_FLAG_ENDED = 1 << 0;
/** Flag bit (one-shot): the source was longer than the buffer and got cut. */
export const PCM_FLAG_TRUNCATED = 1 << 1;
/** Flag bit (one-shot): the audio was converted to another sample rate. */
export const PCM_FLAG_RESAMPLED = 1 << 2;

/** Byte offsets of the header fields. */
export const PCM_OFFSETS = {
    magic: 0,
    version: 4,
    headerBytes: 8,
    mode: 12,
    sampleRate: 16,
    channels: 20,
    capacityFrames: 24,
    seq: 28,
    writeFrames: 32,
    flags: 36,
    hostTimeMs: 40,
    startSeconds: 48,
    epoch: 56,
    writeSlot: 60,
    /** Version 2: index of the newest segment table entry. */
    segmentHead: 64,
    /** Version 2: first of the segment table entries. */
    segments: 72,
} as const;

/** Decoded header of a shared PCM buffer. */
export interface PcmHeader {
    /** Layout version, 1 or 2. */
    version: number;
    /** Byte offset of the samples: 64 for version 1, 192 for version 2. */
    headerBytes: number;
    /** {@link PCM_MODE_ONE_SHOT} or {@link PCM_MODE_RING}. */
    mode: typeof PCM_MODE_ONE_SHOT | typeof PCM_MODE_RING;
    /** Samples per second per channel. */
    sampleRate: number;
    channels: number;
    /** Frames the data region holds. */
    capacityFrames: number;
    /** Seqlock counter: odd while the host is writing. One-shot buffers carry 2. */
    seq: number;
    /** Frames written so far, wrapping at 2^32. One-shot buffers: the frame count. */
    writeFrames: number;
    /** Combination of the `PCM_FLAG_*` bits. */
    flags: number;
    /** Host Unix time in milliseconds of the latest write (one-shot: decode completion). */
    hostTimeMs: number;
    /** One-shot: track time in seconds of the first frame. Ring: 0. */
    startSeconds: number;
    /** Ring: generation, starting at 1 and bumped when the stream format changes. One-shot: 1. */
    epoch: number;
    /** Ring: slot the next frame goes to, 0 to capacityFrames - 1. One-shot: 0. */
    writeSlot: number;
}

/** Why {@link readPcmHeader} rejected a buffer. */
export type PcmHeaderErrorReason =
    | 'too-small'
    | 'bad-magic'
    | 'unsupported-version'
    | 'bad-header-size'
    | 'bad-mode'
    | 'bad-size';

/** Thrown by {@link readPcmHeader} for a buffer it cannot read. */
export class PcmHeaderError extends Error {
    /** Machine-readable cause. */
    readonly reason: PcmHeaderErrorReason;

    constructor(reason: PcmHeaderErrorReason, message: string) {
        super(message);
        this.name = 'PcmHeaderError';
        this.reason = reason;
    }
}

/**
 * Read and validate the header of a shared PCM buffer.
 *
 * Checks the magic number, the layout version, the mode and that the data
 * region fits in the buffer; any mismatch throws {@link PcmHeaderError}.
 * A ring buffer keeps changing after this call: re-read `seq`,
 * `writeFrames` and `writeSlot` through a reader rather than trusting the
 * returned snapshot.
 * @experimental
 */
export function readPcmHeader(buffer: ArrayBuffer): PcmHeader {
    if (buffer.byteLength < PCM_HEADER_BYTES_V1) {
        throw new PcmHeaderError('too-small', `PCM buffer is ${buffer.byteLength} bytes, shorter than its header`);
    }
    const view = new DataView(buffer, 0, PCM_HEADER_BYTES_V1);
    const u32 = (offset: number): number => view.getUint32(offset, true);
    if (u32(PCM_OFFSETS.magic) !== PCM_MAGIC) {
        throw new PcmHeaderError('bad-magic', 'Not a PCM buffer: magic number mismatch');
    }
    const version = u32(PCM_OFFSETS.version);
    const headerBytes = PCM_HEADER_BYTES_BY_VERSION[version];
    if (headerBytes === undefined) {
        throw new PcmHeaderError('unsupported-version', `Unsupported PCM buffer version ${version}`);
    }
    if (u32(PCM_OFFSETS.headerBytes) !== headerBytes) {
        throw new PcmHeaderError('bad-header-size', 'Unexpected PCM header size');
    }
    if (buffer.byteLength < headerBytes) {
        throw new PcmHeaderError('too-small', `PCM buffer is ${buffer.byteLength} bytes, shorter than its header`);
    }
    const mode = u32(PCM_OFFSETS.mode);
    if (mode !== PCM_MODE_ONE_SHOT && mode !== PCM_MODE_RING) {
        throw new PcmHeaderError('bad-mode', `Unknown PCM buffer mode ${mode}`);
    }
    const header: PcmHeader = {
        version,
        headerBytes,
        mode,
        sampleRate: u32(PCM_OFFSETS.sampleRate),
        channels: u32(PCM_OFFSETS.channels),
        capacityFrames: u32(PCM_OFFSETS.capacityFrames),
        seq: u32(PCM_OFFSETS.seq),
        writeFrames: u32(PCM_OFFSETS.writeFrames),
        flags: u32(PCM_OFFSETS.flags),
        hostTimeMs: view.getFloat64(PCM_OFFSETS.hostTimeMs, true),
        startSeconds: view.getFloat64(PCM_OFFSETS.startSeconds, true),
        epoch: u32(PCM_OFFSETS.epoch),
        writeSlot: u32(PCM_OFFSETS.writeSlot),
    };
    const dataBytes = header.capacityFrames * header.channels * 4;
    if (header.channels === 0 || headerBytes + dataBytes > buffer.byteLength) {
        throw new PcmHeaderError('bad-size', 'PCM data region does not fit in the buffer');
    }
    return header;
}
