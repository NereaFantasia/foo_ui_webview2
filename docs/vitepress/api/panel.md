# Panel API

Methods of the `panel` namespace.

## panel

### panel.getConfig

<!-- api-schema:begin panel.getConfig -->
Report the configuration of the calling panel. A panel configuration exists only for a DUI element or a CUI panel; on a standalone window the call fails.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `config` | `PanelConfig` | The configuration. |
| `config.panelName` | `string` | Display name of the panel. |
| `config.templateName` | `string` | Name of the page template the panel loads. |
| `config.edgeStyle` | `integer` | Edge style of the panel frame: `0` none, `1` sunken, `2` grey. |
| `config.urlOverride` | `string` | URL loaded instead of the template; empty when none. |
| `config.transparentBackground` | `boolean` | Whether the panel renders with a transparent background. |
| `config.grabFocus` | `boolean` | Whether a click gives the panel keyboard focus. |
| `config.enableDragDrop` | `boolean` | Whether files can be dropped onto the panel. |
| `config.enableDevTools` | `boolean` | Whether the developer tools are enabled. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('panel.getConfig');
```

### panel.setConfig

<!-- api-schema:begin panel.setConfig -->
Change the calling panel's configuration. Only the keys below can be set from a page; omitted keys keep their value. The other fields change only through the panel's dialog.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `panelName` | `string` | No | Display name of the panel. |
| `transparentBackground` | `boolean` | No | Whether the panel renders with a transparent background. |
| `grabFocus` | `boolean` | No | Whether a click gives the panel keyboard focus. |
| `enableDragDrop` | `boolean` | No | Whether files can be dropped onto the panel. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `changed` | `boolean` | Whether anything changed. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('panel.setConfig', { grabFocus: true });
```

## Owner-family behavior and limits

- `panel.setConfig` changes only its documented panel fields.
