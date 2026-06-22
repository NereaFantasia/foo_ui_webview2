/**
 * `replaygain` — ReplayGain mode / preamp / scan namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    ReplayGainGetResponse,
    ReplayGainGetModeResponse,
    ReplayGainGetPreampResponse,
    ReplayGainGetSettingsResponse,
} from '../../types/responses.js';

export const replaygain = {
    /** Accepts a single path or an array; always sent as `{ paths: string[] }`. */
    get: (paths: string | string[]) =>
        bridge.invoke<ReplayGainGetResponse>('replaygain.get', {
            paths: Array.isArray(paths) ? paths : [paths],
        }),
    getMode: () =>
        bridge.invoke<ReplayGainGetModeResponse>('replaygain.getMode'),
    setMode: (sourceMode: string, processingMode?: string) =>
        bridge.invoke<BaseResponse & { changed?: boolean }>(
            'replaygain.setMode',
            {
                sourceMode,
                ...(processingMode ? { processingMode } : {}),
            },
        ),
    getPreamp: () =>
        bridge.invoke<ReplayGainGetPreampResponse>('replaygain.getPreamp'),
    setPreamp: (withRg?: number, withoutRg?: number) =>
        bridge.invoke<BaseResponse & { changed?: boolean }>(
            'replaygain.setPreamp',
            {
                ...(withRg != null ? { withRg } : {}),
                ...(withoutRg != null ? { withoutRg } : {}),
            },
        ),
    getSettings: () =>
        bridge.invoke<ReplayGainGetSettingsResponse>('replaygain.getSettings'),
    /**
     * Trigger a ReplayGain scan over the given paths via the host's
     * context-menu pipeline.
     *
     * @param paths - Absolute file paths to scan. Empty array is a
     *                no-op that resolves with `success: false`.
     * @param opts.mode - `'track'` (default) scans per-file track gain;
     *                    `'album'` treats the selection as a single
     *                    album. Maps to the host's "Scan per-file
     *                    track gain" and "Scan selection as a single
     *                    album" menu entries respectively.
     */
    scan: (
        paths: string[],
        opts?: { mode?: 'track' | 'album' },
    ) =>
        bridge.invoke<BaseResponse & { scannedCount?: number; note?: string }>(
            'replaygain.scan',
            {
                paths,
                ...(opts?.mode ? { mode: opts.mode } : {}),
            },
        ),
    clear: (paths: string[]) =>
        bridge.invoke<BaseResponse & { clearedCount?: number }>(
            'replaygain.clear',
            { paths },
        ),
};
