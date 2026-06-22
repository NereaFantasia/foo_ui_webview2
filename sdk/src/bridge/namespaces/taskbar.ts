/**
 * `taskbar` namespace - Windows taskbar thumbnail toolbar, progress bar,
 * overlay icon and flash bindings.
 *
 * See docs/vitepress/api/taskbar-tray.md for the full reference.
 */

import { bridge } from '../Bridge.js';
import type { BaseResponse } from '../../types/responses.js';

/** A single thumbnail-toolbar button. */
export interface ThumbnailButton {
    /** Stable identifier reported by `taskbar:buttonClicked`. */
    id: string;
    /** Base64 ICO payload; null or omit uses the foobar2000 main icon. */
    icon?: string | null;
    /** Hover tooltip text. */
    tooltip: string;
    /** Whether the button is clickable (default `true`). */
    enabled?: boolean;
    /** Whether the button is shown (default `true`). */
    visible?: boolean;
    /** Dismiss the thumbnail preview after a click (default `false`). */
    dismissOnClick?: boolean;
}

export const taskbar = {
    /**
     * Install the thumbnail toolbar (max 7 buttons). Windows permits this only
     * once per window; use {@link updateButton} afterwards to change state.
     */
    setThumbnailButtons: (buttons: ThumbnailButton[]) =>
        bridge.invoke<BaseResponse>('taskbar.setThumbnailButtons', { buttons }),

    /** Update one existing thumbnail button in place (cannot add or remove buttons). */
    updateButton: (opts: {
        id: string;
        icon?: string | null;
        tooltip?: string;
        enabled?: boolean;
        visible?: boolean;
    }) => bridge.invoke<BaseResponse>('taskbar.updateButton', opts),

    /**
     * Set the taskbar progress bar. `value` (range 0-1) applies to the
     * `'normal'` / `'error'` / `'paused'` states.
     */
    setProgress: (opts: {
        state: 'none' | 'indeterminate' | 'normal' | 'error' | 'paused';
        value?: number;
    }) => bridge.invoke<BaseResponse>('taskbar.setProgress', opts),

    /** Set or clear the overlay badge icon; omit `icon` to clear it. */
    setOverlayIcon: (opts: { icon?: string | null; description?: string } = {}) =>
        bridge.invoke<BaseResponse>('taskbar.setOverlayIcon', opts),

    /** Flash the taskbar button. `count` defaults to 3, `interval` (ms) to the system default. */
    flash: (opts: { count?: number; interval?: number } = {}) =>
        bridge.invoke<BaseResponse>('taskbar.flash', opts),
};
