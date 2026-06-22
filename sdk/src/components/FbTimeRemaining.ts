/**
 * `<fb-time-remaining>` — time-left display (always negative-prefix,
 * e.g. `-2:43`).
 *
 * Subscribes to both `playback:time` (for the running countdown) and
 * `playback:trackChanged` (to refresh `_duration`). The duration
 * value is sourced from the event payload when present and falls
 * back to `fb.player.getCurrentTrack()` otherwise.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { formatTime } from './constants.js';
import { getFb } from './runtime.js';

export class FbTimeRemaining extends FbBaseElement {
    private _position = 0;
    private _duration = 0;
    private _text!: HTMLSpanElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:inline-block;font-variant-numeric:tabular-nums}</style>` +
            `<span part="text">-0:00</span>`;
        this._text = this._$<HTMLSpanElement>('[part=text]')!;
    }

    protected override _subscribe(): void {
        const fb = getFb();

        this._sub('playback:time', (data) => {
            this._position = data?.position || 0;
            this._updateText();
        });

        this._sub('playback:trackChanged', (data) => {
            if (data && data.duration !== undefined) {
                this._duration = data.duration;
            } else {
                fb.player
                    .getCurrentTrack()
                    .then((t) => {
                        this._duration = t?.duration || 0;
                        this._updateText();
                    })
                    .catch(() => {
                        /* silent */
                    });
            }
            this._position = 0;
            this._updateText();
        });

        // Initial state.
        fb.player
            .getCurrentTrack()
            .then((t) => {
                this._duration = t?.duration || 0;
            })
            .then(() => fb.player.getPosition())
            .then((r) => {
                this._position = (r as { position?: number })?.position || 0;
                this._updateText();
            })
            .catch(() => {
                /* silent */
            });
    }

    private _updateText(): void {
        const remaining = Math.max(0, this._duration - this._position);
        this._text.textContent = '-' + formatTime(remaining);
        this.setAttribute('seconds', Math.floor(remaining).toString());
    }
}
