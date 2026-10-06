import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { PCM_FLAG_ENDED, PCM_MODE_ONE_SHOT } from '../PcmHeader.js';
import type { SharedBufferReceivedEvent } from '../SharedBufferReceiver.js';
import { makePcmBuffer } from './pcmBuffer.js';

type Handler = (data: unknown) => void;

// Stand-ins for the injected bridge (window.fb2k) and window.chrome.webview.
function makeHost() {
    const handlers = new Map<string, Set<Handler>>();
    const native = {
        invoke: vi.fn(),
        on: vi.fn((event: string, handler: Handler) => {
            const set = handlers.get(event) ?? new Set<Handler>();
            set.add(handler);
            handlers.set(event, set);
        }),
        off: vi.fn((event: string, handler: Handler) => {
            handlers.get(event)?.delete(handler);
        }),
    };
    const listeners = new Set<(event: SharedBufferReceivedEvent) => void>();
    const webview = {
        addEventListener: vi.fn((_type: string, listener: (event: SharedBufferReceivedEvent) => void) => {
            listeners.add(listener);
        }),
        removeEventListener: vi.fn((_type: string, listener: (event: SharedBufferReceivedEvent) => void) => {
            listeners.delete(listener);
        }),
        releaseBuffer: vi.fn(),
    };
    const emit = (event: string, payload: unknown): void => {
        for (const handler of [...(handlers.get(event) ?? [])]) handler(payload);
    };
    const post = (taskId: string, frames = 4): ArrayBuffer => {
        const buffer = makePcmBuffer({
            mode: PCM_MODE_ONE_SHOT,
            channels: 1,
            capacityFrames: 8,
            writeFrames: frames,
            seq: 2,
            flags: PCM_FLAG_ENDED,
            sampleRate: 8000,
        });
        const additionalData = { purpose: 'audio.decodePcm', taskId, sampleRate: 8000, channels: 1, frames };
        for (const listener of [...listeners]) listener({ getBuffer: () => buffer, additionalData });
        return buffer;
    };
    const listenerCount = (event: string): number => handlers.get(event)?.size ?? 0;
    // By identity: vitest's toHaveBeenCalledWith compares ArrayBuffers by content.
    const released = (buffer: ArrayBuffer): boolean =>
        webview.releaseBuffer.mock.calls.some((call: unknown[]) => call[0] === buffer);
    return { native, webview, emit, post, listenerCount, released };
}

type Host = ReturnType<typeof makeHost>;

function readyPayload(taskId: string, frames = 4) {
    return {
        taskId,
        path: 'C:\\a.flac',
        sampleRate: 8000,
        channels: 1,
        frames,
        start: 0,
        end: frames / 8000,
        duration: frames / 8000,
        truncated: false,
        resampled: false,
    };
}

async function load(host: Host, { webview = true } = {}) {
    vi.stubGlobal('window', webview ? { fb2k: host.native, chrome: { webview: host.webview } } : { fb2k: host.native });
    return import('../decodePcm.js');
}

// Resolves pending promise continuations, such as the one after the invoke answer.
const flush = () => new Promise<void>((resolve) => setTimeout(resolve, 0));

describe('decodePcm', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.useRealTimers();
        vi.unstubAllGlobals();
    });

    it('resolves once both the buffer and audio:pcmReady arrived, buffer first', async () => {
        const host = makeHost();
        host.native.invoke.mockImplementation(async (method: string) => {
            if (method !== 'audio.decodePcm') return { success: true };
            // The buffer can reach the page before the invoke answer does.
            host.post('pcm_1');
            return { success: true, taskId: 'pcm_1', status: 'pending' };
        });
        const { decodePcm } = await load(host);
        const promise = decodePcm('C:\\a.flac', { start: 1, sampleRate: 8000 });
        await flush();
        host.emit('audio:pcmReady', readyPayload('pcm_1'));
        const pcm = await promise;
        expect(pcm.frames).toBe(4);
        expect(pcm.sampleRate).toBe(8000);
        expect(host.native.invoke).toHaveBeenCalledWith('audio.decodePcm', { start: 1, sampleRate: 8000, path: 'C:\\a.flac' });
        expect(host.listenerCount('audio:pcmReady')).toBe(0);
        expect(host.listenerCount('audio:pcmFailed')).toBe(0);
        expect(host.webview.releaseBuffer).not.toHaveBeenCalled();
    });

    it('resolves when audio:pcmReady comes before the buffer', async () => {
        const host = makeHost();
        host.native.invoke.mockResolvedValue({ success: true, taskId: 'pcm_2', status: 'pending' });
        const { decodePcm } = await load(host);
        const promise = decodePcm('C:\\a.flac');
        await flush();
        host.emit('audio:pcmReady', readyPayload('pcm_2'));
        const buffer = host.post('pcm_2');
        const pcm = await promise;
        expect(pcm.getChannelView(0).buffer).toBe(buffer);
    });

    it('ignores buffers and events of other tasks', async () => {
        vi.useFakeTimers();
        const host = makeHost();
        host.native.invoke.mockResolvedValue({ success: true, taskId: 'pcm_3', status: 'pending' });
        const { decodePcm } = await load(host);
        const promise = decodePcm('C:\\a.flac');
        await vi.advanceTimersByTimeAsync(0);
        host.emit('audio:pcmReady', readyPayload('pcm_9'));
        const foreign = host.post('pcm_9');
        host.emit('audio:pcmReady', readyPayload('pcm_3'));
        const mine = host.post('pcm_3');
        const pcm = await promise;
        expect(pcm.getChannelView(0).buffer).toBe(mine);
        // Unclaimed buffers of a known purpose are released when their hold runs out.
        await vi.advanceTimersByTimeAsync(5000);
        expect(host.released(foreign)).toBe(true);
        expect(host.released(mine)).toBe(false);
    });

    it('keeps events that arrive before the invoke answer', async () => {
        const host = makeHost();
        host.native.invoke.mockImplementation(async () => {
            host.emit('audio:pcmReady', readyPayload('pcm_10'));
            host.post('pcm_10');
            return { success: true, taskId: 'pcm_10', status: 'pending' };
        });
        const { decodePcm } = await load(host);
        const pcm = await decodePcm('C:\\a.flac');
        expect(pcm.frames).toBe(4);
    });

    it('is not completed by the ready event of another task', async () => {
        vi.useFakeTimers();
        const host = makeHost();
        host.native.invoke.mockResolvedValue({ success: true, taskId: 'pcm_11', status: 'pending' });
        const { decodePcm } = await load(host);
        const outcome = decodePcm('C:\\a.flac').catch((e: unknown) => e);
        await vi.advanceTimersByTimeAsync(0);
        host.post('pcm_11');
        // A ready event for another task must not complete this one.
        host.emit('audio:pcmReady', readyPayload('pcm_12'));
        await vi.advanceTimersByTimeAsync(5000);
        expect(await outcome).toMatchObject({ code: 'UNKNOWN_ERROR', taskId: 'pcm_11' });
    });

    it('rejects with the host code when the task fails', async () => {
        const host = makeHost();
        host.native.invoke.mockResolvedValue({ success: true, taskId: 'pcm_4', status: 'pending' });
        const { decodePcm, PcmDecodeError } = await load(host);
        const promise = decodePcm('C:\\a.flac', { start: 99 });
        await flush();
        host.emit('audio:pcmFailed', { taskId: 'pcm_4', path: 'C:\\a.flac', code: 'INVALID_PARAMS', error: 'start is at or beyond the end of the track' });
        const error = await promise.catch((e: unknown) => e);
        expect(error).toBeInstanceOf(PcmDecodeError);
        expect(error).toMatchObject({ code: 'INVALID_PARAMS', taskId: 'pcm_4' });
    });

    it('is not failed by the failure event of another task', async () => {
        const host = makeHost();
        host.native.invoke.mockResolvedValue({ success: true, taskId: 'pcm_13', status: 'pending' });
        const { decodePcm } = await load(host);
        const promise = decodePcm('C:\\a.flac');
        await flush();
        host.emit('audio:pcmFailed', { taskId: 'pcm_14', path: 'C:\\b.flac', code: 'DECODE_FAILED', error: 'x' });
        host.emit('audio:pcmReady', readyPayload('pcm_13'));
        host.post('pcm_13');
        await expect(promise).resolves.toMatchObject({ frames: 4 });
    });

    it('rejects a refused request with its code and leaves no listeners behind', async () => {
        const host = makeHost();
        host.native.invoke.mockResolvedValue({ success: false, code: 'NO_INFO', error: 'Failed to get audio info' });
        const { decodePcm } = await load(host);
        await expect(decodePcm('C:\\a.flac')).rejects.toMatchObject({ code: 'NO_INFO', message: 'Failed to get audio info' });
        expect(host.listenerCount('audio:pcmReady')).toBe(0);
        expect(host.listenerCount('audio:pcmFailed')).toBe(0);
    });

    it('answers NOT_SUPPORTED without a WebView2 page or without a host', async () => {
        const host = makeHost();
        const withoutWebview = await load(host, { webview: false });
        await expect(withoutWebview.decodePcm('C:\\a.flac')).rejects.toMatchObject({ code: 'NOT_SUPPORTED' });
        expect(host.native.invoke).not.toHaveBeenCalled();

        vi.resetModules();
        vi.stubGlobal('window', { chrome: { webview: host.webview } });
        const withoutHost = await import('../decodePcm.js');
        await expect(withoutHost.decodePcm('C:\\a.flac')).rejects.toMatchObject({ code: 'NOT_SUPPORTED' });
    });

    it('does not call the host when the signal is already aborted', async () => {
        const host = makeHost();
        const { decodePcm } = await load(host);
        const controller = new AbortController();
        controller.abort();
        await expect(decodePcm('C:\\a.flac', { signal: controller.signal })).rejects.toMatchObject({ name: 'AbortError' });
        expect(host.native.invoke).not.toHaveBeenCalled();
    });

    it('cancels the host task and rejects with AbortError when aborted while waiting', async () => {
        const host = makeHost();
        host.native.invoke.mockImplementation(async (method: string) =>
            method === 'audio.decodePcm'
                ? { success: true, taskId: 'pcm_5', status: 'pending' }
                : { success: true, cancelled: true },
        );
        const { decodePcm } = await load(host);
        const controller = new AbortController();
        const promise = decodePcm('C:\\a.flac', { signal: controller.signal });
        await flush();
        controller.abort();
        await expect(promise).rejects.toMatchObject({ name: 'AbortError' });
        expect(host.native.invoke).toHaveBeenCalledWith('audio.cancelDecodePcm', { taskId: 'pcm_5' });
        // The CANCELLED event the host sends for this cancel finds no listener.
        expect(host.listenerCount('audio:pcmFailed')).toBe(0);
    });

    it('cancels the host task and rejects with TIMEOUT when the wait runs out', async () => {
        vi.useFakeTimers();
        const host = makeHost();
        host.native.invoke.mockImplementation(async (method: string) =>
            method === 'audio.decodePcm'
                ? { success: true, taskId: 'pcm_6', status: 'pending' }
                : { success: true, cancelled: true },
        );
        const { decodePcm } = await load(host);
        const promise = decodePcm('C:\\a.flac', { timeoutMs: 1000 });
        const outcome = promise.catch((e: unknown) => e);
        await vi.advanceTimersByTimeAsync(1000);
        expect(await outcome).toMatchObject({ code: 'TIMEOUT', taskId: 'pcm_6' });
        expect(host.native.invoke).toHaveBeenCalledWith('audio.cancelDecodePcm', { taskId: 'pcm_6' });
    });

    it('gives up 5 s after audio:pcmReady when its buffer never comes', async () => {
        vi.useFakeTimers();
        const host = makeHost();
        host.native.invoke.mockResolvedValue({ success: true, taskId: 'pcm_7', status: 'pending' });
        const { decodePcm } = await load(host);
        const outcome = decodePcm('C:\\a.flac').catch((e: unknown) => e);
        await vi.advanceTimersByTimeAsync(0);
        host.emit('audio:pcmReady', readyPayload('pcm_7'));
        await vi.advanceTimersByTimeAsync(4999);
        host.emit('audio:pcmReady', readyPayload('pcm_7'));
        await vi.advanceTimersByTimeAsync(1);
        expect(await outcome).toMatchObject({ code: 'UNKNOWN_ERROR', taskId: 'pcm_7' });
    });

    it('releases a buffer whose audio:pcmReady never comes', async () => {
        vi.useFakeTimers();
        const host = makeHost();
        host.native.invoke.mockResolvedValue({ success: true, taskId: 'pcm_8', status: 'pending' });
        const { decodePcm } = await load(host);
        const outcome = decodePcm('C:\\a.flac').catch((e: unknown) => e);
        await vi.advanceTimersByTimeAsync(0);
        const buffer = host.post('pcm_8');
        await vi.advanceTimersByTimeAsync(5000);
        expect(await outcome).toMatchObject({ code: 'UNKNOWN_ERROR' });
        expect(host.released(buffer)).toBe(true);
    });
});
