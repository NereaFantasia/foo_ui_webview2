/**
 * `titleformat` — title-format script evaluation namespace.
 */

import { bridge } from '../Bridge.js';

import type {
    TitleformatBatchResult,
    TitleformatBuiltinFields,
    TitleformatEvalResult,
    TitleformatFieldsBatchResult,
    TitleformatFieldsResult,
} from '../../types/responses.js';

/**
 * `fields` is a `{ fieldName: pattern }` map mirroring the C++
 * `params["fields"]` object. Returned envelope carries one value per
 * field name plus `path` / `success`.
 */
export type TitleformatFieldMap = Record<string, string>;

export const titleformat = {
    /**
     * Evaluate a single pattern against one track.
     *
     * `infoAvailable: false` means the track's metadb info was not ready,
     * so tag-derived output is untrustworthy. See the SDK docs for what
     * the flag does not cover.
     */
    eval: (pattern: string, path?: string) =>
        bridge.invoke<TitleformatEvalResult>('titleformat.eval', {
            pattern,
            ...(path ? { path } : {}),
        }),
    /**
     * Batch variant of `eval()`. Each row carries its own
     * `infoAvailable` flag; rows that failed omit it.
     */
    evalBatch: (pattern: string, paths: string[]) =>
        bridge.invoke<TitleformatBatchResult>('titleformat.evalBatch', {
            pattern,
            paths,
        }),
    /**
     * Evaluate one or more named patterns against a single track. The
     * `fields` argument maps each output key to a titleformat pattern
     * string (e.g. `{ artist: '%artist%', year: '$year(%date%)' }`).
     *
     * `infoAvailable: false` means tag-derived values are untrustworthy.
     * One flag covers the whole merged script and never covers
     * foo_playcount virtual fields — see the SDK docs for the full
     * limitation. A `fields` key named `infoAvailable` overwrites the
     * flag, matching the existing behaviour of `path` and `success`.
     */
    evalFields: (path: string, fields: TitleformatFieldMap) =>
        bridge.invoke<TitleformatFieldsResult>('titleformat.evalFields', {
            path,
            fields,
        }),
    /**
     * Batch variant of {@link evalFields}. Compiles the merged pattern
     * once and applies it to every path — host-side optimisation gives
     * roughly 10× speedup vs. calling {@link evalFields} per track.
     *
     * Each row carries its own `infoAvailable` flag with the same meaning
     * and the same merged-script limitation as {@link evalFields}.
     */
    evalFieldsBatch: (paths: string[], fields: TitleformatFieldMap) =>
        bridge.invoke<TitleformatFieldsBatchResult>(
            'titleformat.evalFieldsBatch',
            { paths, fields },
        ),
    getBuiltinFields: () =>
        bridge.invoke<TitleformatBuiltinFields>('titleformat.getBuiltinFields'),
};
