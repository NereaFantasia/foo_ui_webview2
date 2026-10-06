# fb.event 跨窗口事件

`fb.event` 向已连接的窗口发出应用自定义的事件。接收方用 `fb.on()`、`fb.once()` 或 `fb.off()` 按事件名订阅，处理函数收到 `{ payload, sourceWindowId }`：`payload` 是发送方给的值，`sourceWindowId` 是发送窗口的 ID。

## emit(eventName, payload?, excludeSelf?)

签名：`fb.event.emit(eventName: string, payload?: JsonValue, excludeSelf?: boolean): Promise<EventEmitResponse>`

向所有已连接窗口广播事件。将 `excludeSelf` 设为 `true` 可排除发起调用的窗口；默认值为 `false`。

```javascript
await fb.event.emit('theme:accentChanged', { color: '#4cc2ff' }, true);
```

## emitTo(eventName, payload, targetWindowId)

签名：`fb.event.emitTo(eventName: string, payload: JsonValue, targetWindowId: string): Promise<EventEmitToResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `eventName` | `string` | 是 | 自定义事件名 |
| `payload` | `unknown` | 是 | 可序列化为 JSON 的事件载荷；`null` 送达时为 `{}` |
| `targetWindowId` | `string` | 是 | 目标窗口 ID，如 `main`，或端口报告的 `windowId` |

只把事件发给一个窗口。没有打开的窗口用这个 ID 时以 `NOT_FOUND` 失败，`details` 里带 `targetWindowId`。

```javascript
await fb.event.emitTo(
	'theme:focusSearch',
	{ selectAll: true },
	targetWindowId
);
```
