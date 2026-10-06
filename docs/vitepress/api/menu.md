# Menu API

Methods of the `menu` namespace.

See [Standard command GUIDs](../sdk/menu.md#standard-command-guids) for SDK-defined addresses and [Targets and completion](../sdk/menu.md#command-targets) for the limits of each execution route. The addresses are the same for SDK calls and `fb2k.invoke`.

## menu

### menu.close

<!-- api-schema:begin menu.close -->
Close the self-drawn menu if one is open; its `menu:dismiss` carries `reason`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `reason` | `string` | No | Reason reported in `menu:dismiss`. Default: `"api"`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('menu.close', { reason: 'api' });
```

### menu.getContextMenu

<!-- api-schema:begin menu.getContextMenu -->
Get the context menu for the tracks `mode` selects, as a tree.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | Tracks to build the menu for: `handles` uses `handles`, `playlist` the active playlist, `nowPlaying` the playing track and `selection` the active playlist selection. Any other value, `auto` included, takes the first of `handles`, the playing track, the selection and the active playlist that is available. Given `handles` of which none is usable fail with `INVALID_PARAMS` in every mode, `auto` included, rather than falling back to other tracks; `nowPlaying` with nothing playing and `selection` with nothing selected fail with `NO_ACTIVE_ITEM`. Default: `"auto"`. |
| `locale` | `string` | No | Locale for `displayLabel`: `auto` keeps the host's labels; a `zh` or `en` tag translates common labels. Default: `"auto"`. |
| `i18n` | `boolean` | No | `false` turns the label translation off. Default: `true`. |
| `withAvailability` | `boolean` | No | Add command counts to submenus. Default: `true`. |
| `handles` | `any[]` | No | Tracks for `mode: "handles"` and `auto`: each a path, optionally ending in `\|subsong:N`, or an object `{ path, subsong }`. Paths the security policy refuses and entries of other types are skipped. Each path is checked on disk, so prefer `selection`, `playlist` or `nowPlaying` for tracks the host already lists. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `mode` | `"handles" \| "playlist" \| "nowPlaying" \| "selection"` | The mode used; `auto` resolves to one of the others. |
| `locale` | `string` | The `locale` used. |
| `i18n` | `boolean` | The `i18n` used. |
| `withAvailability` | `boolean` | The `withAvailability` used. |
| `items` | `MenuTreeNode[]` | The top-level nodes. |
| `items[].type` | `"separator" \| "submenu" \| "command"` | Node kind. |
| `items[].label` | `string` | Label as the host reports it (localized on a translated build). |
| `items[].displayLabel` | `string` | `label` translated for `locale`; the same as `label` when translation is off or has no entry for it. |
| `items[].path` | `string` | Slash-separated labels from the top of the menu. |
| `items[].displayPath` | `string` | `path` made of display labels. |
| `items[].flags` | `integer` | Raw display flags. Their bits depend on where the node came from: SDK menu flags from the menu tree and the flat list, Win32 menu state from the Win32 menus. Absent on a submenu of the Win32 menus. |
| `items[].children` | `MenuTreeNode[]` | The children, in menu order; only on a submenu. |
| `items[].availability` | `MenuAvailability` | Command counts below this submenu; only with `withAvailability`, and not on submenus nested inside the Win32 menus. |
| `items[].availability.totalCommands` | `integer` | Commands anywhere below the submenu. |
| `items[].availability.availableCommands` | `integer` | Commands that are enabled. |
| `items[].availability.disabledCommands` | `integer` | Commands that are disabled. |
| `items[].availability.allAvailable` | `boolean` | Whether every command is enabled; also true when there are none. |
| `items[].enabled` | `boolean` | Whether the command can run; only on a command. |
| `items[].available` | `boolean` | The same as `enabled`; only on a command. |
| `items[].checked` | `boolean` | Whether the command shows a check mark (a radio mark counts too); only on a command. |
| `items[].radioChecked` | `boolean` | Whether the check mark is a radio mark; only on a command. |
| `items[].hidden` | `boolean` | Whether the host would not draw the command; only on a command. |
| `items[].commandId` | `integer` | Menu item id. For a context-menu command, `menu.runContextCommandById` runs it. The id is valid only for the menu it came from, so do not store it. Absent on the flat list. |
| `items[].guid` | `string` | Command GUID, the stable address to pass to `menu.runMainMenuCommand` or `menu.runContextCommand`; absent when it could not be resolved. |
| `items[].subGuid` | `string` | GUID of a dynamic child command, passed together with `guid`. |
| `items[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | Where the command came from: the dynamic value when it carries `subGuid`, the static value otherwise, and `hmenu_fallback` for every command of the Win32 menus. |
| `items[].executable` | `boolean` | Whether the command has a `guid` to run it by. |
| `items[].unaddressableReason` | `"noStableIdentifier"` | Why the command cannot be run; present when `executable` is false. |
| `items[].fallback` | `boolean` | `true` on a command of the flat list. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('menu.getContextMenu', { mode: 'selection' });
if (res.success === false) throw new Error(res.error);
const { items } = res;
```

### menu.getMainMenu

<!-- api-schema:begin menu.getMainMenu -->
Get the main menu as a tree. The host builds it from its menu tree, falls back to the Win32 menus (`source: "v1-hmenu"`), and as a last resort returns a flat command list (`fallback: "flat-mainmenu-commands"`); fails with `OPERATION_FAILED` when none can be built.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `root` | `string` | No | Label or slash-separated path of a submenu to return instead of the whole menu. When nothing matches, the whole menu is returned with `rootMatched: false`. Default: `""`. |
| `locale` | `string` | No | Locale for `displayLabel`: `auto` keeps the host's labels; a `zh` or `en` tag translates common labels. Default: `"auto"`. |
| `i18n` | `boolean` | No | `false` turns the label translation off. Default: `true`. |
| `withAvailability` | `boolean` | No | Add command counts to submenus. Default: `true`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `root` | `string` | Label of the submenu returned; empty for the whole menu. |
| `requestedRoot` | `string` | The `root` that was asked for. |
| `rootMatched` | `boolean` | Whether `root` was found; true when none was asked for, except on the flat list, which is always false. |
| `locale` | `string` | The `locale` used. |
| `i18n` | `boolean` | The `i18n` used. |
| `withAvailability` | `boolean` | The `withAvailability` used. |
| `items` | `MenuTreeNode[]` | The top-level nodes. |
| `items[].type` | `"separator" \| "submenu" \| "command"` | Node kind. |
| `items[].label` | `string` | Label as the host reports it (localized on a translated build). |
| `items[].displayLabel` | `string` | `label` translated for `locale`; the same as `label` when translation is off or has no entry for it. |
| `items[].path` | `string` | Slash-separated labels from the top of the menu. |
| `items[].displayPath` | `string` | `path` made of display labels. |
| `items[].flags` | `integer` | Raw display flags. Their bits depend on where the node came from: SDK menu flags from the menu tree and the flat list, Win32 menu state from the Win32 menus. Absent on a submenu of the Win32 menus. |
| `items[].children` | `MenuTreeNode[]` | The children, in menu order; only on a submenu. |
| `items[].availability` | `MenuAvailability` | Command counts below this submenu; only with `withAvailability`, and not on submenus nested inside the Win32 menus. |
| `items[].availability.totalCommands` | `integer` | Commands anywhere below the submenu. |
| `items[].availability.availableCommands` | `integer` | Commands that are enabled. |
| `items[].availability.disabledCommands` | `integer` | Commands that are disabled. |
| `items[].availability.allAvailable` | `boolean` | Whether every command is enabled; also true when there are none. |
| `items[].enabled` | `boolean` | Whether the command can run; only on a command. |
| `items[].available` | `boolean` | The same as `enabled`; only on a command. |
| `items[].checked` | `boolean` | Whether the command shows a check mark (a radio mark counts too); only on a command. |
| `items[].radioChecked` | `boolean` | Whether the check mark is a radio mark; only on a command. |
| `items[].hidden` | `boolean` | Whether the host would not draw the command; only on a command. |
| `items[].commandId` | `integer` | Menu item id. For a context-menu command, `menu.runContextCommandById` runs it. The id is valid only for the menu it came from, so do not store it. Absent on the flat list. |
| `items[].guid` | `string` | Command GUID, the stable address to pass to `menu.runMainMenuCommand` or `menu.runContextCommand`; absent when it could not be resolved. |
| `items[].subGuid` | `string` | GUID of a dynamic child command, passed together with `guid`. |
| `items[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | Where the command came from: the dynamic value when it carries `subGuid`, the static value otherwise, and `hmenu_fallback` for every command of the Win32 menus. |
| `items[].executable` | `boolean` | Whether the command has a `guid` to run it by. |
| `items[].unaddressableReason` | `"noStableIdentifier"` | Why the command cannot be run; present when `executable` is false. |
| `items[].fallback` | `boolean` | `true` on a command of the flat list. |
| `source` | `"v1-hmenu"` | `v1-hmenu` when the tree was built from the Win32 menus. |
| `fallback` | `"flat-mainmenu-commands"` | `flat-mainmenu-commands` when only the flat command list could be built. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Each command leaf carries a `source`. On the menu tree a leaf with `subGuid` reports `mainmenu_dynamic` and one without reports `mainmenu_static`; when the host had to walk the Win32 menu instead, the response carries `source: "v1-hmenu"` and every leaf reports `hmenu_fallback`. The flat fallback (`fallback: "flat-mainmenu-commands"`) marks each item with `fallback: true`, keeps `flags` and has no `commandId`.

```js
const res = await fb2k.invoke('menu.getMainMenu', { root: 'View' });
if (res.success === false) throw new Error(res.error);
const { items } = res;
```

### menu.runContextCommand

<!-- api-schema:begin menu.runContextCommand -->
Run a context-menu command on the playing track, or on the active playlist selection when nothing is playing; fails with `NO_ACTIVE_ITEM` when there is neither. A name that matches no command fails with `MENU_COMMAND_NOT_FOUND`, a GUID that no command owns with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | The command: a GUID or a command name. Must not be empty. |
| `subGuid` | `string` | No | GUID of a dynamic child such as a rating value or a converter preset; without it the owning container is targeted, which does nothing. Empty is the same as absent. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `guid` | `string` | GUID of the command that ran; absent when it was reached by name and has no GUID. |
| `itemCount` | `integer` | How many tracks the command ran on. |
| `executionConfirmed` | `boolean` | `false` when the command has no GUID and was handed to a host entry point that reports nothing back, so whether it ran is unknown. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`executionConfirmed: false` means the command was handed to the host through an
entry point that returns nothing, so completion could not be observed. It only
occurs for a registration that exposes no stable GUID.

```js
// Track properties; the target is resolved when the call runs.
await fb2k.invoke('menu.runContextCommand', {
    command: '{6F441057-1D18-4A58-9AC4-8F409CDA7DFD}',
});

// node is the dynamic leaf the user chose from menu.getContextMenu().
async function runChosenDynamicContextCommand(node) {
    if (node.type !== 'command' || !node.guid || !node.subGuid) return;
    return fb2k.invoke('menu.runContextCommand', {
        command: node.guid,
        subGuid: node.subGuid,
    });
}
```

### menu.runContextCommandById

<!-- api-schema:begin menu.runContextCommandById -->
Run the context-menu item whose `commandId` a `menu.getContextMenu` call reported. The menu is rebuilt from `mode` and `handles`, so pass the `handles` that call used and the `mode` it reported, not `auto` again. An id is a position in the rebuilt menu, not a name: an id past its end fails with `NOT_FOUND`, but when the tracks, the selection, the playing track or the installed components changed since, the same id can run another item without failing.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `id` | `integer` | Yes | The `commandId` of a context-menu node. Between `0` and `4294967295` inclusive. |
| `mode` | `string` | No | The `mode` the menu was built with; see `menu.getContextMenu`. Default: `"auto"`. |
| `handles` | `any[]` | No | The `handles` the menu was built with; see `menu.getContextMenu`. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('menu.runContextCommandById', { id: 3 });
```

### menu.runMainMenuCommand

<!-- api-schema:begin menu.runMainMenuCommand -->
Run a main-menu command addressed by GUID, leaf name or slash-separated path. A disabled command fails with `MENU_ITEM_DISABLED`; a name that matches several commands fails with `MENU_MATCH_AMBIGUOUS` and lists them in `candidates`; a name that matches none fails with `MENU_COMMAND_NOT_FOUND`, and a GUID that no command owns with `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `command` | `string` | Yes | The command: a GUID such as `{11213A01-...}`, a leaf name, or a slash-separated path. Prefer the GUID, the one form that does not depend on the host's language. Must not be empty. |
| `subGuid` | `string` | No | GUID of a dynamic child of the command; only with the GUID form. Empty is the same as absent. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `guid` | `string` | GUID of the command that ran; absent when the menu tree ran it by name or path. |
| `dynamic` | `boolean` | Whether it was a dynamic child command. |
| `subGuid` | `string` | GUID of the dynamic child that ran. |
| `source` | `"v2-tree" \| "index"` | How a name or path was resolved: `v2-tree` by the menu tree, `index` by the command list. Absent for the GUID form. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`command` accepts a GUID, a leaf command name, or a slash-separated path. Prefer
the GUID: it is the only form that is stable across hosts. A localized
foobar2000 build reports localized command labels, so an English name or path
will not resolve there.

Name and path forms are matched exactly per segment. When a name matches more
than one command the call fails with `MENU_MATCH_AMBIGUOUS` and lists the
candidates, rather than picking one.

Failure is always reported as `success: false` with a `code`:

| `code` | Meaning |
| --- | --- |
| `MENU_ITEM_DISABLED` | Command exists but is currently greyed out. |
| `MENU_MATCH_AMBIGUOUS` | Name matched several commands; see `candidates`. |
| `MENU_COMMAND_NOT_FOUND` | No command matched. |
| `NOT_FOUND` | No command owns the GUID. |

```js
// Preferences: an SDK-defined GUID independent of the host's language.
const result = await fb2k.invoke('menu.runMainMenuCommand', {
    command: '{11213A01-9F36-4E69-A1BB-7A72F418DE3A}',
});

// node is the dynamic leaf the user chose from menu.getMainMenu().
async function runChosenDynamicMainCommand(node) {
    if (node.type !== 'command' || !node.guid || !node.subGuid) return;
    return fb2k.invoke('menu.runMainMenuCommand', {
        command: node.guid,
        subGuid: node.subGuid,
    });
}
```

### menu.show

<!-- api-schema:begin menu.show -->
Show a self-drawn menu. The choice arrives as `menu:select` and a dismissal as `menu:dismiss`; changing a rating, slider or segmented row reports `menu:valueChanged` and leaves the menu open. Too many rows, too deep nesting, too many segments or too much style text fail with `INVALID_PARAMS` and `details`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `items` | `any[]` | No | Menu rows; the SDK's `MenuPopupItem` describes their fields. A row icon over 32 KiB is dropped. |
| `x` | `integer` | No | Screen x of the anchor in physical pixels; absent or negative uses the mouse pointer. |
| `y` | `integer` | No | Screen y of the anchor in physical pixels; absent or negative uses the mouse pointer. |
| `windowModel` | `string` | No | `contentSized` draws each panel in a window sized to its content; any other value uses one full-screen overlay window. |
| `css` | `string` | No | Style sheet applied to the menu on top of the built-in styles, at most 256 KiB. |
| `cssReplace` | `boolean` | No | Replace the built-in styles with `css` instead of adding to them. |
| `backdrop` | `string` | No | Window backdrop: `acrylic` (the default), `mica`, `mica-alt` or `none`. Other values keep the default. |
| `backdropDarkMode` | `boolean` | No | Dark tint for the backdrop; the default is `true`. |
| `closeAnimationMs` | `integer` | No | Length of the closing animation in milliseconds, clamped to 0–1000; the default `0` closes at once. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `menuId` | `string` | Id of the menu; `menu:select`, `menu:dismiss` and `menu:valueChanged` carry it. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('menu.show', {
    items: [
        { id: 'play', label: 'Play' },
        { type: 'separator' },
        { id: 'props', label: 'Properties' },
    ],
});
if (res.success === false) throw new Error(res.error);
const { menuId } = res;
```

### menu.showNativePopup

<!-- api-schema:begin menu.showNativePopup -->
Show the host's native context menu at the mouse pointer for the tracks `mode` selects. The menu opens just after the call returns.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `mode` | `string` | No | Tracks to build the menu for; see `menu.getContextMenu`. Default: `"auto"`. |
| `handles` | `any[]` | No | Tracks for `mode: "handles"` and `auto`; see `menu.getContextMenu`. |
| `x` | `integer` | No | Accepted for compatibility and ignored: the menu always opens at the mouse pointer. |
| `y` | `integer` | No | Accepted for compatibility and ignored: the menu always opens at the mouse pointer. |

**Returns**

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('menu.showNativePopup', { mode: 'nowPlaying' });
```

## Owner-family behavior and limits

- `menu.getContextMenu`, `menu.runContextCommandById`, and `menu.showNativePopup` use `mode` to select `handles`, `nowPlaying`, `selection`, or `playlist` context. `auto` tries those sources in that order, ending with playlist context. In `handles` mode, every path is media-access validated before a handle is created. `menu.showNativePopup` uses screen cursor coordinates and returns before the native menu is displayed.
- `menu.show` opens the self-drawn overlay after resource validation. `menu.close` only closes the active overlay; `menu.__*` endpoints are internal and are not public APIs.

## Contract supplements

The sections below close public-contract findings from the strict parameter audit without replacing existing explanations.

<!-- phase3-supplement:menu.getContextMenu -->
### Contract supplement: `menu.getContextMenu`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `handles` | `array` | No | `[]` | Track paths, or `{ path, subsong }` objects, used as the menu context. |
| `i18n` | `boolean` | No | `true` | Translates item labels into `displayLabel`. |
| `locale` | `string` | No | `auto` | Translation locale; `auto` keeps the host's own labels. |
| `mode` | `string` | No | `auto` | One of `auto`, `selection`, `playlist`, `nowPlaying`, or `handles`. |
| `withAvailability` | `boolean` | No | `true` | Includes per-submenu availability counters. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |
| `i18n` | `json` | No |
| `items` | `json` | No |
| `locale` | `json` | No |
| `mode` | `json` | No |
| `withAvailability` | `json` | No |

Semantics: omitted optional parameters use handler defaults. `mode: 'handles'` additionally requires a non-empty `handles` array.

```js
const res = await fb2k.invoke('menu.getContextMenu', { mode: 'nowPlaying' });
if (res.success === false) throw new Error(res.error);
const { items } = res;
```

<!-- phase3-supplement:menu.runContextCommandById -->
### Contract supplement: `menu.runContextCommandById`

| Parameter | Type | Required | Default | Description |
| --- | --- | --- | --- | --- |
| `id` | `integer` | Yes | — | Command id from the built context menu. A missing id fails with `id is required`, a non-integer one with `id must be an integer` and a negative one with `id is out of range`, all as `INVALID_PARAMS`. |
| `mode` | `string` | No | `auto` | One of `selection`, `playlist`, `nowPlaying`, `handles`, or `auto`. |
| `handles` | `array` | No | `[]` | Track paths, or `{ path, subsong }` objects, used as the menu context. |

#### Return fields

| Field | Type | Optional |
| --- | --- | --- |
| `error` | `string` | Yes |
| `success` | `boolean` | No |

Semantics: omitted optional parameters use handler defaults. `mode: 'handles'` additionally requires a non-empty `handles` array.

```js
await fb2k.invoke('menu.runContextCommandById', { id: 3, mode: 'selection' });
```
