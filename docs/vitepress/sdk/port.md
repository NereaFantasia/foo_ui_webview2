# fb.port Cross-Window Ports

`fb.port` provides named cross-window messaging channels. Call `connect(name)` to obtain a `portId`, then use that ID for messaging and disconnection.

## connect(name)

Signature: `fb.port.connect(name: string): Promise<PortConnectResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| name | string | Yes | Channel name; ports with the same name exchange messages |

Opens a port on the named channel for the calling window and announces it with `port:connected`. The response carries the new `portId`, the channel `name`, and the `windowId` the port belongs to. Any number of ports from one or multiple windows can share a channel.

A port's lifecycle is bound to the page that created it. The port remains active until explicitly closed via `disconnect()`, until the page initiates top-level navigation (page reload or URL redirection; hash-only changes do not trigger closure), or until its host WebView is destroyed (popup window closure, panel removal, or WebView re-creation). Any of these conditions automatically closes the port and emits a `port:disconnected` event.

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
const { portId } = res;
```

## postMessage(portId, message)

Signature: `fb.port.postMessage(portId: string, message: JsonValue): Promise<PortPostMessageResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| portId | string | Yes | Sending port, opened by the current window |
| message | JsonValue | Yes | Any JSON value except `null` |

Broadcasts a message to all other ports registered on the sender's channel, delivered as a `port:message` event to each receiving window. The `recipients` field in the response indicates the number of windows that successfully received the event. If the specified sending port belongs to another window, the call fails with `PERMISSION_DENIED`; if the port does not exist, it fails with `PORT_NOT_FOUND`.

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
const sent = await fb.port.postMessage(res.portId, { action: 'play' });
if (sent.success === false) throw new Error(sent.error);
console.log(sent.recipients);
```

## postMessageTo(portId, targetPortId, message)

Signature: `fb.port.postMessageTo(portId: string, targetPortId: string, message: JsonValue): Promise<PortPostMessageToResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| portId | string | Yes | Sending port, opened by the current window |
| targetPortId | string | Yes | Target port identifier |
| message | JsonValue | Yes | Any JSON value except `null` |

Delivers a message to a specific target port, dispatched as a `port:message` event to that port's host window. In addition to the sender validation failures shared with `postMessage()`, if the target port does not exist, the call fails with `TARGET_NOT_FOUND`; if the target window fails to accept the event, it fails with `OPERATION_FAILED`.

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
const list = await fb.port.getPorts('transport');
if (list.success === false) throw new Error(list.error);
const target = list.ports.find((p) => p.portId !== res.portId);
if (target) await fb.port.postMessageTo(res.portId, target.portId, { action: 'pause' });
```

## getPorts(name?)

Signature: `fb.port.getPorts(name?: string): Promise<PortGetPortsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| name | string | No | Channel name filter; omitted to return ports across all channels |

Resolves with a `{ ports }` array where each entry contains `{ portId, windowId, name }`. Entry order is arbitrary.

```javascript
const res = await fb.port.getPorts('transport');
if (res.success === false) throw new Error(res.error);
console.log(res.ports.map((p) => `${p.portId} in ${p.windowId}`));
```

## disconnect(portId)

Signature: `fb.port.disconnect(portId: string): Promise<PortDisconnectResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| portId | string | Yes | Port identifier returned by `connect(name)` |

Closes the specified port and broadcasts a `port:disconnected` event. Only the window that created the port is permitted to close it; requests from other windows fail with `PERMISSION_DENIED`. If no open port matches the specified ID, the call fails with `PORT_NOT_FOUND`.

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
// Explicitly close the port when no longer needed
await fb.port.disconnect(res.portId);
```

## Events

`onMessage(handler)`, `onConnect(handler)` and `onDisconnect(handler)` subscribe to `port:message`, `port:connected` and `port:disconnected`; each returns an unsubscribe function.

| Event | Delivered to | Payload |
| --- | --- | --- |
| `port:connected` | Every window, the opener's included | `{ portId, name, windowId }` |
| `port:disconnected` | Every window | `{ portId, name, windowId }` |
| `port:message` | The window that opened the receiving port | `{ portId, sourcePortId, sourceWindowId, message }` |

When a port is closed due to page navigation, `port:disconnected` events can only be processed by the originating page if delivered before document unload completes.

```javascript
const offMessage = fb.port.onMessage((data) => console.log(data));
const offConnect = fb.port.onConnect((data) => console.log(data));
const offDisconnect = fb.port.onDisconnect((data) => console.log(data));

offMessage();
offConnect();
offDisconnect();
```
