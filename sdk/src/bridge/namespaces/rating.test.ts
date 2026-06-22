import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    _handleResponse: () => void;
}

function makeNative(): MockNative {
    return {
        invoke: vi.fn().mockResolvedValue({ success: true }),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy */
        },
    };
}

describe('rating.set', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('omits `cueIndex` when no opts are provided', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { rating } = await import('./rating.js');

        await rating.set('/a.flac', 4);

        expect(native.invoke).toHaveBeenCalledWith('rating.set', {
            path: '/a.flac',
            rating: 4,
        });
        const params = native.invoke.mock.calls[0][1] as Record<string, unknown>;
        expect(params).not.toHaveProperty('cueIndex');
    });

    it('forwards a non-negative `cueIndex` when supplied', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { rating } = await import('./rating.js');

        await rating.set('/cue.flac', 5, { cueIndex: 2 });

        expect(native.invoke).toHaveBeenCalledWith('rating.set', {
            path: '/cue.flac',
            rating: 5,
            cueIndex: 2,
        });
    });

    it('forwards `cueIndex: 0` (first CUE entry, distinct from missing)', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { rating } = await import('./rating.js');

        await rating.set('/cue.flac', 3, { cueIndex: 0 });

        expect(native.invoke).toHaveBeenCalledWith('rating.set', {
            path: '/cue.flac',
            rating: 3,
            cueIndex: 0,
        });
    });
});
