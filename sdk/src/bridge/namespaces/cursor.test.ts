// sdk/src/bridge/namespaces/cursor.test.ts
//
// Locks the `cursor` namespace contract: `setHidden(boolean)` invokes
// the dot-form `cursor.setHidden` handler with a `{ hidden }` payload
// and `isHidden()` invokes `cursor.isHidden` with no parameters. Also
// asserts the typed `bridge.on('cursor:hiddenChanged')` event flows
// the expected `{ hidden: boolean }` payload.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
}

function makeNative(): MockNative {
    return {
        invoke: vi.fn(),
        on: vi.fn(),
        off: vi.fn(),
    };
}

describe('cursor.setHidden', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('forwards `true` under the `hidden` key', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, changed: true });
        vi.stubGlobal('window', { fb2k: native });
        const { cursor } = await import('./cursor.js');

        const result = await cursor.setHidden(true);

        expect(native.invoke).toHaveBeenCalledWith(
            'cursor.setHidden',
            { hidden: true },
        );
        expect(result).toEqual({ success: true, changed: true });
    });

    it('forwards `false` under the `hidden` key', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, changed: true });
        vi.stubGlobal('window', { fb2k: native });
        const { cursor } = await import('./cursor.js');

        await cursor.setHidden(false);

        expect(native.invoke).toHaveBeenCalledWith(
            'cursor.setHidden',
            { hidden: false },
        );
    });

    it('passes through `changed: false` for redundant flips', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, changed: false });
        vi.stubGlobal('window', { fb2k: native });
        const { cursor } = await import('./cursor.js');

        const result = await cursor.setHidden(true);

        expect(result).toEqual({ success: true, changed: false });
    });

    it('passes through validation failure envelopes verbatim', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            error: 'caller window not found',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { cursor } = await import('./cursor.js');

        const result = await cursor.setHidden(true);

        expect(result).toEqual({
            success: false,
            error: 'caller window not found',
        });
    });
});

describe('cursor.isHidden', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('invokes `cursor.isHidden` with no parameters', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ hidden: true });
        vi.stubGlobal('window', { fb2k: native });
        const { cursor } = await import('./cursor.js');

        const result = await cursor.isHidden();

        expect(native.invoke).toHaveBeenCalledWith(
            'cursor.isHidden',
            undefined,
        );
        expect(result).toEqual({ hidden: true });
    });
});

describe('cursor:hiddenChanged event', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('flows `{ hidden: boolean }` through the typed bridge.on overload', async () => {
        let capturedHandler: ((payload: unknown) => void) | undefined;
        const native: MockNative = {
            invoke: vi.fn(),
            on: vi.fn((_event: string, handler: (data: unknown) => void) => {
                capturedHandler = handler;
            }),
            off: vi.fn(),
        };
        vi.stubGlobal('window', { fb2k: native });
        const { bridge } = await import('../Bridge.js');

        const seen: Array<{ hidden: boolean }> = [];
        const off = bridge.on('cursor:hiddenChanged', (payload) => {
            seen.push(payload);
        });

        expect(native.on).toHaveBeenCalledWith(
            'cursor:hiddenChanged',
            expect.any(Function),
        );

        capturedHandler?.({ hidden: true });
        capturedHandler?.({ hidden: false });
        expect(seen).toEqual([{ hidden: true }, { hidden: false }]);

        off();
        expect(native.off).toHaveBeenCalledWith(
            'cursor:hiddenChanged',
            expect.any(Function),
        );
    });
});
