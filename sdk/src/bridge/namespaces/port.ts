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
    connect: (name: string) =>
        call('port.connect', { name }),
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
    onMessage: (handler: (data: unknown) => void) =>
        subscribe('port:message', handler),
    onDisconnect: (handler: (data: unknown) => void) =>
        subscribe('port:disconnected', handler),
    onConnect: (handler: (data: unknown) => void) =>
        subscribe('port:connected', handler),
};
