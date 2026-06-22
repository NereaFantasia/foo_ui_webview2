/**
 * `<fb-tech-info>` — technical info row (codec / bitrate / samplerate
 * / channels).
 *
 * Attribute (observed):
 * - `field`: `'all'` (default — joins available parts with ` | `),
 *   or one of `'codec' | 'bitrate' | 'samplerate' | 'channels'`.
 *
 * Channel display: `2 → 'Stereo'`, `1 → 'Mono'`, anything else →
 * `'<n>ch'`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { JsonObject } from '../types/json.js';

type TechField = 'all' | 'codec' | 'bitrate' | 'samplerate' | 'channels';

export class FbTechInfo extends FbBaseElement {
    private _text!: HTMLSpanElement;

    static get observedAttributes(): string[] {
        return ['field'];
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
            `:host{display:inline-block}</style>` +
            `<span part="text"></span>`;
        this._text = this._$<HTMLSpanElement>('[part=text]')!;
    }

    protected override _subscribe(): void {
        this._sub('playback:trackChanged', () => {
            void this._fetchAndUpdate();
        });
        void this._fetchAndUpdate();
    }

    private static _formatChannels(n: number): string {
        if (n === 2) return 'Stereo';
        if (n === 1) return 'Mono';
        return n + 'ch';
    }

    private async _fetchAndUpdate(): Promise<void> {
        const field = (this.getAttribute('field') as TechField | null) || 'all';
        try {
            const track = (await getFb().player.getCurrentTrack()) as
                | JsonObject
                | null;
            if (!track) {
                this._text.textContent = '';
                return;
            }
            let value = '';
            if (field === 'all') {
                const parts: string[] = [];
                if (track.codec) parts.push(String(track.codec));
                if (track.bitrate) parts.push(`${track.bitrate} kbps`);
                if (track.sampleRate)
                    parts.push(
                        `${(Number(track.sampleRate) / 1000).toFixed(1)} kHz`,
                    );
                if (track.channels)
                    parts.push(FbTechInfo._formatChannels(Number(track.channels)));
                value = parts.join(' | ');
            } else {
                const raw = field === 'samplerate' ? track.sampleRate : track[field];
                if (field === 'samplerate' && raw)
                    value = `${(Number(raw) / 1000).toFixed(1)} kHz`;
                else if (field === 'bitrate' && raw) value = `${raw} kbps`;
                else if (field === 'channels' && raw)
                    value = FbTechInfo._formatChannels(Number(raw));
                else value = raw != null ? String(raw) : '';
            }
            this._text.textContent = value;
        } catch {
            this._text.textContent = '';
        }
    }
}
