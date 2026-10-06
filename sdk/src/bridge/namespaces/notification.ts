import { call } from '../call.js';
import type {
    UiShowCustomMenuParams,
    UiShowNotificationParams,
    UiShowToastParams,
} from '../../types/generated/params.js';

/**
 * `notification` — toast / custom-menu / notification host bindings.
 *
 * Wraps the `ui.*` notification family so consumers do not have to
 * remember the cross-namespace `invoke('ui.showNotification', ...)`
 * spelling.
 */
export const notification = {
    /** Shows a balloon notification; the response carries the numeric `id`. */
    show: (opts: UiShowNotificationParams) =>
        call('ui.showNotification', opts),
    hide: () => call('ui.hideNotification'),
    /** Opens a native popup menu; `selectedId` is `null` when it is dismissed. */
    showCustomMenu: (opts: UiShowCustomMenuParams) =>
        call('ui.showCustomMenu', opts),
    showToast: (opts: UiShowToastParams) =>
        call('ui.showToast', opts),
};
