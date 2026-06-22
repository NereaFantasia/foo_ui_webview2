/**
 * `<fb-rating>` — interactive N-star track rating widget.
 *
 * Attributes (observed):
 * - `max`: maximum star count, default `5`. Triggers a Shadow DOM
 *   rebuild when changed (number of `[part=star]` nodes depends on
 *   `max`).
 * - `readonly`: when present, disables click / keyboard / hover
 *   preview interactions.
 *
 * Clicking the currently selected rating clears it back to zero,
 * matching the familiar foobar2000 "click selected star to clear"
 * gesture.
 *
 * Theme hooks (zero visual decisions are made by the component):
 * - `[part=star]`                — every star cell.
 * - `[part=star][data-filled]`   — filled (selected) stars; preview
 *   state and committed state both set this dataset flag. Themes
 *   typically render these in a highlight colour (gold by
 *   convention).
 * - `[part=star][aria-checked=true]` — mirrors the committed rating
 *   for accessibility; themes may use it for persistent styling.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbRatingChangeDetail } from './types.js';

export class FbRating extends FbBaseElement {
    private _stars!: HTMLDivElement;
    private _currentPath = '';
    private _currentRating = 0;

    static get observedAttributes(): string[] {
        return ['max', 'readonly'];
    }

    attributeChangedCallback(name: string): void {
        if (!this._domReady) return;
        if (name === 'max') {
            this._abortController.abort();
            this._abortController = new AbortController();
            this._buildDOM();
            this._setupEvents();
            this._updateStars();
        }
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        const max = parseInt(this.getAttribute('max') || '5', 10) || 5;
        // Structure only — hover transitions, scale feedback, and the
        // fill colour are theme decisions. Themes hook into
        // `[part=star]` (+ `[data-filled]` / `[aria-checked]`).
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:inline-flex}` +
            `[part=star]{cursor:pointer;display:inline-block}` +
            `:host([readonly]) [part=star]{cursor:default}` +
            `</style>` +
            `<div part="stars">` +
            Array.from(
                { length: max },
                (_, i) =>
                    `<span part="star" data-value="${i + 1}" role="radio" tabindex="0"` +
                    ` aria-label="${i + 1} star" aria-checked="false">\u2606</span>`,
            ).join('') +
            `</div>`;
        this._stars = this._$<HTMLDivElement>('[part=stars]')!;
    }

    protected override _setupEvents(): void {
        this._listen<MouseEvent>(this._stars, 'click', (e) => {
            if (this.hasAttribute('readonly')) return;
            const star = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=star]',
            );
            if (!star) return;
            const value = parseInt(star.dataset.value || '0', 10);
            void this._setRating(value);
        });

        this._listen<KeyboardEvent>(this._stars, 'keydown', (e) => {
            if (this.hasAttribute('readonly')) return;
            if (e.key !== 'Enter' && e.key !== ' ') return;
            const star = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=star]',
            );
            if (!star) return;
            e.preventDefault();
            void this._setRating(parseInt(star.dataset.value || '0', 10));
        });

        this._listen<MouseEvent>(this._stars, 'mouseover', (e) => {
            if (this.hasAttribute('readonly')) return;
            const star = (e.target as HTMLElement | null)?.closest<HTMLElement>(
                '[part=star]',
            );
            if (!star) return;
            const hoverVal = parseInt(star.dataset.value || '0', 10);
            this._$$<HTMLElement>('[part=star]').forEach((s) => {
                const v = parseInt(s.dataset.value || '0', 10);
                s.textContent = v <= hoverVal ? '\u2605' : '\u2606';
                // Theme hook: `[part=star][data-filled]` receives the
                // preview / committed colour. No inline color here.
                if (v <= hoverVal) s.dataset.filled = '';
                else delete s.dataset.filled;
            });
        });

        this._listen(this._stars, 'mouseleave', () => {
            if (this.hasAttribute('readonly')) return;
            this._updateStars();
        });
    }

    private async _setRating(value: number): Promise<void> {
        // Click-on-active-star → clear (rating === 0).
        let next = value;
        if (next === this._currentRating) next = 0;
        try {
            await getFb().rating.set(this._currentPath, next);
            this._currentRating = next;
            this._updateStars();
            this._emit<FbRatingChangeDetail>('fb-rating-change', {
                value: next,
                path: this._currentPath,
            });
        } catch {
            /* fb-rating set failed — silent */
        }
    }

    protected override _subscribe(): void {
        this._sub('playback:trackChanged', (data) => {
            const d = data as { path?: string; absolutePath?: string } | null;
            this._currentPath = d?.path || d?.absolutePath || '';
            void this._loadRating();
        });

        getFb()
            .player.getCurrentTrack()
            .then((t) => {
                const tt = t as
                    | { path?: string; absolutePath?: string }
                    | null;
                this._currentPath = tt?.path || tt?.absolutePath || '';
                void this._loadRating();
            })
            .catch(() => {
                /* silent */
            });
    }

    private async _loadRating(): Promise<void> {
        if (!this._currentPath) {
            this._currentRating = 0;
            this._updateStars();
            return;
        }
        try {
            const result = (await getFb().rating.get(this._currentPath)) as
                | { rating?: number }
                | null;
            this._currentRating = result?.rating || 0;
        } catch {
            this._currentRating = 0;
        }
        this._updateStars();
    }

    private _updateStars(): void {
        this.setAttribute('value', this._currentRating.toString());
        const stars = this._$$<HTMLElement>('[part=star]');
        stars.forEach((star) => {
            const val = parseInt(star.dataset.value || '0', 10);
            const filled = val <= this._currentRating;
            star.textContent = filled ? '\u2605' : '\u2606';
            // Theme hook: themes pick up `[part=star][data-filled]` for
            // the fill colour. `aria-checked` is kept in sync for a11y.
            if (filled) star.dataset.filled = '';
            else delete star.dataset.filled;
            star.setAttribute('aria-checked', filled.toString());
        });
    }
}
