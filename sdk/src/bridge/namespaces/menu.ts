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
} from '../../types/generated/responses.js';
import type {
    MenuGetContextMenuParams,
    MenuRunContextCommandByIdParams,
    MenuShowNativePopupParams,
} from '../../types/generated/params.js';

/** Merge optional screen-pixel coordinates into a `menu.show` payload. */
function withPosition(
    base: Record<string, unknown>,
    position?: MenuPopupPosition,
): Record<string, unknown> {
    if (position?.x !== undefined) base.x = position.x;
    if (position?.y !== undefined) base.y = position.y;
    return base;
}

export const menu = {
    /** `root` scopes the returned menu tree (e.g. `'Main'` / `'View'`). */
    getMainMenu: (root?: string) =>
        bridge.invoke<MenuGetMainMenuResponse>('menu.getMainMenu', {
            ...(root ? { root } : {}),
        }),
    /** `mode` is one of `'auto' | 'playlist' | 'nowPlaying' | 'handles'`. */
    getContextMenu: (opts?: MenuGetContextMenuParams) =>
        bridge.invoke<MenuGetContextMenuResponse>('menu.getContextMenu', opts || {}),
    runMainMenuCommand: (command: string) =>
        bridge.invoke<BaseResponse & { guid?: string }>(
            'menu.runMainMenuCommand',
            { command },
        ),
    runContextCommand: (command: string) =>
        bridge.invoke<BaseResponse & { guid?: string; itemCount?: number }>(
            'menu.runContextCommand',
            { command },
        ),
    runContextCommandById: (
        id: number,
        opts?: Omit<MenuRunContextCommandByIdParams, 'id'>,
    ) =>
        bridge.invoke<BaseResponse>('menu.runContextCommandById', {
            id,
            ...(opts || {}),
        }),
    showNativePopup: (opts: MenuShowNativePopupParams) =>
        bridge.invoke<MenuShowNativePopupResponse>('menu.showNativePopup', opts),

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
