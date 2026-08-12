/**
 * `menu` - main menu, context menu, and self-drawn popup menu namespace.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    MenuGetMainMenuResponse,
    MenuGetContextMenuResponse,
    MenuShowNativePopupResponse,
    MenuPopupItem,
    MenuPopupPosition,
} from '../../types/responses.js';
import type {
    MenuShowResponse,
    MenuCloseResponse,
    MenuRunContextCommandResponse,
    MenuRunMainMenuCommandResponse,
} from '../../types/generated/responses.js';
import type {
    MenuGetContextMenuParams,
    MenuGetMainMenuParams,
    MenuRunContextCommandByIdParams,
    MenuRunContextCommandParams,
    MenuRunMainMenuCommandParams,
    MenuShowNativePopupParams,
} from '../../types/generated/params.js';

/** Merge optional screen-pixel coordinates into a `menu.show` payload. */
function withPosition<T extends object>(
    base: T,
    position?: MenuPopupPosition,
): T & { x?: number; y?: number } {
    const merged: T & { x?: number; y?: number } = { ...base };
    if (position?.x !== undefined) merged.x = position.x;
    if (position?.y !== undefined) merged.y = position.y;
    return merged;
}

export const menu = {
    /**
     * `root` scopes the returned menu tree (e.g. `'Main'` / `'View'`).
     * `opts.locale` selects the `displayLabel` translation locale; the
     * default `'auto'` keeps the host's native labels untranslated.
     * `opts.i18n: false` disables label translation entirely.
     * `opts.withAvailability` (default `true`) includes per-submenu
     * command availability counters.
     */
    getMainMenu: (root?: string, opts?: Omit<MenuGetMainMenuParams, 'root'>) =>
        bridge.invoke<MenuGetMainMenuResponse>('menu.getMainMenu', {
            ...(root ? { root } : {}),
            ...opts,
        }),
    /**
     * `mode` is one of `'auto' | 'selection' | 'playlist' | 'nowPlaying' | 'handles'`.
     *
     * Prefer `'selection'`, `'playlist'` or `'nowPlaying'` whenever the target
     * is reachable from the host playlist: those resolve handles inside the
     * host and validate no paths. `'handles'` validates every supplied path
     * individually, which costs filesystem metadata calls per entry and is
     * noticeably slower for network shares.
     */
    getContextMenu: (opts?: MenuGetContextMenuParams) =>
        bridge.invoke<MenuGetContextMenuResponse>('menu.getContextMenu', opts || {}),
    /**
     * Runs a main menu command.
     *
     * `command` accepts a GUID (`'{11213A01-...}'`), a leaf name, or a
     * slash-separated path. Prefer the GUID: it is the only form that is stable
     * across hosts, because a localized build reports localized labels.
     *
     * `opts.subGuid` addresses a dynamic child command, paired with its owning
     * command GUID.
     *
     * Failure is reported as `success: false` with a `code`, never as a thrown
     * host error: `MENU_ITEM_DISABLED` (command exists but is greyed out),
     * `MENU_MATCH_AMBIGUOUS` (the name matched several commands — `candidates`
     * lists them), `MENU_COMMAND_NOT_FOUND`.
     */
    runMainMenuCommand: (
        command: string,
        opts?: Omit<MenuRunMainMenuCommandParams, 'command'>,
    ) =>
        bridge.invoke<MenuRunMainMenuCommandResponse>(
            'menu.runMainMenuCommand',
            { command, ...opts },
        ),
    /**
     * Runs a context-menu command against the now-playing track, falling back
     * to the active playlist selection.
     *
     * `command` accepts a GUID or a command name. `opts.subGuid` addresses a
     * dynamically generated child (a rating value, a converter preset); without
     * it the owning container is targeted instead, which is a silent no-op.
     *
     * `executionConfirmed` distinguishes "the host reported the command ran"
     * from "the command was dispatched through an entry point that returns
     * nothing". It is absent on the early-validation failures.
     */
    runContextCommand: (
        command: string,
        opts?: Omit<MenuRunContextCommandParams, 'command'>,
    ) =>
        bridge.invoke<MenuRunContextCommandResponse>('menu.runContextCommand', {
            command,
            ...opts,
        }),
    runContextCommandById: (
        id: number,
        opts?: Omit<MenuRunContextCommandByIdParams, 'id'>,
    ) =>
        bridge.invoke<BaseResponse>('menu.runContextCommandById', {
            id,
            ...opts,
        }),
    /**
     * Defaults to `auto`: handles, now playing, playlist selection, then
     * playlist context.
     *
     * The same cost note as {@link getContextMenu} applies — prefer
     * `mode: 'selection'` over supplying `handles` for playlist rows.
     */
    showNativePopup: (opts?: MenuShowNativePopupParams) =>
        bridge.invoke<MenuShowNativePopupResponse>('menu.showNativePopup', opts || {}),

    /**
     * Show a self-drawn (WebView-rendered) popup menu at `position`
     * (defaults to the cursor). Resolves with the new menu id; the
     * user's choice arrives asynchronously via the `menu:select` /
     * `menu:dismiss` events. Prefer {@link popup} when you only need
     * the chosen item id.
     */
    show: (items: MenuPopupItem[], position?: MenuPopupPosition) =>
        bridge.invoke<MenuShowResponse>(
            'menu.show',
            withPosition({ items }, position),
        ),

    /** Close the active self-drawn popup menu, if any. */
    close: (reason?: string) =>
        bridge.invoke<MenuCloseResponse>(
            'menu.close',
            reason ? { reason } : {},
        ),

    /**
     * Show a self-drawn popup menu and await the user's choice. Resolves
     * with the selected item id, or `null` when the menu is dismissed
     * (outside click, Escape, or any other close reason). Events are
     * matched by the menu id returned from `menu.show`, so overlapping
     * callers never cross-resolve.
     */
    popup: async (
        items: MenuPopupItem[],
        position?: MenuPopupPosition,
    ): Promise<string | null> => {
        const res = await bridge.invoke<MenuShowResponse>(
            'menu.show',
            withPosition({ items }, position),
        );
        if (!res?.success || !res.menuId) return null;
        const menuId = res.menuId;
        return new Promise<string | null>((resolve) => {
            let offSelect = (): void => {};
            let offDismiss = (): void => {};
            const finish = (value: string | null): void => {
                offSelect();
                offDismiss();
                resolve(value);
            };
            offSelect = bridge.on('menu:select', (payload) => {
                if (payload.menuId === menuId) finish(payload.itemId);
            });
            offDismiss = bridge.on('menu:dismiss', (payload) => {
                if (payload.menuId === menuId) finish(null);
            });
        });
    },
};
