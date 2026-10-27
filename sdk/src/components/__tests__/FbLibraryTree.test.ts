import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { AlbumTrackRef } from '../../types/generated/schema-types.js';
import type { FbLibrarySelectDetail } from '../types.js';
import { FakeElement, settle } from './playlistViewHarness.js';

// Minimal DOM surface for the actual tree rendering and gesture handlers.
// Child divs are parsed so two identical display names remain distinct nodes.
class TreeNode extends FakeElement {
    readonly dataset: Record<string, string> = {};
    readonly style = { display: '' };
    readonly children: TreeNode[] = [];
    readonly listeners = new Map<string, (event: object) => void>();
    readonly dispatched: CustomEvent[] = [];
    isConnected = true;
    parent: TreeNode | null = null;
    fragment = false;
    private _html = '';

    override getAttribute(name: string): string | null {
        if (name.startsWith('data-')) {
            return this.dataset[this.dataKey(name)] ?? null;
        }
        return super.getAttribute(name);
    }
    override setAttribute(name: string, value: string): void {
        if (name.startsWith('data-')) this.dataset[this.dataKey(name)] = value;
        else super.setAttribute(name, value);
    }
    private dataKey(name: string): string {
        return name.slice(5).replace(/-([a-z])/g, (_, char: string) => char.toUpperCase());
    }
    hasAttribute(name: string): boolean { return this.getAttribute(name) !== null; }
    hasChildNodes(): boolean { return this.children.length > 0 || !!this._html; }
    appendChild(node: TreeNode): TreeNode {
        if (node.fragment) {
            for (const child of [...node.children]) this.appendChild(child);
        } else {
            node.parent = this;
            this.children.push(node);
        }
        return node;
    }
    get nextElementSibling(): TreeNode | null {
        return this.parent?.children[this.parent.children.indexOf(this) + 1] ?? null;
    }
    set textContent(value: string) {
        this.children.length = 0;
        this._html = value;
    }
    get innerHTML(): string { return this._html; }
    set innerHTML(value: string) {
        this._html = value;
        this.children.length = 0;
        for (const match of value.matchAll(/<div\s+([^>]+)>([\s\S]*?)<\/div>/g)) {
            const child = new TreeNode();
            for (const attr of match[1].matchAll(/([\w-]+)="([^"]*)"/g)) {
                child.setAttribute(attr[1], attr[2].replace(/&quot;/g, '"').replace(/&amp;/g, '&'));
            }
            child.innerHTML = match[2];
            this.appendChild(child);
        }
    }
    private matches(selector: string): boolean {
        return selector.split(',').some((part) =>
            [...part.matchAll(/\[([\w-]+)(?:(~=|=)"?([^"\]]*)"?)?\]/g)].every(
                ([, key, op, value]) => {
                    const actual = this.getAttribute(key);
                    return op === '~=' ? !!actual?.split(' ').includes(value)
                        : op === '=' ? actual === value : actual !== null;
                },
            ),
        );
    }
    closest(selector: string): TreeNode | null {
        return this.matches(selector) ? this : this.parent?.closest(selector) ?? null;
    }
    querySelectorAll(selector: string): TreeNode[] {
        return this.children.flatMap((child) => [
            ...(child.matches(selector) ? [child] : []), ...child.querySelectorAll(selector),
        ]);
    }
    querySelector(selector: string): TreeNode | null {
        return this.querySelectorAll(selector)[0] ?? null;
    }
    addEventListener(name: string, listener: (event: object) => void): void {
        this.listeners.set(name, listener);
    }
    gesture(name: string, target: TreeNode, ctrlKey = false): void {
        this.listeners.get(name)?.({ target, ctrlKey, preventDefault() {}, clientX: 4, clientY: 5 });
    }
    override dispatchEvent(event?: CustomEvent): boolean {
        if (event) this.dispatched.push(event);
        return true;
    }
}

interface Group {
    name: string;
    albumArtist?: string;
    albumId?: string;
    tracks?: AlbumTrackRef[];
}

interface TreeInternals {
    _container: TreeNode;
    _view: 'artist' | 'album' | 'genre';
    _items: Group[];
    _setupEvents(): void;
    _rebuildTree(): void;
    _toggleNode(key: string, node: TreeNode): Promise<void>;
    addToPlaylist(key: string, type: string, artist?: string,
        options?: { albumId?: string; tracks?: AlbumTrackRef[] }): Promise<void>;
}

function makeHost() {
    return {
        on: vi.fn(),
        invoke: vi.fn(),
        library: {
            getArtistAlbums: vi.fn().mockResolvedValue({ success: true, albums: [] }),
            getAlbumTracks: vi.fn().mockResolvedValue({ success: true, tracks: [] }),
            getArtistTracks: vi.fn().mockResolvedValue({ success: true, tracks: [] }),
            search: vi.fn().mockResolvedValue({ success: true, tracks: [] }),
            addToPlaylist: vi.fn().mockResolvedValue({ success: true, added: 1 }),
        },
    };
}

async function mount(host: ReturnType<typeof makeHost>, view: TreeInternals['_view'], items: Group[]) {
    vi.stubGlobal('window', { fb: host });
    const { FbLibraryTree } = await import('../FbLibraryTree.js');
    const tree = new FbLibraryTree() as unknown as TreeInternals & TreeNode;
    tree._container = new TreeNode();
    tree._view = view;
    tree._items = items;
    tree._setupEvents();
    tree._rebuildTree();
    return tree;
}

function detail(tree: TreeNode, eventName = 'fb-library-select'): FbLibrarySelectDetail {
    const events = tree.dispatched.filter((event) => event.type === eventName);
    return events[events.length - 1]?.detail;
}

function ref(path: string, subsong?: number): AlbumTrackRef {
    return { path, absolutePath: path, trackNumber: 1, ...(subsong === undefined ? {} : { subsong }) };
}

describe('FbLibraryTree album identity', () => {
    beforeEach(() => {
        vi.resetModules();
        vi.stubGlobal('HTMLElement', TreeNode);
        vi.stubGlobal('CSS', { escape: (value: string) => value });
        vi.stubGlobal('document', {
            createElement: () => new TreeNode(),
            createDocumentFragment: () => Object.assign(new TreeNode(), { fragment: true }),
        });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('expands equal display names independently and restores each identity after rebuilding', async () => {
        const host = makeHost();
        host.library.getAlbumTracks.mockImplementation(async (_name, _artist, options) => ({
            success: true, tracks: [ref(options?.albumId === 'album1:b90041' ? 'one.cue' : 'two.cue', 2)],
        }));
        const items = ['album1:b90041', 'album1:ba0041'].map((albumId) => ({
            name: '\uFFFD', albumArtist: 'A', albumId,
        }));
        const tree = await mount(host, 'album', items);
        const nodes = tree._container.querySelectorAll('[data-type=group]');

        for (const node of nodes) await tree._toggleNode(node.dataset.key, node);

        expect(nodes.map((node) => node.hasAttribute('expanded'))).toEqual([true, true]);
        expect(host.library.getAlbumTracks.mock.calls.map((call) => call.slice(0, 3))).toEqual([
            ['\uFFFD', 'A', { albumId: items[0].albumId }],
            ['\uFFFD', 'A', { albumId: items[1].albumId }],
        ]);
        expect(tree._container.querySelectorAll('[data-type=track]').map((node) => node.dataset.key))
            .toEqual(['one.cue|subsong:2', 'two.cue|subsong:2']);

        tree._rebuildTree();
        await settle();
        expect(tree._container.querySelectorAll('[expanded]')).toHaveLength(2);
        expect(host.library.getAlbumTracks).toHaveBeenCalledTimes(4);
    });

    it.each(['click', 'dblclick', 'contextmenu'])('carries albumId through %s events and public actions', async (gesture) => {
        const host = makeHost();
        host.library.getAlbumTracks.mockResolvedValue({ success: true, tracks: [ref('album.cue', 3)] });
        const tree = await mount(host, 'album', [{ name: '\uFFFD', albumArtist: '', albumId: 'album1:b900' }]);
        const node = tree._container.querySelector('[data-type=group]')!;

        tree._container.gesture(gesture, node);
        const event = detail(tree, gesture === 'dblclick' ? 'fb-library-play'
            : gesture === 'contextmenu' ? 'fb-library-context' : 'fb-library-select');

        expect(event).toMatchObject({ key: '\uFFFD', artist: '', albumId: 'album1:b900' });
        if (gesture !== 'dblclick') expect(event.selected[0]).toMatchObject({ albumId: 'album1:b900' });
        await tree.addToPlaylist(event.key, event.type, event.artist, event);
        expect(host.library.getAlbumTracks).toHaveBeenCalledWith('\uFFFD', '', { albumId: 'album1:b900' });
        expect(host.library.addToPlaylist).toHaveBeenCalledWith(['album.cue|subsong:3']);
    });

    it.each(['click', 'dblclick', 'contextmenu'])('keeps each same-named artist album member set in %s events', async (gesture) => {
        const host = makeHost();
        const tracks = [[ref('shared.cue', 1)], [ref('shared.cue', 2)]];
        host.library.getArtistAlbums.mockResolvedValue({ success: true,
            albums: tracks.map((members) => ({ name: '\uFFFD', tracks: members })),
        });
        const tree = await mount(host, 'artist', [{ name: 'A' }]);
        const group = tree._container.querySelector('[data-type=group]')!;
        await tree._toggleNode(group.dataset.key, group);
        const nodes = tree._container.querySelectorAll('[data-type=album]');
        expect(nodes).toHaveLength(2);

        for (const [index, node] of nodes.entries()) {
            tree._container.gesture(gesture, node, true);
            const event = detail(tree, gesture === 'dblclick' ? 'fb-library-play'
                : gesture === 'contextmenu' ? 'fb-library-context' : 'fb-library-select');
            expect(event).toMatchObject({ key: '\uFFFD', artist: 'A', tracks: tracks[index] });
            await tree.addToPlaylist(event.key, event.type, event.artist, event);
        }

        expect(host.library.addToPlaylist.mock.calls).toEqual([
            [['shared.cue|subsong:1']], [['shared.cue|subsong:2']],
        ]);
        expect(host.library.getArtistTracks).not.toHaveBeenCalled();
        expect(host.library.getArtistAlbums).toHaveBeenCalledWith('A');
        if (gesture === 'click') expect(detail(tree).selected.map((entry) => entry.tracks)).toEqual(tracks);
    });

    it('treats an explicit empty member set as authoritative', async () => {
        const host = makeHost();
        host.library.getArtistTracks.mockResolvedValue({ success: true, tracks: [{ ...ref('wrong.flac'), album: 'Album' }] });
        host.library.getArtistAlbums.mockResolvedValue({ success: true, albums: [{ name: 'Album', tracks: [] }] });
        const tree = await mount(host, 'artist', [{ name: 'A' }]);
        const group = tree._container.querySelector('[data-type=group]')!;
        await tree._toggleNode(group.dataset.key, group);
        tree._container.gesture('click', tree._container.querySelector('[data-type=album]')!);
        const event = detail(tree);

        expect(event.tracks).toEqual([]);
        await tree.addToPlaylist(event.key, event.type, event.artist, event);
        expect(host.library.getArtistTracks).not.toHaveBeenCalled();
        expect(host.library.addToPlaylist).not.toHaveBeenCalled();
    });

    it('uses the existing artist-name filter only when the host omits member references', async () => {
        const host = makeHost();
        host.library.getArtistTracks.mockResolvedValue({ success: true, tracks: [
            { ...ref('album.cue', 2), album: 'Album' },
            { ...ref('other.flac'), album: 'Other' },
        ] });
        host.library.getArtistAlbums.mockResolvedValue({ success: true, albums: [{ name: 'Album' }] });
        const tree = await mount(host, 'artist', [{ name: 'A' }]);
        const group = tree._container.querySelector('[data-type=group]')!;
        await tree._toggleNode(group.dataset.key, group);
        tree._container.gesture('click', tree._container.querySelector('[data-type=album]')!);
        const event = detail(tree);

        expect(event).not.toHaveProperty('tracks');
        await tree.addToPlaylist(event.key, event.type, event.artist, event);

        expect(host.library.getArtistTracks).toHaveBeenCalledWith('A', Number.MAX_SAFE_INTEGER);
        expect(host.library.addToPlaylist).toHaveBeenCalledWith(['album.cue|subsong:2']);
    });

    it('keeps a literal replacement-character album separate from opaque album identities', async () => {
        const host = makeHost();
        const tree = await mount(host, 'album', [
            { name: '\uFFFD', albumArtist: 'A', albumId: 'album1:b90041' },
            { name: '\uFFFD', albumArtist: 'A', albumId: 'album1:ba0041' },
            { name: '\uFFFD', albumArtist: 'A' },
        ]);
        const nodes = tree._container.querySelectorAll('[data-type=group]');

        for (const node of nodes) await tree._toggleNode(node.dataset.key, node);

        expect(nodes.map((node) => node.hasAttribute('expanded'))).toEqual([true, true, true]);
        expect(host.library.getAlbumTracks.mock.calls.map((call) => call[2]?.albumId))
            .toEqual(['album1:b90041', 'album1:ba0041', undefined]);
        tree._container.gesture('click', nodes[2]);
        expect(detail(tree)).not.toHaveProperty('albumId');
    });

    it.each(['artist', 'genre'] as const)('preserves subsongs in whole %s group actions', async (view) => {
        const host = makeHost();
        const tracks = [ref('shared.cue', 1), ref('shared.cue', 2)];
        host.library.getArtistTracks.mockResolvedValue({ success: true, tracks });
        host.library.search.mockResolvedValue({ success: true, tracks });
        const tree = await mount(host, view, [{ name: 'Group' }]);

        await tree.addToPlaylist('Group', 'group');

        expect(host.library.addToPlaylist).toHaveBeenCalledWith([
            'shared.cue|subsong:1', 'shared.cue|subsong:2',
        ]);
    });
});
