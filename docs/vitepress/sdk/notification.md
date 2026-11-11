# fb.notification Notifications

`fb.notification` provides integrated operations for system balloon notifications, in-app toast messages, and native context menus, wrapping the underlying `ui.showNotification`, `ui.showToast`, and `ui.showCustomMenu` APIs.

## hide()

Signature: `fb.notification.hide(): Promise<UiHideNotificationResponse>`

Hides the balloon notification shown by `show()`; has no effect if no notification is displayed.

```javascript
const result = await fb.notification.hide();
```

## show(options) {#show-a-notification}

Signature: `fb.notification.show(options: UiShowNotificationParams): Promise<UiShowNotificationResponse>`

Displays a Windows balloon notification originating from the component's notification tray icon (created on first call if not yet initialized). Options include `title`, `body`, `silent`, and `timeout` in milliseconds (passed as a hint to the Windows Shell, defaults to 5000). At least one of `title` or `body` must be provided; the notification body field is `body`. The `id` field in the response represents a 1-based sequential notification identifier.

```javascript
await fb.notification.show({
	title: 'Library scan',
	body: 'The scan has completed.',
	timeout: 5000,
});
```

## showToast(options) {#show-a-toast}

Signature: `fb.notification.showToast(options: UiShowToastParams): Promise<UiShowToastResponse>`

Options include `message`, `duration` (display duration in milliseconds, defaults to 3000), `type` (`'info'`, `'success'`, `'warning'`, or `'error'`, defaults to `'info'`), and `position` (defaults to `'bottom-right'`).

The host does not render UI elements directly; instead, it dispatches the parameters as a typed `ui:toast` event (with payload `UiToastPayload`) to the calling window's page for frontend rendering.

```javascript
await fb.notification.showToast({
	message: 'Added to the playlist',
	type: 'success',
	duration: 3000,
	position: 'bottom-right',
});
```

## showCustomMenu(options) {#show-a-custom-menu}

Signature: `fb.notification.showCustomMenu(options: UiShowCustomMenuParams): Promise<UiShowCustomMenuResponse>`

Opens a modal native popup menu at the current system cursor coordinates and waits for user selection. The `selectedId` field in the response contains the ID of the clicked item, or `null` if the menu is dismissed without selection. An accepted choice also dispatches a `ui:menuItemClicked` event (`{ id, label }`) to the calling window.

The `x`, `y`, and `suppressDefault` options are retained for backward compatibility and do not affect placement; the menu always opens at the current system cursor position.

```javascript
const res = await fb.notification.showCustomMenu({
	items: [
		{ id: 'play', label: 'Play' },
		{ id: 'queue', label: 'Add to queue' },
	],
});
if (res.success === false) throw new Error(res.error);
const { selectedId } = res;
```
