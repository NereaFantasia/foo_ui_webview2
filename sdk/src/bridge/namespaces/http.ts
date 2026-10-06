import { subscribe } from '../subscribe.js';
import { call } from '../call.js';
import type { HttpDownloadCompletePayload } from '../../types/events.js';
import type { JsonValue } from '../../types/json.js';
import type { ApiFailure, HttpRequestOptions } from '../../types/responses.js';
import type {
    HttpDeleteParams,
    HttpDownloadParams,
    HttpGetParams,
    HttpPatchParams,
    HttpPostParams,
    HttpPutParams,
} from '../../types/generated/params.js';
import type {
    HttpDownloadResponse,
    HttpGetResponse,
    HttpGetSuccess,
    HttpHeadResponse,
} from '../../types/generated/responses.js';

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
let _defaultHttpDownloadLoggerOff: (() => void) | null = subscribe(
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
 * Sub-type of {@link HttpRequestOptions} that selects an `ArrayBuffer`
 * body. Used to drive the binary overload of every `http.*` verb.
 */
export interface HttpBinaryRequestOptions extends HttpRequestOptions {
    responseType: 'arraybuffer' | 'binary';
}

/**
 * Options for `http.download`: its parameters other than `url` and
 * `saveTo`. Defaults to synchronous mode with a 60 s host-side timeout.
 */
export interface HttpDownloadOptions extends Omit<HttpDownloadParams, 'url' | 'saveTo'> {
    /**
     * Accept invalid or self-signed certificates for this download.
     * Requires the host-side "Allow self-signed / invalid TLS
     * certificates" advanced setting; ignored otherwise. See
     * {@link HttpRequestOptions.insecureTls} for the full security caveat.
     */
    insecureTls?: boolean;
    /**
     * @deprecated Has no effect; `http:downloadComplete` carries the
     * `requestId` returned in the receipt.
     */
    requestId?: string;
    /** @deprecated Has no effect; use `insecureTls` to accept invalid certificates. */
    verifyTls?: boolean;
}

/**
 * Reply of `http.get` / `post` / `put` / `delete` / `patch` and
 * {@link http.request}.
 *
 * By default the host dispatches the request and replies at once with a
 * receipt (`requestId`, `async: true`); the response arrives later as the
 * `http:response` event, which {@link http.request} waits for. With
 * `async: false` the reply is the response itself (`status`, `headers`,
 * `body`, `responseType`).
 */
export type HttpResponse = HttpGetResponse;

/** A successful {@link HttpResponse} whose `body` is decoded into an `ArrayBuffer`. */
export interface HttpBinarySuccess extends Omit<HttpGetSuccess, 'body'> {
    body?: ArrayBuffer;
}

/**
 * Variant of {@link HttpResponse} where `body` is decoded into an
 * `ArrayBuffer`. Returned by every `http.*` verb when the caller passes
 * `responseType: 'arraybuffer'` or `'binary'`.
 */
export type HttpBinaryResponse = HttpBinarySuccess | ApiFailure;

/**
 * The options as sent: the deprecated keys have no effect and the host
 * rejects keys it does not declare, so they are dropped here.
 */
function _wire(opts: HttpRequestOptions = {}): Omit<HttpGetParams, 'url'> {
    const { verifyTls: _verifyTls, sync: _sync, ...wire } = opts;
    return wire;
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
    if (!resp || resp.success === false || typeof resp.body !== 'string' || resp.responseType !== 'base64') {
        return resp as HttpBinaryResponse;
    }
    return { ...resp, body: _base64ToArrayBuffer(resp.body) };
}

/** {@link _decodeBinary} for a response already known to be a success. */
function _decodeBinarySuccess(resp: HttpGetSuccess): HttpBinarySuccess {
    if (typeof resp.body !== 'string' || resp.responseType !== 'base64') {
        return resp as HttpBinarySuccess;
    }
    return { ...resp, body: _base64ToArrayBuffer(resp.body) };
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
    return _finalizeHttp(
        call('http.get', { url, ..._wire(opts) }),
        opts,
    );
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
    return _finalizeHttp(
        call('http.post', { url, body, ..._wire(opts) }),
        opts,
    );
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
    return _finalizeHttp(
        call('http.put', { url, body, ..._wire(opts) }),
        opts,
    );
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
    return _finalizeHttp(
        call('http.delete', { url, body, ..._wire(opts) }),
        opts,
    );
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
    return _finalizeHttp(
        call('http.patch', { url, body, ..._wire(opts) }),
        opts,
    );
}

// A HEAD response has no body, so `responseType` does not apply.
function httpHead(
    url: string,
    opts?: Omit<HttpRequestOptions, 'responseType'>,
): Promise<HttpHeadResponse> {
    return call('http.head', { url, ..._wire(opts) });
}

function httpRequest(url: string): Promise<HttpGetSuccess>;
function httpRequest(url: string, opts: HttpBinaryRequestOptions): Promise<HttpBinarySuccess>;
function httpRequest(url: string, opts: HttpRequestOptions): Promise<HttpGetSuccess>;
function httpRequest(
    url: string,
    opts?: HttpRequestOptions,
): Promise<HttpGetSuccess | HttpBinarySuccess> {
    const promise = _httpRequest(
        { url, ..._wire(opts) },
        typeof opts?.timeout === 'number' ? opts.timeout + 5000 : 35000,
    );
    return _wantsBinary(opts) ? promise.then(_decodeBinarySuccess) : promise;
}

function httpDownload(
    url: string,
    saveTo: string,
    opts: HttpDownloadOptions = {},
): Promise<HttpDownloadResponse> {
    const { requestId: _requestId, verifyTls: _verifyTls, ...wire } = opts;
    return call(
        'http.download',
        { url, saveTo, ...wire },
    );
}

/**
 * `http` — HTTP requests made by the host process.
 *
 * Requests are not subject to CORS. Only `http` and `https` URLs are
 * allowed, and local or private network addresses fail with
 * `PERMISSION_DENIED` unless the host setting allows them.
 *
 * Body-bearing verbs (`post` / `put` / `delete` / `patch`) accept the
 * body as a positional argument; the host sends a non-string body as its
 * JSON text.
 */
export const http = {
    get: httpGet,
    post: httpPost,
    put: httpPut,
    delete: httpDelete,
    patch: httpPatch,
    head: httpHead,
    download: httpDownload,
    abort: (requestId: string) =>
        call('http.abort', { requestId }),
    /** Detach the default failure logger; see {@link disableDefaultHttpDownloadLogger}. */
    disableDefaultDownloadLogger: disableDefaultHttpDownloadLogger,

    /**
     * Event-driven GET that awaits the `http:response` event. The
     * host may either respond synchronously (`async: false`) or queue
     * the request and deliver the result via `http:response`. A
     * client-side timeout (default 35 s, or `opts.timeout + 5000` ms)
     * guards against the event never arriving.
     *
     * Unlike the other verbs it resolves with the success branch only.
     * A failure the host reports rejects with an `Error` whose message
     * is the failure's `error`; a failure delivered by `http:response`
     * also carries that payload as the error's `response`. The client
     * timeout rejects as well, after aborting the host request.
     */
    request: httpRequest,
};

/** An `http:response` payload: the response or failure plus the id from the receipt. */
type HttpResponseEvent = HttpResponse & { requestId: string };

/**
 * Wraps a verb-specific invoke into a Promise that resolves on the
 * first `http:response` event whose `requestId` matches the dispatched
 * call. Cleans up the listener and the client-side watchdog on every
 * exit path so concurrent calls cannot leak.
 *
 * `clientTimeoutMs` should be slightly larger than the host-side
 * timeout in `payload.timeout`; defaults to 35 s.
 */
function _httpRequest(
    payload: HttpGetParams,
    clientTimeoutMs = 35000,
): Promise<HttpGetSuccess> {
    return new Promise<HttpGetSuccess>((resolve, reject) => {
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

        call('http.get', payload)
            .then((init) => {
                if (init.success === false) {
                    cleanup();
                    reject(new Error(init.error || 'HTTP request failed'));
                    return;
                }
                // Synchronous path: host already returned the full response.
                if (!init.async) {
                    cleanup();
                    resolve(init);
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
                    call('http.abort', { requestId })
                        .catch(() => {
                            /* swallow abort errors */
                        });
                    reject(
                        new Error(
                            `HTTP request timeout (clientTimeout=${clientTimeoutMs}ms, requestId=${requestId})`,
                        ),
                    );
                }, clientTimeoutMs);

                off = subscribe(
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
