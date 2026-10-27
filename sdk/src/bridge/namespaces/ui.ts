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
    /** Minimize the calling window with the system animation. */
    minimize: () => call('window.minimize'),
    /** Maximize the calling window with the system animation. */
    maximize: () => call('window.maximize'),
    /** Restore the calling window from minimized or maximized state. */
    restore: () => call('window.restore'),
    /** Close the calling window as the system close command does. */
    close: () => call('window.close'),
    /** Maximize the calling window, or restore it when it is maximized. */
    toggleMaximize: () =>
        call('window.toggleMaximize'),
    /** Start moving the calling window with the mouse. Call from a `mousedown` handler. */
    startDrag: () => call('window.startDrag'),
    /** Start resizing the calling window with the mouse along the specified edge. */
    startResize: (edge: string) =>
        call('window.startResize', {
            edge,
        }),
    /** Reload the calling window's page. */
    reload: () => call('window.reload'),

    // === State queries ===
    /** Report the calling window's state flags and bounding rectangle. */
    getState: () => call('window.getState'),
    /** Resolve with `{ maximized, isMaximized }` (both carry the same value). */
    isMaximized: () =>
        call('window.isMaximized'),
    /** Resolve with `{ minimized }` (the host does not send an `isMinimized` alias). */
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
    /** Resolve with `{ enabled, isAlwaysOnTop }` (both carry the same value). */
    isAlwaysOnTop: () =>
        call('window.isAlwaysOnTop'),
    /** `windowId` targets another window; omitted, the calling window. */
    isResizable: (windowId?: string) =>
        call('window.isResizable', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /** Report the title of the calling window. */
    getTitle: () => call('window.getTitle'),
    /** Resolve with `{ mode, panelMode, windowId }`; read it on startup to adapt to panel mode. */
    getMode: () => call('window.getMode'),

    // === Position and size ===
    /** Set the calling window's top-left corner coordinates on screen. */
    setPosition: (x: number, y: number) =>
        call('window.setPosition', {
            x,
            y,
        }),
    /** Set the calling window's width and height in pixels. */
    setSize: (width: number, height: number) =>
        call('window.setSize', {
            width,
            height,
        }),
    /** Set the calling window's title text. */
    setTitle: (title: string) =>
        call('window.setTitle', {
            title,
        }),
    /** Report the calling window's position and dimensions in pixels. */
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
    /** Center the calling window on the monitor it currently sits on. */
    center: () => call('window.center'),
    /** Report whether the calling window has saved position and dimensions. */
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
    /** Enable or disable keeping the calling window on top of others. */
    setAlwaysOnTop: (enabled: boolean) =>
        call('window.setAlwaysOnTop', {
            enabled,
        }),
    /** Flip the always-on-top state of the calling window. */
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
    /**
     * Show, restore and request foreground activation for the exact window id,
     * or the caller's top-level window when omitted (including a panel's host).
     * Supersedes older popup focus-restoration work. Windows may refuse foreground
     * activation; a successful response only confirms that the request was made.
     */
    focus: (windowId?: string) =>
        call(
            'window.focus',
            windowId ? { windowId } : {},
        ),
    /** Remove keyboard focus from the calling window. */
    blur: () => call('window.blur'),
    /** Flash the calling window's title bar or frame. */
    flash: (opts: WindowFlashParams) =>
        call('window.flash', opts),
    /** Flash the calling window's taskbar button. */
    flashTaskbar: (count?: number) =>
        call('window.flashTaskbar', {
            ...(count != null ? { count } : {}),
        }),
    /** Show the system window menu (Restore, Move, Size, Minimize, Maximize, Close). */
    showSystemMenu: (x: number, y: number, w?: number, h?: number) =>
        call('window.showSystemMenu', {
            x,
            y,
            ...(w != null ? { w, h } : {}),
        }),

    // === DWM effects ===
    /** Enable or disable the Mica backdrop material on the window. */
    setMica: (opts: WindowSetMicaParams = {}) =>
        call('window.setMica', opts),
    /** Same as {@link setMica}. */
    setMicaEffect: (opts: WindowSetMicaEffectParams = {}) =>
        call('window.setMicaEffect', opts),
    /** Enable or disable the Acrylic backdrop material on the window. */
    setAcrylic: (opts: WindowSetAcrylicParams) =>
        call('window.setAcrylic', opts),
    /** Enable or disable the blur-behind effect on the window. */
    setBlur: (opts: WindowSetBlurParams) =>
        call('window.setBlur', opts),
    /** `windowId` targets another window; omitted, the calling window. */
    setDarkMode: (enabled: boolean, windowId?: string) =>
        call('window.setDarkMode', {
            enabled,
            ...(windowId != null ? { windowId } : {}),
        }),
    /** Set the WebView background to transparent or opaque. */
    setBackgroundTransparency: (opts: WindowSetBackgroundTransparencyParams) =>
        call(
            'window.setBackgroundTransparency',
            opts,
        ),
    /** Redraw the calling window's WebView to clear rendering artifacts. */
    refreshWebView: () =>
        call('window.refreshWebView'),
    /** Set the main window's Windows 11 corner rounding preference. */
    setCornerPreference: (mode: string) =>
        call('window.setCornerPreference', {
            mode,
        }),
    /** Resolve with `{ mode, preference }` (both carry the same value). */
    getCornerPreference: () =>
        call('window.getCornerPreference'),

    // === Titlebar ===
    /** Report the main window's title bar height in pixels. */
    getTitlebarHeight: () =>
        call('window.getTitlebarHeight'),
    /** Set the main window's title bar height in pixels. */
    setTitlebarHeight: (height: number) =>
        call('window.setTitlebarHeight', {
            height,
        }),
    /** Resolve with `{ width, buttonWidth }`: all three buttons, and one of them. */
    getCaptionButtonsWidth: () =>
        call('window.getCaptionButtonsWidth'),
    /** Report the main window's title bar height, caption button widths, and maximized state. */
    getTitlebarInfo: () =>
        call('window.getTitlebarInfo'),
    /** Register non-client drag regions where dragging moves the window. */
    setDragRegions: (regions: WindowRegion[]) =>
        call('window.setDragRegions', {
            regions,
        }),
    /** Clear all registered drag regions on the window. */
    clearDragRegions: () =>
        call('window.clearDragRegions'),
    /** Register regions excluded from window dragging (e.g. interactive controls). */
    setNoDragRegions: (regions: WindowRegion[]) =>
        call('window.setNoDragRegions', {
            regions,
        }),
    /** Clear all registered no-drag regions on the window. */
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
    /** Create a secondary popup window hosting a WebView. */
    createPopup: (opts: WindowCreatePopupOptions) =>
        call('window.createPopup', opts),
    /** Close a secondary popup window by its window ID. */
    closePopup: (windowId: string) =>
        call('window.closePopup', {
            windowId,
        }),
    /** Close all open secondary popup windows. */
    closeAllPopups: () => call('window.closeAllPopups'),
    /** Entries are discriminated by `isMain`: popups carry `url`, `profile`, `behavior` and `resolvedBehavior`. */
    // The declaration types every entry with the popup-only fields optional;
    // the host fills them exactly when `isMain` is false.
    getAllWindows: () => call('window.getAllWindows') as Promise<WindowListResponse>,
    /** Report the window ID of the calling window. */
    getCurrentWindowId: () =>
        call('window.getCurrentWindowId'),
    /** Popups only; `windowId` omitted, the calling popup. */
    getPopupBehavior: (windowId?: string) =>
        call('window.getPopupBehavior', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /** Configure behavior flags (e.g. always-on-top, focus, frame) for a popup window. */
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
    /** Enable or disable click-through transparency on a popup window. */
    setClickThrough: (opts: WindowSetClickThroughParams) =>
        call('window.setClickThrough', opts),
    /** `windowId` targets another popup; omitted, the calling window. */
    isClickThrough: (windowId?: string) =>
        call('window.isClickThrough', {
            ...(windowId != null ? { windowId } : {}),
        }),
    /** Define interactive exclude regions within a click-through popup window. */
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
    /** Send an inter-window JSON message to a target window by its ID. */
    sendMessage: (targetWindowId: string, message: JsonValue) =>
        call('window.sendMessage', {
            targetWindowId,
            message,
        }),
    /** Broadcast an inter-window JSON message to all windows. */
    broadcast: (message: JsonValue) =>
        call('window.broadcast', {
            message,
        }),
    /** Cancel window closure from a `window:beforeClose` handler. */
    cancelClose: () => call('window.cancelClose'),
    /** Proceed with closing the window from a `window:beforeClose` handler. */
    confirmClose: () => call('window.confirmClose'),

    // === Zoom and DPI ===
    /** Report the DPI scaling factor of the monitor the calling window sits on. */
    getDpiScale: () => call('window.getDpiScale'),
    /** Set the zoom factor for the calling window's WebView. */
    setZoom: (zoom: number) =>
        call('window.setZoom', {
            zoom,
        }),
    /** Report the current zoom factor of the calling window's WebView. */
    getZoom: () => call('window.getZoom'),
    /** Reset the zoom factor of the calling window's WebView to 1.0. */
    resetZoom: () => call('window.resetZoom'),
    /** Set or recalculate the WebView zoom factor adapted to the monitor DPI. */
    setZoomForDpi: (dpi?: number) =>
        call('window.setZoomForDpi', {
            ...(dpi != null ? { dpi } : {}),
        }),

    // === Dev server ===
    /** Report development server settings and address for hot reload. */
    getDevServerConfig: () =>
        call('window.getDevServerConfig'),
    /** Configure the development server settings for the next page load. */
    setDevServerConfig: (opts: WindowSetDevServerConfigParams) =>
        call('window.setDevServerConfig', opts),
    /** Open the main window's native context menu at coordinates or cursor position. */
    showContextMenu: (x?: number, y?: number) =>
        call('ui.showContextMenu', {
            ...(x != null ? { x, y } : {}),
        }),
};
