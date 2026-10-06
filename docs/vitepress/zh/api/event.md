# Event 自定义事件 API

`event` 命名空间的方法：经跨窗口通信中枢（`PortHub`）向所有窗口广播自定义事件，或定向投递给一个窗口。

## Event API

### event.emit

<!-- api-schema:begin event.emit -->
向每个窗口广播事件。接收方收到名为 `event` 的事件，数据为 `{ payload, sourceWindowId }`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `event` | `string` | 是 | 接收方订阅的事件名。不能为空。 |
| `payload` | `any` | 否 | 作为 `payload` 送达的数据；不传或为 `null` 时发送空对象。 |
| `excludeSelf` | `boolean` | 否 | 不发给调用窗口自己。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `recipients` | `integer` | 事件发往的窗口数，按打开着的窗口计。 |

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
向一个窗口发送事件。它收到名为 `event` 的事件，数据为 `{ payload, sourceWindowId }`。没有打开的窗口是该 id 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `event` | `string` | 是 | 接收方订阅的事件名。不能为空。 |
| `targetWindowId` | `string` | 是 | 接收窗口的 id，例如 `main` 或端口报告的 `windowId`。不能为空。 |
| `payload` | `any` | 否 | 作为 `payload` 送达的数据；不传或为 `null` 时发送空对象。 |

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

## 事件 envelope

- `event.emit` 广播指定事件名，`event.emitTo` 定向到一个窗口。接收方获得 envelope `{ payload, sourceWindowId }`；只有 `event.emit` 使用 `excludeSelf`。应用自定义事件名使用 `namespace:eventName` 约定，例如 `ui:themeChanged` 或 `lyrics:update`。
