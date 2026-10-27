import { bridge } from '../Bridge.js';
import { call } from '../call.js';
import type {
    ConfigGetAdvancedConfigParams,
    ConfigGetParams,
    ConfigSetOutputBufferParams,
} from '../../types/generated/params.js';
import type { ConfigSetOutputBufferResponse } from '../../types/generated/responses.js';
import type { JsonValue } from '../../types/json.js';
import type {
    ReplaygainSourceMode,
    ReplaygainSourceModeName,
} from '../../types/responses.js';

/**
 * `config` — output / DSP / advanced / portable-config namespace.
 */
export const config = {
    /** The devices of every output module, in `devices`, flagged when one is in effect. */
    getOutputDevices: () => call('config.getOutputDevices'),
    /** Report the output settings in effect: module, device, buffer length, bit depth, dithering and fades. */
    getOutputConfig: () =>
        call('config.getOutputConfig'),
    /** Switch output to a device of an output module; take both GUIDs from `config.getOutputDevices`. */
    setOutputDevice: (outputId: string, deviceId: string) =>
        call('config.setOutputDevice', {
            outputId,
            deviceId,
        }),
    /**
     * Set the output buffer length.
     *
     * Accepts either a single numeric argument (milliseconds, matching
     * the original `(ms)` signature) or an options object that lets
     * callers pick the unit explicitly:
     * - `milliseconds` — buffer in ms, from 50 to 2000.
     * - `bufferLength` — buffer in seconds (the host's native unit), from
     *   0.05 to 2.
     *
     * If both fields are present the host uses `milliseconds`, though it
     * still refuses a `bufferLength` outside its range. Out-of-range values
     * and a call with neither field resolve with an `INVALID_PARAMS`
     * failure envelope.
     *
     * @param value - Numeric milliseconds, **or** an options object
     *                with `milliseconds` and/or `bufferLength`.
     */
    setOutputBuffer: ((
        value: number | ConfigSetOutputBufferParams,
    ): Promise<ConfigSetOutputBufferResponse> => {
        const opts =
            typeof value === 'number' ? { milliseconds: value } : value;
        return call(
            'config.setOutputBuffer',
            {
                ...(opts.milliseconds != null
                    ? { milliseconds: opts.milliseconds }
                    : {}),
                ...(opts.bufferLength != null
                    ? { bufferLength: opts.bufferLength }
                    : {}),
            },
        );
    }) as {
        (ms: number): Promise<ConfigSetOutputBufferResponse>;
        (opts: ConfigSetOutputBufferParams): Promise<ConfigSetOutputBufferResponse>;
    },
    /**
     * foobar2000's Advanced preferences from the root, or from below
     * `parentGuid`, in `entries`; each branch has its entries nested in
     * `children`.
     */
    getAdvancedConfig: (parentGuid?: string) =>
        call(
            'config.getAdvancedConfig',
            parentGuid ? ({ parentGuid } satisfies ConfigGetAdvancedConfigParams) : undefined,
        ),
    /** Read one Advanced preferences entry by GUID. */
    getAdvancedConfigValue: (guid: string) =>
        call(
            'config.getAdvancedConfigValue',
            { guid },
        ),
    /**
     * Write one Advanced preferences entry: a boolean for a checkbox or
     * radio entry, a string or a number for a string or integer entry.
     * A value of the wrong kind resolves with an `INVALID_PARAMS` failure
     * envelope.
     */
    setAdvancedConfigValue: (guid: string, value: JsonValue) =>
        call(
            'config.setAdvancedConfigValue',
            { guid, value },
        ),
    /** Return one Advanced preferences entry to its default value. */
    resetAdvancedConfig: (guid: string) =>
        call(
            'config.resetAdvancedConfig',
            { guid },
        ),
    /** Every preferences page, then every preferences branch, in `pages`. */
    getPreferencesPages: () => call('config.getPreferencesPages'),
    /** Report the GUIDs of foobar2000's standard preferences parents. */
    getPreferencesStandardGuids: () =>
        call(
            'config.getPreferencesStandardGuids',
        ),
    /** Report whether the media library is enabled and loaded, and how many items it holds. */
    getLibraryStatus: () =>
        call('config.getLibraryStatus'),
    /**
     * Each pattern is present only when foobar2000 has it configured, so
     * with neither configured the response is `{ success: true }` alone.
     */
    getLibraryFilePatterns: () =>
        call(
            'config.getLibraryFilePatterns',
        ),
    /** Open foobar2000's Media Library preferences page. */
    showLibraryPreferences: () =>
        call(
            'config.showLibraryPreferences',
        ),
    /** The installed components, in `components`. */
    getComponents: () => call('config.getComponents'),
    /** Report the foobar2000 version, this component's version and the profile folder. */
    getVersionInfo: () =>
        call('config.getVersionInfo'),
    /** The stored DSP presets, in `presets`. */
    getDspPresets: () => call('config.getDspPresets'),
    /** Report which DSP preset is selected. */
    getActiveDspPreset: () =>
        call('config.getActiveDspPreset'),
    /**
     * Select a preset by its `index` from {@link config.getDspPresets}. An
     * index past the end resolves with an `INVALID_INDEX` failure envelope.
     */
    setActiveDspPreset: (index: number) =>
        call('config.setActiveDspPreset', {
            index,
        }),
    // === Portable config storage ===
    /**
     * Stores any JSON value under `key`. A top-level `null` is refused
     * with an `INVALID_PARAMS` failure envelope; clear a key with
     * {@link config.remove} instead.
     */
    set: (key: string, value: JsonValue) =>
        call('config.set', {
            key,
            value,
        }),
    /**
     * Reads the value stored under `key`. A key that is not stored is not an
     * error: the response has `found: false` and `value` set to
     * `defaultValue`, or `null` when no default is given.
     *
     * @param defaultValue - Any JSON value to answer with when the key is
     *                       absent; sent as the host's `default`. Omitted
     *                       from the call when `undefined`.
     */
    get: (key: string, defaultValue?: ConfigGetParams['default']) =>
        call('config.get', {
            key,
            ...(defaultValue !== undefined ? { default: defaultValue } : {}),
        }),
    /** Delete a key from the component's configuration store. */
    remove: (key: string) =>
        call('config.remove', {
            key,
        }),
    /**
     * Full snapshot of the portable-config cache. Returns the
     * `{ success, items, configs, count }` envelope; `items` and
     * `configs` are interchangeable aliases of the same map.
     */
    getAll: () => call('config.getAll'),
    /** Read the whole store both as a map and as one JSON text. */
    export: () => call('config.export'),
    // === Cursor follow / playback follow / ReplayGain mode ===
    /** Report foobar2000's "cursor follows playback" setting. */
    getCursorFollowPlayback: () =>
        call(
            'config.getCursorFollowPlayback',
        ),
    /** Report foobar2000's "playback follows cursor" setting. */
    getPlaybackFollowCursor: () =>
        call(
            'config.getPlaybackFollowCursor',
        ),
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
        call(
            'config.getReplaygainMode',
        ),
    /** Turn foobar2000's "cursor follows playback" setting on or off. */
    setCursorFollowPlayback: (enabled: boolean) =>
        call(
            'config.setCursorFollowPlayback',
            { enabled },
        ),
    /** Turn foobar2000's "playback follows cursor" setting on or off. */
    setPlaybackFollowCursor: (enabled: boolean) =>
        call(
            'config.setPlaybackFollowCursor',
            { enabled },
        ),
    /**
     * Set the active ReplayGain source mode.
     *
     * Accepts either an integer (`0`-`3`, see `REPLAYGAIN_SOURCE_MODE`),
     * sent as `mode`, or a name, sent as `sourceMode`
     * (`'none' | 'track' | 'album' | 'byPlaybackOrder' | 'auto'`).
     * `'auto'` is treated by the host as an alias of
     * `'byPlaybackOrder'` and yields integer mode `3`.
     *
     * @param mode - Numeric mode or named alias.
     * @returns Echoes the integer mode the host applied. A value outside
     *          those accepted resolves with `success: false` and
     *          `code: 'INVALID_PARAMS'`, leaving the mode unchanged.
     */
    setReplaygainMode: (
        mode: ReplaygainSourceMode | ReplaygainSourceModeName,
    ) =>
        call(
            'config.setReplaygainMode',
            (typeof mode === 'number'
                ? { mode }
                : { sourceMode: mode }),
        ),
};
