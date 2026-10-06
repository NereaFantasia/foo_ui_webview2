/**
 * The page's one {@link SharedBufferReceiver}, created on first use and shared
 * by every SDK feature that receives shared buffers.
 */

import { SharedBufferReceiver, stringField, type SharedBufferHost } from './SharedBufferReceiver.js';

/** `additionalData.purpose` of the buffers `audio.decodePcm` posts. */
export const DECODE_PCM_PURPOSE = 'audio.decodePcm';
/** `additionalData.purpose` of the ring buffers `audio.subscribeStream` posts. */
export const STREAM_PURPOSE = 'audio.subscribeStream';

let receiver: SharedBufferReceiver | null = null;

function isSharedBufferHost(value: unknown): value is SharedBufferHost {
    return (
        typeof value === 'object' &&
        value !== null &&
        typeof Reflect.get(value, 'addEventListener') === 'function' &&
        typeof Reflect.get(value, 'removeEventListener') === 'function' &&
        typeof Reflect.get(value, 'releaseBuffer') === 'function'
    );
}

function findWebview(): SharedBufferHost | null {
    if (typeof window === 'undefined') return null;
    const chrome: unknown = Reflect.get(window, 'chrome');
    if (typeof chrome !== 'object' || chrome === null) return null;
    const webview: unknown = Reflect.get(chrome, 'webview');
    return isSharedBufferHost(webview) ? webview : null;
}

/**
 * The receiver listening on `window.chrome.webview`, or `null` outside a
 * WebView2 page, where shared buffers cannot arrive.
 */
export function getPageReceiver(): SharedBufferReceiver | null {
    if (receiver) return receiver;
    const webview = findWebview();
    if (!webview) return null;
    receiver = new SharedBufferReceiver(webview);
    receiver.definePurpose(DECODE_PCM_PURPOSE, (data) => stringField(data, 'taskId'));
    receiver.definePurpose(STREAM_PURPOSE, (data) => stringField(data, 'subscriptionId'));
    return receiver;
}
