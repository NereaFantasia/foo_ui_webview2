/**
 * `<fb-playlist-selector>` — `<select>`-based playlist picker.
 *
 * Lists every playlist returned by `fb.playlist.getAll()` with its
 * track count. Subscribes to `playlist:created` / `:removed` /
 * `:renamed` / `:reordered` and refreshes its option list on each event.
 * `fb-playlist-pick` carries the picked playlist's GUID next to its
 * index; the GUID keeps naming that playlist after the list changes.
 *
 * Reflects host attributes `selected-index` and `selected-name`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbPlaylistPickDetail } from './types.js';

interface PlaylistRow {
    guid: string;
    name: string;
    isActive: boolean;
    trackCount: number;
}

export class FbPlaylistSelector extends FbBaseElement {
    private _select!: HTMLSelectElement;
    private _playlists: PlaylistRow[] = [];
    /** Incremented by every read of the playlists, so an answer that arrives after a newer read is dropped. */
    private _loadQuery = 0;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}select{all:unset}</style>` +
            `<select part="select" aria-label="Select playlist"></select>`;
        this._select = this._$<HTMLSelectElement>('select')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._select, 'change', () => {
            const index = parseInt(this._select.value, 10);
            const picked = this._playlists[index];
            const name = picked?.name ?? '';
            this.setAttribute('selected-index', index.toString());
            this.setAttribute('selected-name', name);
            this._emit<FbPlaylistPickDetail>('fb-playlist-pick', {
                index,
                guid: picked?.guid ?? '',
                name,
            });
        });
    }

    protected override _subscribe(): void {
        const refresh = (): void => {
            void this._loadPlaylists();
        };
        this._sub('playlist:created', refresh);
        this._sub('playlist:removed', refresh);
        this._sub('playlist:renamed', refresh);
        // Option values are indices, so a reorder has to redraw them.
        this._sub('playlist:reordered', refresh);
        void this._loadPlaylists();
    }

    private async _loadPlaylists(): Promise<void> {
        const query = ++this._loadQuery;
        try {
            const result = await getFb().playlist.getAll();
            // A failed call keeps the list shown so far; an older answer yields to a newer one.
            if (result.success === false || query !== this._loadQuery) return;
            this._playlists = result.playlists;
            this._select.innerHTML = this._playlists
                .map(
                    (pl, i) =>
                        `<option value="${i}"${pl.isActive ? ' selected' : ''}>${this._escHtml(pl.name)} (${pl.trackCount})</option>`,
                )
                .join('');
            const active = this._playlists.findIndex((p) => p.isActive);
            if (active >= 0) {
                this.setAttribute('selected-index', active.toString());
                this.setAttribute(
                    'selected-name',
                    this._playlists[active]!.name,
                );
            }
        } catch {
            /* A rejected playlist read leaves the displayed list unchanged. */
        }
    }
}
