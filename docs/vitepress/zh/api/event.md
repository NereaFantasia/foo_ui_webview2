# Event 自定义事件 API

`event` 命名空间提供跨窗口自定义事件通信能力，可经通信中枢（`PortHub`）向全部窗口广播事件，或定向投递至指定窗口。

## Event API

### event.emit

<!-- api-schema:begin event.emit -->
向所有打开的窗口广播自定义事件（接收方收到以参数 event 为名的事件通知，负载数据为 { payload, sourceWindowId }；可通过 excludeSelf 排除调用方窗口）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `event` | `string` | 是 | 接收方订阅的事件名不能为空。 |
| `payload` | `any` | 否 | 作为 payload 送达的业务数据（不传或为 null 时发送空对象 {}） |
| `excludeSelf` | `boolean` | 否 | 是否排除当前调用方窗口默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `recipients` | `integer` | 实际接收事件的打开窗口总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('event.emit', {
    event: 'ui:themeChanged',
    payload: { theme: 'dark' }
});
```

### event.emitTo

<!-- api-schema:begin event.emitTo -->
向指定窗口定向发送自定义事件（目标窗口收到以参数 event 为名的事件通知，负载数据为 { payload, sourceWindowId }；目标窗口不存在时返回 NOT_FOUND）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `event` | `string` | 是 | 接收方订阅的事件名不能为空。 |
| `targetWindowId` | `string` | 是 | 目标窗口标识（例如 main 或端口报告的 windowId）不能为空。 |
| `payload` | `any` | 否 | 作为 payload 送达的业务数据（不传或为 null 时发送空对象 {}） |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('event.emitTo', {
    event: 'lyrics:update',
    targetWindowId: 'popup_01',
    payload: { line: 5 }
});
```

## 事件信封（Envelope） {#事件-envelope}

- `event.emit` 广播指定事件名，`event.emitTo` 定向投递至单个窗口。接收方均收到信封对象 `{ payload, sourceWindowId }`；`excludeSelf` 仅对 `event.emit` 生效。应用自定义事件建议遵循 `namespace:eventName` 命名约定（例如 `ui:themeChanged` 或 `lyrics:update`）。
