/**
 * `<fb-time-current>` — current playback position display (`m:ss` /
 * `h:mm:ss`).
 *
 * Pure display component. Subscribes to `playback:time` (R7: only
 * `textContent` and host-attribute writes inside the high-frequency
 * callback) and reflects the current value in seconds via the
 * `seconds` host attribute for theme-side hooks.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { formatTime } from './constants.js';
import { getFb } from './runtime.js';

export class FbTimeCurrent extends FbBaseElement {
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
        this._sub('playback:time', (data) => {
            this._seconds = data?.position || 0;
            this._text.textContent = formatTime(this._seconds);
            this.setAttribute('seconds', Math.floor(this._seconds).toString());
        });

        getFb()
            .player.getPosition()
            .then((r) => {
                this._seconds = (r as { position?: number })?.position || 0;
                this._text.textContent = formatTime(this._seconds);
                this.setAttribute(
                    'seconds',
                    Math.floor(this._seconds).toString(),
                );
            })
            .catch(() => {
                /* silent */
            });
    }
}
