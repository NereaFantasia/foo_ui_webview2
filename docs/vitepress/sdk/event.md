# fb.event Cross-Window Events

`fb.event` emits application-defined events to connected windows. Subscribe to those event names through `fb.on()`, `fb.once()`, or `fb.off()`; the handler receives `{ payload, sourceWindowId }`, the value the sender gave and the ID of the window that sent it.

## emit(eventName, payload?, excludeSelf?)

Signature: `fb.event.emit(eventName: string, payload?: JsonValue, excludeSelf?: boolean): Promise<EventEmitResponse>`

Broadcasts an event to every connected window. Set `excludeSelf` to `true` to omit the originating window; it defaults to `false`.

```javascript
await fb.event.emit('theme:accentChanged', { color: '#4cc2ff' }, true);
```

## emitTo(eventName, payload, targetWindowId)

Signature: `fb.event.emitTo(eventName: string, payload: JsonValue, targetWindowId: string): Promise<EventEmitToResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `eventName` | `string` | Yes | Custom event name |
| `payload` | `unknown` | Yes | JSON-serializable event payload; `null` arrives as `{}` |
| `targetWindowId` | `string` | Yes | Destination window ID, such as `main` or a `windowId` a port reports |

Sends the event to one window only. Fails with `NOT_FOUND`, `details` carrying `targetWindowId`, when no open window has that ID.

```javascript
await fb.event.emitTo(
	'theme:focusSearch',
	{ selectAll: true },
	targetWindowId
);
```
