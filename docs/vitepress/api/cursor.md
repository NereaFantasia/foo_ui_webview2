# Cursor API

Methods of the `cursor` namespace.

## cursor

### cursor.isHidden

<!-- api-schema:begin cursor.isHidden -->
Report whether the calling window's cursor is hidden; `false` when the calling window cannot be resolved.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `hidden` | `boolean` | Whether the cursor is hidden. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('cursor.isHidden');
```

### cursor.setHidden

<!-- api-schema:begin cursor.setHidden -->
Hide or restore the client-area cursor of the calling window. Only a call that changes the state announces `cursor:hiddenChanged` to that window; repeating the same value reports `changed: false`. Each window keeps its own state.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `hidden` | `boolean` | Yes | `true` hides the cursor, `false` restores it. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `changed` | `boolean` | Whether the state changed; `false` when it was already as requested. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

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
const res = await fb.cursor.isHidden();
if (res.success === false) throw new Error(res.error);
const { hidden } = res;
fb.on('cursor:hiddenChanged', ({ hidden: next }) => {
	document.documentElement.classList.toggle('cursor-hidden', next);
});
```
