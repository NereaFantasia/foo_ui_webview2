// sdk/src/bridge/namespaces/output.test.ts
//
// Locks the output.getDevices facade contract: the wrapper unwraps the
// `{ devices, count }` envelope, surfaces failure envelopes as thrown
// errors (the flat array return type has no error channel), and falls
// back to an empty array only for malformed success payloads.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

type Handler = (data: unknown) => void;

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

describe('output.getDevices', () => {
    beforeEach(() => vi.resetModules());
    afterEach(() => vi.unstubAllGlobals());

    it('unwraps the devices envelope into a flat array', async () => {
        const native = makeNative();
        const devices = [
            {
                guid: '{00000000-0000-0000-0000-000000000000}',
                name: 'Primary Sound Driver',
                entry: 'Default',
                entryGuid: '{D41D2423-FBB0-4635-B233-7054F79814AB}',
            },
        ];
        native.invoke.mockResolvedValue({ devices, count: 1 });
        vi.stubGlobal('window', { fb2k: native });
        const { output } = await import('./output.js');

        const result = await output.getDevices();

        expect(native.invoke).toHaveBeenCalledWith(
            'output.getDevices',
            undefined,
        );
        expect(result).toEqual(devices);
    });

    it('throws when the host reports a failure envelope', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            error: 'enumeration failed',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { output } = await import('./output.js');

        await expect(output.getDevices()).rejects.toThrow(
            'enumeration failed',
        );
    });

    it('returns an empty array for a malformed success payload', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ count: 0 });
        vi.stubGlobal('window', { fb2k: native });
        const { output } = await import('./output.js');

        await expect(output.getDevices()).resolves.toEqual([]);
    });
});
