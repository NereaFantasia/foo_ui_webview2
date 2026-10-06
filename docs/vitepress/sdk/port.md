# fb.port Cross-Window Ports

`fb.port` provides named cross-window messaging channels. Call `connect(name)` to obtain a `portId`, then use that ID for messaging and disconnection.

## connect(name)

Signature: `fb.port.connect(name: string): Promise<PortConnectResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| name | string | Yes | Channel name; ports with the same name exchange messages |

Opens a port on the named channel for the calling window and announces it with `port:connected`. The response carries the new `portId`, the channel `name` and the `windowId` the port belongs to. Any number of ports, from one window or several, can share a channel.

A port belongs to the page that opened it. It stays open until `disconnect()`, until that page starts a top-level navigation (a reload or another address; a hash change does not count), or until its WebView goes away: the popup closes, the panel is removed, or the WebView is rebuilt. Each of these closes the port with `port:disconnected`.

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
const { portId } = res;
```

## postMessage(portId, message)

Signature: `fb.port.postMessage(portId: string, message: JsonValue): Promise<PortPostMessageResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| portId | string | Yes | Sending port, opened by this window |
| message | JsonValue | Yes | Any JSON value except `null`, which the host refuses as missing |

Sends the message to every other port on the sender's channel, as a `port:message` event to each port's window. The response's `recipients` is how many of those windows took the event. A sending port of another window fails with `PERMISSION_DENIED`, an unknown one with `PORT_NOT_FOUND`.

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
| portId | string | Yes | Sending port, opened by this window |
| targetPortId | string | Yes | Port to deliver to |
| message | JsonValue | Yes | Any JSON value except `null` |

Sends the message to one port, as a `port:message` event to that port's window. Besides the sending-port failures of `postMessage()`, an unknown target port fails with `TARGET_NOT_FOUND`, and a target window that did not take the event with `OPERATION_FAILED`.

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
| name | string | No | Only list the ports of this channel; omitted, every channel |

Resolves with `{ ports }`, each `{ portId, windowId, name }`, in no particular order.

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

Closes the port and announces it with `port:disconnected`. Only the window that opened the port may close it: another window fails with `PERMISSION_DENIED`, and an id no open port has fails with `PORT_NOT_FOUND`.

```javascript
const res = await fb.port.connect('transport');
if (res.success === false) throw new Error(res.error);
// The port stays open until it is no longer needed.
await fb.port.disconnect(res.portId);
```

## Events

`onMessage(handler)`, `onConnect(handler)` and `onDisconnect(handler)` subscribe to `port:message`, `port:connected` and `port:disconnected`; each returns an unsubscribe function.

| Event | Delivered to | Payload |
| --- | --- | --- |
| `port:connected` | Every window, the opener's included | `{ portId, name, windowId }` |
| `port:disconnected` | Every window | `{ portId, name, windowId }` |
| `port:message` | The window that opened the receiving port | `{ portId, sourcePortId, sourceWindowId, message }` |

A page that closes its ports by navigating receives their `port:disconnected` events only if they arrive before it unloads.

```javascript
const offMessage = fb.port.onMessage((data) => console.log(data));
const offConnect = fb.port.onConnect((data) => console.log(data));
const offDisconnect = fb.port.onDisconnect((data) => console.log(data));

offMessage();
offConnect();
offDisconnect();
```
