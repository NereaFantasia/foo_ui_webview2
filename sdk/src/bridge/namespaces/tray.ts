import { call } from '../call.js';
import type {
    TraySetMenuZonesParams,
    TrayShowBalloonParams,
} from '../../types/generated/params.js';
import type {
    TrayIconSvg,
    TrayMenuConfig,
    TrayMenuItem,
    TraySegment,
} from '../../types/generated/schema-types.js';

// The shapes come from the declarations in src/api/schema/tray.ts; these are the module's
// public names for them.
export type { TrayIconSvg, TrayMenuConfig, TrayMenuItem, TraySegment };

/** Menu zone identifier. See {@link TrayMenuConfig.customPosition}. */
export type TrayMenuPosition = NonNullable<TrayMenuConfig['customPosition']>;

/**
 * `tray` namespace - system tray icon bindings.
 *
 * See the taskbar-tray page of the API reference for the full contract.
 */
export const tray = {
    /** Create the tray icon. Must be called once before any other `tray.*` API. */
    create: (opts: { icon?: string | null; tooltip?: string } = {}) =>
        call('tray.create', {
            ...(opts.icon != null ? { icon: opts.icon } : {}),
            ...(opts.tooltip !== undefined ? { tooltip: opts.tooltip } : {}),
        }),

    /** Remove the tray icon. */
    destroy: () => call('tray.destroy', {}),

    /** Replace the tray icon image; omit or pass null to fall back to the main icon. */
    setIcon: (icon?: string | null) =>
        call('tray.setIcon', {
            ...(icon != null ? { icon } : {}),
        }),

    /** Update the hover tooltip (max 128 characters). */
    setTooltip: (tooltip: string) =>
        call('tray.setTooltip', {
            tooltip,
        }),

    /** Show a balloon notification. `icon` is `'info'` (default) / `'warning'` / `'error'`. */
    showBalloon: (opts: { title: string; message: string; icon?: TrayShowBalloonParams['icon'] }) =>
        call('tray.showBalloon', opts),

    /**
     * Replace the items of the zone determined by `config.customPosition`
     * (default `'top'`). The other two zones are left intact. Use
     * {@link setMenuZones} to replace every zone at once, and the incremental
     * helpers below for partial updates.
     */
    setContextMenu: (items: TrayMenuItem[], config?: TrayMenuConfig) =>
        call(
            'tray.setContextMenu',
            config ? { items, config } : { items },
        ),

    /**
     * Replace the items of all three zones in one call; a zone left out is
     * cleared. The built-in items of `showPlaybackControls` and
     * `showSystemItems` are not stored in the zones and stay. When an item is
     * rejected or the resource limits are exceeded, nothing is stored,
     * `config` included.
     *
     * Prefer this to {@link clearMenuItems} followed by
     * {@link appendMenuItems} when the whole menu changes: a menu opened
     * between those calls shows part of the update, and two such updates
     * that interleave add their items twice. Of two calls sent back to back,
     * the later one is what remains.
     */
    setMenuZones: (zones: Omit<TraySetMenuZonesParams, 'config'>, config?: TrayMenuConfig) =>
        call(
            'tray.setMenuZones',
            config ? { ...zones, config } : { ...zones },
        ),

    /** Hide to the tray instead of the taskbar when the window is minimized. */
    setMinimizeToTray: (enabled: boolean) =>
        call('tray.setMinimizeToTray', {
            enabled,
        }),

    /** Hide to the tray instead of quitting when the window is closed. */
    setCloseToTray: (enabled: boolean) =>
        call('tray.setCloseToTray', {
            enabled,
        }),

    /** Resolve whether the tray icon currently exists. */
    isVisible: () => call('tray.isVisible', {}),

    /** Append items to the given zone (default `'top'`). */
    appendMenuItems: (items: TrayMenuItem[], position: TrayMenuPosition = 'top') =>
        call('tray.appendMenuItems', {
            items,
            position,
        }),

    /** Remove items with the given ids from all zones, submenus included. Resolves with the number removed. */
    removeMenuItems: (ids: string[]) =>
        call('tray.removeMenuItems', {
            ids,
        }),

    /** Clear the given zone, or all zones if `position` is omitted. */
    clearMenuItems: (position?: TrayMenuPosition) =>
        call(
            'tray.clearMenuItems',
            position ? { position } : {},
        ),

    /**
     * Get all user-defined menu items, flattened in zone order
     * (`top -> playback -> bottom`). Built-in items injected by
     * `showPlaybackControls` / `showSystemItems` are **not** included.
     */
    getMenuItems: () =>
        call('tray.getMenuItems', {}),

    /**
     * Update a single menu item's `checked` / `enabled` state in place. The id
     * is searched across all zones and recursively into submenus. At least one
     * of `checked` / `enabled` must be supplied. This is a granular alternative
     * to {@link setContextMenu}, which performs a full-zone replace.
     *
     * Providing `checked` (true or false) marks the item checkable so subsequent
     * `getMenuItems()` and the WebView overlay keep checkbox semantics even when
     * the value is `false`.
     *
     * The native tray menu is rebuilt from stored data each time it is opened,
     * so the new state takes effect on the **next** open rather than mutating a
     * menu that is already showing. An id no row has fails with `NOT_FOUND`.
     */
    setMenuItemState: (id: string, state: { checked?: boolean; enabled?: boolean }) =>
        call('tray.setMenuItemState', {
            id,
            ...state,
        }),
};
