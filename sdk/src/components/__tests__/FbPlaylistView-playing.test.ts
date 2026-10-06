// sdk/src/components/__tests__/FbPlaylistView-playing.test.ts
//
// `<fb-playlist-view>` marks the row of the playing track with `playing`.
// The `playback:trackChanged` payload is the track itself and carries no
// playlist position, so the view asks `playback.getCurrentTrackIndex` where
// the track sits, and marks a row only when that location is in the playlist
// the view shows, compared by GUID. See `playlistViewHarness.ts` for how the
// view runs without a DOM.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import {
    FakeElement,
    guidOf,
    makeFb,
    mount,
    playingAt,
    settle,
    type ViewInternals,
} from './playlistViewHarness.js';

type Host = ReturnType<typeof makeFb>;

async function mountOn(host: Host, playlist: number | null): Promise<ViewInternals> {
    vi.stubGlobal('window', { fb: host.fb });
    return mount(playlist);
}

describe('FbPlaylistView · playing row', () => {
    beforeEach(() => {
        vi.resetModules();
        vi.stubGlobal('HTMLElement', FakeElement);
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('marks the row the host reports when the track plays from the shown playlist', async () => {
        const host = makeFb(playingAt(1, 4));
        const view = await mountOn(host, 1);

        expect(host.getCurrentTrackIndex.calls.length).toBeGreaterThan(0);
        expect(view._playingIndex).toBe(4);
    });

    it('marks no row when the track plays from another playlist', async () => {
        const host = makeFb(playingAt(0, 4));
        const view = await mountOn(host, 1);

        expect(view._playingIndex).toBe(-1);
    });

    it('goes by the GUID, not the index, of the playing location', async () => {
        const host = makeFb({ ...playingAt(1, 4), playlistGuid: guidOf(5) });
        const view = await mountOn(host, 1);

        expect(view._playingIndex).toBe(-1);
    });

    it('marks no row when the playing track has no playlist location', async () => {
        const host = makeFb({
            success: true,
            found: false,
            playlist: null,
            playlistGuid: null,
            index: null,
        });
        const view = await mountOn(host, 1);

        expect(view._playingIndex).toBe(-1);
    });

    it('marks no row when the host answers with a failure', async () => {
        const host = makeFb({ success: false, error: 'boom', code: 'INTERNAL_ERROR' });
        const view = await mountOn(host, 1);

        expect(view._playingIndex).toBe(-1);
    });

    it('compares against the active playlist when the view follows it', async () => {
        const host = makeFb(playingAt(3, 1), 3);
        const view = await mountOn(host, null);

        expect(host.fb.playlist.getActive.calls.length).toBeGreaterThan(0);
        expect(view._playingIndex).toBe(1);
    });

    it('reads the location again on playback:trackChanged, ignoring the payload', async () => {
        const host = makeFb(playingAt(1, 4));
        const view = await mountOn(host, 1);

        host.answerWith(playingAt(1, 5));
        host.emit('playback:trackChanged', { title: 'next', duration: 1 });
        await settle();
        expect(view._playingIndex).toBe(5);

        host.answerWith(playingAt(2, 0));
        host.emit('playback:trackChanged', { title: 'elsewhere', duration: 1 });
        await settle();
        expect(view._playingIndex).toBe(-1);
    });

    it('reads the location again when the rows reload', async () => {
        const host = makeFb(playingAt(1, 6));
        const view = await mountOn(host, 1);

        host.answerWith(playingAt(1, 3));
        host.emit('playlist:itemsRemoved', {
            playlist: 1,
            playlistGuid: guidOf(1),
            oldCount: 20,
            newCount: 17,
        });
        await settle();

        expect(view._playingIndex).toBe(3);
    });

    it('clears the mark on a stop, but not on a stop that starts another track', async () => {
        const host = makeFb(playingAt(1, 4));
        const view = await mountOn(host, 1);
        const reads = host.getCurrentTrackIndex.calls.length;

        host.emit('playback:stopped', { reason: 'starting_another' });
        await settle();
        expect(view._playingIndex).toBe(4);

        host.emit('playback:stopped', { reason: 'user' });
        await settle();
        expect(view._playingIndex).toBe(-1);
        expect(host.getCurrentTrackIndex.calls.length).toBe(reads);
    });

    it('drops an answer that arrives after a newer read', async () => {
        const host = makeFb(playingAt(1, 2));
        const view = await mountOn(host, 1);

        const answerOlder = host.holdNext();
        const answerNewer = host.holdNext();
        host.emit('playback:trackChanged', {});
        host.emit('playback:trackChanged', {});

        answerNewer(playingAt(1, 7));
        await settle();
        expect(view._playingIndex).toBe(7);

        answerOlder(playingAt(1, 3));
        await settle();
        expect(view._playingIndex).toBe(7);
    });

    it('drops an answer that arrives after a stop', async () => {
        const host = makeFb(playingAt(1, 2));
        const view = await mountOn(host, 1);

        const answer = host.holdNext();
        host.emit('playback:trackChanged', {});
        host.emit('playback:stopped', { reason: 'user' });
        answer(playingAt(1, 5));
        await settle();

        expect(view._playingIndex).toBe(-1);
    });
});
