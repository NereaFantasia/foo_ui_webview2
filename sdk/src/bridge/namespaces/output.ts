import { call } from '../call.js';
import type {
    OutputGetDevicesSuccess,
} from '../../types/generated/responses.js';

/**
 * Single device row returned by `output.getDevices`.
 *
 * `guid` is all-zero (`{00000000-...}`) for the "default device" row of an
 * output backend and may repeat across backends, so it is not globally
 * unique on its own — key rows by the `(entryGuid, guid)` pair.
 */
export type OutputDeviceInfo = OutputGetDevicesSuccess['devices'][number];

/**
 * `output` — audio-output device discovery namespace.
 */
export const output = {
    /** The available output devices, in `devices`. */
    getDevices: () => call('output.getDevices'),
    /** List the output modules (foobar2000's output entries) with their capability flags. */
    getEntries: () =>
        call('output.getEntries'),
    /** Report the display names of the output modules. */
    getSettings: () =>
        call('output.getSettings'),
};
