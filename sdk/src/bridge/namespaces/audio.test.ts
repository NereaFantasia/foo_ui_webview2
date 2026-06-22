// sdk/src/bridge/namespaces/audio.test.ts
//
// Regression guards for `audio` namespace methods that are sensitive to
// host-side stub state. Currently covers Bug #6
// (`AUDIT_REPORT_2026-05-09 §3 MINOR`):
//
//   `audio.subscribeStream` bridges into a C++ handler that returns
//   `{success: false, error: "Stream capture requires
//   playback_stream_capture integration"}` and never emits
//   `audio:stream`. The SDK now (a) carries an `@deprecated` JSDoc and
//   (b) surfaces a `console.warn` when the host response confirms the
//   stub state, so callers stop registering listeners that will never
//   fire.

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

describe('audio.subscribeStream (Bug #6 host stub guard)', () => {
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
