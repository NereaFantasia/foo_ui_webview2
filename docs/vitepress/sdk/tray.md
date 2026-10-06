# `fb.tray` system tray

`fb.tray` controls the application-scoped tray icon, notifications, tray visibility behavior, and native or WebView-rendered context menus.

## appendMenuItems(items, position?)

Signature: `fb.tray.appendMenuItems(items: TrayMenuItem[], position?: TrayMenuPosition): Promise<TrayAppendMenuItemsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `TrayMenuItem[]` | Yes | Items to append |
| `position` | `TrayMenuPosition` | No | Target zone; defaults to `'top'` |

Returns the `tray.appendMenuItems` result.

```javascript
const result = await fb.tray.appendMenuItems([{ id: 'settings', label: 'Settings' }]);
```

## clearMenuItems(position?)

Signature: `fb.tray.clearMenuItems(position?: TrayMenuPosition): Promise<TrayClearMenuItemsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `position` | `TrayMenuPosition` | No | Zone to clear; omitted to clear all zones |

Returns the `tray.clearMenuItems` result.

```javascript
const result = await fb.tray.clearMenuItems();
```

## create(opts?)

Signature: `fb.tray.create(opts?: { icon?: string | null; tooltip?: string }): Promise<TrayCreateResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts` | `{ icon?: string \| null; tooltip?: string }` | No | Initial icon as raw Base64 `.ico` file bytes, plus tooltip |

Creates the tray icon. Call this before other `tray.*` methods.

```javascript
const result = await fb.tray.create();
```

## destroy()

Signature: `fb.tray.destroy(): Promise<TrayDestroyResponse>`

Removes the tray icon.

```javascript
const result = await fb.tray.destroy();
```

## getMenuItems()

Signature: `fb.tray.getMenuItems(): Promise<TrayGetMenuItemsResponse>`

Returns user-defined items flattened in `top -> playback -> bottom` order. Runtime-injected playback/system items are excluded.

```javascript
const result = await fb.tray.getMenuItems();
```

## isVisible()

Signature: `fb.tray.isVisible(): Promise<TrayIsVisibleResponse>`

Returns whether the tray icon currently exists.

```javascript
const result = await fb.tray.isVisible();
```

## removeMenuItems(ids)

Signature: `fb.tray.removeMenuItems(ids: string[]): Promise<TrayRemoveMenuItemsResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `ids` | `string[]` | Yes | Item IDs to remove from all zones |

Returns the operation result and number of removed items.

```javascript
const result = await fb.tray.removeMenuItems(['settings']);
```

## setCloseToTray(enabled)

Signature: `fb.tray.setCloseToTray(enabled: boolean): Promise<TraySetCloseToTrayResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | Yes | Hide to tray instead of quitting on close |

Returns the `tray.setCloseToTray` result.

```javascript
const result = await fb.tray.setCloseToTray(true);
```

## setContextMenu(items, config?)

Signature: `fb.tray.setContextMenu(items: TrayMenuItem[], config?: TrayMenuConfig): Promise<TraySetContextMenuResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `TrayMenuItem[]` | Yes | User-defined menu items |
| `config` | `TrayMenuConfig` | No | Zone, renderer, layout, styling, and behavior options |

Replaces items in the configured zone; the other zones remain intact.

```javascript
const result = await fb.tray.setContextMenu([{ id: 'settings', label: 'Settings' }]);
```

## setIcon(icon?)

Signature: `fb.tray.setIcon(icon?: string | null): Promise<TraySetIconResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `icon` | `string \| null` | No | Raw Base64-encoded `.ico` file bytes without a prefix; omitted/null uses the main icon |

Returns the `tray.setIcon` result.

PNG, JPEG, SVG, Data URLs, and `base64:`-prefixed strings are not valid tray
icon payloads. Invalid values fall back to the foobar2000 main icon. The
separate `TrayMenuItem.icon` field is reserved and not rendered by either menu
backend; use `iconSvg` for WebView-rendered menu-item icons.

```javascript
const result = await fb.tray.setIcon();
```

## setMenuItemState(id, state)

Signature: `fb.tray.setMenuItemState(id: string, state: { checked?: boolean; enabled?: boolean }): Promise<TraySetMenuItemStateResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `string` | Yes | Item ID searched recursively across all zones |
| `state` | `{ checked?: boolean; enabled?: boolean }` | Yes | At least one field must be supplied |

`found` is `true` on success; an id no row has fails with `code: 'NOT_FOUND'`. The native menu reflects changes on the next open.

```javascript
const result = await fb.tray.setMenuItemState('settings', { enabled: false });
```

## setMenuZones(zones, config?)

Signature: `fb.tray.setMenuZones(zones: Omit<TraySetMenuZonesParams, 'config'>, config?: TrayMenuConfig): Promise<TraySetMenuZonesResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `zones` | `{ top?, playback?, bottom? }` | Yes | Items of each zone; a zone left out is cleared |
| `config` | `TrayMenuConfig` | No | Renderer, layout, styling, and behavior options; `customPosition` does not apply to this call |

Replaces all three zones in one step, so a menu opened meanwhile never shows a
partial update and the later of two quick calls is what remains. When an item
is rejected or a resource limit is exceeded, nothing changes, `config`
included.

```javascript
const result = await fb.tray.setMenuZones({
    top: [{ id: 'settings', label: 'Settings' }],
    bottom: [{ id: 'about', label: 'About' }],
});
```

## setMinimizeToTray(enabled)

Signature: `fb.tray.setMinimizeToTray(enabled: boolean): Promise<TraySetMinimizeToTrayResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `enabled` | `boolean` | Yes | Hide to tray instead of the taskbar on minimize |

Returns the `tray.setMinimizeToTray` result.

```javascript
const result = await fb.tray.setMinimizeToTray(true);
```

## setTooltip(tooltip)

Signature: `fb.tray.setTooltip(tooltip: string): Promise<TraySetTooltipResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `tooltip` | `string` | Yes | Tooltip text, up to 128 characters |

Returns the `tray.setTooltip` result.

```javascript
const result = await fb.tray.setTooltip('foobar2000');
```

## showBalloon(opts)

Signature: `fb.tray.showBalloon(opts: { title: string; message: string; icon?: TrayShowBalloonParams['icon'] }): Promise<TrayShowBalloonResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `opts.title` | `string` | Yes | Notification title |
| `opts.message` | `string` | Yes | Notification body |
| `opts.icon` | `string` | No | `'info'` (default), `'warning'`, or `'error'` |

```javascript
await fb.tray.showBalloon({
  title: 'Playback',
  message: 'The playlist has finished.',
  icon: 'info',
});
```

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
if (ver.success === false) throw new Error(ver.error);
const plugin = ver?.plugin?.version; // probe before opting into zones
// Zones ship from 1.10.0; probe the runtime version if you must support older hosts.
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
const res = await fb.config.getVersionInfo();
if (res.success === false) throw new Error(res.error);
const { plugin } = res;
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
- Editor mode (rating / slider / segmented): Enter or Right enters; focus moves to the internal control — `.fb-rating-control`, `.fb-slider-control`, or the selected `.fb-seg-btn`, all marked `data-fb-focusable="1"`; Escape/Enter returns to the row. These three are the only elements that take real DOM focus, so they are where the browser draws its default focus ring (`:focus-visible`). Neither the default nor the protected stylesheet styles that ring; in `cssReplace` mode, style or reset it on these three selectors rather than on `.fb-item`.
- Mouse wheel over a rating / slider / segmented row steps its value (scroll up = increase), matching the arrow keys. The row must be enabled, and a slider must be non-constant. While a *different* row is in editor mode the wheel is ignored and the menu scrolls as usual.
- Vertical slider: min bottom / max top; Up/Right increase; Down/Left decrease; Home=min; End=max.
- `checked: false` still marks a checkable item (`menuitemcheckbox`); omit `checked` for a normal item.
- Default enter/exit animations honor `prefers-reduced-motion: reduce` (disable transform/transition). Custom CSS should do the same; this is **not** in protected CSS and does not change the hide protocol / `closeAnimationMs`.
- Native backend ignores `orientation` (stepped submenu degrade). Older runtimes ignore the key and keep horizontal interaction.
