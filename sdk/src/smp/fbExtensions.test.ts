// sdk/src/smp/fbExtensions.test.ts
//
// `fb.GetQueryItems` / `fb.GetLibraryItems` read contracts, and how
// `fb.RunContextCommandWithMetadb` reaches the tracks it names.
//
// GetQueryItems issues at most two `library.search` calls regardless of
// hit count: a one-row probe for the total, then a single projected
// fetch. GetLibraryItems keeps paging `library.getAll` in 500-row
// chunks because that pipeline has no projection and a whole-library
// single shot would materialise it all at once.

import { describe, expect, it, vi } from 'vitest';

import { createInitialCache } from './cache.js';
import { attachFbExtensions } from './fbExtensions.js';
import type { SmpBridgeShape } from './bridgeShape.js';
import { FbMetadbHandle } from './classes/FbMetadbHandle.js';
import { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
import type { SMPPlman } from './types.js';

interface InvokeCall {
    method: string;
    params: Record<string, unknown>;
}

interface FbExtSurface {
    ClearPlaylist: () => Promise<boolean>;
    GetFocusItem: () => Promise<FbMetadbHandle | null>;
    GetQueryItems: (
        handlesLike: unknown,
        query: string,
    ) => Promise<FbMetadbHandleList>;
    GetLibraryItems: () => Promise<FbMetadbHandleList>;
    RunContextCommandWithMetadb: (
        command: string,
        handleOrList: unknown,
        flags?: number,
    ) => Promise<boolean>;
}

/** Build a track row shaped like the projected `library.search` output. */
function row(path: string, subsong = 0, duration = 1, fileSize = 2): object {
    return { absolutePath: path, path, subsong, duration, fileSize };
}

/**
 * Attach the extensions onto a bridge whose `invoke` is recorded, and
 * return both the extension surface and the call log.
 */
function setup(
    responder: (call: InvokeCall) => unknown,
): { ext: FbExtSurface; calls: InvokeCall[] } {
    const calls: InvokeCall[] = [];
    const fb = {
        invoke: vi.fn(async (method: string, params?: object) => {
            const call = {
                method,
                params: (params ?? {}) as Record<string, unknown>,
            };
            calls.push(call);
            return responder(call);
        }),
        on: vi.fn(() => vi.fn()),
        off: vi.fn(),
        once: vi.fn(),
    } as unknown as SmpBridgeShape;

    attachFbExtensions(
        fb,
        {} as unknown as SMPPlman,
        createInitialCache(),
        () => {},
    );

    return { ext: fb as unknown as FbExtSurface, calls };
}

/** Call log filtered to one method. */
function callsTo(calls: InvokeCall[], method: string): InvokeCall[] {
    return calls.filter((c) => c.method === method);
}

// The cache may lag behind the host's active playlist, so these name no
// playlist on the first call and the host reads the active one as it arrives.
describe('fb.ClearPlaylist / fb.GetFocusItem and the active playlist', () => {
    const GUID = '{00000000-0000-0000-0000-000000000004}';

    it('clears without naming a playlist', async () => {
        const { ext, calls } = setup(() => ({ success: true, clearedCount: 3 }));

        expect(await ext.ClearPlaylist()).toBe(true);
        expect(callsTo(calls, 'playlist.clear').map((c) => c.params)).toEqual([{}]);
    });

    it('reads the focused row from the playlist the focus came from', async () => {
        const { ext, calls } = setup((call) =>
            call.method === 'playlist.getFocusedTrack'
                ? { success: true, playlist: 4, playlistGuid: GUID, index: 2 }
                : { success: true, tracks: [row('C:\\focused.flac')] },
        );

        const handle = await ext.GetFocusItem();

        expect(handle?.Path).toBe('C:\\focused.flac');
        expect(callsTo(calls, 'playlist.getFocusedTrack')[0]!.params).toEqual({});
        expect(callsTo(calls, 'playlist.getTracks')[0]!.params).toEqual({
            playlistGuid: GUID,
            start: 2,
            count: 1,
        });
    });

    it('answers null without a second read when there is no active playlist', async () => {
        const { ext, calls } = setup(() => ({ success: true, playlist: -1, index: -1 }));

        expect(await ext.GetFocusItem()).toBe(null);
        expect(callsTo(calls, 'playlist.getTracks')).toEqual([]);
    });
});

describe('fb.GetQueryItems', () => {
    it('makes no bridge call for an empty query', async () => {
        const { ext, calls } = setup(() => ({}));

        const list = await ext.GetQueryItems(null, '');

        expect(list.Count).toBe(0);
        expect(calls).toHaveLength(0);
    });

    it('probes with limit=1 then fetches every hit in one call', async () => {
        const { ext, calls } = setup((call) => {
            if (call.params.limit === 1) return { total: 3 };
            return {
                tracks: [row('C:\\a.flac'), row('C:\\b.flac'), row('C:\\c.flac')],
                total: 3,
            };
        });

        const list = await ext.GetQueryItems(null, '%genre% IS Jazz');
        const searches = callsTo(calls, 'library.search');

        expect(searches).toHaveLength(2);
        expect(searches[0].params).toEqual({
            query: '%genre% IS Jazz',
            offset: 0,
            limit: 1,
        });
        expect(searches[1].params).toEqual({
            query: '%genre% IS Jazz',
            offset: 0,
            limit: 3,
            fields: ['absolutePath', 'path', 'subsong', 'duration', 'fileSize'],
        });
        expect(list.Count).toBe(3);
    });

    it('stays at two calls when the hit count is large', async () => {
        const hits = 8000;
        const { ext, calls } = setup((call) => {
            if (call.params.limit === 1) return { total: hits };
            const tracks = Array.from({ length: hits }, (_v, i) =>
                row(`C:\\track-${i}.flac`),
            );
            return { tracks, total: hits };
        });

        const list = await ext.GetQueryItems(null, 'lossless');

        expect(callsTo(calls, 'library.search')).toHaveLength(2);
        expect(list.Count).toBe(hits);
    });

    it('stops after the probe when nothing matches', async () => {
        const { ext, calls } = setup(() => ({
            tracks: [],
            total: 0,
        }));

        const list = await ext.GetQueryItems(null, 'no such thing');

        expect(list.Count).toBe(0);
        expect(callsTo(calls, 'library.search')).toHaveLength(1);
    });

    it('stops after the probe when the response carries no total', async () => {
        const { ext, calls } = setup(() => ({ success: false, error: 'boom' }));

        const list = await ext.GetQueryItems(null, 'bad ( query');

        expect(list.Count).toBe(0);
        expect(callsTo(calls, 'library.search')).toHaveLength(1);
    });

    it('prefers tracks over items on pre-1.13 hosts that still send both', async () => {
        const { ext } = setup((call) => {
            if (call.params.limit === 1) return { total: 2 };
            return {
                tracks: [row('C:\\x.flac'), row('C:\\y.flac')],
                items: [row('C:\\wrong.flac'), row('C:\\wrong2.flac')],
                total: 2,
            };
        });

        const list = await ext.GetQueryItems(null, 'anything');

        expect(list.Count).toBe(2);
        expect(list[0].Path).toBe('C:\\x.flac');
        expect(list[1].Path).toBe('C:\\y.flac');
    });

    it('keeps hit order and every handle property the projection backs', async () => {
        const { ext } = setup((call) => {
            if (call.params.limit === 1) return { total: 2 };
            const tracks = [
                row('C:\\first.flac', 0, 120.5, 4096),
                row('C:\\second.flac', 3, 61.25, 2048),
            ];
            return { tracks, total: 2 };
        });

        const list = await ext.GetQueryItems(null, 'ordered');

        expect(list.Count).toBe(2);
        expect(list[0].Path).toBe('C:\\first.flac');
        expect(list[0].SubSong).toBe(0);
        expect(list[0].Length).toBe(120.5);
        expect(list[0].FileSize).toBe(4096);
        expect(list[1].Path).toBe('C:\\second.flac');
        expect(list[1].SubSong).toBe(3);
        expect(list.CalcTotalDuration()).toBeCloseTo(181.75, 5);
        expect(list.CalcTotalSize()).toBe(6144);
    });
});

describe('fb.GetLibraryItems', () => {
    it('keeps paging library.getAll in 500-row chunks', async () => {
        const total = 1200;
        const { ext, calls } = setup((call) => {
            const offset = Number(call.params.offset ?? 0);
            const limit = Number(call.params.limit ?? 0);
            const count = Math.max(0, Math.min(limit, total - offset));
            const tracks = Array.from({ length: count }, (_v, i) =>
                row(`C:\\lib-${offset + i}.flac`),
            );
            return { tracks, total };
        });

        const list = await ext.GetLibraryItems();
        const pages = callsTo(calls, 'library.getAll');

        expect(pages).toHaveLength(3);
        expect(pages.map((p) => p.params.limit)).toEqual([500, 500, 500]);
        expect(pages.map((p) => p.params.offset)).toEqual([0, 500, 1000]);
        expect(list.Count).toBe(total);
        expect(pages.every((p) => !('fields' in p.params))).toBe(true);
    });
});

// `menu.runContextCommand` acts on the playing track or the selection and
// declares no `handles`, so the host refuses the key and dropping it would run
// the command on other tracks. The command is looked up in a menu built for
// the given tracks and run by the id that menu reports, with the same `mode`
// and `handles`.
describe('fb.RunContextCommandWithMetadb', () => {
    /** A context menu with `Properties` on top and `Rating/5` one level down. */
    const MENU = {
        success: true,
        mode: 'handles',
        items: [
            { type: 'command', label: 'Properties', path: 'Properties', commandId: 3 },
            { type: 'separator' },
            {
                type: 'submenu',
                label: 'Playback Statistics',
                path: 'Playback Statistics',
                children: [
                    {
                        type: 'submenu',
                        label: 'Rating',
                        path: 'Playback Statistics/Rating',
                        children: [
                            {
                                type: 'command',
                                label: '5',
                                path: 'Playback Statistics/Rating/5',
                                commandId: 17,
                            },
                        ],
                    },
                ],
            },
        ],
    };

    function menuHost(
        overrides: { menu?: unknown; run?: unknown } = {},
    ): ReturnType<typeof setup> {
        return setup((call) => {
            if (call.method === 'menu.getContextMenu') return overrides.menu ?? MENU;
            if (call.method === 'menu.runContextCommandById') {
                return overrides.run ?? { success: true };
            }
            return { success: false, error: 'unexpected', code: 'INVALID_PARAMS' };
        });
    }

    it('runs the command found by path in a menu built for the handle', async () => {
        const { ext, calls } = menuHost();
        const handle = new FbMetadbHandle('C:\\album.flac|subsong:2');

        const ok = await ext.RunContextCommandWithMetadb(
            'playback statistics/rating/5',
            handle,
        );

        expect(ok).toBe(true);
        expect(calls.map((c) => c.method)).toEqual([
            'menu.getContextMenu',
            'menu.runContextCommandById',
        ]);
        expect(calls[0].params).toEqual({
            mode: 'handles',
            handles: ['C:\\album.flac|subsong:2'],
        });
        expect(calls[1].params).toEqual({
            id: 17,
            mode: 'handles',
            handles: ['C:\\album.flac|subsong:2'],
        });
    });

    it('takes every handle of a handle list, and a plain path', async () => {
        const list = new FbMetadbHandleList([
            new FbMetadbHandle('C:\\a.flac'),
            new FbMetadbHandle('C:\\b.cue|subsong:3'),
        ]);
        const first = menuHost();
        expect(await first.ext.RunContextCommandWithMetadb('Properties', list)).toBe(true);
        expect(first.calls[1].params).toEqual({
            id: 3,
            mode: 'handles',
            handles: ['C:\\a.flac', 'C:\\b.cue|subsong:3'],
        });

        const second = menuHost();
        expect(await second.ext.RunContextCommandWithMetadb('Properties', 'C:\\c.mp3')).toBe(true);
        expect(second.calls[0].params).toEqual({ mode: 'handles', handles: ['C:\\c.mp3'] });
    });

    it('never calls menu.runContextCommand, which would run on other tracks', async () => {
        const { ext, calls } = menuHost();

        await ext.RunContextCommandWithMetadb('Properties', 'C:\\a.flac');

        expect(callsTo(calls, 'menu.runContextCommand')).toHaveLength(0);
    });

    it('runs nothing and reports false when the menu has no such command', async () => {
        const { ext, calls } = menuHost();

        const ok = await ext.RunContextCommandWithMetadb('Rating/5', 'C:\\a.flac');

        expect(ok).toBe(false);
        expect(callsTo(calls, 'menu.runContextCommandById')).toHaveLength(0);
    });

    it('runs nothing when the host cannot build the menu for the handles', async () => {
        const { ext, calls } = menuHost({
            menu: { success: false, error: 'no usable handles', code: 'INVALID_PARAMS' },
        });

        const ok = await ext.RunContextCommandWithMetadb('Properties', 'C:\\gone.flac');

        expect(ok).toBe(false);
        expect(callsTo(calls, 'menu.runContextCommandById')).toHaveLength(0);
    });

    it('reports false when running the command fails', async () => {
        const { ext } = menuHost({
            run: { success: false, error: 'No context menu item has this id', code: 'NOT_FOUND' },
        });

        expect(await ext.RunContextCommandWithMetadb('Properties', 'C:\\a.flac')).toBe(false);
    });

    it('makes no call without a command or without a handle', async () => {
        const { ext, calls } = menuHost();

        expect(await ext.RunContextCommandWithMetadb('', 'C:\\a.flac')).toBe(false);
        expect(await ext.RunContextCommandWithMetadb('Properties', null)).toBe(false);
        expect(await ext.RunContextCommandWithMetadb('Properties', new FbMetadbHandleList())).toBe(false);
        expect(calls).toHaveLength(0);
    });
});
