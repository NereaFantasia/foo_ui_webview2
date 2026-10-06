# fb.panel Panel Configuration

`fb.panel` reads and updates configuration for the current WebView panel.

## getConfig()

Signature: `fb.panel.getConfig(): Promise<PanelGetConfigResponse>`

The response's `config` field is a `PanelConfig` (also exported as `PanelConfigShape`) with `panelName`, `templateName`, `edgeStyle`, `urlOverride`, `transparentBackground`, `grabFocus`, `enableDragDrop`, and `enableDevTools`. On a standalone window, which has no panel, the call fails with `code: 'NOT_FOUND'`.

```javascript
const res = await fb.panel.getConfig();
if (res.success === false) throw new Error(res.error);
const { config } = res;
```

## setConfig(options)

Signature: `fb.panel.setConfig(options: PanelSetConfigParams): Promise<PanelSetConfigResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.panelName` | `string` | No | Panel name. |
| `options.transparentBackground` | `boolean` | No | Whether the panel background is transparent. |
| `options.grabFocus` | `boolean` | No | Whether the panel takes focus. |
| `options.enableDragDrop` | `boolean` | No | Whether drag-and-drop is enabled. |

Only these four keys can be set from a page; omitted keys keep their value, and the other `PanelConfig` fields change only through the panel's settings dialog. The response's `changed` tells whether any field actually changed.

```javascript
const result = await fb.panel.setConfig({
	panelName: 'Library',
	transparentBackground: true,
	enableDragDrop: true,
});
```

## Events

Panel lifecycle and configuration changes use the `panel:*` event family. Subscribe through `fb.on()`:

- `panel:configChanged` — `PanelConfigChangedPayload`
- `panel:focus` and `panel:blur` — empty payload `{}`
- `panel:initialized` — payload `{ mode, panelMode, windowId }`
- `panel:visibilityChanged` — payload `{ visible }`

```javascript
const off = fb.on('panel:configChanged', (config) => {
	console.log(config.panelName, config.transparentBackground);
});

off();
```
