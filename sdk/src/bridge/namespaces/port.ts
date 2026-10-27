import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import type { JsonValue } from '../../types/json.js';

/**
 * `port` — cross-window named-channel messaging hub.
 *
 * Use {@link port.connect} to obtain a `portId`; subsequent
 * {@link port.postMessage} / {@link port.postMessageTo} calls route via
 * that handle. Receivers subscribe with {@link port.onMessage}.
 */
export const port = {
    /** Open a port on a named channel for the calling window and announce it with `port:connected`. */
    connect: (name: string) =>
        call('port.connect', { name }),
    /** Close a port and announce it with `port:disconnected`. */
    disconnect: (portId: string) =>
        call('port.disconnect', {
            portId,
        }),
    /** `null` is not a message; the host refuses it as missing. */
    postMessage: (portId: string, message: JsonValue) =>
        call('port.postMessage', {
            portId,
            message,
        }),
    /** Send a message to one port, as a `port:message` event to that port's window. */
    postMessageTo: (
        portId: string,
        targetPortId: string,
        message: JsonValue,
    ) =>
        call('port.postMessageTo', {
            portId,
            targetPortId,
            message,
        }),
    /** Omit `name` to list the ports of every channel. */
    getPorts: (name?: string) =>
        call(
            'port.getPorts',
            name === undefined ? {} : { name },
        ),
    /** Subscribe to incoming messages on ports. Returns an unsubscribe function. */
    onMessage: (handler: (data: unknown) => void) =>
        subscribe('port:message', handler),
    /** Subscribe to port disconnection events. Returns an unsubscribe function. */
    onDisconnect: (handler: (data: unknown) => void) =>
        subscribe('port:disconnected', handler),
    /** Subscribe to port connection events. Returns an unsubscribe function. */
    onConnect: (handler: (data: unknown) => void) =>
        subscribe('port:connected', handler),
};
