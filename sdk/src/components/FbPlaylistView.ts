/**
 * `<fb-playlist-view>` — virtually-scrolled playlist row view.
 *
 * Capabilities:
 * - Virtual scroll with a row pool (`_visibleRows`) and per-row data
 *   cache (`_dataCache`); only the rows in the viewport (+2 above /
 *   below as buffer) are realised in DOM.
 * - Selection model (Set) with click / Ctrl-toggle / Shift-range /
 *   Ctrl-A select-all semantics, mirrored back to the host via
 *   `fb.playlist.setSelection`.
 * - Keyboard navigation (Arrow / Enter / Delete / Ctrl-A).
 * - Per-row rating cell (5 stars, `#FFD700` hover preview, click on
 *   active value clears).
 * - `playback:trackChanged` reflects the playing index via the
 *   `playing` row attribute.
 *
 * Attributes (observed):
 * - `playlist`: index to render. `< 0` (or omitted) means "follow
 *   the active playlist".
 * - `columns`: comma-separated column ids (default
 *   `'index,title,artist,album,duration'`).
 * - `row-height`: row height in pixels (default `32`).
 * - `grid-template`: `grid-template-columns` value forwarded to each
 *   row. When set, rows render as CSS grid (used by the parent's
 *   `<fb-resizable-header>` to keep columns aligned).
 * - `formats`: JSON `{ columnId: titleFormat }` map for custom
 *   formatting passed to `fb.playlist.getTracks(..., formats)`.
 *
 * Reflects host attributes `track-count` and `selected-count`.
 *
 * Implementation note: per-row render state (`_built`, `_ratingVal`,
 * …) is attached as ad-hoc properties on the row DOM nodes so the
 * row pool can reuse nodes without re-rendering identical cells.
 * The {@link RowEl} / {@link RatingCell} interfaces type these
 * extensions — same lifecycle, no behavioural drift.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { formatTime } from './constants.js';
import { getFb } from './runtime.js';
import type { JsonValue } from '../types/json.js';
import type {
    FbTrackContextDetail,
    FbTrackPlayDetail,
    FbTrackSelectDetail,
} from './types.js';

/** Row pool element with extra render-state metadata. */
interface RowEl extends HTMLDivElement {
    _built?: boolean;
    _colKey?: string;
    _empty?: boolean;
}

/** Rating cell with extra metadata for hover-preview and click handling. */
interface RatingCell extends HTMLSpanElement {
    _ratingVal?: number;
    _ratingBuilt?: boolean;
    _ratingHandler?: (e: MouseEvent) => void;
    _trackPath?: string;
}

/** Loose track shape used by the playlist data cache. */
interface PlaylistTrackData {
    duration?: number;
    rating?: number;
    bitrate?: number;
    sampleRate?: number;
    fileSize?: number;
    path?: string;
    absolutePath?: string;
    [key: string]: JsonValue;
}

export class FbPlaylistView extends FbBaseElement {
    private _playlistIndex = -1;
    private _trackCount = 0;
    private _columns: string[] = [
        'index',
        'title',
        'artist',
        'album',
        'duration',
    ];
    private _rowHeight = 32;
    private _selection: Set<number> = new Set();
    private _focusedIndex = -1;
    private _shiftAnchor = -1;
    private _playingIndex = -1;
    private _scrollTop = 0;
    private _visibleRows: RowEl[] = [];
    private _dataCache: Map<number, PlaylistTrackData> = new Map();
    private _rafId: number | null = null;
    private _loadingRange = false;
    private _gridTemplate = '';
    private _formats: Record<string, string> = {};
    private _resizeObserver: ResizeObserver | null = null;

    private _viewport!: HTMLDivElement;
    private _scrollContent!: HTMLDivElement;

    /**
     * Column-id → track-property mapping.
     *
     * Accepts both the snake_case / camelCase IDs the host emits and the
     * Chinese column IDs that `foo_playcount` exposes to other components.
     * The Chinese entries are **data identifiers**, not UI labels, and
     * must be kept verbatim so that playlist columns authored with
     * `foo_playcount`'s default localised headers resolve to the right
     * track property.
     */
    private static get _colMap(): Record<string, string> {
        return {
            album_artist: 'albumArtist',
            tracknumber: 'trackNumber',
            discnumber: 'discNumber',
            samplerate: 'sampleRate',
            filesize: 'fileSize',
            // foo_playcount English ids
            play_count: 'playCount',
            first_played: 'firstPlayed',
            last_played: 'lastPlayed',
            // foo_playcount Chinese ids (external data keys, not UI text)
            播放次数: 'playCount',
            首次播放: 'firstPlayed',
            最近播放: 'lastPlayed',
            添加日期: 'added',
            等级: 'rating',
        };
    }

    static get observedAttributes(): string[] {
        return ['playlist', 'columns', 'row-height', 'grid-template', 'formats'];
    }

    attributeChangedCallback(name: string): void {
        if (!this._domReady) return;
        if (name === 'columns') {
            this._columns = (
                this.getAttribute('columns') ||
                'index,title,artist,album,duration'
            )
                .split(',')
                .map((s) => s.trim());
            // Force every pooled row to rebuild its cells on next render.
            this._visibleRows.forEach((r) => {
                r._built = false;
            });
            this._updateRows();
        } else if (name === 'row-height') {
            this._rowHeight =
                parseInt(this.getAttribute('row-height') || '32', 10) || 32;
            this._recalcLayout();
        } else if (name === 'playlist') {
            void this._loadPlaylist();
        } else if (name === 'grid-template') {
            this._gridTemplate = this.getAttribute('grid-template') || '';
            this._applyGridToRows();
        } else if (name === 'formats') {
            try {
                this._formats = JSON.parse(
                    this.getAttribute('formats') || '{}',
                );
            } catch {
                this._formats = {};
            }
            // Custom-column change: drop the cache and reload.
            this._dataCache.clear();
            this._updateRows();
        }
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        this._rowHeight =
            parseInt(this.getAttribute('row-height') || '32', 10) || 32;
        this._columns = (
            this.getAttribute('columns') ||
            'index,title,artist,album,duration'
        )
            .split(',')
            .map((s) => s.trim());
        this._gridTemplate = this.getAttribute('grid-template') || '';
        try {
            this._formats = JSON.parse(this.getAttribute('formats') || '{}');
        } catch {
            this._formats = {};
        }
        const plAttr = this.getAttribute('playlist');
        if (plAttr !== null) this._playlistIndex = parseInt(plAttr, 10);

        // Structure only — themes own the scrollbar look (`background`
        // / `border-radius` for `::-webkit-scrollbar-*`). We keep
        // `width:6px` because the thin-scrollbar geometry is a
        // viewport-layout decision, not a colour decision.
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:block;overflow:hidden}` +
            `[part=viewport]{overflow-y:auto;position:relative;height:100%}` +
            `[part=viewport]::-webkit-scrollbar{width:6px}` +
            `[part=scroll-content]{position:relative}` +
            `[part=row]{position:absolute;left:0;right:0;display:flex;align-items:center;cursor:default}` +
            `[part=row]>*{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;min-width:0}` +
            `[part=row][data-grid]{display:grid;align-items:center;gap:0!important}` +
            `</style>` +
            `<div part="viewport">` +
            `<div part="scroll-content"></div>` +
            `</div>`;
        this._viewport = this._$<HTMLDivElement>('[part=viewport]')!;
        this._scrollContent = this._$<HTMLDivElement>('[part=scroll-content]')!;
    }

    protected override _setupEvents(): void {
        // R7: rAF-throttled scroll handler.
        this._listen(this._viewport, 'scroll', () => {
            if (this._rafId) return;
            this._rafId = requestAnimationFrame(() => {
                this._rafId = null;
                this._onScroll();
            });
        });

        // ResizeObserver: viewport size change → re-render. Solves the
        // first-paint blank when connectedCallback fires before layout.
        this._resizeObserver = new ResizeObserver(() => {
            if (this._trackCount > 0) this._recalcLayout();
        });
        this._resizeObserver.observe(this._viewport);

        this._listen<MouseEvent>(this._viewport, 'click', (e) => {
            const row = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=row]',
            );
            if (!row) return;
            const idx = parseInt(row.dataset.index || '-1', 10);
            if (isNaN(idx)) return;
            this._handleSelect(idx, e.ctrlKey || e.metaKey, e.shiftKey);
        });

        this._listen<MouseEvent>(this._viewport, 'dblclick', (e) => {
            const row = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=row]',
            );
            if (!row) return;
            const idx = parseInt(row.dataset.index || '-1', 10);
            if (!isNaN(idx)) void this._playTrack(idx);
        });

        this._listen<MouseEvent>(this._viewport, 'contextmenu', (e) => {
            void (async () => {
                const row = (
                    e.target as HTMLElement | null
                )?.closest<HTMLElement>('[part=row]');
                e.preventDefault();
                if (row) {
                    const idx = parseInt(row.dataset.index || '-1', 10);
                    if (!isNaN(idx) && !this._selection.has(idx)) {
                        this._handleSelect(idx, false, false);
                    }
                }
                // Wait for the C++ selection mirror before emitting.
                await this._syncSelection();
                this._emit<FbTrackContextDetail>('fb-track-context', {
                    indices: [...this._selection],
                    x: e.clientX,
                    y: e.clientY,
                });
            })();
        });

        this._listen<KeyboardEvent>(this._viewport, 'keydown', (e) => {
            switch (e.key) {
                case 'ArrowDown':
                    e.preventDefault();
                    this._moveFocus(1, e.shiftKey);
                    break;
                case 'ArrowUp':
                    e.preventDefault();
                    this._moveFocus(-1, e.shiftKey);
                    break;
                case 'Enter':
                    if (this._focusedIndex >= 0)
                        void this._playTrack(this._focusedIndex);
                    break;
                case 'Delete':
                    void this._deleteSelected();
                    break;
                case 'a':
                    if (e.ctrlKey || e.metaKey) {
                        e.preventDefault();
                        this._selectAll();
                    }
                    break;
            }
        });
    }

    // ─── Selection model ───────────────────────────────────────────

    private _handleSelect(
        index: number,
        ctrlKey: boolean,
        shiftKey: boolean,
    ): void {
        if (shiftKey && this._shiftAnchor >= 0) {
            const from = Math.min(this._shiftAnchor, index);
            const to = Math.max(this._shiftAnchor, index);
            if (!ctrlKey) this._selection.clear();
            for (let i = from; i <= to; i++) this._selection.add(i);
        } else if (ctrlKey) {
            if (this._selection.has(index)) this._selection.delete(index);
            else this._selection.add(index);
            this._shiftAnchor = index;
        } else {
            this._selection.clear();
            this._selection.add(index);
            this._shiftAnchor = index;
        }
        this._focusedIndex = index;
        void this._syncSelection();
        this._updateRows();
        this._emit<FbTrackSelectDetail>('fb-track-select', {
            index,
            indices: [...this._selection],
        });
    }

    private _moveFocus(delta: number, shiftKey: boolean): void {
        const newIdx = Math.max(
            0,
            Math.min(this._trackCount - 1, this._focusedIndex + delta),
        );
        if (newIdx === this._focusedIndex) return;
        this._handleSelect(newIdx, false, shiftKey);
        this._scrollToIndex(newIdx);
    }

    private _selectAll(): void {
        this._selection.clear();
        for (let i = 0; i < this._trackCount; i++) this._selection.add(i);
        void this._syncSelection();
        this._updateRows();
    }

    private async _syncSelection(): Promise<void> {
        try {
            const pl = await this._getPlaylistIndex();
            await getFb().playlist.setSelection(pl, [...this._selection]);
        } catch {
            /* fb-playlist-view syncSelection error — silent */
        }
        this.setAttribute('selected-count', this._selection.size.toString());
    }

    private async _deleteSelected(): Promise<void> {
        if (this._selection.size === 0) return;
        try {
            const pl = await this._getPlaylistIndex();
            await getFb().playlist.removeTracks(pl, [...this._selection]);
            this._selection.clear();
        } catch {
            /* fb-playlist-view deleteSelected error — silent */
        }
    }

    private async _playTrack(index: number): Promise<void> {
        try {
            const pl = await this._getPlaylistIndex();
            await getFb().playlist.playTrack(pl, index);
            this._emit<FbTrackPlayDetail>('fb-track-play', { index });
        } catch {
            /* fb-playlist-view playTrack error — silent */
        }
    }

    // ─── Grid layout ──────────────────────────────────────────────

    private _applyGridToRows(): void {
        const gt = this._gridTemplate;
        this._visibleRows.forEach((row) => {
            if (gt) {
                row.dataset.grid = '';
                row.style.gridTemplateColumns = gt;
                row.style.gap = '0';
            } else {
                delete row.dataset.grid;
                row.style.gridTemplateColumns = '';
                row.style.gap = '';
            }
        });
    }

    // ─── Virtual scroll ───────────────────────────────────────────

    private _onScroll(): void {
        this._scrollTop = this._viewport.scrollTop;
        this._updateRows();
    }

    private _recalcLayout(): void {
        if (!this._scrollContent) return; // upgrade-window guard
        this._scrollContent.style.height =
            this._trackCount * this._rowHeight + 'px';
        this._updateRows();
    }

    private _scrollToIndex(index: number): void {
        const top = index * this._rowHeight;
        const bottom = top + this._rowHeight;
        if (top < this._viewport.scrollTop) {
            this._viewport.scrollTop = top;
        } else if (
            bottom >
            this._viewport.scrollTop + this._viewport.clientHeight
        ) {
            this._viewport.scrollTop = bottom - this._viewport.clientHeight;
        }
    }

    private _updateRows(): void {
        if (!this._viewport) return;
        const viewHeight = this._viewport.clientHeight;
        if (viewHeight === 0) return;
        const rh = this._rowHeight;
        const st = this._scrollTop;
        const startIdx = Math.max(0, Math.floor(st / rh) - 2);
        const endIdx = Math.min(
            this._trackCount,
            Math.ceil((st + viewHeight) / rh) + 2,
        );
        const needed = endIdx - startIdx;

        const useGrid = !!this._gridTemplate;

        // Grow the row pool on demand.
        while (this._visibleRows.length < needed) {
            const row = document.createElement('div') as RowEl;
            row.setAttribute('part', 'row');
            if (useGrid) {
                row.dataset.grid = '';
                row.style.gridTemplateColumns = this._gridTemplate;
                row.style.gap = '0';
            }
            this._scrollContent.appendChild(row);
            this._visibleRows.push(row);
        }
        // Hide unused pooled rows.
        for (let i = needed; i < this._visibleRows.length; i++) {
            this._visibleRows[i]!.style.display = 'none';
        }

        let loadMin = Infinity;
        let loadMax = -1;
        for (let vi = 0; vi < needed; vi++) {
            const i = startIdx + vi;
            const row = this._visibleRows[vi]!;
            row.style.display = '';
            // Apply / clear grid mode on each render so attribute toggles work.
            if (useGrid && !row.dataset.grid) {
                row.dataset.grid = '';
                row.style.gridTemplateColumns = this._gridTemplate;
                row.style.gap = '0';
            } else if (!useGrid && row.dataset.grid !== undefined) {
                delete row.dataset.grid;
                row.style.gridTemplateColumns = '';
                row.style.gap = '';
            }
            row.style.top = i * rh + 'px';
            row.style.height = rh + 'px';
            row.dataset.index = i.toString();

            if (this._selection.has(i)) row.setAttribute('selected', '');
            else row.removeAttribute('selected');
            if (i === this._focusedIndex) row.setAttribute('focused', '');
            else row.removeAttribute('focused');
            if (i === this._playingIndex) row.setAttribute('playing', '');
            else row.removeAttribute('playing');

            const cached = this._dataCache.get(i);
            if (cached) {
                this._fillRow(row, cached, i);
            } else {
                if (i < loadMin) loadMin = i;
                if (i > loadMax) loadMax = i;
                if (!row._empty) {
                    row.textContent = '';
                    row._empty = true;
                }
            }
        }

        if (loadMax >= 0) void this._loadRange(loadMin, loadMax + 1);
    }

    private _fillRow(
        row: RowEl,
        track: PlaylistTrackData,
        index: number,
    ): void {
        const isRatingCol = (col: string): boolean =>
            // `'等级'` is the `foo_playcount` Chinese column ID (data
            // identifier), not UI text — see `_colMap` JSDoc.
            col === '等级' || col === 'rating';
        const colKey = this._columns.join(',');
        if (row._built && row._colKey === colKey) {
            this._columns.forEach((col, ci) => {
                const cell = row.children[ci];
                if (!cell) return;
                if (isRatingCol(col))
                    this._fillRatingCell(cell as RatingCell, track);
                else cell.textContent = this._getCellValue(track, col, index);
            });
        } else {
            row.innerHTML = this._columns
                .map((col) => `<span part="row-${col}"></span>`)
                .join('');
            this._columns.forEach((col, ci) => {
                const cell = row.children[ci];
                if (!cell) return;
                if (isRatingCol(col))
                    this._fillRatingCell(cell as RatingCell, track);
                else cell.textContent = this._getCellValue(track, col, index);
            });
            row._built = true;
            row._colKey = colKey;
        }
        row._empty = false;
    }

    private _fillRatingCell(
        cell: RatingCell,
        track: PlaylistTrackData,
    ): void {
        const rating = parseInt(String(track.rating ?? 0), 10) || 0;
        const path = track.absolutePath || track.path || '';
        cell._trackPath = path;
        if (cell._ratingVal === rating && cell._ratingBuilt) return;
        cell._ratingVal = rating;
        cell._ratingBuilt = true;
        cell.innerHTML = '';
        for (let i = 1; i <= 5; i++) {
            const s = document.createElement('span');
            s.dataset.star = i.toString();
            const filled = i <= rating;
            s.textContent = filled ? '\u2605' : '\u2606';
            // Structure only — themes hook `[data-star][data-filled]` for
            // the fill colour and any hover transitions.
            // `padding:0 1px` widens the click target for the rating
            // cells (see `AGENTS.md §2.6.1` hit-target rule).
            s.style.cssText = 'cursor:pointer;padding:0 1px;display:inline-block';
            if (filled) s.dataset.filled = '';
            cell.appendChild(s);
        }
        if (!cell._ratingHandler) {
            const view = this;
            cell._ratingHandler = (e: MouseEvent): void => {
                e.stopPropagation();
                const star = (
                    e.target as HTMLElement | null
                )?.closest<HTMLElement>('[data-star]');
                if (!star) return;
                let val = parseInt(star.dataset.star || '0', 10);
                if (val === cell._ratingVal) val = 0;
                void (async () => {
                    try {
                        await getFb().rating.set(cell._trackPath || '', val);
                        cell._ratingVal = val;
                        cell.querySelectorAll<HTMLElement>('[data-star]').forEach(
                            (s) => {
                                const v = parseInt(s.dataset.star || '0', 10);
                                const filled = v <= val;
                                s.textContent = filled ? '\u2605' : '\u2606';
                                // Theme hook: `[data-star][data-filled]` for colour.
                                if (filled) s.dataset.filled = '';
                                else delete s.dataset.filled;
                            },
                        );
                        const rowParent = cell.closest<HTMLElement>('[part=row]');
                        const idx = parseInt(rowParent?.dataset.index || 'NaN', 10);
                        if (!isNaN(idx)) {
                            const cached = view._dataCache.get(idx);
                            if (cached) cached.rating = val;
                        }
                    } catch {
                        /* silent */
                    }
                })();
            };
            cell.addEventListener('click', cell._ratingHandler);
            // Hover preview.
            cell.addEventListener('mouseover', (e: Event) => {
                const star = (
                    (e as MouseEvent).target as HTMLElement | null
                )?.closest<HTMLElement>('[data-star]');
                if (!star) return;
                const hoverVal = parseInt(star.dataset.star || '0', 10);
                cell.querySelectorAll<HTMLElement>('[data-star]').forEach((s) => {
                    const v = parseInt(s.dataset.star || '0', 10);
                    s.textContent = v <= hoverVal ? '\u2605' : '\u2606';
                    s.style.color = v <= hoverVal ? '#FFD700' : '';
                    s.style.transform = v <= hoverVal ? 'scale(1.3)' : '';
                });
            });
            cell.addEventListener('mouseleave', () => {
                const ratingNow = cell._ratingVal || 0;
                cell.querySelectorAll<HTMLElement>('[data-star]').forEach((s) => {
                    const v = parseInt(s.dataset.star || '0', 10);
                    const filled = v <= ratingNow;
                    s.textContent = filled ? '\u2605' : '\u2606';
                    s.style.color = filled ? '#FFD700' : '';
                    s.style.transform = '';
                });
            });
        }
    }

    private _getCellValue(
        track: PlaylistTrackData,
        col: string,
        index: number,
    ): string {
        if (col === 'index') return (index + 1).toString();
        if (col === 'duration') return formatTime(track.duration || 0);
        if (col === 'filename') {
            const p = track.absolutePath || track.path || '';
            return p.split(/[\\/]/).pop()?.replace(/\.[^.]+$/, '') || '';
        }
        if (col === 'directory') {
            const p = track.absolutePath || track.path || '';
            const parts = p.split(/[\\/]/);
            return parts.length > 1 ? parts[parts.length - 2]! : '';
        }
        if (col === 'filesize') {
            const sz = track.fileSize ?? 0;
            if (!sz || sz <= 0) return '';
            if (sz < 1024) return sz + ' B';
            if (sz < 1048576) return (sz / 1024).toFixed(0) + ' KB';
            return (sz / 1048576).toFixed(1) + ' MB';
        }
        if (col === 'bitrate') {
            return track.bitrate ? track.bitrate + ' kbps' : '';
        }
        if (col === 'samplerate') {
            return track.sampleRate ? track.sampleRate + ' Hz' : '';
        }
        const key = FbPlaylistView._colMap[col] || col;
        const val = track[key];
        if (
            val == null ||
            (val === 0 && (col === 'tracknumber' || col === 'discnumber'))
        )
            return '';
        return String(val);
    }

    private async _loadRange(start: number, end: number): Promise<void> {
        if (this._loadingRange) return;
        this._loadingRange = true;
        try {
            const pl = await this._getPlaylistIndex();
            const fmts =
                Object.keys(this._formats).length > 0
                    ? this._formats
                    : undefined;
            // `getTracks` resolves with a flat `PlaylistTrack[]`; the
            // envelope unwrap lives inside the SDK wrapper.
            // Route through `unknown` because `PlaylistTrackData` carries
            // an index signature the canonical `PlaylistTrack` doesn't.
            const arr = (await getFb().playlist.getTracks(
                pl,
                start,
                end - start,
                fmts,
            )) as unknown as PlaylistTrackData[];
            arr.forEach((t, i) => {
                this._dataCache.set(start + i, t);
            });
            // Only re-render when something was loaded — avoids an infinite
            // loop when the host returns an empty page for a valid range.
            if (arr.length > 0) this._updateRows();
        } catch {
            /* silent */
        } finally {
            this._loadingRange = false;
        }
    }

    private async _getPlaylistIndex(): Promise<number> {
        if (this._playlistIndex >= 0) return this._playlistIndex;
        try {
            const r = (await getFb().playlist.getActive()) as
                | number
                | { playlist?: number; index?: number; active?: number }
                | null;
            if (r == null) return 0;
            if (typeof r === 'number') return r;
            if (typeof r.playlist === 'number') return r.playlist;
            if (typeof r.index === 'number') return r.index;
            if (typeof r.active === 'number') return r.active;
            return 0;
        } catch {
            return 0;
        }
    }

    // ─── Subscriptions ───────────────────────────────────────────

    protected override _subscribe(): void {
        const reloadData = (): void => {
            this._dataCache.clear();
            void this._loadPlaylist();
        };
        this._sub('playlist:itemsAdded', reloadData);
        this._sub('playlist:itemsRemoved', reloadData);
        this._sub('playlist:itemsReordered', reloadData);
        this._sub('playlist:itemsReplaced', reloadData);
        this._sub('playlist:selectionChanged', () => {
            void this._loadExternalSelection();
        });
        this._sub('playlist:focusChanged', (data) => {
            const to = data?.to;
            if (to !== undefined) {
                this._focusedIndex = to;
                this._updateRows();
            }
        });
        this._sub('playlist:activated', () => {
            if (this._playlistIndex < 0) {
                this._dataCache.clear();
                void this._loadPlaylist();
            }
        });
        this._sub('playback:trackChanged', (data) => {
            this._playingIndex =
                (data as { index?: number } | null | undefined)?.index ?? -1;
            this._updateRows();
        });
        void this._loadPlaylist();
    }

    private async _loadPlaylist(): Promise<void> {
        try {
            const pl = await this._getPlaylistIndex();
            const r = (await getFb().playlist.getCount(pl)) as
                | { count?: number }
                | number
                | null;
            this._trackCount =
                (typeof r === 'object' && r !== null && r.count) ||
                (typeof r === 'number' ? r : 0) ||
                0;
            this.setAttribute('track-count', this._trackCount.toString());
            this._recalcLayout();
        } catch {
            /* fb-playlist-view loadPlaylist error — silent */
        }
    }

    private async _loadExternalSelection(): Promise<void> {
        try {
            const pl = await this._getPlaylistIndex();
            const sel = (await getFb().playlist.getSelection(pl)) as
                | { items?: number[] }
                | null;
            this._selection = new Set(sel?.items || []);
            this.setAttribute(
                'selected-count',
                this._selection.size.toString(),
            );
            this._updateRows();
        } catch {
            /* fb-playlist-view loadSelection error — silent */
        }
    }

    override disconnectedCallback(): void {
        if (this._rafId) {
            cancelAnimationFrame(this._rafId);
            this._rafId = null;
        }
        if (this._resizeObserver) {
            this._resizeObserver.disconnect();
            this._resizeObserver = null;
        }
        super.disconnectedCallback();
    }
}
