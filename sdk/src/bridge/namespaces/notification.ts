/**
 * `notification` — toast / custom-menu / notification host bindings.
 *
 * Wraps the `ui.*` notification family so consumers do not have to
 * remember the cross-namespace `invoke('ui.showNotification', ...)`
 * spelling.
 */

import { bridge } from '../Bridge.js';
import type {
    BaseResponse,
    UiShowCustomMenuResponse,
} from '../../types/responses.js';
import type {
    UiShowCustomMenuParams,
    UiShowNotificationParams,
    UiShowToastParams,
} from '../../types/generated/params.js';

export const notification = {
    show: (opts: UiShowNotificationParams) =>
        bridge.invoke<BaseResponse & { id?: string }>(
            'ui.showNotification',
            opts,
        ),
    hide: () => bridge.invoke<BaseResponse>('ui.hideNotification'),
    showCustomMenu: (opts: UiShowCustomMenuParams) =>
        bridge.invoke<UiShowCustomMenuResponse>('ui.showCustomMenu', opts),
    showToast: (opts: UiShowToastParams) =>
        bridge.invoke<BaseResponse>('ui.showToast', opts),
};
