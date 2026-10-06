import { call } from '../call.js';


/**
 * `fields` is a `{ fieldName: pattern }` map mirroring the C++
 * `params["fields"]` object. Returned envelope carries one value per
 * field name plus `path` / `success`.
 */
export type TitleformatFieldMap = Record<string, string>;

/**
 * `titleformat` — title-format script evaluation namespace.
 */
export const titleformat = {
    /**
     * Evaluate a single pattern against one track, or against the playing
     * track when `path` is omitted or empty. Evaluating the playing track
     * also fills dynamic fields such as `%playback_time%` and stream
     * titles; with nothing playing it resolves `success: false` with
     * `code: 'NO_ACTIVE_ITEM'`.
     *
     * `infoAvailable: false` means the host could not get the track's info
     * (a remote path with nothing cached, or an unreadable file), so
     * tag-derived output is untrustworthy. See the SDK docs for what the
     * flag does not cover.
     */
    eval: (pattern: string, path?: string) =>
        call('titleformat.eval', {
            pattern,
            ...(path ? { path } : {}),
        }),
    /**
     * Batch variant of `eval()`. Each row carries its own
     * `infoAvailable` flag; rows that failed omit it.
     */
    evalBatch: (pattern: string, paths: string[]) =>
        call('titleformat.evalBatch', {
            pattern,
            paths,
        }),
    /**
     * Evaluate one or more named patterns against a single track. The
     * `fields` argument maps each output key to a titleformat pattern
     * string (e.g. `{ artist: '%artist%', year: '$year(%date%)' }`).
     *
     * `infoAvailable: false` means tag-derived values are untrustworthy,
     * under the same conditions as {@link eval}. One flag covers the whole
     * merged script and never covers
     * foo_playcount virtual fields — see the SDK docs for the full
     * limitation. Keys of `fields` named `success`, `path` or
     * `infoAvailable` are dropped because the response fields of those
     * names take precedence; keys named `error` or `code` are kept, so
     * check `success` to tell a failure.
     */
    evalFields: (path: string, fields: TitleformatFieldMap) =>
        call('titleformat.evalFields', {
            path,
            fields,
        }),
    /**
     * Batch variant of {@link evalFields}. Compiles the merged pattern
     * once and applies it to every path — host-side optimisation gives
     * roughly 10× speedup vs. calling {@link evalFields} per track.
     *
     * Each row carries its own `infoAvailable` flag with the same meaning
     * and the same merged-script limitation as {@link evalFields}. Keys of
     * `fields` named `path`, `success` or `infoAvailable` are dropped from
     * each row; a key named `error` is kept, so check the row's `success`.
     */
    evalFieldsBatch: (paths: string[], fields: TitleformatFieldMap) =>
        call(
            'titleformat.evalFieldsBatch',
            { paths, fields },
        ),
    getBuiltinFields: () =>
        call('titleformat.getBuiltinFields'),
};
