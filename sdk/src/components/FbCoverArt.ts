/**
 * `<fb-cover-art>` — current-track album-art display.
 *
 * Visual contract:
 * - Two CSS parts: `container`, `image`. One named slot:
 *   `placeholder` for the empty / error state.
 * - Reflects host attribute `loaded` (set when image load fires).
 *
 * Attributes (observed):
 * - `type`: `'front' | 'back' | 'disc' | 'icon' | 'artist'`
 *   (default `'front'`).
 * - `use-fb2k`: when present, tries `artwork.getFb2kUrl` first and
 *   accepts only a `data:` URL from its response. If no usable URL
 *   is returned, falls back to `artwork.getCurrent`. A rejected
 *   request shows the placeholder.
 *
 * Track changes are debounced for 300 ms to limit host requests
 * during rapid skips.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbCoverErrorDetail, FbCoverLoadDetail } from './types.js';

type CoverType = 'front' | 'back' | 'disc' | 'icon' | 'artist';

export class FbCoverArt extends FbBaseElement {
    private _debounceTimer: ReturnType<typeof setTimeout> | null = null;
    private _img!: HTMLImageElement;
    private _placeholder!: HTMLSlotElement | null;

    static get observedAttributes(): string[] {
        return ['type', 'use-fb2k'];
    }

    attributeChangedCallback(): void {
        if (!this._domReady) return;
        void this._loadCover();
    }

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}` +
            `:host{display:inline-block;overflow:hidden}` +
            `[part=container]{position:relative;width:100%;height:100%}` +
            `[part=image]{display:block;width:100%;height:100%;object-fit:cover}` +
            `</style>` +
            `<div part="container">` +
            `<img part="image" loading="lazy" hidden />` +
            `<slot name="placeholder"></slot>` +
            `</div>`;
        this._img = this._$<HTMLImageElement>('[part=image]')!;
        this._placeholder = this._$<HTMLSlotElement>('slot[name=placeholder]');
    }

    protected override _setupEvents(): void {
        this._listen(this._img, 'load', () => {
            this._img.hidden = false;
            if (this._placeholder) this._placeholder.hidden = true;
            this.setAttribute('loaded', '');
            this._emit<FbCoverLoadDetail>('fb-cover-load', {});
        });

        this._listen(this._img, 'error', () => {
            this._img.hidden = true;
            if (this._placeholder) this._placeholder.hidden = false;
            this.removeAttribute('loaded');
            this.removeAttribute('src');
            this._emit<FbCoverErrorDetail>('fb-cover-error', {});
        });
    }

    protected override _subscribe(): void {
        this._sub('playback:trackChanged', () => {
            // Debounce for 300 ms to limit host requests during rapid skips.
            if (this._debounceTimer) clearTimeout(this._debounceTimer);
            this._debounceTimer = setTimeout(() => {
                void this._loadCover();
            }, 300);
        });
        void this._loadCover();
    }

    override disconnectedCallback(): void {
        if (this._debounceTimer) clearTimeout(this._debounceTimer);
        this._debounceTimer = null;
        super.disconnectedCallback();
    }

    private async _loadCover(): Promise<void> {
        const type = (this.getAttribute('type') as CoverType | null) || 'front';
        const useFb2k = this.hasAttribute('use-fb2k');
        const fb = getFb();

        try {
            let src = '';

            if (useFb2k && fb.artwork.getFb2kUrl) {
                const result = (await fb.artwork.getFb2kUrl(type)) as
                    | { available?: boolean; dataUrl?: string; url?: string }
                    | null;
                if (result && result.available !== false) {
                    const url = result.dataUrl || result.url || '';
                    if (url.startsWith('data:')) src = url;
                }
            }

            if (!src) {
                const art = (await fb.artwork.getCurrent(type)) as
                    | {
                          available?: boolean;
                          dataUrl?: string;
                          data?: string;
                          mimeType?: string;
                      }
                    | null;
                if (art && art.available !== false && art.dataUrl) {
                    src = art.dataUrl;
                } else if (art && art.data) {
                    src = `data:${art.mimeType || 'image/jpeg'};base64,${art.data}`;
                }
            }

            if (src) {
                this._img.src = src;
                // Keep large data URLs out of the host's reflected `src` attribute.
                if (!src.startsWith('data:')) this.setAttribute('src', src);
                else this.removeAttribute('src');
            } else {
                this._img.hidden = true;
                this._img.removeAttribute('src');
                if (this._placeholder) this._placeholder.hidden = false;
                this.removeAttribute('loaded');
                this.removeAttribute('src');
            }
        } catch {
            // Request failures show the placeholder without emitting `fb-cover-error`.
            this._img.hidden = true;
            if (this._placeholder) this._placeholder.hidden = false;
            this.removeAttribute('loaded');
            this.removeAttribute('src');
        }
    }
}
