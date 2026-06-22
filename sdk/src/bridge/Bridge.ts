/**
 * `Bridge` — runtime bridge to the C++-injected native fb2k surface.
 *
 * Wraps the host-injected {@link NativeFb2k} into a small,
 * strongly-typed facade that all per-namespace proxies share.
 *
 * Key behaviours:
 *
 * - **Mock fallback** — When `window.fb2k` is missing (e.g. running
 *   in a non-WebView2 page), {@link Bridge.invoke} resolves with
 *   `{ mock: true, method }` after 100ms instead of throwing. This
 *   keeps storybook / unit-test consumers from blowing up.
 * - **Late host injection** — When `window.chrome.webview` is present
 *   but `window.fb2k` has not yet been injected, {@link Bridge} polls
 *   every 100ms (max 50 attempts = 5 s) and resolves
 *   {@link Bridge.ready} once the host appears.
 * - **Unsubscribe semantics** — {@link Bridge.on} / {@link Bridge.once}
 *   return a cleanup callback, not the handler itself, so consumers
 *   can `const off = bridge.on(...); off()`.
 */

import type { FBEventName, FBEventPayloadMap } from '../types/events.js';
import type { NativeFb2k } from '../types/native.js';
import type { JsonObject } from '../types/json.js';

/**
 * Strongly-typed event handler used by {@link Bridge.on} /
 * {@link Bridge.once} when the event name is a known
 * {@link FBEventName}. Falls back to `unknown` for arbitrary strings.
 */
export type FBEventHandler<K extends FBEventName> = (
    data: FBEventPayloadMap[K],
) => void;

/** Generic raw-event handler for unknown event names. */
export type RawEventHandler = (data: unknown) => void;

/**
 * Sentinel resolved by {@link Bridge.invoke} when the C++ host is not
 * available (typical in unit tests or storybook previews).
 */
export interface MockInvokeResponse {
    mock: true;
    method: string;
}

/**
 * Snapshot delivered to the {@link BridgeMetricsHook} after every
 * {@link Bridge.invoke} call settles. Both successful and failed calls
 * trigger the hook; consumers should branch on the `success` flag.
 */
export interface BridgeInvokeMetrics {
    /** Canonical `<namespace>.<method>` name passed to `invoke`. */
    method: string;
    /** Wall-clock duration from invoke entry to settle, in milliseconds. */
    durationMs: number;
    /** `true` when the host (or the mock fallback) resolved the call. */
    success: boolean;
    /** Resolved value for successful calls; absent on failure. */
    result?: unknown;
    /** Rejection reason for failed calls; absent on success. */
    error?: unknown;
}

/**
 * Instrumentation callback installed via
 * {@link Bridge.setMetricsHook}. Receives one
 * {@link BridgeInvokeMetrics} per settled invoke. Exceptions thrown by
 * the hook are captured and discarded so observability code cannot
 * destabilise the runtime invoke pipeline.
 */
export type BridgeMetricsHook = (metrics: BridgeInvokeMetrics) => void;

export class Bridge {
    /**
     * Lazily resolved native bridge handle. Set once the host injects
     * `window.fb2k`; remains `undefined` in mock environments.
     */
    private nativeFb2k: NativeFb2k | undefined;

    /**
     * `true` once {@link Bridge.nativeFb2k} has been resolved successfully.
     * Exposed as a getter so the value can flip from `false` → `true`
     * after late host injection.
     */
    private _isAvailable: boolean;

    private _readyPromise?: Promise<void>;
    private _readyResolve?: () => void;

    /**
     * Instrumentation hook installed via
     * {@link Bridge.setMetricsHook}; `undefined` while no consumer is
     * subscribed.
     */
    private _metricsHook?: BridgeMetricsHook;

    constructor() {
        this.nativeFb2k = this.getNativeFb2k();
        this._isAvailable = !!this.nativeFb2k;
        this.checkAvailability();
    }

    /**
     * Install or remove an instrumentation hook fired after every
     * {@link Bridge.invoke} call settles, including the mock-fallback
     * path. Useful for telemetry, logging, and per-method latency
     * tracking.
     *
     * Exceptions thrown by the hook are caught and discarded so
     * downstream observability code cannot break invoke callers. Pass
     * `undefined` to detach a previously installed hook.
     *
     * @param hook - Receives a {@link BridgeInvokeMetrics} snapshot per
     *               settled invoke, or `undefined` to detach.
     */
    setMetricsHook(hook: BridgeMetricsHook | undefined): void {
        this._metricsHook = hook;
    }

    /**
     * Dispatch a {@link BridgeInvokeMetrics} snapshot to the installed
     * hook. Exceptions thrown by the hook are surfaced via
     * `console.warn` (with the method name and the original error) and
     * then discarded so observability failures cannot destabilise the
     * invoke pipeline.
     */
    private _emitMetrics(metrics: BridgeInvokeMetrics): void {
        const hook = this._metricsHook;
        if (!hook) return;
        try {
            hook(metrics);
        } catch (err) {
            try {
                console.warn(
                    `[fb2k SDK] BridgeMetricsHook threw for "${metrics.method}":`,
                    err,
                );
            } catch {
                /* deliberately empty: console itself unavailable. */
            }
        }
    }

    /**
     * High-resolution monotonic timestamp in milliseconds, with a
     * `Date.now()` fallback for environments without `performance`.
     */
    private _now(): number {
        return typeof performance !== 'undefined' && performance.now
            ? performance.now()
            : Date.now();
    }

    /**
     * Resolve the native bridge handle from the global `window`.
     *
     * Returns the cached `window._nativeFb2k` alias when present,
     * otherwise the host-injected `window.fb2k` if it exposes an
     * `invoke` function. Returns `undefined` when running outside a
     * WebView2 host so callers can transparently fall back to the
     * mock path.
     */
    private getNativeFb2k(): NativeFb2k | undefined {
        if (typeof window === 'undefined') return undefined;
        if (window._nativeFb2k) return window._nativeFb2k;
        const candidate = window.fb2k;
        if (candidate && typeof candidate.invoke === 'function') {
            window._nativeFb2k = candidate;
            return candidate;
        }
        return undefined;
    }

    /**
     * If the native bridge is missing but a WebView2 host is detected,
     * poll for late injection (the host may install `fb2k` after the
     * SDK loads). Polls 50 times at 100ms intervals (5 s budget).
     */
    private checkAvailability(): void {
        if (this.nativeFb2k) return;
        if (typeof window === 'undefined') return;
        const w = window as Window & { chrome?: { webview?: unknown } };
        if (!w.chrome?.webview) return;

        let attempts = 0;
        const maxAttempts = 50;
        const poll = (): void => {
            this.nativeFb2k = this.getNativeFb2k();
            if (this.nativeFb2k) {
                this._isAvailable = true;
                if (this._readyResolve) this._readyResolve();
                return;
            }
            if (++attempts < maxAttempts) {
                setTimeout(poll, 100);
            }
        };
        setTimeout(poll, 100);
    }

    /**
     * Read-only availability flag, exposed indirectly via
     * `fb.isAvailable()` on the runtime aggregate object.
     */
    get isAvailable(): boolean {
        return this._isAvailable;
    }

    /**
     * Promise that resolves once the native bridge is reachable. Resolves
     * immediately if the host is already present; otherwise resolves when
     * {@link Bridge.checkAvailability} succeeds via late injection.
     */
    ready(): Promise<void> {
        if (this._isAvailable) return Promise.resolve();
        if (!this._readyPromise) {
            this._readyPromise = new Promise<void>((resolve) => {
                this._readyResolve = resolve;
            });
        }
        return this._readyPromise;
    }

    /**
     * Invoke a registered C++ API.
     *
     * @typeParam TResp - Expected response shape (caller-supplied so each
     *                    namespace can request the correct
     *                    `*Response` interface from `src/types/responses.ts`).
     * @param method - Canonical `<namespace>.<method>` API name in dot
     *                 notation (e.g. `playback.play`).
     * @param params - Parameter object; structure determined by the
     *                 registered C++ handler.
     * @returns Promise that resolves with `TResp`. In mock environments
     *          (no host) resolves with {@link MockInvokeResponse} cast
     *          to `TResp` after a 100ms delay.
     */
    async invoke<TResp = unknown>(
        method: string,
        params?: object,
    ): Promise<TResp> {
        const start = this._now();
        const native = this.getNativeFb2k();
        if (native) {
            try {
                const result = await (native.invoke(method, params) as Promise<TResp>);
                this._emitMetrics({
                    method,
                    durationMs: this._now() - start,
                    success: true,
                    result,
                });
                return result;
            } catch (error) {
                this._emitMetrics({
                    method,
                    durationMs: this._now() - start,
                    success: false,
                    error,
                });
                throw error;
            }
        }
        return new Promise<TResp>((resolve) => {
            setTimeout(() => {
                const stub: MockInvokeResponse = { mock: true, method };
                this._emitMetrics({
                    method,
                    durationMs: this._now() - start,
                    success: true,
                    result: stub,
                });
                resolve(stub as unknown as TResp);
            }, 100);
        });
    }

    /**
     * Subscribe to a C++ event. Returns an unsubscribe callback (not the
     * handler) so consumers can `const off = bridge.on(...); off()`.
     *
     * Two overloads:
     * - Strongly-typed when `event` is a known {@link FBEventName}.
     * - Untyped fallback for arbitrary event strings (custom user events).
     */
    on<K extends FBEventName>(event: K, handler: FBEventHandler<K>): () => void;
    on(event: string, handler: RawEventHandler): () => void;
    on(event: string, handler: RawEventHandler): () => void {
        const native = this.getNativeFb2k();
        if (native) {
            native.on(event, handler);
            return () => this.off(event, handler);
        }
        return () => {
            /* no-op cleanup in mock mode */
        };
    }

    /**
     * Detach a previously registered handler. No-op when the host is
     * unavailable.
     */
    off(event: string, handler: RawEventHandler): void {
        const native = this.getNativeFb2k();
        if (native) {
            native.off(event, handler);
        }
    }

    /**
     * One-shot subscription. Implemented as a self-unbinding `on` so
     * that the unsubscribe path is uniform (single cleanup point via
     * {@link Bridge.off}).
     */
    once<K extends FBEventName>(event: K, handler: FBEventHandler<K>): () => void;
    once(event: string, handler: RawEventHandler): () => void;
    once(event: string, handler: RawEventHandler): () => void {
        const wrapper: RawEventHandler = (data) => {
            this.off(event, wrapper);
            handler(data);
        };
        return this.on(event, wrapper);
    }
}

/**
 * Singleton {@link Bridge} instance shared by every namespace proxy.
 */
export const bridge = new Bridge();
