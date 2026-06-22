/**
 * `<fb-next-button>` — jumps to the next track.
 *
 * Visual contract:
 * - One default slot for the icon (defaults to U+23ED `⏭`).
 *
 * Emits `fb-next` after the host call resolves.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbNextDetail } from './types.js';

export class FbNextButton extends FbBaseElement {
    private _btn!: HTMLButtonElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` +
            `<button part="button" role="button" tabindex="0" aria-label="Next">` +
            `<slot>\u23ED</slot>` +
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
            await getFb().player.next();
            this._emit<FbNextDetail>('fb-next', {});
        } catch {
            /* R6: silent degradation when SDK unavailable */
        }
    }
}
