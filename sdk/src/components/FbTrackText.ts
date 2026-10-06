/**
 * `<fb-track-text>` — generic single-field track-text display.
 *
 * One configurable component covers what previously required separate
 * `<fb-title>` / `<fb-artist>` / `<fb-album>` elements.
 *
 * Attributes (observed):
 * - `field`: TrackInfo property name (default `'title'`).
 * - `tf`: title-formatting expression. When present overrides `field`.
 * - `placeholder`: text shown when the source returns an empty string.
 *
 * Updates on `playback:trackChanged` and `playback:dynamicInfoTrack`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { TrackInfo } from '../types/responses.js';

export class FbTrackText extends FbBaseElement {
    private _text!: HTMLSpanElement;

    static get observedAttributes(): string[] {
        return ['field', 'tf', 'placeholder'];
    }

    attributeChangedCallback(): void {
        if (!this._domReady) return;
        void this._fetchAndUpdate();
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:inline-block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}` +
            `</style>` +
            `<span part="text"></span>`;
        this._text = this._$<HTMLSpanElement>('[part=text]')!;
    }

    protected override _subscribe(): void {
        this._sub('playback:trackChanged', () => {
            void this._fetchAndUpdate();
        });
        this._sub('playback:dynamicInfoTrack', () => {
            void this._fetchAndUpdate();
        });
        void this._fetchAndUpdate();
    }

    private async _fetchAndUpdate(): Promise<void> {
        const tf = this.getAttribute('tf');
        const field = this.getAttribute('field') || 'title';
        const placeholder = this.getAttribute('placeholder') || '';
        const fb = getFb();

        try {
            let value: string | number = '';
            if (tf) {
                const result = await fb.utils.formatTitle(tf);
                value = (result as { result?: string })?.result || '';
            } else {
                const result = await fb.player.getCurrentTrack();
                // The no-track envelope (`found: false`) carries no metadata.
                const track = 'found' in result ? null : result;
                if (track) {
                    // `field` is a free-form attribute; a missing TrackInfo
                    // property reads as `undefined` and shows the placeholder.
                    const raw = track[field as keyof TrackInfo];
                    if (raw == null) value = '';
                    else if (typeof raw === 'number') value = raw.toString();
                    else value = String(raw);
                }
            }
            this._text.textContent = value ? String(value) : placeholder;
        } catch {
            // Keep the placeholder visible when the metadata request fails.
            this._text.textContent = placeholder;
        }
    }
}
