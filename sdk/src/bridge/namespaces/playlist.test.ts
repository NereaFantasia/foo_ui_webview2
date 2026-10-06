// sdk/src/bridge/namespaces/playlist.test.ts
//
// The playlist wrappers resolve with the host's envelope as it arrives:
//
//   1. `getTracks(...)` sends `{ playlist, start, count }` (and the
//      optional `formats` / `fields` keys) to the host.
//   2. A page resolves as `{ playlist, start, count, total, tracks }`.
//   3. A failure resolves as the failure envelope. It is neither thrown
//      nor turned into an empty array, so a failed call and an empty
//      playlist or selection stay distinguishable.
//   4. `getSelectedTracks`, `getAll` and `getAvailableColumns` follow
//      the same policy; `getTracksPage` is the same call as `getTracks`.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { expectFailure, expectSuccess } from './__tests__/expectEnvelope.js';

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

describe('playlist namespace — envelopes pass through', () => {
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

    it('getTracks resolves with the page envelope', async () => {
        const native = makeNative();
        const envelope = {
            success: true,
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
        };
        native.invoke.mockResolvedValue(envelope);
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const page = await playlist.getTracks(0, 0, 100);

        expect(page).toEqual(envelope);
        expectSuccess(page);
        expect(page.tracks).toHaveLength(2);
        expect(page.tracks[0]).toMatchObject({ index: 0, title: 'A', path: '/a.flac' });
    });

    it('getTracks resolves with the failure envelope instead of an empty array', async () => {
        const native = makeNative();
        const failure = { success: false, error: 'Invalid playlist index', code: 'INVALID_INDEX' };
        native.invoke.mockResolvedValue(failure);
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const page = await playlist.getTracks(42, 0, 10);
        expect(page).toEqual(failure);
        expectFailure(page);
        expect(page.code).toBe('INVALID_INDEX');
    });

    // ── getSelectedTracks ────────────────────────────────────────

    it('getSelectedTracks sends `{ playlist }` and resolves with the envelope', async () => {
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

        const selected = await playlist.getSelectedTracks(1);

        expect(native.invoke).toHaveBeenCalledWith(
            'playlist.getSelectedTracks',
            { playlist: 1 },
        );
        expectSuccess(selected);
        expect(selected.tracks[0]?.path).toBe('/sel.flac');
    });

    it('getSelectedTracks tells a failed call apart from an empty selection', async () => {
        const native = makeNative();
        // The host's failure for a missing playlist still carries an empty tracks.
        native.invoke.mockResolvedValueOnce({
            success: false,
            error: 'Invalid playlist index',
            code: 'INVALID_INDEX',
            tracks: [],
        });
        native.invoke.mockResolvedValueOnce({ success: true, playlist: 0, count: 0, tracks: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const failed = await playlist.getSelectedTracks(99);
        expectFailure(failed);
        expect(failed.code).toBe('INVALID_INDEX');

        const empty = await playlist.getSelectedTracks(0);
        expectSuccess(empty);
        expect(empty.tracks).toEqual([]);
    });

    it('getAll and getAvailableColumns resolve with the envelope and never throw', async () => {
        const native = makeNative();
        const lists = { success: true, playlists: [{ index: 0, name: 'Default' }], count: 1 };
        const failure = { success: false, error: 'boom', code: 'OPERATION_FAILED' };
        native.invoke.mockResolvedValueOnce(lists).mockResolvedValueOnce(failure);
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await expect(playlist.getAll()).resolves.toEqual(lists);
        await expect(playlist.getAvailableColumns()).resolves.toEqual(failure);
        expect(native.invoke.mock.calls.map((c) => c[0])).toEqual([
            'playlist.getAll',
            'playlist.getAvailableColumns',
        ]);
    });

    // -- getTracksPage: deprecated alias of getTracks ---------------------

    it('getTracksPage resolves with the same page as getTracks', async () => {
        const native = makeNative();
        const envelope = {
            success: true,
            playlist: 0,
            start: 200,
            count: 2,
            total: 102400,
            tracks: [{ index: 200 }, { index: 201 }],
        };
        native.invoke.mockResolvedValue(envelope);
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const page = await playlist.getTracksPage(0, 200, 2);
        expectSuccess(page);

        expect(page).toEqual(envelope);
        // `total` is the whole point: the grouped list compares it against
        // getGroupRuns's total to notice a playlist that changed underneath.
        expect(page.total).toBe(102400);
    });

    it('getTracksPage forwards fields and formats as getTracks does', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ playlist: 0, start: 0, count: 0, total: 0, tracks: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await playlist.getTracksPage(0, 0, 100, {}, ['title', 'album']);
        expect(native.invoke).toHaveBeenCalledWith('playlist.getTracks', {
            playlist: 0,
            start: 0,
            count: 100,
            fields: ['title', 'album'],
        });
    });

    // -- getGroupRuns -----------------------------------------------------

    it('getGroupRuns sends patterns and omits playlist when not given', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, playlist: 0, total: 0, runs: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await playlist.getGroupRuns(['%album artist% | %album%']);

        expect(native.invoke).toHaveBeenCalledWith('playlist.getGroupRuns', {
            patterns: ['%album artist% | %album%'],
        });
    });

    it('getGroupRuns passes the playlist index through when given', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, playlist: 3, total: 0, runs: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await playlist.getGroupRuns(['%album%'], 3);

        expect(native.invoke).toHaveBeenCalledWith('playlist.getGroupRuns', {
            patterns: ['%album%'],
            playlist: 3,
        });
    });

    it('getGroupRuns resolves with the run envelope, sub starts absolute', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: true,
            playlist: 0,
            total: 33,
            runs: [
                {
                    start: 0,
                    count: 24,
                    key: 'Nujabes | Modal Soul',
                    sub: [
                        { start: 0, count: 12, key: 'Disc 1' },
                        { start: 12, count: 12, key: 'Disc 2' },
                    ],
                },
                {
                    start: 24,
                    count: 9,
                    key: 'Portishead | Dummy',
                    sub: [{ start: 24, count: 9, key: 'Disc 1' }],
                },
            ],
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const res = await playlist.getGroupRuns(['%album artist% | %album%', '%discnumber%']);
        expectSuccess(res);

        expect(res.success).toBe(true);
        expect(res.runs).toHaveLength(2);
        // The discriminating case: the second run's first sub starts at the
        // parent's start, not at 0.
        expect(res.runs[1].sub?.[0].start).toBe(24);
        // Runs tile the table.
        expect(res.runs.reduce((n, r) => n + r.count, 0)).toBe(res.total);
    });

    it('getGroupRuns surfaces the host error envelope rather than swallowing it', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({
            success: false,
            code: 'INVALID_PARAMS',
            error: 'patterns must not contain an empty string',
            details: { pattern: 0 },
        });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const res = await playlist.getGroupRuns(['']);
        expectFailure(res);

        expect(res.success).toBe(false);
        expect(res.code).toBe('INVALID_PARAMS');
    });
});

describe('playlist namespace — a playlist by index or by GUID', () => {
    const GUID = '{12345678-1111-2222-3031-323334353637}';

    beforeEach(() => {
        vi.resetModules();
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('sends a string as `playlistGuid` and a number as `playlist`, never both', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await playlist.addHandles(GUID, ['a.flac']);
        await playlist.addHandles(2, ['a.flac']);
        await playlist.rename(GUID, 'Renamed');
        await playlist.getTracks(GUID, 0, 10);

        expect(native.invoke.mock.calls[0]).toEqual([
            'playlist.addHandles',
            { playlistGuid: GUID, handles: ['a.flac'] },
        ]);
        expect(native.invoke.mock.calls[1]).toEqual([
            'playlist.addHandles',
            { playlist: 2, handles: ['a.flac'] },
        ]);
        expect(native.invoke.mock.calls[2]).toEqual([
            'playlist.rename',
            { playlistGuid: GUID, name: 'Renamed' },
        ]);
        expect(native.invoke.mock.calls[3]).toEqual([
            'playlist.getTracks',
            { playlistGuid: GUID, start: 0, count: 10 },
        ]);
    });

    it('library.addToPlaylist and artwork.getByPlaylistItem take a GUID the same way', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true });
        vi.stubGlobal('window', { fb2k: native });
        const { library } = await import('./library.js');
        const { artwork } = await import('./artwork.js');

        await library.addToPlaylist(['a.flac'], GUID);
        await library.addToPlaylist(['a.flac']);
        await artwork.getByPlaylistItem(GUID, 3);

        expect(native.invoke.mock.calls[0]).toEqual([
            'library.addToPlaylist',
            { paths: ['a.flac'], playlistGuid: GUID },
        ]);
        expect(native.invoke.mock.calls[1]).toEqual(['library.addToPlaylist', { paths: ['a.flac'] }]);
        expect(native.invoke.mock.calls[2]).toEqual([
            'artwork.getByPlaylistItem',
            { playlistGuid: GUID, index: 3 },
        ]);
    });

    it('reorderPlaylists sends GUIDs as `newOrderGuids` and indices as `newOrder`', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, count: 2 });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        const OTHER = '{00000000-1111-2222-3333-444444444444}';
        await playlist.reorderPlaylists([OTHER, GUID]);
        await playlist.reorderPlaylists([1, 0]);
        await playlist.reorderPlaylists([]);

        expect(native.invoke.mock.calls[0]).toEqual([
            'playlist.reorderPlaylists',
            { newOrderGuids: [OTHER, GUID] },
        ]);
        expect(native.invoke.mock.calls[1]).toEqual(['playlist.reorderPlaylists', { newOrder: [1, 0] }]);
        expect(native.invoke.mock.calls[2]).toEqual(['playlist.reorderPlaylists', { newOrder: [] }]);
    });

    it('getMatchingRows sends the query with the playlist only when one is named', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, playlist: 0, total: 0, items: [], count: 0 });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await playlist.getMatchingRows('artist HAS nachi', GUID);
        await playlist.getMatchingRows('%rating% GREATER 3');

        expect(native.invoke.mock.calls).toEqual([
            ['playlist.getMatchingRows', { query: 'artist HAS nachi', playlistGuid: GUID }],
            ['playlist.getMatchingRows', { query: '%rating% GREATER 3' }],
        ]);
    });

    it('getTracksAt sends the rows as given and `formats` / `fields` only when set', async () => {
        const native = makeNative();
        native.invoke.mockResolvedValue({ success: true, playlist: 1, total: 9, count: 0, tracks: [] });
        vi.stubGlobal('window', { fb2k: native });
        const { playlist } = await import('./playlist.js');

        await playlist.getTracksAt(GUID, [7, 2, 7]);
        await playlist.getTracksAt(1, [0], { r: '%rating%' }, ['title']);

        expect(native.invoke.mock.calls).toEqual([
            ['playlist.getTracksAt', { playlistGuid: GUID, rows: [7, 2, 7] }],
            [
                'playlist.getTracksAt',
                { playlist: 1, rows: [0], formats: { r: '%rating%' }, fields: ['title'] },
            ],
        ]);
    });
});
