/**
 * `output` — audio-output device discovery namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    OutputDevice,
    OutputGetEntriesResponse,
    OutputGetSettingsResponse,
} from '../../types/responses.js';

/**
 * Envelope returned by the C++ `output.getDevices` handler. The SDK
 * wrapper {@link output.getDevices} unwraps `.devices` so callers get
 * a flat `OutputDevice[]`.
 */
export interface OutputGetDevicesResponse {
    devices: OutputDevice[];
    count: number;
}

export const output = {
    /**
     * Returns the flat list of available output devices. Internally the
     * C++ host wraps the array in a `{ devices, count }` envelope; this
     * wrapper unwraps it for ergonomic iteration.
     */
    getDevices: async (): Promise<OutputDevice[]> => {
        const response = await bridge.invoke<OutputGetDevicesResponse>(
            'output.getDevices',
        );
        return Array.isArray(response?.devices) ? response.devices : [];
    },
    getEntries: () =>
        bridge.invoke<OutputGetEntriesResponse>('output.getEntries'),
    getSettings: () =>
        bridge.invoke<OutputGetSettingsResponse>('output.getSettings'),
};
