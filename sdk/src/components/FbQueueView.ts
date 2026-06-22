/**
 * `<fb-queue-view>` — playback-queue list.
 *
 * Attribute:
 * - `columns`: comma-separated list of column ids
 *   (default `'index,title,artist,duration'`).
 *
 * Reflects host attributes `count` and `empty` (set when the queue is
 * empty so themes can `:host([empty]) [part=empty] { display: block }`).
 *
 * Emits:
 * - `fb-queue-context` on right-click (theme owns the menu).
 * - `fb-queue-remove` on row double-click after the host removal succeeded.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { formatTime } from './constants.js';
import { getFb } from './runtime.js';
import type {
    FbQueueContextDetail,
    FbQueueRemoveDetail,
} from './types.js';
import type { JsonValue } from '../types/json.js';

interface QueueRowItem {
    duration?: number;
    [key: string]: JsonValue;
}

export class FbQueueView extends FbBaseElement {
    private _container!: HTMLDivElement;
    private _columns: string[] = ['index', 'title', 'artist', 'duration'];
    private _items: QueueRowItem[] = [];

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        const cols = (
            this.getAttribute('columns') || 'index,title,artist,duration'
        )
            .split(',')
            .map((s) => s.trim());
        this._columns = cols;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:block;overflow-y:auto}` +
            `[part=container]{display:flex;flex-direction:column}` +
            `[part=row]{display:flex;align-items:center;cursor:default}` +
            `[part=empty]{display:none}` +
            `:host([empty]) [part=empty]{display:block}` +
            `:host([empty]) [part=container]{display:none}` +
            `</style>` +
            `<div part="container"></div>` +
            `<slot name="empty" part="empty"></slot>`;
        this._container = this._$<HTMLDivElement>('[part=container]')!;
    }

    protected override _setupEvents(): void {
        this._listen<MouseEvent>(this._container, 'contextmenu', (e) => {
            const row = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=row]',
            );
            if (!row) return;
            e.preventDefault();
            const idx = parseInt(row.dataset.index || '-1', 10);
            this._emit<FbQueueContextDetail>('fb-queue-context', {
                index: idx,
                x: e.clientX,
                y: e.clientY,
            });
        });

        this._listen<MouseEvent>(this._container, 'dblclick', (e) => {
            const row = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=row]',
            );
            if (!row) return;
            const idx = parseInt(row.dataset.index || '-1', 10);
            void this._removeItem(idx);
        });
    }

    private async _removeItem(index: number): Promise<void> {
        try {
            await getFb().queue.remove(index);
            this._emit<FbQueueRemoveDetail>('fb-queue-remove', { index });
        } catch {
            /* fb-queue-view removeItem error — silent */
        }
    }

    protected override _subscribe(): void {
        this._sub('playback:queueChanged', () => {
            void this._loadQueue();
        });
        void this._loadQueue();
    }

    private async _loadQueue(): Promise<void> {
        try {
            const result = (await getFb().queue.get()) as unknown as
                | { items?: QueueRowItem[] }
                | null;
            this._items = result?.items ?? [];
        } catch {
            this._items = [];
        }
        this._rebuildRows();
    }

    private _rebuildRows(): void {
        if (this._items.length === 0) {
            this.setAttribute('empty', '');
            this._container.innerHTML = '';
            this.setAttribute('count', '0');
            return;
        }
        this.removeAttribute('empty');
        this._container.innerHTML = this._items
            .map(
                (item, i) =>
                    `<div part="row" data-index="${i}">` +
                    this._columns
                        .map((col) => {
                            let val = '';
                            if (col === 'index') val = (i + 1).toString();
                            else if (col === 'duration')
                                val = formatTime(item.duration || 0);
                            else
                                val =
                                    item[col] != null ? String(item[col]) : '';
                            return `<span part="row-${col}">${this._escHtml(val)}</span>`;
                        })
                        .join('') +
                    `</div>`,
            )
            .join('');
        this.setAttribute('count', this._items.length.toString());
    }
}
