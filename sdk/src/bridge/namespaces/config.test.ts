// sdk/src/bridge/namespaces/config.test.ts
//
// Regression guard — `config.getAll` resolves the envelope
// `{ success, items, configs, count }` returned by the C++ handler, where
// `items` and `configs` reference the same map, not a bare
// `Record<string, unknown>`. Lock that contract so the wrapper surface
// stays accurate. The five listing endpoints are the opposite case: the
// host wraps each list in an envelope and the wrapper resolves with the
// list itself.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { expectSuccess } from './__tests__/expectEnvelope.js';

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

describe('config namespace — getAll envelope', () => {
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
        expectSuccess(result);

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

describe('config list endpoints resolve with their envelopes', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    const ZERO_GUID = '{00000000-0000-0000-0000-000000000000}';
    const cases = [
        {
            method: 'getOutputDevices',
            api: 'config.getOutputDevices',
            key: 'devices',
            list: [
                {
                    name: 'Primary Sound Driver',
                    id: ZERO_GUID,
                    outputId: '{D41D2423-FBB0-4635-B233-7054F79814AB}',
                    deviceId: ZERO_GUID,
                    isCurrent: true,
                },
            ],
        },
        {
            method: 'getAdvancedConfig',
            api: 'config.getAdvancedConfig',
            key: 'entries',
            list: [
                {
                    name: 'Display',
                    guid: '{A1B2C3D9-E5F6-7890-1234-56789ABCDEF5}',
                    sortPriority: 0,
                    type: 'branch',
                    children: [],
                },
            ],
        },
        {
            method: 'getPreferencesPages',
            api: 'config.getPreferencesPages',
            key: 'pages',
            list: [
                {
                    name: 'Display',
                    guid: '{A1B2C3D9-E5F6-7890-1234-56789ABCDEF6}',
                    parentGuid: ZERO_GUID,
                    sortPriority: 0,
                },
            ],
        },
        {
            method: 'getComponents',
            api: 'config.getComponents',
            key: 'components',
            list: [
                {
                    name: 'WebView2 UI',
                    version: '1.14.0',
                    filename: 'foo_ui_webview2.dll',
                    fileName: 'foo_ui_webview2.dll',
                },
            ],
        },
        {
            method: 'getDspPresets',
            api: 'config.getDspPresets',
            key: 'presets',
            list: [{ index: 0, name: 'Default' }],
        },
    ] as const;

    it.each(cases)('$method resolves with the envelope that holds the list under `$key`', async ({ method, api, key, list }) => {
        const native = makeNative();
        const envelope = { success: true, [key]: list, count: list.length };
        native.invoke.mockResolvedValue(envelope);
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        const result = await config[method]();

        expect(native.invoke).toHaveBeenCalledWith(api, undefined);
        expect(result).toEqual(envelope);
    });

    it.each(cases)('$method resolves with the failure envelope instead of throwing', async ({ method }) => {
        const native = makeNative();
        const failure = {
            success: false,
            error: 'enumeration failed',
            code: 'OPERATION_FAILED',
        };
        native.invoke.mockResolvedValue(failure);
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await expect(config[method]()).resolves.toEqual(failure);
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

    it('passes the failure envelope through rather than throwing', async () => {
        const native = makeNative();
        const refusal = {
            success: false,
            error: 'mode or sourceMode is required',
            code: 'INVALID_PARAMS',
        };
        native.invoke.mockResolvedValue(refusal);
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await expect(config.setReplaygainMode(2)).resolves.toEqual(refusal);
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
        native.invoke.mockResolvedValue({
            success: false,
            error: 'milliseconds is out of range',
            code: 'INVALID_PARAMS',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        const result = await config.setOutputBuffer(0);

        expect(native.invoke).toHaveBeenCalledWith(
            'config.setOutputBuffer',
            { milliseconds: 0 },
        );
        expect(result).toEqual({
            success: false,
            error: 'milliseconds is out of range',
            code: 'INVALID_PARAMS',
        });
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

describe('config.get default', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('sends only the key when no default is given', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, key: 'k', found: false, value: null });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.get('k');

        expect(native.invoke).toHaveBeenCalledWith('config.get', { key: 'k' });
    });

    it('sends the default as `default`, including falsy JSON values', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, key: 'k', found: false, value: 0 });
        vi.stubGlobal('window', { fb2k: native });
        const { config } = await import('./config.js');

        await config.get('k', { theme: 'dark' });
        await config.get('k', 0);
        await config.get('k', false);
        await config.get('k', null);

        expect(native.invoke.mock.calls.map((c) => c[1])).toEqual([
            { key: 'k', default: { theme: 'dark' } },
            { key: 'k', default: 0 },
            { key: 'k', default: false },
            { key: 'k', default: null },
        ]);
    });
});
