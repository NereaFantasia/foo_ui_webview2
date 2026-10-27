import { call } from '../call.js';
import type { DspChainSpec } from '../../types/generated/schema-types.js';
import type {
    DspGetAvailableResponse,
    DspGetChainResponse,
    DspGetPresetsResponse,
} from '../../types/generated/responses.js';

// The shapes come from the declarations in src/api/schema/dsp.ts; these are the module's
// public names for them.
export type {
    DspGetAvailableResponse,
    DspGetChainResponse,
    DspGetPresetsResponse,
} from '../../types/generated/responses.js';
export type {
    DspAvailableEntry,
    DspChainEntry,
    DspChainSpec,
} from '../../types/generated/schema-types.js';

/**
 * `dsp` — DSP chain / preset namespace.
 */
export const dsp = {
    /** Read the active DSP chain and which preset, if any, it currently corresponds to. */
    getChain: () => call('dsp.getChain'),
    /**
     * Replace the whole chain. Only `guid` is sent for each entry, since the host refuses
     * any other key; the rows `getChain` reports can therefore be passed back as they are.
     */
    setChain: (dsps: ReadonlyArray<Pick<DspChainSpec, 'guid'>>) =>
        call('dsp.setChain', {
            dsps: dsps.map(({ guid }) => ({ guid })),
        }),
    /** List the DSP presets stored in the profile and which one is selected. */
    getPresets: () => call('dsp.getPresets'),
    /** Apply a preset by index (number) or name (string). */
    applyPreset: (indexOrName: number | string) =>
        call(
            'dsp.applyPreset',
            (typeof indexOrName === 'string'
                ? { name: indexOrName }
                : { index: indexOrName }),
        ),
    /** List every installed DSP processor. */
    getAvailable: () =>
        call('dsp.getAvailable'),
    /** Insert a processor into the active chain with its default settings. */
    addDsp: (guid: string, position?: number) =>
        call('dsp.addDsp', {
            guid,
            ...(position != null ? { position } : {}),
        }),
    /** Remove one entry from the active chain by index. */
    removeDsp: (index: number) =>
        call('dsp.removeDsp', {
            index,
        }),
    /** Move one entry of the active chain to another position, keeping its settings. */
    moveDsp: (from: number, to: number) =>
        call('dsp.moveDsp', {
            from,
            to,
        }),
};
