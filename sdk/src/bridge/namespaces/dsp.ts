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
    getChain: () => call('dsp.getChain'),
    /**
     * Replaces the whole chain. Only `guid` is sent for each entry, since the host refuses
     * any other key; the rows `getChain` reports can therefore be passed back as they are.
     */
    setChain: (dsps: ReadonlyArray<Pick<DspChainSpec, 'guid'>>) =>
        call('dsp.setChain', {
            dsps: dsps.map(({ guid }) => ({ guid })),
        }),
    getPresets: () => call('dsp.getPresets'),
    /** Accepts either preset index (number) or preset name (string). */
    applyPreset: (indexOrName: number | string) =>
        call(
            'dsp.applyPreset',
            (typeof indexOrName === 'string'
                ? { name: indexOrName }
                : { index: indexOrName }),
        ),
    getAvailable: () =>
        call('dsp.getAvailable'),
    addDsp: (guid: string, position?: number) =>
        call('dsp.addDsp', {
            guid,
            ...(position != null ? { position } : {}),
        }),
    removeDsp: (index: number) =>
        call('dsp.removeDsp', {
            index,
        }),
    moveDsp: (from: number, to: number) =>
        call('dsp.moveDsp', {
            from,
            to,
        }),
};
