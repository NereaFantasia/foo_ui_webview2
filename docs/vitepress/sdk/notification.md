# fb.notification Notifications

`fb.notification` wraps the `ui.*` notification, toast, and custom-menu handlers behind one SDK namespace.

## hide()

Signature: `fb.notification.hide(): Promise<UiHideNotificationResponse>`

Hides the balloon that `show()` displayed; does nothing when none is shown.

```javascript
const result = await fb.notification.hide();
```

## Show a Notification

`fb.notification.show(options: UiShowNotificationParams)` invokes `ui.showNotification`, which shows a Windows balloon notification from the plugin's notification icon and creates the icon on first use. The options are `title`, `body`, `silent`, and `timeout` in milliseconds (a hint to the shell, default 5000). At least one of `title` and `body` must be given; the text key is `body`, not `message`. The response's `id` is the notification's sequence number, starting at `1`.

```javascript
await fb.notification.show({
	title: 'Library scan',
	body: 'The scan has completed.',
	timeout: 5000,
});
```

## Show a Toast

`fb.notification.showToast(options: UiShowToastParams)` invokes `ui.showToast`. The options include `message`, `duration` (milliseconds, default 3000), `type` (`'info'`, `'success'`, `'warning'`, or `'error'`, default `'info'`), and `position` (default `'bottom-right'`).

The host paints nothing itself: it delivers the payload to the calling window's page as the typed `ui:toast` event with `UiToastPayload`, and the page renders the toast.

```javascript
await fb.notification.showToast({
	message: 'Added to the playlist',
	type: 'success',
	duration: 3000,
	position: 'bottom-right',
});
```

## Show a Custom Menu

`fb.notification.showCustomMenu(options: UiShowCustomMenuParams): Promise<UiShowCustomMenuResponse>` invokes `ui.showCustomMenu`, which opens a native popup menu at the system cursor and waits for the choice. The response's `selectedId` is the clicked item ID, or `null` when the menu is dismissed. A chosen row is also announced to the calling window as `ui:menuItemClicked` with `{ id, label }`.

`x`, `y`, and `suppressDefault` are accepted for compatibility and have no effect; the menu always opens at the system cursor.

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
