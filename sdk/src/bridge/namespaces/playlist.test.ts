// sdk/src/bridge/namespaces/playlist.test.ts
//
// Phase 5 §5.3 regression guards — covers the playlist-envelope drift
// discovered by `scripts/audit_sdk_cpp_payloads.mjs`.
//
//   §5.3 finding  Before the fix, `bridge.invoke<PlaylistTrack[]>(
//                 'playlist.getTracks', …)` pretended the response was
//                 a bare array, but the C++ handler always returns
//                 `{ playlist, start, count, total, tracks: [...] }`.
//                 Callers expecting `Array.isArray(result)` (including
//                 the integration tests in `web/src/tests/`) saw the
//                 envelope object and broke silently.
//
// This test gate locks in the envelope-unwrap contract:
//
//   1. `getTracks(...)` sends `{ playlist, start, count }` (and the
//      optional `formats` key) to the host.
//   2. When the host resolves with the full envelope, the SDK wrapper
//      resolves with the inner `tracks` array.
//   3. A malformed / null host response yields `[]` so callers can
//      always iterate safely.
//   4. `getSelectedTracks(...)` follows the same unwrap policy.

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

describe('playlist namespace — §5.3 envelope unwrap', () => {
    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    // ── getTracks ─────────────────────────────────────────────────

    it('getTracks sends `{ playlist, start, count }` to the host', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            playlist: 0,
            start: 10,
            count: 0,
            total: 0,
            tracks: [],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await playlist.getTracks(0, 10, 50);

        expect(native.invoke).toHaveBeenCalledTimes(1);
        expect(native.invoke).toHaveBeenCalledWith('playlist.getTracks', {
            playlist: 0,
            start: 10,
            count: 50,
        });
    });

    it('getTracks forwards the `formats` key only when non-empty', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            playlist: 0,
            start: 0,
            count: 0,
            total: 0,
            tracks: [],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await playlist.getTracks(0, 0, 100, { rating: '%rating%' });
        expect(native.invoke.mock.calls[0][1]).toMatchObject({
            formats: { rating: '%rating%' },
        });

        native.invoke.mockClear();
        await playlist.getTracks(0, 0, 100, {});
        expect(native.invoke.mock.calls[0][1]).not.toHaveProperty('formats');
    });

    it('getTracks unwraps `tracks` from the envelope (§5.3 BLOCKER)', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            playlist: 0,
            start: 0,
            count: 2,
            total: 2,
            tracks: [
                {
                    index: 0,
                    title: 'A',
                    artist: 'X',
                    album: 'Y',
                    duration: 120,
                    path: '/a.flac',
                    isPlaying: false,
                    isSelected: true,
                },
                {
                    index: 1,
                    title: 'B',
                    artist: 'X',
                    album: 'Y',
                    duration: 180,
                    path: '/b.flac',
                    isPlaying: false,
                    isSelected: false,
                },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const tracks = await playlist.getTracks(0, 0, 100);

        // Regression guard: legacy typing claimed `PlaylistTrack[]` so
        // the old codepath returned the envelope object and every
        // `Array.isArray(result)` check silently failed. The wrapper
        // now unwraps so callers get a real array back.
        expect(Array.isArray(tracks)).toBe(true);
        expect(tracks).toHaveLength(2);
        expect(tracks[0]).toMatchObject({
            index: 0,
            title: 'A',
            path: '/a.flac',
        });
    });

    it('getTracks returns `[]` when the host omits tracks', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            error: 'Invalid playlist index',
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const tracks = await playlist.getTracks(42, 0, 10);
        expect(Array.isArray(tracks)).toBe(true);
        expect(tracks).toHaveLength(0);
    });

    it('getTracks returns `[]` when the host resolves with null', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue(null);
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const tracks = await playlist.getTracks(0, 0, 10);
        expect(tracks).toEqual([]);
    });

    // ── getSelectedTracks ────────────────────────────────────────

    it('getSelectedTracks sends `{ playlist }` and unwraps tracks', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            playlist: 1,
            count: 1,
            tracks: [
                {
                    index: 3,
                    title: 'Selected',
                    artist: 'X',
                    album: 'Y',
                    duration: 90,
                    path: '/sel.flac',
                    isPlaying: false,
                    isSelected: true,
                },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const tracks = await playlist.getSelectedTracks(1);

        expect(native.invoke).toHaveBeenCalledWith(
            'playlist.getSelectedTracks',
            { playlist: 1 },
        );
        expect(Array.isArray(tracks)).toBe(true);
        expect(tracks[0].path).toBe('/sel.flac');
    });

    it('getSelectedTracks returns `[]` on host error envelope', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            error: 'Invalid playlist index',
            tracks: undefined,
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const tracks = await playlist.getSelectedTracks(-1);
        expect(tracks).toEqual([]);
    });
});
