/**
 * `<fb-playback-order>` — full 7-state playback-order picker.
 *
 * Two display modes (selected via the `mode` attribute):
 * - `'select'` (default) → `<select>` with all
 *   {@link ORDER_NAMES} options.
 * - `'button'` → single `<button>` whose label cycles through the
 *   order names on click.
 *
 * Mode is observed: switching `mode` rebuilds the Shadow DOM and
 * re-binds events. The previous DOM-listener AbortController is
 * aborted before rebuild so old listeners never leak.
 *
 * Reflects the resolved order via the `order` host attribute
 * (string label, e.g. `'shuffle-tracks'`).
 */

import { FbBaseElement } from './FbBaseElement.js';
import { ORDER_NAMES } from './constants.js';
import { getFb } from './runtime.js';
import type { FbOrderChangeDetail } from './types.js';

export class FbPlaybackOrder extends FbBaseElement {
    private _select: HTMLSelectElement | null = null;
    private _btn: HTMLButtonElement | null = null;
    private _slot: HTMLSlotElement | null = null;

    static get observedAttributes(): string[] {
        return ['mode'];
    }

    attributeChangedCallback(): void {
        if (!this._domReady) return;
        // Tear down DOM listeners from the previous mode and rebuild.
        this._abortController.abort();
        this._abortController = new AbortController();
        this._buildDOM();
        this._setupEvents();
        getFb()
            .player.getOrder()
            .then((r) => this._update(r.order))
            .catch(() => {
                /* silent */
            });
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        const useSelect = this.getAttribute('mode') === 'select';
        if (useSelect) {
            root.innerHTML =
                `<style>${FbBaseElement.baseCSS}select{all:unset}</style>` +
                `<select part="select" aria-label="Playback order">` +
                ORDER_NAMES.map(
                    (name, i) => `<option value="${i}">${name}</option>`,
                ).join('') +
                `</select>`;
            this._select = this._$<HTMLSelectElement>('select');
            this._btn = null;
            this._slot = null;
        } else {
            root.innerHTML =
                `<style>${FbBaseElement.baseCSS}</style>` +
                `<button part="button" role="button" tabindex="0" aria-label="Playback order">` +
                `<slot>default</slot>` +
                `</button>`;
            this._btn = this._$<HTMLButtonElement>('button');
            this._slot = this._$<HTMLSlotElement>('slot');
            this._select = null;
        }
    }

    protected override _setupEvents(): void {
        if (this._select) {
            this._listen(this._select, 'change', () => {
                void this._selectOrder();
            });
        } else if (this._btn) {
            this._listen(this._btn, 'click', () => {
                void this._cycleOrder();
            });
            this._listen<KeyboardEvent>(this._btn, 'keydown', (e) => {
                if (e.key === 'Enter' || e.key === ' ') {
                    e.preventDefault();
                    void this._cycleOrder();
                }
            });
        }
    }

    private async _selectOrder(): Promise<void> {
        if (!this._select) return;
        try {
            const order = parseInt(this._select.value, 10);
            await getFb().player.setOrder(order);
            this._emit<FbOrderChangeDetail>('fb-order-change', {
                order,
                name: ORDER_NAMES[order] ?? 'unknown',
            });
        } catch {
            /* R6: silent degradation */
        }
    }

    private async _cycleOrder(): Promise<void> {
        try {
            const fb = getFb();
            const current = await fb.player.getOrder();
            const newOrder = (current.order + 1) % ORDER_NAMES.length;
            await fb.player.setOrder(newOrder);
            this._emit<FbOrderChangeDetail>('fb-order-change', {
                order: newOrder,
                name: ORDER_NAMES[newOrder] ?? 'unknown',
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
        const name = ORDER_NAMES[order] ?? 'unknown';
        this.setAttribute('order', name);
        if (this._select) {
            this._select.value = order.toString();
        } else if (this._slot) {
            this._slot.textContent = name;
        }
    }
}
