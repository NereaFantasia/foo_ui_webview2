/**
 * `<fb-search-bar>` — debounced media-library search input.
 *
 * Emits `fb-search` (with the trimmed query) before the request and
 * `fb-search-result` (with `tracks`, `count`, `query`) once
 * `fb.library.search` resolves. On error we still emit `fb-search-result`
 * with `tracks: []` so theme code can clear the UI.
 *
 * Attributes:
 * - `placeholder` — input placeholder text (default `'Search library...'`).
 * - `min-length`  — fire when query length ≥ value (default 2).
 * - `debounce`    — keystroke debounce ms (default 300).
 *
 * Slots:
 * - `icon` — optional icon glyph next to the input (default 🔍).
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbSearchDetail, FbSearchResultDetail } from './types.js';

export class FbSearchBar extends FbBaseElement {
    private _input!: HTMLInputElement;
    private _debounceTimer: ReturnType<typeof setTimeout> | null = null;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` +
            `<div part="container">` +
            `<input part="input" type="text" />` +
            `<slot name="icon">🔍</slot>` +
            `</div>`;
        this._input = this._$<HTMLInputElement>('input')!;
    }

    protected override _setupEvents(): void {
        const placeholder =
            this.getAttribute('placeholder') || 'Search library...';
        this._input.placeholder = placeholder;

        this._listen(this._input, 'input', () => {
            if (this._debounceTimer) clearTimeout(this._debounceTimer);
            const query = this._input.value.trim();
            const minLen =
                parseInt(this.getAttribute('min-length') || '', 10) || 2;
            if (query.length < minLen) return;
            const debounce =
                parseInt(this.getAttribute('debounce') || '', 10) || 300;
            this._debounceTimer = setTimeout(() => {
                void this._search(query);
            }, debounce);
        });

        this._listen(this._input, 'keydown', (e: Event) => {
            const ke = e as KeyboardEvent;
            if (ke.key === 'Enter') {
                if (this._debounceTimer) clearTimeout(this._debounceTimer);
                void this._search(this._input.value.trim());
            }
        });
    }

    protected override _subscribe(): void {
        /* no host-side subscriptions */
    }

    override disconnectedCallback(): void {
        if (this._debounceTimer) clearTimeout(this._debounceTimer);
        this._debounceTimer = null;
        super.disconnectedCallback();
    }

    private async _search(query: string): Promise<void> {
        if (!query) return;
        this._emit<FbSearchDetail>('fb-search', { query });
        try {
            const result = (await getFb().library.search(query, 100)) as
                | { items?: unknown[]; tracks?: unknown[] }
                | null;
            const tracks = result?.items ?? result?.tracks ?? [];
            this._emit<FbSearchResultDetail>('fb-search-result', {
                tracks,
                count: tracks.length,
                query,
            });
        } catch {
            this._emit<FbSearchResultDetail>('fb-search-result', {
                tracks: [],
                count: 0,
                query,
            });
        }
    }
}
