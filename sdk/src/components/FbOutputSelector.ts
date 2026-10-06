/**
 * `<fb-output-selector>` — `<select>`-based audio output device picker.
 *
 * Calls `fb.config.getOutputDevices()` on connect and on every
 * `audio:outputDeviceChanged` event to populate the dropdown. The
 * device flagged with `isCurrent: true` becomes the selected option.
 *
 * On change, calls `fb.config.setOutputDevice(outputId, deviceId)`
 * with the picked device's ids and emits `fb-output-change`.
 *
 * Reflects host attribute `device-name`.
 */

import { FbBaseElement } from './FbBaseElement.js';
import { getFb } from './runtime.js';
import type { FbOutputChangeDetail } from './types.js';

/** The fields of a `config.getOutputDevices` row this component reads. */
interface OutputDeviceInfo {
    name: string;
    outputId: string;
    deviceId: string;
    isCurrent?: boolean;
}

export class FbOutputSelector extends FbBaseElement {
    private _select!: HTMLSelectElement;
    private _devices: OutputDeviceInfo[] = [];

    protected override _buildDOM(): void {
        const root = this.shadowRoot;
        if (!root) return;
        root.innerHTML =
            `<style>${FbBaseElement.baseCSS}select{all:unset}</style>` +
            `<select part="select" aria-label="Output device"></select>`;
        this._select = this._$<HTMLSelectElement>('select')!;
    }

    protected override _setupEvents(): void {
        this._listen(this._select, 'change', () => {
            const idx = parseInt(this._select.value, 10);
            const device = this._devices[idx];
            if (!device) return;
            getFb()
                .config.setOutputDevice(device.outputId, device.deviceId)
                .catch(() => {
                    /* silent */
                });
            this.setAttribute('device-name', device.name);
            this._emit<FbOutputChangeDetail>('fb-output-change', {
                index: idx,
                name: device.name,
                outputId: device.outputId,
                deviceId: device.deviceId,
            });
        });
    }

    protected override _subscribe(): void {
        this._sub('audio:outputDeviceChanged', () => {
            void this._loadDevices();
        });
        void this._loadDevices();
    }

    private async _loadDevices(): Promise<void> {
        try {
            const result = await getFb().config.getOutputDevices();
            // A failed call keeps the list shown so far.
            if (result.success === false) return;
            this._devices = result.devices;
            this._select.innerHTML = this._devices
                .map(
                    (d, i) =>
                        `<option value="${i}"${d.isCurrent ? ' selected' : ''}>${this._escHtml(d.name)}</option>`,
                )
                .join('');
            const current = this._devices.find((d) => d.isCurrent);
            if (current) {
                this.setAttribute('device-name', current.name);
            }
        } catch {
            /* silent */
        }
    }
}
