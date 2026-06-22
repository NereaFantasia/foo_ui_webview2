/**
 * `port` — cross-window named-channel messaging hub.
 *
 * Use {@link port.connect} to obtain a `portId`; subsequent
 * {@link port.postMessage} / {@link port.postMessageTo} calls route via
 * that handle. Receivers subscribe with {@link port.onMessage}.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

export const port = {
    connect: (name: string) =>
        bridge.invoke<{ portId: string }>('port.connect', { name }),
    disconnect: (portId: string) =>
        bridge.invoke<BaseResponse>('port.disconnect', { portId }),
    postMessage: (portId: string, message: unknown) =>
        bridge.invoke<BaseResponse>('port.postMessage', { portId, message }),
    postMessageTo: (
        portId: string,
        targetPortId: string,
        message: unknown,
    ) =>
        bridge.invoke<BaseResponse>('port.postMessageTo', {
            portId,
            targetPortId,
            message,
        }),
    getPorts: (name: string) =>
        bridge.invoke<{ ports: unknown[] }>('port.getPorts', { name }),
    onMessage: (handler: (data: unknown) => void) =>
        bridge.on('port:message', handler),
    onDisconnect: (handler: (data: unknown) => void) =>
        bridge.on('port:disconnected', handler),
    onConnect: (handler: (data: unknown) => void) =>
        bridge.on('port:connected', handler),
};
