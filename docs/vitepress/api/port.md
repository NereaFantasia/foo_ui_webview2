# Port API

Methods of the `port` namespace.

## port

### port.connect

<!-- api-schema:begin port.connect -->
Open a port on a named channel for the calling window and announce it with `port:connected`. Any number of ports, from one window or several, can share a channel. A port belongs to the page that opened it and stays open until `port.disconnect`, until that page starts a top-level navigation (a reload or another address; same-document navigation such as a hash change does not count, and a navigation that ends in a download still does), or until its WebView goes away: the popup closes, the panel is removed, or the WebView is rebuilt or crashes beyond recovery. Each of these closes the port with `port:disconnected` before the next page can open ports.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | `string` | Yes | Channel name; ports with the same name exchange messages. Must not be empty. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `portId` | `string` | Id of the new port; the other port methods take it. |
| `name` | `string` | The channel name. |
| `windowId` | `string` | Id of the calling window, which the port belongs to. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('port.connect', { name: 'lyrics' });
if (res.success === false) throw new Error(res.error);
const { portId } = res;
```

### port.disconnect

<!-- api-schema:begin port.disconnect -->
Close a port and announce it with `port:disconnected`. Only the window that opened the port may close it; another window fails with `PERMISSION_DENIED`, and an id no open port has with `PORT_NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `portId` | `string` | Yes | Id of the port to close. Must not be empty. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('port.disconnect', { portId: 'port_00000001' });
```

### port.getPorts

<!-- api-schema:begin port.getPorts -->
List the open ports, of every channel or of one.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `name` | `string` | No | List only the ports of this channel. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `ports` | `PortInfo[]` | The ports, in no particular order. |
| `ports[].portId` | `string` | Id of the port; the other port methods take it. |
| `ports[].windowId` | `string` | Id of the window the port belongs to, such as `main`. |
| `ports[].name` | `string` | The channel name. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('port.getPorts', { name: 'lyrics' });
if (res.success === false) throw new Error(res.error);
const { ports } = res;
```

### port.postMessage

<!-- api-schema:begin port.postMessage -->
Send a message to every other port on the sender's channel, as a `port:message` event to each port's window. The sending port must belong to the calling window, otherwise the call fails with `PERMISSION_DENIED`; an unknown sending port fails with `PORT_NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `portId` | `string` | Yes | Id of the sending port. Must not be empty. |
| `message` | `any` | Yes | The message, delivered as `message` of the event. `null` counts as missing. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `recipients` | `integer` | How many of the other ports' windows took the event. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('port.postMessage', { portId: 'port_00000001', message: { text: 'hello' } });
```

### port.postMessageTo

<!-- api-schema:begin port.postMessageTo -->
Send a message to one port, as a `port:message` event to that port's window. An unknown sending port fails with `PORT_NOT_FOUND`, a sending port of another window with `PERMISSION_DENIED`, an unknown target port with `TARGET_NOT_FOUND`, and a target window that did not take the event with `OPERATION_FAILED`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `portId` | `string` | Yes | Id of the sending port. Must not be empty. |
| `targetPortId` | `string` | Yes | Id of the port to deliver to. Must not be empty. |
| `message` | `any` | Yes | The message, delivered as `message` of the event. `null` counts as missing. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('port.postMessageTo', { portId: 'port_00000001', targetPortId: 'port_00000002', message: { text: 'sync' } });
```

## Routing and PortHub events

- `port.connect` binds the new port to the invoking window. Only that owner may disconnect it or send through it; `port.postMessage` excludes the sending port and routes `port:message` to peer ports on the same name.
- Public PortHub events are `port:connected`, `port:disconnected`, `port:message`, `state:changed`, and `state:deleted`.
