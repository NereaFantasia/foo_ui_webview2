import { call } from '../call.js';
import type {
    TaskbarFlashParams,
    TaskbarSetOverlayIconParams,
    TaskbarSetProgressParams,
    TaskbarSetThumbnailButtonsParams,
    TaskbarUpdateButtonParams,
} from '../../types/generated/params.js';

/** A single thumbnail-toolbar button. */
export type ThumbnailButton = TaskbarSetThumbnailButtonsParams['buttons'][number];

/**
 * `taskbar` namespace - Windows taskbar thumbnail toolbar, progress bar,
 * overlay icon and flash bindings.
 *
 * Parameter and response shapes come from src/api/schema/taskbar.ts through
 * the generated types; see the taskbar-tray page of the API reference.
 */
export const taskbar = {
    /**
     * Install the thumbnail toolbar (max 7 buttons). Windows permits this only
     * once per window; use {@link updateButton} afterwards to change state.
     */
    setThumbnailButtons: (buttons: ThumbnailButton[]) =>
        call('taskbar.setThumbnailButtons', {
            buttons,
        }),

    /** Update one existing thumbnail button in place (cannot add or remove buttons). */
    updateButton: (opts: TaskbarUpdateButtonParams) =>
        call('taskbar.updateButton', opts),

    /**
     * Set the taskbar progress bar. `value` (range 0-1) applies to the
     * `'normal'` / `'error'` / `'paused'` states.
     */
    setProgress: (opts: TaskbarSetProgressParams) =>
        call('taskbar.setProgress', opts),

    /** Set or clear the overlay badge icon; omit `icon` to clear it. */
    setOverlayIcon: (opts: TaskbarSetOverlayIconParams = {}) =>
        call('taskbar.setOverlayIcon', opts),

    /** Flash the taskbar button. `count` defaults to 3, `interval` (ms) to the system default. */
    flash: (opts: TaskbarFlashParams = {}) =>
        call('taskbar.flash', opts),
};
