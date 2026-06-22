/**
 * `<fb-play-button>` — toggles between play and pause.
 *
 * Visual contract:
 * - Two slots, `play-icon` and `pause-icon`. The host shows whichever
 *   one matches the current playback state; the other is `hidden`.
 * - The host attribute `playing` is set/unset to mirror state, so
 *   themes can style via `fb-play-button[playing] { ... }`.
 *
 * Emits `fb-play` when entering the playing state, `fb-pause`
 * otherwise. Both events bubble through the Shadow DOM boundary.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbPauseDetail, FbPlayDetail } from './types.js';

export class FbPlayButton extends FbBaseElement {
    private _btn!: HTMLButtonElement;
    private _playSlot!: HTMLSlotElement;
    private _pauseSlot!: HTMLSlotElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}</style>` +
            `<button part="button" role="button" tabindex="0" aria-label="Play">` +
            `<slot name="play-icon">\u25B6</slot>` +
            `<slot name="pause-icon" hidden>\u23F8</slot>` +
            `</button>`;
        this._btn = this._$<HTMLButtonElement>('button')!;
        this._playSlot = this._$<HTMLSlotElement>('slot[name=play-icon]')!;
        this._pauseSlot = this._$<HTMLSlotElement>('slot[name=pause-icon]')!;
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

    protected override _subscribe(): void {
        this._sub('playback:stateChanged', (data) => {
            this._update(data?.state === 'playing');
        });
        // Prime initial state — silently ignore failures (R6 degrade).
        getFb()
            .player.getState()
            .then((s) => {
                this._update((s as { state?: string })?.state === 'playing');
            })
            .catch(() => {
                /* SDK unavailable — leave button in default state */
            });
    }

    private async _handleClick(): Promise<void> {
        try {
            await getFb().player.toggle();
        } catch {
            /* R6: silent degradation when SDK unavailable */
        }
    }

    private _update(isPlaying: boolean): void {
        this._playSlot.hidden = isPlaying;
        this._pauseSlot.hidden = !isPlaying;
        this._btn.setAttribute('aria-label', isPlaying ? 'Pause' : 'Play');
        if (isPlaying) {
            this.setAttribute('playing', '');
            this._emit<FbPlayDetail>('fb-play', {});
        } else {
            this.removeAttribute('playing');
            this._emit<FbPauseDetail>('fb-pause', {});
        }
    }
}
