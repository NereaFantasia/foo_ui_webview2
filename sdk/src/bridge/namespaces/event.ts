/**
 * `event` — custom event broadcast / direct delivery namespace.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

export const event = {
    /** Broadcast an event to every connected window (optionally excluding self). */
    emit: (eventName: string, payload?: unknown, excludeSelf = false) =>
        bridge.invoke<BaseResponse>('event.emit', {
            event: eventName,
            payload,
            excludeSelf,
        }),
    /** Deliver an event to a specific window by id. */
    emitTo: (eventName: string, payload: unknown, targetWindowId: string) =>
        bridge.invoke<BaseResponse>('event.emitTo', {
            event: eventName,
            payload,
            targetWindowId,
        }),
};
