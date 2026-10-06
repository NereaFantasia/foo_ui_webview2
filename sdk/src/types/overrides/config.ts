/**
 * ReplayGain source-mode unions and constants for `config.getReplaygainMode`
 * and `config.setReplaygainMode`.
 *
 * The host reports the source mode as an integer (`0`-`3`); the setter also
 * accepts a name. These unions give both forms autocompletion and
 * type-checking.
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
 * the host's source-mode enum. Compare the `mode` that
 * `config.getReplaygainMode` reports against entries in this dictionary
 * instead of remembering the literals:
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
