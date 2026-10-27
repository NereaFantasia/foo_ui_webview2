import type { JsonValue } from '../../types/json.js';
import type { JsonObject } from '../../types/json.js';

interface FbFileInfoInput {
    tags?: JsonObject;
    meta?: JsonObject;
    info?: JsonObject;
    [key: string]: JsonValue;
}

function _normalizeArray(value: unknown): string[] {
    if (Array.isArray(value)) {
        return value.map((v) => (v == null ? '' : String(v)));
    }
    if (value == null) return [''];
    return [String(value)];
}

function _findIndexCaseInsensitive(list: readonly string[], name: string): number {
    if (!name) return -1;
    const target = String(name).toLowerCase();
    for (let i = 0; i < list.length; i++) {
        if (String(list[i]).toLowerCase() === target) return i;
    }
    return -1;
}

/**
 * `FbFileInfo` — meta + technical-info accessor.
 *
 * Wraps the JSON envelope returned by `metadata.read` so SMP-style
 * scripts can use `MetaCount` / `MetaFind(name)` / `MetaValue(...)` to
 * inspect tags, and `InfoCount` / `InfoFind(name)` / `InfoValue(idx)`
 * for technical info (codec / sample rate / etc.).
 *
 * Lookups are case-insensitive to match the SMP runtime behaviour.
 */
export class FbFileInfo {
    private readonly _metaNames: string[] = [];
    private readonly _metaValues: string[][] = [];
    private readonly _infoNames: string[] = [];
    private readonly _infoValues: string[] = [];

    constructor(input?: FbFileInfoInput | unknown) {
        const src = (input ?? {}) as FbFileInfoInput;
        const tags = (src.tags ?? src.meta ?? {}) as JsonObject;
        const info = (src.info ?? {}) as JsonObject;

        if (tags && typeof tags === 'object') {
            for (const key of Object.keys(tags)) {
                this._metaNames.push(key);
                this._metaValues.push(_normalizeArray(tags[key]));
            }
        }

        if (info && typeof info === 'object') {
            for (const key of Object.keys(info)) {
                this._infoNames.push(key);
                this._infoValues.push(info[key] == null ? '' : String(info[key]));
            }
        }
    }

    /** Number of distinct tag fields. */
    get MetaCount(): number {
        return this._metaNames.length;
    }

    /** Number of distinct technical-info fields. */
    get InfoCount(): number {
        return this._infoNames.length;
    }

    /** Case-insensitive tag-name lookup; returns `-1` when absent. */
    MetaFind(name: string): number {
        return _findIndexCaseInsensitive(this._metaNames, name);
    }

    /** Case-insensitive info-name lookup; returns `-1` when absent. */
    InfoFind(name: string): number {
        return _findIndexCaseInsensitive(this._infoNames, name);
    }

    /** Get the name of the meta tag at the given zero-based index. */
    MetaName(index: number): string {
        return this._metaNames[index] || '';
    }

    /** Get the name of the technical info field at the given zero-based index. */
    InfoName(index: number): string {
        return this._infoNames[index] || '';
    }

    /** Number of distinct values stored under `MetaName(index)`. */
    MetaValueCount(index: number): number {
        const values = this._metaValues[index];
        return values ? values.length : 0;
    }

    /** Read the `valueIndex`-th value of meta field `index`. */
    MetaValue(index: number, valueIndex: number): string {
        const values = this._metaValues[index];
        if (!values || valueIndex == null) return '';
        return values[valueIndex] || '';
    }

    /** Get the value of the technical info field at the given zero-based index. */
    InfoValue(index: number): string {
        return this._infoValues[index] || '';
    }
}
