# Port / Event / State API 

跨窗口通信中枢（`PortHub`）提供三类能力：

- `port.*`：命名通道与点对点消息
- `event.*`：跨窗口事件广播/定向投递
- `state.*`：共享键值状态（支持 TTL）

> 方法调用使用 dot 格式（如 `port.connect`），事件监听使用 colon 格式（如 `port:message`、`state:changed`）。

## Port API 

### port.connect 

创建命名端口。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| name | string | ? | 端口通道名 |

**返回值（成功）**:

```json
{
  "portId": "port_00000001",
  "name": "lyrics",
  "windowId": "main"
}
```

**返回值（失败）**:

```json
{ "error": "Port name is required", "code": "INVALID_PARAMS" }
```

```javascript
const port = await fb2k.invoke('port.connect', { name: 'lyrics' });
console.log('端口 ID:', port.portId);
```

### port.disconnect 

销毁端口。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| portId | string | ? | 端口 ID |

**返回值（成功）**: `{ "success": true }`

**返回值（失败）**:

```json
{ "error": "Port not found", "code": "PORT_NOT_FOUND" }
```

```javascript
await fb2k.invoke('port.disconnect', { portId: 'port_00000001' });
```

### port.postMessage 

向同名通道的其它端口发送消息（不回送给自身）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| portId | string | ? | 发送方端口 ID |
| message | any | ? | 消息体 |

**返回值（成功）**:

```json
{ "success": true, "recipients": 2 }
```

**返回值（失败）**:

```json
{ "success": false, "error": "Port not found", "code": "PORT_NOT_FOUND" }
```

```javascript
await fb2k.invoke('port.postMessage', {
    portId: 'port_00000001',
    message: { text: 'hello' }
});
```

### port.postMessageTo 

向指定端口发送消息。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| portId | string | ? | 发送方端口 ID |
| targetPortId | string | ? | 目标端口 ID |
| message | any | ? | 消息体 |

**返回值**:

```json
{ "success": true }
```

失败时可能返回：`PORT_NOT_FOUND` / `TARGET_NOT_FOUND`。

```javascript
await fb2k.invoke('port.postMessageTo', {
    portId: 'port_00000001',
    targetPortId: 'port_00000002',
    message: 'sync'
});
```

### port.getPorts 

获取端口列表（可选按 `name` 过滤）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| name | string | ? | 仅返回该通道名端口 |

**返回值**:

```json
{
  "ports": [
    { "portId": "port_00000001", "name": "lyrics", "windowId": "main" }
  ]
}
```

```javascript
const result = await fb2k.invoke('port.getPorts', { name: 'lyrics' });
console.log(`找到 ${result.ports.length} 个端口`);
```

## Event API 

### event.emit 

广播自定义事件到所有窗口。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| event | string | ? | 事件名（建议 namespace:eventName） |
| payload | object | ? | 事件负载，默认 {} |
| excludeSelf | boolean | ? | 是否排除发送窗口，默认 false |

**返回值**:

```json
{ "success": true, "recipients": 3 }
```

> 接收端实际收到的事件 envelope 结构：

```json
{ "payload": { ... }, "sourceWindowId": "main" }
```

```javascript
await fb2k.invoke('event.emit', {
    event: 'ui:themeChanged',
    payload: { theme: 'dark' }
});
```

### event.emitTo 

定向投递事件到指定窗口。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| event | string | ? | 事件名 |
| targetWindowId | string | ? | 目标窗口 ID |
| payload | object | ? | 事件负载，默认 {} |

**返回值**:

```json
{ "success": true }
```

```javascript
await fb2k.invoke('event.emitTo', {
    event: 'lyrics:update',
    targetWindowId: 'popup_01',
    payload: { line: 5 }
});
```

## State API 

### state.get 

读取共享状态。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| key | string | ? | 状态键 |

**返回值（存在）**:

```json
{ "key": "lyrics:offset", "value": 120, "exists": true, "expiresAt": 1760000000000 }
```

**返回值（不存在）**:

```json
{ "value": null, "exists": false }
```

```javascript
const result = await fb2k.invoke('state.get', { key: 'lyrics:offset' });
if (result.exists) console.log('偶移:', result.value);
```

### state.set 

设置共享状态。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| key | string | ? | 状态键 |
| value | any | ? | 状态值 |
| silent | boolean | ? | 不广播 state:changed，默认 false |
| ttlMs | number | ? | 生存时间（毫秒），到期自动删除 |

**返回值**:

```json
{ "success": true, "expiresAt": 1760000000000 }
```

```javascript
await fb2k.invoke('state.set', {
    key: 'lyrics:offset',
    value: 120,
    ttlMs: 60000
});
```

### state.delete 

删除共享状态。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| key | string | ? | 状态键 |

**返回值**:

```json
{ "success": true, "existed": true }
```

```javascript
await fb2k.invoke('state.delete', { key: 'lyrics:offset' });
```

### state.keys 

列出状态键，支持 `*` 通配。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| pattern | string | ? | 模式，默认 "*"，如 "lyrics:*" |

**返回值**:

```json
{ "keys": ["lyrics:offset", "lyrics:theme"] }
```

```javascript
const result = await fb2k.invoke('state.keys', { pattern: 'lyrics:*' });
console.log('状态键:', result.keys);
```

## 事件列表（PortHub） 

| 事件名 | 触发时机 | 主要字段 |
| --- | --- | --- |
| port:connected | 创建端口 | portId, name, windowId |
| port:disconnected | 销毁端口/窗口清理 | portId, name, windowId |
| port:message | 收到端口消息 | portId, sourcePortId, sourceWindowId, message |
| state:changed | state.set 且非 silent | key, value, previousValue, sourceWindowId, expiresAt? |
| state:deleted | state.delete 或 TTL 到期 | key, sourceWindowId, reason |
