# Port 跨窗口端口 API

`port` 命名空间的方法：跨窗口通信中枢（`PortHub`）的命名通道与点对点消息。

> 方法调用使用 dot 格式（如 `port.connect`），事件监听使用 colon 格式（如 `port:message`、`state:changed`）。

## Port API

### port.connect

<!-- api-schema:begin port.connect -->
为调用窗口在一个命名频道上打开端口，并以 `port:connected` 事件通告。同一频道可以有任意多个端口，来自一个或多个窗口。端口属于打开它的页面，一直开着，直到 `port.disconnect`，或者该页面开始顶层导航（重载或跳到别的地址；锚点变化这类同文档导航不算，最后变成下载的导航也算），或者它的 WebView 消失：popup 关闭、面板移除、WebView 重建或崩溃后无法恢复。这几种情况都会以 `port:disconnected` 关闭端口，且在下一个页面能打开端口之前完成。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 是 | 频道名；同名的端口之间互通消息。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `portId` | `string` | 新端口的 id；其他端口方法都用它。 |
| `name` | `string` | 频道名。 |
| `windowId` | `string` | 调用窗口的 id，端口归它所有。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const port = await fb2k.invoke('port.connect', { name: 'lyrics' });
if (port.success === false) throw new Error(port.error);
console.log('端口 ID:', port.portId);
```

### port.disconnect

<!-- api-schema:begin port.disconnect -->
关闭端口并以 `port:disconnected` 事件通告。只有打开端口的窗口能关闭它；别的窗口以 `PERMISSION_DENIED` 失败，没有打开的端口是该 id 时以 `PORT_NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `portId` | `string` | 是 | 要关闭的端口的 id。不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('port.disconnect', { portId: 'port_00000001' });
```

### port.postMessage

<!-- api-schema:begin port.postMessage -->
向发送端口所在频道的其他每个端口发送消息，以 `port:message` 事件送到各端口的窗口。发送端口必须属于调用窗口，否则以 `PERMISSION_DENIED` 失败；发送端口不存在时以 `PORT_NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `portId` | `string` | 是 | 发送端口的 id。不能为空。 |
| `message` | `any` | 是 | 消息，作为事件的 `message` 送达。`null` 按没传处理。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `recipients` | `integer` | 其他端口的窗口里有多少个收下了事件。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('port.postMessage', {
    portId: 'port_00000001',
    message: { text: 'hello' }
});
```

### port.postMessageTo

<!-- api-schema:begin port.postMessageTo -->
向一个端口发送消息，以 `port:message` 事件送到该端口的窗口。发送端口不存在时以 `PORT_NOT_FOUND` 失败，发送端口属于别的窗口时以 `PERMISSION_DENIED` 失败，目标端口不存在时以 `TARGET_NOT_FOUND` 失败，目标窗口没有收下事件时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `portId` | `string` | 是 | 发送端口的 id。不能为空。 |
| `targetPortId` | `string` | 是 | 接收端口的 id。不能为空。 |
| `message` | `any` | 是 | 消息，作为事件的 `message` 送达。`null` 按没传处理。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('port.postMessageTo', {
    portId: 'port_00000001',
    targetPortId: 'port_00000002',
    message: 'sync'
});
```

### port.getPorts

<!-- api-schema:begin port.getPorts -->
列出打开着的端口，全部频道或某一个频道。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 否 | 只列出该频道的端口。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `ports` | `PortInfo[]` | 端口，不保证顺序。 |
| `ports[].portId` | `string` | 端口的 id；其他端口方法都用它。 |
| `ports[].windowId` | `string` | 端口所属窗口的 id，例如 `main`。 |
| `ports[].name` | `string` | 频道名。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('port.getPorts', { name: 'lyrics' });
if (result.success === false) throw new Error(result.error);
console.log(`找到 ${result.ports.length} 个端口`);
```

## 事件列表（PortHub）

| 事件名 | 触发时机 | 主要字段 |
| --- | --- | --- |
| port:connected | 创建端口 | portId, name, windowId |
| port:disconnected | 销毁端口/窗口清理 | portId, name, windowId |
| port:message | 收到端口消息 | portId, sourcePortId, sourceWindowId, message |
| state:changed | state.set 且非 silent | key, value, previousValue, sourceWindowId, expiresAt? |
| state:deleted | state.delete 或 TTL 到期 | key, sourceWindowId, reason |

## 路由与 PortHub 事件

- `port.connect` 将新端口绑定到调用窗口。只有该 owner 能断开端口或通过该端口发送；`port.postMessage` 会排除发送端口，并将 `port:message` 路由到同名的其他端口。
- 公开 PortHub 事件包括 `port:connected`、`port:disconnected`、`port:message`、`state:changed` 和 `state:deleted`。
