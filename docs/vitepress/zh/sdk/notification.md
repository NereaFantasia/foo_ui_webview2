# fb.notification 通知

`fb.notification` 把 `ui.*` 里的通知、Toast 与自定义菜单几个方法收进同一个 SDK 命名空间。

## hide()

签名：`fb.notification.hide(): Promise<UiHideNotificationResponse>`

隐藏 `show()` 显示的气泡；没有显示时什么也不做。

```javascript
const result = await fb.notification.hide();
```

## 显示系统通知

`fb.notification.show(options: UiShowNotificationParams)` 调用 `ui.showNotification`，从插件的通知图标显示一条 Windows 气泡通知，首次使用时创建图标。选项包括 `title`、`body`、`silent` 和以毫秒为单位的 `timeout`（供 shell 参考，默认 5000）。`title` 与 `body` 至少给一个；正文键是 `body`，不是 `message`。返回值里的 `id` 是通知的序号，从 `1` 起。

```javascript
await fb.notification.show({
	title: '媒体库扫描',
	body: '扫描已经完成。',
	timeout: 5000,
});
```

## 显示 Toast

`fb.notification.showToast(options: UiShowToastParams)` 调用 `ui.showToast`，可传入 `message`、`duration`（毫秒，默认 3000）、`type`（`'info'`、`'success'`、`'warning'` 或 `'error'`，默认 `'info'`）和 `position`（默认 `'bottom-right'`）。

宿主自己不绘制：它把载荷以类型化的 `ui:toast` 事件（payload 为 `UiToastPayload`）发给调用窗口的页面，由页面渲染 Toast。

```javascript
await fb.notification.showToast({
	message: '已添加到播放列表',
	type: 'success',
	duration: 3000,
	position: 'bottom-right',
});
```

## 显示自定义菜单

`fb.notification.showCustomMenu(options: UiShowCustomMenuParams): Promise<UiShowCustomMenuResponse>` 调用 `ui.showCustomMenu`，在系统光标处弹出原生菜单并等待选择。返回值中的 `selectedId` 是被单击项目的 ID；关闭菜单而未选择时为 `null`。选中的行还会以 `ui:menuItemClicked`（`{ id, label }`）通知调用窗口。

`x`、`y` 与 `suppressDefault` 为兼容而保留，没有作用；菜单总在系统光标处弹出。

```javascript
const res = await fb.notification.showCustomMenu({
	items: [
		{ id: 'play', label: '播放' },
		{ id: 'queue', label: '加入队列' },
	],
});
if (res.success === false) throw new Error(res.error);
const { selectedId } = res;
```
