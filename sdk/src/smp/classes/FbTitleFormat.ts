import type {
    TitleformatEvalBatchParams,
    TitleformatEvalParams,
} from '../../types/generated/params.js';
import type { JsonValue } from '../../types/json.js';
/**
 * `FbTitleFormat` — title-format script accessor.
 *
 * Mirrors the SMP API: a wrapper around a single titleformat
 * expression that can be evaluated against the current track
 * (`Eval`), a single handle (`EvalWithMetadb`), or a list
 * (`EvalWithMetadbs`).
 *
 * Backend mapping:
 * - `Eval` / `EvalWithMetadb` → `titleformat.eval` (`{ path, pattern }`).
 * - `EvalWithMetadbs` → `titleformat.evalBatch` (`{ paths, pattern }`).
 *
 * Each path is the track's handle id, `|subsong:N` suffix included.
 *
 * The single-pattern shape is preserved (rather than upgraded to the
 * newer `titleformat.evalFields` API) because SMP scripts depend on
 * the one-expression-per-call contract.
 */

import { getTypedInvoke, successOf, toHandleId } from '../utils.js';
import type { SmpHandleLike } from '../types.js';
import { FbMetadbHandleList } from './FbMetadbHandleList.js';

interface FbTitleFormatEvalResponse {
    success?: boolean;
    result?: string;
    [key: string]: JsonValue;
}

interface FbTitleFormatBatchResponse {
    results?: Array<{
        success?: boolean;
        result?: string;
        [key: string]: JsonValue;
    }>;
    [key: string]: JsonValue;
}

/**
 * The track key sent as `path`: the handle id, keeping any `|subsong:N`
 * suffix so a subsong of a multi-track file (a CUE sheet, for one) is
 * evaluated as itself rather than as the file's first subsong.
 */
function _getPathFromMetadb(handleLike: SmpHandleLike | unknown): string {
    return toHandleId(handleLike);
}

function _collectPaths(list: unknown): string[] {
    const paths: string[] = [];
    if (!list) return paths;

    if (list instanceof FbMetadbHandleList) {
        for (const h of list) {
            const p = _getPathFromMetadb(h);
            if (p) paths.push(p);
        }
        return paths;
    }

    if (Array.isArray(list)) {
        for (const h of list) {
            const p = _getPathFromMetadb(h);
            if (p) paths.push(p);
        }
        return paths;
    }

    const iterable = list as Iterable<unknown>;
    if (typeof iterable[Symbol.iterator] === 'function') {
        for (const h of iterable) {
            const p = _getPathFromMetadb(h);
            if (p) paths.push(p);
        }
    }

    return paths;
}

function _requireInvoke(): NonNullable<ReturnType<typeof getTypedInvoke>> {
    const inv = getTypedInvoke();
    if (!inv) {
        throw new Error('[SMP] smp.invoke is not available. Load sdk/smp-compat.js first.');
    }
    return inv;
}

/** SMP-style cache shape we read for the now-playing track in `Eval()`. */
interface SmpCacheShape {
    currentTrack?: SmpHandleLike | unknown;
}

interface SmpHostShape {
    cache?: SmpCacheShape;
}

export class FbTitleFormat {
    private readonly _expr: string;

    constructor(expression: string) {
        this._expr = String(expression || '');
    }

    get Expression(): string {
        return this._expr;
    }

    /**
     * Evaluate against the current now-playing track. Returns the empty
     * string when nothing is playing or the bridge is unavailable.
     */
    async Eval(_force?: boolean): Promise<string> {
        const cache = (globalThis as { smp?: SmpHostShape }).smp?.cache;
        const path = _getPathFromMetadb(cache?.currentTrack ?? null);
        if (!path) return '';

        const inv = _requireInvoke();
        const res = successOf(await inv('titleformat.eval', {
            path,
            pattern: this._expr,
        } satisfies TitleformatEvalParams));
        return res?.result ?? '';
    }

    /** Evaluate against a single handle / track-info object. */
    async EvalWithMetadb(handleLike: SmpHandleLike | unknown): Promise<string> {
        const path = _getPathFromMetadb(handleLike);
        if (!path) return '';

        const inv = _requireInvoke();
        const res = successOf(await inv('titleformat.eval', {
            path,
            pattern: this._expr,
        } satisfies TitleformatEvalParams));
        return res?.result ?? '';
    }

    /** Batch evaluate against a list of handles. */
    async EvalWithMetadbs(handleListLike: unknown): Promise<string[]> {
        const paths = _collectPaths(handleListLike);
        if (paths.length === 0) return [];

        const inv = _requireInvoke();
        const res = successOf(await inv('titleformat.evalBatch', {
            paths,
            pattern: this._expr,
        } satisfies TitleformatEvalBatchParams));
        const results = res?.results;
        if (!Array.isArray(results)) return [];

        return results.map((r) => {
            if (!r || r.success === false) return '';
            return r.result || '';
        });
    }

    toString(): string {
        return this._expr;
    }
}
