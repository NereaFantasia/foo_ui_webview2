/**
 * `http` — HTTP client routed through the host process.
 *
 * Bypasses the WebView2 sandbox so consumers can talk to file://,
 * localhost, or CORS-restricted origins.
 *
 * Body-bearing verbs (`post` / `put` / `delete` / `patch`) accept the
 * body as a positional argument; the host serialises non-string bodies
 * to JSON before dispatch.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';
import type { HttpDownloadCompletePayload } from '../../types/events.js';
import type { JsonObject, JsonValue } from '../../types/json.js';

/**
 * Default failure logger for `http:downloadComplete`.
 *
 * `http.download` returns immediately with a `requestId` and signals its
 * eventual outcome via `http:downloadComplete`. Without a subscriber,
 * download failures silently disappear; this module-level handler
 * surfaces non-cancelled failures into `console.warn` so theme authors
 * notice them during development.
 *
 * Theme code wanting custom handling can detach the default logger via
 * {@link disableDefaultHttpDownloadLogger}, register its own
 * `bridge.on('http:downloadComplete', ...)` listener, or both.
 */
let _defaultHttpDownloadLoggerOff: (() => void) | null = bridge.on(
    'http:downloadComplete',
    (event: HttpDownloadCompletePayload) => {
        if (event && event.success === false && !event.cancelled) {
            console.warn('[fb.http] download failed:', {
                requestId: event.requestId,
                path: event.path,
                status: event.status,
                error: event.error,
                bytesWritten: event.bytesWritten,
            });
        }
    },
);

/**
 * Detach the default `http:downloadComplete` logger installed at module
 * load. Idempotent; safe to call from theme bootstrap before installing
 * a custom toast / progress-bar handler.
 */
export function disableDefaultHttpDownloadLogger(): void {
    if (_defaultHttpDownloadLoggerOff) {
        _defaultHttpDownloadLoggerOff();
        _defaultHttpDownloadLoggerOff = null;
    }
}

/**
 * Options shared across `http.get` / `http.post` / ... verbs.
 *
 * Mirrors the host-recognised fields (`timeout`, `async`, `redirect`,
 * `responseType`) plus typed headers. Defaults: async dispatch with a
 * 30 s timeout, `follow` redirects, `text` response type.
 */
export interface HttpRequestOptions {
    headers?: Record<string, string>;
    /** Host-side timeout in ms; default 30000. */
    timeout?: number;
    /** Set `false` for synchronous response; default `true`. */
    async?: boolean;
    /** Redirect handling: `'follow'` (default) or `'manual'`. */
    redirect?: string;
    /**
     * Response decoding hint:
     * - `'text'` (default) — UTF-8 string body. Rejects when the response
     *   contains non-UTF-8 bytes; use a binary mode for arbitrary bytes
     *   such as `.wasm` modules or images.
     * - `'base64'` — body returned as a base64-encoded string.
     * - `'arraybuffer'` / `'binary'` — `body` is decoded into an
     *   `ArrayBuffer` (transported as base64 under the hood).
     */
    responseType?: 'text' | 'base64' | 'arraybuffer' | 'binary';
    /**
     * Skip TLS certificate validation for this request (self-signed,
     * expired, host mismatch, untrusted CA). Modeled after
     * `curl --insecure`.
     *
     * **Requires** the host-side advanced setting "Allow self-signed /
     * invalid TLS certificates" to be enabled; if disabled the request
     * keeps strict validation regardless of this flag (no error, just
     * ignored). The request also emits a console audit line when the
     * skip actually takes effect.
     *
     * **Security**: do not enable for requests carrying credentials or
     * personal data — the connection becomes vulnerable to MITM. Prefer
     * adding the target CA to the system trust store when possible.
     */
    insecureTls?: boolean;
    /** @deprecated Not honoured by the host; use `insecureTls` to opt out of TLS validation. */
    verifyTls?: boolean;
    /** @deprecated Use `async: false` instead; will be removed. */
    sync?: boolean;
}

/**
 * Sub-type of {@link HttpRequestOptions} that selects an `ArrayBuffer`
 * body. Used to drive the binary overload of every `http.*` verb.
 */
export interface HttpBinaryRequestOptions extends HttpRequestOptions {
    responseType: 'arraybuffer' | 'binary';
}

/**
 * Options for `http.download`. Defaults to synchronous mode with a
 * 60 s host-side timeout.
 */
export interface HttpDownloadOptions {
    headers?: Record<string, string>;
    /** Host-side timeout in ms; default 60000. */
    timeout?: number;
    /** Redirect handling: `'follow'` (default) or `'manual'`. */
    redirect?: string;
    /** Set `true` for async dispatch via `http:downloadComplete`. */
    async?: boolean;
    /** Override the request id used to correlate `http:downloadComplete`. */
    requestId?: string;
    /**
     * Skip TLS certificate validation for this download. Requires the
     * host-side "Allow self-signed / invalid TLS certificates" advanced
     * setting; ignored otherwise. See {@link HttpRequestOptions.insecureTls}
     * for the full security caveat.
     */
    insecureTls?: boolean;
    /** @deprecated Not honoured by the host; use `insecureTls` to opt out of TLS validation. */
    verifyTls?: boolean;
}

/**
 * Result of any `http.*` verb call.
 *
 * The C++ host runs every HTTP verb in either sync or async mode (default
 * async). In **async** mode the immediate return is a *dispatch* envelope
 * `{ success, requestId, async: true }` and the actual response arrives
 * later through the `http:response` event (use {@link http.request} for
 * the awaited variant). In **sync** mode the call resolves with the full
 * response (`status` / `body` / `headers`).
 *
 * Every field is optional so the same shape covers both paths; callers
 * checking `requestId` know they got the dispatch envelope, callers
 * checking `status` know they got the synchronous response.
 */
export interface HttpResponse {
    /** HTTP status code (sync mode only). */
    status?: number;
    statusText?: string;
    headers?: Record<string, string>;
    body?: string;
    success?: boolean;
    error?: string;
    /** Correlation id for the deferred result (async mode only). */
    requestId?: string;
    /** True when the call dispatched asynchronously. */
    async?: boolean;
    /** HEAD-only convenience: parsed `Content-Length` header. */
    contentLength?: number;
    /**
     * Actual transport encoding used by the host:
     * - `'text'` for UTF-8 string bodies.
     * - `'base64'` when the host base64-encoded a binary payload (the
     *   SDK auto-decodes this into {@link HttpBinaryResponse} when the
     *   caller asked for `'arraybuffer'` / `'binary'`).
     */
    responseType?: 'text' | 'base64';
}

/**
 * Variant of {@link HttpResponse} where `body` is decoded into an
 * `ArrayBuffer`. Returned by every `http.*` verb when the caller passes
 * `responseType: 'arraybuffer'` or `'binary'`.
 */
export interface HttpBinaryResponse extends Omit<HttpResponse, 'body'> {
    body?: ArrayBuffer;
}

/**
 * Detect whether the caller asked for binary auto-decoding. Used by every
 * verb wrapper to pick the right return shape.
 */
function _wantsBinary(opts?: HttpRequestOptions): boolean {
    return opts?.responseType === 'arraybuffer' || opts?.responseType === 'binary';
}

/**
 * Decode a base64 string into an `ArrayBuffer` using the platform `atob`.
 * Falls back to a manual lookup when `atob` is unavailable.
 */
function _base64ToArrayBuffer(s: string): ArrayBuffer {
    const decode = typeof atob === 'function'
        ? atob
        : (input: string) => {
            const lookup = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
            const cleaned = input.replace(/=+$/, '');
            let bits = 0;
            let value = 0;
            let out = '';
            for (let i = 0; i < cleaned.length; i++) {
                const idx = lookup.indexOf(cleaned.charAt(i));
                if (idx < 0) continue;
                value = (value << 6) | idx;
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    out += String.fromCharCode((value >> bits) & 0xff);
                }
            }
            return out;
        };
    const bin = decode(s);
    const len = bin.length;
    const buf = new ArrayBuffer(len);
    const view = new Uint8Array(buf);
    for (let i = 0; i < len; i++) view[i] = bin.charCodeAt(i);
    return buf;
}

/**
 * Convert an {@link HttpResponse} carrying a base64 string body into an
 * {@link HttpBinaryResponse} carrying an `ArrayBuffer`. No-op when the
 * response is empty, an async-dispatch envelope, or already non-base64.
 */
function _decodeBinary(resp: HttpResponse): HttpBinaryResponse {
    if (!resp || typeof resp.body !== 'string') {
        return resp as HttpBinaryResponse;
    }
    if (resp.responseType !== 'base64') {
        return resp as HttpBinaryResponse;
    }
    const decoded = _base64ToArrayBuffer(resp.body);
    return { ...resp, body: decoded };
}

/**
 * Dispatch a single-shot HTTP verb through the host bridge and, when the
 * caller asked for a binary response, decode the base64 body into an
 * `ArrayBuffer` once the host reply lands.
 */
function _finalizeHttp(
    promise: Promise<HttpResponse>,
    opts: HttpRequestOptions | undefined,
): Promise<HttpResponse | HttpBinaryResponse> {
    return _wantsBinary(opts) ? promise.then(_decodeBinary) : promise;
}

function httpGet(url: string): Promise<HttpResponse>;
function httpGet(url: string, opts: HttpBinaryRequestOptions): Promise<HttpBinaryResponse>;
function httpGet(url: string, opts: HttpRequestOptions): Promise<HttpResponse>;
function httpGet(
    url: string,
    opts?: HttpRequestOptions,
): Promise<HttpResponse | HttpBinaryResponse> {
    return _finalizeHttp(bridge.invoke<HttpResponse>('http.get', { url, ...(opts || {}) }), opts);
}

function httpPost(url: string, body?: JsonValue): Promise<HttpResponse>;
function httpPost(
    url: string,
    body: JsonValue | undefined,
    opts: HttpBinaryRequestOptions,
): Promise<HttpBinaryResponse>;
function httpPost(
    url: string,
    body: JsonValue | undefined,
    opts: HttpRequestOptions,
): Promise<HttpResponse>;
function httpPost(
    url: string,
    body?: JsonValue,
    opts?: HttpRequestOptions,
): Promise<HttpResponse | HttpBinaryResponse> {
    return _finalizeHttp(bridge.invoke<HttpResponse>('http.post', { url, body, ...(opts || {}) }), opts);
}

function httpPut(url: string, body?: JsonValue): Promise<HttpResponse>;
function httpPut(
    url: string,
    body: JsonValue | undefined,
    opts: HttpBinaryRequestOptions,
): Promise<HttpBinaryResponse>;
function httpPut(
    url: string,
    body: JsonValue | undefined,
    opts: HttpRequestOptions,
): Promise<HttpResponse>;
function httpPut(
    url: string,
    body?: JsonValue,
    opts?: HttpRequestOptions,
): Promise<HttpResponse | HttpBinaryResponse> {
    return _finalizeHttp(bridge.invoke<HttpResponse>('http.put', { url, body, ...(opts || {}) }), opts);
}

function httpDelete(url: string, body?: JsonValue): Promise<HttpResponse>;
function httpDelete(
    url: string,
    body: JsonValue | undefined,
    opts: HttpBinaryRequestOptions,
): Promise<HttpBinaryResponse>;
function httpDelete(
    url: string,
    body: JsonValue | undefined,
    opts: HttpRequestOptions,
): Promise<HttpResponse>;
function httpDelete(
    url: string,
    body?: JsonValue,
    opts?: HttpRequestOptions,
): Promise<HttpResponse | HttpBinaryResponse> {
    return _finalizeHttp(bridge.invoke<HttpResponse>('http.delete', { url, body, ...(opts || {}) }), opts);
}

function httpPatch(url: string, body?: JsonValue): Promise<HttpResponse>;
function httpPatch(
    url: string,
    body: JsonValue | undefined,
    opts: HttpBinaryRequestOptions,
): Promise<HttpBinaryResponse>;
function httpPatch(
    url: string,
    body: JsonValue | undefined,
    opts: HttpRequestOptions,
): Promise<HttpResponse>;
function httpPatch(
    url: string,
    body?: JsonValue,
    opts?: HttpRequestOptions,
): Promise<HttpResponse | HttpBinaryResponse> {
    return _finalizeHttp(bridge.invoke<HttpResponse>('http.patch', { url, body, ...(opts || {}) }), opts);
}

function httpHead(url: string, opts?: HttpRequestOptions): Promise<HttpResponse> {
    return bridge.invoke<HttpResponse>('http.head', { url, ...(opts || {}) });
}

function httpRequest(url: string): Promise<HttpResponse>;
function httpRequest(url: string, opts: HttpBinaryRequestOptions): Promise<HttpBinaryResponse>;
function httpRequest(url: string, opts: HttpRequestOptions): Promise<HttpResponse>;
function httpRequest(
    url: string,
    opts?: HttpRequestOptions,
): Promise<HttpResponse | HttpBinaryResponse> {
    const promise = _httpRequest(
        'http.get',
        { url, ...(opts || {}) },
        typeof opts?.timeout === 'number' ? opts.timeout + 5000 : 35000,
    );
    return _wantsBinary(opts) ? promise.then(_decodeBinary) : promise;
}

export const http = {
    get: httpGet,
    post: httpPost,
    put: httpPut,
    delete: httpDelete,
    patch: httpPatch,
    head: httpHead,
    download: (url: string, saveTo: string, opts?: HttpDownloadOptions) =>
        bridge.invoke<
            BaseResponse & {
                requestId?: string;
                path?: string;
                bytesWritten?: number;
                cancelled?: boolean;
            }
        >('http.download', { url, saveTo, ...(opts || {}) }),
    abort: (requestId: string) =>
        bridge.invoke<BaseResponse & { cancelled?: boolean }>('http.abort', {
            requestId,
        }),
    /** Detach the default failure logger; see {@link disableDefaultHttpDownloadLogger}. */
    disableDefaultDownloadLogger: disableDefaultHttpDownloadLogger,

    /**
     * Event-driven GET that awaits the `http:response` event. The
     * host may either respond synchronously (`async: false`) or queue
     * the request and deliver the result via `http:response`. A
     * client-side timeout (default 35 s) guards against the event
     * never arriving.
     */
    request: httpRequest,
};

/**
 * Wraps a verb-specific invoke into a Promise that resolves on the
 * first `http:response` event whose `requestId` matches the dispatched
 * call. Cleans up the listener and the client-side watchdog on every
 * exit path so concurrent calls cannot leak.
 *
 * `clientTimeoutMs` should be slightly larger than the host-side
 * timeout in `opts.timeout`; defaults to 35 s.
 */
interface HttpInitResponse extends HttpResponse {
    async?: boolean;
    requestId?: string;
}

interface HttpResponseEvent extends HttpResponse {
    requestId: string;
}

function _httpRequest(
    method: string,
    payload: JsonObject,
    clientTimeoutMs = 35000,
): Promise<HttpResponse> {
    return new Promise<HttpResponse>((resolve, reject) => {
        let timerId: ReturnType<typeof setTimeout> | null = null;
        let off: (() => void) | null = null;

        const cleanup = (): void => {
            if (timerId !== null) {
                clearTimeout(timerId);
                timerId = null;
            }
            if (off !== null) {
                off();
                off = null;
            }
        };

        bridge
            .invoke<HttpInitResponse>(method, payload)
            .then((init) => {
                // Synchronous path: host already returned the full response.
                if (!init || !init.async) {
                    cleanup();
                    if (init && init.success === false) {
                        reject(new Error(init.error || 'HTTP request failed'));
                    } else {
                        resolve(init);
                    }
                    return;
                }

                const requestId = init.requestId;
                if (!requestId) {
                    cleanup();
                    reject(
                        new Error(
                            'HTTP async response missing requestId (host contract violation).',
                        ),
                    );
                    return;
                }

                timerId = setTimeout(() => {
                    cleanup();
                    bridge
                        .invoke<HttpResponse>('http.abort', { requestId })
                        .catch(() => {
                            /* swallow abort errors */
                        });
                    reject(
                        new Error(
                            `HTTP request timeout (clientTimeout=${clientTimeoutMs}ms, requestId=${requestId})`,
                        ),
                    );
                }, clientTimeoutMs);

                off = bridge.on(
                    'http:response',
                    (raw: unknown) => {
                        const data = raw as HttpResponseEvent | undefined;
                        if (!data || data.requestId !== requestId) return;
                        cleanup();
                        if (data.success) {
                            resolve(data);
                        } else {
                            const err: Error & { response?: HttpResponseEvent } =
                                new Error(data.error || 'HTTP request failed');
                            err.response = data;
                            reject(err);
                        }
                    },
                );
            })
            .catch((err) => {
                cleanup();
                reject(err);
            });
    });
}
