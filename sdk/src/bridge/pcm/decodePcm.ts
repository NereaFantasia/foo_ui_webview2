/**
 * `audio.decodePcm` for pages: sends the request, then pairs the shared buffer
 * with the `audio:pcmReady` event of the same task, which may arrive in either
 * order.
 */

import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import { PcmBuffer } from './PcmBuffer.js';
import { DECODE_PCM_PURPOSE, getPageReceiver } from './pageReceiver.js';
import { SHARED_BUFFER_HOLD_MS, type SharedBufferDelivery } from './SharedBufferReceiver.js';
import type { AudioPcmFailedPayload, AudioPcmReadyPayload } from '../../types/events.js';
import type { AudioDecodePcmParams } from '../../types/generated/params.js';
import type {
    AudioDecodePcmResponse,
} from '../../types/generated/responses.js';

/** How long {@link decodePcm} waits for the result by default, in milliseconds. */
export const DECODE_PCM_TIMEOUT_MS = 120000;

/** Options of {@link decodePcm}; the range and format fields go to the host. */
export interface DecodePcmOptions extends Omit<AudioDecodePcmParams, 'path'> {
    /** Aborting cancels the host task and rejects with a `DOMException` named `AbortError`. */
    signal?: AbortSignal;
    /**
     * Milliseconds to wait for the result, queueing included, before cancelling
     * the host task and rejecting with code `TIMEOUT`; `0` waits forever.
     * Defaults to {@link DECODE_PCM_TIMEOUT_MS}.
     */
    timeoutMs?: number;
}

/** Why {@link decodePcm} rejected, with the host's error code. */
export class PcmDecodeError extends Error {
    /**
     * The host's code (`INVALID_PARAMS`, `NO_INFO`, `NOT_SUPPORTED`,
     * `CANCELLED`, ...), or `TIMEOUT` when the wait ran out.
     */
    readonly code: string;
    /** Set once the host has accepted the request. */
    readonly taskId?: string;

    constructor(code: string, message: string, taskId?: string) {
        super(message);
        this.name = 'PcmDecodeError';
        this.code = code;
        this.taskId = taskId;
    }
}

function abortError(): DOMException {
    return new DOMException('The operation was aborted.', 'AbortError');
}

function hasTaskId(value: unknown, taskId: string): boolean {
    return typeof value === 'object' && value !== null && Reflect.get(value, 'taskId') === taskId;
}

/**
 * Decode a track, or a range of it, and resolve with its samples.
 *
 * Rejects with {@link PcmDecodeError} when the host refuses the request or
 * the task fails; its `code` is `NOT_SUPPORTED` outside a WebView2 host or on
 * a runtime without shared buffers. An aborted `signal` rejects with a
 * `DOMException` named `AbortError`. Both an abort and a timeout cancel the
 * host task.
 *
 * @param path - Track path; a `|subsong:N` suffix selects a subsong.
 */
export async function decodePcm(path: string, options: DecodePcmOptions = {}): Promise<PcmBuffer> {
    const { signal, timeoutMs = DECODE_PCM_TIMEOUT_MS, ...params } = options;
    if (signal?.aborted) throw abortError();
    const receiver = getPageReceiver();
    if (!receiver) {
        throw new PcmDecodeError('NOT_SUPPORTED', 'Shared buffers are only available in a WebView2 host page');
    }

    // The ready or failed event of a task that finishes quickly can land before
    // this function learns the taskId, so events are collected from the start.
    const early: Array<{ ready?: AudioPcmReadyPayload; failed?: AudioPcmFailedPayload }> = [];
    let onReady = (payload: AudioPcmReadyPayload): void => {
        early.push({ ready: payload });
    };
    let onFailed = (payload: AudioPcmFailedPayload): void => {
        early.push({ failed: payload });
    };
    const offReady = subscribe('audio:pcmReady', (payload) => onReady(payload));
    const offFailed = subscribe('audio:pcmFailed', (payload) => onFailed(payload));

    let answer: AudioDecodePcmResponse;
    try {
        // path last: an untyped caller's options.path must not override the argument.
        const request: AudioDecodePcmParams = { ...params, path };
        answer = await call('audio.decodePcm', request);
    } catch (error) {
        offReady();
        offFailed();
        throw error;
    }
    if (answer.success !== true || typeof answer.taskId !== 'string') {
        offReady();
        offFailed();
        if (answer.success === false) throw new PcmDecodeError(answer.code, answer.error);
        throw new PcmDecodeError('UNKNOWN_ERROR', 'audio.decodePcm returned no taskId');
    }
    const taskId = answer.taskId;
    const cancelHostTask = (): void => {
        // Fire and forget: the host answers cancelled false once the task has settled.
        call('audio.cancelDecodePcm', { taskId })
            .catch(() => undefined);
    };

    return new Promise<PcmBuffer>((resolve, reject) => {
        let ready: AudioPcmReadyPayload | null = null;
        let delivery: SharedBufferDelivery | null = null;
        let waitTimer: ReturnType<typeof setTimeout> | null = null;
        let pairTimer: ReturnType<typeof setTimeout> | null = null;
        let settled = false;
        let stopListening = (): void => undefined;

        const settle = (): boolean => {
            if (settled) return false;
            settled = true;
            offReady();
            offFailed();
            stopListening();
            if (waitTimer) clearTimeout(waitTimer);
            if (pairTimer) clearTimeout(pairTimer);
            signal?.removeEventListener('abort', onAbort);
            return true;
        };
        const fail = (error: unknown): void => {
            const pending = delivery;
            if (!settle()) return;
            if (pending) receiver.release(pending.buffer);
            reject(error);
        };
        const tryResolve = (): void => {
            if (settled) return;
            if (!ready || !delivery) {
                // The host sends both halves together; one missing for this long will not come.
                pairTimer ??= setTimeout(
                    () =>
                        fail(
                            new PcmDecodeError(
                                'UNKNOWN_ERROR',
                                'Only one of the shared buffer and audio:pcmReady arrived',
                                taskId,
                            ),
                        ),
                    SHARED_BUFFER_HOLD_MS,
                );
                return;
            }
            const buffer = delivery.buffer;
            try {
                const pcm = new PcmBuffer(buffer, (b) => receiver.release(b));
                settle();
                resolve(pcm);
            } catch (error) {
                fail(
                    new PcmDecodeError(
                        'NOT_SUPPORTED',
                        error instanceof Error ? error.message : String(error),
                        taskId,
                    ),
                );
            }
        };
        const onAbort = (): void => {
            cancelHostTask();
            fail(abortError());
        };

        onReady = (payload) => {
            if (!hasTaskId(payload, taskId) || ready) return;
            ready = payload;
            tryResolve();
        };
        onFailed = (payload) => {
            if (!hasTaskId(payload, taskId)) return;
            fail(new PcmDecodeError(payload.code, payload.error, taskId));
        };
        stopListening = receiver.listen(DECODE_PCM_PURPOSE, taskId, (incoming) => {
            if (delivery || settled) {
                receiver.release(incoming.buffer);
                return;
            }
            delivery = incoming;
            tryResolve();
        });
        signal?.addEventListener('abort', onAbort, { once: true });
        if (timeoutMs > 0) {
            waitTimer = setTimeout(() => {
                cancelHostTask();
                fail(new PcmDecodeError('TIMEOUT', `audio.decodePcm timed out after ${timeoutMs} ms`, taskId));
            }, timeoutMs);
        }
        for (const event of early.splice(0)) {
            if (event.ready) onReady(event.ready);
            else if (event.failed) onFailed(event.failed);
        }
        if (signal?.aborted) onAbort();
    });
}
