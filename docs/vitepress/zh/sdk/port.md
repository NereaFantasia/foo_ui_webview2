# fb.port 跨窗口端口

`fb.port` 提供具名的跨窗口消息通道。先调用 `connect(name)` 取得 `portId`，之后使用该 ID 发送消息或断开连接。

## connect(name)

签名：`fb.port.connect(name: string): Promise<PortConnectResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| name | string | 是 | 频道名；同名的端口之间互通消息 |

为调用窗口在命名频道上打开一个端口，并以 `port:connected` 通告。响应带新端口的 `portId`、频道名 `name` 与端口所属的 `windowId`。同一频道可以有任意多个端口，来自一个或多个窗口。

端口属于打开它的页面，一直开着，直到调用 `disconnect()`，或者该页面开始顶层导航（重载或跳到别的地址；锚点变化不算），或者它的 WebView 消失：popup 关闭、面板移除、WebView 重建。这几种情况都会以 `port:disconnected` 关闭端口。

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
const { portId } = res;
```

## postMessage(portId, message)

签名：`fb.port.postMessage(portId: string, message: JsonValue): Promise<PortPostMessageResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| portId | string | 是 | 发送端口，须由本窗口打开 |
| message | JsonValue | 是 | 除 `null` 以外的任意 JSON 值；`null` 被宿主按没传处理 |

向发送端口所在频道的其他每个端口发送消息，以 `port:message` 事件送到各端口的窗口。响应里的 `recipients` 是收下事件的窗口数。发送端口属于别的窗口时以 `PERMISSION_DENIED` 失败，不存在时以 `PORT_NOT_FOUND` 失败。

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
| portId | string | 是 | 发送端口，须由本窗口打开 |
| targetPortId | string | 是 | 接收端口 |
| message | JsonValue | 是 | 除 `null` 以外的任意 JSON 值 |

向一个端口发送消息，以 `port:message` 事件送到该端口的窗口。除了与 `postMessage()` 相同的发送端口失败，目标端口不存在时以 `TARGET_NOT_FOUND` 失败，目标窗口没有收下事件时以 `OPERATION_FAILED` 失败。

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
| name | string | 否 | 只列这个通道的端口；省略时列出全部通道 |

返回 `{ ports }`，每项为 `{ portId, windowId, name }`，顺序不固定。

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

关闭端口并以 `port:disconnected` 通告。只有打开端口的窗口能关闭它：别的窗口以 `PERMISSION_DENIED` 失败，没有打开的端口是该 ID 时以 `PORT_NOT_FOUND` 失败。

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
// 端口用完之后再关闭。
await fb.port.disconnect(res.portId);
```

## 事件

`onMessage(handler)`、`onConnect(handler)` 与 `onDisconnect(handler)` 分别订阅 `port:message`、`port:connected` 与 `port:disconnected`，都返回取消订阅函数。

| 事件 | 送达 | 载荷 |
| --- | --- | --- |
| `port:connected` | 所有窗口，包括打开端口的窗口 | `{ portId, name, windowId }` |
| `port:disconnected` | 所有窗口 | `{ portId, name, windowId }` |
| `port:message` | 打开接收端口的窗口 | `{ portId, sourcePortId, sourceWindowId, message }` |

因导航而关闭端口的页面，只有在卸载之前到达的 `port:disconnected` 才收得到。

```javascript
const offMessage = fb.port.onMessage((data) => console.log(data));
const offConnect = fb.port.onConnect((data) => console.log(data));
const offDisconnect = fb.port.onDisconnect((data) => console.log(data));

offMessage();
offConnect();
offDisconnect();
```
