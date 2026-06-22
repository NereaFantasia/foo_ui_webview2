/**
 * `<fb-time-total>` — total track duration display (`m:ss` /
 * `h:mm:ss`).
 *
 * Subscribes to `playback:trackChanged`. When the event payload omits
 * `duration` (some hosts only fire a minimal track-changed event)
 * falls back to a `fb.player.getCurrentTrack()` lookup.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { formatTime } from './constants.js';
import { getFb } from './runtime.js';

export class FbTimeTotal extends FbBaseElement {
    private _seconds = 0;
    private _text!: HTMLSpanElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:inline-block;font-variant-numeric:tabular-nums}` +
            `</style>` +
            `<span part="text">0:00</span>`;
        this._text = this._$<HTMLSpanElement>('[part=text]')!;
    }

    protected override _subscribe(): void {
        const fb = getFb();

        this._sub('playback:trackChanged', (data) => {
            if (data && data.duration !== undefined) {
                this._seconds = data.duration;
                this._updateText();
            } else {
                fb.player
                    .getCurrentTrack()
                    .then((t) => {
                        this._seconds = t?.duration || 0;
                        this._updateText();
                    })
                    .catch(() => {
                        /* silent */
                    });
            }
        });

        fb.player
            .getCurrentTrack()
            .then((t) => {
                this._seconds = t?.duration || 0;
                this._updateText();
            })
            .catch(() => {
                /* silent */
            });
    }

    private _updateText(): void {
        this._text.textContent = formatTime(this._seconds);
        this.setAttribute('seconds', Math.floor(this._seconds).toString());
    }
}
