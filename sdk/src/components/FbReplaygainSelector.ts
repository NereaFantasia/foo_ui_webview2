/**
 * `<fb-replaygain-selector>` — fixed-options `<select>` for the
 * host's ReplayGain source mode (Off / Track / Album / Auto).
 *
 * The four `<option>` values are hard-coded (`'none' | 'track' |
 * 'album' | 'auto'`) — the host enum is stable across foobar2000
 * releases. Numeric `audio:replaygainModeChanged` payloads are
 * mapped through {@link RG_MODE_NAMES}.
 *
 * On change, calls `fb.replaygain.setMode(mode)` and emits
 * `fb-replaygain-change`. Reflects host attribute `mode`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { RG_MODE_NAMES } from './constants.js';
import { getFb } from './runtime.js';
import type { FbReplaygainChangeDetail } from './types.js';

interface ReplaygainModeResult {
    sourceMode?: string;
}

export class FbReplaygainSelector extends FbBaseElement {
    private _select!: HTMLSelectElement;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}select{all:unset}</style>` +
            `<select part="select" aria-label="ReplayGain mode">` +
            `<option value="none">Off</option>` +
            `<option value="track">Track</option>` +
            `<option value="album">Album</option>` +
            `<option value="auto">Auto</option>` +
            `</select>`;
        this._select = this._$<HTMLSelectElement>('select')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._select, 'change', () => {
            const mode = this._select.value;
            void (async () => {
                try {
                    await getFb().replaygain.setMode(mode);
                } catch {
                    /* silent */
                }
            })();
            this.setAttribute('mode', mode);
            this._emit<FbReplaygainChangeDetail>('fb-replaygain-change', {
                mode,
            });
        });
    }

    protected override _subscribe(): void {
        this._sub('audio:replaygainModeChanged', (data) => {
            const modeNum = (data as { mode?: number } | null)?.mode ?? 0;
            const name = RG_MODE_NAMES[modeNum] || 'none';
            this._updateSelection(name);
        });
        getFb()
            .replaygain.getMode()
            .then((r) => {
                const result = r as ReplaygainModeResult | null;
                this._updateSelection(result?.sourceMode || 'none');
            })
            .catch(() => {
                /* silent */
            });
    }

    private _updateSelection(mode: string): void {
        this._select.value = mode;
        this.setAttribute('mode', mode);
    }
}
