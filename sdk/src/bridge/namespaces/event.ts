import { call } from '../call.js';
import type { JsonValue } from '../../types/json.js';

/**
 * `event` — custom event broadcast / direct delivery namespace.
 */
export const event = {
    /**
     * Broadcast an event to every connected window (optionally excluding self).
     * Receivers get `{ payload, sourceWindowId }` under `eventName`.
     */
    emit: (eventName: string, payload?: JsonValue, excludeSelf = false) =>
        call('event.emit', {
            event: eventName,
            payload,
            excludeSelf,
        }),
    /**
     * Deliver an event to a specific window by id. An id no open window has
     * fails with `code: 'NOT_FOUND'`.
     */
    emitTo: (eventName: string, payload: JsonValue, targetWindowId: string) =>
        call('event.emitTo', {
            event: eventName,
            payload,
            targetWindowId,
        }),
};
