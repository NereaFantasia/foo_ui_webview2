/**
 * `<fb-shuffle-button>` — toggles between sequential and shuffle
 * playback orders.
 *
 * Click semantics: if the host is currently in any shuffle order
 * (3-6, see {@link SHUFFLE_ORDERS}) → switch to `default` (0); else
 * switch to `shuffle-tracks` (4). Two-state toggle.
 *
 * Reflects the host attribute `active` and `aria-pressed` for ARIA.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { SHUFFLE_ORDERS } from './constants.js';
import { getFb } from './runtime.js';
import type { FbShuffleToggleDetail } from './types.js';

export class FbShuffleButton extends FbBaseElement {
    private _btn!: HTMLButtonElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` +
            `<button part="button" role="button" tabindex="0" aria-label="Shuffle" aria-pressed="false">` +
            `<slot>\uD83D\uDD00</slot>` +
            `</button>`;
        this._btn = this._$<HTMLButtonElement>('button')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._btn, 'click', () => {
            void this._handleClick();
        });
        this._listen<KeyboardEvent>(this._btn, 'keydown', (e) => {
            if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                void this._handleClick();
            }
        });
    }

    private async _handleClick(): Promise<void> {
        try {
            const fb = getFb();
            const current = await fb.player.getOrder();
            const newOrder = SHUFFLE_ORDERS.has(current.order) ? 0 : 4;
            await fb.player.setOrder(newOrder);
            this._emit<FbShuffleToggleDetail>('fb-shuffle-toggle', {
                active: newOrder !== 0,
                order: newOrder,
            });
        } catch {
            /* R6: silent degradation */
        }
    }

    protected override _subscribe(): void {
        this._sub('playback:orderChanged', (data) => {
            this._update(data?.orderIndex ?? 0);
        });
        getFb()
            .player.getOrder()
            .then((r) => this._update(r.order))
            .catch(() => {
                /* silent */
            });
    }

    private _update(order: number): void {
        const active = SHUFFLE_ORDERS.has(order);
        if (active) this.setAttribute('active', '');
        else this.removeAttribute('active');
        this._btn.setAttribute('aria-pressed', active.toString());
    }
}
