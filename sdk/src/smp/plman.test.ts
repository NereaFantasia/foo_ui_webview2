// sdk/src/smp/plman.test.ts
//
// AddItemToPlaybackQueue hands the handle id to queue.addPaths as it is:
// the declaration accepts the `|subsong:N` suffix, so a CUE subsong is
// queued as itself and not as the file's first subsong.
//
// The methods that take more than one host call name the playlist by its
// index only in the first; the later calls name the playlist the first one
// reported by GUID, so a playlist added or removed in between cannot make
// them act on another one.

import { describe, expect, it, vi } from 'vitest';

import type { SmpBridgeShape } from './bridgeShape.js';
import { createInitialCache } from './cache.js';
import { FbMetadbHandle } from './classes/FbMetadbHandle.js';
import { buildPlman } from './plman.js';
import type { SmpCompatCache } from './types.js';

type Responder = (method: string, params: Record<string, unknown>) => unknown;

function makeFb(
    responder: Responder = () => ({ success: true, addedCount: 1 }),
): { fb: SmpBridgeShape; invoke: ReturnType<typeof vi.fn> } {
    const invoke = vi.fn(async (method: string, params?: object) =>
        responder(method, (params ?? {}) as Record<string, unknown>),
    );
    const fb: SmpBridgeShape = {
        on: () => () => undefined,
        off: () => undefined,
        once: () => () => undefined,
        invoke: invoke as SmpBridgeShape['invoke'],
    };
    return { fb, invoke };
}

const GUID = '{00000000-0000-0000-0000-000000000002}';

function guidAt(index: number): string {
    return `{00000000-0000-0000-0000-${String(index).padStart(12, '0')}}`;
}

describe('plman.AddItemToPlaybackQueue', () => {
    it('queues a subsong with its suffix and a plain file as its path', async () => {
        const { fb, invoke } = makeFb();
        const plman = buildPlman(fb, createInitialCache(), () => undefined);

        const handle = new FbMetadbHandle('C:\\album.cue|subsong:2');
        await plman.AddItemToPlaybackQueue(handle.HandleId);
        await plman.AddItemToPlaybackQueue({ Path: 'C:\\album.cue', SubSong: 3 });
        await plman.AddItemToPlaybackQueue('C:\\a.flac');

        expect(invoke.mock.calls).toEqual([
            ['queue.addPaths', { paths: ['C:\\album.cue|subsong:2'], useQueuePlaylist: true }],
            ['queue.addPaths', { paths: ['C:\\album.cue|subsong:3'], useQueuePlaylist: true }],
            ['queue.addPaths', { paths: ['C:\\a.flac'], useQueuePlaylist: true }],
        ]);
    });
});

describe('plman · multi-step methods keep to one playlist', () => {
    it('crops the playlist the selection was read from', async () => {
        const { fb, invoke } = makeFb((method) => {
            if (method === 'playlist.getSelection') {
                return { success: true, playlist: 2, playlistGuid: GUID, items: [1], count: 1 };
            }
            if (method === 'playlist.getTrackCount') return { success: true, count: 3 };
            return { success: true };
        });
        const plman = buildPlman(fb, createInitialCache(), () => undefined);

        expect(await plman.RemovePlaylistSelection(2, true)).toBe(true);

        expect(invoke.mock.calls).toEqual([
            ['playlist.getSelection', { playlist: 2 }],
            ['playlist.getTrackCount', { playlistGuid: GUID }],
            ['playlist.removeTracks', { playlistGuid: GUID, items: [0, 2] }],
        ]);
    });

    it('crops nothing when the selection cannot be read', async () => {
        const { fb, invoke } = makeFb(() => ({ success: false, code: 'INVALID_INDEX', error: 'x' }));
        const plman = buildPlman(fb, createInitialCache(), () => undefined);

        expect(await plman.RemovePlaylistSelection(9, true)).toBe(false);
        expect(invoke.mock.calls.map(([method]) => method)).toEqual(['playlist.getSelection']);
    });

    it('deselects in the playlist the selection was read from', async () => {
        const { fb, invoke } = makeFb((method) =>
            method === 'playlist.getSelection'
                ? { success: true, playlist: 2, playlistGuid: GUID, items: [0, 1, 4], count: 3 }
                : { success: true },
        );
        const plman = buildPlman(fb, createInitialCache(), () => undefined);

        expect(await plman.SetPlaylistSelection(2, [1], false)).toBe(true);

        expect(invoke.mock.calls[1]).toEqual([
            'playlist.setSelection',
            { playlistGuid: GUID, indices: [0, 4], clearOthers: true },
        ]);
    });

    it('selects the added rows in the playlist they went into', async () => {
        const { fb, invoke } = makeFb((method) =>
            method === 'playlist.addPaths'
                ? { success: true, playlist: 2, playlistGuid: GUID, addedCount: 2, countBefore: 5 }
                : { success: true },
        );
        const plman = buildPlman(fb, createInitialCache(), () => undefined);

        expect(await plman.AddLocations(2, ['C:\\a.flac', 'C:\\b.flac'], true)).toBe(2);

        expect(invoke.mock.calls[1]).toEqual([
            'playlist.setSelection',
            { playlistGuid: GUID, indices: [5, 6], clearOthers: true },
        ]);
    });

    it('selects the inserted rows in the playlist they went into', async () => {
        const { fb, invoke } = makeFb((method) =>
            method === 'playlist.insertTracks'
                ? { success: true, playlist: 2, playlistGuid: GUID, addedCount: 1, insertIndex: 3 }
                : { success: true },
        );
        const plman = buildPlman(fb, createInitialCache(), () => undefined);

        expect(await plman.InsertPlaylistItems(2, 3, ['C:\\a.flac'], true)).toBe(1);

        expect(invoke.mock.calls[1]).toEqual([
            'playlist.setSelection',
            { playlistGuid: GUID, indices: [3], clearOthers: true },
        ]);
    });

    it('reads every page after the first from the playlist the first came from', async () => {
        const rows = Array.from({ length: 700 }, (_, i) => ({ path: `C:\\${i}.flac` }));
        const { fb, invoke } = makeFb((_method, params) => {
            const start = params.start as number;
            return {
                success: true,
                playlist: 2,
                playlistGuid: GUID,
                start,
                total: rows.length,
                tracks: rows.slice(start, start + (params.count as number)),
            };
        });
        const plman = buildPlman(fb, createInitialCache(), () => undefined);

        const list = await plman.GetPlaylistItems(2);

        expect(list.Count).toBe(700);
        expect(invoke.mock.calls.map(([, params]) => params)).toEqual([
            { playlist: 2, start: 0, count: 500 },
            { playlistGuid: GUID, start: 500, count: 500 },
        ]);
    });

    it('moves a playlist by the GUIDs of the cached playlists', async () => {
        const { fb, invoke } = makeFb(() => ({ success: true }));
        const cache: SmpCompatCache = createInitialCache();
        cache.playlists = [0, 1, 2].map((index) => ({
            index,
            guid: guidAt(index),
            name: `list ${index}`,
            trackCount: 0,
            isActive: index === 0,
            isPlaying: false,
            isLocked: false,
            isAutoplaylist: false,
        }));
        cache.playlistCount = 3;
        const plman = buildPlman(fb, cache, () => undefined);

        expect(await plman.MovePlaylist!(0, 2)).toBe(true);

        expect(invoke.mock.calls).toEqual([
            ['playlist.reorderPlaylists', { newOrderGuids: [guidAt(1), guidAt(2), guidAt(0)] }],
        ]);
    });
});
