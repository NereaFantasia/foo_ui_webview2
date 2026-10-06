import { bridge } from '../Bridge.js';
import { call } from '../call.js';
import type {
    WindowBackdropPolicyPatch,
    WindowCreatePopupOptions,
    WindowListResponse,
    WindowSetPopupBehaviorOptions,
} from '../../types/responses.js';
import type { JsonValue } from '../../types/json.js';
import type { WindowRegion } from '../../types/generated/schema-types.js';
import type {
    WindowFlashParams,
    WindowSetAcrylicParams,
    WindowSetBackgroundTransparencyParams,
    WindowSetBlurParams,
    WindowSetBoundsParams,
    WindowSetClickThroughExcludeRegionsParams,
    WindowSetClickThroughParams,
    WindowSetDevServerConfigParams,
    WindowSetMicaEffectParams,
    WindowSetMicaParams,
} from '../../types/generated/params.js';

/**
 * `ui` — window-management namespace.
 */
export const ui = {
    // === Basic window controls ===
    minimize: () => call('window.minimize'),
    maximize: () => call('window.maximize'),
    restore: () => call('window.restore'),
    close: () => call('window.close'),
    toggleMaximize: () =>
        call('window.toggleMaximize'),
    startDrag: () => call('window.startDrag'),
    startResize: (edge: string) =>
        call('window.startResize', {
            edge,
        }),
    reload: () => call('window.reload'),

    // === State queries ===
    getState: () => call('window.getState'),
    /** Resolves with `{ maximized, isMaximized }` (both carry the same value). */
    isMaximized: () =>
        call('window.isMaximized'),
    /** Resolves with `{ minimized }` (the host does not send an `isMinimized` alias). */
    isMinimized: () =>
        call('window.isMinimized'),
    /**
     * Resolves with `{ fullscreen, isFullscreen, windowId }`. `windowId` targets another window;
     * omitted, the calling window.
     */
    isFullscreen: (windowId?: string) =>
        call('window.isFullscreen', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /** Resolves with `{ enabled, isAlwaysOnTop }` (both carry the same value). */
    isAlwaysOnTop: () =>
        call('window.isAlwaysOnTop'),
    /** `windowId` targets another window; omitted, the calling window. */
    isResizable: (windowId?: string) =>
        call('window.isResizable', {
            ...(windowId != null ? { windowId } : {}),
        }),
    getTitle: () => call('window.getTitle'),
    /** Resolves with `{ mode, panelMode, windowId }`; read it on startup to adapt to panel mode. */
    getMode: () => call('window.getMode'),

    // === Position and size ===
    setPosition: (x: number, y: number) =>
        call('window.setPosition', {
            x,
            y,
        }),
    setSize: (width: number, height: number) =>
        call('window.setSize', {
            width,
            height,
        }),
    setTitle: (title: string) =>
        call('window.setTitle', {
            title,
        }),
    getBounds: () => call('window.getBounds'),
    /**
     * Only `x`, `y`, `width` and `height` are sent, so the object `getBounds` resolved with can be
     * edited and passed back as is.
     */
    setBounds: ({ x, y, width, height }: WindowSetBoundsParams) =>
        call('window.setBounds', {
            x,
            y,
            width,
            height,
        }),
    center: () => call('window.center'),
    hasSavedBounds: () =>
        call('window.hasSavedBounds'),

    // === Size constraints ===
    /** Physical pixels. `windowId` targets another window; omitted, the calling window. */
    setMinSize: (width: number, height: number, windowId?: string) =>
        call('window.setMinSize', {
            width,
            height,
            ...(windowId != null ? { windowId } : {}),
        }),
    /** `windowId` targets another window; omitted, the calling window. */
    getMinSize: (windowId?: string) =>
        call('window.getMinSize', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /**
     * Physical pixels; `0` removes the bound. `windowId` targets another window; omitted, the
     * calling window.
     */
    setMaxSize: (width: number, height: number, windowId?: string) =>
        call('window.setMaxSize', {
            width,
            height,
            ...(windowId != null ? { windowId } : {}),
        }),
    /** `windowId` targets another window; omitted, the calling window. */
    getMaxSize: (windowId?: string) =>
        call('window.getMaxSize', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /** `windowId` targets another window; omitted, the calling window. */
    setResizable: (resizable: boolean, windowId?: string) =>
        call('window.setResizable', {
            resizable,
            ...(windowId != null ? { windowId } : {}),
        }),

    // === Always-on-top ===
    setAlwaysOnTop: (enabled: boolean) =>
        call('window.setAlwaysOnTop', {
            enabled,
        }),
    toggleAlwaysOnTop: () =>
        call('window.toggleAlwaysOnTop'),

    // === Fullscreen ===
    /** `windowId` targets another window; omitted, the calling window. */
    toggleFullscreen: (windowId?: string) =>
        call('window.toggleFullscreen', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /**
     * Fails with `OPERATION_FAILED` when the window is already fullscreen. `windowId` targets
     * another window; omitted, the calling window.
     */
    enterFullscreen: (windowId?: string) =>
        call('window.enterFullscreen', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /**
     * Fails with `OPERATION_FAILED` when the window is not fullscreen. `windowId` targets another
     * window; omitted, the calling window.
     */
    exitFullscreen: (windowId?: string) =>
        call('window.exitFullscreen', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /** `windowId` targets another window; omitted, the calling window. */
    setFullscreen: (enabled: boolean, windowId?: string) =>
        call('window.setFullscreen', {
            enabled,
            ...(windowId != null ? { windowId } : {}),
        }),

    // === Focus ===
    focus: (windowId?: string) =>
        call(
            'window.focus',
            windowId ? { windowId } : {},
        ),
    blur: () => call('window.blur'),
    flash: (opts: WindowFlashParams) =>
        call('window.flash', opts),
    flashTaskbar: (count?: number) =>
        call('window.flashTaskbar', {
            ...(count != null ? { count } : {}),
        }),
    showSystemMenu: (x: number, y: number, w?: number, h?: number) =>
        call('window.showSystemMenu', {
            x,
            y,
            ...(w != null ? { w, h } : {}),
        }),

    // === DWM effects ===
    setMica: (opts: WindowSetMicaParams = {}) =>
        call('window.setMica', opts),
    setMicaEffect: (opts: WindowSetMicaEffectParams = {}) =>
        call('window.setMicaEffect', opts),
    setAcrylic: (opts: WindowSetAcrylicParams) =>
        call('window.setAcrylic', opts),
    setBlur: (opts: WindowSetBlurParams) =>
        call('window.setBlur', opts),
    /** `windowId` targets another window; omitted, the calling window. */
    setDarkMode: (enabled: boolean, windowId?: string) =>
        call('window.setDarkMode', {
            enabled,
            ...(windowId != null ? { windowId } : {}),
        }),
    setBackgroundTransparency: (opts: WindowSetBackgroundTransparencyParams) =>
        call(
            'window.setBackgroundTransparency',
            opts,
        ),
    refreshWebView: () =>
        call('window.refreshWebView'),
    setCornerPreference: (mode: string) =>
        call('window.setCornerPreference', {
            mode,
        }),
    /** Resolves with `{ mode, preference }` (both carry the same value). */
    getCornerPreference: () =>
        call('window.getCornerPreference'),

    // === Titlebar ===
    getTitlebarHeight: () =>
        call('window.getTitlebarHeight'),
    setTitlebarHeight: (height: number) =>
        call('window.setTitlebarHeight', {
            height,
        }),
    /** Resolves with `{ width, buttonWidth }`: all three buttons, and one of them. */
    getCaptionButtonsWidth: () =>
        call('window.getCaptionButtonsWidth'),
    getTitlebarInfo: () =>
        call('window.getTitlebarInfo'),
    setDragRegions: (regions: WindowRegion[]) =>
        call('window.setDragRegions', {
            regions,
        }),
    clearDragRegions: () =>
        call('window.clearDragRegions'),
    setNoDragRegions: (regions: WindowRegion[]) =>
        call('window.setNoDragRegions', {
            regions,
        }),
    clearNoDragRegions: () =>
        call('window.clearNoDragRegions'),
    /**
     * Where the page draws the main window's maximize button, in CSS pixels, so
     * that on Windows 11 hovering it offers Snap layouts. Omit `region` to remove
     * it. The host passes the mouse input on the button back to the page, so its
     * hover styles and click handler keep working; call again whenever layout
     * moves the button. Main window only.
     */
    setMaximizeButtonRegion: (region?: WindowRegion) =>
        call('window.setMaximizeButtonRegion', region ? { region } : {}),
    /** `windowId` targets another window; omitted, the calling window. */
    setFrameless: (frameless: boolean, windowId?: string) =>
        call('window.setFrameless', {
            frameless,
            ...(windowId != null ? { windowId } : {}),
        }),

    // === Multi-window ===
    createPopup: (opts: WindowCreatePopupOptions) =>
        call('window.createPopup', opts),
    closePopup: (windowId: string) =>
        call('window.closePopup', {
            windowId,
        }),
    closeAllPopups: () => call('window.closeAllPopups'),
    /** Entries are discriminated by `isMain`: popups carry `url`, `profile`, `behavior` and `resolvedBehavior`. */
    // The declaration types every entry with the popup-only fields optional;
    // the host fills them exactly when `isMain` is false.
    getAllWindows: () => call('window.getAllWindows') as Promise<WindowListResponse>,
    getCurrentWindowId: () =>
        call('window.getCurrentWindowId'),
    /** Popups only; `windowId` omitted, the calling popup. */
    getPopupBehavior: (windowId?: string) =>
        call('window.getPopupBehavior', {
            ...(windowId != null ? { windowId } : {}),
        }),
    setPopupBehavior: (opts: WindowSetPopupBehaviorOptions) =>
        call('window.setPopupBehavior', opts),
    /** `windowId` targets another window; omitted, the calling window. */
    getBackdropPolicy: (windowId?: string) =>
        call('window.getBackdropPolicy', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /**
     * Takes the policy fields flat and sends them as `backdropPolicy`; a `null` field removes that
     * override. `windowId` targets another window; omitted, the calling window.
     */
    setBackdropPolicy: ({ windowId, ...policy }: WindowBackdropPolicyPatch & { windowId?: string }) =>
        call('window.setBackdropPolicy', {
            ...(windowId != null ? { windowId } : {}),
            backdropPolicy: policy,
        }),
    setClickThrough: (opts: WindowSetClickThroughParams) =>
        call('window.setClickThrough', opts),
    /** `windowId` targets another popup; omitted, the calling window. */
    isClickThrough: (windowId?: string) =>
        call('window.isClickThrough', {
            ...(windowId != null ? { windowId } : {}),
        }),
    setClickThroughExcludeRegions: (opts: WindowSetClickThroughExcludeRegionsParams) =>
        call(
            'window.setClickThroughExcludeRegions',
            opts,
        ),
    /** `windowId` targets another popup; omitted, the calling window. */
    clearClickThroughExcludeRegions: (windowId?: string) =>
        call(
            'window.clearClickThroughExcludeRegions',
            { ...(windowId != null ? { windowId } : {}) },
        ),
    sendMessage: (targetWindowId: string, message: JsonValue) =>
        call('window.sendMessage', {
            targetWindowId,
            message,
        }),
    broadcast: (message: JsonValue) =>
        call('window.broadcast', {
            message,
        }),
    cancelClose: () => call('window.cancelClose'),
    confirmClose: () => call('window.confirmClose'),

    // === Zoom and DPI ===
    getDpiScale: () => call('window.getDpiScale'),
    setZoom: (zoom: number) =>
        call('window.setZoom', {
            zoom,
        }),
    getZoom: () => call('window.getZoom'),
    resetZoom: () => call('window.resetZoom'),
    setZoomForDpi: (dpi?: number) =>
        call('window.setZoomForDpi', {
            ...(dpi != null ? { dpi } : {}),
        }),

    // === Dev server ===
    getDevServerConfig: () =>
        call('window.getDevServerConfig'),
    setDevServerConfig: (opts: WindowSetDevServerConfigParams) =>
        call('window.setDevServerConfig', opts),
    showContextMenu: (x?: number, y?: number) =>
        call('ui.showContextMenu', {
            ...(x != null ? { x, y } : {}),
        }),
};
