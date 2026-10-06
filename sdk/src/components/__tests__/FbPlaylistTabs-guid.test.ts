// sdk/src/components/__tests__/FbPlaylistTabs-guid.test.ts
//
// `<fb-playlist-tabs>` lists playlists as they were when it last read them,
// while the host may have added or removed some since. The tabs therefore
// name playlists by GUID when they activate or reorder them, announce a
// gesture only once the host carried it out, and ignore a read of the
// playlists that a newer read overtook. The element runs without a DOM here:
// it is never connected, and the test calls the methods a gesture ends in.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { FakeElement, guidOf, settle } from './playlistViewHarness.js';

interface TabsInternals {
    _playlists: Array<{ guid: string; name: string }>;
    _activeIndex: number;
    _rebuildTabs(): void;
    _updateActive(): void;
    _subscribe(): void;
    _loadPlaylists(): Promise<void>;
    _activatePlaylist(index: number): Promise<void>;
    _reorderPlaylists(fromIndex: number, toIndex: number): Promise<void>;
}

type Handler = (data: unknown) => void;

/** `FakeElement` that keeps the events the element dispatches. */
class RecordingElement extends FakeElement {
    readonly dispatched: CustomEvent[] = [];
    override dispatchEvent(event?: unknown): boolean {
        this.dispatched.push(event as CustomEvent);
        return true;
    }
}

function rows(guids: string[]) {
    return guids.map((guid, index) => ({
        index,
        guid,
        name: `list ${index}`,
        trackCount: 0,
        isActive: index === 0,
    }));
}

function makeFb() {
    const handlers = new Map<string, Handler[]>();
    const pending: Array<(guids: string[]) => void> = [];
    const listed = [guidOf(0), guidOf(1), guidOf(2)];
    let holding = false;
    let setActiveAnswer: { success: boolean; code?: string } = { success: true };
    const setActive = vi.fn(async (_ref: number | string) => setActiveAnswer);
    const reorderPlaylists = vi.fn(async (_order: readonly number[] | readonly string[]) => ({
        success: true,
    }));
    const fb = {
        on: (event: string, handler: Handler) => {
            handlers.set(event, [...(handlers.get(event) ?? []), handler]);
            return () => undefined;
        },
        playlist: {
            getAll: vi.fn(
                () =>
                    new Promise((resolve) => {
                        const answer = (guids: string[]) =>
                            resolve({ success: true, count: guids.length, playlists: rows(guids) });
                        if (holding) pending.push(answer);
                        else answer(listed);
                    }),
            ),
            setActive,
            reorderPlaylists,
        },
        // The components accept window.fb only when it has the SDK's invoke; the tabs never call it.
        invoke: vi.fn(async () => ({ success: false, code: 'NOT_SUPPORTED' })),
    };
    return {
        fb,
        setActive,
        reorderPlaylists,
        failSetActive(): void {
            setActiveAnswer = { success: false, code: 'NOT_FOUND' };
        },
        /** Hold every later read until `release` answers it. */
        holdReads(): void {
            holding = true;
        },
        /** Answer the held read number `index`, counted from `0`, with these playlists. */
        release(index: number, guids: string[]): void {
            pending[index]!(guids);
        },
        emit(event: string, data: unknown): void {
            for (const h of handlers.get(event) ?? []) h(data);
        },
    };
}

type Host = ReturnType<typeof makeFb>;

async function mountTabs(host: Host): Promise<TabsInternals & RecordingElement> {
    vi.stubGlobal('window', { fb: host.fb });
    const { FbPlaylistTabs } = await import('../FbPlaylistTabs.js');
    const tabs = new FbPlaylistTabs() as unknown as TabsInternals & RecordingElement;
    // Neither touches anything the tests read; both need the Shadow DOM.
    tabs._rebuildTabs = () => undefined;
    tabs._updateActive = () => undefined;
    tabs._subscribe();
    await settle();
    return tabs;
}

function details(tabs: RecordingElement, name: string): unknown[] {
    return tabs.dispatched.filter((e) => e.type === name).map((e) => e.detail);
}

describe('FbPlaylistTabs · playlists named by GUID', () => {
    beforeEach(() => {
        vi.resetModules();
        vi.stubGlobal('HTMLElement', RecordingElement);
    });

    afterEach(() => {
        vi.unstubAllGlobals();
    });

    it('activates a tab by its GUID and announces index and GUID', async () => {
        const host = makeFb();
        const tabs = await mountTabs(host);

        await tabs._activatePlaylist(2);

        expect(host.setActive).toHaveBeenCalledWith(guidOf(2));
        expect(details(tabs, 'fb-playlist-select')).toEqual([{ index: 2, guid: guidOf(2) }]);
    });

    it('announces nothing when the tab\'s playlist is gone', async () => {
        const host = makeFb();
        const tabs = await mountTabs(host);
        host.failSetActive();

        await tabs._activatePlaylist(1);

        expect(details(tabs, 'fb-playlist-select')).toEqual([]);
    });

    it('reorders by GUID and reports the order both ways', async () => {
        const host = makeFb();
        const tabs = await mountTabs(host);

        await tabs._reorderPlaylists(0, 3);

        const guids = [guidOf(1), guidOf(2), guidOf(0)];
        expect(host.reorderPlaylists).toHaveBeenCalledWith(guids);
        expect(details(tabs, 'fb-playlist-reorder')).toEqual([
            { fromIndex: 0, toIndex: 3, newOrder: [1, 2, 0], newOrderGuids: guids },
        ]);
    });

    it('keeps the newer read of the playlists when an older one answers last', async () => {
        const host = makeFb();
        const tabs = await mountTabs(host);
        host.holdReads();

        void tabs._loadPlaylists();
        void tabs._loadPlaylists();
        host.release(1, [guidOf(5), guidOf(6)]);
        await settle();
        host.release(0, [guidOf(0), guidOf(1), guidOf(2)]);
        await settle();

        expect(tabs._playlists.map((p) => p.guid)).toEqual([guidOf(5), guidOf(6)]);
    });

    it('finds the newly active tab by the GUID of playlist:activated', async () => {
        const host = makeFb();
        const tabs = await mountTabs(host);

        host.emit('playlist:activated', { oldIndex: 0, newIndex: 0, newGuid: guidOf(2) });
        expect(tabs._activeIndex).toBe(2);

        host.emit('playlist:activated', { oldIndex: 2, newIndex: -1, newGuid: null });
        expect(tabs._activeIndex).toBe(-1);
    });
});
