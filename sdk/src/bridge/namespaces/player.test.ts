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

    it('passes the no-track answer through unchanged, keyed by `found: false`', async () => {
        const native = makeNative();
        const envelope = { success: true, found: false };
        native.invoke.mockResolvedValue(envelope);
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        const result = await player.getCurrentTrack();
        expectSuccess(result);

        expect(native.invoke).toHaveBeenCalledWith('playback.getCurrentTrack', undefined);
        // No normalisation to `null`: callers see exactly what the host sent.
        expect(result).not.toBeNull();
        expect(result).toEqual(envelope);
        expect(result.found).toBe(false);
        expect(result.track).toBeUndefined();
    });

    it('passes a loaded track through unchanged under `track`', async () => {
        const native = makeNative();
        const track = {
            handle: 'C:\\music\\a.flac',
            title: 'A',
            artist: 'B',
            album: 'C',
            duration: 240.5,
            path: 'C:\\music\\a.flac',
        };
        native.invoke.mockResolvedValue({ success: true, found: true, track });
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        const result = await player.getCurrentTrack();
        expectSuccess(result);

        expect(result.found).toBe(true);
        expect(result.track).toEqual(track);
        expect(result.track?.duration).toBe(240.5);
        expect(result.track?.title).toBe('A');
    });
});

describe('player.seek and player.setOrder', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('sends the seek target under `position`', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        await player.seek(42.5);

        expect(native.invoke).toHaveBeenCalledWith('playback.setPosition', { position: 42.5 });
    });

    it('sends a numeric order as `order` and a name as `name`', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { player } = await import('./player.js');

        await player.setOrder(3);
        await player.setOrder('shuffle-albums');

        expect(native.invoke).toHaveBeenNthCalledWith(1, 'playback.setPlaybackOrder', { order: 3 });
        expect(native.invoke).toHaveBeenNthCalledWith(2, 'playback.setPlaybackOrder', {
            name: 'shuffle-albums',
        });
    });
});
