import type { JsonObject } from '../types/json.js';
/**
 * Type-only contract for the runtime bridge surface that the SMP
 * compatibility layer needs.
 *
 * Why a separate file:
 * - The bootstrap module accepts `fb` as an argument (so the IIFE
 *   bundle can read `window.fb` without importing the bridge module
 *   and inlining a duplicate copy).
 * - Re-using `typeof import('../bridge/index.js').fb` from many call
 *   sites would couple every SMP file to the entire aggregate type;
 *   this thin shape narrows the surface to just what SMP touches.
 */

import type { FBEventName, FBEventPayloadMap } from '../types/events.js';
import type { BaseResponse } from '../types/responses.js';

/**
 * Minimal projection of `fb` (the runtime aggregate exported by
 * `src/bridge/index.ts`) consumed by the SMP layer.
 *
 * Values that aren't used here (`fb.state`, `fb.audio`, etc.) are
 * intentionally omitted — keep this list narrow so accidental new
 * coupling shows up as a compile error.
 */
export interface SmpBridgeShape {
    /** Subscribe to a strongly-typed event. */
    on<K extends FBEventName>(
        event: K,
        handler: (data: FBEventPayloadMap[K]) => void,
    ): () => void;
    /** Subscribe to an arbitrary event name (untyped fallback). */
    on(event: string, handler: (data: unknown) => void): () => void;

    /** Detach handler from a strongly-typed event. */
    off<K extends FBEventName>(
        event: K,
        handler: (data: FBEventPayloadMap[K]) => void,
    ): void;
    /** Detach handler from an arbitrary event name. */
    off(event: string, handler: (data: unknown) => void): void;

    /** One-shot subscribe to a strongly-typed event. */
    once<K extends FBEventName>(
        event: K,
        handler: (data: FBEventPayloadMap[K]) => void,
    ): () => void;
    /** One-shot subscribe to an arbitrary event name. */
    once(event: string, handler: (data: unknown) => void): () => void;

    /** Direct invoke escape hatch. */
    invoke<TResp = unknown>(
        method: string,
        params?: object,
    ): Promise<TResp>;

    // Player namespace methods touched by the SMP fb extensions
    // (Play/Pause/Stop/Next/Prev/...). Typed as optional `unknown`
    // function shapes so the compiler tolerates either the full
    // `fb.player.*` runtime aggregate or a partial mock.
    readonly player?: {
        play?: () => Promise<BaseResponse>;
        pause?: () => Promise<BaseResponse>;
        stop?: () => Promise<BaseResponse>;
        next?: () => Promise<BaseResponse>;
        prev?: () => Promise<BaseResponse>;
        random?: () => Promise<BaseResponse>;
        toggle?: () => Promise<BaseResponse>;
        mute?: () => Promise<BaseResponse>;
        seek?: (seconds: number) => Promise<BaseResponse>;
        setStopAfterCurrent?: (enabled: boolean) => Promise<BaseResponse>;
    };

    readonly ui?: {
        close?: () => Promise<BaseResponse>;
        setAlwaysOnTop?: (enabled: boolean) => Promise<BaseResponse>;
    };
}

/**
 * Loosely-typed helper used by `Object.defineProperty` calls inside
 * the SMP bootstrap that augment the runtime `fb` instance with
 * SMP-style methods (Play / Pause / Volume / etc.).
 */
export type ExtensibleFb = SmpBridgeShape & JsonObject;
