/**
 * `<fb-stop-after-current>` — toggles the host's "stop after current
 * track" flag.
 *
 * Reflects the boolean state via the host attribute `active` and the
 * `aria-pressed` ARIA attribute.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbStopAfterCurrentToggleDetail } from './types.js';

export class FbStopAfterCurrent extends FbBaseElement {
    private _btn!: HTMLButtonElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` +
            `<button part="button" role="button" tabindex="0"` +
            ` aria-label="Stop after current" aria-pressed="false">` +
            `<slot>\u23CF</slot>` +
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
            const current = await fb.player.getStopAfterCurrent();
            const newVal = !current.enabled;
            await fb.player.setStopAfterCurrent(newVal);
            this._emit<FbStopAfterCurrentToggleDetail>(
                'fb-stop-after-current-toggle',
                { active: newVal },
            );
        } catch {
            /* R6: silent degradation */
        }
    }

    protected override _subscribe(): void {
        this._sub('playback:stopAfterCurrentChanged', (data) => {
            this._update(!!data?.enabled);
        });
        getFb()
            .player.getStopAfterCurrent()
            .then((r) => this._update(!!r.enabled))
            .catch(() => {
                /* silent */
            });
    }

    private _update(active: boolean): void {
        if (active) this.setAttribute('active', '');
        else this.removeAttribute('active');
        this._btn.setAttribute('aria-pressed', (!!active).toString());
    }
}
