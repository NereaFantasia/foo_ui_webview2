# Event API

Methods of the `event` namespace.

## event

### event.emit

<!-- api-schema:begin event.emit -->
Broadcast an event to every window. Receivers get an event named `event` whose data is `{ payload, sourceWindowId }`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `event` | `string` | Yes | Event name the receivers subscribe to. Must not be empty. |
| `payload` | `any` | No | Data delivered as `payload`; absent or `null` sends an empty object. |
| `excludeSelf` | `boolean` | No | Leave out the calling window. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `recipients` | `integer` | How many windows the event was sent to, counted from the open windows. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('event.emit', { event: 'ui:themeChanged', payload: { theme: 'dark' } });
```

### event.emitTo

<!-- api-schema:begin event.emitTo -->
Send an event to one window. It receives an event named `event` whose data is `{ payload, sourceWindowId }`. A window id no open window has fails with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `event` | `string` | Yes | Event name the receiver subscribes to. Must not be empty. |
| `targetWindowId` | `string` | Yes | Id of the window to send to, such as `main` or the `windowId` a port reported. Must not be empty. |
| `payload` | `any` | No | Data delivered as `payload`; absent or `null` sends an empty object. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('event.emitTo', { event: 'lyrics:update', targetWindowId: 'popup_01', payload: { line: 5 } });
```

## Event envelopes

- `event.emit` broadcasts the requested event name and `event.emitTo` targets one window. Receivers get the envelope `{ payload, sourceWindowId }`; `excludeSelf` affects only `event.emit`. Use the `namespace:eventName` convention for application-defined event names, such as `ui:themeChanged` or `lyrics:update`.
