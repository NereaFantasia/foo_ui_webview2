/**
 * The parameter keys the host accepts for each declared method, as runtime data, and a check
 * that applies them the way the host does. Meant for test doubles and tools that stand in for
 * the host: a double that answers from {@link findParamKeyProblem} refuses the same calls as
 * the real host, without keeping a hand-written copy of the declarations.
 *
 * Only key names are checked here. Value types, ranges and path security are checked by the
 * host as well, and a call that passes {@link findParamKeyProblem} can still fail on those.
 *
 * @packageDocumentation
 */

import { API_PARAM_SHAPES, PARAM_SHAPE_TYPES } from '../types/generated/param-shapes.js';
import type { ParamShape } from '../types/generated/param-shapes.js';
import type { JsonObject } from '../types/json.js';

export { API_PARAM_SHAPES, PARAM_SHAPE_TYPES };
export type { ParamShape };

/** Why the host would refuse a call before its handler runs. */
export interface ParamKeyProblem {
    /** Where the offending key sits, in the host's notation, for example `items[2].path`; empty for the whole parameter object. */
    readonly path: string;
    /**
     * `unknown`: the method declares no such key. `missing`: a required key is absent or
     * `null`. `notObject`: the parameters, or a value that has to be an object, are not a
     * plain object.
     */
    readonly reason: 'unknown' | 'missing' | 'notObject';
}

const isPlainObject = (value: unknown): value is JsonObject =>
    typeof value === 'object' && value !== null && !Array.isArray(value);

const hasOwn = (object: object, key: string): boolean => Object.prototype.hasOwnProperty.call(object, key);

const join = (path: string, key: string): string => (path ? `${path}.${key}` : key);

// The host's generated parser first refuses keys the object does not declare, then reads the
// declared keys in declaration order, descending into a nested object where it reads one.
function checkObject(value: unknown, shape: ParamShape, path: string): ParamKeyProblem | null {
    if (!isPlainObject(value)) return { path, reason: 'notObject' };
    // Keys starting with `_` are the bridge's own and pass at every level. A key whose value is
    // `undefined` never reaches the host: the params travel as JSON, which drops it.
    if (!shape.open) {
        for (const key of Object.keys(value)) {
            if (value[key] === undefined) continue;
            if (!key.startsWith('_') && !hasOwn(shape.keys, key)) {
                return { path: join(path, key), reason: 'unknown' };
            }
        }
    }
    const required = shape.required ?? [];
    for (const [key, nested] of Object.entries(shape.keys)) {
        const child = value[key];
        if (child === undefined || child === null) {
            if (required.includes(key)) return { path: join(path, key), reason: 'missing' };
            continue;
        }
        if (nested === null) continue;
        const childShape = typeof nested === 'string' ? PARAM_SHAPE_TYPES[nested] : nested;
        if (!childShape) continue;
        const problem = Array.isArray(child)
            ? checkElements(child, childShape, join(path, key))
            : checkObject(child, childShape, join(path, key));
        if (problem) return problem;
    }
    return null;
}

function checkElements(items: readonly unknown[], shape: ParamShape, path: string): ParamKeyProblem | null {
    for (const [index, item] of items.entries()) {
        const problem = checkObject(item, shape, `${path}[${index}]`);
        if (problem) return problem;
    }
    return null;
}

/**
 * Checks the keys of `params` against the declaration of `method` the way the host does
 * before the handler runs, and returns the first key the host would refuse the call for, or
 * `null` when every key passes.
 *
 * Keys starting with `_` pass at every level, because the bridge adds its own under such
 * names. A key whose value is `null` counts as absent, as it does for the host; so does one
 * whose value is `undefined`, since the params travel as JSON, which leaves such keys out. Omitted
 * `params` count as an empty object. A method without a declaration (the host registers a
 * few, such as `test.echo`) has no shape and always passes.
 *
 * @param method - Method name as passed to `invoke`, for example `playlist.getTracks`.
 * @param params - The parameters as the page would send them.
 * @returns The first problem in the host's check order, or `null`: an object's undeclared
 *   keys first, then its declared keys in declaration order, a nested object where it occurs.
 */
export function findParamKeyProblem(method: string, params: unknown): ParamKeyProblem | null {
    if (!hasOwn(API_PARAM_SHAPES, method)) return null;
    const shape = API_PARAM_SHAPES[method as keyof typeof API_PARAM_SHAPES];
    return checkObject(params === undefined || params === null ? {} : params, shape, '');
}
