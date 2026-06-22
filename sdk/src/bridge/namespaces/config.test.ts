// sdk/src/bridge/namespaces/config.test.ts
//
// Phase 5 §5.4 regression guard — `config.getAll` previously claimed
// to return `Record<string, unknown>`, but the C++ handler returns the
// envelope `{ success, items, configs, count }` where `items` and
// `configs` reference the same map. Lock the new contract so the
// wrapper surface stays accurate.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    _handleResponse: () => void;
}

function makeNative(): MockNative {
    return {
        invoke: vi.fn(),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy */
        },
    };
}

describe('config namespace — §5.4 getAll envelope', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('getAll resolves to the `{ success, items, configs, count }` envelope verbatim', async () => {
        const native = makeNative();
        const cache = {
            'theme.background': '#000',
            'window.zoom': 1.25,
        };
        native.invoke.mockResolvedValue({
            success: true,
            items: cache,
            configs: cache,
            count: 2,
        });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        const result = await config.getAll();

        // bridge.invoke threads no params for getAll, so the second arg
        // is `undefined`; spelt out so the assertion matches the spy
        // call shape exactly.
        expect(native.invoke).toHaveBeenCalledWith('config.getAll', undefined);
        expect(result?.success).toBe(true);
        expect(result?.count).toBe(2);
        // `items` and `configs` are aliases of the same map — they must
        // both round-trip and stay structurally equal.
        expect(result?.items).toEqual(cache);
        expect(result?.configs).toEqual(cache);
    });
});

describe('config.setReplaygainMode dual signature', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('forwards a numeric mode under the `mode` key', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, mode: 1, value: 1 });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setReplaygainMode(1);

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setReplaygainMode',
            { mode: 1 },
        );
    });

    it('forwards a numeric mode of `0` (none) explicitly', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, mode: 0, value: 0 });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setReplaygainMode(0);

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setReplaygainMode',
            { mode: 0 },
        );
    });

    it('forwards a named alias under the `sourceMode` key', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, mode: 1, value: 1 });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setReplaygainMode('track');

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setReplaygainMode',
            { sourceMode: 'track' },
        );
    });

    it('forwards `auto` as a sourceMode alias of byPlaybackOrder', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, mode: 3, value: 3 });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setReplaygainMode('auto');

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setReplaygainMode',
            { sourceMode: 'auto' },
        );
    });
});

describe('config.setOutputBuffer dual signature', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('treats a numeric argument as milliseconds (compatibility path)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setOutputBuffer(500);

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setOutputBuffer',
            { milliseconds: 500 },
        );
    });

    it('forwards `milliseconds` from the options object', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setOutputBuffer({ milliseconds: 250 });

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setOutputBuffer',
            { milliseconds: 250 },
        );
    });

    it('forwards `bufferLength` (seconds) from the options object', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setOutputBuffer({ bufferLength: 0.5 });

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setOutputBuffer',
            { bufferLength: 0.5 },
        );
    });

    it('forwards both fields when both are supplied', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setOutputBuffer({ milliseconds: 500, bufferLength: 0.5 });

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setOutputBuffer',
            { milliseconds: 500, bufferLength: 0.5 },
        );
    });

    it('treats `0` ms as a real value (host validates the range)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.setOutputBuffer(0);

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setOutputBuffer',
            { milliseconds: 0 },
        );
    });
});

describe('REPLAYGAIN_SOURCE_MODE constant dictionary', () => {
    it('mirrors the host enum values 0-3', async () => {
        const { REPLAYGAIN_SOURCE_MODE } = await import('../../types/responses.js');
        expect(REPLAYGAIN_SOURCE_MODE.none).toBe(0);
        expect(REPLAYGAIN_SOURCE_MODE.track).toBe(1);
        expect(REPLAYGAIN_SOURCE_MODE.album).toBe(2);
        expect(REPLAYGAIN_SOURCE_MODE.byPlaybackOrder).toBe(3);
    });
});
