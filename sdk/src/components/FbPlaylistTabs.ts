/**
 * `<fb-playlist-tabs>` — horizontal playlist tab strip with reorder,
 * per-tab width adjustment, native context menu integration, and a
 * persistent tab-width cache in `localStorage`.
 *
 * Supported gestures:
 * - **Click** on a tab → `fb.playlist.setActive(index)`, emits
 *   `fb-playlist-select`.
 * - **Right-click** → emits `fb-playlist-context`. If
 *   `native-context` attribute is present, also calls
 *   `fb.menu.showNativePopup({ mode: 'playlist' })` after activating.
 * - **Drag** (>= 5 px from press point) → reorder via
 *   `fb.playlist.reorderPlaylists(perm)`, emits `fb-playlist-reorder`.
 * - **Drag from rightmost 5 px of tab** → resize tab width;
 *   width is keyed by playlist *name* in `localStorage` so renaming
 *   loses the saved width (intentional).
 * - **Wheel (vertical or horizontal)** → horizontal scroll when the
 *   tabs container overflows.
 * - **`+` button** → `fb.playlist.create('New Playlist')`, emits
 *   `fb-playlist-add`.
 *
 * Reflects host attributes: `active-index`, `count`, `dragging`,
 * `overflow`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type {
    FbPlaylistAddDetail,
    FbPlaylistContextDetail,
    FbPlaylistReorderDetail,
    FbPlaylistSelectDetail,
} from './types.js';

interface PlaylistRow {
    name: string;
    isActive?: boolean;
    isLocked?: boolean;
    trackCount: number;
}

interface DragState {
    tabEl: HTMLElement;
    startX: number;
    startY: number;
    index: number;
    started: boolean;
    pointerId: number;
}

interface ResizeState {
    tabEl: HTMLElement;
    startX: number;
    startWidth: number;
    index: number;
    pointerId: number;
}

export class FbPlaylistTabs extends FbBaseElement {
    private static readonly DRAG_THRESHOLD = 5;
    /** Width of the right-edge zone that triggers resize (pixels). */
    private static readonly RESIZE_EDGE = 5;
    private static readonly STORAGE_KEY = 'fb-playlist-tab-widths';

    private _activeIndex = -1;
    private _playlists: PlaylistRow[] = [];
    private _dragState: DragState | null = null;
    private _resizeState: ResizeState | null = null;
    private _tabWidths: Record<string, number> = {};

    private _container!: HTMLDivElement;
    private _dropIndicator!: HTMLDivElement;
    private _addBtn!: HTMLDivElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:flex;overflow:hidden;align-items:stretch;width:100%;min-width:0}` +
            `[part=tabs-container]{display:flex;flex:1;min-width:0;overflow-x:auto;overflow-y:hidden;scrollbar-width:none}` +
            `[part=tabs-container]::-webkit-scrollbar{display:none}` +
            `[part=tab]{cursor:pointer;display:inline-flex;align-items:center;flex-shrink:0;position:relative;box-sizing:border-box;user-select:none}` +
            `[part=drop-indicator]{position:absolute;top:0;bottom:0;width:2px;pointer-events:none;display:none;z-index:1}` +
            `[part=add-button]{cursor:pointer;display:inline-flex;align-items:center;justify-content:center;flex-shrink:0;user-select:none}` +
            `</style>` +
            `<div part="tabs-container" role="tablist">` +
            `<div part="drop-indicator"></div>` +
            `</div>` +
            `<div part="add-button" role="button" tabindex="0" aria-label="New playlist">+</div>`;
        this._container = this._$<HTMLDivElement>('[part=tabs-container]')!;
        this._dropIndicator = this._$<HTMLDivElement>('[part=drop-indicator]')!;
        this._addBtn = this._$<HTMLDivElement>('[part=add-button]')!;
        // Restore persisted tab widths.
        try {
            const saved = localStorage.getItem(FbPlaylistTabs.STORAGE_KEY);
            if (saved) this._tabWidths = JSON.parse(saved);
        } catch {
            /* corrupt JSON — fall through */
        }
    }

    protected override _setupEvents(): void {
        // ── Click activation ──
        this._listen<MouseEvent>(this._container, 'click', (e) => {
            if (this._dragState?.started) return; // suppress click after drag
            const tab = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=tab]',
            );
            if (!tab) return;
            const index = parseInt(tab.dataset.index || '-1', 10);
            if (!isNaN(index)) void this._activatePlaylist(index);
        });

        // ── Right-click ──
        this._listen<MouseEvent>(this._container, 'contextmenu', (e) => {
            const tab = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=tab]',
            );
            if (!tab) return;
            e.preventDefault();
            const index = parseInt(tab.dataset.index || '-1', 10);
            this._emit<FbPlaylistContextDetail>('fb-playlist-context', {
                index,
                x: e.clientX,
                y: e.clientY,
            });
            if (this.hasAttribute('native-context')) {
                void this._showNativeMenu(index);
            }
        });

        // ── Keyboard navigation ──
        this._listen<KeyboardEvent>(this._container, 'keydown', (e) => {
            if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') {
                e.preventDefault();
                const delta = e.key === 'ArrowRight' ? 1 : -1;
                const newIndex = Math.max(
                    0,
                    Math.min(
                        this._playlists.length - 1,
                        this._activeIndex + delta,
                    ),
                );
                void this._activatePlaylist(newIndex);
            }
        });

        // ── Pointer-down dispatches resize vs drag ──
        this._listen<PointerEvent>(this._container, 'pointerdown', (e) => {
            if (e.button !== 0) return;
            const tab = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=tab]',
            );
            if (!tab) return;
            const index = parseInt(tab.dataset.index || '-1', 10);
            if (isNaN(index)) return;

            const rect = tab.getBoundingClientRect();
            const isResizeEdge =
                e.clientX >= rect.right - FbPlaylistTabs.RESIZE_EDGE;

            if (isResizeEdge) {
                e.preventDefault();
                tab.setPointerCapture(e.pointerId);
                this._resizeState = {
                    tabEl: tab,
                    startX: e.clientX,
                    startWidth: rect.width,
                    index,
                    pointerId: e.pointerId,
                };
            } else {
                this._dragState = {
                    tabEl: tab,
                    startX: e.clientX,
                    startY: e.clientY,
                    index,
                    started: false,
                    pointerId: e.pointerId,
                };
            }
        });

        this._listen<PointerEvent>(this._container, 'pointermove', (e) => {
            // Resize path
            if (
                this._resizeState &&
                e.pointerId === this._resizeState.pointerId
            ) {
                const { tabEl, startX, startWidth } = this._resizeState;
                const delta = e.clientX - startX;
                const newWidth = Math.max(40, startWidth + delta);
                tabEl.style.width = newWidth + 'px';
                return;
            }
            // Drag-reorder path
            if (
                !this._dragState ||
                e.pointerId !== this._dragState.pointerId
            )
                return;
            const { startX, startY, started } = this._dragState;
            if (!started) {
                const dx = Math.abs(e.clientX - startX);
                const dy = Math.abs(e.clientY - startY);
                if (
                    dx < FbPlaylistTabs.DRAG_THRESHOLD &&
                    dy < FbPlaylistTabs.DRAG_THRESHOLD
                )
                    return;
                this._dragState.started = true;
                this.setAttribute('dragging', '');
                this._dragState.tabEl.setPointerCapture(e.pointerId);
            }
            this._updateDropPosition(e.clientX);
        });

        this._listen<PointerEvent>(this._container, 'pointerup', (e) => {
            // End resize
            if (
                this._resizeState &&
                e.pointerId === this._resizeState.pointerId
            ) {
                const { tabEl, index } = this._resizeState;
                const name = this._playlists[index]?.name;
                if (name) {
                    this._tabWidths[name] = tabEl.getBoundingClientRect().width;
                    this._saveTabWidths();
                }
                this._resizeState = null;
                return;
            }
            // End drag-reorder
            if (
                !this._dragState ||
                e.pointerId !== this._dragState.pointerId
            )
                return;
            const ds = this._dragState;
            this._dragState = null;
            this.removeAttribute('dragging');
            this._dropIndicator.style.display = 'none';
            if (!ds.started) return;

            const dropIdx = this._calcDropIndex(e.clientX);
            if (dropIdx === -1 || dropIdx === ds.index) return;
            void this._reorderPlaylists(ds.index, dropIdx);
        });

        this._listen<PointerEvent>(this._container, 'pointercancel', () => {
            if (this._resizeState) this._resizeState = null;
            if (this._dragState) {
                this._dragState = null;
                this.removeAttribute('dragging');
                this._dropIndicator.style.display = 'none';
            }
        });

        // ── Horizontal wheel ──
        this._listen<WheelEvent>(
            this,
            'wheel',
            (e) => {
                if (
                    this._container.scrollWidth <= this._container.clientWidth
                )
                    return;
                e.preventDefault();
                this._container.scrollLeft += e.deltaY || e.deltaX;
            },
            { passive: false },
        );

        // ── "+" new playlist button ──
        this._listen(this._addBtn, 'click', () => {
            void (async () => {
                try {
                    await getFb().playlist.create('New Playlist');
                    this._emit<FbPlaylistAddDetail>('fb-playlist-add', {});
                } catch {
                    /* fb-playlist-tabs create error — silent */
                }
            })();
        });

        // ── Resize-edge cursor ──
        this._listen<MouseEvent>(this._container, 'mousemove', (e) => {
            const tab = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=tab]',
            );
            if (!tab) {
                this._container.style.cursor = '';
                return;
            }
            const rect = tab.getBoundingClientRect();
            tab.style.cursor =
                e.clientX >= rect.right - FbPlaylistTabs.RESIZE_EDGE
                    ? 'col-resize'
                    : 'pointer';
        });
    }

    private async _showNativeMenu(index: number): Promise<void> {
        try {
            const fb = getFb();
            await fb.playlist.setActive(index);
            await fb.menu.showNativePopup({ mode: 'playlist' });
        } catch {
            /* fb-playlist-tabs nativeMenu error — silent */
        }
    }

    private _updateDropPosition(clientX: number): void {
        const tabs = this._$$<HTMLElement>('[part=tab]');
        let insertBefore = -1;
        for (let i = 0; i < tabs.length; i++) {
            const r = tabs[i]!.getBoundingClientRect();
            if (clientX < r.left + r.width / 2) {
                insertBefore = i;
                break;
            }
        }
        if (insertBefore === -1) insertBefore = tabs.length;
        const containerRect = this._container.getBoundingClientRect();
        let indicatorLeft: number;
        if (insertBefore < tabs.length) {
            indicatorLeft =
                tabs[insertBefore]!.getBoundingClientRect().left -
                containerRect.left +
                this._container.scrollLeft;
        } else if (tabs.length > 0) {
            const last = tabs[tabs.length - 1]!.getBoundingClientRect();
            indicatorLeft =
                last.right - containerRect.left + this._container.scrollLeft;
        } else {
            indicatorLeft = 0;
        }
        this._dropIndicator.style.display = 'block';
        this._dropIndicator.style.left = indicatorLeft + 'px';
    }

    private _calcDropIndex(clientX: number): number {
        const tabs = this._$$<HTMLElement>('[part=tab]');
        for (let i = 0; i < tabs.length; i++) {
            const r = tabs[i]!.getBoundingClientRect();
            if (clientX < r.left + r.width / 2) return i;
        }
        return tabs.length > 0 ? tabs.length : -1;
    }

    private async _reorderPlaylists(
        fromIndex: number,
        toIndex: number,
    ): Promise<void> {
        const count = this._playlists.length;
        const order: number[] = [];
        for (let i = 0; i < count; i++) order.push(i);
        const [removed] = order.splice(fromIndex, 1);
        const insertAt = toIndex > fromIndex ? toIndex - 1 : toIndex;
        order.splice(insertAt, 0, removed!);
        try {
            await getFb().playlist.reorderPlaylists(order);
            this._emit<FbPlaylistReorderDetail>('fb-playlist-reorder', {
                fromIndex,
                toIndex,
                newOrder: order,
            });
        } catch {
            /* fb-playlist-tabs reorder error — silent */
        }
    }

    private _saveTabWidths(): void {
        try {
            localStorage.setItem(
                FbPlaylistTabs.STORAGE_KEY,
                JSON.stringify(this._tabWidths),
            );
        } catch {
            /* localStorage unavailable / quota — silent */
        }
    }

    private _applyTabWidths(): void {
        const tabs = this._$$<HTMLElement>('[part=tab]');
        tabs.forEach((tab, i) => {
            const name = this._playlists[i]?.name;
            const w = name && this._tabWidths[name];
            if (w) tab.style.width = w + 'px';
            else tab.style.width = '';
        });
    }

    private _checkOverflow(): void {
        if (this._container.scrollWidth > this._container.clientWidth) {
            this.setAttribute('overflow', '');
        } else {
            this.removeAttribute('overflow');
        }
    }

    private async _activatePlaylist(index: number): Promise<void> {
        try {
            await getFb().playlist.setActive(index);
            this._emit<FbPlaylistSelectDetail>('fb-playlist-select', {
                index,
            });
        } catch {
            /* fb-playlist-tabs activate error — silent */
        }
    }

    protected override _subscribe(): void {
        const refresh = (): void => {
            void this._loadPlaylists();
        };
        this._sub('playlist:created', refresh);
        this._sub('playlist:removed', refresh);
        this._sub('playlist:renamed', refresh);
        this._sub('playlist:reordered', refresh);
        this._sub('playlist:itemsAdded', refresh);
        this._sub('playlist:itemsRemoved', refresh);
        this._sub('playlist:activated', (data) => {
            this._activeIndex = data?.newIndex ?? -1;
            this._updateActive();
        });
        void this._loadPlaylists();
    }

    private async _loadPlaylists(): Promise<void> {
        try {
            const result = (await getFb().playlist.getAll()) as
                | PlaylistRow[]
                | null;
            this._playlists = result ?? [];
            this._activeIndex = this._playlists.findIndex((p) => p.isActive);
            this._rebuildTabs();
        } catch {
            /* fb-playlist-tabs loadPlaylists error — silent */
        }
    }

    private _rebuildTabs(): void {
        const indicatorEl = this._dropIndicator;
        this._container.innerHTML = this._playlists
            .map(
                (pl, i) =>
                    `<div part="tab" data-index="${i}" role="tab" tabindex="${i === this._activeIndex ? '0' : '-1'}"` +
                    ` aria-selected="${i === this._activeIndex}"` +
                    `${pl.isLocked ? ' locked' : ''}>` +
                    `<span part="tab-name">${this._escHtml(pl.name)}</span>` +
                    `<span part="tab-count">${pl.trackCount}</span>` +
                    `</div>`,
            )
            .join('');
        this._container.appendChild(indicatorEl);
        this.setAttribute('active-index', this._activeIndex.toString());
        this.setAttribute('count', this._playlists.length.toString());
        this._applyTabWidths();
        // Defer overflow detection until layout settles.
        requestAnimationFrame(() => this._checkOverflow());
    }

    private _updateActive(): void {
        const tabs = this._$$<HTMLElement>('[part=tab]');
        tabs.forEach((tab) => {
            const i = parseInt(tab.dataset.index || '-1', 10);
            const active = i === this._activeIndex;
            tab.setAttribute('aria-selected', active.toString());
            tab.setAttribute('tabindex', active ? '0' : '-1');
        });
        this.setAttribute('active-index', this._activeIndex.toString());
        const activeTab = this._$<HTMLElement>(
            '[part=tab][aria-selected="true"]',
        );
        if (activeTab)
            activeTab.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    }
}
