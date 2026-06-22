/**
 * `foo-webview-sdk/smp-compat` — Spider Monkey Panel compatibility entry.
 *
 * ESM consumers import the bootstrap helper plus the wrapper classes
 * and types directly:
 *
 * ```ts
 * import { bootstrapSmpCompat, FbMetadbHandle } from 'foo-webview-sdk/smp-compat';
 * import { fb } from 'foo-webview-sdk/bridge';
 *
 * const { smp, plman } = bootstrapSmpCompat(fb);
 * window.smp = smp;
 * window.plman = plman;
 * ```
 *
 * The IIFE bundle (`dist/smp-compat.global.js`) consumed by `<script>`-
 * tag themes wires the same call automatically — see
 * {@link "./iife"}. Reading `window.fb` from the IIFE keeps a single
 * runtime `fb` instance shared across `dist/bridge.global.js` +
 * `dist/smp-compat.global.js`.
 */

// Bootstrap orchestrator
export {
    bootstrapSmpCompat,
    installSmpGlobals,
    type SmpBootstrapResult,
} from './bootstrap.js';

// Public type re-exports for SMP compatibility consumers.
export type {
    SMPPlman,
    SmpCompatApi,
    SmpCompatCache,
    SmpEventCallback,
    SmpEventName,
    SmpHandleId,
    SmpHandleLike,
    SmpMenuBuildState,
    SmpPlaybackQueueItem,
    SmpRawMenuItem,
    SmpStructuredMenuItem,
} from './types.js';
export { SMP_MENU_FLAGS } from './types.js';

// Wrapper classes (concrete export — IIFE side-effect installs them
// on `window.*` via `installSmpGlobals`).
export { FbFileInfo } from './classes/FbFileInfo.js';
export { FbMetadbHandle } from './classes/FbMetadbHandle.js';
export { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
export { FbProfiler } from './classes/FbProfiler.js';
export { FbTitleFormat } from './classes/FbTitleFormat.js';
export { FbUiSelectionHolder } from './classes/FbUiSelectionHolder.js';
export { ContextMenuManager } from './classes/ContextMenuManager.js';
export { MainMenuManager } from './classes/MainMenuManager.js';

// Handle-id helpers (also exposed on `window.smp.*`)
export {
    HANDLE_TOKEN,
    formatHandleId,
    parseHandleId,
    stripSubsongSuffix,
} from './handleId.js';

// Shared smpUtils helpers (`window.smpUtils`)
export {
    MENU_FLAGS,
    buildMenuItems,
    clamp,
    getInvoke,
    normalizeHandleList,
    sleep,
    smpUtils,
    toHandleId,
    toHandleIdArray,
    type SmpInvokeFn,
    type SmpUtilsNamespace,
} from './utils.js';

// SMP `utils` namespace (`window.utils`)
export {
    smpUtilsNamespace,
    type SmpUtilsNamespaceShape,
} from './utilsCompat.js';

// Event map exports (advanced use only — most consumers use `fb.onSMP`)
export {
    SMP_EVENT_MAP,
    SMP_PARAM_ADAPTERS,
    createOnSmp,
} from './eventMap.js';
