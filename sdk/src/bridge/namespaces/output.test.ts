// sdk/src/bridge/namespaces/output.test.ts
//
// Locks the output.getDevices contract: the wrapper resolves with the
// `{ devices, count }` envelope as the host sent it, and with the failure
// envelope when the call fails; it never throws for a reported failure.

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

    it('resolves with the devices envelope', async () => {
        const native = makeNative();
        const envelope = {
            success: true,
            devices: [
                {
                    guid: '{00000000-0000-0000-0000-000000000000}',
                    name: 'Primary Sound Driver',
                    entry: 'Default',
                    entryGuid: '{D41D2423-FBB0-4635-B233-7054F79814AB}',
                },
            ],
            count: 1,
        };
        native.invoke.mockResolvedValue(envelope);
        vi.stubGlobal('window', { fb2k: native });
        const { output } = await import('./output.js');

        const result = await output.getDevices();

        expect(native.invoke).toHaveBeenCalledWith(
            'output.getDevices',
            undefined,
        );
        expect(result).toEqual(envelope);
    });

    it('resolves with the failure envelope instead of throwing', async () => {
        const native = makeNative();
        const failure = {
            success: false,
            error: 'enumeration failed',
            code: 'OPERATION_FAILED',
        };
        native.invoke.mockResolvedValue(failure);
        vi.stubGlobal('window', { fb2k: native });
        const { output } = await import('./output.js');

        await expect(output.getDevices()).resolves.toEqual(failure);
    });
});
