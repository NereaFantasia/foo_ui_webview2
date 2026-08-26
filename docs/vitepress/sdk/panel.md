# fb.panel Panel Configuration

`fb.panel` reads and updates configuration for the current WebView panel.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## Additional methods

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### getConfig()

Signature: `fb.panel.getConfig(): Promise<PanelGetConfigResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| None | — | — | This method takes no arguments. |

Returns a response envelope whose optional `config` field is a `PanelConfigShape`. It can include `panelName`, `templateName`, `edgeStyle`, `urlOverride`, `transparentBackground`, `grabFocus`, `enableDragDrop`, and `enableDevTools`.

```javascript
const { config } = await fb.panel.getConfig();
```

### setConfig()

Signature: `fb.panel.setConfig(options: PanelSetConfigParams): Promise<BaseResponse & { changed?: boolean }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.panelName` | `string` | No | Panel name. |
| `options.transparentBackground` | `boolean` | No | Whether the panel background is transparent. |
| `options.grabFocus` | `boolean` | No | Whether the panel takes focus. |
| `options.enableDragDrop` | `boolean` | No | Whether drag-and-drop is enabled. |

Returns the `panel.setConfig` response envelope. `changed` is present when reported by the host.

```javascript
const result = await fb.panel.setConfig({
	panelName: 'Library',
	transparentBackground: true,
	enableDragDrop: true,
});
```

<!-- END AUTO-GENERATED SDK STUBS -->

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
