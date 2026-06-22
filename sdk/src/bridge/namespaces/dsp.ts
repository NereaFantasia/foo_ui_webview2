/**
 * `dsp` — DSP chain / preset namespace.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse, DspPreset } from '../../types/responses.js';

/** Envelope returned by `dsp.getPresets`. */
export interface DspGetPresetsResponse {
    presets: Array<DspPreset & { active?: boolean }>;
    count: number;
    /** Index of the currently selected preset; `pfc::infinite_size` when none. */
    selectedIndex: number;
}

/** Single DSP entry in `DspGetChainResponse.dsps`. */
export interface DspChainEntry {
    /** Position in the active chain. */
    index: number;
    /** Owning DSP entry GUID rendered as `{...}`. */
    guid: string;
    /** Display name of the DSP. */
    name: string;
}

/**
 * Envelope returned by `dsp.getChain`. The success branch carries the
 * active chain and (best-effort) the active preset name; a failure
 * branch carries `{ success: false, error }`.
 */
export interface DspGetChainResponse {
    /** Active DSP chain in execution order. */
    dsps?: DspChainEntry[];
    /** Active preset display name; `null` when no preset is currently selected. */
    activePreset?: string | null;
    /** Active preset index; only present when `activePreset` is non-null. */
    activePresetIndex?: number;
    /** Only present on failure. */
    success?: false;
    error?: string;
}

/** Single available-DSP descriptor in `DspGetAvailableResponse.dsps`. */
export interface DspAvailableEntry {
    /** DSP entry GUID rendered as `{...}`. */
    guid: string;
    /** Display name of the DSP. */
    name: string;
    /** True when the DSP exposes a configuration popup. */
    hasConfig: boolean;
}

/** Envelope returned by `dsp.getAvailable`. */
export interface DspGetAvailableResponse {
    /** Discovered DSP entries (excludes presets). */
    dsps?: DspAvailableEntry[];
    /** Number of entries in {@link dsps}. */
    count?: number;
    /** Only present on failure. */
    success?: false;
    error?: string;
}

export const dsp = {
    getChain: () => bridge.invoke<DspGetChainResponse>('dsp.getChain'),
    setChain: (dsps: unknown[]) =>
        bridge.invoke<BaseResponse & { count?: number }>('dsp.setChain', {
            dsps,
        }),
    getPresets: () => bridge.invoke<DspGetPresetsResponse>('dsp.getPresets'),
    /** Accepts either preset index (number) or preset name (string). */
    applyPreset: (indexOrName: number | string) =>
        bridge.invoke<
            BaseResponse & { appliedPreset?: string; appliedIndex?: number }
        >(
            'dsp.applyPreset',
            typeof indexOrName === 'string'
                ? { name: indexOrName }
                : { index: indexOrName },
        ),
    getAvailable: () =>
        bridge.invoke<DspGetAvailableResponse>('dsp.getAvailable'),
    addDsp: (guid: string, position?: number) =>
        bridge.invoke<BaseResponse & { addedDsp?: string }>('dsp.addDsp', {
            guid,
            ...(position != null ? { position } : {}),
        }),
    removeDsp: (index: number) =>
        bridge.invoke<BaseResponse & { removedDsp?: string }>(
            'dsp.removeDsp',
            { index },
        ),
    moveDsp: (from: number, to: number) =>
        bridge.invoke<BaseResponse & { movedDsp?: string; message?: string }>(
            'dsp.moveDsp',
            { from, to },
        ),
};
