/**
 * `<fb-library-tree>` — typed library tree (artist / album / genre views).
 *
 * Renders top-level groups (artists, albums, or genres depending on
 * the `view` attribute) and lazily expands the children of each group
 * via `fb.library.getArtistAlbums` / `getAlbumTracks` / `search`.
 *
 * Selection model mirrors the DUI library viewer:
 * - Plain click       — single select.
 * - Ctrl / Cmd click  — toggle this row.
 * - Shift click       — range select from the previous anchor.
 * - Right click       — if the row is not selected, single-select it
 *                       first, then emit `fb-library-context` so theme
 *                       code can `fb.ui.showCustomMenu`.
 *
 * Events:
 * - `fb-library-select`   — selection changed (`{ key, type, view, artist?, selected }`).
 * - `fb-library-play`     — row double-clicked.
 * - `fb-library-context`  — right-click (`{ ..., x, y, selected }`).
 * - `fb-library-added`    — internal helper-only; emitted when the
 *                           component proactively adds tracks via
 *                           {@link FbLibraryTree.addToPlaylist}.
 *
 * Attributes (all observed):
 * - `view`   — `'artist' | 'album' | 'genre'` (default `'artist'`).
 * - `sort`   — currently passed through; default `'name'`.
 * - `filter` — currently passed through; default empty.
 *
 * Reflects host attributes: `view`, `item-count`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { formatTime } from './constants.js';
import { getFb } from './runtime.js';
import type {
    FbLibraryAddedDetail,
    FbLibraryContextDetail,
    FbLibraryPlayDetail,
    FbLibrarySelectDetail,
    FbLibrarySelectionEntry,
} from './types.js';

type LibraryView = 'artist' | 'album' | 'genre';

interface LibraryGroupItem {
    name?: string;
    trackCount?: number;
}

interface LibraryAlbumChild {
    name?: string;
    trackCount?: number;
}

interface LibraryTrackChild {
    title?: string;
    duration?: number;
    path?: string;
    absolutePath?: string;
}

const HEADER_LABELS: Record<LibraryView, string> = {
    artist: 'All Artists',
    album: 'All Albums',
    genre: 'All Genres',
};

export class FbLibraryTree extends FbBaseElement {
    private _container!: HTMLElement;
    private _view: LibraryView = 'artist';
    private _sort = 'name';
    private _filter = '';
    private _items: LibraryGroupItem[] = [];
    private _expanded = new Set<string>();
    private _loading = false;
    private _lastSelected: HTMLElement | null = null;
    /**
     * Monotonic counter bumped whenever the component enters a new
     * display generation — view / sort / filter change, or disconnect.
     * Every async `_loadItems` / `_loadChildren` call captures the current
     * value and bails if a newer generation started while the host call
     * was in flight, preventing stale results from clobbering the active
     * view or mutating a detached DOM.
     */
    private _generation = 0;

    static get observedAttributes(): string[] {
        return ['view', 'sort', 'filter'];
    }

    attributeChangedCallback(name: string): void {
        if (!this._domReady) return;
        if (name === 'view') {
            this._view =
                (this.getAttribute('view') as LibraryView) || 'artist';
            this._expanded.clear();
            this._generation++;
            void this._loadItems();
        }
        if (name === 'sort') {
            this._sort = this.getAttribute('sort') || 'name';
            this._generation++;
            void this._loadItems();
        }
        if (name === 'filter') {
            this._filter = this.getAttribute('filter') || '';
            this._generation++;
            void this._loadItems();
        }
    }

    override disconnectedCallback(): void {
        // Invalidate any in-flight `_loadItems` / `_loadChildren`
        // dispatches before the base class tears down subscriptions.
        this._generation++;
        super.disconnectedCallback();
    }

    protected override _buildDOM(): void {
        this._view = (this.getAttribute('view') as LibraryView) || 'artist';
        this._sort = this.getAttribute('sort') || 'name';
        this._filter = this.getAttribute('filter') || '';

        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:block;overflow-y:auto}` +
            `[part=container]{display:flex;flex-direction:column}` +
            `[part~=node]{display:flex;align-items:center;cursor:pointer;gap:4px}` +
            `[part~=child]{display:flex;align-items:center;cursor:pointer;gap:4px}` +
            `</style>` +
            `<div part="container" role="tree"></div>`;
        this._container = this._$<HTMLElement>('[part=container]')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._container, 'click', (e: Event) => {
            const me = e as MouseEvent;
            const target = me.target as HTMLElement | null;
            const node = target?.closest(
                '[part~=node],[part~=child]',
            ) as HTMLElement | null;
            if (!node) return;
            const key = node.dataset.key || '';
            const type = node.dataset.type || '';
            if (type === 'header') return;

            if (type === 'group') {
                // Click on the disclosure caret only — toggle without
                // changing the selection.
                if (target?.closest('[part=node-toggle]')) {
                    void this._toggleNode(key, node);
                    return;
                }
                this._selectNode(node, me.ctrlKey || me.metaKey, me.shiftKey);
                this._emit<FbLibrarySelectDetail>('fb-library-select', {
                    key,
                    type,
                    view: this._view,
                    selected: this._getSelectedInfo(),
                });
            } else if (type === 'album' || type === 'track') {
                this._selectNode(node, me.ctrlKey || me.metaKey, me.shiftKey);
                this._emit<FbLibrarySelectDetail>('fb-library-select', {
                    key,
                    type,
                    view: this._view,
                    artist: node.dataset.artist,
                    selected: this._getSelectedInfo(),
                });
            }
        });

        this._listen(this._container, 'dblclick', (e: Event) => {
            const target = (e as MouseEvent).target as HTMLElement | null;
            const node = target?.closest(
                '[part~=node],[part~=child]',
            ) as HTMLElement | null;
            if (!node) return;
            const key = node.dataset.key || '';
            const type = node.dataset.type || '';
            this._emit<FbLibraryPlayDetail>('fb-library-play', {
                key,
                type,
                view: this._view,
            });
        });

        this._listen(this._container, 'contextmenu', (e: Event) => {
            const me = e as MouseEvent;
            const target = me.target as HTMLElement | null;
            const node = target?.closest(
                '[part~=node],[part~=child]',
            ) as HTMLElement | null;
            if (!node) return;
            me.preventDefault();
            const type = node.dataset.type || '';
            if (type === 'header') return;
            // DUI behaviour: right-click on an unselected row → single-
            // select it first so the context menu reflects what the
            // user clicked, not the previous selection.
            if (!node.hasAttribute('selected')) {
                this._selectNode(node, false, false);
                this._emit<FbLibrarySelectDetail>('fb-library-select', {
                    key: node.dataset.key || '',
                    type,
                    view: this._view,
                    artist: node.dataset.artist,
                    selected: this._getSelectedInfo(),
                });
            }
            this._emit<FbLibraryContextDetail>('fb-library-context', {
                key: node.dataset.key || '',
                type,
                view: this._view,
                x: me.clientX,
                y: me.clientY,
                selected: this._getSelectedInfo(),
            });
        });
    }

    protected override _subscribe(): void {
        const reload = (): void => {
            this._items = [];
            void this._loadItems();
        };
        this._sub('library:itemsAdded', reload);
        this._sub('library:itemsRemoved', reload);
        this._sub('library:itemsModified', reload);
        this._sub('library:initialized', reload);
        void this._loadItems();
    }

    private async _toggleNode(key: string, node: HTMLElement): Promise<void> {
        if (this._expanded.has(key)) {
            this._expanded.delete(key);
            node.removeAttribute('expanded');
            const toggle = node.querySelector('[part=node-toggle]');
            if (toggle) toggle.textContent = '\u25B8';
            const children = node.nextElementSibling as HTMLElement | null;
            if (children?.getAttribute('part') === 'node-children') {
                children.style.display = 'none';
            }
        } else {
            this._expanded.add(key);
            node.setAttribute('expanded', '');
            const toggle = node.querySelector('[part=node-toggle]');
            if (toggle) toggle.textContent = '\u25BE';
            const children = node.nextElementSibling as HTMLElement | null;
            if (children?.getAttribute('part') === 'node-children') {
                children.style.display = '';
                if (!children.hasChildNodes()) {
                    await this._loadChildren(key, children);
                }
            }
        }
    }

    private async _loadItems(): Promise<void> {
        if (this._loading) return;
        this._loading = true;
        const fb = getFb();
        const gen = this._generation;

        try {
            let items: LibraryGroupItem[] = [];
            switch (this._view) {
                case 'album': {
                    const result = (await fb.library.getAlbums(5000)) as
                        | { albums?: LibraryGroupItem[] }
                        | null;
                    items = result?.albums || [];
                    break;
                }
                case 'genre': {
                    const result = (await fb.library.getGenres()) as
                        | {
                              items?: LibraryGroupItem[];
                              genres?: LibraryGroupItem[];
                          }
                        | null;
                    items = result?.items || result?.genres || [];
                    break;
                }
                case 'artist':
                default: {
                    const result = (await fb.library.getArtists(5000)) as
                        | { items?: LibraryGroupItem[] }
                        | null;
                    items = result?.items || [];
                    break;
                }
            }
            // Generation guard: view / sort / filter / disconnect happened
            // while the host call was in flight — drop the late result.
            if (gen !== this._generation || !this.isConnected) return;
            this._items = items;
            this._rebuildTree();
        } catch {
            if (gen !== this._generation || !this.isConnected) return;
            this._items = [];
            this._rebuildTree();
        } finally {
            this._loading = false;
        }
    }

    private _rebuildTree(): void {
        const view = this._view;
        const totalCount = this._items.length;
        const frag = document.createDocumentFragment();

        // Header row
        const header = document.createElement('div');
        header.setAttribute('part', 'header');
        header.dataset.type = 'header';
        header.setAttribute('role', 'treeitem');
        header.innerHTML =
            `<span part="node-label">${HEADER_LABELS[view] || HEADER_LABELS.artist}</span>` +
            `<span part="node-count">(${totalCount})</span>`;
        frag.appendChild(header);

        for (const item of this._items) {
            const name = item.name || '(Unknown)';
            const key = name;
            const count = item.trackCount || 0;
            const expanded = this._expanded.has(key);

            const node = document.createElement('div');
            node.setAttribute('part', 'node');
            node.dataset.key = key;
            node.dataset.type = 'group';
            node.setAttribute('role', 'treeitem');
            if (expanded) node.setAttribute('expanded', '');
            node.innerHTML =
                `<span part="node-toggle">${expanded ? '\u25BE' : '\u25B8'}</span>` +
                `<span part="node-label">${this._escHtml(name)}</span>` +
                `<span part="node-count">(${count})</span>`;
            frag.appendChild(node);

            const childrenDiv = document.createElement('div');
            childrenDiv.setAttribute('part', 'node-children');
            childrenDiv.setAttribute('role', 'group');
            if (!expanded) childrenDiv.style.display = 'none';
            frag.appendChild(childrenDiv);
        }

        this._container.textContent = '';
        this._container.appendChild(frag);
        this.setAttribute('item-count', totalCount.toString());
        this.setAttribute('view', view);

        // Repopulate previously expanded groups (cache miss → refetch).
        for (const key of this._expanded) {
            const node = this._container.querySelector(
                `[data-key="${CSS.escape(key)}"][expanded]`,
            ) as HTMLElement | null;
            if (node) {
                const children = node.nextElementSibling as HTMLElement | null;
                if (
                    children?.getAttribute('part') === 'node-children' &&
                    !children.hasChildNodes()
                ) {
                    void this._loadChildren(key, children);
                }
            }
        }
    }

    private async _loadChildren(
        key: string,
        container: HTMLElement,
    ): Promise<void> {
        const fb = getFb();
        const gen = this._generation;
        try {
            let html = '';
            const view = this._view;

            if (view === 'artist') {
                const result = (await fb.library.getArtistAlbums(key)) as
                    | { albums?: LibraryAlbumChild[] }
                    | null;
                const albums = result?.albums || [];
                html = albums
                    .map(
                        (a) =>
                            `<div part="child" data-key="${this._escAttr(a.name || '')}" data-type="album" ` +
                            `data-artist="${this._escAttr(key)}" role="treeitem">` +
                            `<span part="node-label">${this._escHtml(a.name || '(Unknown Album)')}</span>` +
                            `<span part="node-count">(${a.trackCount || 0})</span></div>`,
                    )
                    .join('');
            } else if (view === 'album') {
                const result = (await fb.library.getAlbumTracks(key)) as
                    | {
                          items?: LibraryTrackChild[];
                          tracks?: LibraryTrackChild[];
                      }
                    | null;
                const tracks = result?.items || result?.tracks || [];
                html = tracks
                    .map(
                        (t) =>
                            `<div part="child" data-key="${this._escAttr(t.path || t.absolutePath || '')}" ` +
                            `data-type="track" role="treeitem">` +
                            `<span part="node-label">${this._escHtml(t.title || '(Unknown)')}</span>` +
                            `<span part="node-count">${formatTime(t.duration || 0)}</span></div>`,
                    )
                    .join('');
            } else if (view === 'genre') {
                const result = (await fb.library.search(
                    'genre IS "' + key.replace(/"/g, '\\"') + '"',
                    500,
                )) as
                    | {
                          items?: LibraryTrackChild[];
                          tracks?: LibraryTrackChild[];
                      }
                    | null;
                const tracks = result?.items || result?.tracks || [];
                html = tracks
                    .map(
                        (t) =>
                            `<div part="child" data-key="${this._escAttr(t.path || t.absolutePath || '')}" ` +
                            `data-type="track" role="treeitem">` +
                            `<span part="node-label">${this._escHtml(t.title || '(Unknown)')}</span>` +
                            `<span part="node-count">${formatTime(t.duration || 0)}</span></div>`,
                    )
                    .join('');
            }
            // Generation guard: disconnect or view / sort / filter change
            // happened while we awaited; drop the result and leave any
            // detached container untouched (GC handles cleanup).
            if (
                gen !== this._generation ||
                !this.isConnected ||
                !container.isConnected
            )
                return;
            container.innerHTML =
                html ||
                '<div part="child" data-type="empty" role="treeitem"><span part="node-label">(empty)</span></div>';
        } catch {
            if (
                gen !== this._generation ||
                !this.isConnected ||
                !container.isConnected
            )
                return;
            container.innerHTML =
                '<div part="child" data-type="empty" role="treeitem"><span part="node-label">(failed to load)</span></div>';
        }
    }

    /**
     * Programmatically add a row's tracks to the active playlist.
     * Themes typically wire this into the right-click context menu;
     * exposed publicly so callers don't have to re-implement the same
     * fan-out logic.
     */
    async addToPlaylist(
        key: string,
        type: string,
        artist?: string,
    ): Promise<void> {
        const fb = getFb();
        try {
            let paths: string[] = [];
            if (type === 'track') {
                paths = [key];
            } else if (type === 'album') {
                const result = (await fb.library.getAlbumTracks(
                    key,
                    artist || '',
                )) as
                    | {
                          items?: LibraryTrackChild[];
                          tracks?: LibraryTrackChild[];
                      }
                    | null;
                paths = (result?.items || result?.tracks || [])
                    .map((t) => t.path || t.absolutePath || '')
                    .filter(Boolean);
            } else if (type === 'group') {
                if (this._view === 'artist') {
                    const result = (await fb.library.getArtistTracks(
                        key,
                    )) as
                        | {
                              items?: LibraryTrackChild[];
                              tracks?: LibraryTrackChild[];
                          }
                        | null;
                    paths = (result?.items || result?.tracks || [])
                        .map((t) => t.path || t.absolutePath || '')
                        .filter(Boolean);
                } else if (this._view === 'genre') {
                    const result = (await fb.library.search(
                        'genre IS "' + key.replace(/"/g, '\\"') + '"',
                        5000,
                    )) as
                        | {
                              items?: LibraryTrackChild[];
                              tracks?: LibraryTrackChild[];
                          }
                        | null;
                    paths = (result?.items || result?.tracks || [])
                        .map((t) => t.path || t.absolutePath || '')
                        .filter(Boolean);
                } else if (this._view === 'album') {
                    const result = (await fb.library.getAlbumTracks(
                        key,
                    )) as
                        | {
                              items?: LibraryTrackChild[];
                              tracks?: LibraryTrackChild[];
                          }
                        | null;
                    paths = (result?.items || result?.tracks || [])
                        .map((t) => t.path || t.absolutePath || '')
                        .filter(Boolean);
                }
            }
            if (paths.length > 0) {
                await fb.library.addToPlaylist(paths);
                this._emit<FbLibraryAddedDetail>('fb-library-added', {
                    count: paths.length,
                    type,
                    key,
                });
            }
        } catch {
            /* silent */
        }
    }

    private _selectNode(
        node: HTMLElement,
        ctrlKey = false,
        shiftKey = false,
    ): void {
        const sel = (n: HTMLElement): void => {
            n.setAttribute('selected', '');
            const part = n.getAttribute('part') || '';
            n.setAttribute('part', `${part} selected`);
        };
        const desel = (n: HTMLElement): void => {
            n.removeAttribute('selected');
            const part = n.getAttribute('part') || '';
            n.setAttribute('part', part.replace(' selected', ''));
        };
        if (ctrlKey) {
            if (node.hasAttribute('selected')) desel(node);
            else sel(node);
        } else if (shiftKey && this._lastSelected) {
            const allNodes = Array.from(
                this._container.querySelectorAll<HTMLElement>(
                    '[data-type=group],[data-type=album],[data-type=track]',
                ),
            );
            const from = allNodes.indexOf(this._lastSelected);
            const to = allNodes.indexOf(node);
            if (from >= 0 && to >= 0) {
                this._container
                    .querySelectorAll<HTMLElement>('[selected]')
                    .forEach((n) => desel(n));
                const [start, end] = from < to ? [from, to] : [to, from];
                for (let i = start; i <= end; i++) sel(allNodes[i]);
            }
        } else {
            this._container
                .querySelectorAll<HTMLElement>('[selected]')
                .forEach((n) => desel(n));
            sel(node);
        }
        this._lastSelected = node;
    }

    private _getSelectedInfo(): FbLibrarySelectionEntry[] {
        return Array.from(
            this._container.querySelectorAll<HTMLElement>('[selected]'),
        ).map((n) => ({
            key: n.dataset.key || '',
            type: n.dataset.type || '',
            artist: n.dataset.artist,
        }));
    }

    /**
     * Attribute-safe escape (separate from {@link _escHtml}). HTML
     * escape doesn't always fully sanitise an attribute context, so
     * `&` / `"` / `<` / `>` are escaped together for safe `data-*`
     * payload interpolation.
     */
    private _escAttr(s: string): string {
        return String(s)
            .replace(/&/g, '&amp;')
            .replace(/"/g, '&quot;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
    }
}
