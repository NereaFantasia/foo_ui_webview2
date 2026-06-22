/**
 * `<fb-playlist-selector>` — `<select>`-based playlist picker.
 *
 * Lists every playlist returned by `fb.playlist.getAll()` with its
 * track count. Subscribes to `playlist:created` / `:removed` /
 * `:renamed` and refreshes its option list on each event.
 *
 * Reflects host attributes `selected-index` and `selected-name`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbPlaylistPickDetail } from './types.js';

interface PlaylistRow {
    name: string;
    isActive: boolean;
    trackCount: number;
}

export class FbPlaylistSelector extends FbBaseElement {
    private _select!: HTMLSelectElement;
    private _playlists: PlaylistRow[] = [];

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
            const name = this._playlists[index]?.name ?? '';
            this.setAttribute('selected-index', index.toString());
            this.setAttribute('selected-name', name);
            this._emit<FbPlaylistPickDetail>('fb-playlist-pick', {
                index,
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
        void this._loadPlaylists();
    }

    private async _loadPlaylists(): Promise<void> {
        try {
            const result = await getFb().playlist.getAll();
            this._playlists = (result as PlaylistRow[] | null) ?? [];
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
            /* R6: silent degradation */
        }
    }
}
