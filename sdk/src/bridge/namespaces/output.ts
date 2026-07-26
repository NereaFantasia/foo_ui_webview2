/**
 * `output` — audio-output device discovery namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    OutputGetEntriesResponse,
    OutputGetSettingsResponse,
} from '../../types/responses.js';

/**
 * Single device row returned by `output.getDevices`.
 *
 * `guid` is all-zero (`{00000000-...}`) for the "default device" row of an
 * output backend and may repeat across backends, so it is not globally
 * unique on its own — key rows by the `(entryGuid, guid)` pair.
 */
export interface OutputDeviceInfo {
    /** Device GUID rendered as `{...}`; not globally unique (see above). */
    guid: string;
    /** Human-readable device name. */
    name: string;
    /** Display name of the output backend that owns this device. */
    entry: string;
    /** GUID of the owning output backend rendered as `{...}`. */
    entryGuid: string;
}

/**
 * Envelope returned by the C++ `output.getDevices` handler. The SDK
 * wrapper {@link output.getDevices} unwraps `.devices` so callers get
 * a flat `OutputDeviceInfo[]`.
 */
export interface OutputGetDevicesResponse {
    devices?: OutputDeviceInfo[];
    count?: number;
    /** Only present on failure. */
    success?: false;
    error?: string;
}

export const output = {
    /**
     * Returns the flat list of available output devices. Internally the
     * C++ host wraps the array in a `{ devices, count }` envelope; this
     * wrapper unwraps it for ergonomic iteration.
     *
     * @throws Error when the host reports a failure envelope — the flat
     *         array return type leaves no channel to carry an error value.
     */
    getDevices: async (): Promise<OutputDeviceInfo[]> => {
        const response = await bridge.invoke<OutputGetDevicesResponse>(
            'output.getDevices',
        );
        if (response && response.success === false) {
            throw new Error(response.error ?? 'output.getDevices failed');
        }
        return Array.isArray(response?.devices) ? response.devices : [];
    },
    getEntries: () =>
        bridge.invoke<OutputGetEntriesResponse>('output.getEntries'),
    getSettings: () =>
        bridge.invoke<OutputGetSettingsResponse>('output.getSettings'),
};
