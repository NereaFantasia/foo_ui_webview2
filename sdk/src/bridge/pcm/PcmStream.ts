/**
 * `audio.subscribeStream` for pages: one subscription to the audio foobar2000
 * is playing, read from the ring buffers the host shares with the page.
 *
 * The host posts a buffer when the first chunk of audio arrives and a new one,
 * with a higher `epoch`, whenever the sample rate or channel count changes.
 * The buffer and the host's answer to the subscription can arrive in either
 * order, so the buffer listener is registered before the request is sent.
 */

import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import { PcmRingReader, type PcmRingRead } from './PcmRingReader.js';
import { STREAM_PURPOSE, getPageReceiver } from './pageReceiver.js';
import type { SharedBufferDelivery, SharedBufferReceiver } from './SharedBufferReceiver.js';
import type { AudioStreamPayload } from '../../types/events.js';
import type { AudioSubscribeStreamParams } from '../../types/generated/params.js';
import type {
    AudioSubscribeStreamResponse,
} from '../../types/generated/responses.js';

/** Options of {@link subscribeStream}; both go to the host unchanged. */
export type PcmStreamOptions = Omit<AudioSubscribeStreamParams, 'subscriptionId'>;

/** Format of the buffer the host currently writes, from its header. */
export interface PcmStreamFormat {
    /** Samples per second per channel. */
    sampleRate: number;
    channels: number;
    /** Frames the ring holds; a reader that falls further behind loses the oldest frames. */
    capacityFrames: number;
    /** Buffer generation, starting at 1 and rising by one per format change. */
    epoch: number;
}

/** How the host answered the subscription; `code` is the host's error code. */
export type PcmStreamOutcome =
    | { ok: true; subscriptionId: string }
    | { ok: false; code: string; error?: string };

/**
 * How long a buffer whose `ended` flag is set may go without a successor
 * before the stream counts as ended, in milliseconds. A format change sets
 * the flag on the old buffer and posts the new one; the two are observed a
 * few milliseconds apart at most.
 */
export const PCM_STREAM_END_GRACE_MS = 1000;

function newStreamId(): string {
    if (typeof globalThis.crypto !== 'undefined' && typeof globalThis.crypto.randomUUID === 'function') {
        return globalThis.crypto.randomUUID();
    }
    return `stream_${Date.now().toString(36)}_${Math.random().toString(36).slice(2)}`;
}

function toOutcome(answer: AudioSubscribeStreamResponse, subscriptionId: string): PcmStreamOutcome {
    if (typeof answer !== 'object' || answer === null) return { ok: false, code: 'UNKNOWN_ERROR' };
    if (answer.success !== true) {
        return {
            ok: false,
            code: typeof answer.code === 'string' ? answer.code : 'UNKNOWN_ERROR',
            error: typeof answer.error === 'string' ? answer.error : undefined,
        };
    }
    return { ok: true, subscriptionId: typeof answer.subscriptionId === 'string' ? answer.subscriptionId : subscriptionId };
}

/**
 * A live stream subscription. Poll {@link PcmStream.read} from
 * `requestAnimationFrame` or a timer; the host cannot signal the page when
 * frames arrive.
 *
 * The buffers are **read-only shared memory**: {@link PcmStream.read} copies
 * frames out of them, and the class releases each buffer when its epoch is
 * superseded or the stream ends. Frames of the previous epoch that were not
 * read before its successor arrived are discarded with it and counted in the
 * `dropped` of the next read that returns frames.
 * @experimental
 */
export class PcmStream {
    /** The id the host knows this subscription by. */
    readonly subscriptionId: string;
    /** The host's answer to the subscription. Never rejects. */
    readonly ready: Promise<PcmStreamOutcome>;

    private readonly receiver: SharedBufferReceiver | null;
    private reader: PcmRingReader | null = null;
    private buffer: ArrayBuffer | null = null;
    // Unread frames of superseded epochs, not yet reported as dropped
    private carriedDropped = 0;
    private closed = false;
    private endTimer: ReturnType<typeof setTimeout> | null = null;
    private stopBuffers: () => void = () => undefined;
    private offStreamEvent: () => void = () => undefined;
    private readonly formatListeners = new Set<(format: PcmStreamFormat) => void>();
    private readonly endedListeners = new Set<() => void>();

    /** @internal Create through {@link subscribeStream}. */
    constructor(subscriptionId: string, options: PcmStreamOptions) {
        this.subscriptionId = subscriptionId;
        this.receiver = getPageReceiver();
        if (!this.receiver) {
            this.closed = true;
            this.ready = Promise.resolve({
                ok: false,
                code: 'NOT_SUPPORTED',
                error: 'Shared buffers are only available in a WebView2 host page',
            });
            return;
        }
        this.stopBuffers = this.receiver.listen(STREAM_PURPOSE, subscriptionId, (delivery) => this.onBuffer(delivery));
        this.offStreamEvent = subscribe('audio:stream', (payload) => this.onStreamEvent(payload));
        const request: AudioSubscribeStreamParams = { ...options, subscriptionId };
        this.ready = call('audio.subscribeStream', request)
            .then(
                (answer) => toOutcome(answer, subscriptionId),
                (error: unknown): PcmStreamOutcome => ({
                    ok: false,
                    code: 'UNKNOWN_ERROR',
                    error: error instanceof Error ? error.message : String(error),
                }),
            )
            .then((outcome) => {
                if (!outcome.ok) this.finish();
                return outcome;
            });
    }

    /** Format of the buffer being written, or `null` until the first chunk of audio arrived. */
    get format(): PcmStreamFormat | null {
        const reader = this.reader;
        if (!reader) return null;
        return {
            sampleRate: reader.sampleRate,
            channels: reader.channels,
            capacityFrames: reader.capacityFrames,
            epoch: reader.epoch,
        };
    }

    /**
     * Called with the format of the first buffer and again for every later
     * epoch. Returns a function that removes the listener.
     */
    onFormat(listener: (format: PcmStreamFormat) => void): () => void {
        this.formatListeners.add(listener);
        return () => {
            this.formatListeners.delete(listener);
        };
    }

    /**
     * `true` once nothing more will arrive: after {@link PcmStream.unsubscribe},
     * when the host refused the subscription, or when the host stopped writing
     * and no new epoch followed within {@link PCM_STREAM_END_GRACE_MS}.
     */
    get ended(): boolean {
        return this.closed;
    }

    /** Called once when the stream ends; called at once if it already has. */
    onEnded(listener: () => void): () => void {
        if (this.closed) {
            listener();
            return () => undefined;
        }
        this.endedListeners.add(listener);
        return () => {
            this.endedListeners.delete(listener);
        };
    }

    /**
     * Copy out the frames written since the previous call, one `Float32Array`
     * per channel, with the number of frames the ring overwrote before they
     * were read, or that a format change discarded, in `dropped`. `segments`
     * says where media time restarts within the frames and what track time
     * each run begins at.
     *
     * Returns `null` before the first buffer arrived, when nothing new was
     * written (playback stopped or paused), after the stream ended, or when the
     * host kept writing during every attempt; the next call tries again.
     */
    read(): PcmRingRead | null {
        const reader = this.reader;
        if (!reader || this.closed) return null;
        const result = reader.read();
        // The host set the flag when it stopped writing this buffer: a format
        // change, whose successor cancels the timer, or the end of the stream.
        if (reader.ended) this.scheduleEndCheck(reader.epoch);
        if (result && this.carriedDropped > 0) {
            result.dropped += this.carriedDropped;
            this.carriedDropped = 0;
        }
        return result;
    }

    /**
     * Remove the subscription on the host and free the page's view. Safe to
     * call more than once.
     */
    unsubscribe(): void {
        if (this.closed) return;
        call('audio.unsubscribeStream', { subscriptionId: this.subscriptionId })
            .catch(() => undefined);
        this.finish();
    }

    private onBuffer(delivery: SharedBufferDelivery): void {
        const receiver = this.receiver;
        if (!receiver) return;
        if (this.closed) {
            receiver.release(delivery.buffer);
            return;
        }
        let reader: PcmRingReader;
        try {
            reader = new PcmRingReader(delivery.buffer);
        } catch {
            receiver.release(delivery.buffer);
            return;
        }
        if (this.reader && reader.epoch <= this.reader.epoch) {
            receiver.release(delivery.buffer);
            return;
        }
        const previous = this.buffer;
        if (this.reader) this.carriedDropped += this.reader.unreadFrames();
        this.reader = reader;
        this.buffer = delivery.buffer;
        this.clearEndCheck();
        if (previous) receiver.release(previous);
        const format = this.format;
        if (format) for (const listener of [...this.formatListeners]) listener(format);
    }

    private onStreamEvent(payload: AudioStreamPayload): void {
        if (payload.subscriptionId !== this.subscriptionId || payload.type !== 'ended') return;
        // The host ended that epoch and has posted its successor; if the
        // successor never shows up, the grace period ends the stream.
        if (this.reader && this.reader.epoch === payload.epoch) this.scheduleEndCheck(payload.epoch);
    }

    private scheduleEndCheck(epoch: number): void {
        if (this.endTimer !== null) return;
        this.endTimer = setTimeout(() => {
            this.endTimer = null;
            if (this.closed) return;
            if (this.reader && this.reader.epoch === epoch && this.reader.ended) this.finish();
        }, PCM_STREAM_END_GRACE_MS);
    }

    private clearEndCheck(): void {
        if (this.endTimer === null) return;
        clearTimeout(this.endTimer);
        this.endTimer = null;
    }

    private finish(): void {
        if (this.closed) return;
        this.closed = true;
        this.clearEndCheck();
        this.stopBuffers();
        this.offStreamEvent();
        if (this.buffer && this.receiver) this.receiver.release(this.buffer);
        this.buffer = null;
        this.reader = null;
        const listeners = [...this.endedListeners];
        this.endedListeners.clear();
        this.formatListeners.clear();
        for (const listener of listeners) listener();
    }
}

/**
 * Subscribe to the audio foobar2000 is playing and return the handle at once;
 * await {@link PcmStream.ready} to learn whether the host accepted it.
 */
export function subscribeStream(options: PcmStreamOptions = {}): PcmStream {
    return new PcmStream(newStreamId(), options);
}
