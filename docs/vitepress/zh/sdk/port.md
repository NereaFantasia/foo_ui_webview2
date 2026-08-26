# fb.port 跨窗口端口

`fb.port` 提供具名的跨窗口消息通道。先调用 `connect(name)` 取得 `portId`，之后使用该 ID 发送消息或断开连接。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### disconnect()

封装 `port.disconnect`。接收端可通过 `onDisconnect(handler)` 订阅 `port:disconnected`。

```javascript
const result = await fb.port.disconnect();
```

### getPorts()

封装 `port.getPorts`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.port.getPorts(/* 参数见 TypeScript 声明 */);
```

### postMessage()

封装 `port.postMessage`、`port.postMessageTo`。`postMessageTo(portId, targetPortId, message)` 可定向发送到一个已连接端口；`onMessage(handler)` 接收 `port:message`，`onConnect(handler)` 与 `onDisconnect(handler)` 订阅生命周期事件。每个订阅方法都返回取消订阅函数。

```javascript
const { portId } = await fb.port.connect('transport');
const result = await fb.port.postMessage(portId, { action: 'play' });
await fb.port.postMessageTo(portId, targetPortId, { action: 'pause' });

const offMessage = fb.port.onMessage((data) => console.log(data));
const offConnect = fb.port.onConnect((data) => console.log(data));
const offDisconnect = fb.port.onDisconnect((data) => console.log(data));

offMessage();
offConnect();
offDisconnect();
```

<!-- END AUTO-GENERATED SDK STUBS -->
