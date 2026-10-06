# fb.panel 面板配置

`fb.panel` 读取和修改当前 WebView 面板的配置。

## getConfig()

签名：`fb.panel.getConfig(): Promise<PanelGetConfigResponse>`

响应的 `config` 字段是 `PanelConfig`（也以 `PanelConfigShape` 导出），含 `panelName`、`templateName`、`edgeStyle`、`urlOverride`、`transparentBackground`、`grabFocus`、`enableDragDrop` 与 `enableDevTools`。独立窗口没有面板，调用以 `code: 'NOT_FOUND'` 失败。

```javascript
const res = await fb.panel.getConfig();
if (res.success === false) throw new Error(res.error);
const { config } = res;
```

## setConfig(options)

签名：`fb.panel.setConfig(options: PanelSetConfigParams): Promise<PanelSetConfigResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `options.panelName` | `string` | 否 | 面板显示名。 |
| `options.transparentBackground` | `boolean` | 否 | 面板是否透明背景。 |
| `options.grabFocus` | `boolean` | 否 | 点击是否让面板获得键盘焦点。 |
| `options.enableDragDrop` | `boolean` | 否 | 是否允许拖放到面板。 |

页面只能改这四个键，省略的键保持原值；`PanelConfig` 的其余字段只能经面板的设置对话框修改。响应里的 `changed` 表示是否真有字段改动。

```javascript
const result = await fb.panel.setConfig({
	panelName: 'Library',
	transparentBackground: true,
	enableDragDrop: true,
});
```

## 事件

面板生命周期与配置变化使用 `panel:*` 事件族，并通过 `fb.on()` 订阅：

- `panel:configChanged` — `PanelConfigChangedPayload`
- `panel:focus` 与 `panel:blur` — payload 为空对象 `{}`
- `panel:initialized` — payload 为 `{ mode, panelMode, windowId }`
- `panel:visibilityChanged` — payload 为 `{ visible }`

```javascript
const off = fb.on('panel:configChanged', (config) => {
	console.log(config.panelName, config.transparentBackground);
});

off();
```
