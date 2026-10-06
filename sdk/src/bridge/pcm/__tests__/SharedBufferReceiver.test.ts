import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import {
    SharedBufferReceiver,
    stringField,
    type SharedBufferDelivery,
    type SharedBufferHost,
    type SharedBufferReceivedEvent,
} from '../SharedBufferReceiver.js';

class FakeHost implements SharedBufferHost {
    listeners: Array<(event: SharedBufferReceivedEvent) => void> = [];
    released: ArrayBuffer[] = [];

    addEventListener(_type: 'sharedbufferreceived', listener: (event: SharedBufferReceivedEvent) => void): void {
        this.listeners.push(listener);
    }

    removeEventListener(_type: 'sharedbufferreceived', listener: (event: SharedBufferReceivedEvent) => void): void {
        this.listeners = this.listeners.filter((l) => l !== listener);
    }

    releaseBuffer(buffer: ArrayBuffer): void {
        this.released.push(buffer);
    }

    post(additionalData: unknown): ArrayBuffer {
        const buffer = new ArrayBuffer(8);
        for (const listener of this.listeners) listener({ getBuffer: () => buffer, additionalData });
        return buffer;
    }
}

function makeReceiver(host: FakeHost): SharedBufferReceiver {
    const receiver = new SharedBufferReceiver(host);
    receiver.definePurpose('audio.decodePcm', (data) => stringField(data, 'taskId'));
    return receiver;
}

describe('SharedBufferReceiver', () => {
    beforeEach(() => {
        vi.useFakeTimers();
    });
    afterEach(() => {
        vi.useRealTimers();
    });

    it('releases buffers of unknown purposes and buffers without a key at once', () => {
        const host = new FakeHost();
        makeReceiver(host);
        const unknown = host.post({ purpose: 'something.else', taskId: 'x' });
        const bare = host.post(undefined);
        const noKey = host.post({ purpose: 'audio.decodePcm' });
        expect(host.released).toEqual([unknown, bare, noKey]);
    });

    it('hands a buffer to the consumer listening for its key', () => {
        const host = new FakeHost();
        const receiver = makeReceiver(host);
        const got: SharedBufferDelivery[] = [];
        receiver.listen('audio.decodePcm', 'pcm_1', (d) => got.push(d));
        const buffer = host.post({ purpose: 'audio.decodePcm', taskId: 'pcm_1', frames: 3 });
        expect(got).toHaveLength(1);
        expect(got[0].buffer).toBe(buffer);
        expect(Reflect.get(got[0].additionalData, 'frames')).toBe(3);
        expect(host.released).toEqual([]);
    });

    it('holds a buffer that arrives before its consumer and delivers it on listen', () => {
        const host = new FakeHost();
        const receiver = makeReceiver(host);
        const buffer = host.post({ purpose: 'audio.decodePcm', taskId: 'pcm_2' });
        vi.advanceTimersByTime(4000);
        const got: ArrayBuffer[] = [];
        receiver.listen('audio.decodePcm', 'pcm_2', (d) => got.push(d.buffer));
        expect(got).toEqual([buffer]);
        vi.advanceTimersByTime(10000);
        expect(host.released).toEqual([]);
    });

    it('releases a held buffer nobody claims within the hold time', () => {
        const host = new FakeHost();
        makeReceiver(host);
        const buffer = host.post({ purpose: 'audio.decodePcm', taskId: 'pcm_3' });
        vi.advanceTimersByTime(4999);
        expect(host.released).toEqual([]);
        vi.advanceTimersByTime(1);
        expect(host.released).toEqual([buffer]);
    });

    it('stops delivering after the returned function is called', () => {
        const host = new FakeHost();
        const receiver = makeReceiver(host);
        const got: ArrayBuffer[] = [];
        const stop = receiver.listen('audio.decodePcm', 'pcm_4', (d) => got.push(d.buffer));
        stop();
        const buffer = host.post({ purpose: 'audio.decodePcm', taskId: 'pcm_4' });
        expect(got).toEqual([]);
        vi.advanceTimersByTime(5000);
        expect(host.released).toEqual([buffer]);
    });

    it('dispose removes the listener and releases what is still held', () => {
        const host = new FakeHost();
        const receiver = makeReceiver(host);
        const held = host.post({ purpose: 'audio.decodePcm', taskId: 'pcm_5' });
        receiver.dispose();
        expect(host.listeners).toHaveLength(0);
        expect(host.released).toEqual([held]);
    });
});
