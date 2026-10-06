# fb.menu Menu APIs

`fb.menu` queries and executes main or context menu commands and provides WebView-rendered popup menus.

## Standard command GUIDs {#standard-command-guids}

Use command identifiers when recognizing a host command; translated labels and menu positions are not identifiers. The following values are defined by foobar2000 SDK's `standard_commands` in `menu_helpers.h` and `guids.cpp`. They identify commands, not a promise that every host configuration exposes or enables them.

| Command | SDK identifier | GUID | Execute with |
| --- | --- | --- | --- |
| Track properties | `guid_context_file_properties` | `{6F441057-1D18-4A58-9AC4-8F409CDA7DFD}` | `runContextCommand` |
| Open containing directory | `guid_context_file_open_directory` | `{EFC1E9C8-EEEF-427A-8F42-E5781605846D}` | `runContextCommand` |
| Copy names | `guid_context_copy_names` | `{FFE18008-BCA2-4B29-AB88-8816B492C434}` | `runContextCommand` |
| Send to playlist | `guid_context_send_to_playlist` | `{44B8F02B-5408-4361-8240-18DEC881B95E}` | `runContextCommand` |
| Reload file information | `guid_context_reload_info` | `{8C3BA2CB-BC4D-4752-8282-C6F9AED75A78}` | `runContextCommand` |
| Reload changed file information | `guid_context_reload_info_if_changed` | `{BD045EA4-E5E9-4206-8FF9-12AD9F5DCDE1}` | `runContextCommand` |
| Preferences | `guid_main_preferences` | `{11213A01-9F36-4E69-A1BB-7A72F418DE3A}` | `runMainMenuCommand` |
| About | `guid_main_about` | `{EDA23441-5D38-4499-A22C-FE0CE0A987D9}` | `runMainMenuCommand` |

The send-to-playlist GUID identifies the SDK command. It does not identify a particular destination playlist or one of another component's dynamic children; the command itself determines its destination behavior.

Match the menu family as well as the GUID, and compare GUID text without regard to letter case. A dynamic leaf needs both its `guid` and `subGuid`; matching the parent GUID alone does not identify a rating value, converter preset or destination. Preserve the enumerated `enabled` and `hidden` state in a self-drawn menu. `executable: false` / `noStableIdentifier` describes the lack of a GUID address; it does not make a positional `commandId` stable or mean that every execution route is unavailable.

This table does not assign universal addresses to queue commands or third-party rating levels. For those operations, prefer the dedicated [queue](queue.md) and [rating](rating.md) APIs. When retaining host menu entries, do not infer their meaning from a translated name, child position or a shared parent GUID. A custom control replaces a menu action only when command identity, targets, parameters and effects agree.

### Targets and completion {#command-targets}

- `runContextCommand` resolves its target when called: the playing track wins; only when nothing is playing does it use the active playlist selection. It accepts neither `handles` nor `mode`. Building a menu for explicit handles does not bind a later GUID call to those handles.
- `runContextCommandById` accepts a target mode and handles, but rebuilds the menu. Even with the same inputs, dynamic entries can change and the same position can execute a different command. Refreshing the tree narrows the interval but does not provide an atomic identity check.
- To let the host show a native menu for supplied tracks, use `showNativePopup({ mode: 'handles', handles })`. This is distinct from executing a self-drawn selection by a stable address and explicit target in one call, which the current API does not provide.
- A successful command call, including `executionConfirmed: true`, is not a final result for a dialog or asynchronous third-party operation. It does not report whether the user cancelled a dialog, nor which objects eventually changed. A timeout is not proof that nothing executed, so do not automatically replay a command that can write or delete data.

## getContextMenu(options?)

Signature: `fb.menu.getContextMenu(options?: MenuGetContextMenuParams): Promise<MenuGetContextMenuResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.mode` | `string` | No | Context mode: `'auto'`, `'selection'`, `'playlist'`, `'nowPlaying'`, or `'handles'`. |
| `options.handles` | `JsonValue[]` | No | Tracks for `'handles'` and `'auto'`: paths, optionally ending in `\|subsong:N`, or `{ path, subsong }` objects. |
| `options.locale` | `string` | No | Locale selector; defaults to `'auto'`. |
| `options.i18n` | `boolean` | No | Enables localized labels. |
| `options.withAvailability` | `boolean` | No | Includes availability metadata. |

Returns a `MenuGetContextMenuResponse` containing the recursive `items` menu tree and context metadata. A command leaf's `source` is `contextmenu_dynamic` when it carries `subGuid`, otherwise `contextmenu_static`.

```javascript
const result = await fb.menu.getContextMenu({ mode: 'nowPlaying' });
```

Use `selection` for the selected tracks in the active playlist. `playlist` is
the playlist-level context and may contain only playlist-wide commands.

```javascript
const result = await fb.menu.getContextMenu({ mode: 'selection' });
```

## getMainMenu(root?, options?)

Signature: `fb.menu.getMainMenu(root?: string, options?: Omit<MenuGetMainMenuParams, 'root'>): Promise<MenuGetMainMenuResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `root` | `string` | No | Limits the returned tree, for example `'Main'` or `'View'`. |
| `options.locale` | `string` | No | Locale selector; defaults to `'auto'`. |
| `options.i18n` | `boolean` | No | Enables localized labels. |
| `options.withAvailability` | `boolean` | No | Includes availability metadata. |

Returns a `MenuGetMainMenuResponse`. Its `items` field is a recursive `MenuItem[]` tree. A command leaf's `source` follows its own address: `mainmenu_dynamic` when it carries `subGuid`, `mainmenu_static` otherwise. When the response reports `source: 'v1-hmenu'`, every leaf is `hmenu_fallback`. The flat fallback (`fallback: 'flat-mainmenu-commands'`, each item with `fallback: true`) keeps `flags` and has no `commandId`.

```javascript
const result = await fb.menu.getMainMenu('View');
```

## runContextCommand(command, options?)

Signature: `fb.menu.runContextCommand(command: string, options?: Omit<MenuRunContextCommandParams, 'command'>): Promise<MenuRunContextCommandResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | Context-menu command path, name, or GUID. |
| `options.subGuid` | `string` | No | Node GUID of a dynamically generated child. Without it the owning container is targeted, which runs nothing. |

Invokes only `menu.runContextCommand`. The command runs on the playing track, or on the active playlist selection when nothing is playing; with neither it fails with `NO_ACTIVE_ITEM`. A name that matches no command fails with `MENU_COMMAND_NOT_FOUND`, and a GUID that no command owns fails with `NOT_FOUND`.

The response may include the command `guid`, the affected `itemCount`, and `executionConfirmed` — `false` there means the command reached an entry point that returns nothing, so completion could not be observed.

```javascript
// Properties of the playing track, or the active playlist selection when stopped.
const result = await fb.menu.runContextCommand('{6F441057-1D18-4A58-9AC4-8F409CDA7DFD}');

// Pass only the dynamic leaf the user selected from getContextMenu().items.
async function runChosenDynamicContextCommand(node) {
    if (node.type !== 'command' || !node.guid || !node.subGuid) return;
    return fb.menu.runContextCommand(node.guid, { subGuid: node.subGuid });
}
```

## runContextCommandById(id, options?)

Signature: `fb.menu.runContextCommandById(id: number, options?: Omit<MenuRunContextCommandByIdParams, 'id'>): Promise<MenuRunContextCommandByIdResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `number` | Yes | The `commandId` of a context-menu node, as `getContextMenu()` reported it. |
| `options.mode` | `string` | No | The `mode` the menu was built with; defaults to `'auto'`. |
| `options.handles` | `JsonValue[]` | No | The `handles` the menu was built with. |

Runs the context-menu item whose `commandId` a `getContextMenu()` call reported. The host rebuilds the menu from `options.mode` and `options.handles`, so pass the values that call used. An id is valid only for the menu it came from, so do not store it; an id the rebuilt menu does not contain fails with `NOT_FOUND`.

A changed menu can also have a different command at that same id and execute it without failing. See [Targets and completion](#command-targets) before using this route for destructive actions.

```javascript
const opts = { mode: 'selection' };
const menu = await fb.menu.getContextMenu(opts);
if (menu.success === false) throw new Error(menu.error);
const node = menu.items.find((item) => item.type === 'command');
if (node && node.type === 'command' && node.commandId !== undefined) {
    const res = await fb.menu.runContextCommandById(node.commandId, opts);
    if (res.success === false) console.warn(res.code, res.error);
}
```

## runMainMenuCommand(command, options?)

Signature: `fb.menu.runMainMenuCommand(command: string, options?: Omit<MenuRunMainMenuCommandParams, 'command'>): Promise<MenuRunMainMenuCommandResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | Command GUID, leaf name, or slash-separated path. |
| `options.subGuid` | `string` | No | Sub-command GUID of a dynamic child. |

On success the response carries `guid`, the GUID of the command that ran; it is absent when the menu tree ran the command by name or path.

Prefer the GUID form: it is the only address that is stable across hosts. A
localized foobar2000 build reports localized command labels, so an English name
or path will not resolve there. Obtain a GUID from
`discovery.getMainMenuCommands()` or from the `guid` on a `menu.getMainMenu()`
leaf.

Failure is reported as `success: false` with a `code` — `MENU_ITEM_DISABLED`,
`MENU_MATCH_AMBIGUOUS` (see `candidates`), `MENU_COMMAND_NOT_FOUND`, or `NOT_FOUND`
when no command owns the GUID.

```javascript
// Preferences: this SDK-defined GUID does not depend on the host's language.
const result = await fb.menu.runMainMenuCommand(
    '{11213A01-9F36-4E69-A1BB-7A72F418DE3A}',
);

// Pass only the dynamic leaf the user selected from getMainMenu().items.
async function runChosenDynamicMainCommand(node) {
    if (node.type !== 'command' || !node.guid || !node.subGuid) return;
    return fb.menu.runMainMenuCommand(node.guid, { subGuid: node.subGuid });
}
```

## showNativePopup(options?)

Signature: `fb.menu.showNativePopup(options?: MenuShowNativePopupParams): Promise<MenuShowNativePopupResponse>`

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `options.mode` | `string` | No | Context mode; defaults to `'auto'`. Auto tries handles, now playing, playlist selection, then playlist context. |
| `options.handles` | `JsonValue[]` | No | Tracks for `'handles'` and `'auto'`, as for `getContextMenu()`. |
| `options.x`, `options.y` | `number` | No | Accepted for compatibility and ignored: the menu opens at the mouse pointer. |

The host opens the menu at the mouse pointer just after the call returns.

```javascript
const result = await fb.menu.showNativePopup({ mode: 'selection' });
```

## Self-drawn Popup Menus

`menu.show`, `menu.close`, and `menu.popup` render a context menu in WebView rather than through the native Win32 popup. The renderer supports submenus, keyboard navigation, inline rich controls, and full CSS restyling through `options.css`.

> **Boundary with tray zones:** public `menu.show` always uses direct item children under `#menu > .fb-item`; it does not produce `.fb-zone` containers. Zoned layout through `layoutMode: 'zones'` is an opt-in capability of `tray.setContextMenu` with `render: 'webview'`.

### fb.menu.popup(items, position?, options?)

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

Omit `position` or either coordinate to use the current cursor coordinate for that axis. The third argument is the presentation config described in [Presentation options](#presentation-options); for a context menu, prefer `windowModel: 'contentSized'`.

A rich control reports through `menu:valueChanged` without closing the menu, so this promise stays pending until an ordinary row is chosen or the menu is dismissed. Subscribe to that event separately when the menu contains rating, slider, or segmented rows.

### fb.menu.show(items, position?, options?)

Low-level API. It shows the menu and resolves with `MenuShowResponse`, including `{ success, menuId }` on success. User interaction arrives asynchronously through `menu:select` and `menu:dismiss`.

```javascript
const res = await fb.menu.show([{ id: 'a', label: 'A' }]);
if (res.success === false) throw new Error(res.error);
const { menuId } = res;
fb.on('menu:select', (e) => { if (e.menuId === menuId) console.log(e.itemId); });
fb.on('menu:dismiss', (e) => { if (e.menuId === menuId) console.log('dismissed', e.reason); });
```

#### Caller, Security, and Resource Boundaries

- **Public entry points:** theme pages may call `menu.show`, `menu.close`, and `menu.popup`. Internal endpoints such as `menu.__select`, `menu.__valueChanged`, `menu.__dismiss`, `menu.__ready`, and `menu.__getMenuState` belong to the overlay window and are not part of the public SDK contract.
- **Authorization:** an unauthorized internal IPC call, including an incorrect caller or mismatched `menuId`, returns `INVALID_PARAMS` without changing menu state.
- **Validation limits:** validation is transactional before the overlay opens. Exceeding a limit returns `success: false`; `details` identifies `field`, `limit`, and `actual`.
  - At most 512 total items.
  - At most 8 recursive levels, with the root at level 1.
  - At most 64 options in one `segmented` item.
  - At most 256 KiB of SVG `content` per menu, summed after oversized individual icons are discarded.
  - At most 256 KiB of `options.css`, reported as a breach of the `css` field.
- **Individual SVG limit:** an icon larger than 32 KiB is discarded silently, without failing the rest of the menu.
- This API is a generic self-drawn menu, not the tray-zone configuration surface. Use `tray.setContextMenu` or `tray.appendMenuItems` for top, playback, and bottom tray zones.

### Presentation options {#presentation-options}

Both methods take an optional third argument of type `MenuPopupOptions`. Every field is applied per call, so a page can follow its own theme state on each right-click; omitted fields keep their defaults.

| Field | Type | Default | Description |
| --- | --- | --- | --- |
| `windowModel` | `'fullscreen' \| 'contentSized'` | `'fullscreen'` | `'contentSized'` draws the root and its first-level submenu in separate compact windows measured to their content, so each panel carries the real DWM backdrop material across its own surface plus the system window shadow. Recommended for context menus. `'fullscreen'` is the compatibility default: one fullscreen overlay window hosting the menu DOM. |
| `css` | `string` | — | Frontend style takeover, at most 256 KiB, injected into the overlay's dedicated style layer and applied on every open. |
| `cssReplace` | `boolean` | `false` | `true` switches `css` from override/append to replace mode: the built-in styles are disabled and only your CSS plus the protected structural layer remain, so the entire look, including the entry animation, is yours. |
| `backdrop` | `'acrylic' \| 'mica' \| 'mica-alt' \| 'none'` | `'acrylic'` | DWM system backdrop for the menu window. `'acrylic'` is the transient-surface material and the correct default for a pop-up; `'mica'` and `'mica-alt'` are designed as main-window backgrounds and may look off on a transient menu. |
| `backdropDarkMode` | `boolean` | `true` | Dark tint for the backdrop. Pass `false` to follow a light theme. |
| `closeAnimationMs` | `number` | `0` | Exit (fade-out) duration in milliseconds, clamped to `0..1000`. `0` hides immediately. |

#### Full example

```javascript
document.addEventListener('contextmenu', async (e) => {
  e.preventDefault();
  const id = await fb.menu.popup(
    [
      { id: 'play', label: 'Play' },
      { id: 'queue', label: 'Add to queue' },
      { type: 'separator' },
      { id: 'rating', type: 'rating', label: 'Rating', value: 3 },
      { id: 'volume', type: 'slider', label: 'Volume', value: 60, min: 0, max: 100 },
      { type: 'separator' },
      { id: 'props', label: 'Properties' },
    ],
    undefined,
    {
      windowModel: 'contentSized',
      backdrop: 'acrylic',
      css: `
        .fb-menu { background: rgba(32, 32, 32, 0.82); border-radius: 8px; padding: 4px; }
        .fb-item { color: #f2f2f2; border-radius: 4px; }
        .fb-item.active { background: rgba(255, 255, 255, 0.08); }
      `,
    },
  );
  if (id) console.log('selected', id);
});

fb.on('menu:valueChanged', (e) => {
  console.log('value changed', e.itemId, e.value);
});
```

Passing `undefined` for `position` anchors the menu at the cursor, which is what a `contextmenu` handler usually wants.

#### Styling guide

The overlay is an isolated top-level document, so a host page's `::part()` selectors cannot reach it. The supported styling contract is the stable class names: `.fb-menu`, `.fb-item` (with `.nrm`, `.disabled`, `.active`, `.checked`, `.has-sub`), `.fb-item-ico`, `.fb-arrow`, `.fb-sep`, the now-playing `.fb-np*`, rating `.fb-rating*` and `.fb-star`, slider `.fb-slider*`, and segmented `.fb-seg*`. In the default override mode your rules win by source order or `!important`. A small protected structural layer (`#viewport`, menu box-sizing, fixed positioning, overflow, and the hidden-state fallback) is always force-applied last.

For a translucent menu, keep the background alpha around 0.75 to 0.9. That preserves text contrast, and Windows 11's own system menus are similarly restrained about how much they let through.

Animation and material are a trade-off, because the DWM backdrop is a window-level, all-or-nothing effect: it appears and disappears the instant the window is shown or hidden and cannot fade with CSS animations. `closeAnimationMs` therefore animates the web content only, so with `acrylic` or `mica` enabled the backdrop pops while the content fades. For a fully smooth fade, set `backdrop: 'none'` and give `.fb-menu` a translucent CSS background instead, accepting that CSS translucency has no real blur. On close the renderer toggles the root menu's class from `#menu.in` to `#menu.out`, and the built-in `#menu.out` rule can be overridden through `css`; the `replaced` and internal timeout close paths always hide immediately.

#### Platform requirements

Acrylic needs Windows 11 22H2 or newer. On Windows 10 the backdrop degrades to whatever material the system supports, so a theme that must look identical everywhere should use `backdrop: 'none'` with a translucent CSS background.

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
| `type` | `'normal' \| 'separator' \| 'nowplaying' \| 'rating' \| 'slider' \| 'segmented'` | `'separator'` renders a divider; the rich kinds render an inline control. |
| `enabled` | `boolean` | Disabled rows cannot be selected; defaults to enabled. |
| `checked` | `boolean` | Displays a check mark. |
| `iconSvg` | `{ viewBox, content }` | Inline monochrome SVG drawn before the label. `content` is the SVG inner markup. |
| `cover` | `string` | `'nowplaying'` album art: a data URL, an `http(s)` URL, or raw base64 decoded as JPEG. |
| `title` | `string` | `'nowplaying'` primary line; falls back to `label`. |
| `subtitle` | `string` | `'nowplaying'` secondary line. |
| `value` | `number` | Current value: `'rating'` stars `0..5`, `'slider'` an integer in `[min, max]`, `'segmented'` the zero-based selected index. |
| `min` / `max` | `number` | `'slider'` range; defaults to `0` and `100`. |
| `orientation` | `'horizontal' \| 'vertical'` | `'slider'` axis; defaults to horizontal. |
| `segments` | `{ label?, iconSvg?, enabled? }[]` | `'segmented'` options, rendered as one row of mutually exclusive choices. |
| `submenu` | `MenuPopupItem[]` | Nested child items rendered as a flyout. |

#### Rich items

`'rating'`, `'slider'`, and `'segmented'` are value controls: changing one reports through `menu:valueChanged` as `{ menuId, itemId, value }` and **keeps the menu open**, so the page decides what a value means and can update foobar2000 while the menu stays on screen. A `'nowplaying'` card is an ordinary selection instead: it reports through `menu:select` and closes the menu, like any normal row.

Besides clicking and the arrow keys, the mouse wheel over a value-control row steps it (scroll up = increase): one step per wheel event, one star for `'rating'`, a twentieth of the range for `'slider'`, and the next enabled segment for `'segmented'`. The row must be enabled, and a slider whose `min` equals its `max` never moves. While a *different* row is in editor mode the wheel is ignored and the menu scrolls instead.

An `iconSvg` is parsed with `DOMParser` and only an allowlisted set of shape elements and attributes is cloned into the live document, so raw markup injection is not possible. Illegal or oversized icons are dropped silently and the row is drawn without one.

### Events

| Event | Payload | Timing |
| --- | --- | --- |
| `menu:select` | `MenuSelectPayload` with `{ menuId, itemId }` | An item is selected; the menu then closes. |
| `menu:valueChanged` | `{ menuId, itemId, value }` | A rating, slider, or segmented control changes value. The menu stays open. |
| `menu:dismiss` | `MenuDismissPayload` with `{ menuId, reason }` | The menu closes. Host reasons include `outside`, `escape`, `select`, `api`, `timeout`, and `blur`. |

These events go to the page that called `menu.show`, so `fb.menu.popup()` resolves in a popup or panel page as it does in the main window. When that page is gone by the time the menu reports, they go to a page under the same top-level window, and then to the main window's page. A menu closed by another page's `menu.show` or `menu.close` still reports its `menu:dismiss` to the page that opened it.

## Main and Context Menu Query/Execution

```javascript
await fb.menu.getMainMenu('View');
await fb.menu.getContextMenu({ mode: 'auto' });
await fb.menu.runMainMenuCommand('View/Console');
await fb.menu.runContextCommand('Properties');
await fb.menu.runContextCommandById(3, { mode: 'selection' });
await fb.menu.showNativePopup({ mode: 'selection' });
```

`runContextCommandById(id, options?)` runs the context-menu item whose `commandId` `getContextMenu` reported; see [runContextCommandById()](#runcontextcommandbyid-id-options) for the parameters and failure code.
