/**
 * `FbProfiler` — Simple wall-clock stopwatch.
 *
 * Mirrors the SMP `FbProfiler` API (`Time` getter, `Reset()`,
 * `Print()`).
 *
 * Implementation prefers `performance.now()` when available (sub-ms
 * resolution) and falls back to `Date.now()` for environments without
 * a high-resolution timer.
 */

function _now(): number {
    if (
        typeof performance !== 'undefined' &&
        typeof performance.now === 'function'
    ) {
        return performance.now();
    }
    return Date.now();
}

export class FbProfiler {
    private _name: string;
    private _start: number;

    constructor(name?: string) {
        this._name = String(name ?? 'Profiler');
        this._start = _now();
    }

    /** Elapsed time since construction / last `Reset()` in milliseconds. */
    get Time(): number {
        return Math.round(_now() - this._start);
    }

    /** Restart the wall-clock baseline. */
    Reset(): void {
        this._start = _now();
    }

    /** Log the current elapsed time using the configured profiler name. */
    Print(): void {
        // eslint-disable-next-line no-console
        console.log(`[Profiler] ${this._name}: ${this.Time}ms`);
    }
}
