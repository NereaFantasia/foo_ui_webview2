/**
 * Event subscriptions used by the namespace facades, restricted to the declared
 * events: the name must be an {@link FBEventName}, so a misspelt or undeclared
 * name does not compile and the handler receives the declared payload type. The
 * public `bridge.on` / `bridge.once` also accept custom event names.
 */

import { bridge, type FBEventHandler } from './Bridge.js';
import type { FBEventName } from '../types/events.js';

/** Subscribe to a declared event through the shared bridge; returns the unsubscribe callback. */
export function subscribe<K extends FBEventName>(event: K, handler: FBEventHandler<K>): () => void {
    return bridge.on(event, handler);
}

/** Subscribe to the next delivery of a declared event; returns the unsubscribe callback. */
export function subscribeOnce<K extends FBEventName>(event: K, handler: FBEventHandler<K>): () => void {
    return bridge.once(event, handler);
}
