/**
 * `<fb-volume-control>` — volume slider with mute button.
 *
 * Visual contract:
 * - Four CSS parts: `mute-button`, `track`, `fill`, `thumb`.
 * - One named slot: `icon` (defaults to U+1F50A 🔊).
 * - Two observed attributes: `vertical` (column-reverse layout),
 *   `no-icon` (hide the mute button).
 * - Reflects host attributes: `volume` (0-100) and `muted` (boolean).
 *
 * Interaction:
 * - Click anywhere on the track → `fb.player.setVolume(level)`.
 * - Mouse-down + drag → continuous visual updates, host call on
 *   mouse-up.
 * - Wheel → ±5 step (passive: false to allow `preventDefault`).
 * - Arrow / Home / End keys → ±5 / 0 / 100 (R5 keyboard).
 * - Mute button click / Enter / Space → `fb.player.toggleMute()`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type {
    FbMuteToggleDetail,
    FbVolumeChangeDetail,
} from './types.js';

export class FbVolumeControl extends FbBaseElement {
    private _volume = 100;
    private _muted = false;
    private _isDragging = false;
    private _dragController: AbortController | null = null;

    private _muteBtn!: HTMLButtonElement;
    private _trackEl!: HTMLDivElement;
    private _fillEl!: HTMLDivElement;
    private _thumbEl!: HTMLDivElement;

    static get observedAttributes(): string[] {
        return ['vertical', 'no-icon'];
    }

    /**
     * Re-paint the slider whenever an observed attribute changes.
     *
     * `vertical` flips the layout from row to column-reverse via CSS,
     * which means the fill / thumb elements must swap which axis they
     * write into (`width` ↔ `height`, `left` ↔ `bottom`).
     * `_updateVisual()` already clears the inactive axis, so a single
     * call here is enough to recover from the stale style values left
     * over from the previous orientation.
     *
     * `no-icon` is purely a CSS toggle (`:host([no-icon]) [part=mute-button]`)
     * and does not need a JS update, but invoking `_updateVisual()` is
     * harmless and keeps the callback shape simple.
     */
    attributeChangedCallback(): void {
        if (!this._domReady) return;
        this._updateVisual();
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:inline-flex;align-items:center}` +
            `:host([vertical]){flex-direction:column-reverse}` +
            `:host([no-icon]) [part=mute-button]{display:none}` +
            // `padding:6px 0` widens the click target for the thin
            // volume track — hit-target only, not a visual decision
            // (see AGENTS.md §2.6.1 "Hit-target padding" whitelist).
            `[part=track]{position:relative;cursor:pointer;padding:6px 0}` +
            `[part=fill]{position:absolute;pointer-events:none}` +
            `:host(:not([vertical])) [part=fill]{top:50%;left:0;transform:translateY(-50%)}` +
            `:host([vertical]) [part=fill]{bottom:0;left:0;width:100%}` +
            `[part=thumb]{position:absolute;pointer-events:none}` +
            `</style>` +
            `<button part="mute-button" role="button" tabindex="0" aria-label="Mute">` +
            `<slot name="icon">\uD83D\uDD0A</slot>` +
            `</button>` +
            `<div part="track" role="slider" tabindex="0"` +
            ` aria-label="Volume" aria-valuemin="0" aria-valuemax="100" aria-valuenow="100">` +
            `<div part="fill"></div>` +
            `<div part="thumb"></div>` +
            `</div>`;
        this._muteBtn = this._$<HTMLButtonElement>('[part=mute-button]')!;
        this._trackEl = this._$<HTMLDivElement>('[part=track]')!;
        this._fillEl = this._$<HTMLDivElement>('[part=fill]')!;
        this._thumbEl = this._$<HTMLDivElement>('[part=thumb]')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._muteBtn, 'click', () => {
            void this._toggleMute();
        });
        this._listen<KeyboardEvent>(this._muteBtn, 'keydown', (e) => {
            if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                void this._toggleMute();
            }
        });

        this._listen<MouseEvent>(this._trackEl, 'click', (e) => {
            if (this._isDragging) return;
            void this._setVolumeFromEvent(e);
        });

        this._listen<MouseEvent>(this._trackEl, 'mousedown', (e) => {
            e.preventDefault();
            this._isDragging = true;
            this._dragController = new AbortController();
            const signal = this._dragController.signal;

            document.addEventListener(
                'mousemove',
                (ev) => {
                    this._volume = this._calcVolume(ev as MouseEvent);
                    this._updateVisual();
                },
                { signal },
            );

            document.addEventListener(
                'mouseup',
                (ev) => {
                    this._isDragging = false;
                    if (this._dragController) {
                        this._dragController.abort();
                        this._dragController = null;
                    }
                    void this._setVolumeFromEvent(ev as MouseEvent);
                },
                { signal },
            );
        });

        this._listen<WheelEvent>(
            this._trackEl,
            'wheel',
            (e) => {
                e.preventDefault();
                const delta = e.deltaY > 0 ? -5 : 5;
                this._volume = Math.max(0, Math.min(100, this._volume + delta));
                void this._applyVolume();
            },
            { passive: false },
        );

        this._listen<KeyboardEvent>(this._trackEl, 'keydown', (e) => {
            let delta = 0;
            if (e.key === 'ArrowRight' || e.key === 'ArrowUp') delta = 5;
            else if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') delta = -5;
            else if (e.key === 'Home') {
                e.preventDefault();
                this._volume = 0;
                void this._applyVolume();
                return;
            } else if (e.key === 'End') {
                e.preventDefault();
                this._volume = 100;
                void this._applyVolume();
                return;
            } else {
                return;
            }
            e.preventDefault();
            this._volume = Math.max(0, Math.min(100, this._volume + delta));
            void this._applyVolume();
        });
    }

    protected override _subscribe(): void {
        this._sub('playback:volumeChanged', (data) => {
            this._volume = data?.volume ?? this._volume;
            this._muted = data?.muted ?? this._muted;
            this._updateVisual();
        });

        getFb()
            .player.getVolume()
            .then((r) => {
                const resp = r as {
                    volume?: number;
                    muted?: boolean;
                    isMuted?: boolean;
                };
                this._volume = resp?.volume ?? 100;
                this._muted = resp?.muted ?? resp?.isMuted ?? false;
                this._updateVisual();
            })
            .catch(() => {
                /* silent */
            });
    }

    override disconnectedCallback(): void {
        if (this._dragController) {
            this._dragController.abort();
            this._dragController = null;
        }
        super.disconnectedCallback();
    }

    /** Compute the 0-100 volume implied by a mouse event on the track. */
    private _calcVolume(e: MouseEvent): number {
        const rect = this._trackEl.getBoundingClientRect();
        let pct: number;
        if (this.hasAttribute('vertical')) {
            pct = 1 - (e.clientY - rect.top) / rect.height;
        } else {
            pct = (e.clientX - rect.left) / rect.width;
        }
        return Math.round(Math.max(0, Math.min(1, pct)) * 100);
    }

    private async _setVolumeFromEvent(e: MouseEvent): Promise<void> {
        this._volume = this._calcVolume(e);
        await this._applyVolume();
    }

    private async _applyVolume(): Promise<void> {
        this._updateVisual();
        try {
            await getFb().player.setVolume(this._volume);
            this._emit<FbVolumeChangeDetail>('fb-volume-change', {
                volume: this._volume,
            });
        } catch {
            /* R6: silent degradation */
        }
    }

    private async _toggleMute(): Promise<void> {
        try {
            await getFb().player.toggleMute();
            this._emit<FbMuteToggleDetail>('fb-mute-toggle', {});
        } catch {
            /* R6: silent degradation */
        }
    }

    /** R7: only `style` / attribute writes during high-frequency updates. */
    private _updateVisual(): void {
        const pct = this._volume + '%';
        if (this.hasAttribute('vertical')) {
            this._fillEl.style.height = pct;
            this._thumbEl.style.bottom = pct;
            this._thumbEl.style.left = '';
            this._fillEl.style.width = '';
        } else {
            this._fillEl.style.width = pct;
            this._thumbEl.style.left = pct;
            this._thumbEl.style.bottom = '';
            this._fillEl.style.height = '';
        }
        this.style.setProperty('--fb-volume', (this._volume / 100).toString());

        this.setAttribute('volume', this._volume.toString());
        if (this._muted) this.setAttribute('muted', '');
        else this.removeAttribute('muted');

        this._trackEl.setAttribute('aria-valuenow', this._volume.toString());
    }
}
