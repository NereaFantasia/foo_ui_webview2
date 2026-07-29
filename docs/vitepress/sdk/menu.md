# fb.menu Menu APIs

`fb.menu` queries and executes main or context menu commands and provides WebView-rendered popup menus.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK Method Stubs

> This block maintains SDK-facing method coverage and may be expanded with complete examples and best practices.

### getContextMenu()

Signature: `fb.menu.getContextMenu(options?: MenuGetContextMenuParams): Promise<MenuGetContextMenuResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.mode` | `string` | No | Context mode: `'auto'`, `'selection'`, `'playlist'`, `'nowPlaying'`, or `'handles'`. |
| `options.handles` | `unknown[]` | No | Handles used by handle-based context. |
| `options.path` | `string` | No | Optional track path. |
| `options.subsong` | `number` | No | Optional subsong index. |
| `options.locale` | `string` | No | Locale selector; defaults to `'auto'`. |
| `options.i18n` | `boolean` | No | Enables localized labels. |
| `options.withAvailability` | `boolean` | No | Includes availability metadata. |

Returns a `MenuGetContextMenuResponse` containing the recursive `items` menu tree and context metadata.

```javascript
const result = await fb.menu.getContextMenu({ mode: 'nowPlaying' });
```

Use `selection` for the selected tracks in the active playlist. `playlist` is
the playlist-level context and may contain only playlist-wide commands.

```javascript
const result = await fb.menu.getContextMenu({ mode: 'selection' });
```

### getMainMenu()

Signature: `fb.menu.getMainMenu(root?: string): Promise<MenuGetMainMenuResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `root` | `string` | No | Limits the returned tree, for example `'Main'` or `'View'`. |

Returns a `MenuGetMainMenuResponse`. The optional `items` field is a recursive `MenuItem[]` tree.

```javascript
const result = await fb.menu.getMainMenu('View');
```

### runContextCommand()

Signature: `fb.menu.runContextCommand(command: string): Promise<BaseResponse & { guid?: string; itemCount?: number }>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | Context-menu command path or name. |

Invokes only `menu.runContextCommand`. The response may include the command `guid` and affected `itemCount`.

```javascript
const result = await fb.menu.runContextCommand('Properties');
```

### runMainMenuCommand()

Signature: `fb.menu.runMainMenuCommand(command: string, options?: Omit<MenuRunMainMenuCommandParams, 'command'>): Promise<MenuRunMainMenuCommandResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | Command GUID, leaf name, or slash-separated path. |
| `options.subGuid` | `string` | No | Sub-command GUID of a dynamic child. |

Returns the `menu.runMainMenuCommand` response envelope and may include the resolved `guid`.

Prefer the GUID form: it is the only address that is stable across hosts. A
localized foobar2000 build reports localized command labels, so an English name
or path will not resolve there. Obtain a GUID from
`discovery.getMainMenuCommands()` or from the `guid` on a `menu.getMainMenu()`
leaf.

Failure is reported as `success: false` with a `code` — `MENU_ITEM_DISABLED`,
`MENU_MATCH_AMBIGUOUS` (see `candidates`), or `MENU_COMMAND_NOT_FOUND`.

```javascript
// Preferred: address by GUID.
const result = await fb.menu.runMainMenuCommand(
    '{11213A01-9F36-4E69-A1BB-7A72F418DE3A}',
);

// A dynamic child command needs its owning GUID plus subGuid.
await fb.menu.runMainMenuCommand('{41D98AF1-8C4F-4F0E-8B7A-1A4B0F7B1234}', {
    subGuid: '{A222D5A9-2903-AA8C-EEAE-4B9230558B55}',
});
```

### showNativePopup()

Signature: `fb.menu.showNativePopup(options?: MenuShowNativePopupParams): Promise<MenuShowNativePopupResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.mode` | `string` | No | Context mode; defaults to `'auto'`. Auto tries handles, now playing, playlist selection, then playlist context. |
| `options.handles` | `unknown[]` | No | Optional handle list. |
| `options.path` | `string` | No | Optional track path. |
| `options.subsong` | `number` | No | Optional subsong index. |

Returns the `menu.showNativePopup` response envelope. Native rendering is scheduled by the host.

```javascript
const result = await fb.menu.showNativePopup({ mode: 'selection' });
```

<!-- END AUTO-GENERATED SDK STUBS -->

## Self-drawn Popup Menus

`menu.show`, `menu.close`, and `menu.popup` render a context menu in WebView rather than through the native Win32 popup. The renderer supports submenus, keyboard navigation, and `::part()` theme hooks.

> **Boundary with tray zones:** public `menu.show` always uses direct item children under `#menu > .fb-item`; it does not produce `.fb-zone` containers. Zoned layout through `layoutMode: 'zones'` is an opt-in capability of `tray.setContextMenu` with `render: 'webview'`.

### fb.menu.popup(items, position?)

Preferred convenience API. It displays a self-drawn menu and waits for a selection. It resolves to the selected item's `id`, or `null` when the menu is dismissed by an outside click, Escape, or another close reason. Events are correlated by menu ID, so overlapping calls do not resolve one another.

```javascript
const id = await fb.menu.popup(
  [
    { id: 'play', label: 'Play' },
    { id: 'queue', label: 'Add to queue', checked: true },
    { type: 'separator' },
    { id: 'more', label: 'More', submenu: [
      { id: 'props', label: 'Properties' },
      { id: 'remove', label: 'Remove', enabled: false },
    ] },
  ],
  { x: 200, y: 150 },
);
if (id) console.log('selected', id);
```

Omit `position` or either coordinate to use the current cursor coordinate for that axis.

### fb.menu.show(items, position?)

Low-level API. It shows the menu and resolves with `MenuShowResponse`, including `{ success, menuId }` on success. User interaction arrives asynchronously through `menu:select` and `menu:dismiss`.

```javascript
const { menuId } = await fb.menu.show([{ id: 'a', label: 'A' }]);
fb.on('menu:select', (e) => { if (e.menuId === menuId) console.log(e.itemId); });
fb.on('menu:dismiss', (e) => { if (e.menuId === menuId) console.log('dismissed', e.reason); });
```

#### Caller, Security, and Resource Boundaries

- **Public entry points:** theme pages may call `menu.show`, `menu.close`, and `menu.popup`. Internal endpoints such as `menu.__select`, `menu.__valueChanged`, `menu.__dismiss`, `menu.__ready`, and `menu.__getMenuState` belong to the overlay window and are not part of the public SDK contract.
- **Authorization:** an unauthorized internal IPC call, including an incorrect caller or mismatched `menuId`, returns `INVALID_PARAMS` without changing menu state.
- **Validation limits:** validation is transactional before the overlay opens. Exceeding a limit returns `success: false`; `details` identifies `field`, `limit`, and `actual`.
  - At most 512 total items.
  - At most 8 recursive levels, with the root at level 1.
  - The shared renderer allows at most 64 options in one internally supplied `segmented` item. `segmented` is not part of the public `MenuPopupItem` type.
  - The shared renderer allows at most 256 KiB of SVG `content` per menu after oversized individual icons are discarded. The public `MenuPopupItem` type does not expose an SVG field.
- **Individual SVG limit:** within the shared renderer, an icon larger than 32 KiB is discarded without failing the entire menu.
- This API is a generic self-drawn menu, not the tray-zone configuration surface. Use `tray.setContextMenu` or `tray.appendMenuItems` for top, playback, and bottom tray zones.

### fb.menu.close(reason?)

Closes the active self-drawn menu. The facade omits the `reason` field when no reason is supplied.

```javascript
await fb.menu.close('api');
```

### MenuPopupItem

| Field | Type | Description |
| --- | --- | --- |
| `id` | `string` | Stable ID echoed through `menu:select`. |
| `label` | `string` | Visible row text; omitted for separators. |
| `type` | `'normal' \| 'separator'` | `'separator'` renders a divider. |
| `enabled` | `boolean` | Disabled rows cannot be selected; defaults to enabled. |
| `checked` | `boolean` | Displays a check mark. |
| `submenu` | `MenuPopupItem[]` | Nested child items rendered as a flyout. |

### Events

| Event | Payload | Timing |
| --- | --- | --- |
| `menu:show` | `MenuShowPayload` with `{ menuId }` | The menu becomes visible. |
| `menu:select` | `MenuSelectPayload` with `{ menuId, itemId }` | An item is selected; the menu then closes. |
| `menu:dismiss` | `MenuDismissPayload` with `{ menuId, reason }` | The menu closes. Host reasons include `outside`, `escape`, `select`, `api`, `timeout`, and `blur`. |

## Main and Context Menu Query/Execution

```javascript
await fb.menu.getMainMenu('View');
await fb.menu.getContextMenu({ mode: 'auto' });
await fb.menu.runMainMenuCommand('View/Console');
await fb.menu.runContextCommand('Properties');
await fb.menu.runContextCommandById(3, { mode: 'selection' });
await fb.menu.showNativePopup({ mode: 'selection' });
```

`runContextCommandById(id, options?)` is a separate facade method even though it is not listed in the generated stub block. `options` is `Omit<MenuRunContextCommandByIdParams, 'id'>`.
