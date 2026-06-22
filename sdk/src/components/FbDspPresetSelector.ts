/**
 * `<fb-dsp-preset-selector>` — `<select>` for the host DSP preset
 * chain.
 *
 * Calls `fb.config.getDspPresets()` and `fb.config.getActiveDspPreset()`
 * in parallel on connect and after every `audio:dspPresetChanged`
 * event. Each preset object carries an `index` and `name`; the option
 * `value` uses `preset.index` directly so it stays correct even if
 * the host reorders presets.
 *
 * On change, fires `fb.config.setActiveDspPreset(index)` and emits
 * `fb-dsp-change`. Reflects host attribute `preset-name`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbDspChangeDetail } from './types.js';

/**
 * Wider shape than the bridge's exported `DspPreset` (`name`,
 * `isActive`) — the runtime JSON also carries an `index` field that
 * this component uses as the `<option value>`. Kept private so the
 * public bridge type stays the source of truth.
 */
interface DspPresetInfo {
    index: number;
    name: string;
}

interface ActivePreset {
    index?: number;
    name?: string;
}

export class FbDspPresetSelector extends FbBaseElement {
    private _select!: HTMLSelectElement;
    private _presets: DspPresetInfo[] = [];
    private _activeIndex = -1;

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}select{all:unset}</style>` +
            `<select part="select" aria-label="DSP preset"></select>`;
        this._select = this._$<HTMLSelectElement>('select')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._select, 'change', () => {
            const index = parseInt(this._select.value, 10);
            const preset = this._presets.find((p) => p.index === index);
            getFb()
                .config.setActiveDspPreset(index)
                .catch(() => {
                    /* silent */
                });
            this.setAttribute('preset-name', preset?.name || '');
            this._emit<FbDspChangeDetail>('fb-dsp-change', {
                index,
                name: preset?.name || '',
            });
        });
    }

    protected override _subscribe(): void {
        this._sub('audio:dspPresetChanged', () => {
            void this._loadPresets();
        });
        void this._loadPresets();
    }

    private async _loadPresets(): Promise<void> {
        try {
            const fb = getFb();
            const [presets, active] = await Promise.all([
                fb.config.getDspPresets(),
                fb.config.getActiveDspPreset(),
            ]);
            // Cast through `unknown` because the bridge's exported
            // `DspPreset` type omits the `index` field returned by the host.
            this._presets =
                ((presets as unknown) as DspPresetInfo[] | null) ?? [];
            const activeData = active as ActivePreset | null;
            this._activeIndex = activeData?.index ?? -1;
            this._select.innerHTML = this._presets
                .map(
                    (p) =>
                        `<option value="${p.index}"${p.index === this._activeIndex ? ' selected' : ''}>${this._escHtml(p.name)}</option>`,
                )
                .join('');
            if (activeData?.name) {
                this.setAttribute('preset-name', activeData.name);
            }
        } catch {
            /* silent */
        }
    }
}
