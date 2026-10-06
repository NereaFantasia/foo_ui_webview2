import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { PCM_FLAG_ENDED, PCM_OFFSETS } from '../PcmHeader.js';
import type { SharedBufferReceivedEvent } from '../SharedBufferReceiver.js';
import { FakeRingWriter, headerBytesOf, pushSegment } from './pcmBuffer.js';

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
    // Posts a ring the way the host does: header first, then the page gets the event.
    const post = (subscriptionId: string, epoch: number, channels = 2, capacity = 8, version = 1): FakeRingWriter => {
        const writer = new FakeRingWriter(capacity, channels, 0, 0, version);
        new DataView(writer.buffer).setUint32(PCM_OFFSETS.epoch, epoch, true);
        const additionalData = {
            purpose: 'audio.subscribeStream',
            subscriptionId,
            epoch,
            sampleRate: 48000,
            channels,
            capacityFrames: capacity,
            headerBytes: headerBytesOf(version),
        };
        for (const listener of [...listeners]) listener({ getBuffer: () => writer.buffer, additionalData });
        return writer;
    };
    const markEnded = (writer: FakeRingWriter): void => {
        const view = new DataView(writer.buffer);
        view.setUint32(PCM_OFFSETS.flags, view.getUint32(PCM_OFFSETS.flags, true) | PCM_FLAG_ENDED, true);
    };
    const released = (buffer: ArrayBuffer): boolean =>
        webview.releaseBuffer.mock.calls.some((call: unknown[]) => call[0] === buffer);
    const listenerCount = (event: string): number => handlers.get(event)?.size ?? 0;
    return { native, webview, emit, post, markEnded, released, listenerCount };
}

type Host = ReturnType<typeof makeHost>;

function accepting(host: Host): void {
    host.native.invoke.mockImplementation(async (method: string, params: { subscriptionId?: string }) =>
        method === 'audio.subscribeStream'
            ? { success: true, subscriptionId: params.subscriptionId, bufferSeconds: 1 }
            : { success: true, removed: 1 },
    );
}

async function load(host: Host, { webview = true } = {}) {
    vi.stubGlobal('window', webview ? { fb2k: host.native, chrome: { webview: host.webview } } : { fb2k: host.native });
    return import('../PcmStream.js');
}

const flush = () => new Promise<void>((resolve) => setTimeout(resolve, 0));

describe('PcmStream', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.useRealTimers();
        vi.unstubAllGlobals();
    });

    it('reads frames from the buffer that arrives after the host accepted', async () => {
        const host = makeHost();
        accepting(host);
        const { subscribeStream } = await load(host);
        const stream = subscribeStream({ bufferSeconds: 1 });
        await expect(stream.ready).resolves.toEqual({ ok: true, subscriptionId: stream.subscriptionId });
        expect(stream.format).toBeNull();
        expect(stream.read()).toBeNull();

        const formats: unknown[] = [];
        stream.onFormat((format) => formats.push(format));
        const writer = host.post(stream.subscriptionId, 1);
        expect(stream.format).toEqual({ sampleRate: 48000, channels: 2, capacityFrames: 8, epoch: 1 });
        expect(formats).toEqual([{ sampleRate: 48000, channels: 2, capacityFrames: 8, epoch: 1 }]);

        writer.write(3, 1000);
        const chunk = stream.read();
        expect(chunk).not.toBeNull();
        expect(chunk?.frames).toBe(3);
        expect(chunk?.dropped).toBe(0);
        expect(chunk?.hostTimeMs).toBe(1000);
        expect(Array.from(chunk?.planes[0] ?? [])).toEqual([0, 1, 2]);
        expect(Array.from(chunk?.planes[1] ?? [])).toEqual([0.25, 1.25, 2.25]);
        expect(stream.read()).toBeNull();
        expect(stream.ended).toBe(false);
    });

    it('keeps a buffer that arrives before the host answers', async () => {
        const host = makeHost();
        let answer: (value: unknown) => void = () => undefined;
        host.native.invoke.mockImplementation(
            (method: string) =>
                new Promise((resolve) => {
                    if (method === 'audio.subscribeStream') answer = resolve;
                    else resolve({ success: true, removed: 1 });
                }),
        );
        const { subscribeStream } = await load(host);
        const stream = subscribeStream();
        const writer = host.post(stream.subscriptionId, 1);
        writer.write(2);
        expect(stream.read()?.frames).toBe(2);
        answer({ success: true, subscriptionId: stream.subscriptionId, bufferSeconds: 1 });
        await expect(stream.ready).resolves.toMatchObject({ ok: true });
    });

    it('switches to the new epoch, releases the old buffer and ignores stale ones', async () => {
        const host = makeHost();
        accepting(host);
        const { subscribeStream } = await load(host);
        const stream = subscribeStream();
        await stream.ready;
        const first = host.post(stream.subscriptionId, 1, 2);
        first.write(4);
        stream.read();
        const formats: number[] = [];
        stream.onFormat((format) => formats.push(format.epoch));

        host.markEnded(first);
        const second = host.post(stream.subscriptionId, 2, 1);
        expect(stream.format).toMatchObject({ channels: 1, epoch: 2 });
        expect(formats).toEqual([2]);
        expect(host.released(first.buffer)).toBe(true);
        expect(host.released(second.buffer)).toBe(false);

        second.write(2);
        expect(stream.read()?.planes).toHaveLength(1);

        const stale = host.post(stream.subscriptionId, 1, 2);
        expect(host.released(stale.buffer)).toBe(true);
        expect(stream.format?.epoch).toBe(2);
        expect(stream.ended).toBe(false);
    });

    it('counts the frames a format change discarded in the next read that returns frames', async () => {
        const host = makeHost();
        accepting(host);
        const { subscribeStream } = await load(host);
        const stream = subscribeStream();
        await stream.ready;
        const first = host.post(stream.subscriptionId, 1, 2, 8);
        first.write(3);
        expect(stream.read()?.dropped).toBe(0);
        // Five frames of the old format that nobody reads before the new buffer arrives.
        first.write(5);
        host.markEnded(first);
        const second = host.post(stream.subscriptionId, 2, 2, 8);
        expect(stream.read()).toBeNull();
        second.write(2);
        const r = stream.read();
        expect(r?.frames).toBe(2);
        expect(r?.dropped).toBe(5);
        expect(r?.segments).toEqual([{ offset: 0, segment: null, startSeconds: null, reason: null, estimated: false }]);
        second.write(1);
        expect(stream.read()?.dropped).toBe(0);
    });

    it('passes on a segment number the new epoch continues', async () => {
        const host = makeHost();
        accepting(host);
        const { subscribeStream } = await load(host);
        const stream = subscribeStream();
        await stream.ready;
        const first = host.post(stream.subscriptionId, 1, 2, 480, 2);
        pushSegment(first.buffer, { segment: 3, startFrame: 0, reason: 2, startSeconds: 12 });
        first.write(48);
        expect(stream.read()?.segments).toEqual([{ offset: 0, segment: 3, startSeconds: 12, reason: 'seek', estimated: false }]);
        host.markEnded(first);
        const second = host.post(stream.subscriptionId, 2, 1, 441, 2);
        pushSegment(second.buffer, { segment: 3, startFrame: 0, reason: 5, estimated: true, startSeconds: 12.001 });
        second.write(10);
        expect(stream.read()?.segments).toEqual([{ offset: 0, segment: 3, startSeconds: 12.001, reason: 'format', estimated: true }]);
    });

    it('ends when the host stopped writing and no new epoch followed within the grace period', async () => {
        vi.useFakeTimers();
        const host = makeHost();
        accepting(host);
        const { subscribeStream, PCM_STREAM_END_GRACE_MS } = await load(host);
        const stream = subscribeStream();
        await stream.ready;
        const writer = host.post(stream.subscriptionId, 1);
        writer.write(1);
        const ended = vi.fn();
        stream.onEnded(ended);

        host.markEnded(writer);
        expect(stream.read()?.frames).toBe(1);
        expect(stream.ended).toBe(false);
        await vi.advanceTimersByTimeAsync(PCM_STREAM_END_GRACE_MS + 10);
        expect(stream.ended).toBe(true);
        expect(ended).toHaveBeenCalledTimes(1);
        expect(host.released(writer.buffer)).toBe(true);
        expect(stream.read()).toBeNull();
        expect(host.listenerCount('audio:stream')).toBe(0);
    });

    it('does not end on a format change whose successor arrives within the grace period', async () => {
        vi.useFakeTimers();
        const host = makeHost();
        accepting(host);
        const { subscribeStream, PCM_STREAM_END_GRACE_MS } = await load(host);
        const stream = subscribeStream();
        await stream.ready;
        const first = host.post(stream.subscriptionId, 1);
        const ended = vi.fn();
        stream.onEnded(ended);

        host.markEnded(first);
        host.emit('audio:stream', {
            subscriptionId: stream.subscriptionId,
            type: 'ended',
            epoch: 1,
            reason: 'format-change',
        });
        stream.read();
        await vi.advanceTimersByTimeAsync(PCM_STREAM_END_GRACE_MS / 2);
        host.post(stream.subscriptionId, 2);
        await vi.advanceTimersByTimeAsync(PCM_STREAM_END_GRACE_MS);
        expect(stream.ended).toBe(false);
        expect(ended).not.toHaveBeenCalled();
        expect(stream.format?.epoch).toBe(2);
    });

    it('unsubscribe tells the host once, releases the buffer and fires onEnded', async () => {
        const host = makeHost();
        accepting(host);
        const { subscribeStream } = await load(host);
        const stream = subscribeStream();
        await stream.ready;
        const writer = host.post(stream.subscriptionId, 1);
        const ended = vi.fn();
        stream.onEnded(ended);

        stream.unsubscribe();
        stream.unsubscribe();
        await flush();
        const unsubscribes = host.native.invoke.mock.calls.filter((c: unknown[]) => c[0] === 'audio.unsubscribeStream');
        expect(unsubscribes).toEqual([['audio.unsubscribeStream', { subscriptionId: stream.subscriptionId }]]);
        expect(stream.ended).toBe(true);
        expect(ended).toHaveBeenCalledTimes(1);
        expect(host.released(writer.buffer)).toBe(true);
        expect(stream.format).toBeNull();
        // A listener added afterwards is told at once.
        const late = vi.fn();
        stream.onEnded(late);
        expect(late).toHaveBeenCalledTimes(1);
    });

    it('resolves ready with the host code, ends, and releases a buffer that still arrives', async () => {
        const host = makeHost();
        host.native.invoke.mockResolvedValue({
            success: false,
            code: 'OPERATION_FAILED',
            error: 'This page already has 8 stream subscriptions',
        });
        const { subscribeStream } = await load(host);
        const stream = subscribeStream();
        await expect(stream.ready).resolves.toEqual({
            ok: false,
            code: 'OPERATION_FAILED',
            error: 'This page already has 8 stream subscriptions',
        });
        expect(stream.ended).toBe(true);
        const writer = host.post(stream.subscriptionId, 1);
        await flush();
        expect(host.released(writer.buffer)).toBe(false); // held by the receiver, released when unclaimed
        expect(stream.read()).toBeNull();
    });

    it('passes on the code of a failure envelope and maps a transport failure to UNKNOWN_ERROR', async () => {
        const refusing = makeHost();
        refusing.native.invoke.mockResolvedValue({ success: false, code: 'NOT_SUPPORTED', error: 'no stream' });
        let { subscribeStream } = await load(refusing);
        await expect(subscribeStream().ready).resolves.toEqual({ ok: false, code: 'NOT_SUPPORTED', error: 'no stream' });

        vi.resetModules();
        const broken = makeHost();
        broken.native.invoke.mockRejectedValue(new Error('transport down'));
        ({ subscribeStream } = await load(broken));
        await expect(subscribeStream().ready).resolves.toEqual({
            ok: false,
            code: 'UNKNOWN_ERROR',
            error: 'transport down',
        });
    });

    it('answers NOT_SUPPORTED on a WebView2 page without the injected bridge', async () => {
        vi.useFakeTimers();
        const host = makeHost();
        vi.stubGlobal('window', { chrome: { webview: host.webview } });
        const { subscribeStream } = await import('../PcmStream.js');
        const ready = subscribeStream().ready;
        await vi.advanceTimersByTimeAsync(200);
        await expect(ready).resolves.toMatchObject({ ok: false, code: 'NOT_SUPPORTED' });
    });

    it('answers NOT_SUPPORTED without a WebView2 page', async () => {
        const host = makeHost();
        const { subscribeStream } = await load(host, { webview: false });
        const stream = subscribeStream();
        await expect(stream.ready).resolves.toMatchObject({ ok: false, code: 'NOT_SUPPORTED' });
        expect(stream.ended).toBe(true);
        expect(host.native.invoke).not.toHaveBeenCalled();
    });
});
