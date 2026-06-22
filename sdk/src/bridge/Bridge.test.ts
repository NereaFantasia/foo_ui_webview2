// sdk/src/bridge/Bridge.test.ts
//
// Phase 2 test gate (plan §8.7.1) — minimum coverage of Bridge core
// semantics that audit BLOCKER 1 / 2 / CRITICAL 3 depend on:
//
//   - mock fallback when window.fb2k is missing
//   - happy-path delegation to the native invoke()
//   - ready() returns Promise<void> (NOT a method reference — BLOCKER 1)
//   - on() returns an unsubscribe callback that calls native.off()
//   - once() auto-detaches its wrapper after first delivery
//   - invoke() propagates host rejections without swallowing them
//
// Every test isolates module state via `vi.resetModules()` so the
// `bridge` singleton in Bridge.ts is freshly constructed against the
// stubbed `window`.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

interface MockNativeFb2k {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    once?: ReturnType<typeof vi.fn>;
    _handleResponse: () => void;
}

function makeNative(overrides: Partial<MockNativeFb2k> = {}): MockNativeFb2k {
    return {
        invoke: vi.fn().mockResolvedValue(undefined),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy, only existence is checked by Bridge.getNativeFb2k */
        },
        ...overrides,
    };
}

describe('Bridge', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('invoke resolves to a mock sentinel when window.fb2k is absent', async () => {
        vi.stubGlobal('window', {});
        const { bridge } = await import('./Bridge.js');
        const result = await bridge.invoke('test.method');
        expect(result).toEqual({ mock: true, method: 'test.method' });
    });

    it('invoke delegates to native fb2k.invoke when present', async () => {
        const native = makeNative({
            invoke: vi.fn().mockResolvedValue({ ok: true, value: 42 }),
        });
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('./Bridge.js');

        const r = await bridge.invoke('foo.bar', { x: 1 });

        expect(r).toEqual({ ok: true, value: 42 });
        expect(native.invoke).toHaveBeenCalledTimes(1);
        expect(native.invoke).toHaveBeenCalledWith('foo.bar', { x: 1 });
    });

    it('invoke propagates rejection from native fb2k', async () => {
        const native = makeNative({
            invoke: vi.fn().mockRejectedValue(new Error('host failure')),
        });
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('./Bridge.js');

        await expect(bridge.invoke('failing.method')).rejects.toThrow(
            'host failure',
        );
    });

    it('ready() returns a Promise<void> (BLOCKER 1 regression guard)', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('./Bridge.js');

        const ready = bridge.ready();
        expect(ready).toBeInstanceOf(Promise);
        // BLOCKER 1: must resolve to undefined (void), not return the
        // method reference itself.
        await expect(ready).resolves.toBeUndefined();
    });

    it('on() returns an unsubscribe that calls native.off with the same handler', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('./Bridge.js');

        const handler = vi.fn();
        const unsubscribe = bridge.on('test:event', handler);

        expect(typeof unsubscribe).toBe('function');
        expect(native.on).toHaveBeenCalledWith('test:event', handler);

        unsubscribe();
        expect(native.off).toHaveBeenCalledWith('test:event', handler);
    });

    it('once() auto-detaches the wrapper after first delivery', async () => {
        let registered: ((payload: unknown) => void) | null = null;
        const native = makeNative({
            on: vi.fn((_event: string, h: (payload: unknown) => void) => {
                registered = h;
            }),
        });
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('./Bridge.js');

        const handler = vi.fn();
        bridge.once('once:event', handler);

        expect(registered).not.toBeNull();
        expect(native.on).toHaveBeenCalledTimes(1);

        registered!({ payload: 1 });

        expect(handler).toHaveBeenCalledTimes(1);
        expect(handler).toHaveBeenCalledWith({ payload: 1 });
        // Wrapper must auto-detach after its first invocation.
        expect(native.off).toHaveBeenCalledTimes(1);
    });

    it('isAvailable mirrors whether a native fb2k is wired up at construction', async () => {
        // 1) without fb2k
        vi.stubGlobal('window', {});
        const { bridge: bridgeMock } = await import('./Bridge.js');
        expect(bridgeMock.isAvailable).toBe(false);

        // 2) with fb2k
        vi.resetModules();
        vi.stubGlobal('window', { fb2k: makeNative() });
        const { bridge: bridgeReal } = await import('./Bridge.js');
        expect(bridgeReal.isAvailable).toBe(true);
    });
});

describe('Bridge.setMetricsHook', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('fires the hook with method/duration/result on a successful invoke', async () => {
        const native = makeNative({
            invoke: vi.fn().mockResolvedValue({ ok: true }),
        });
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('./Bridge.js');

        const hook = vi.fn();
        bridge.setMetricsHook(hook);

        const result = await bridge.invoke('foo.bar', { x: 1 });

        expect(result).toEqual({ ok: true });
        expect(hook).toHaveBeenCalledTimes(1);
        const metrics = hook.mock.calls[0][0];
        expect(metrics.method).toBe('foo.bar');
        expect(metrics.success).toBe(true);
        expect(metrics.result).toEqual({ ok: true });
        expect(metrics.error).toBeUndefined();
        expect(typeof metrics.durationMs).toBe('number');
        expect(metrics.durationMs).toBeGreaterThanOrEqual(0);
    });

    it('fires the hook with success=false and rethrows when invoke rejects', async () => {
        const failure = new Error('host failure');
        const native = makeNative({
            invoke: vi.fn().mockRejectedValue(failure),
        });
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('./Bridge.js');

        const hook = vi.fn();
        bridge.setMetricsHook(hook);

        await expect(bridge.invoke('failing.method')).rejects.toThrow(
            'host failure',
        );

        expect(hook).toHaveBeenCalledTimes(1);
        const metrics = hook.mock.calls[0][0];
        expect(metrics.method).toBe('failing.method');
        expect(metrics.success).toBe(false);
        expect(metrics.error).toBe(failure);
        expect(metrics.result).toBeUndefined();
        expect(typeof metrics.durationMs).toBe('number');
    });

    it('detaches the hook when setMetricsHook(undefined) is called', async () => {
        const native = makeNative({
            invoke: vi.fn().mockResolvedValue({ ok: true }),
        });
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('./Bridge.js');

        const hook = vi.fn();
        bridge.setMetricsHook(hook);
        await bridge.invoke('first.call');
        expect(hook).toHaveBeenCalledTimes(1);

        bridge.setMetricsHook(undefined);
        await bridge.invoke('second.call');
        // Still 1 — second call must not have re-fired the hook.
        expect(hook).toHaveBeenCalledTimes(1);
    });

    it('surfaces hook exceptions via console.warn but still resolves invoke', async () => {
        const native = makeNative({
            invoke: vi.fn().mockResolvedValue({ ok: true }),
        });
        vi.stubGlobal('window', { fb2k: native });
        const consoleSpy = vi
            .spyOn(console, 'warn')
            .mockImplementation(() => {
                /* silence test output */
            });
        const { bridge } = await import('./Bridge.js');

        const exploded = new Error('hook exploded');
        bridge.setMetricsHook(() => {
            throw exploded;
        });

        // Invoke must resolve normally — the hook's throw is captured.
        const result = await bridge.invoke('safe.method');
        expect(result).toEqual({ ok: true });

        // The error must surface to console.warn so observability
        // failures stay visible to the caller.
        expect(consoleSpy).toHaveBeenCalledTimes(1);
        const warnArgs = consoleSpy.mock.calls[0];
        expect(String(warnArgs[0])).toContain('safe.method');
        expect(warnArgs[1]).toBe(exploded);

        consoleSpy.mockRestore();
    });

    it('fires the hook on the mock-fallback path when no host is present', async () => {
        vi.stubGlobal('window', {});
        const { bridge } = await import('./Bridge.js');

        const hook = vi.fn();
        bridge.setMetricsHook(hook);

        const result = await bridge.invoke('mock.method');

        expect(result).toEqual({ mock: true, method: 'mock.method' });
        expect(hook).toHaveBeenCalledTimes(1);
        const metrics = hook.mock.calls[0][0];
        expect(metrics.method).toBe('mock.method');
        expect(metrics.success).toBe(true);
        expect(metrics.result).toEqual({ mock: true, method: 'mock.method' });
    });
});
