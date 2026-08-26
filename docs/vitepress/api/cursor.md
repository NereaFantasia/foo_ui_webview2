# Cursor API

English API reference for the `cursor` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## cursor

### cursor.isHidden


_No parameters._

**Returns**: `{"hidden":"..."}`

```js
const result = await fb2k.invoke('cursor.isHidden');
```

### cursor.setHidden


| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `hidden` | `boolean` | Yes | `true` hides the client-area cursor, `false` restores it. |

**Returns**: `{"changed":"...","error":"...","success":true}`

```js
await fb2k.invoke('cursor.setHidden', { hidden: true });
```

## Cursor visibility lifecycle

`cursor.setHidden` changes the cursor state for the calling WebView host, not
for the desktop or another popup. It requires a boolean `hidden` value and
returns `changed: false` when the requested state was already active. When the
state actually changes, the caller receives `cursor:hiddenChanged` with
`{ hidden }`; the event is not broadcast to other windows.

The `foo_ui_webview2` host uses Visual Hosting, so CSS `cursor: none` alone is
not a reliable replacement for this API. A theme may use both approaches: CSS
for the element-level visual rule and this API for idle-time behavior.

```js
import { fb } from 'foo-webview-sdk/bridge';

await fb.cursor.setHidden(true);
const { hidden } = await fb.cursor.isHidden();
fb.on('cursor:hiddenChanged', ({ hidden: next }) => {
	document.documentElement.classList.toggle('cursor-hidden', next);
});
```
