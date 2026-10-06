// sdk/src/bridge/namespaces/audio.test.ts
//
// Regression guards for `audio` namespace methods that are sensitive to
// host-side stub state:
//
//   `audio.subscribeStream` bridges into a C++ handler that returns
//   `{success: false, error: "Stream capture requires
//   playback_stream_capture integration"}` and never emits
//   `audio:stream`. The SDK now (a) carries an `@deprecated` JSDoc and
//   (b) surfaces a `console.warn` when the host response confirms the
//   stub state, so callers stop registering listeners that will never
//   fire.
//
//   `audio.subscribeSpectrum` returns an unsubscribe callback whose `ready`
//   promise maps the host's answer (or its absence) onto an outcome and
//   never rejects; its listener drops frames tagged with another
//   subscription's id.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
}

function makeNative(): MockNative {
    return {
        invoke: vi.fn(),
        on: vi.fn().mockReturnValue(() => {
            /* default unsubscribe no-op */
        }),
        off: vi.fn(),
    };
}

describe('audio.subscribeStream (host stub guard)', () => {
    let warnSpy: ReturnType<typeof vi.spyOn>;

    beforeEach(() => {
        vi.resetModules();
        warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {
            /* swallow during assertion */
        });
    });

    afterEach(() => {
        vi.unstubAllGlobals();
        warnSpy.mockRestore();
    });

    it('warns when the host returns success=false with the stub error', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            error: 'Stream capture requires playback_stream_capture integration',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const cb = vi.fn();
        const unsub = audio.subscribeStream(cb);

        // Drain the microtask queue so the warning Promise resolves.
        await new Promise<void>((resolve) => setTimeout(resolve, 0));

        expect(warnSpy).toHaveBeenCalledTimes(1);
        const msg = String(warnSpy.mock.calls[0][0]);
        expect(msg).toContain('audio.subscribeStream');
        expect(msg).toContain('callback will never fire');
        expect(msg).toContain('playback_stream_capture integration');

        // Unsubscribe is still callable and triggers the cleanup invoke.
        expect(typeof unsub).toBe('function');
        unsub();
        const apis = native.invoke.mock.calls.map((c) => c[0]);
        expect(apis).toContain('audio.subscribeStream');
        expect(apis).toContain('audio.unsubscribeStream');
    });

    it('does not warn when the host returns success=true', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        audio.subscribeStream(() => {
            /* never called */
        });
        await new Promise<void>((resolve) => setTimeout(resolve, 0));

        expect(warnSpy).not.toHaveBeenCalled();
    });

    it('does not throw when bridge.invoke rejects', async () => {
        const native = makeNative();
        native.invoke.mockRejectedValue(new Error('transport down'));
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const unsub = audio.subscribeStream(() => {
            /* unused */
        });
        await new Promise<void>((resolve) => setTimeout(resolve, 0));

        expect(typeof unsub).toBe('function');
        // Rejected invoke should not surface as a console.warn — the
        // bridge layer already owns transport-error reporting.
        expect(warnSpy).not.toHaveBeenCalled();
    });

    it('registers `audio:stream` on the bridge so cleanup is symmetrical', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: false, error: 'stub' });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const cb = vi.fn();
        const unsub = audio.subscribeStream(cb);
        await new Promise<void>((resolve) => setTimeout(resolve, 0));

        expect(native.on).toHaveBeenCalledWith('audio:stream', cb);

        // The SDK's `bridge.on` ignores `native.on`'s return value and
        // returns its own thunk that hits `native.off(event, handler)`,
        // so the symmetrical-cleanup contract is checked there.
        unsub();
        expect(native.off).toHaveBeenCalledWith('audio:stream', cb);
    });
});

describe('audio.subscribeSpectrum (ready outcome and frame filtering)', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
        vi.useRealTimers();
    });

    it('resolves ready with ok: true and the registered values', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            subscriptionId: 'from-host',
            fftSize: 2048,
            bands: 64,
            fps: 30,
            scale: 'db',
            backgroundThrottle: false,
            minFrequency: 500,
            maxFrequency: 2000,
            event: 'audio:spectrum',
            streamReady: true,
        });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const stop = audio.subscribeSpectrum(() => undefined, {
            bands: 64,
            fftSize: 2048,
            scale: 'db',
            backgroundThrottle: false,
            minFrequency: 500,
            maxFrequency: 2000,
        });
        await expect(stop.ready).resolves.toEqual({
            ok: true,
            subscriptionId: 'from-host',
            fftSize: 2048,
            bands: 64,
            fps: 30,
            scale: 'db',
            backgroundThrottle: false,
            minFrequency: 500,
            maxFrequency: 2000,
            streamReady: true,
        });
        const [method, params] = native.invoke.mock.calls[0];
        expect(method).toBe('audio.subscribeSpectrum');
        expect(params).toMatchObject({
            bands: 64,
            fftSize: 2048,
            scale: 'db',
            backgroundThrottle: false,
            minFrequency: 500,
            maxFrequency: 2000,
        });
    });

    it('fills scale, backgroundThrottle, the range and streamReady for hosts that do not report them', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, subscriptionId: 'x', fftSize: 1024, bands: 48, fps: 30 });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const outcome = await audio.subscribeSpectrum(() => undefined).ready;
        expect(outcome).toMatchObject({
            ok: true,
            scale: 'weighted',
            backgroundThrottle: true,
            minFrequency: 20,
            maxFrequency: null,
            streamReady: true,
        });
    });

    it('resolves ok: false with the host code for rejected parameters', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            code: 'INVALID_PARAMS',
            error: 'fftSize must be a power of 2 between 256 and 65536',
            details: { param: 'fftSize', value: 1000 },
        });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const outcome = await audio.subscribeSpectrum(() => undefined, { fftSize: 1000 }).ready;
        expect(outcome).toEqual({
            ok: false,
            code: 'INVALID_PARAMS',
            error: 'fftSize must be a power of 2 between 256 and 65536',
        });
    });

    it('resolves ok: false with UNKNOWN_ERROR when the host answers without a code', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: false, error: 'stream failed' });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const outcome = await audio.subscribeSpectrum(() => undefined).ready;
        expect(outcome).toEqual({ ok: false, code: 'UNKNOWN_ERROR', error: 'stream failed' });
    });

    it('resolves ok: false with UNKNOWN_ERROR when invoke rejects, and never rejects', async () => {
        const native = makeNative();
        native.invoke.mockRejectedValue(new Error('transport down'));
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const outcome = await audio.subscribeSpectrum(() => undefined).ready;
        expect(outcome).toEqual({ ok: false, code: 'UNKNOWN_ERROR', error: 'transport down' });
    });

    it('resolves ok: false with NOT_SUPPORTED without a host', async () => {
        vi.useFakeTimers();
        vi.stubGlobal('window', {});
        const { audio } = await import('./audio.js');

        const ready = audio.subscribeSpectrum(() => undefined).ready;
        await vi.advanceTimersByTimeAsync(200);
        await expect(ready).resolves.toMatchObject({ ok: false, code: 'NOT_SUPPORTED' });
    });

    it("passes on this subscription's frames and untagged frames only", async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const cb = vi.fn();
        audio.subscribeSpectrum(cb, { event: 'audio:spectrum' });
        const own = native.invoke.mock.calls[0][1].subscriptionId as string;
        const handler = native.on.mock.calls[0][1] as (data: unknown) => void;

        handler({ subscriptionId: 'someone-else', spectrum: [1, 2, 3] });
        handler({ subscriptionId: own, spectrum: [4, 5] });
        handler({ spectrum: [6] });

        expect(cb).toHaveBeenCalledTimes(2);
        expect(cb.mock.calls[0][0]).toEqual({ subscriptionId: own, spectrum: [4, 5] });
        expect(cb.mock.calls[1][0]).toEqual({ spectrum: [6] });
    });

    it('unsubscribes with the same subscriptionId it registered', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const stop = audio.subscribeSpectrum(() => undefined);
        stop();
        const [, subscribeParams] = native.invoke.mock.calls[0];
        const [method, unsubscribeParams] = native.invoke.mock.calls[1];
        expect(method).toBe('audio.unsubscribeSpectrum');
        expect(unsubscribeParams).toEqual({ subscriptionId: subscribeParams.subscriptionId });
        expect(native.off).toHaveBeenCalledTimes(1);
    });
});

describe('audio.generateFullWaveform (signal, timeout and host cancellation)', () => {
    const PENDING = { success: true, status: 'pending', taskId: 'waveform_7', cached: false };

    function hostWith(answer: unknown): MockNative {
        const native = makeNative();
        native.invoke.mockImplementation((method: string) =>
            Promise.resolve(
                method === 'audio.cancelFullWaveform' ? { success: true, cancelled: true } : answer,
            ),
        );
        return native;
    }

    function handlerFor(native: MockNative, event: string): (data: unknown) => void {
        const call = native.on.mock.calls.find((c) => c[0] === event);
        expect(call, `no listener registered for ${event}`).toBeTruthy();
        return call![1] as (data: unknown) => void;
    }

    const methods = (native: MockNative): string[] => native.invoke.mock.calls.map((c) => c[0]);

    /** A host whose generateFullWaveform answer is held until the test releases it. */
    function heldHost(): { native: MockNative; release: (answer: unknown) => void } {
        const native = makeNative();
        let release: (answer: unknown) => void = () => undefined;
        const held = new Promise<unknown>((resolve) => {
            release = resolve;
        });
        native.invoke.mockImplementation((method: string) =>
            method === 'audio.cancelFullWaveform'
                ? Promise.resolve({ success: true, cancelled: true })
                : held,
        );
        return { native, release: (answer) => release(answer) };
    }

    async function expectAbortError(promise: Promise<unknown>): Promise<void> {
        const error = await promise.then(
            () => {
                throw new Error('expected an AbortError rejection');
            },
            (e: unknown) => e,
        );
        expect(error).toBeInstanceOf(DOMException);
        expect((error as DOMException).name).toBe('AbortError');
    }

    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
        vi.useRealTimers();
    });

    it('rejects with AbortError without calling the host when the signal is already aborted', async () => {
        const native = hostWith(PENDING);
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const controller = new AbortController();
        controller.abort();
        await expectAbortError(audio.generateFullWaveform('E:\\a.flac', { signal: controller.signal }));
        expect(native.invoke).not.toHaveBeenCalled();
    });

    it('cancels the host task and rejects with AbortError when the signal aborts while waiting', async () => {
        const native = hostWith(PENDING);
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const controller = new AbortController();
        const pending = audio.generateFullWaveform('E:\\a.flac', { signal: controller.signal });
        await vi.waitFor(() => expect(native.on).toHaveBeenCalledTimes(2));
        controller.abort();

        await expectAbortError(pending);
        const cancel = native.invoke.mock.calls.find((c) => c[0] === 'audio.cancelFullWaveform');
        expect(cancel?.[1]).toEqual({ taskId: 'waveform_7' });
        // The CANCELLED event the host sends for that cancel finds no listener.
        expect(native.off).toHaveBeenCalledTimes(2);
    });

    it('cancels the host task when the client-side timeout fires', async () => {
        vi.useFakeTimers();
        const native = hostWith(PENDING);
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const pending = audio.generateFullWaveform('E:\\a.flac', { timeout: 500 });
        const settled = expect(pending).rejects.toMatchObject({
            success: false,
            error: 'TIMEOUT',
            taskId: 'waveform_7',
        });
        await vi.advanceTimersByTimeAsync(600);
        await settled;
        const cancel = native.invoke.mock.calls.find((c) => c[0] === 'audio.cancelFullWaveform');
        expect(cancel?.[1]).toEqual({ taskId: 'waveform_7' });
    });

    it('cancels the host task when the signal aborts while the host is still answering', async () => {
        const { native, release } = heldHost();
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const controller = new AbortController();
        const pending = audio.generateFullWaveform('E:\\a.flac', { signal: controller.signal });
        controller.abort();
        release(PENDING);

        await expectAbortError(pending);
        const cancel = native.invoke.mock.calls.find((c) => c[0] === 'audio.cancelFullWaveform');
        expect(cancel?.[1]).toEqual({ taskId: 'waveform_7' });
        expect(native.on).not.toHaveBeenCalled();
    });

    it('resolves a cache hit even when the signal aborts while the host is answering', async () => {
        const hit = { success: true, status: 'ready', cached: true, waveform: [0.5, 1], maxAmplitude: 0.3 };
        const { native, release } = heldHost();
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const controller = new AbortController();
        const pending = audio.generateFullWaveform('E:\\a.flac', { signal: controller.signal });
        controller.abort();
        release(hit);

        await expect(pending).resolves.toEqual(hit);
        expect(methods(native)).toEqual(['audio.generateFullWaveform']);
    });

    it('sends neither timeout nor signal to the host', async () => {
        const native = hostWith({ success: true, status: 'ready', waveform: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        await audio.generateFullWaveform('E:\\a.flac', {
            resolution: 512,
            method: 'peak',
            timeout: 1000,
            signal: new AbortController().signal,
        });
        expect(native.invoke.mock.calls[0][1]).toEqual({ path: 'E:\\a.flac', resolution: 512, method: 'peak' });
    });

    it('keeps the path argument when an untyped caller also passes opts.path', async () => {
        const native = hostWith({ success: true, status: 'ready', waveform: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const opts = JSON.parse('{"path":"E:\\\\other.flac"}') as Record<string, never>;
        await audio.generateFullWaveform('E:\\a.flac', opts);
        expect(native.invoke.mock.calls[0][1]).toEqual({ path: 'E:\\a.flac' });
    });

    it('resolves on its own ready event, rejects on its own failure, and does not cancel either way', async () => {
        const native = hostWith(PENDING);
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const ok = audio.generateFullWaveform('E:\\a.flac', { timeout: 0 });
        await vi.waitFor(() => expect(native.on).toHaveBeenCalledTimes(2));
        const ready = handlerFor(native, 'audio:fullWaveformReady');
        ready({ taskId: 'someone-else', waveform: [9] });
        ready({ taskId: 'waveform_7', waveform: [1], maxAmplitude: 0.2 });
        await expect(ok).resolves.toMatchObject({ success: true, status: 'ready', waveform: [1], maxAmplitude: 0.2 });

        native.on.mockClear();
        const bad = audio.generateFullWaveform('E:\\a.flac', { timeout: 0 });
        await vi.waitFor(() => expect(native.on).toHaveBeenCalledTimes(2));
        const failure = { taskId: 'waveform_7', path: 'E:\\a.flac', error: 'Failed to open decoder', code: 'DECODER_FAILED' };
        handlerFor(native, 'audio:fullWaveformFailed')(failure);
        await expect(bad).rejects.toEqual(failure);
        expect(methods(native)).not.toContain('audio.cancelFullWaveform');
    });

    it('cancelFullWaveform forwards the taskId', async () => {
        const native = hostWith(PENDING);
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        await expect(audio.cancelFullWaveform('waveform_7')).resolves.toEqual({ success: true, cancelled: true });
        expect(native.invoke.mock.calls[0]).toEqual(['audio.cancelFullWaveform', { taskId: 'waveform_7' }]);
    });
});

describe('audio.getWaveform (channels and points)', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('forwards channels and points unchanged and returns both channels', async () => {
        const native = makeNative();
        const answer = {
            success: true,
            left: [0.1, -0.2],
            right: [0, 0],
            duration: 0.05,
            signed: true,
            channels: 'stereo',
            sampleRate: 48000,
            channelCount: 2,
        };
        native.invoke.mockResolvedValue(answer);
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        const opts = { duration: 0.05, signed: true, channels: 'stereo', points: 256 } as const;
        await expect(audio.getWaveform(opts)).resolves.toEqual(answer);
        expect(native.invoke.mock.calls[0]).toEqual(['audio.getWaveform', opts]);
    });

    it('keeps the deprecated (path, opts) form ignoring the path', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, waveform: [0.5] });
        vi.stubGlobal('window', { fb2k: native });
        const { audio } = await import('./audio.js');

        await audio.getWaveform('E:\\ignored.flac', { channels: 'mix' });
        expect(native.invoke.mock.calls[0]).toEqual(['audio.getWaveform', { channels: 'mix' }]);
    });
});
