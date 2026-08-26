# fb.notification 通知

本页是 `fb.notification` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### hide()

封装 `ui.hideNotification`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.notification.hide(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

## 显示系统通知

`fb.notification.show(options: UiShowNotificationParams)` 调用 `ui.showNotification`。选项包括 `title`、`body`、`silent` 和以毫秒为单位的 `timeout`；返回值可能包含通知 `id`。

```javascript
await fb.notification.show({
	title: '媒体库扫描',
	body: '扫描已经完成。',
	timeout: 5000,
});
```

## 显示 Toast

`fb.notification.showToast(options: UiShowToastParams)` 调用 `ui.showToast`，可传入 `message`、`duration`、`type` 和 `position`。

```javascript
await fb.notification.showToast({
	message: '已添加到播放列表',
	type: 'success',
	duration: 3000,
	position: 'bottom-right',
});
```

成功的 Toast 请求可产生类型化的 `ui:toast` 事件，其 payload 为 `UiToastPayload`。

## 显示自定义菜单

`fb.notification.showCustomMenu(options: UiShowCustomMenuParams): Promise<UiShowCustomMenuResponse>` 调用 `ui.showCustomMenu`。返回值中的可选 `selectedId` 是被单击项目的 ID；关闭菜单而未选择时为 `null`。

```javascript
const { selectedId } = await fb.notification.showCustomMenu({
	items: [
		{ id: 'play', label: '播放' },
		{ id: 'queue', label: '加入队列' },
	],
	x: 120,
	y: 80,
	suppressDefault: true,
});
```
