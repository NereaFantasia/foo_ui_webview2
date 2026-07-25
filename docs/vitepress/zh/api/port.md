# Port / Event / State API

跨窗口通信中枢（`PortHub`）提供三类能力：

- `port.*`：命名通道与点对点消息
- `event.*`：跨窗口事件广播/定向投递
- `state.*`：共享键值状态（支持 TTL）

> 方法调用使用 dot 格式（如 `port.connect`），事件监听使用 colon 格式（如 `port:message`、`state:changed`）。

## Port API

### port.connect

创建命名端口。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 否 | 可选；默认 。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `portId` | `string` | 否 | 可选；默认 。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `json` | 是 | 必填。 |
| `portId` | `string` | 否 | 可选；默认 。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `message` | `json` | 是 | 必填。 |
| `portId` | `string` | 否 | 可选；默认 。 |
| `targetPortId` | `string` | 否 | 可选；默认 。 |

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

获取端口列表（可选按 `name` 过滤）。权威源：`src/api/PortApi.cpp:108-114` → `PortHub::GetPorts`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `name` | `string` | 否 | 可省略 | 可选通道名过滤；省略则返回全部端口。 |

**返回值**:

```json
{
  "success": true,
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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `event` | `string` | 否 | 可选；默认 。 |
| `excludeSelf` | `boolean` | 否 | 可选；默认 false。 |
| `payload` | `object` | 否 | 可选；默认 {}。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `event` | `string` | 否 | 可选；默认 。 |
| `payload` | `object` | 否 | 可选；默认 {}。 |
| `targetWindowId` | `string` | 否 | 可选；默认 。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 否 | 可选；默认 。 |


**返回值**: `{"code":"...","error":"..."}`

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 否 | 可选；默认 。 |
| `silent` | `boolean` | 否 | 可选；默认 false。 |
| `ttlMs` | `integer` | 否 | 可选；默认 omitted。 |
| `value` | `json` | 是 | 必填。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `key` | `string` | 否 | 可选；默认 。 |

**返回值**:

```json
{ "success": true, "existed": true }
```

```javascript
await fb2k.invoke('state.delete', { key: 'lyrics:offset' });
```

### state.keys

列出状态键，支持 `*` 通配。权威源：`src/api/PortApi.cpp:188-191` → `PortHub::GetStateKeys`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `pattern` | `string` | 否 | `*` | 过滤模式；`*` 匹配全部，末尾 `*` 为前缀匹配。 |

**返回值**:

```json
{ "success": true, "keys": ["lyrics:offset", "lyrics:theme"] }
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

## 合同补充

以下章节补齐严格参数审计发现的公开 contract；不会改变前文的已有说明。

<!-- phase3-supplement:state.set -->
### Contract 补充：`state.set`

经复核的补充 contract。权威源：`src/api/PortApi.cpp:158-175`。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `key` | `string` | 否 | `` | 可选；默认 。 |
| `silent` | `boolean` | 否 | `false` | 可选；默认 false。 |
| `ttlMs` | `integer` | 否 | 可省略 | 可选；默认 omitted。 |
| `value` | `json` | 是 | 无 | 必填。 |

#### 返回字段

| 字段 | 类型 | 可选 |
| --- | --- | --- |
| `code` | `string` | 是 |
| `error` | `string` | 是 |
| `success` | `boolean` | 否 |

语义：省略可选参数时使用 handler 默认值；失败分支及错误字段以该源文件为准。

```js
const result = await fb2k.invoke('state.set', { key: /* value */, silent: /* value */, ttlMs: /* value */, value: /* value */ });
```

## 路由、状态与事件 envelope

- `port.connect` 将新端口绑定到调用窗口。只有该 owner 能断开端口或通过该端口发送；`port.postMessage` 会排除发送端口，并将 `port:message` 路由到同名的其他端口。
- `event.emit` 广播指定事件名，`event.emitTo` 定向到一个窗口。接收方获得 envelope `{ payload, sourceWindowId }`；只有 `event.emit` 使用 `excludeSelf`。应用自定义事件名使用 `namespace:eventName` 约定，例如 `ui:themeChanged` 或 `lyrics:update`。
- `state.*` 是进程级 `PortHub` 单例持有的内存状态。在当前 foobar2000 进程中，本组件的多个 WebView 窗口可共享它；但它不会写入磁盘、进程重启后丢失，也不是跨进程或 SMP/全局持久化机制。它与 SDK 的 `fb.state` 播放状态镜像不同。
- 键不存在时，`state.get` 返回 `exists: false` 与 `value: null`。`state.set` 同时要求 `key` 和 `value`；正数 `ttlMs` 会创建过期时间戳，`silent: true` 会抑制 `state:changed`。
- `state.delete` 返回 `existed`。显式删除会发出 `reason: "deleted"` 的 `state:deleted`；过期会发出相同事件，但 `reason` 为 `"expired"`，且 `sourceWindowId` 为空。
- 公开 PortHub 事件包括 `port:connected`、`port:disconnected`、`port:message`、`state:changed` 和 `state:deleted`。它们的 payload 由 `src/api/PortHub.cpp` 发出。
