# fb.event 跨窗口事件

本页是 `fb.event` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### emit(eventName, payload?, excludeSelf?)

签名：`fb.event.emit(eventName: string, payload?: unknown, excludeSelf?: boolean): Promise<BaseResponse>`

向所有已连接窗口广播事件。将 `excludeSelf` 设为 `true` 可排除发起调用的窗口；默认值为 `false`。

```javascript
await fb.event.emit('theme:accentChanged', { color: '#4cc2ff' }, true);
```

### emitTo()

封装 `event.emitTo`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.event.emitTo(
	'theme:focusSearch',
	{ selectAll: true },
	targetWindowId
);
```

<!-- END AUTO-GENERATED SDK STUBS -->
