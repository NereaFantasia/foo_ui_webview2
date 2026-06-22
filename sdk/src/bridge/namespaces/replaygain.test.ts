import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    _handleResponse: () => void;
}

function makeNative(): MockNative {
    return {
        invoke: vi.fn().mockResolvedValue({ success: true, scannedCount: 0 }),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy */
        },
    };
}

describe('replaygain.scan', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('omits `mode` when no opts are provided (track scan default)', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { replaygain } = await import('./replaygain.js');

        await replaygain.scan(['/a.flac']);

        expect(native.invoke).toHaveBeenCalledWith('replaygain.scan', {
            paths: ['/a.flac'],
        });
        const params = native.invoke.mock.calls[0][1] as Record<string, unknown>;
        expect(params).not.toHaveProperty('mode');
    });

    it('forwards `mode: track` verbatim when explicitly requested', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { replaygain } = await import('./replaygain.js');

        await replaygain.scan(['/a.flac'], { mode: 'track' });

        expect(native.invoke).toHaveBeenCalledWith('replaygain.scan', {
            paths: ['/a.flac'],
            mode: 'track',
        });
    });

    it('forwards `mode: album` for whole-album scans', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { replaygain } = await import('./replaygain.js');

        await replaygain.scan(['/a.flac', '/b.flac'], { mode: 'album' });

        expect(native.invoke).toHaveBeenCalledWith('replaygain.scan', {
            paths: ['/a.flac', '/b.flac'],
            mode: 'album',
        });
    });
});
