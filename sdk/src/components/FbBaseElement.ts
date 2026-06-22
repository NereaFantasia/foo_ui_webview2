/**
 * `FbBaseElement` — shared base class for every `fb-*` web component.
 *
 * Implements the lifecycle contract documented in `2.6.1` of the
 * project AGENTS rules: zero visual styling, expose function only.
 *
 * Subclass hooks:
 * - {@link FbBaseElement._buildDOM}    — one-shot Shadow DOM build.
 * - {@link FbBaseElement._setupEvents} — bind DOM event listeners.
 * - {@link FbBaseElement._subscribe}   — register `fb.on(...)` callbacks.
 *
 * Cleanup is automatic:
 * - `_subscriptions` accumulates `fb.on()` unsubscribe callbacks; they
 *   fire once on `disconnectedCallback`.
 * - `_abortController` carries an `AbortSignal` that every DOM listener
 *   bound via {@link FbBaseElement._listen} consumes. Abort once on
 *   disconnect; the controller is recreated to support re-attachment.
 *
 * `static baseCSS` provides the minimal structural styling (display,
 * cursor, button reset) shared by every component. **No** color /
 * background / typography is included — themes own visual decisions.
 */

import type { FBEventHandler, RawEventHandler } from '../bridge/Bridge.js';
import type { FBEventName } from '../types/events.js';
import { getFb } from './runtime.js';

export class FbBaseElement extends HTMLElement {
    /**
     * Unsubscribe callbacks returned by `fb.on(...)`. Drained on
     * `disconnectedCallback`; intentionally `protected` so subclasses
     * can prepend additional cleanups in edge cases.
     */
    protected _subscriptions: Array<() => void> = [];

    /**
     * `AbortController` whose `signal` is forwarded to every DOM
     * listener registered via {@link FbBaseElement._listen}.
     * `disconnectedCallback` calls `abort()` to detach all listeners
     * in one shot, then replaces the controller so a subsequent
     * `connectedCallback` (re-attach) starts with a fresh signal.
     */
    protected _abortController: AbortController = new AbortController();

    /**
     * `true` once {@link FbBaseElement._buildDOM} has finished. Useful
     * for subclasses that schedule async work and need to no-op before
     * the Shadow DOM is ready.
     */
    protected _domReady = false;

    constructor() {
        super();
        this.attachShadow({ mode: 'open' });
    }

    connectedCallback(): void {
        this._buildDOM();
        this._domReady = true;
        this._setupEvents();
        this._subscribe();
    }

    disconnectedCallback(): void {
        for (const off of this._subscriptions) {
            try {
                off();
            } catch {
                /* swallow user callback errors during teardown */
            }
        }
        this._subscriptions.length = 0;
        this._abortController.abort();
        this._abortController = new AbortController();
    }

    /** Subclass hook — build Shadow DOM exactly once per attachment. */
    protected _buildDOM(): void {
        /* override in subclass */
    }

    /** Subclass hook — bind DOM event listeners (use {@link _listen}). */
    protected _setupEvents(): void {
        /* override in subclass */
    }

    /** Subclass hook — register fb event subscriptions (use {@link _sub}). */
    protected _subscribe(): void {
        /* override in subclass */
    }

    /**
     * Subscribe to a typed `fb` event. The unsubscribe callback is
     * recorded so `disconnectedCallback` can drain it.
     */
    protected _sub<K extends FBEventName>(
        event: K,
        handler: FBEventHandler<K>,
    ): void;
    protected _sub(event: string, handler: RawEventHandler): void;
    protected _sub(event: string, handler: RawEventHandler): void {
        this._subscriptions.push(getFb().on(event, handler));
    }

    /**
     * Add a DOM listener whose lifetime is tied to
     * {@link _abortController}. No manual `removeEventListener` is
     * needed — the listener is auto-detached on `disconnectedCallback`.
     */
    protected _listen<E extends Event = Event>(
        target: EventTarget,
        event: string,
        handler: (e: E) => void,
        options: AddEventListenerOptions = {},
    ): void {
        target.addEventListener(event, handler as EventListener, {
            ...options,
            signal: this._abortController.signal,
        });
    }

    /**
     * Dispatch a CustomEvent that bubbles through the Shadow DOM
     * boundary (`composed: true`). All `fb-*` events follow this
     * pattern so theme code can listen at any ancestor.
     */
    protected _emit<T = unknown>(
        name: string,
        detail: T,
        cancelable = false,
    ): boolean {
        return this.dispatchEvent(
            new CustomEvent<T>(name, {
                bubbles: true,
                composed: true,
                cancelable,
                detail,
            }),
        );
    }

    /** Shadow-root scoped `querySelector` shortcut. */
    protected _$<E extends Element = Element>(sel: string): E | null {
        return this.shadowRoot?.querySelector<E>(sel) ?? null;
    }

    /** Shadow-root scoped `querySelectorAll` shortcut. */
    protected _$$<E extends Element = Element>(sel: string): NodeListOf<E> {
        const root = this.shadowRoot;
        if (!root) {
            // Synthesise an empty NodeList via a detached DocumentFragment.
            return document
                .createDocumentFragment()
                .querySelectorAll<E>(sel);
        }
        return root.querySelectorAll<E>(sel);
    }

    /**
     * HTML-escape a value for safe innerHTML interpolation. Escapes
     * `&`, `<`, `>`, and `"` to their named entities.
     */
    protected _escHtml(s: unknown): string {
        return String(s)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    }

    /**
     * Cached structural CSS shared by every component. Intentionally
     * minimal: layout primitives + button reset only.
     */
    private static _baseCSSCache: string | null = null;
    static get baseCSS(): string {
        if (!FbBaseElement._baseCSSCache) {
            FbBaseElement._baseCSSCache =
                ':host{display:inline-flex;box-sizing:border-box}' +
                ':host([hidden]){display:none}' +
                'button{all:unset;cursor:pointer;display:inline-flex;align-items:center;justify-content:center}' +
                'button:disabled{cursor:not-allowed;pointer-events:none}';
        }
        return FbBaseElement._baseCSSCache;
    }
}
