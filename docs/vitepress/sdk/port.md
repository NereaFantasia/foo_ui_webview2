# fb.port Cross-Window Ports

`fb.port` provides named cross-window messaging channels. Call `connect(name)` to obtain a `portId`, then use that ID for messaging and disconnection.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block records SDK method coverage and may later be expanded with complete examples and best practices.

### disconnect()

Signature: `fb.port.disconnect(portId: string): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| portId | string | Yes | Port identifier returned by `connect(name)` |

Disconnects the port. Receivers can subscribe to `port:disconnected` through `onDisconnect(handler)`.

```javascript
const result = await fb.port.disconnect(portId);
```

### getPorts()

Signature: `fb.port.getPorts(name: string): Promise<{ ports: unknown[] }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| name | string | Yes | Named channel to inspect |

Returns the connected ports for the requested channel as `{ ports }`.

```javascript
const result = await fb.port.getPorts('transport');
```

### postMessage()

Signature: `fb.port.postMessage(portId: string, message: unknown): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| portId | string | Yes | Sending port identifier |
| message | unknown | Yes | Structured-clone-compatible message payload |

Broadcasts through the named channel. `postMessageTo(portId, targetPortId, message)` targets one connected port. `onMessage(handler)` receives `port:message`; `onConnect(handler)` and `onDisconnect(handler)` subscribe to lifecycle events. Each subscription method returns an unsubscribe function.

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
