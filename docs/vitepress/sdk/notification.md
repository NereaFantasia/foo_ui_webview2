# fb.notification Notifications

`fb.notification` wraps the `ui.*` notification, toast, and custom-menu handlers behind one SDK namespace.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### hide()

Signature: `fb.notification.hide(): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns the `ui.hideNotification` response envelope.

```javascript
const result = await fb.notification.hide();
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Show a Notification

`fb.notification.show(options: UiShowNotificationParams)` invokes `ui.showNotification`. The options are `title`, `body`, `silent`, and `timeout` in milliseconds. The response may include an `id`.

```javascript
await fb.notification.show({
	title: 'Library scan',
	body: 'The scan has completed.',
	timeout: 5000,
});
```

## Show a Toast

`fb.notification.showToast(options: UiShowToastParams)` invokes `ui.showToast`. The options include `message`, `duration`, `type`, and `position`.

```javascript
await fb.notification.showToast({
	message: 'Added to the playlist',
	type: 'success',
	duration: 3000,
	position: 'bottom-right',
});
```

Successful toast requests can produce the typed `ui:toast` event with `UiToastPayload`.

## Show a Custom Menu

`fb.notification.showCustomMenu(options: UiShowCustomMenuParams): Promise<UiShowCustomMenuResponse>` invokes `ui.showCustomMenu`. The response's optional `selectedId` is the clicked item ID or `null` when the menu is dismissed.

```javascript
const { selectedId } = await fb.notification.showCustomMenu({
	items: [
		{ id: 'play', label: 'Play' },
		{ id: 'queue', label: 'Add to queue' },
	],
	x: 120,
	y: 80,
	suppressDefault: true,
});
```
