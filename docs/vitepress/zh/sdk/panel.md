# fb.panel panel

本页是 `fb.panel` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### getConfig()

封装 `panel.getConfig`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.panel.getConfig(/* 参数见 TypeScript 声明 */);
```

### setConfig()

封装 `panel.setConfig`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.panel.setConfig(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->

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
