// sdk/src/bridge/namespaces/playcount.test.ts
//
// Phase 5 §5.2 regression guards — covers the two M4 audit findings:
//
//   §5.2.1  `playcount.get(path)` MUST send `{ paths: [path] }` (not
//           `{ path }`) and unwrap the first per-track result, because
//           the C++ handler `PlaycountApi.cpp::PlaycountGet` rejects any
//           payload that doesn't carry a `paths` JSON array.
//
//   §5.2.2  `playcount.set` is a placeholder on the C++ side and the
//           wrapper MUST not throw on a `{ success: false }` host
//           response, so consumers see the @deprecated reality without
//           a runtime exception.
//
// Module state is reset between tests so the `bridge` singleton in
// Bridge.ts is freshly constructed against the stubbed `window`.

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

describe('playcount namespace', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('exposes exactly the four registered methods', async () => {
        vi.stubGlobal('window', { fb2k: makeNative() });
        const { playcount } = await import('./playcount.js');

        expect(Object.keys(playcount).sort()).toEqual(
            ['get', 'getBatch', 'getStats', 'set'].sort(),
        );
    });

    it('get(path) sends `paths: [path]` to the host (§5.2.1 BLOCKER)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            count: 1,
            results: [
                {
                    path: '/music/track.flac',
                    success: true,
                    playCount: 7,
                    inLibrary: true,
                },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playcount } = await import('./playcount.js');

        await playcount.get('/music/track.flac');

        expect(native.invoke).toHaveBeenCalledTimes(1);
        expect(native.invoke).toHaveBeenCalledWith('playcount.get', {
            paths: ['/music/track.flac'],
        });
        // Regression guard: never send the legacy `{ path }` shape that
        // the C++ handler rejects with `paths array is required`.
        const call = native.invoke.mock.calls[0];
        expect(call[1]).not.toHaveProperty('path');
    });

    it('get(path) unwraps results[0] from the batch envelope', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            count: 1,
            results: [
                {
                    path: '/music/track.flac',
                    success: true,
                    playCount: 7,
                    firstPlayed: '2024-05-01 12:00:00',
                    rating: 4,
                    inLibrary: true,
                },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playcount } = await import('./playcount.js');

        const r = await playcount.get('/music/track.flac');

        expect(r).not.toBeNull();
        expect(r?.path).toBe('/music/track.flac');
        expect(r?.playCount).toBe(7);
        expect(r?.rating).toBe(4);
        expect(r?.inLibrary).toBe(true);
    });

    it('get(path) returns null when the envelope reports success: false', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            error: 'paths array is required',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playcount } = await import('./playcount.js');

        const r = await playcount.get('/music/track.flac');

        expect(r).toBeNull();
    });

    it('get(path) returns null when results array is empty', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            count: 0,
            results: [],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playcount } = await import('./playcount.js');

        const r = await playcount.get('/missing/track.flac');

        expect(r).toBeNull();
    });

    it('getBatch(paths) sends the array as-is and returns the full envelope', async () => {
        const native = makeNative();
        const envelope = {
            success: true,
            count: 2,
            results: [
                { path: '/a', success: true, playCount: 1 },
                { path: '/b', success: false, error: 'Failed to open file' },
            ],
        };
        native.invoke.mockResolvedValue(envelope);
        vi.stubGlobal('window', { fb2k: native });
        const { playcount } = await import('./playcount.js');

        const r = await playcount.getBatch(['/a', '/b']);

        expect(native.invoke).toHaveBeenCalledWith('playcount.getBatch', {
            paths: ['/a', '/b'],
        });
        expect(r).toEqual(envelope);
    });

    it('set(path, count) does NOT throw on host placeholder rejection (§5.2.2)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            error: 'Direct playcount modification not supported. Use rating.set for ratings.',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playcount } = await import('./playcount.js');

        // The wrapper passes the count through (so the cpp_api_param
        // schema stays satisfied) but consumers must still get a value
        // back, not an exception.
        const r = await playcount.set('/music/track.flac', 100);

        expect(native.invoke).toHaveBeenCalledWith('playcount.set', {
            path: '/music/track.flac',
            count: 100,
        });
        expect(r).toBeDefined();
        expect((r as { success: boolean }).success).toBe(false);
    });

    it('getStats() forwards to playcount.getStats with no params', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            totalTracks: 42,
            playedTracks: 10,
            unplayedTracks: 32,
            ratedTracks: 5,
            totalPlayCount: 100,
            maxPlayCount: 50,
            averagePlayCount: 10,
            averageRating: 3.4,
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playcount } = await import('./playcount.js');

        const r = await playcount.getStats();

        // bridge.invoke forwards params=undefined when not supplied.
        expect(native.invoke).toHaveBeenCalledTimes(1);
        expect(native.invoke.mock.calls[0][0]).toBe('playcount.getStats');
        expect(r.totalTracks).toBe(42);
        expect(r.averageRating).toBe(3.4);
    });
});
