/**
 * `<fb-resizable-header>` — header row used by the playlist view and
 * other tabular components.
 *
 * Capabilities:
 * - Column resize via per-cell `[part=resize-handle]` (drag the right
 *   edge of a column).
 * - Column reorder via cell drag (5 px threshold; `<` 5 px movement
 *   counts as a click that toggles sort direction instead).
 * - Click-to-sort (asc → desc → asc) on any column with
 *   `sortable !== false` and not `fixed`.
 *
 * Attributes (observed):
 * - `columns`: JSON array of column descriptors:
 *   `[{ id, label, width?, sortable?, fixed? }]`.
 * - `sort-column` and `sort-direction`: read-back of the current sort.
 *
 * Public getters used by parent components:
 * - {@link FbResizableHeader.gridTemplate} — current
 *   `grid-template-columns` value (fr/px).
 * - {@link FbResizableHeader.columnIds} — column id list in current
 *   visual order.
 *
 * **Note**: drag/resize listeners are attached to `document` directly
 * (not via {@link FbBaseElement._listen}) so the cursor stays pinned
 * even when the mouse exits the component during a drag. The
 * paired `onUp` handler removes them.
 */

import { FbBaseElement } from './FbBaseElement.js';
import type {
    FbColumnReorderDetail,
    FbColumnResizeDetail,
    FbColumnSortDetail,
    FbHeaderContextDetail,
} from './types.js';

export interface ColumnDescriptor {
    id: string;
    label?: string;
    /** Fallback header text used when {@link label} is not provided. */
    name?: string;
    /** CSS grid track value (e.g. `'1fr'` or `'120px'`). */
    width?: string;
    sortable?: boolean;
    /** Locked columns: cannot be reordered or resized. */
    fixed?: boolean;
}

interface DragState {
    active: boolean;
    colIdx: number;
    startX: number;
    startY: number;
    isDragging: boolean;
    dropTarget: number;
    dropPos: 'left' | 'right' | null;
}

interface ResizeState {
    active: boolean;
    colIdx: number;
    startX: number;
    startW: number;
}

export class FbResizableHeader extends FbBaseElement {
    private _cols: ColumnDescriptor[] = [];
    private _colWidths: number[] = [];
    private _sortCol: string | null = null;
    private _sortDir: 'asc' | 'desc' = 'asc';
    private _widthsResolved = false;
    private _drag: DragState = {
        active: false,
        colIdx: -1,
        startX: 0,
        startY: 0,
        isDragging: false,
        dropTarget: -1,
        dropPos: null,
    };
    private _rz: ResizeState = {
        active: false,
        colIdx: -1,
        startX: 0,
        startW: 0,
    };
    private _ro: ResizeObserver | null = null;

    private _headerEl!: HTMLDivElement;
    private _dropEl!: HTMLDivElement;
    private _ghostEl!: HTMLDivElement;

    static get observedAttributes(): string[] {
        return ['columns', 'sort-column', 'sort-direction'];
    }

    attributeChangedCallback(name: string): void {
        if (!this._domReady) return;
        if (name === 'columns') {
            this._parseColumns();
            this._rebuildCells();
        } else if (name === 'sort-column') {
            this._sortCol = this.getAttribute('sort-column');
            this._updateSortAttrs();
        } else if (name === 'sort-direction') {
            this._sortDir =
                (this.getAttribute('sort-direction') as 'asc' | 'desc') || 'asc';
            this._updateSortAttrs();
        }
    }

    /** Current `grid-template-columns` CSS value (fr or px). */
    get gridTemplate(): string {
        if (!this._widthsResolved || this._colWidths.length === 0) {
            return this._cols.map((c) => c.width || '1fr').join(' ');
        }
        return this._colWidths.map((w) => w + 'px').join(' ');
    }

    /** Column id list in current visual order. */
    get columnIds(): string[] {
        return this._cols.map((c) => c.id);
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        this._sortCol = this.getAttribute('sort-column');
        this._sortDir =
            (this.getAttribute('sort-direction') as 'asc' | 'desc') || 'asc';

        root.innerHTML =
            // Structure only — themes own:
            //   - `[part=sort-icon]` spacing (use theme-side gap / margin)
            //   - `[part=drop-indicator]` / `[part=drag-ghost]` transitions
            //     and colours (themes opt into opacity animations themselves)
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:block;user-select:none}` +
            `[part=header]{display:grid;align-items:center;position:relative}` +
            `[part~=cell]{display:flex;align-items:center;min-width:0;position:relative;cursor:default;box-sizing:border-box}` +
            `[part=resize-handle]{position:absolute;top:0;right:-4px;width:8px;height:100%;cursor:col-resize;z-index:5}` +
            `[part=sort-icon]{display:inline-flex;flex-shrink:0}` +
            `[part=drop-indicator]{position:absolute;top:0;bottom:0;width:3px;pointer-events:none;opacity:0;z-index:20}` +
            `[part=drag-ghost]{position:fixed;pointer-events:none;z-index:10000;white-space:nowrap;opacity:0}` +
            `</style>` +
            `<div part="header"></div>` +
            `<div part="drop-indicator"></div>` +
            `<div part="drag-ghost"></div>`;

        this._headerEl = this._$<HTMLDivElement>('[part=header]')!;
        this._dropEl = this._$<HTMLDivElement>('[part=drop-indicator]')!;
        this._ghostEl = this._$<HTMLDivElement>('[part=drag-ghost]')!;

        this._parseColumns();
        this._rebuildCells();
    }

    private _parseColumns(): void {
        try {
            const attr = this.getAttribute('columns');
            this._cols = attr ? (JSON.parse(attr) as ColumnDescriptor[]) : [];
        } catch {
            this._cols = [];
        }
        this._widthsResolved = false;
        this._colWidths = [];
    }

    private _rebuildCells(): void {
        let html = '';
        this._cols.forEach((col, i) => {
            const sorted = this._sortCol === col.id;
            const sortable = col.sortable !== false && !col.fixed;
            html +=
                `<div part="cell cell-${col.id}" data-index="${i}" data-col="${col.id}"` +
                (sorted
                    ? ` aria-sort="${this._sortDir === 'desc' ? 'descending' : 'ascending'}"`
                    : '') +
                `>` +
                `<span part="label">${this._escHtml(col.label || col.name || col.id)}</span>`;
            if (sortable) {
                html +=
                    `<span part="sort-icon${sorted ? ' sort-active' : ''}">` +
                    `<svg viewBox="0 0 16 16" width="12" height="12" fill="currentColor">` +
                    `<path d="M8 4l4 5H4z"/>` +
                    `<path d="M8 12l4-5H4z"/>` +
                    `</svg></span>`;
            }
            if (!col.fixed) {
                html += `<div part="resize-handle" data-index="${i}"></div>`;
            }
            html += `</div>`;
        });
        this._headerEl.innerHTML = html;
        // Initial grid template from column specs (fr/px).
        this._headerEl.style.gridTemplateColumns = this._cols
            .map((c) => c.width || '1fr')
            .join(' ');
    }

    protected override _setupEvents(): void {
        this._listen<MouseEvent>(this._headerEl, 'mousedown', (e) => {
            if (e.button !== 0) return;

            const handle = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=resize-handle]',
            );
            if (handle) {
                e.preventDefault();
                this._startResize(e, parseInt(handle.dataset.index || '-1', 10));
                return;
            }

            const cell = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part~=cell]',
            );
            if (cell) {
                const idx = parseInt(cell.dataset.index || '-1', 10);
                const col = this._cols[idx];
                if (!col || col.fixed) return;
                this._startDrag(e, idx);
            }
        });

        this._listen<MouseEvent>(this._headerEl, 'contextmenu', (e) => {
            e.preventDefault();
            this._emit<FbHeaderContextDetail>('fb-header-context', {
                x: e.clientX,
                y: e.clientY,
            });
        });

        // Invalidate pixel widths on container resize.
        this._ro = new ResizeObserver(() => {
            if (
                this._widthsResolved &&
                !this._rz.active &&
                !this._drag.isDragging
            ) {
                this._widthsResolved = false;
            }
        });
        this._ro.observe(this._headerEl);
    }

    protected override _subscribe(): void {
        // No fb backend events needed.
    }

    private _resolveWidths(): void {
        if (this._widthsResolved) return;
        const cells = [...this._$$<HTMLElement>('[part~=cell]')];
        if (cells.length === 0) return;
        this._colWidths = cells.map((c) => c.offsetWidth);
        this._widthsResolved = true;
    }

    private _applyWidths(): void {
        this._headerEl.style.gridTemplateColumns = this._colWidths
            .map((w) => w + 'px')
            .join(' ');
    }

    /**
     * `document`-level listeners are bound here (not via `_listen`) so
     * the drag tracks cursor motion outside the component's bounding
     * box. Cleanup happens in the paired `onUp` handler.
     */
    private _startResize(e: MouseEvent, idx: number): void {
        this._resolveWidths();
        this._rz.active = true;
        this._rz.colIdx = idx;
        this._rz.startX = e.clientX;
        this._rz.startW = this._colWidths[idx] ?? 0;
        this.setAttribute('resizing', '');
        document.body.style.cursor = 'col-resize';
        document.body.style.userSelect = 'none';

        const onMove = (ev: MouseEvent): void => {
            const delta = ev.clientX - this._rz.startX;
            const newW = Math.max(50, this._rz.startW + delta);
            this._colWidths[this._rz.colIdx] = newW;
            this._applyWidths();
            this._emit<FbColumnResizeDetail>('fb-column-resize', {
                column: this._cols[this._rz.colIdx]!.id,
                width: newW,
                gridTemplate: this.gridTemplate,
            });
        };
        const onUp = (): void => {
            this._rz.active = false;
            this.removeAttribute('resizing');
            document.body.style.cursor = '';
            document.body.style.userSelect = '';
            document.removeEventListener('mousemove', onMove);
            document.removeEventListener('mouseup', onUp);
        };
        document.addEventListener('mousemove', onMove);
        document.addEventListener('mouseup', onUp);
    }

    private _startDrag(e: MouseEvent, idx: number): void {
        const d = this._drag;
        d.active = true;
        d.colIdx = idx;
        d.startX = e.clientX;
        d.startY = e.clientY;
        d.isDragging = false;

        const onMove = (ev: MouseEvent): void => {
            const dx = Math.abs(ev.clientX - d.startX);
            const dy = Math.abs(ev.clientY - d.startY);

            // 5 px threshold: distinguish click from drag.
            if (!d.isDragging && (dx > 5 || dy > 5)) {
                d.isDragging = true;
                this.setAttribute('reordering', '');
                document.body.style.cursor = 'grabbing';
                document.body.style.userSelect = 'none';
                const col = this._cols[d.colIdx]!;
                this._ghostEl.textContent = col.label || col.name || col.id;
                this._ghostEl.style.opacity = '1';
            }
            if (!d.isDragging) return;

            this._ghostEl.style.left = ev.clientX + 'px';
            this._ghostEl.style.top = ev.clientY + 'px';

            const cells = this._$$<HTMLElement>('[part~=cell]');
            let found = false;
            for (let i = 0; i < cells.length; i++) {
                if (this._cols[i]?.fixed) continue;
                const rect = cells[i]!.getBoundingClientRect();
                if (ev.clientX >= rect.left && ev.clientX <= rect.right) {
                    const mid = rect.left + rect.width / 2;
                    d.dropTarget = i;
                    d.dropPos = ev.clientX < mid ? 'left' : 'right';
                    const x = d.dropPos === 'left' ? rect.left : rect.right;
                    const hRect = this._headerEl.getBoundingClientRect();
                    this._dropEl.style.left = x - hRect.left - 1 + 'px';
                    this._dropEl.style.opacity = '1';
                    found = true;
                    break;
                }
            }
            if (!found) {
                d.dropTarget = -1;
                this._dropEl.style.opacity = '0';
            }
        };

        const onUp = (): void => {
            document.removeEventListener('mousemove', onMove);
            document.removeEventListener('mouseup', onUp);
            document.body.style.cursor = '';
            document.body.style.userSelect = '';

            const wasDragging = d.isDragging;
            const fromIdx = d.colIdx;
            const dropTarget = d.dropTarget;
            const dropPos = d.dropPos;

            this._ghostEl.style.opacity = '0';
            this._dropEl.style.opacity = '0';
            this.removeAttribute('reordering');
            d.active = false;
            d.isDragging = false;

            if (wasDragging && dropTarget >= 0 && fromIdx !== dropTarget) {
                let toIdx = dropTarget;
                if (dropPos === 'right') toIdx++;
                if (fromIdx < toIdx) toIdx--;
                if (
                    toIdx !== fromIdx &&
                    toIdx >= 0 &&
                    toIdx < this._cols.length
                ) {
                    const col = this._cols.splice(fromIdx, 1)[0]!;
                    this._cols.splice(toIdx, 0, col);
                    if (this._widthsResolved) {
                        const w = this._colWidths.splice(fromIdx, 1)[0]!;
                        this._colWidths.splice(toIdx, 0, w);
                    }
                    this._rebuildCells();
                    if (this._widthsResolved) this._applyWidths();
                    this._emit<FbColumnReorderDetail>('fb-column-reorder', {
                        fromIndex: fromIdx,
                        toIndex: toIdx,
                        columns: this._cols.map((c) => c.id),
                    });
                }
            } else if (!wasDragging) {
                // Click — toggle sort.
                const col = this._cols[fromIdx];
                if (col && col.sortable !== false && !col.fixed) {
                    if (this._sortCol === col.id) {
                        this._sortDir =
                            this._sortDir === 'asc' ? 'desc' : 'asc';
                    } else {
                        this._sortCol = col.id;
                        this._sortDir = 'asc';
                    }
                    this.setAttribute('sort-column', this._sortCol);
                    this.setAttribute('sort-direction', this._sortDir);
                    this._updateSortAttrs();
                    this._emit<FbColumnSortDetail>('fb-column-sort', {
                        column: this._sortCol,
                        direction: this._sortDir,
                    });
                }
            }
        };

        document.addEventListener('mousemove', onMove);
        document.addEventListener('mouseup', onUp);
    }

    private _updateSortAttrs(): void {
        this._$$<HTMLElement>('[part~=cell]').forEach((cell) => {
            const colId = cell.dataset.col;
            if (colId === this._sortCol) {
                cell.setAttribute(
                    'aria-sort',
                    this._sortDir === 'desc' ? 'descending' : 'ascending',
                );
                const icon = cell.querySelector('[part~=sort-icon]');
                if (icon)
                    icon.setAttribute('part', 'sort-icon sort-active');
            } else {
                cell.removeAttribute('aria-sort');
                const icon = cell.querySelector('[part~=sort-icon]');
                if (icon) icon.setAttribute('part', 'sort-icon');
            }
        });
    }

    override disconnectedCallback(): void {
        if (this._ro) {
            this._ro.disconnect();
            this._ro = null;
        }
        super.disconnectedCallback();
    }
}
