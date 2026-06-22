/**
 * `ui` — window-management namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    DpiScaleResponse,
    WindowBackdropPolicyPatch,
    WindowBackdropPolicyResponse,
    WindowBackdropPolicyState,
    WindowBounds,
    WindowDevServerConfig,
    WindowListResponse,
    WindowPopupBehaviorPatch,
    WindowPopupBehaviorResponse,
    WindowPopupBehaviorState,
    WindowState,
    WindowTitlebarInfo,
} from '../../types/responses.js';
import type { JsonObject } from '../../types/json.js';
import type {
    WindowCreatePopupParams,
    WindowFlashParams,
    WindowSetAcrylicParams,
    WindowSetBackgroundTransparencyParams,
    WindowSetBlurParams,
    WindowSetClickThroughExcludeRegionsParams,
    WindowSetClickThroughParams,
    WindowSetDevServerConfigParams,
} from '../../types/generated/params.js';

export const ui = {
    // === Basic window controls ===
    minimize: () => bridge.invoke<BaseResponse>('window.minimize'),
    maximize: () => bridge.invoke<BaseResponse>('window.maximize'),
    restore: () => bridge.invoke<BaseResponse>('window.restore'),
    close: () => bridge.invoke<BaseResponse>('window.close'),
    toggleMaximize: () =>
        bridge.invoke<BaseResponse & { maximized?: boolean }>(
            'window.toggleMaximize',
        ),
    startDrag: () => bridge.invoke<BaseResponse>('window.startDrag'),
    startResize: (edge: string) =>
        bridge.invoke<BaseResponse>('window.startResize', { edge }),
    reload: () => bridge.invoke<BaseResponse>('window.reload'),

    // === State queries ===
    getState: () => bridge.invoke<WindowState>('window.getState'),
    isMaximized: () =>
        bridge.invoke<{ isMaximized: boolean }>('window.isMaximized'),
    isMinimized: () =>
        bridge.invoke<{ isMinimized: boolean }>('window.isMinimized'),
    isFullscreen: () =>
        bridge.invoke<{ isFullscreen: boolean }>('window.isFullscreen'),
    isAlwaysOnTop: () =>
        bridge.invoke<{ alwaysOnTop: boolean }>('window.isAlwaysOnTop'),
    isResizable: () =>
        bridge.invoke<{ resizable: boolean }>('window.isResizable'),
    getTitle: () => bridge.invoke<{ title: string }>('window.getTitle'),
    getMode: () => bridge.invoke<{ mode: string }>('window.getMode'),

    // === Position and size ===
    setPosition: (x: number, y: number) =>
        bridge.invoke<BaseResponse>('window.setPosition', { x, y }),
    setSize: (width: number, height: number) =>
        bridge.invoke<BaseResponse>('window.setSize', { width, height }),
    setTitle: (title: string) =>
        bridge.invoke<BaseResponse>('window.setTitle', { title }),
    getBounds: () => bridge.invoke<WindowBounds>('window.getBounds'),
    setBounds: (opts: Partial<WindowBounds>) =>
        bridge.invoke<BaseResponse>('window.setBounds', opts),
    center: () => bridge.invoke<BaseResponse>('window.center'),
    hasSavedBounds: () =>
        bridge.invoke<{ hasSavedBounds: boolean }>('window.hasSavedBounds'),

    // === Size constraints ===
    setMinSize: (width: number, height: number) =>
        bridge.invoke<BaseResponse>('window.setMinSize', { width, height }),
    getMinSize: () =>
        bridge.invoke<{ width: number; height: number }>('window.getMinSize'),
    setMaxSize: (width: number, height: number) =>
        bridge.invoke<BaseResponse>('window.setMaxSize', { width, height }),
    getMaxSize: () =>
        bridge.invoke<{ width: number; height: number }>('window.getMaxSize'),
    setResizable: (resizable: boolean) =>
        bridge.invoke<BaseResponse>('window.setResizable', { resizable }),

    // === Always-on-top ===
    setAlwaysOnTop: (enabled: boolean) =>
        bridge.invoke<BaseResponse>('window.setAlwaysOnTop', { enabled }),
    toggleAlwaysOnTop: () =>
        bridge.invoke<BaseResponse & { enabled?: boolean }>(
            'window.toggleAlwaysOnTop',
        ),

    // === Fullscreen ===
    toggleFullscreen: () =>
        bridge.invoke<BaseResponse & { fullscreen?: boolean }>(
            'window.toggleFullscreen',
        ),
    enterFullscreen: () =>
        bridge.invoke<BaseResponse & { isFullscreen?: boolean }>(
            'window.enterFullscreen',
        ),
    exitFullscreen: () =>
        bridge.invoke<BaseResponse & { isFullscreen?: boolean }>(
            'window.exitFullscreen',
        ),
    setFullscreen: (enabled: boolean) =>
        bridge.invoke<BaseResponse & { fullscreen?: boolean }>(
            'window.setFullscreen',
            { enabled },
        ),

    // === Focus ===
    focus: (windowId?: string) =>
        bridge.invoke<BaseResponse>(
            'window.focus',
            windowId ? { windowId } : {},
        ),
    blur: () => bridge.invoke<BaseResponse>('window.blur'),
    flash: (opts: WindowFlashParams) =>
        bridge.invoke<BaseResponse>('window.flash', opts),
    flashTaskbar: (count?: number) =>
        bridge.invoke<BaseResponse>('window.flashTaskbar', {
            ...(count != null ? { count } : {}),
        }),
    showSystemMenu: (x: number, y: number, w?: number, h?: number) =>
        bridge.invoke<BaseResponse>('window.showSystemMenu', {
            x,
            y,
            ...(w != null ? { w, h } : {}),
        }),

    // === DWM effects ===
    setMica: (opts: JsonObject = {}) =>
        bridge.invoke<
            BaseResponse & {
                enabled?: boolean;
                variant?: string;
                darkMode?: boolean;
            }
        >('window.setMica', opts),
    setMicaEffect: (opts: JsonObject = {}) =>
        bridge.invoke<
            BaseResponse & {
                enabled?: boolean;
                variant?: string;
                darkMode?: boolean;
            }
        >('window.setMicaEffect', opts),
    setAcrylic: (opts: WindowSetAcrylicParams) =>
        bridge.invoke<BaseResponse>('window.setAcrylic', opts),
    setBlur: (opts: WindowSetBlurParams) =>
        bridge.invoke<BaseResponse>('window.setBlur', opts),
    setDarkMode: (enabled: boolean) =>
        bridge.invoke<BaseResponse>('window.setDarkMode', { enabled }),
    setBackgroundTransparency: (opts: WindowSetBackgroundTransparencyParams) =>
        bridge.invoke<BaseResponse & { description?: string }>(
            'window.setBackgroundTransparency',
            opts,
        ),
    refreshWebView: () => bridge.invoke<BaseResponse>('window.refreshWebView'),
    setCornerPreference: (mode: string) =>
        bridge.invoke<BaseResponse>('window.setCornerPreference', { mode }),
    getCornerPreference: () =>
        bridge.invoke<{ mode: string }>('window.getCornerPreference'),

    // === Titlebar ===
    getTitlebarHeight: () =>
        bridge.invoke<{ height: number }>('window.getTitlebarHeight'),
    setTitlebarHeight: (height: number) =>
        bridge.invoke<BaseResponse>('window.setTitlebarHeight', { height }),
    getCaptionButtonsWidth: () =>
        bridge.invoke<{ width: number }>('window.getCaptionButtonsWidth'),
    getTitlebarInfo: () =>
        bridge.invoke<WindowTitlebarInfo>('window.getTitlebarInfo'),
    setDragRegions: (regions: unknown[]) =>
        bridge.invoke<BaseResponse & { count?: number; dpiScale?: number }>(
            'window.setDragRegions',
            { regions },
        ),
    clearDragRegions: () =>
        bridge.invoke<BaseResponse>('window.clearDragRegions'),
    setNoDragRegions: (regions: unknown[]) =>
        bridge.invoke<BaseResponse & { count?: number; dpiScale?: number }>(
            'window.setNoDragRegions',
            { regions },
        ),
    clearNoDragRegions: () =>
        bridge.invoke<BaseResponse>('window.clearNoDragRegions'),
    setFrameless: (frameless: boolean) =>
        bridge.invoke<BaseResponse>('window.setFrameless', { frameless }),

    // === Multi-window ===
    createPopup: (opts: WindowCreatePopupParams) =>
        bridge.invoke<{ windowId: string }>('window.createPopup', opts),
    closePopup: (windowId: string) =>
        bridge.invoke<BaseResponse>('window.closePopup', { windowId }),
    closeAllPopups: () => bridge.invoke<BaseResponse>('window.closeAllPopups'),
    getAllWindows: () =>
        bridge.invoke<WindowListResponse>('window.getAllWindows'),
    getCurrentWindowId: () =>
        bridge.invoke<{ windowId: string }>('window.getCurrentWindowId'),
    getPopupBehavior: (windowId?: string) =>
        bridge.invoke<WindowPopupBehaviorState>('window.getPopupBehavior', {
            ...(windowId != null ? { windowId } : {}),
        }),
    setPopupBehavior: (
        opts: WindowPopupBehaviorPatch & { windowId?: string },
    ) =>
        bridge.invoke<WindowPopupBehaviorResponse>(
            'window.setPopupBehavior',
            opts as JsonObject,
        ),
    getBackdropPolicy: (windowId?: string) =>
        bridge.invoke<WindowBackdropPolicyState>('window.getBackdropPolicy', {
            ...(windowId != null ? { windowId } : {}),
        }),
    setBackdropPolicy: (
        opts: WindowBackdropPolicyPatch & { windowId?: string },
    ) =>
        bridge.invoke<WindowBackdropPolicyResponse>(
            'window.setBackdropPolicy',
            opts as JsonObject,
        ),
    setClickThrough: (opts: WindowSetClickThroughParams) =>
        bridge.invoke<BaseResponse & { clickThrough?: boolean }>(
            'window.setClickThrough',
            opts,
        ),
    isClickThrough: (windowId?: string) =>
        bridge.invoke<{ clickThrough: boolean }>('window.isClickThrough', {
            ...(windowId != null ? { windowId } : {}),
        }),
    setClickThroughExcludeRegions: (opts: WindowSetClickThroughExcludeRegionsParams) =>
        bridge.invoke<
            BaseResponse & {
                count?: number;
                dpiScale?: number;
                warning?: string;
            }
        >('window.setClickThroughExcludeRegions', opts),
    clearClickThroughExcludeRegions: (windowId?: string) =>
        bridge.invoke<BaseResponse>(
            'window.clearClickThroughExcludeRegions',
            { ...(windowId != null ? { windowId } : {}) },
        ),
    sendMessage: (targetWindowId: string, message: unknown) =>
        bridge.invoke<BaseResponse>('window.sendMessage', {
            targetWindowId,
            message,
        }),
    broadcast: (message: unknown) =>
        bridge.invoke<BaseResponse>('window.broadcast', { message }),
    cancelClose: () => bridge.invoke<BaseResponse>('window.cancelClose'),
    confirmClose: () => bridge.invoke<BaseResponse>('window.confirmClose'),

    // === Zoom and DPI ===
    getDpiScale: () => bridge.invoke<DpiScaleResponse>('window.getDpiScale'),
    setZoom: (zoom: number) =>
        bridge.invoke<BaseResponse>('window.setZoom', { zoom }),
    getZoom: () => bridge.invoke<{ zoom: number }>('window.getZoom'),
    resetZoom: () =>
        bridge.invoke<BaseResponse & { zoom?: number }>('window.resetZoom'),
    setZoomForDpi: (dpi?: number) =>
        bridge.invoke<BaseResponse & { zoom?: number }>(
            'window.setZoomForDpi',
            { ...(dpi != null ? { dpi } : {}) },
        ),

    // === Dev server ===
    getDevServerConfig: () =>
        bridge.invoke<WindowDevServerConfig>('window.getDevServerConfig'),
    setDevServerConfig: (opts: WindowSetDevServerConfigParams) =>
        bridge.invoke<BaseResponse>('window.setDevServerConfig', opts),
    showContextMenu: (x?: number, y?: number) =>
        bridge.invoke<BaseResponse>('ui.showContextMenu', {
            ...(x != null ? { x, y } : {}),
        }),
};
