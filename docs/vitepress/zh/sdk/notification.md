# fb.notification 通知

`fb.notification` 提供系统气泡通知、应用内 Toast 消息与上下文原生菜单的集成操作，统一封装底层的 `ui.showNotification`、`ui.showToast` 与 `ui.showCustomMenu` 接口。

## hide()

签名：`fb.notification.hide(): Promise<UiHideNotificationResponse>`

隐藏由 `show()` 弹出的气泡通知；若当前无处于活动状态的通知，调用静默忽略。

```javascript
const result = await fb.notification.hide();
```

## show(options) {#显示系统通知}

签名：`fb.notification.show(options: UiShowNotificationParams): Promise<UiShowNotificationResponse>`

通过插件托盘通知图标弹出 Windows 系统气泡通知（若托盘图标尚未初始化，首次调用时自动创建）。参数包含 `title`、`body`、`silent` 以及以毫秒为单位的显示超时时长 `timeout`（作为 Windows Shell 建议参考值，缺省为 5000）。`title` 与 `body` 至少须指定一项；消息正文字段为 `body`。响应对象中的 `id` 字段为从 `1` 开始自增的通知序列号。

```javascript
await fb.notification.show({
	title: '媒体库扫描',
	body: '扫描已经完成。',
	timeout: 5000,
});
```

## showToast(options) {#显示-toast}

签名：`fb.notification.showToast(options: UiShowToastParams): Promise<UiShowToastResponse>`

参数包含 `message`、`duration`（显示时长毫秒数，缺省为 3000）、`type`（可选值为 `'info'`、`'success'`、`'warning'` 或 `'error'`，缺省为 `'info'`）以及 `position`（缺省为 `'bottom-right'`）。

C++ 宿主不负责直接绘制界面元素，而是将参数组装为类型化的 `ui:toast` 事件（载荷结构为 `UiToastPayload`）分发至调用窗口页面，由前端 UI 组件负责渲染呈现。

```javascript
await fb.notification.showToast({
	message: '已添加到播放列表',
	type: 'success',
	duration: 3000,
	position: 'bottom-right',
});
```

## showCustomMenu(options) {#显示自定义菜单}

签名：`fb.notification.showCustomMenu(options: UiShowCustomMenuParams): Promise<UiShowCustomMenuResponse>`

在当前系统光标所在坐标弹出模态原生快捷菜单并阻塞等待用户交互。响应中的 `selectedId` 为被单击菜单项的 ID；若菜单直接关闭未选择任何项，值为 `null`。选中项同时触发 `ui:menuItemClicked` 事件（载荷为 `{ id, label }`）通知调用窗口。

参数中的 `x`、`y` 与 `suppressDefault` 字段为向后兼容保留，不产生定位影响；菜单始终锚定于系统光标位置。

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
