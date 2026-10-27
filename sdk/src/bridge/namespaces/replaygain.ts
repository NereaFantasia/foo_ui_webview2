import { call } from '../call.js';
import type {
    ReplaygainSetModeParams,
} from '../../types/generated/params.js';

/**
 * `replaygain` — ReplayGain mode / preamp / scan namespace.
 */
export const replaygain = {
    /** Accepts a single path or an array; always sent as `{ paths: string[] }`. */
    get: (paths: string | string[]) =>
        call('replaygain.get', {
            paths: Array.isArray(paths) ? paths : [paths],
        }),
    /** Read the ReplayGain source mode and processing mode. */
    getMode: () =>
        call('replaygain.getMode'),
    /** Change the ReplayGain source mode, processing mode, or both. */
    setMode: (
        sourceMode: NonNullable<ReplaygainSetModeParams['sourceMode']>,
        processingMode?: ReplaygainSetModeParams['processingMode'],
    ) =>
        call('replaygain.setMode', {
            sourceMode,
            ...(processingMode ? { processingMode } : {}),
        }),
    /** Read the two ReplayGain preamps, in dB. */
    getPreamp: () =>
        call('replaygain.getPreamp'),
    /** Change one or both ReplayGain preamps in dB. */
    setPreamp: (withRg?: number, withoutRg?: number) =>
        call('replaygain.setPreamp', {
            ...(withRg != null ? { withRg } : {}),
            ...(withoutRg != null ? { withoutRg } : {}),
        }),
    /** Read the complete ReplayGain configuration: modes, preamps, and active status. */
    getSettings: () =>
        call('replaygain.getSettings'),
    /**
     * Trigger a ReplayGain scan over the given paths via the host's
     * context-menu pipeline.
     *
     * @param paths - Absolute file paths to scan; the host refuses an
     *                empty list with `INVALID_PARAMS`.
     * @param opts.mode - `'track'` (default) scans per-file track gain;
     *                    `'album'` treats the selection as a single
     *                    album. Maps to the host's "Scan per-file
     *                    track gain" and "Scan selection as a single
     *                    album" menu entries respectively.
     */
    scan: (paths: string[], opts?: { mode?: 'track' | 'album' }) =>
        call('replaygain.scan', {
            paths,
            ...(opts?.mode ? { mode: opts.mode } : {}),
        }),
    /** Remove ReplayGain values from the specified files. */
    clear: (paths: string[]) =>
        call('replaygain.clear', {
            paths,
        }),
};
