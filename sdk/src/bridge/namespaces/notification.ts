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
    /** Show a balloon notification; the response carries the numeric `id`. */
    show: (opts: UiShowNotificationParams) =>
        call('ui.showNotification', opts),
    /** Hide the balloon notification shown by `show`. */
    hide: () => call('ui.hideNotification'),
    /** Open a native popup menu; `selectedId` is `null` when it is dismissed. */
    showCustomMenu: (opts: UiShowCustomMenuParams) =>
        call('ui.showCustomMenu', opts),
    /** Dispatch a toast notification event (`ui:toast`) to the calling window. */
    showToast: (opts: UiShowToastParams) =>
        call('ui.showToast', opts),
};
