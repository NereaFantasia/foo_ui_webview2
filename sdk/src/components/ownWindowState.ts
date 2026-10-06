/**
 * Narrows `window:stateChanged` down to the component's own window.
 *
 * The host sends the event to every window, and its `windowId` names
 * the window whose state changed. The own window's id comes from
 * `window.getCurrentWindowId`, asked once per connection.
 */

import { getFb } from './runtime.js';

/** The part of a `window:stateChanged` payload the filter reads. */
export interface WindowIdentified {
    /** Window whose state changed; absent when the host does not send it. */
    windowId?: string;
}

/**
 * Delivers the `window:stateChanged` payloads that concern the own
 * window.
 *
 * - An event without `windowId` comes from a host that does not send
 *   it and is delivered at once, as before the field existed.
 * - Until the own id is known, the latest event of each window is
 *   held; once it is known, the own window's held event is delivered
 *   and the others are dropped. From then on only the own window's
 *   events pass.
 * - When the id cannot be learned, the latest held event and every
 *   later event are delivered, which is the behavior of a component
 *   that does not filter.
 */
export class OwnWindowStateFilter<T extends WindowIdentified> {
    private _mode: 'pending' | 'own' | 'all' = 'pending';
    private _ownId = '';
    private readonly _held = new Map<string, T>();
    private _latestHeld: T | undefined;

    constructor(private readonly _deliver: (state: T) => void) {}

    /** Pass on one incoming event, or hold it while the own id is unknown. */
    push(state: T): void {
        const id = state?.windowId;
        if (typeof id !== 'string' || this._mode === 'all') {
            this._deliver(state);
            return;
        }
        if (this._mode === 'pending') {
            this._held.set(id, state);
            this._latestHeld = state;
            return;
        }
        if (id === this._ownId) this._deliver(state);
    }

    /** Take the own window's id and deliver the event held for it, if any. Only the first call counts. */
    resolve(ownId: string): void {
        if (this._mode !== 'pending') return;
        this._mode = 'own';
        this._ownId = ownId;
        const held = this._held.get(ownId);
        this._release();
        if (held) this._deliver(held);
    }

    /** Give up on the own id: deliver the latest held event and all later ones. Ignored once resolved. */
    fail(): void {
        if (this._mode !== 'pending') return;
        this._mode = 'all';
        const latest = this._latestHeld;
        this._release();
        if (latest) this._deliver(latest);
    }

    private _release(): void {
        this._held.clear();
        this._latestHeld = undefined;
    }
}

/**
 * Ask the host for the calling page's window id and settle `filter`
 * with the answer: `resolve` with the id, or `fail` when the call is
 * rejected or the answer carries no id.
 */
export function learnOwnWindowId<T extends WindowIdentified>(filter: OwnWindowStateFilter<T>): void {
    let fb: ReturnType<typeof getFb>;
    try {
        fb = getFb();
    } catch {
        filter.fail();
        return;
    }
    fb.ui
        .getCurrentWindowId()
        .then((r) => {
            const id = (r as WindowIdentified | null | undefined)?.windowId;
            if (typeof id === 'string' && id !== '') filter.resolve(id);
            else filter.fail();
        })
        .catch(() => filter.fail());
}
