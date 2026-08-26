# fb.event Cross-Window Events

`fb.event` emits application-defined events to connected windows. Subscribe to those event names through `fb.on()`, `fb.once()`, or `fb.off()`.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

### emit(eventName, payload?, excludeSelf?)

Signature: `fb.event.emit(eventName: string, payload?: unknown, excludeSelf?: boolean): Promise<BaseResponse>`

Broadcasts an event to every connected window. Set `excludeSelf` to `true` to omit the originating window; it defaults to `false`.

```javascript
await fb.event.emit('theme:accentChanged', { color: '#4cc2ff' }, true);
```

### emitTo(eventName, payload, targetWindowId)

Signature: `fb.event.emitTo(eventName: string, payload: unknown, targetWindowId: string): Promise<BaseResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `eventName` | `string` | Yes | Custom event name |
| `payload` | `unknown` | Yes | JSON-serializable event payload |
| `targetWindowId` | `string` | Yes | Destination window ID |

```javascript
await fb.event.emitTo(
	'theme:focusSearch',
	{ selectAll: true },
	targetWindowId
);
```

<!-- END AUTO-GENERATED SDK STUBS -->
