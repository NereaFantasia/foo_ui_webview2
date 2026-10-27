# Port 跨窗口端口 API

提供基于 PortHub 跨窗口通信中枢的命名通道广播与点对点消息传递能力。

> 方法调用使用 dot 格式（如 `port.connect`），事件监听使用 colon 格式（如 `port:message`、`state:changed`）。

## Port API

### port.connect

<!-- api-schema:begin port.connect -->
为调用窗口在指定的命名频道上创建并打开通信端口（向所有窗口广播 port:connected 事件；同一频道允许多个窗口的端口共存；端口生命周期绑定至打开它的页面上下文，持续生效直至显式调用 port.disconnect、页面发生顶层导航或宿主 WebView 销毁重构；端口关闭时自动广播 port:disconnected 事件并在后续新页面打开端口前完成清理）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 是 | 待连接的频道名称（同名频道内的端口之间可互相通信）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `portId` | `string` | 新分配的端口唯一标识符 portId |
| `name` | `string` | 端口连接的频道名称 |
| `windowId` | `string` | 拥有该端口的调用窗口标识符 windowId |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const port = await fb2k.invoke('port.connect', { name: 'lyrics' });
if (port.success === false) throw new Error(port.error);
console.log('端口 ID:', port.portId);
```

### port.disconnect

<!-- api-schema:begin port.disconnect -->
关闭指定的通信端口并广播 port:disconnected 事件（仅允许打开该端口的拥有者窗口调用；跨窗口调用将以 PERMISSION_DENIED 拒绝；端口不存在时返回 PORT_NOT_FOUND）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `portId` | `string` | 是 | 待关闭的端口唯一标识符 portId不能为空。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('port.disconnect', { portId: 'port_00000001' });
```

### port.postMessage

<!-- api-schema:begin port.postMessage -->
向指定发送端口所在频道内的其他全部对等端口广播消息（以 port:message 事件定向派发给各对等端口所属的窗口；发送端口必须归属于调用窗口，否则返回 PERMISSION_DENIED；端口不存在时返回 PORT_NOT_FOUND）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `portId` | `string` | 是 | 负责发送消息的源端口标识符 portId不能为空。 |
| `message` | `any` | 是 | 待发送的消息正文（将作为事件的 message 载荷送达；传入 null 按缺失处理） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `recipients` | `integer` | 成功接收并收下该消息事件的对等端口所属窗口总数 |

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
向指定目标端口单播发送点对点消息（以 port:message 事件定向送达目标端口所属窗口；发送端口须属于调用窗口，否则返回 PERMISSION_DENIED；发送端口不存在返回 PORT_NOT_FOUND；目标端口不存在返回 TARGET_NOT_FOUND；目标窗口未能接收事件时返回 OPERATION_FAILED）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `portId` | `string` | 是 | 负责发送消息的源端口标识符 portId不能为空。 |
| `targetPortId` | `string` | 是 | 目标接收端口的唯一标识符 targetPortId不能为空。 |
| `message` | `any` | 是 | 待发送的消息正文（将作为事件的 message 载荷送达；传入 null 按缺失处理） |

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
获取当前所有活跃端口列表（可指定频道名称仅查询该频道内的端口）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 否 | 待筛选的目标频道名称（缺省时查询全部频道的端口） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `ports` | `PortInfo[]` | 活跃端口列表（顺序不固定） |
| `ports[].portId` | `string` | 端口唯一标识符 portId |
| `ports[].windowId` | `string` | 端口所属的窗口标识符 windowId（如 main） |
| `ports[].name` | `string` | 端口所属的频道名称 |

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
