# Discovery API

Methods of the `discovery` namespace.

## discovery

### discovery.executeContextMenuByPath

<!-- api-schema:begin discovery.executeContextMenuByPath -->
Run a context-menu command addressed by its slash-separated label path, such as `Playback Statistics/Rating/5`. Each segment must match one label exactly after normalization (mnemonic `&`, trailing ellipsis, accelerator text and ASCII case are ignored). A path matching several commands is refused with `INVALID_PARAMS` and the candidates listed; one matching none is `NOT_FOUND`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Slash-separated label path of the command, such as `Playback Statistics/Rating/5`. Must not be empty. |
| `trackPath` | `string` | No | Run against this track instead of the current target. A `\|subsong:N` suffix selects a subsong; without one the first subsong of the file is used. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `path` | `string` | The path as given. |
| `foundName` | `string` | Full name of the command that ran. |
| `match` | `"unique"` | Always `unique` on success: exactly one command matched. |
| `candidateCount` | `integer` | Number of commands the path matched; `1` on success. |
| `itemCount` | `integer` | How many tracks the command ran against. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
await fb2k.invoke('discovery.executeContextMenuByPath', { path: 'Playback Statistics/Rating/5' });
```

Matching is never a substring test, so `Rating/1` cannot resolve to `Rating/10`. A path that matches several commands is refused instead of guessed, because duplicated labels are common in real hosts: the failure carries `match: "ambiguous"`, `candidateCount` and a `candidates` list of full names. Use it to refine the path, or address the command by GUID through `discovery.executeContextMenuCommand`, which is the only stable identifier. A path that matches nothing fails with `NOT_FOUND` and `match: "notFound"`.

### discovery.executeContextMenuCommand

<!-- api-schema:begin discovery.executeContextMenuCommand -->
Run a context-menu command by GUID against the playing track, or the active playlist selection when nothing is playing. A command the host would never draw (`FORCE_OFF`) is refused with `NOT_SUPPORTED` unless `force` is set; no target at all is `NO_ACTIVE_ITEM`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Command GUID in `{...}` form, as `discovery.getContextMenuCommands` reports it. Must not be empty. |
| `force` | `boolean` | No | Dispatch even a command the host would never draw. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `guid` | `string` | The GUID that was executed. |
| `name` | `string` | Label of the command. |
| `hidden` | `boolean` | Whether the host would never draw the command (`FORCE_OFF`). |
| `resolved` | `boolean` | Whether a registered item owns the GUID. |
| `force` | `boolean` | The `force` that applied. |
| `itemCount` | `integer` | How many tracks the command ran against. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

A `FORCE_OFF` command is refused rather than dispatched: the SDK treats that state as "keyboard-shortcut list only", so the host never draws it and running it would perform something the user could not have clicked. The refusal fails with `NOT_SUPPORTED` and carries `hidden: true`, `name`, `resolved` and `force`. `DEFAULT_OFF` commands (hidden unless Shift is held) remain reachable and are never refused. A GUID no registered item owns fails with `NOT_FOUND` and `resolved: false`.

`hidden` and `resolved` come back on the refusal and the success path alike, so "was not refused" is distinguishable from "this build does not report the field".

```js
const res = await fb2k.invoke('discovery.getContextMenuCommands');
if (res.success === false) throw new Error(res.error);
const { commands } = res;
const rate5 = commands.find((c) => c.name === 'Rating/5');
await fb2k.invoke('discovery.executeContextMenuCommand', { guid: rate5.guid });
```

`name` is the label the host reports, so it is localized on a translated build. Cache the `guid` rather than repeating the label match.

### discovery.executeMainMenuCommand

<!-- api-schema:begin discovery.executeMainMenuCommand -->
Run a main-menu command by GUID. A child of a dynamic submenu is addressed by its parent's `guid` plus its own `subGuid`. Fails with `NOT_FOUND` when no command owns the GUID.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Command GUID in `{...}` form. For a dynamic child, its parent command's GUID. Must not be empty. |
| `subGuid` | `string` | No | `subGuid` of a dynamic child, as `discovery.getMainMenuCommands` reports it. Empty means absent. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `guid` | `string` | The GUID that was executed. |
| `subGuid` | `string` | The `subGuid` that was executed; only on the dynamic path. |
| `dynamic` | `boolean` | Whether the dynamic execution path was taken. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('discovery.getMainMenuCommands');
if (res.success === false) throw new Error(res.error);
const { commands } = res;
const prefs = commands.find((c) => c.path === 'File/Preferences');
await fb2k.invoke('discovery.executeMainMenuCommand', { guid: prefs.guid });
```

`path` is built from host labels, so it is localized on a translated build. Cache the `guid` rather than repeating the path match. The `NOT_FOUND` failure echoes `guid`, `dynamic` and, on the dynamic path, `subGuid`.

### discovery.getAllServices

<!-- api-schema:begin discovery.getAllServices -->
Count the discoverable service families in the running process: how many main-menu commands, context-menu commands, groups, input formats, UI elements, DSPs, output backends, preference pages and components are registered. Both menu families are counted through the same walks the listing endpoints use, filtered to what the host would show.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `services` | `DiscoveryServiceCounts` | Count per service family. |
| `services.mainMenuCommands` | `integer` | Main-menu commands the host would show, dynamic children included. |
| `services.mainMenuDynamicCommands` | `integer` | How many of `mainMenuCommands` were expanded from dynamic submenus. |
| `services.mainMenuGroups` | `integer` | Main-menu groups. |
| `services.contextMenuCommands` | `integer` | Context-menu commands the host would show. |
| `services.inputFormats` | `integer` | Playable file types. |
| `services.uiElements` | `integer` | Registered UI elements. |
| `services.dspEntries` | `integer` | Registered DSPs. |
| `services.outputDevices` | `integer` | Registered output backends. |
| `services.preferencePages` | `integer` | Preference pages. |
| `services.components` | `integer` | Installed components. |
| `contextMenuHiddenFiltered` | `integer` | How many context-menu entries the "would the host show it" filter removed. |
| `stateKnown` | `boolean` | Whether context-menu state was observable; false when nothing was selected or playing. |
| `totalServices` | `integer` | Sum of every family except `mainMenuDynamicCommands`, which `mainMenuCommands` already includes. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`services.contextMenuCommands` counts context-menu commands through the same walk `discovery.getContextMenuCommands` performs, and is included in `totalServices`. Both menu families are filtered to what the host would actually show, so the counts stay comparable with the listing endpoints.

```js
const result = await fb2k.invoke('discovery.getAllServices');
```

### discovery.getComponents

<!-- api-schema:begin discovery.getComponents -->
List the installed components.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `components` | `DiscoveryComponentInfo[]` | The components. |
| `components[].filename` | `string` | DLL file name. |
| `components[].name` | `string` | Component name. |
| `components[].version` | `string` | Version string as the component reports it. |
| `components[].about` | `string` | About text. |
| `count` | `integer` | Number of entries in `components`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('discovery.getComponents');
```

### discovery.getContextMenuCommands

<!-- api-schema:begin discovery.getContextMenuCommands -->
List the registered context-menu commands, flattened, with their state for the current target. `enabled` and `checked` are only observable with a track selected or playing; check `stateKnown` before trusting them.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeHidden` | `boolean` | No | Also list entries the host would not show (`FORCE_OFF`, shortcut-list-only). Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `commands` | `DiscoveryContextMenuCommand[]` | The commands. |
| `commands[].enabled` | `boolean` | Whether the entry can be clicked. Meaningless while `stateKnown` is false. |
| `commands[].checked` | `boolean` | Whether the entry shows a check mark (a radio mark counts too). |
| `commands[].radioChecked` | `boolean` | Whether the check mark is a radio mark. |
| `commands[].hidden` | `boolean` | Whether the host would not draw the entry. |
| `commands[].stateKnown` | `boolean` | Whether `enabled` and `checked` were observed. False when the state could not be evaluated, such as a context-menu entry with nothing selected or playing. |
| `commands[].flags` | `integer` | The raw display flags the component reported. Main menu: `1` disabled, `2` checked, `4` radio, `8` default-hidden. Context menu: `1` checked, `2` disabled, `4` grayed, `8` radio. |
| `commands[].name` | `string` | Label as the host reports it. |
| `commands[].description` | `string` | Description the component provides; empty when it has none. |
| `commands[].guid` | `string` | Command GUID. |
| `commands[].parentGuid` | `string` | GUID of the parent group; the null GUID for a service without one. |
| `commands[].index` | `integer` | Index of the command within its service. |
| `commands[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | Where the entry came from. |
| `commands[].executable` | `boolean` | Whether the entry can be executed. |
| `commands[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | Why it cannot be executed; empty when it can. |
| `count` | `integer` | Number of entries in `commands`. |
| `includeHidden` | `boolean` | The `includeHidden` that applied. |
| `hiddenFiltered` | `integer` | How many entries the filter removed; `0` when `includeHidden` is set. |
| `stateKnown` | `boolean` | Whether `enabled` and `checked` were observed: true only with a track selected or playing. |
| `selectionCount` | `integer` | How many tracks the state was evaluated against. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

`enabled` / `checked` are only observable with a track selected or playing, because the SDK's `item_get_display_data_root()` evaluates display data against a `metadb_handle_list`. Check the response's `stateKnown` first: when it is false those two fields carry no observation and only `hidden` stays meaningful, since `FORCE_OFF` is a constant property of the item rather than a per-selection one.

```js
const result = await fb2k.invoke('discovery.getContextMenuCommands');
```

### discovery.getContextMenuTree

<!-- api-schema:begin discovery.getContextMenuTree -->
Dump the context menu for the current target as a nested tree, keeping submenus and separators. Depth and children per node are bounded; any clipping is reported through the `truncated` flags rather than left silent.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `tree` | `DiscoveryContextMenuTreeNode` | The root node. |
| `tree.name` | `string` | Label as the host reports it. |
| `tree.type` | `"command" \| "popup" \| "separator" \| "unknown"` | Node kind. |
| `tree.depth` | `integer` | Depth below the root, which is `0`. |
| `tree.enabled` | `boolean` | Whether the entry can be clicked. Absent on a separator. |
| `tree.checked` | `boolean` | Whether the entry shows a check mark. Absent on a separator. |
| `tree.radioChecked` | `boolean` | Whether the check mark is a radio mark. Absent on a separator. |
| `tree.hidden` | `boolean` | Whether the host would not draw the entry; always false here, because the tree is built from what the host shows. Absent on a separator. |
| `tree.stateKnown` | `boolean` | Whether the state was observed; always true here. Absent on a separator. |
| `tree.flags` | `integer` | The raw display flags. Absent on a separator. |
| `tree.fullName` | `string` | Full slash-separated name; only on a `command` node. |
| `tree.childCount` | `integer` | The host's real number of children; only on a `popup` node. |
| `tree.childrenReturned` | `integer` | How many children this response holds; only on a `popup` node. |
| `tree.children` | `DiscoveryContextMenuTreeNode[]` | The children, in menu order; only on a `popup` node. |
| `tree.truncated` | `boolean` | Whether anything below this node was clipped. |
| `tree.depthExceeded` | `boolean` | Whether the depth limit clipped something below this node. |
| `tree.childrenExceeded` | `boolean` | Whether the per-node children limit clipped something below this node. |
| `truncated` | `boolean` | Whether anything in the tree was clipped. |
| `depthExceeded` | `boolean` | Whether the depth limit clipped something. |
| `childrenExceeded` | `boolean` | Whether the per-node children limit clipped something. |
| `maxDepth` | `integer` | The depth limit that applied. |
| `maxChildrenPerNode` | `integer` | The per-node children limit that applied. |
| `itemCount` | `integer` | How many tracks the menu was built for. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Truncation is explicit rather than silent. A `popup` node reports `childCount` (the host's real count) alongside `childrenReturned` (what this response contains), so the two can be reconciled without counting the array. Any node whose subtree was clipped carries `truncated`, with `depthExceeded` / `childrenExceeded` distinguishing the cause; the flags propagate upward, so the response's top-level `truncated` covers the whole tree. `maxDepth` and `maxChildrenPerNode` echo the limits that were applied.

```js
const result = await fb2k.invoke('discovery.getContextMenuTree');
```

### discovery.getDspEntries

<!-- api-schema:begin discovery.getDspEntries -->
List the registered DSP entries.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `entries` | `DiscoveryDspEntryInfo[]` | The DSPs. |
| `entries[].guid` | `string` | DSP GUID. |
| `entries[].name` | `string` | Display name. |
| `count` | `integer` | Number of entries in `entries`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('discovery.getDspEntries');
```

### discovery.getInputFormats

<!-- api-schema:begin discovery.getInputFormats -->
List the playable input file types, each with a display name and a filename mask.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `fileTypes` | `DiscoveryInputFormatType[]` | The file types. |
| `fileTypes[].name` | `string` | Display name, such as `FLAC`. |
| `fileTypes[].mask` | `string` | Filename mask, such as `*.FLAC`. |
| `fileTypes[].index` | `integer` | Index within the service that registered it. |
| `count` | `integer` | Number of entries in `fileTypes`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('discovery.getInputFormats');
```

### discovery.getMainMenuCommands

<!-- api-schema:begin discovery.getMainMenuCommands -->
List the main-menu commands, flattened, with their label path, GUID and display state. Submenus that components build at runtime (`mainmenu_commands_v2`) are expanded by default, so their child commands appear next to the parent slot.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `expandDynamic` | `boolean` | No | Expand submenus that components build at runtime. `false` lists the static registry only. Default: `true`. |
| `includeHidden` | `boolean` | No | Also list entries the host would not show. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `commands` | `DiscoveryMainMenuCommand[]` | The commands, static slots first within each service, followed by their expanded children. |
| `commands[].enabled` | `boolean` | Whether the entry can be clicked. Meaningless while `stateKnown` is false. |
| `commands[].checked` | `boolean` | Whether the entry shows a check mark (a radio mark counts too). |
| `commands[].radioChecked` | `boolean` | Whether the check mark is a radio mark. |
| `commands[].hidden` | `boolean` | Whether the host would not draw the entry. |
| `commands[].stateKnown` | `boolean` | Whether `enabled` and `checked` were observed. False when the state could not be evaluated, such as a context-menu entry with nothing selected or playing. |
| `commands[].flags` | `integer` | The raw display flags the component reported. Main menu: `1` disabled, `2` checked, `4` radio, `8` default-hidden. Context menu: `1` checked, `2` disabled, `4` grayed, `8` radio. |
| `commands[].name` | `string` | Label as the host reports it (localized on a translated build). |
| `commands[].description` | `string` | Description the component provides; empty when it has none. |
| `commands[].guid` | `string` | Command GUID. For an entry expanded from a dynamic submenu this is the owning command's GUID. |
| `commands[].parentGuid` | `string` | GUID of the group the command is filed under. |
| `commands[].index` | `integer` | Index of the command within its service. |
| `commands[].path` | `string` | Slash-separated label path; for a static slot just its label. |
| `commands[].isDynamic` | `boolean` | Whether the entry belongs to a dynamic submenu, as its parent slot or as an expanded child. |
| `commands[].isDynamicParent` | `boolean` | Whether the entry is the parent slot of a dynamic submenu: a container that cannot be executed on its own. |
| `commands[].subGuid` | `string` | GUID of the child node inside a dynamic submenu; pass it with `guid` to `discovery.executeMainMenuCommand`. Absent on static entries. |
| `commands[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | Where the entry came from. |
| `commands[].executable` | `boolean` | Whether the entry can be executed. |
| `commands[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | Why it cannot be executed; empty when it can. |
| `count` | `integer` | Number of entries in `commands`. |
| `expandDynamic` | `boolean` | The `expandDynamic` that applied. |
| `includeHidden` | `boolean` | The `includeHidden` that applied. |
| `dynamicCount` | `integer` | How many entries were expanded from dynamic submenus. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Dynamic submenus registered through `mainmenu_commands_v2` (used by SMP-era components such as ESLyric) are expanded by default. A parent slot has `isDynamicParent: true` and is only a container; entries expanded from its subtree carry `isDynamic: true` and `subGuid`. Executing one requires passing its `subGuid` to `discovery.executeMainMenuCommand`.

```js
const result = await fb2k.invoke('discovery.getMainMenuCommands');
```

### discovery.getMainMenuGroups

<!-- api-schema:begin discovery.getMainMenuGroups -->
List the groups main-menu commands are filed under.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `groups` | `DiscoveryMainMenuGroup[]` | The groups. |
| `groups[].guid` | `string` | Group GUID. |
| `groups[].parentGuid` | `string` | GUID of the parent group. |
| `groups[].name` | `string` | Display name; empty for a group that is not a popup. |
| `groups[].sortPriority` | `integer` | Sort priority within the parent. |
| `count` | `integer` | Number of entries in `groups`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('discovery.getMainMenuGroups');
```

### discovery.getOutputDevices

<!-- api-schema:begin discovery.getOutputDevices -->
List the registered output backends by GUID.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `devices` | `DiscoveryOutputDeviceEntry[]` | The backends. |
| `devices[].guid` | `string` | Backend GUID. |
| `count` | `integer` | Number of entries in `devices`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('discovery.getOutputDevices');
```

### discovery.getPreferencePages

<!-- api-schema:begin discovery.getPreferencePages -->
List the pages of the Preferences dialog.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `pages` | `DiscoveryPreferencePageInfo[]` | The pages. |
| `pages[].guid` | `string` | Page GUID. |
| `pages[].parentGuid` | `string` | GUID of the parent page. |
| `pages[].name` | `string` | Display name. |
| `count` | `integer` | Number of entries in `pages`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('discovery.getPreferencePages');
```

### discovery.getUIElements

<!-- api-schema:begin discovery.getUIElements -->
List the registered UI elements.

This method takes no parameters.

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `elements` | `DiscoveryUIElementInfo[]` | The elements. |
| `elements[].guid` | `string` | Element GUID. |
| `elements[].subclassGuid` | `string` | Subclass GUID. |
| `elements[].name` | `string` | Display name. |
| `elements[].description` | `string` | Description. |
| `elements[].isUserAddable` | `boolean` | Whether a user may add it to a layout. |
| `count` | `integer` | Number of entries in `elements`. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('discovery.getUIElements');
```

### discovery.searchCommands

<!-- api-schema:begin discovery.searchCommands -->
Search command names, descriptions and menu paths across both menu families, case-insensitively (ASCII folding only). Each hit carries the same state fields the listing endpoints return, so a caller can tell whether it is invocable without a second call.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | Yes | Text to look for in names, descriptions and paths; ASCII case is ignored. Must not be empty. |
| `expandDynamic` | `boolean` | No | Also search commands expanded from dynamic submenus. Default: `true`. |
| `scope` | `"all" \| "mainmenu" \| "contextmenu"` | No | Which menu family to search. Default: `"all"`. |
| `includeHidden` | `boolean` | No | Also search entries the host would not show. Default: `false`. |

**Returns**

| Field | Type | Description |
| --- | --- | --- |
| `query` | `string` | The query as given. |
| `results` | `DiscoverySearchResult[]` | The hits, main menu first. |
| `results[].enabled` | `boolean` | Whether the entry can be clicked. Meaningless while `stateKnown` is false. |
| `results[].checked` | `boolean` | Whether the entry shows a check mark (a radio mark counts too). |
| `results[].radioChecked` | `boolean` | Whether the check mark is a radio mark. |
| `results[].hidden` | `boolean` | Whether the host would not draw the entry. |
| `results[].stateKnown` | `boolean` | Whether `enabled` and `checked` were observed. False when the state could not be evaluated, such as a context-menu entry with nothing selected or playing. |
| `results[].flags` | `integer` | The raw display flags the component reported. Main menu: `1` disabled, `2` checked, `4` radio, `8` default-hidden. Context menu: `1` checked, `2` disabled, `4` grayed, `8` radio. |
| `results[].name` | `string` | Label as the host reports it. |
| `results[].description` | `string` | Description; empty when the component has none. |
| `results[].guid` | `string` | Command GUID; for a dynamic child, the owning command's GUID. |
| `results[].path` | `string` | Slash-separated label path. A context-menu entry is registered flat, so its path is just its label. |
| `results[].isDynamic` | `boolean` | Whether the hit was expanded from a dynamic submenu. |
| `results[].type` | `"mainmenu" \| "contextmenu"` | Which menu family the hit belongs to. |
| `results[].subGuid` | `string` | GUID of the dynamic child node; pass it with `guid` to `discovery.executeMainMenuCommand`. |
| `results[].source` | `"mainmenu_static" \| "mainmenu_dynamic" \| "contextmenu_static" \| "contextmenu_dynamic" \| "hmenu_fallback"` | Where the entry came from. |
| `results[].executable` | `boolean` | Whether the entry can be executed. |
| `results[].unaddressableReason` | `"" \| "separator" \| "dynamicParent" \| "noStableIdentifier" \| "emptyNode"` | Why it cannot be executed; empty when it can. |
| `count` | `integer` | Number of entries in `results`. |
| `expandDynamic` | `boolean` | The `expandDynamic` that applied. |
| `scope` | `"all" \| "mainmenu" \| "contextmenu"` | The `scope` that applied. |
| `includeHidden` | `boolean` | The `includeHidden` that applied. |
| `mainMenuHits` | `integer` | Hits from the main menu. |
| `contextMenuHits` | `integer` | Hits from the context menu. |
| `stateKnown` | `boolean` | Whether context-menu state was observed. False when the context family was searched with nothing selected or playing; the hits' `enabled` and `checked` must not be filtered on then. |

`success` is `true` on success. On failure the response is `{ success: false, error, code }`; see [Error codes](../reference/errors.md) for `code`.
<!-- api-schema:end -->

Dynamic parent slots are skipped, since their expanded children carry the executable identity. Entries produced by a dynamic submenu carry `subGuid`; pass both `guid` and `subGuid` to `discovery.executeMainMenuCommand`. Context-menu items are registered flat and placed by the host, so for those `path` is just the label.

```js
const res = await fb2k.invoke('discovery.searchCommands', { query: 'rating' });
if (res.success === false) throw new Error(res.error);
const { results } = res;
```

## Discovery scope and execution rules

- Results enumerate services registered in the current foobar2000 process; counts and names vary with installed components and host configuration.
- `discovery.executeMainMenuCommand` and `discovery.executeContextMenuCommand` require a well-formed GUID; a malformed one fails with `INVALID_PARAMS`, one that no command owns with `NOT_FOUND`. A context command applies to the now-playing item when available, otherwise to the active playlist selection; with neither it fails with `NO_ACTIVE_ITEM`.
- Components that build their main-menu subtree at runtime (`mainmenu_commands_v2`, for example ESLyric) are expanded by `discovery.getMainMenuCommands` and `discovery.searchCommands`. Expanded entries are identified by `guid` plus `subGuid`, and `discovery.executeMainMenuCommand` dispatches them through the dynamic execution path when `subGuid` is supplied.
- Dynamic submenus are a snapshot of the moment the call is made: their contents can depend on the current track, selection, or component state.
- `discovery.executeContextMenuByPath` requires `path`; optional `trackPath` is subject to media-read security. Without it, the runtime uses the same now-playing/selection fallback.
- `discovery.getContextMenuTree` is diagnostic output. It requires an active target item. Recursion and per-node child count are both bounded, and any clipping is reported through `truncated` / `depthExceeded` / `childrenExceeded` rather than left for the caller to notice; the applied limits come back as `maxDepth` and `maxChildrenPerNode`.
- `discovery.searchCommands` requires a non-empty `query` and performs a case-insensitive match over command names, descriptions, and menu paths, across both the main menu and the context menu. Case folding is ASCII-only, so UTF-8 multi-byte sequences (CJK labels, which have no case to fold) pass through unchanged. `scope` accepts only `all`, `mainmenu` and `contextmenu`; any other value fails with `INVALID_PARAMS`.
