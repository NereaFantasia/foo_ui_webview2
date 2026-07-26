# fb.tray 系统托盘

本页是 `fb.tray` 的 SDK 视角文档入口。

`create()` / `setIcon()` 的顶层 `icon` 只接受裸 Base64 编码的 `.ico`
文件字节，不带 `data:` 或 `base64:` 前缀。PNG、JPEG、SVG、Data URL 或
无效 Base64 会回退到 foobar2000 主图标。`TrayMenuItem.icon` 是当前不渲染
的保留字段；WebView 菜单项图标请使用 `iconSvg`。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### appendMenuItems()

签名：`fb.tray.appendMenuItems(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.appendMenuItems` 调用结果。

```javascript
const result = await fb.tray.appendMenuItems();
```

### clearMenuItems()

签名：`fb.tray.clearMenuItems(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.clearMenuItems` 调用结果。

```javascript
const result = await fb.tray.clearMenuItems();
```

### create()

签名：`fb.tray.create(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.create` 调用结果。

```javascript
const result = await fb.tray.create();
```

### destroy()

签名：`fb.tray.destroy(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.destroy` 调用结果。

```javascript
const result = await fb.tray.destroy();
```

### getMenuItems()

签名：`fb.tray.getMenuItems(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.getMenuItems` 调用结果。

```javascript
const result = await fb.tray.getMenuItems();
```

### isVisible()

签名：`fb.tray.isVisible(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.isVisible` 调用结果。

```javascript
const result = await fb.tray.isVisible();
```

### removeMenuItems()

签名：`fb.tray.removeMenuItems(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.removeMenuItems` 调用结果。

```javascript
const result = await fb.tray.removeMenuItems();
```

### setCloseToTray()

签名：`fb.tray.setCloseToTray(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.setCloseToTray` 调用结果。

```javascript
const result = await fb.tray.setCloseToTray();
```

### setContextMenu()

签名：`fb.tray.setContextMenu(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.setContextMenu` 调用结果。

```javascript
const result = await fb.tray.setContextMenu();
```

### setIcon()

签名：`fb.tray.setIcon(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.setIcon` 调用结果。

```javascript
const result = await fb.tray.setIcon();
```

### setMenuItemState()

签名：`fb.tray.setMenuItemState(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.setMenuItemState` 调用结果。

```javascript
const result = await fb.tray.setMenuItemState();
```

### setMinimizeToTray()

签名：`fb.tray.setMinimizeToTray(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.setMinimizeToTray` 调用结果。

```javascript
const result = await fb.tray.setMinimizeToTray();
```

### setTooltip()

签名：`fb.tray.setTooltip(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `tray.setTooltip` 调用结果。

```javascript
const result = await fb.tray.setTooltip();
```

### showBalloon()

签名：`fb.tray.showBalloon(opts: { title: string; message: string; icon?: string }): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `opts.title` | `string` | 是 | 通知标题 |
| `opts.message` | `string` | 是 | 通知正文 |
| `opts.icon` | `string` | 否 | `'info'`（默认）、`'warning'` 或 `'error'` |

```javascript
await fb.tray.showBalloon({
  title: '播放',
  message: '播放列表已经结束。',
  icon: 'info',
});
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Layout guide (`layoutMode`)

`TrayMenuConfig.layoutMode` is **`'flat'` by default**. Zero-config WebView tray menus keep the legacy direct-child DOM (`#menu > .fb-item` / `.fb-sep`), so existing structure selectors do not need to migrate.

### Flat (default) — no DOM migration

```javascript
await fb.tray.setContextMenu(items, {
  render: 'webview',
  // layoutMode omitted → 'flat'
  css: `
    #menu { display: flex; flex-direction: column; gap: 2px; }
    .fb-item[data-zone="playback"] { opacity: 0.95; }
  `,
});
```

### Zones (opt-in) — top / playback / bottom containers

```javascript
const ver = await fb.config.getVersionInfo();
const plugin = ver?.plugin?.version; // probe before opting into zones
// zones 自 1.10.0 起提供；需兼容旧版时先探测运行时版本。
await fb.tray.setContextMenu(items, {
  render: 'webview',
  layoutMode: 'zones',
  css: `
    .fb-zone[data-zone="top"] { display: flex; flex-direction: column; }
    .fb-zone[data-zone="playback"] { display: grid; gap: 4px; }
    .fb-zone[data-zone="bottom"] { display: flex; flex-direction: column; }
    .fb-item[data-item-id="volume"] { padding-inline: 12px; }
  `,
});
```

### Compatibility notes

- **Native** (`render: 'native'`) ignores `layoutMode`.
- **Older runtimes** ignore the unknown key (no crash) but do **not** create `.fb-zone` wrappers.
- **`menu.show` / `fb.menu.popup`** always use legacy direct-child DOM — they never inherit tray zones.
- Stable hooks: `.fb-menu[data-depth]`, `.fb-zone[data-zone]`, `.fb-item[data-item-id|data-kind|data-depth|data-zone]`. Prefer these over `:nth-child()`.
- `data-item-token` is an internal single-show identity — **not** a public CSS contract.
- This is an opt-in capability, not a claim of “full compatibility” with every historical theme selector.

## Slider orientation & accessibility

`TrayMenuItem.orientation` is **slider-only** (`'horizontal' | 'vertical'`). Default when omitted: horizontal. The SDK passes the field through as supplied and does **not** inject a default.

### Horizontal / vertical examples

```javascript
// Horizontal (default) — old runtime / native ignore unknown orientation safely.
await fb.tray.setContextMenu([
  { id: 'vol', type: 'slider', label: 'Volume', min: 0, max: 100, value: 40 },
], { render: 'webview' });

// Vertical — probe plugin version first; do not hard-code a fake minimum version.
const { plugin } = await fb.config.getVersionInfo();
await fb.tray.setContextMenu([
  {
    id: 'vol',
    type: 'slider',
    label: 'Volume',
    min: 0,
    max: 100,
    value: 40,
    orientation: 'vertical',
  },
], { render: 'webview' });
```

### Keyboard / ARIA / reduced motion

- Navigation mode: roving `tabindex` + real row focus; Up/Down/Home/End; Enter/Space activate; submenu Right/Enter open + focus, Left close + restore focus, Escape layer-by-layer.
- Editor mode (rating / slider / segmented): Enter or Right enters; internal control focuses; Escape/Enter returns to the row.
- Vertical slider: min bottom / max top; Up/Right increase; Down/Left decrease; Home=min; End=max.
- `checked: false` still marks a checkable item (`menuitemcheckbox`); omit `checked` for a normal item.
- Default enter/exit animations honor `prefers-reduced-motion: reduce` (disable transform/transition). Custom CSS should do the same; this is **not** in protected CSS and does not change the hide protocol / `closeAnimationMs`.
- Native backend ignores `orientation` (stepped submenu degrade). Older runtimes ignore the key and keep horizontal interaction.
