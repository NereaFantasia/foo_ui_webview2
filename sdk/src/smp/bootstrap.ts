import type { JsonObject } from '../types/json.js';
/**
 * SMP compatibility bootstrap orchestrator.
 *
 * Wires the cache, event subscriptions, plman methods, fb extensions,
 * `fb.onSMP` registry, window-level properties, and SMP wrapper
 * classes onto the provided runtime `fb` object. Returns the fully
 * populated {@link SmpCompatApi} that the caller installs onto
 * `window.smp`.
 *
 * Idempotent — re-invoking with the same `fb` is safe because every
 * extension uses `_defineIfMissing` / `__smpCompatLoaded` guards.
 *
 */

import type { SmpBridgeShape } from './bridgeShape.js';
import {
    attachCacheEventSubscriptions,
    createInitialCache,
    createPlaylistRefresher,
    populateCache,
} from './cache.js';
import { ContextMenuManager } from './classes/ContextMenuManager.js';
import { FbFileInfo } from './classes/FbFileInfo.js';
import { FbMetadbHandle } from './classes/FbMetadbHandle.js';
import { FbMetadbHandleList } from './classes/FbMetadbHandleList.js';
import { FbProfiler } from './classes/FbProfiler.js';
import { FbTitleFormat } from './classes/FbTitleFormat.js';
import { FbUiSelectionHolder } from './classes/FbUiSelectionHolder.js';
import { MainMenuManager } from './classes/MainMenuManager.js';
import { createOnSmp } from './eventMap.js';
import { attachFbExtensions } from './fbExtensions.js';
import { formatHandleId, parseHandleId, stripSubsongSuffix } from './handleId.js';
import { buildPlman } from './plman.js';
import type { SmpCompatApi, SmpEventCallback, SmpEventName } from './types.js';
import { smpUtils } from './utils.js';
import { smpUtilsNamespace } from './utilsCompat.js';
import { attachWindowProperties } from './windowProperties.js';

const LOG_PREFIX = '[SMP-Compat]';

/**
 * The window-level globals the SMP IIFE installs alongside the main
 * `window.smp` API. Surfacing as a single shape keeps the bootstrap
 * return value self-documenting — every value is intended to be hung
 * off `window.*` by the caller.
 */
export interface SmpBootstrapResult {
    /** Public surface for `window.smp`. */
    smp: SmpCompatApi;
    /** Public surface for `window.plman`. */
    plman: ReturnType<typeof buildPlman>;
    /** Shared `smpUtils` helpers (`window.smpUtils`). */
    smpUtils: typeof smpUtils;
    /** SMP-style `utils` namespace (`window.utils`). */
    utils: typeof smpUtilsNamespace;
    /** Constructors for `window.FbMetadbHandle` etc. */
    classes: {
        FbFileInfo: typeof FbFileInfo;
        FbMetadbHandle: typeof FbMetadbHandle;
        FbMetadbHandleList: typeof FbMetadbHandleList;
        FbProfiler: typeof FbProfiler;
        FbTitleFormat: typeof FbTitleFormat;
        FbUiSelectionHolder: typeof FbUiSelectionHolder;
        ContextMenuManager: typeof ContextMenuManager;
        MainMenuManager: typeof MainMenuManager;
    };
}

/**
 * Bootstrap the SMP compatibility layer against `fb`.
 *
 * Side effects performed:
 *   1. Build the cache + dispose array.
 *   2. Wire `bridge.on(...)` subscriptions for cache sync.
 *   3. Build `plman` using {@link buildPlman}.
 *   4. Attach SMP-style methods / properties to `fb` via {@link attachFbExtensions}.
 *   5. Attach `fb.onSMP(...)` via {@link createOnSmp}.
 *   6. Attach `window.GetProperty / SetProperty / NotifyOthers` via {@link attachWindowProperties}.
 *   7. Kick off the initial `populateCache` + `wrappersInstalled.resolve()`.
 *
 * @returns A {@link SmpBootstrapResult} aggregate the caller should
 *          install onto `window.*`. The IIFE entry does that at script
 *          load; ESM consumers may install selectively.
 */
export function bootstrapSmpCompat(fb: SmpBridgeShape): SmpBootstrapResult {
    const cache = createInitialCache();
    const { schedule: schedulePlaylistRefresh, refresh: refreshPlaylists } =
        createPlaylistRefresher(fb, cache);

    const eventUnsubscribers = attachCacheEventSubscriptions(
        fb,
        cache,
        schedulePlaylistRefresh,
    );

    const plman = buildPlman(fb, cache, schedulePlaylistRefresh);

    attachFbExtensions(fb, plman, cache, schedulePlaylistRefresh);
    attachWindowProperties(fb);

    // Attach `fb.onSMP` (mutates the runtime `fb` object directly).
    const fbExt = fb as SmpBridgeShape & {
        onSMP?: (event: SmpEventName | string, cb: SmpEventCallback) => () => void;
    };
    if (typeof fbExt.onSMP !== 'function') {
        Object.defineProperty(fbExt, 'onSMP', {
            configurable: true,
            value: createOnSmp(fb),
        });
    }

    const dispose = (): void => {
        for (const unsub of eventUnsubscribers) {
            try {
                if (typeof unsub === 'function') unsub();
            } catch {
                /* swallow — best-effort cleanup */
            }
        }
        eventUnsubscribers.length = 0;
        try {
            // eslint-disable-next-line no-console
            console.log(LOG_PREFIX, 'disposed - all event listeners removed');
        } catch {
            /* ignore */
        }
    };

    const refreshCache = async (): Promise<void> => {
        await populateCache(fb, cache);
        try {
            // eslint-disable-next-line no-console
            console.log(LOG_PREFIX, 'cache refreshed (full)');
        } catch {
            /* ignore */
        }
    };

    // Kick off the initial cache population (including paths and
    // version) and resolve the `ready` promise as soon as it finishes,
    // whether successfully or with an internal failure.
    const ready: Promise<boolean> = (async () => {
        try {
            await populateCache(fb, cache, { includePaths: true });
        } catch {
            // Swallow population errors so `ready` always resolves;
            // surface them via the cached warn channel only.
        }
        return true;
    })();

    // The shared `_invoke` wrapper exposed as `smp.invoke`.
    const invoke: SmpCompatApi['invoke'] = (method, params) =>
        fb.invoke(method, params ?? {});

    const smp: SmpCompatApi = {
        ready,
        cache,
        invoke,
        parseHandleId,
        formatHandleId,
        stripSubsongSuffix,
        refreshCache,
        dispose,
        __smpCompatLoaded: true,
    };

    // Expose `fb._cache` as a non-enumerable debugging hook.
    const fbWithCache = fb as SmpBridgeShape & { _cache?: typeof cache };
    if (!fbWithCache._cache) {
        Object.defineProperty(fbWithCache, '_cache', {
            value: cache,
            enumerable: false,
            configurable: true,
        });
    }

    // Touch refreshPlaylists so consumers (and TS unused-vars) see
    // a reachable reference even though the schedule wrapper covers
    // the immediate path.
    void refreshPlaylists;

    return {
        smp,
        plman,
        smpUtils,
        utils: smpUtilsNamespace,
        classes: {
            FbFileInfo,
            FbMetadbHandle,
            FbMetadbHandleList,
            FbProfiler,
            FbTitleFormat,
            FbUiSelectionHolder,
            ContextMenuManager,
            MainMenuManager,
        },
    };
}

/**
 * Install the bootstrap result onto `globalThis` / `window` for
 * SMP-compatible script consumers. Idempotent — every assignment is
 * gated by the `__smpCompatLoaded` flag and `hasOwnProperty` checks.
 */
export function installSmpGlobals(result: SmpBootstrapResult): void {
    if (typeof globalThis === 'undefined') return;
    const g = globalThis as typeof globalThis & {
        smp?: SmpCompatApi;
        plman?: SmpBootstrapResult['plman'];
        smpUtils?: SmpBootstrapResult['smpUtils'];
        utils?: SmpBootstrapResult['utils'] & { __smpUtilsCompat?: boolean };
    };

    // Re-entry guard: bail out if a previous bootstrap has already
    // installed the globals.
    if (g.smp && (g.smp as SmpCompatApi).__smpCompatLoaded) {
        return;
    }

    g.smp = result.smp;
    g.plman = result.plman;
    g.smpUtils = result.smpUtils;
    if (!g.utils?.__smpUtilsCompat) {
        g.utils = result.utils;
    }

    // Wrapper class globals (`window.FbMetadbHandle` etc.). They are
    // bundled statically so the order of script tags does not matter.
    // `Reflect.set` is used to avoid widening `g` with an open
    // `Record<string, unknown>` index signature.
    for (const [name, ctor] of Object.entries(result.classes)) {
        if (!Object.prototype.hasOwnProperty.call(g, name)) {
            Reflect.set(g, name, ctor);
        }
    }
}
