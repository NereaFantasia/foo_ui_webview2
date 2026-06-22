/**
 * `<fb-prev-button>` — jumps to the previous track.
 *
 * Visual contract:
 * - One default slot for the icon (defaults to U+23EE `⏮`).
 *
 * Emits `fb-prev` after the host call resolves.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbPrevDetail } from './types.js';

export class FbPrevButton extends FbBaseElement {
    private _btn!: HTMLButtonElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` +
            `<button part="button" role="button" tabindex="0" aria-label="Previous">` +
            `<slot>\u23EE</slot>` +
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
            await getFb().player.prev();
            this._emit<FbPrevDetail>('fb-prev', {});
        } catch {
            /* R6: silent degradation when SDK unavailable */
        }
    }
}
