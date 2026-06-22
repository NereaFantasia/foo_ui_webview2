/**
 * `<fb-seek-bar>` — playback progress bar with drag-to-seek and
 * keyboard-to-seek support.
 *
 * Visual contract:
 * - Three CSS parts: `track`, `fill`, `thumb`. Themes style each part
 *   independently — the component only sets `width` / `left` and
 *   `--fb-seek-progress`, `--fb-seek-hover-progress` custom props.
 * - Reflects three read-only host attributes: `position` (floor-seconds),
 *   `duration` (floor-seconds), `progress` (0-1, four decimals).
 *
 * Interaction:
 * - Click anywhere on the track → `fb.player.seek(position)`.
 * - Mouse-down + drag → continuous `fb-seeking` events; mouse-up →
 *   single `fb-seek` event.
 * - Arrow / Home / End keys → ±5s / 0 / duration seek (R5 keyboard).
 *
 * Lifecycle:
 * - A dedicated `_dragController` drives `mousemove` / `mouseup`
 *   listeners so a fast disconnect during a drag aborts cleanly.
 *   Overrides `disconnectedCallback` to abort the drag controller
 *   *before* delegating to {@link FbBaseElement.disconnectedCallback}.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbSeekDetail, FbSeekingDetail } from './types.js';

export class FbSeekBar extends FbBaseElement {
    private _position = 0;
    private _duration = 0;
    private _isDragging = false;
    /** Drag-only AbortController; recreated per drag, aborted on mouse-up or disconnect. */
    private _dragController: AbortController | null = null;

    private _track!: HTMLDivElement;
    private _fill!: HTMLDivElement;
    private _thumb!: HTMLDivElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:flex;align-items:center;width:100%}` +
            // `padding:6px 0` widens the click target for the thin
            // seek track — hit-target only, not a visual decision
            // (see AGENTS.md §2.6.1 "Hit-target padding" whitelist).
            `[part=track]{position:relative;flex:1;cursor:pointer;padding:6px 0}` +
            `[part=fill]{position:absolute;top:50%;left:0;transform:translateY(-50%);pointer-events:none}` +
            `[part=thumb]{position:absolute;top:50%;transform:translateY(-50%);pointer-events:none}` +
            `</style>` +
            `<div part="track" role="slider" tabindex="0"` +
            ` aria-label="Seek" aria-valuemin="0" aria-valuemax="0" aria-valuenow="0">` +
            `<div part="fill"></div>` +
            `<div part="thumb"></div>` +
            `</div>`;
        this._track = this._$<HTMLDivElement>('[part=track]')!;
        this._fill = this._$<HTMLDivElement>('[part=fill]')!;
        this._thumb = this._$<HTMLDivElement>('[part=thumb]')!;
    }

    protected override _setupEvents(): void {
        this._listen<MouseEvent>(this._track, 'click', (e) => {
            if (this._isDragging) return;
            this._seekToEvent(e);
        });

        this._listen<MouseEvent>(this._track, 'mousedown', (e) => {
            e.preventDefault();
            this._isDragging = true;
            this._dragController = new AbortController();
            const signal = this._dragController.signal;

            document.addEventListener(
                'mousemove',
                (ev) => {
                    const me = ev as MouseEvent;
                    const pct = this._calcPercent(me);
                    this._position = pct * this._duration;
                    this._updateVisual();
                    this._emit<FbSeekingDetail>('fb-seeking', {
                        position: this._position,
                    });
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
                    this._seekToEvent(ev as MouseEvent);
                },
                { signal },
            );
        });

        this._listen<MouseEvent>(this._track, 'mousemove', (e) => {
            if (this._isDragging) return;
            const pct = this._calcPercent(e);
            this.style.setProperty('--fb-seek-hover-progress', pct.toString());
        });

        this._listen(this._track, 'mouseleave', () => {
            this.style.removeProperty('--fb-seek-hover-progress');
        });

        this._listen<KeyboardEvent>(this._track, 'keydown', (e) => {
            let delta = 0;
            if (e.key === 'ArrowRight' || e.key === 'ArrowUp') delta = 5;
            else if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') delta = -5;
            else if (e.key === 'Home') {
                e.preventDefault();
                void this._seekTo(0);
                return;
            } else if (e.key === 'End') {
                e.preventDefault();
                void this._seekTo(this._duration);
                return;
            } else {
                return;
            }
            e.preventDefault();
            void this._seekTo(
                Math.max(0, Math.min(this._duration, this._position + delta)),
            );
        });
    }

    protected override _subscribe(): void {
        const fb = getFb();

        this._sub('playback:time', (data) => {
            if (!this._isDragging) {
                this._position = data?.position || 0;
                this._updateVisual();
            }
        });

        this._sub('playback:trackChanged', (data) => {
            if (data && data.duration !== undefined) {
                this._duration = data.duration;
            } else {
                fb.player
                    .getCurrentTrack()
                    .then((t) => {
                        if (t) this._duration = t.duration || 0;
                    })
                    .catch(() => {
                        /* silent */
                    });
            }
            this._position = 0;
            this._updateVisual();
        });

        this._sub('playback:seeked', (data) => {
            this._position = data?.position || 0;
            this._updateVisual();
        });

        // Initial state — silently ignore failures.
        fb.player
            .getCurrentTrack()
            .then((t) => {
                if (t) this._duration = t.duration || 0;
            })
            .then(() => fb.player.getPosition())
            .then((r) => {
                this._position = (r as { position?: number })?.position || 0;
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

    /** Mouse X relative to the track, clamped to 0-1. */
    private _calcPercent(e: MouseEvent): number {
        const rect = this._track.getBoundingClientRect();
        return Math.max(
            0,
            Math.min(1, (e.clientX - rect.left) / rect.width),
        );
    }

    private _seekToEvent(e: MouseEvent): void {
        const pct = this._calcPercent(e);
        void this._seekTo(pct * this._duration);
    }

    private async _seekTo(seconds: number): Promise<void> {
        this._position = seconds;
        this._updateVisual();
        try {
            await getFb().player.seek(seconds);
            this._emit<FbSeekDetail>('fb-seek', { position: seconds });
        } catch {
            /* R6: silent degradation */
        }
    }

    /**
     * R7: only `style` and host attribute writes — never DOM
     * structural mutations from the high-frequency `playback:time`
     * callback path.
     */
    private _updateVisual(): void {
        const progress =
            this._duration > 0 ? this._position / this._duration : 0;
        const pct = (progress * 100).toFixed(2) + '%';
        this._fill.style.width = pct;
        this._thumb.style.left = pct;
        this.style.setProperty('--fb-seek-progress', progress.toString());

        this.setAttribute('position', Math.floor(this._position).toString());
        this.setAttribute('duration', Math.floor(this._duration).toString());
        this.setAttribute('progress', progress.toFixed(4));

        this._track.setAttribute(
            'aria-valuenow',
            Math.floor(this._position).toString(),
        );
        this._track.setAttribute(
            'aria-valuemax',
            Math.floor(this._duration).toString(),
        );
    }
}
