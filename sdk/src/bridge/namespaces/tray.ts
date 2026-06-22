/**
 * `tray` namespace - system tray icon bindings.
 *
 * See docs/vitepress/api/taskbar-tray.md for the full reference.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

/** A single tray context-menu item. */
export interface TrayMenuItem {
    /** Stable identifier reported by `tray:menuItemClicked`; omit for separators. */
    id?: string;
    /** Display text. */
    label?: string;
    /**
     * Item kind. A checkmark is driven by `checked`, not by a dedicated type.
     *
     * The rich kinds (`'nowplaying'` / `'rating'` / `'slider'` / `'segmented'`)
     * are fully rendered only by the `render: 'webview'` backend; the native
     * `TrackPopupMenu` backend degrades them (now-playing → disabled header
     * line, rating → a "★1-5" submenu, slider → a stepped-level submenu,
     * segmented → a plain text item).
     */
    type?: 'normal' | 'separator' | 'submenu' | 'nowplaying' | 'rating' | 'slider' | 'segmented';
    /** Whether the item is clickable (default `true`). */
    enabled?: boolean;
    /** Whether the item is shown (default `true`). */
    visible?: boolean;
    /** Renders a checkmark when `true` (default `false`). */
    checked?: boolean;
    /**
     * Base64 ICO payload. Rendered by the `'webview'` backend only (the native
     * backend ignores it).
     */
    icon?: string;
    /**
     * Inline monochrome SVG icon, drawn before the label by the `'webview'`
     * backend only (the native backend ignores it). `content` is the SVG inner
     * markup (e.g. `"<path d=\"...\"/>"`), rendered as
     * `<svg viewBox=...>content</svg>` with `fill: currentColor` so it follows
     * the menu text colour. Within a menu layer, if any item supplies an icon,
     * every normal/submenu item reserves a fixed icon column so labels stay
     * left-aligned. Keep `content` to trusted shape markup (paths/shapes); it is
     * injected as-is, so if you source icons from untrusted input, sanitize them
     * first (strip `<script>` and `on*` handlers).
     */
    iconSvg?: { viewBox: string; content: string };

    // ── Rich-item payload (webview backend only) ──────────────────────────

    /**
     * `type: 'nowplaying'` album art, rendered as a fixed 40x40 thumbnail.
     * Accepts three forms: a full data URL (`data:image/jpeg;base64,...`), an
     * `http(s)://` URL (used directly), or raw base64 (decoded as JPEG). If
     * omitted and {@link TrayMenuConfig.autoNowPlaying} is on, the `'webview'`
     * backend fills it with the current track's front art (downscaled).
     */
    cover?: string;
    /** `type: 'nowplaying'` primary line (track title). Falls back to `label`. */
    title?: string;
    /** `type: 'nowplaying'` secondary line (artist / album). */
    subtitle?: string;
    /**
     * Current value. `type: 'rating'` → integer stars `0..5`;
     * `type: 'slider'` → integer within `[min, max]`;
     * `type: 'segmented'` → zero-based index of the selected segment.
     */
    value?: number;
    /** `type: 'slider'` range minimum (default `0`). */
    min?: number;
    /** `type: 'slider'` range maximum (default `100`). */
    max?: number;
    /**
     * `type: 'segmented'` segments — an inline single-select control (one row of
     * mutually exclusive options). Rendered by the `'webview'` backend only; the
     * native backend degrades the row to a plain text item. The selected segment
     * is {@link value} (a zero-based index). Each segment shows its `iconSvg`
     * when present, otherwise its `label`; a segment with `enabled: false` is
     * greyed out and cannot be picked. Clicking a segment reports through
     * `tray:menuItemClicked` as `{ id, value: index }` and **keeps the menu
     * open** (Left/Right also move the selection); mapping an index to its
     * meaning (e.g. a playback mode) is the frontend's job, so this contract
     * stays generic. `iconSvg.content` is injected as-is, so sanitize segment
     * icons sourced from untrusted input (strip `<script>` / `on*`).
     */
    segments?: { label?: string; iconSvg?: { viewBox: string; content: string }; enabled?: boolean }[];
    /** Child items; requires `type: 'submenu'`. */
    submenu?: TrayMenuItem[];
}

/** Menu zone identifier. See {@link TrayMenuConfig.customPosition}. */
export type TrayMenuPosition = 'top' | 'playback' | 'bottom';

/**
 * Optional configuration for {@link tray.setContextMenu}.
 *
 * - `showPlaybackControls` (default `true`): auto-inject built-in
 *   play/pause/prev/next/stop items in the playback zone.
 * - `showSystemItems` (default `true`): auto-inject "Exit foobar2000"
 *   in the bottom zone.
 * - `customPosition` (default `'top'`): which zone the `items` array
 *   passed to setContextMenu is written into.
 */
export interface TrayMenuConfig {
    showPlaybackControls?: boolean;
    showSystemItems?: boolean;
    customPosition?: TrayMenuPosition;
    /**
     * Menu render backend (default `'native'`).
     *
     * - `'native'`: Win32 tray menu (`TrackPopupMenu`).
     * - `'webview'`: self-drawn content-sized overlay rendered by the WebView2
     *   engine, floating above the taskbar. Selection still arrives via the
     *   `tray:menuItemClicked` event and `tray:beforeContextMenu` is still
     *   fired; the webview tray menu does **not** emit `menu:select` /
     *   `menu:dismiss` (those belong to the `menu.*` namespace).
     *
     * The rich item kinds (`'nowplaying'` / `'rating'` / `'slider'` /
     * `'segmented'`) are only fully rendered by `'webview'`. Their interactions
     * report through `tray:menuItemClicked`: a now-playing click sends `{ id }`
     * and closes the menu; a rating/slider/segmented change sends
     * `{ id, value }` and keeps the menu open. The `'native'` backend degrades
     * the rich kinds (see {@link TrayMenuItem.type}) but still reports
     * `{ id, value }` for the degraded rating/slider submenu picks.
     */
    render?: 'native' | 'webview';

    /**
     * When `true`, `nowplaying` items get any field the frontend leaves empty
     * (`cover` / `title` / `subtitle`) auto-filled from the current track at
     * right-click time — **frontend-first, backend-fallback** (any value you
     * supply always wins). Lets a pure-local app declare just
     * `{ type: 'nowplaying', id: 'np' }` and get a live cover + title + artist.
     *
     * `cover` auto-fill is `'webview'`-only (the native backend fills text
     * only) and reads the current track's front art (downscaled to a
     * thumbnail), so it stays empty for sources foobar2000 cannot extract art
     * from (e.g. most streams — pass `cover` yourself there). Default `false`.
     */
    autoNowPlaying?: boolean;

    /**
     * Frontend style takeover for the self-drawn tray menu (**`render: 'webview'`
     * only**; ignored by the native backend). The CSS string is injected into the
     * overlay's dedicated `<style>` layer and applied on every open.
     *
     * By default this is **override / append** mode: your rules sit on top of the
     * built-in styles, so target the menu's stable class names — `.fb-menu`,
     * `.fb-item` (with `.nrm` / `.disabled` / `.active` / `.checked` / `.has-sub`),
     * `.fb-item-ico`, `.fb-arrow`, `.fb-sep`, the now-playing `.fb-np*`, rating
     * `.fb-rating*` / `.fb-star`, slider `.fb-slider*`, and segmented
     * `.fb-seg*` (`.fb-seg-label` / `.fb-seg-group` / `.fb-seg-btn` with `.on` /
     * `.disabled`) — and win by source order or `!important`. (The overlay is an
     * isolated top-level document, so a
     * host page's `::part()` cannot reach it; the stable class names are the
     * supported hook — see STYLING_TAKEOVER_DESIGN §12.3 D-B.)
     *
     * A small protected structural layer (root box-sizing / overflow / fixed
     * positioning / root display) is always force-applied last to keep the
     * content-sized window measurement stable; it is not overridable.
     */
    css?: string;

    /**
     * When `true`, switches {@link css} from override/append to **replace** mode:
     * the built-in default styles are disabled and only your `css` plus the
     * protected structural layer remain, so the menu's entire look (including the
     * entry animation) is yours to define. Default `false`. `'webview'`-only.
     */
    cssReplace?: boolean;

    /**
     * DWM system backdrop for the self-drawn tray menu (**`render: 'webview'`
     * only**; ignored by the native backend). Shares the same effect vocabulary
     * as the main / tabbed windows (mapped to DWM `DWMSBT_*`): `'none'` (no
     * backdrop), `'mica'`, `'mica-alt'` and `'acrylic'`.
     *
     * Default `'acrylic'` — the transient-surface material, which is the correct
     * default for a pop-up menu. `'mica'` / `'mica-alt'` are designed as
     * main-window / tabbed-window backgrounds and may look off on a transient
     * menu; `'none'` disables the backdrop. Re-applied on every open, so it can
     * follow the current theme per right-click.
     *
     * ⚠️ The DWM backdrop is a window-level, all-or-nothing effect: it appears /
     * disappears the instant the window is shown / hidden and **cannot fade with
     * CSS animations** (see {@link closeAnimationMs}). For a smoothly animated
     * open / close, use a CSS translucent background instead — set this to
     * `'none'` and give `.fb-menu` a translucent `background` via {@link css}.
     */
    backdrop?: 'acrylic' | 'mica' | 'mica-alt' | 'none';

    /**
     * Dark tint for the {@link backdrop} (**`render: 'webview'` only**). Default
     * `true`; pass `false` to follow a light theme. Applied together with
     * {@link backdrop} on every open.
     */
    backdropDarkMode?: boolean;

    /**
     * Exit (fade-out) animation duration in milliseconds for the self-drawn tray
     * menu (**`render: 'webview'` only**; ignored by the native backend).
     *
     * Default `0` — the menu hides immediately on close with no exit animation
     * (zero regression). When `> 0` (clamped to `0..1000`), a user-initiated
     * close (clicking outside, `Escape`, selecting an item, or losing focus)
     * first plays an exit transition for this many milliseconds before the
     * window is hidden. Set it to roughly your own `#menu.out` transition
     * duration so the fade finishes just as the window disappears.
     *
     * On close the renderer toggles the root menu's class from `#menu.in` to
     * `#menu.out`; the built-in `#menu.out` rule mirrors the entry animation, and
     * you can override it via {@link css}. The `replaced` (a new menu opening
     * over this one) and internal timeout close paths always hide immediately,
     * regardless of this value.
     *
     * ⚠️ This animates the web content only — **not** the DWM {@link backdrop},
     * which is a window-level effect that snaps in / out with the window. With
     * `acrylic` / `mica` enabled, the backdrop pops while the content fades, so
     * the transition is out of sync. For a fully smooth fade, use a CSS
     * translucent background ({@link backdrop} `'none'` + a translucent
     * `.fb-menu` background via {@link css}) rather than the DWM backdrop.
     */
    closeAnimationMs?: number;
}

export const tray = {
    /** Create the tray icon. Must be called once before any other `tray.*` API. */
    create: (opts: { icon?: string | null; tooltip?: string } = {}) =>
        bridge.invoke<BaseResponse>('tray.create', opts),

    /** Remove the tray icon. */
    destroy: () => bridge.invoke<BaseResponse>('tray.destroy', {}),

    /** Replace the tray icon image; omit or pass null to fall back to the main icon. */
    setIcon: (icon?: string | null) => bridge.invoke<BaseResponse>('tray.setIcon', { icon }),

    /** Update the hover tooltip (max 128 characters). */
    setTooltip: (tooltip: string) => bridge.invoke<BaseResponse>('tray.setTooltip', { tooltip }),

    /** Show a balloon notification. `icon` is `'info'` (default) / `'warning'` / `'error'`. */
    showBalloon: (opts: { title: string; message: string; icon?: string }) =>
        bridge.invoke<BaseResponse>('tray.showBalloon', opts),

    /**
     * Set the full context menu, replacing items in the zone determined by
     * `config.customPosition` (default `'top'`). The other two zones are left
     * intact. Use the incremental helpers below for partial updates.
     */
    setContextMenu: (items: TrayMenuItem[], config?: TrayMenuConfig) =>
        bridge.invoke<BaseResponse>('tray.setContextMenu',
            config ? { items, config } : { items }),

    /** Hide to the tray instead of the taskbar when the window is minimized. */
    setMinimizeToTray: (enabled: boolean) =>
        bridge.invoke<BaseResponse>('tray.setMinimizeToTray', { enabled }),

    /** Hide to the tray instead of quitting when the window is closed. */
    setCloseToTray: (enabled: boolean) =>
        bridge.invoke<BaseResponse>('tray.setCloseToTray', { enabled }),

    /** Resolve whether the tray icon currently exists. */
    isVisible: () => bridge.invoke<{ success: boolean; visible: boolean }>('tray.isVisible', {}),

    /** Append items to the given zone (default `'top'`). */
    appendMenuItems: (items: TrayMenuItem[], position: TrayMenuPosition = 'top') =>
        bridge.invoke<BaseResponse>('tray.appendMenuItems', { items, position }),

    /** Remove items with the given ids from all zones. Resolves with the number removed. */
    removeMenuItems: (ids: string[]) =>
        bridge.invoke<BaseResponse & { removed: number }>('tray.removeMenuItems', { ids }),

    /** Clear the given zone, or all zones if `position` is omitted. */
    clearMenuItems: (position?: TrayMenuPosition) =>
        bridge.invoke<BaseResponse>('tray.clearMenuItems',
            position ? { position } : {}),

    /**
     * Get all user-defined menu items, flattened in zone order
     * (`top -> playback -> bottom`). Built-in items injected by
     * `showPlaybackControls` / `showSystemItems` are **not** included.
     */
    getMenuItems: () =>
        bridge.invoke<{ success: boolean; items: TrayMenuItem[] }>('tray.getMenuItems', {}),

    /**
     * Update a single menu item's `checked` / `enabled` state in place. The id
     * is searched across all zones and recursively into submenus. At least one
     * of `checked` / `enabled` must be supplied. This is a granular alternative
     * to {@link setContextMenu}, which performs a full-zone replace.
     *
     * The native tray menu is rebuilt from stored data each time it is opened,
     * so the new state takes effect on the **next** open rather than mutating a
     * menu that is already showing. The resolved `found` flag reports whether an
     * item with the given id existed.
     */
    setMenuItemState: (id: string, state: { checked?: boolean; enabled?: boolean }) =>
        bridge.invoke<BaseResponse & { found: boolean }>('tray.setMenuItemState',
            { id, ...state }),
};
