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
    eval: (pattern: string, path?: string) =>
        bridge.invoke<TitleformatEvalResult>('titleformat.eval', {
            pattern,
            ...(path ? { path } : {}),
        }),
    evalBatch: (pattern: string, paths: string[]) =>
        bridge.invoke<TitleformatBatchResult>('titleformat.evalBatch', {
            pattern,
            paths,
        }),
    /**
     * Evaluate one or more named patterns against a single track. The
     * `fields` argument maps each output key to a titleformat pattern
     * string (e.g. `{ artist: '%artist%', year: '$year(%date%)' }`).
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
     */
    evalFieldsBatch: (paths: string[], fields: TitleformatFieldMap) =>
        bridge.invoke<TitleformatFieldsBatchResult>(
            'titleformat.evalFieldsBatch',
            { paths, fields },
        ),
    getBuiltinFields: () =>
        bridge.invoke<TitleformatBuiltinFields>('titleformat.getBuiltinFields'),
};
