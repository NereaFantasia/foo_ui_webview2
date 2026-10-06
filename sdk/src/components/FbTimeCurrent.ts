/**
 * `<fb-time-current>` — current playback position display (`m:ss` /
 * `h:mm:ss`).
 *
 * DOM writes in the high-frequency `playback:time` callback are
 * limited to `textContent` and the `seconds` host attribute, which
 * exposes the current position in whole seconds for themes.
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
