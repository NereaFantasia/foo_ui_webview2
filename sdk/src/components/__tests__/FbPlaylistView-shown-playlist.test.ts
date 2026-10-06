// sdk/src/components/__tests__/FbPlaylistView-shown-playlist.test.ts
//
// State `<fb-playlist-view>` keeps by row number belongs to the playlist it
// shows. The view works out that playlist's GUID from the `playlist`
// attribute (an index, a GUID, or nothing for the active playlist) and names
// it by that GUID in every host call. When the playlist shown changes,
// through the attribute, a switch of the followed active playlist, or an
// index that names another playlist after playlists moved, the view drops
// the cached rows, selection and focus of the old one; otherwise Delete would
// remove the rows with the same numbers from the new playlist. Changes of
// other playlists leave the view alone. See `playlistViewHarness.ts` for how
// the view runs without a DOM.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import {
    FakeElement,
    guidOf,
    makeFb,
    mount,
    playingAt,
    settle,
    type Location,
    type ViewInternals,
} from './playlistViewHarness.js';

type Host = ReturnType<typeof makeFb>;

const NOT_PLAYING: Location = {
    success: true,
    found: false,
    playlist: null,
    playlistGuid: null,
    index: null,
};

async function mountOn(host: Host, playlist: number | string | null): Promise<ViewInternals> {
    vi.stubGlobal('window', { fb: host.fb });
    return mount(playlist);
}

/** Give the view a selection, focus and cached rows, as a user's clicks would. */
function dirty(view: ViewInternals): void {
    view._selection = new Set([0, 1, 2]);
    view._focusedIndex = 2;
    view._shiftAnchor = 0;
    view._dataCache.set(0, { title: 'cached' });
}

/** The playlist named by the latest `playlist.getCount` read. */
function lastCountRead(host: Host): number | string | undefined {
    const calls = host.fb.playlist.getCount.calls;
    return calls[calls.length - 1]?.[0];
}

function expectClean(view: ViewInternals): void {
    expect([...view._selection]).toEqual([]);
    expect(view._focusedIndex).toBe(-1);
    expect(view._shiftAnchor).toBe(-1);
    expect(view._dataCache.size).toBe(0);
    expect(view.getAttribute('selected-count')).toBe('0');
}

function expectKept(view: ViewInternals): void {
    expect([...view._selection]).toEqual([0, 1, 2]);
    expect(view._focusedIndex).toBe(2);
}

describe('FbPlaylistView · the playlist shown', () => {
    beforeEach(() => {
        vi.resetModules();
        vi.stubGlobal('HTMLElement', FakeElement);
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('names the playlist at the index by its GUID', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, 1);

        expect(view._shown).toBe(guidOf(1));
        expect(lastCountRead(host)).toBe(guidOf(1));
    });

    it('loads the playlist a changed `playlist` attribute names', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, 1);

        view.setAttribute('playlist', '2');
        view.attributeChangedCallback('playlist');
        await settle();

        expect(view._source).toBe(2);
        expect(lastCountRead(host)).toBe(guidOf(2));
    });

    it('takes a GUID in the attribute, in either hex case', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, 1);

        view.setAttribute('playlist', guidOf(6).toLowerCase());
        view.attributeChangedCallback('playlist');
        await settle();

        expect(view._source).toBe(guidOf(6).toLowerCase());
        expect(view._shown).toBe(guidOf(6));
        expect(lastCountRead(host)).toBe(guidOf(6));
    });

    it('follows the active playlist once the attribute is removed', async () => {
        const host = makeFb(NOT_PLAYING, 4);
        const view = await mountOn(host, 1);

        view.removeAttribute('playlist');
        view.attributeChangedCallback('playlist');
        await settle();

        expect(view._source).toBe(null);
        expect(lastCountRead(host)).toBe(guidOf(4));
    });

    it('shows no rows, and reads none, while there is no active playlist to follow', async () => {
        const host = makeFb(NOT_PLAYING, -1);
        const view = await mountOn(host, null);

        expect(view._shown).toBe(null);
        expect(host.fb.playlist.getCount.calls).toEqual([]);
        expect(view.getAttribute('track-count')).toBe('0');
    });

    it('drops the rows, selection and focus of the old playlist on an attribute change', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, 1);
        dirty(view);

        view.setAttribute('playlist', '2');
        view.attributeChangedCallback('playlist');

        expectClean(view);
    });

    it('drops them when the followed active playlist switches', async () => {
        const host = makeFb(NOT_PLAYING, 0);
        const view = await mountOn(host, null);
        dirty(view);

        host.activate(guidOf(5));
        host.emit('playlist:activated', { oldIndex: 0, newIndex: 5, newGuid: guidOf(5) });
        await settle();

        expectClean(view);
        expect(lastCountRead(host)).toBe(guidOf(5));
    });

    it('keeps them on an activation when it shows a fixed playlist', async () => {
        const host = makeFb(NOT_PLAYING, 0);
        const view = await mountOn(host, 1);
        dirty(view);

        host.emit('playlist:activated', { oldIndex: 0, newIndex: 5, newGuid: guidOf(5) });
        await settle();

        expectKept(view);
    });

    it('shows the playlist now at its index once one is added in front, and drops the old state', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, 1);
        dirty(view);

        const added = '{00000000-0000-0000-0000-0000000000AA}';
        host.setPlaylists([added, ...Array.from({ length: 8 }, (_, i) => guidOf(i))]);
        host.emit('playlist:created', { index: 0, guid: added, name: 'new' });
        await settle();

        expect(view._shown).toBe(guidOf(0));
        expect(lastCountRead(host)).toBe(guidOf(0));
        expectClean(view);
    });

    it('keeps its state when playlists move but the index still names the same one', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, 1);
        dirty(view);
        const reads = host.fb.playlist.getCount.calls.length;

        const order = [guidOf(0), guidOf(1), guidOf(3), guidOf(2), guidOf(4), guidOf(5), guidOf(6), guidOf(7)];
        host.setPlaylists(order);
        host.emit('playlist:reordered', { count: 8, guids: order });
        await settle();

        expectKept(view);
        expect(host.fb.playlist.getCount.calls.length).toBe(reads);
    });

    it('stays on a GUID while playlists in front of it are removed, and empties once it goes', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, guidOf(3));
        dirty(view);

        host.setPlaylists([guidOf(2), guidOf(3), guidOf(4)]);
        host.emit('playlist:removed', {
            oldCount: 8,
            newCount: 3,
            indices: [0, 1, 5, 6, 7],
            guids: [guidOf(0), guidOf(1), guidOf(5), guidOf(6), guidOf(7)],
        });
        await settle();
        expect(view._shown).toBe(guidOf(3));
        expectKept(view);

        const reads = host.fb.playlist.getCount.calls.length;
        host.setPlaylists([guidOf(2), guidOf(4)]);
        host.emit('playlist:removed', { oldCount: 3, newCount: 2, indices: [1], guids: [guidOf(3)] });
        await settle();
        expect(view._shown).toBe(null);
        expectClean(view);
        expect(host.fb.playlist.getCount.calls.length).toBe(reads);
        expect(view.getAttribute('track-count')).toBe('0');
    });

    it('reloads its rows only for item changes of the playlist it shows', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, 1);
        const reads = host.fb.playlist.getCount.calls.length;

        host.emit('playlist:itemsAdded', { playlist: 0, playlistGuid: guidOf(0), start: 0, count: 1 });
        await settle();
        expect(host.fb.playlist.getCount.calls.length).toBe(reads);

        host.emit('playlist:itemsAdded', { playlist: 1, playlistGuid: guidOf(1), start: 0, count: 1 });
        await settle();
        expect(host.fb.playlist.getCount.calls.length).toBe(reads + 1);
    });

    it('marks the playing row of the newly shown playlist', async () => {
        const host = makeFb(playingAt(2, 9));
        const view = await mountOn(host, 1);
        expect(view._playingIndex).toBe(-1);

        view.setAttribute('playlist', '2');
        view.attributeChangedCallback('playlist');
        await settle();

        expect(view._playingIndex).toBe(9);
    });

    it('moves its focus only for a focus change of the playlist it shows', async () => {
        const host = makeFb(NOT_PLAYING);
        const view = await mountOn(host, 1);

        host.emit('playlist:focusChanged', { playlist: 0, playlistGuid: guidOf(0), from: -1, to: 7 });
        await settle();
        expect(view._focusedIndex).toBe(-1);

        host.emit('playlist:focusChanged', { playlist: 1, playlistGuid: guidOf(1), from: -1, to: 3 });
        await settle();
        expect(view._focusedIndex).toBe(3);
    });

    it('compares a focus change with the active playlist when it follows it', async () => {
        const host = makeFb(NOT_PLAYING, 2);
        const view = await mountOn(host, null);

        host.emit('playlist:focusChanged', { playlist: 1, playlistGuid: guidOf(1), from: -1, to: 7 });
        await settle();
        expect(view._focusedIndex).toBe(-1);

        host.emit('playlist:focusChanged', { playlist: 2, playlistGuid: guidOf(2), from: -1, to: 4 });
        await settle();
        expect(view._focusedIndex).toBe(4);
    });
});
