/**
 * `<fb-repeat-button>` — cycles through the three repeat modes
 * `off → playlist → track → off`.
 *
 * Shows one of three named slots based on the resolved mode:
 * `off`, `playlist`, `track`. Reflects the resolved label via the
 * `mode` host attribute (`'off' | 'playlist' | 'track'`).
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbRepeatChangeDetail } from './types.js';

type RepeatMode = 'off' | 'playlist' | 'track';

export class FbRepeatButton extends FbBaseElement {
    private _btn!: HTMLButtonElement;
    private _offSlot!: HTMLSlotElement;
    private _plSlot!: HTMLSlotElement;
    private _trackSlot!: HTMLSlotElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` +
            `<button part="button" role="button" tabindex="0" aria-label="Repeat">` +
            `<slot name="off">\uD83D\uDD01</slot>` +
            `<slot name="playlist" hidden>\uD83D\uDD01</slot>` +
            `<slot name="track" hidden>\uD83D\uDD02</slot>` +
            `</button>`;
        this._btn = this._$<HTMLButtonElement>('button')!;
        this._offSlot = this._$<HTMLSlotElement>('slot[name=off]')!;
        this._plSlot = this._$<HTMLSlotElement>('slot[name=playlist]')!;
        this._trackSlot = this._$<HTMLSlotElement>('slot[name=track]')!;
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
            let newOrder: number;
            if (current.order === 1) newOrder = 2; // playlist → track
            else if (current.order === 2) newOrder = 0; // track → off
            else newOrder = 1; // off / shuffle / random → playlist
            await fb.player.setOrder(newOrder);
            this._emit<FbRepeatChangeDetail>('fb-repeat-change', {
                mode: this._orderToMode(newOrder),
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
        const mode = this._orderToMode(order);
        this.setAttribute('mode', mode);
        this._offSlot.hidden = mode !== 'off';
        this._plSlot.hidden = mode !== 'playlist';
        this._trackSlot.hidden = mode !== 'track';
    }

    private _orderToMode(order: number): RepeatMode {
        if (order === 1) return 'playlist';
        if (order === 2) return 'track';
        return 'off';
    }
}
