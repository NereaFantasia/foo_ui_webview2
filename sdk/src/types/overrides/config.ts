/**
 * Hand-written type overrides for `config.{get,set}ReplaygainMode`.
 *
 * The ReplayGain source mode is an integer (`0`-`3`); the setter also accepts
 * a string alias. These unions provide autocompletion and type-checking.
 */

/**
 * ReplayGain source-mode integer. Values match the host enum:
 * `0` = none / `1` = track / `2` = album / `3` = byPlaybackOrder.
 */
export type ReplaygainSourceMode = 0 | 1 | 2 | 3;

/**
 * Named alias accepted on input by `config.setReplaygainMode`. The host
 * resolves `'auto'` to `'byPlaybackOrder'`; both yield mode `3`.
 */
export type ReplaygainSourceModeName =
    | 'none'
    | 'track'
    | 'album'
    | 'byPlaybackOrder'
    | 'auto';

/**
 * Numeric identifiers for {@link ReplaygainSourceModeName}, mirroring
 * the host's source-mode enum. Compare a
 * {@link ConfigGetReplaygainModeResponse.mode} value against entries in
 * this dictionary instead of remembering the literals:
 *
 *     const r = await config.getReplaygainMode();
 *     if (r.mode === REPLAYGAIN_SOURCE_MODE.track) { ... }
 *
 * `auto` is intentionally omitted: it is an input-only alias of
 * `byPlaybackOrder`.
 */
export const REPLAYGAIN_SOURCE_MODE = {
    none: 0,
    track: 1,
    album: 2,
    byPlaybackOrder: 3,
} as const satisfies Record<
    Exclude<ReplaygainSourceModeName, 'auto'>,
    ReplaygainSourceMode
>;

/**
 * @codegen-override response:config.getReplaygainMode
 * @codegen-snapshot mode:unknown,value:unknown
 */
export interface ConfigGetReplaygainModeResponse {
    /** Active source-mode integer. */
    mode: ReplaygainSourceMode;
    /** Alias of {@link mode} returned by the host for compatibility. */
    value: ReplaygainSourceMode;
}

/**
 * @codegen-override params:config.setReplaygainMode
 * @codegen-snapshot mode:primitive,sourceMode:primitive,value:primitive
 */
export interface ConfigSetReplaygainModeParams {
    /** Integer mode (0-3). Takes precedence over `value` and `sourceMode`. */
    mode?: ReplaygainSourceMode;
    /** Alias of {@link mode}; checked when `mode` is absent. */
    value?: ReplaygainSourceMode;
    /** Named alias; resolved to {@link mode} when neither integer is supplied. */
    sourceMode?: ReplaygainSourceModeName;
}

/**
 * @codegen-override response:config.setReplaygainMode
 * @codegen-snapshot code:primitive,error:callexpr,mode:primitive,success:primitive,value:primitive
 */
export interface ConfigSetReplaygainModeResponse {
    /** `true` when the mode was applied. */
    success?: boolean;
    /** Echo of the integer mode the host applied. */
    mode?: ReplaygainSourceMode;
    /** Alias of {@link mode}. */
    value?: ReplaygainSourceMode;
    /** Human-readable error when `sourceMode` does not match a known alias. */
    error?: string;
    /** Stable error code; `'INVALID_PARAMS'` for an unknown `sourceMode`. */
    code?: string;
}
