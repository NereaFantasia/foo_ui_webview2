import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

interface MockNative {
    invoke: ReturnType<typeof vi.fn>;
    on: ReturnType<typeof vi.fn>;
    off: ReturnType<typeof vi.fn>;
    _handleResponse: () => void;
}

function makeNative(): MockNative {
    return {
        invoke: vi.fn().mockResolvedValue({ success: true, tracksAdded: 0 }),
        on: vi.fn(),
        off: vi.fn(),
        _handleResponse: () => {
            /* dummy */
        },
    };
}

describe('player.playPaths', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('omits `startIndex` and `replace` when no second argument is supplied', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        await player.playPaths(['/a.flac']);

        expect(native.invoke).toHaveBeenCalledWith('playback.playPaths', {
            paths: ['/a.flac'],
        });
        const params = native.invoke.mock.calls[0][1] as Record<string, unknown>;
        expect(params).not.toHaveProperty('startIndex');
        expect(params).not.toHaveProperty('replace');
    });

    it('treats a numeric second argument as `startIndex` (compatibility path)', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        await player.playPaths(['/a.flac', '/b.flac'], 1);

        expect(native.invoke).toHaveBeenCalledWith('playback.playPaths', {
            paths: ['/a.flac', '/b.flac'],
            startIndex: 1,
        });
    });

    it('treats numeric `0` as a real startIndex, not as missing', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        await player.playPaths(['/a.flac'], 0);

        expect(native.invoke).toHaveBeenCalledWith('playback.playPaths', {
            paths: ['/a.flac'],
            startIndex: 0,
        });
    });

    it('forwards `replace: true` when supplied via the options object', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        await player.playPaths(['/a.flac'], { replace: true });

        expect(native.invoke).toHaveBeenCalledWith('playback.playPaths', {
            paths: ['/a.flac'],
            replace: true,
        });
    });

    it('forwards both `startIndex` and `replace` from the options object', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        await player.playPaths(['/a.flac', '/b.flac'], {
            startIndex: 1,
            replace: true,
        });

        expect(native.invoke).toHaveBeenCalledWith('playback.playPaths', {
            paths: ['/a.flac', '/b.flac'],
            startIndex: 1,
            replace: true,
        });
    });

    it('omits `replace: false` when not specified (defaults remain implicit)', async () => {
        const native = makeNative();
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        await player.playPaths(['/a.flac'], { startIndex: 0 });

        const params = native.invoke.mock.calls[0][1] as Record<string, unknown>;
        expect(params).not.toHaveProperty('replace');
        expect(params.startIndex).toBe(0);
    });
});

describe('player.getCurrentTrack', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('passes the no-track envelope through unchanged, keyed by `found: false`', async () => {
        const native = makeNative();
        const envelope = { success: true, found: false, playing: false };
        native.invoke.mockResolvedValue(envelope);
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        const result = await player.getCurrentTrack();

        expect(native.invoke).toHaveBeenCalledWith('playback.getCurrentTrack', undefined);
        // No normalisation to `null`: callers see exactly what the host sent.
        expect(result).not.toBeNull();
        expect(result).toEqual(envelope);
        expect('found' in result).toBe(true);
        if ('found' in result) {
            expect(result.found).toBe(false);
            expect(result.playing).toBe(false);
        }
    });

    it('passes a loaded track through unchanged, with no `found` key to narrow on', async () => {
        const native = makeNative();
        const track = {
            id: 'C:\\music\\a.flac',
            title: 'A',
            artist: 'B',
            album: 'C',
            duration: 240.5,
            path: 'C:\\music\\a.flac',
        };
        native.invoke.mockResolvedValue(track);
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        const result = await player.getCurrentTrack();

        expect(result).toEqual(track);
        expect('found' in result).toBe(false);
        if (!('found' in result)) {
            expect(result.duration).toBe(240.5);
            expect(result.title).toBe('A');
        }
    });
});
