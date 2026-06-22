/**
 * `FbMetadbHandleList` — ordered collection of {@link FbMetadbHandle}.
 *
 * Matches the Spider Monkey Panel handle-list shape: indexed access
 * via `list[i]`, a `Count` getter, mutation via
 * `Add` / `AddRange` / `Remove*`, and helpers for total duration / file
 * size. Indexed read-write is implemented with a `Proxy` so consumer
 * code that relies on `list[0] = newHandle` keeps working.
 *
 * Implements the SMP runtime contract; the only TS-specific addition
 * is the `Iterable<FbMetadbHandle>` declaration so `for ... of` is
 * automatically typed.
 */

import { FbMetadbHandle } from './FbMetadbHandle.js';
import type { SmpHandleLike } from '../types.js';

type HandleInput = SmpHandleLike | FbMetadbHandle;
type HandleListLike =
    | FbMetadbHandleList
    | HandleInput[]
    | Iterable<HandleInput>
    | null
    | undefined;

function _toHandle(item: HandleInput): FbMetadbHandle {
    return item instanceof FbMetadbHandle ? item : new FbMetadbHandle(item);
}

/**
 * Internal Proxy handler factory. The list itself extends a real class
 * but the public-facing object is a Proxy that translates numeric
 * string keys into `_items` access — the SMP `list[i]` syntax that
 * theme code expects.
 */
function _buildProxy(target: FbMetadbHandleList): FbMetadbHandleList {
    return new Proxy(target, {
        get(t, prop, receiver) {
            if (prop === 'length') {
                // Expose `length` for convenience even though the canonical
                // SMP API uses `Count` — many themes read both.
                return t.Count;
            }
            if (typeof prop === 'string' && /^[0-9]+$/.test(prop)) {
                return (t as unknown as { _items: FbMetadbHandle[] })._items[Number(prop)];
            }
            const val = Reflect.get(t, prop, receiver) as unknown;
            return typeof val === 'function' ? (val as Function).bind(t) : val;
        },
        set(t, prop, value) {
            if (typeof prop === 'string' && /^[0-9]+$/.test(prop)) {
                const idx = Number(prop);
                const items = (t as unknown as { _items: FbMetadbHandle[] })._items;
                if (idx < 0 || idx >= items.length) return false;
                items[idx] = _toHandle(value as HandleInput);
                return true;
            }
            (t as unknown as Record<PropertyKey, unknown>)[prop] = value;
            return true;
        },
        has(t, prop) {
            if (typeof prop === 'string' && /^[0-9]+$/.test(prop)) {
                const idx = Number(prop);
                const items = (t as unknown as { _items: FbMetadbHandle[] })._items;
                return idx >= 0 && idx < items.length;
            }
            return prop in t;
        },
    });
}

export class FbMetadbHandleList implements Iterable<FbMetadbHandle> {
    private _items: FbMetadbHandle[] = [];

    constructor(items?: HandleListLike) {
        if (items) this.AddRange(items);
        // The proxy is what callers receive so that numeric-index access
        // (`list[0]`) works transparently like an array.
        return _buildProxy(this) as FbMetadbHandleList;
    }

    /** Length getter (SMP-style). */
    get Count(): number {
        return this._items.length;
    }

    Add(handle: HandleInput): boolean {
        this._items.push(_toHandle(handle));
        return true;
    }

    AddRange(list: HandleListLike): boolean {
        if (!list) return false;

        if (list instanceof FbMetadbHandleList) {
            for (const h of list._items) this._items.push(_toHandle(h));
            return true;
        }

        if (Array.isArray(list)) {
            for (const h of list) this._items.push(_toHandle(h));
            return true;
        }

        const iterable = list as Iterable<HandleInput>;
        if (typeof iterable[Symbol.iterator] === 'function') {
            for (const h of iterable) this._items.push(_toHandle(h));
            return true;
        }

        return false;
    }

    /** Shallow copy (handles themselves are shared by reference). */
    Clone(): FbMetadbHandleList {
        const out = new FbMetadbHandleList();
        out.AddRange(this._items);
        return out;
    }

    /** Remove the first handle that compares equal; returns `true` on hit. */
    Remove(handle: HandleInput): boolean {
        const idx = this.Find(handle);
        if (idx >= 0) {
            this._items.splice(idx, 1);
            return true;
        }
        return false;
    }

    RemoveAll(): boolean {
        this._items.length = 0;
        return true;
    }

    /** Remove by absolute index; returns `false` on out-of-range. */
    RemoveById(index: number): boolean {
        const idx = index | 0;
        if (idx < 0 || idx >= this._items.length) return false;
        this._items.splice(idx, 1);
        return true;
    }

    /** First-match index by handle equality (path + subsong). */
    Find(handle: HandleInput): number {
        const h = _toHandle(handle);
        for (let i = 0; i < this._items.length; i++) {
            if (this._items[i].Compare(h)) return i;
        }
        return -1;
    }

    /** Snapshot copy as a plain array. */
    Convert(): FbMetadbHandle[] {
        return this._items.slice();
    }

    /** Sum of `Length` across all handles (seconds). */
    CalcTotalDuration(): number {
        let total = 0;
        for (const h of this._items) total += h.Length || 0;
        return total;
    }

    /** Sum of `FileSize` across all handles (bytes). */
    CalcTotalSize(): number {
        let total = 0;
        for (const h of this._items) total += h.FileSize || 0;
        return total;
    }

    [Symbol.iterator](): Iterator<FbMetadbHandle> {
        return this._items[Symbol.iterator]();
    }

    // Numeric index signature — declared for TypeScript ergonomics; the
    // actual lookup is implemented by the Proxy returned from the
    // constructor.
    [index: number]: FbMetadbHandle;
}
