/**
 * `config` — output / DSP / advanced / portable-config namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    ActiveDspPresetInfo,
    AdvancedConfigResponse,
    AdvancedConfigValueResponse,
    BaseResponse,
    ComponentInfo,
    ConfigExportResponse,
    ConfigGetAllResponse,
    ConfigGetReplaygainModeResponse,
    ConfigSetReplaygainModeResponse,
    DspPreset,
    LibraryFilePatternsResponse,
    LibraryStatus,
    OutputConfig,
    OutputDevice,
    PreferencesPagesResponse,
    PreferencesStandardGuids,
    ReplaygainSourceMode,
    ReplaygainSourceModeName,
    VersionInfo,
} from '../../types/responses.js';

export const config = {
    getOutputDevices: () =>
        bridge.invoke<OutputDevice[]>('config.getOutputDevices'),
    getOutputConfig: () => bridge.invoke<OutputConfig>('config.getOutputConfig'),
    setOutputDevice: (outputId: string, deviceId: string) =>
        bridge.invoke<BaseResponse>('config.setOutputDevice', {
            outputId,
            deviceId,
        }),
    /**
     * Set the output buffer length.
     *
     * Accepts either a single numeric argument (milliseconds, matching
     * the original `(ms)` signature) or an options object that lets
     * callers pick the unit explicitly:
     * - `milliseconds` — buffer in ms; the host divides by 1000.
     * - `bufferLength` — buffer in seconds (the host's native unit).
     *
     * If both fields are present the host prefers `milliseconds`. The
     * resolved buffer must fall within `[0.05, 2.0]` seconds; the host
     * rejects out-of-range values with an error envelope.
     *
     * @param value - Numeric milliseconds, **or** an options object
     *                with `milliseconds` and/or `bufferLength`.
     */
    setOutputBuffer: ((
        value: number | { milliseconds?: number; bufferLength?: number },
    ): Promise<BaseResponse> => {
        const opts =
            typeof value === 'number' ? { milliseconds: value } : value;
        return bridge.invoke<BaseResponse>('config.setOutputBuffer', {
            ...(opts.milliseconds != null
                ? { milliseconds: opts.milliseconds }
                : {}),
            ...(opts.bufferLength != null
                ? { bufferLength: opts.bufferLength }
                : {}),
        });
    }) as {
        (ms: number): Promise<BaseResponse>;
        (opts: {
            milliseconds?: number;
            bufferLength?: number;
        }): Promise<BaseResponse>;
    },
    getAdvancedConfig: () =>
        bridge.invoke<AdvancedConfigResponse>('config.getAdvancedConfig'),
    getAdvancedConfigValue: (guid: string) =>
        bridge.invoke<AdvancedConfigValueResponse>(
            'config.getAdvancedConfigValue',
            { guid },
        ),
    setAdvancedConfigValue: (guid: string, value: unknown) =>
        bridge.invoke<BaseResponse>('config.setAdvancedConfigValue', {
            guid,
            value,
        }),
    resetAdvancedConfig: (guid: string) =>
        bridge.invoke<BaseResponse>('config.resetAdvancedConfig', { guid }),
    getPreferencesPages: () =>
        bridge.invoke<PreferencesPagesResponse>('config.getPreferencesPages'),
    getPreferencesStandardGuids: () =>
        bridge.invoke<PreferencesStandardGuids>(
            'config.getPreferencesStandardGuids',
        ),
    getLibraryStatus: () =>
        bridge.invoke<LibraryStatus>('config.getLibraryStatus'),
    getLibraryFilePatterns: () =>
        bridge.invoke<LibraryFilePatternsResponse>(
            'config.getLibraryFilePatterns',
        ),
    showLibraryPreferences: () =>
        bridge.invoke<BaseResponse>('config.showLibraryPreferences'),
    getComponents: () =>
        bridge.invoke<ComponentInfo[]>('config.getComponents'),
    getVersionInfo: () => bridge.invoke<VersionInfo>('config.getVersionInfo'),
    getDspPresets: () => bridge.invoke<DspPreset[]>('config.getDspPresets'),
    getActiveDspPreset: () =>
        bridge.invoke<ActiveDspPresetInfo>('config.getActiveDspPreset'),
    setActiveDspPreset: (index: number) =>
        bridge.invoke<BaseResponse>('config.setActiveDspPreset', { index }),
    // === Portable config storage ===
    set: (key: string, value: unknown) =>
        bridge.invoke<BaseResponse>('config.set', { key, value }),
    get: (key: string) =>
        bridge.invoke<{ value: unknown }>('config.get', { key }),
    remove: (key: string) =>
        bridge.invoke<BaseResponse & { existed?: boolean }>(
            'config.remove',
            { key },
        ),
    /**
     * Full snapshot of the portable-config cache. Returns the
     * `{ success, items, configs, count }` envelope; `items` and
     * `configs` reference the same map and are kept as legacy aliases.
     */
    getAll: () => bridge.invoke<ConfigGetAllResponse>('config.getAll'),
    export: () => bridge.invoke<ConfigExportResponse>('config.export'),
    // === Cursor follow / playback follow / ReplayGain mode ===
    getCursorFollowPlayback: () =>
        bridge.invoke<{ enabled: boolean }>('config.getCursorFollowPlayback'),
    getPlaybackFollowCursor: () =>
        bridge.invoke<{ enabled: boolean }>('config.getPlaybackFollowCursor'),
    /**
     * Resolve the active ReplayGain source mode.
     *
     * The host returns the mode as an integer (`0` = none, `1` = track,
     * `2` = album, `3` = byPlaybackOrder). Use the
     * `REPLAYGAIN_SOURCE_MODE` constant dictionary exported alongside
     * the response type to compare against named entries:
     *
     *     const r = await config.getReplaygainMode();
     *     if (r.mode === REPLAYGAIN_SOURCE_MODE.track) { ... }
     *
     * The `value` field is an alias of `mode` retained for
     * compatibility with older host builds.
     */
    getReplaygainMode: () =>
        bridge.invoke<ConfigGetReplaygainModeResponse>(
            'config.getReplaygainMode',
        ),
    setCursorFollowPlayback: (enabled: boolean) =>
        bridge.invoke<BaseResponse>('config.setCursorFollowPlayback', {
            enabled,
        }),
    setPlaybackFollowCursor: (enabled: boolean) =>
        bridge.invoke<BaseResponse>('config.setPlaybackFollowCursor', {
            enabled,
        }),
    /**
     * Set the active ReplayGain source mode.
     *
     * Accepts either an integer (`0`-`3`, see `REPLAYGAIN_SOURCE_MODE`)
     * or the named alias the host accepts on input
     * (`'none' | 'track' | 'album' | 'byPlaybackOrder' | 'auto'`).
     * `'auto'` is treated by the host as an alias of
     * `'byPlaybackOrder'` and yields integer mode `3`.
     *
     * @param mode - Numeric mode or named alias.
     * @returns Echoes the integer mode the host applied. When an
     *          invalid `'auto'`-style string slips past the typed
     *          surface the host returns `code: 'INVALID_PARAMS'` plus a
     *          human-readable `error` instead of `success: true`.
     */
    setReplaygainMode: (
        mode: ReplaygainSourceMode | ReplaygainSourceModeName,
    ) =>
        bridge.invoke<ConfigSetReplaygainModeResponse>(
            'config.setReplaygainMode',
            typeof mode === 'number' ? { mode } : { sourceMode: mode },
        ),
};
