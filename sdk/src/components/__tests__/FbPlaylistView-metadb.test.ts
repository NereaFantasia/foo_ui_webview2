// sdk/src/components/__tests__/FbPlaylistView-metadb.test.ts
//
// `<fb-playlist-view>` reads again the rows whose tracks `metadb:changed`
// names, so a rating, a play count or a tag edit shows without a reload. See
// `playlistViewHarness.ts` for how the view runs without a DOM.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { FakeElement, guidOf, makeFb, mount, playingAt, settle } from './playlistViewHarness.js';

function changed(handles: string[], truncated = false) {
    return { tracks: [], count: handles.length, handles, truncated, fromHook: true, timestamp: 0 };
}

async function setUp() {
    const host = makeFb(playingAt(0, 0));
    const calls: Array<[string, number[]]> = [];
    Object.assign(host.fb.playlist, {
        getTracksAt: async (playlist: string, rows: number[]) => {
            calls.push([playlist, [...rows]]);
            return { success: true, tracks: rows.map((index) => ({ index, handle: `T${index}`, title: 'new' })) };
        },
    });
    vi.stubGlobal('window', { fb: host.fb });
    const view = await mount(1);
    view._dataCache.clear();
    for (const index of [0, 1, 2]) view._dataCache.set(index, { index, handle: `T${index}`, title: 'old' });
    return { host, calls, view };
}

describe('FbPlaylistView · metadb:changed', () => {
    beforeEach(() => {
        vi.resetModules();
        vi.stubGlobal('HTMLElement', FakeElement);
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('reads again only the kept rows of the changed tracks', async () => {
        const { host, calls, view } = await setUp();

        host.emit('metadb:changed', changed(['T2', 'T0', 'elsewhere']));
        await settle();

        expect(calls).toEqual([[guidOf(1), [0, 2]]]);
        expect(view._dataCache.get(0)).toMatchObject({ title: 'new' });
        expect(view._dataCache.get(1)).toMatchObject({ title: 'old' });
        expect(view._dataCache.get(2)).toMatchObject({ title: 'new' });
    });

    it('reads nothing when no kept row is named', async () => {
        const { host, calls } = await setUp();

        host.emit('metadb:changed', changed(['elsewhere']));
        await settle();

        expect(calls).toEqual([]);
    });

    it('drops every kept row when too many tracks changed to name them', async () => {
        const { host, calls, view } = await setUp();

        host.emit('metadb:changed', changed([], true));
        await settle();

        expect(calls).toEqual([]);
        expect(view._dataCache.size).toBe(0);
    });
});
