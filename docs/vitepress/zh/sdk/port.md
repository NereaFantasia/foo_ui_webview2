# fb.port 跨窗口端口

`fb.port` 提供具名的跨窗口消息通道。先调用 `connect(name)` 取得 `portId`，之后使用该 ID 发送消息或断开连接。

## connect(name)

签名：`fb.port.connect(name: string): Promise<PortConnectResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| name | string | 是 | 频道名；同名的端口之间互通消息 |

为调用窗口在指定频道上打开通信端口，并广播 `port:connected` 事件。响应包含新建端口的 `portId`、频道名 `name` 以及所属窗口标识 `windowId`。同一频道支持由单个或多个窗口建立任意数量的端口。

端口生命周期绑定至所属页面。显式调用 `disconnect()`、页面触发顶层导航（页面重载或地址跳转，URL 锚点变化除外）、或所属 WebView 销毁（弹出窗口关闭、面板移除、WebView 实例重建）时，端口自动关闭并广播 `port:disconnected` 事件。

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
const { portId } = res;
```

## postMessage(portId, message)

签名：`fb.port.postMessage(portId: string, message: JsonValue): Promise<PortPostMessageResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| portId | string | 是 | 发送端端口，须由当前窗口创建 |
| message | JsonValue | 是 | 除 `null` 以外的任意 JSON 值 |

向发送端口所在频道的其他全部端口广播消息，目标窗口将接收到 `port:message` 事件。响应中的 `recipients` 为成功接收事件的窗口数量。若发送端口属于其他窗口，操作以 `PERMISSION_DENIED` 错误拒绝；若端口不存在，以 `PORT_NOT_FOUND` 错误拒绝。

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
const sent = await fb.port.postMessage(res.portId, { action: 'play' });
if (sent.success === false) throw new Error(sent.error);
console.log(sent.recipients);
```

## postMessageTo(portId, targetPortId, message)

签名：`fb.port.postMessageTo(portId: string, targetPortId: string, message: JsonValue): Promise<PortPostMessageToResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| portId | string | 是 | 发送端端口，须由当前窗口创建 |
| targetPortId | string | 是 | 接收端端口标识 |
| message | JsonValue | 是 | 除 `null` 以外的任意 JSON 值 |

向指定端口定向发送消息，目标窗口将接收到 `port:message` 事件。除与 `postMessage()` 相同的发送端校验错误外，若目标端口不存在，以 `TARGET_NOT_FOUND` 错误拒绝；若目标窗口未成功接收事件，以 `OPERATION_FAILED` 错误拒绝。

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
const list = await fb.port.getPorts('transport');
if (list.success === false) throw new Error(list.error);
const target = list.ports.find((p) => p.portId !== res.portId);
if (target) await fb.port.postMessageTo(res.portId, target.portId, { action: 'pause' });
```

## getPorts(name?)

签名：`fb.port.getPorts(name?: string): Promise<PortGetPortsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| name | string | 否 | 目标频道名称；省略时列出全部频道的端口 |

返回 `{ ports }` 列表，元素结构为 `{ portId, windowId, name }`，列表顺序未作保证。

```javascript
const res = await fb.port.getPorts('transport');
if (res.success === false) throw new Error(res.error);
console.log(res.ports.map((p) => `${p.portId} 位于 ${p.windowId}`));
```

## disconnect(portId)

签名：`fb.port.disconnect(portId: string): Promise<PortDisconnectResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| portId | string | 是 | `connect(name)` 返回的端口 ID |

关闭指定端口并广播 `port:disconnected` 事件。仅创建端口的窗口具备关闭权限；其他窗口尝试关闭将以 `PERMISSION_DENIED` 错误拒绝；若指定 ID 的端口不存在，以 `PORT_NOT_FOUND` 错误拒绝。

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
// 显式关闭不再使用的端口
await fb.port.disconnect(res.portId);
```

## 事件

`onMessage(handler)`、`onConnect(handler)` 与 `onDisconnect(handler)` 分别订阅 `port:message`、`port:connected` 与 `port:disconnected`，都返回取消订阅函数。

| 事件 | 送达 | 载荷 |
| --- | --- | --- |
| `port:connected` | 所有窗口，包括打开端口的窗口 | `{ portId, name, windowId }` |
| `port:disconnected` | 所有窗口 | `{ portId, name, windowId }` |
| `port:message` | 打开接收端口的窗口 | `{ portId, sourcePortId, sourceWindowId, message }` |

因页面导航而触发关闭的端口，其 `port:disconnected` 事件仅在页面卸载（unload）完成前到达时可被该页面接收。

```javascript
const offMessage = fb.port.onMessage((data) => console.log(data));
const offConnect = fb.port.onConnect((data) => console.log(data));
const offDisconnect = fb.port.onDisconnect((data) => console.log(data));

offMessage();
offConnect();
offDisconnect();
```
