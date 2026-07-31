/**
 * Hand-written type overrides for the `window.*` namespace: structured shapes
 * for the `behavior` / `backdropPolicy` patches and the `profile` union used by
 * `setPopupBehavior` / `setBackdropPolicy` / `createPopup`.
 */

import type {
    WindowBackdropPolicyPatch,
    WindowPopupBehaviorPatch,
} from '../responses.js';

/**
 * Behavior preset for popup windows. Unrecognised / empty values fall back to
 * no preset (field-driven mode); the three recognised presets are:
 *
 * - `'standard'` — owner = main; popup tracks main-window minimize/close.
 * - `'miniPlayer'` — owner = none; hidden from taskbar/Alt-Tab; resists
 *   Win+D minimize. Pair with `alwaysOnTop: true` for stable z-order.
 * - `'desktopLyrics'` — same as miniPlayer plus `noActivate: true` and
 *   transparent backdrop (`activeEffect: 'none'` / `inactiveEffect: 'none'`).
 */
export type WindowPopupProfile =
    | 'standard'
    | 'miniPlayer'
    | 'desktopLyrics';

/**
 * Parameters for `window.createPopup`.
 *
 * The `profile` / `behavior` / `backdropPolicy` slots are documented in
 * detail at `docs/vitepress/api/window.md`; this override simply re-shapes
 * the auto-generated `unknown` placeholders so IDE consumers get
 * autocompletion and type-checking for the nested-object fields.
 *
 * Field-resolution order (highest priority first):
 *   1. `behavior.*` / `backdropPolicy.*` per-field overrides
 *   2. legacy top-level fields (`showInTaskbar`, etc.)
 *   3. `profile` preset defaults
 *   4. host built-in defaults
 *
 * @codegen-override params:window.createPopup
 * @codegen-snapshot alwaysOnTop:primitive,backdropPolicy:unknown,beforeClose:primitive,behavior:unknown,clickThrough:primitive,frame:primitive,height:primitive,maxHeight:primitive,maxWidth:primitive,minHeight:primitive,minWidth:primitive,profile:primitive,resizable:primitive,showInTaskbar:primitive,title:primitive,transparent:primitive,url:primitive,width:primitive,x:primitive,y:primitive
 */
export interface WindowCreatePopupParams {
    /** Initial document URL. @default "" */
    url?: string;
    /** Initial width in physical pixels; no DPI scaling is applied. @default 400 */
    width?: number;
    /** Initial height in physical pixels; no DPI scaling is applied. @default 300 */
    height?: number;
    /** Initial top-left X in screen coordinates; defaults to centred. */
    x?: number;
    /** Initial top-left Y in screen coordinates; defaults to centred. */
    y?: number;
    /** Window title shown in caption / taskbar. @default "" */
    title?: string;
    /** Whether the user may resize the popup. @default true */
    resizable?: boolean;
    /**
     * Global topmost (`WS_EX_TOPMOST`). Use only when the popup must stay
     * above non-application windows; for "stay above main window only"
     * prefer `behavior.owner = 'main'` (or `profile: 'standard'`) instead.
     * Combining `alwaysOnTop` with `behavior.owner = 'main'` triggers
     * activation-link side effects and is discouraged. @default false
     */
    alwaysOnTop?: boolean;
    /** Show in Windows taskbar / Alt-Tab. @default false */
    showInTaskbar?: boolean;
    /** Lower-bound for user resize, in physical pixels. @default 200 */
    minWidth?: number;
    /** Lower-bound for user resize, in physical pixels. @default 150 */
    minHeight?: number;
    /** Upper-bound for user resize, in physical pixels; 0 disables the cap. @default 0 */
    maxWidth?: number;
    /** Upper-bound for user resize, in physical pixels; 0 disables the cap. @default 0 */
    maxHeight?: number;
    /** Render native window frame / caption. @default true */
    frame?: boolean;
    /** Transparent background (requires frame=false for full effect). @default false */
    transparent?: boolean;
    /** Emit `window:beforeClose` and require an explicit confirm/cancel. @default false */
    beforeClose?: boolean;
    /** Click-through (`WS_EX_TRANSPARENT`); window ignores mouse input. @default false */
    clickThrough?: boolean;
    /** Behavior preset. See {@link WindowPopupProfile}. */
    profile?: WindowPopupProfile;
    /**
     * Per-field overrides on top of the resolved profile / legacy defaults.
     * Most callers that just want "popup pinned above the main window"
     * should prefer `profile: 'standard'`; reach for `behavior` only when
     * a single field needs to deviate from the preset.
     */
    behavior?: WindowPopupBehaviorPatch;
    /** Per-field DWM backdrop overrides applied on top of the profile. */
    backdropPolicy?: WindowBackdropPolicyPatch;
}

/**
 * Parameters for `window.setPopupBehavior` (runtime mutation).
 *
 * Mirrors the create-time slots minus geometry / chrome — only the
 * behavior preset and per-field patch are mutable at runtime. The host
 * emits `window:behaviorChanged` once the patch is applied.
 *
 * @codegen-override params:window.setPopupBehavior
 * @codegen-snapshot behavior:unknown,profile:primitive,windowId:primitive
 */
export interface WindowSetPopupBehaviorParams {
    /** Target popup id; omit to address the calling window. @default "" */
    windowId?: string;
    /** Switch the active preset. */
    profile?: WindowPopupProfile;
    /** Per-field overrides; pass `null` to clear an individual field. */
    behavior?: WindowPopupBehaviorPatch;
}

/**
 * Parameters for `window.getBackdropPolicy`.
 *
 * The host resolves the target window through the shared window-target
 * resolver, which lives in a separate translation unit from the handler —
 * the auto-generator's single-TU AST walk cannot see the `windowId` read,
 * so this override documents it by hand.
 *
 * @codegen-override params:window.getBackdropPolicy
 * @codegen-snapshot 
 */
export interface WindowGetBackdropPolicyParams {
    /** Target window id; omit to address the calling window (main-window fallback). */
    windowId?: string;
}

/**
 * Parameters for `window.setBackdropPolicy`.
 *
 * `backdropPolicy` is required: the host rejects requests that omit it or
 * pass a non-object. `windowId` is read by the shared window-target
 * resolver in a separate translation unit (invisible to the
 * auto-generator's single-TU AST walk); mutations do not fall back to the
 * main window when neither `windowId` nor a caller window is resolvable.
 *
 * @codegen-override params:window.setBackdropPolicy
 * @codegen-snapshot backdropPolicy:unknown
 */
export interface WindowSetBackdropPolicyParams {
    /** Target window id; omit to address the calling window. */
    windowId?: string;
    /** Per-field DWM backdrop overrides; pass `null` to clear an individual field. */
    backdropPolicy: WindowBackdropPolicyPatch;
}

/**
 * Parameters for `window.setMinSize`.
 *
 * The host resolves the target window through the shared window-target
 * resolver, which lives in a separate translation unit from the handler —
 * the auto-generator's single-TU AST walk cannot see the `windowId` read,
 * so this override documents it by hand.
 *
 * @codegen-override params:window.setMinSize
 * @codegen-snapshot height:primitive,width:primitive
 */
export interface WindowSetMinSizeParams {
    /** Target window id; omit to address the calling window. */
    windowId?: string;
    /** Lower-bound width in physical pixels. @default 0 */
    width?: number;
    /** Lower-bound height in physical pixels. @default 0 */
    height?: number;
}

/**
 * Parameters for `window.getMinSize`.
 *
 * `windowId` is read by the shared window-target resolver in a separate
 * translation unit, invisible to the auto-generator's single-TU AST walk.
 *
 * @codegen-override params:window.getMinSize
 * @codegen-snapshot 
 */
export interface WindowGetMinSizeParams {
    /** Target window id; omit to address the calling window. */
    windowId?: string;
}

/**
 * Parameters for `window.setMaxSize`.
 *
 * `windowId` is read by the shared window-target resolver in a separate
 * translation unit, invisible to the auto-generator's single-TU AST walk.
 *
 * @codegen-override params:window.setMaxSize
 * @codegen-snapshot height:primitive,width:primitive
 */
export interface WindowSetMaxSizeParams {
    /** Target window id; omit to address the calling window. */
    windowId?: string;
    /** Upper-bound width in physical pixels; 0 disables the cap. @default 0 */
    width?: number;
    /** Upper-bound height in physical pixels; 0 disables the cap. @default 0 */
    height?: number;
}

/**
 * Parameters for `window.getMaxSize`.
 *
 * `windowId` is read by the shared window-target resolver in a separate
 * translation unit, invisible to the auto-generator's single-TU AST walk.
 *
 * @codegen-override params:window.getMaxSize
 * @codegen-snapshot 
 */
export interface WindowGetMaxSizeParams {
    /** Target window id; omit to address the calling window. */
    windowId?: string;
}

/**
 * Parameters for `window.setResizable`.
 *
 * `windowId` is read by the shared window-target resolver in a separate
 * translation unit, invisible to the auto-generator's single-TU AST walk.
 *
 * @codegen-override params:window.setResizable
 * @codegen-snapshot resizable:primitive
 */
export interface WindowSetResizableParams {
    /** Target window id; omit to address the calling window. */
    windowId?: string;
    /** Whether the user may resize the window. @default true */
    resizable?: boolean;
}

/**
 * Parameters for `window.isResizable`.
 *
 * `windowId` is read by the shared window-target resolver in a separate
 * translation unit, invisible to the auto-generator's single-TU AST walk.
 *
 * @codegen-override params:window.isResizable
 * @codegen-snapshot 
 */
export interface WindowIsResizableParams {
    /** Target window id; omit to address the calling window. */
    windowId?: string;
}
