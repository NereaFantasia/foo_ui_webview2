# Discovery API

English API reference for the `discovery` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## discovery

### discovery.executeContextMenuByPath

Runs a context-menu command addressed by its slash-separated label path.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | Yes | Slash-separated label path of the command, for example `Playback Statistics/Rating/5`. An empty path is rejected. |
| `trackPath` | `string` | No | Target a specific file instead of the current one. Subject to media-read security. Omit to use the playing track, or the active playlist selection when nothing is playing. |

**Returns**: `{"error":"...","foundName":"...","itemCount":"...","path":"...","success":true}`

```js
await fb2k.invoke('discovery.executeContextMenuByPath', { path: 'Playback Statistics/Rating/5' });
```

### discovery.executeContextMenuCommand

Runs a context-menu command addressed by GUID.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Braced command GUID, as returned by `discovery.getContextMenuCommands`. A missing or empty value is rejected. |
| `force` | `boolean` | No | Default `false`. Set to `true` to dispatch even a command the host would never draw. |

**Returns**: `{"error":"...","guid":"...","itemCount":"...","success":true,"name":"...","hidden":false,"resolved":true,"force":false}`

A `FORCE_OFF` command is refused rather than dispatched: the SDK treats that state as "keyboard-shortcut list only", so the host never draws it and running it would perform something the user could not have clicked. Such a refusal comes back as `success: false` with `hidden: true`. `DEFAULT_OFF` commands (hidden unless Shift is held) remain reachable and are never refused.

`hidden` and `resolved` are returned on both the refusal and the success path, so "was not refused" is distinguishable from "this build does not report the field". `resolved` is false when no registered item owns the GUID, in which case there was no state to evaluate.

```js
const { commands } = await fb2k.invoke('discovery.getContextMenuCommands');
const rate5 = commands.find((c) => c.name === 'Rating/5');
await fb2k.invoke('discovery.executeContextMenuCommand', { guid: rate5.guid });
```

`name` is the label the host reports, so it is localized on a translated build. Cache the `guid` rather than repeating the label match.

### discovery.executeMainMenuCommand

Runs a main-menu command addressed by GUID, including a child of a dynamic submenu.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Command GUID. For a dynamic child, pass its parent command GUID. |
| `subGuid` | `string` | No | `subGuid` of a dynamic child; dispatches via `mainmenu_commands::g_execute_dynamic`. |

**Returns**: `{"error":"...","guid":"...","subGuid":"...","dynamic":true,"success":true}`

```js
const { commands } = await fb2k.invoke('discovery.getMainMenuCommands');
const prefs = commands.find((c) => c.path === 'File/Preferences');
await fb2k.invoke('discovery.executeMainMenuCommand', { guid: prefs.guid });
```

`path` is built from host labels, so it is localized on a translated build. Cache the `guid` rather than repeating the path match.

### discovery.getAllServices

Returns a count-only summary of the discoverable service families in the running process.

_No parameters._

**Returns**: `{"services":[],"success":true,"totalServices":"...","contextMenuHiddenFiltered":"...","stateKnown":true}`

`services.contextMenuCommands` counts context-menu commands through the same walk `discovery.getContextMenuCommands` performs, and is included in `totalServices`. Both menu families are filtered to what the host would actually show, so the counts stay comparable with the listing endpoints; `contextMenuHiddenFiltered` reports how many entries that filtering removed. `stateKnown` is false when nothing was selected or playing.

```js
const result = await fb2k.invoke('discovery.getAllServices');
```

### discovery.getComponents

Lists the installed components, each with its `filename`, `name`, `version` and `about` text.

_No parameters._

**Returns**: `{"components":"...","count":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getComponents');
```

### discovery.getContextMenuCommands

Lists the registered context-menu commands, flattened, with their per-selection state.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `includeHidden` | `boolean` | No | Default `false`. Set to `true` to also list entries the host would not show. |

**Returns**: `{"commands":"...","count":"...","success":true,"includeHidden":false,"hiddenFiltered":"...","stateKnown":true,"selectionCount":"..."}`

Each entry carries `enabled`, `checked`, `radioChecked`, `hidden`, `stateKnown`, the raw `flags`, `source`, `executable` and `unaddressableReason`.

`enabled` / `checked` are only observable with a track selected or playing, because the SDK's `item_get_display_data_root()` evaluates display data against a `metadb_handle_list`. Check the response's `stateKnown` first: when it is false those two fields carry no observation and only `hidden` stays meaningful, since `FORCE_OFF` is a constant property of the item rather than a per-selection one.

```js
const result = await fb2k.invoke('discovery.getContextMenuCommands');
```

### discovery.getContextMenuTree

Dumps the context menu as a nested tree, preserving submenu structure and separators.

_No parameters._

**Returns**: `{"error":"...","itemCount":"...","success":true,"tree":"...","truncated":false,"depthExceeded":false,"childrenExceeded":false,"maxDepth":"...","maxChildrenPerNode":"..."}`

Each node carries `name`, `type` (`"command"` / `"popup"` / `"separator"`) and `depth`. A non-separator node also carries the state vocabulary shared with the enumeration endpoints: `enabled`, `checked`, `radioChecked`, `hidden`, `stateKnown` and the raw `flags`. A `command` node adds `fullName`.

Truncation is explicit rather than silent. A `popup` node reports `childCount` (the host's real count) alongside `childrenReturned` (what this response contains), so the two can be reconciled without counting the array. Any node whose subtree was clipped carries `truncated`, with `depthExceeded` / `childrenExceeded` distinguishing the cause; the flags propagate upward, so the response's top-level `truncated` covers the whole tree. `maxDepth` and `maxChildrenPerNode` echo the limits that were applied.

```js
const result = await fb2k.invoke('discovery.getContextMenuTree');
```

### discovery.getDspEntries

Lists the registered DSP entries, each with its `guid` and `name`.

_No parameters._

**Returns**: `{"count":"...","entries":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getDspEntries');
```

### discovery.getInputFormats

Lists the playable input file types, each with a display `name` and a filename `mask`.

_No parameters._

**Returns**: `{"count":"...","fileTypes":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getInputFormats');
```

### discovery.getMainMenuCommands

Lists the main-menu commands, flattened, with their label path and GUID.

Dynamic submenus registered through `mainmenu_commands_v2` (used by SMP-era components such as ESLyric) are expanded by default, so their runtime child commands appear alongside the static parent slot.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `expandDynamic` | `boolean` | No | Defaults to `true`. Set `false` for the static registry only. |

**Returns**: `{"commands":"...","count":"...","dynamicCount":"...","expandDynamic":true,"success":true}`

Every entry carries `path` (slash-separated label path) plus `isDynamic` and `isDynamicParent`. A parent slot has `isDynamicParent: true` and is only a container — it is not executable on its own. Entries expanded from the subtree carry `isDynamic: true`, `subGuid`, and `flags` (the raw `mainmenu_commands` display bitmask: `1` disabled, `2` checked, `4` radio-checked, `8` default-hidden). Executing one requires passing its `subGuid` to `discovery.executeMainMenuCommand`.

```js
const result = await fb2k.invoke('discovery.getMainMenuCommands');
```

### discovery.getMainMenuGroups

Lists the main-menu groups that commands are filed under, each with its `guid`, `parentGuid`, `name` and `sortPriority`.

_No parameters._

**Returns**: `{"count":"...","groups":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getMainMenuGroups');
```

### discovery.getOutputDevices

Lists the registered output backends by `guid`.

_No parameters._

**Returns**: `{"count":"...","devices":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getOutputDevices');
```

### discovery.getPreferencePages

Lists the pages in the Preferences dialog, each with its `guid`, `parentGuid` and `name`.

_No parameters._

**Returns**: `{"count":"...","pages":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getPreferencePages');
```

### discovery.getUIElements

Lists the registered UI elements, each with its `guid`, `subclassGuid`, `name`, `description` and `isUserAddable`.

_No parameters._

**Returns**: `{"count":"...","elements":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getUIElements');
```

### discovery.searchCommands

Searches both menu families by name, description and menu path.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | Yes | Search text, matched case-insensitively. An empty query is rejected. |
| `expandDynamic` | `boolean` | No | Default `true`. Also searches commands expanded from `mainmenu_commands_v2` dynamic submenus. |
| `scope` | `string` | No | Default `all`. Set to `mainmenu` or `contextmenu` to search one family only; an unrecognized value widens to `all` rather than dropping results. |
| `includeHidden` | `boolean` | No | Default `false`. Set to `true` to also search entries the host would not show. |

**Returns**: `{"count":"...","error":"...","query":"...","results":"...","success":true,"scope":"...","includeHidden":false,"mainMenuHits":"...","contextMenuHits":"...","stateKnown":true}`

Both menu families are searched by default. Each result carries `name`, `description`, `guid`, `path`, `isDynamic` and `type` (`"mainmenu"` or `"contextmenu"`), plus the same state fields the enumeration endpoints return — `enabled`, `checked`, `radioChecked`, `hidden`, `stateKnown`, `flags`, `source`, `executable`, `unaddressableReason` — so a caller can tell whether a hit is invocable without a second round trip. Dynamic parent slots are skipped. Entries produced by a dynamic submenu additionally carry `subGuid`; pass both `guid` and `subGuid` to `discovery.executeMainMenuCommand`. Context-menu items are registered flat and placed by the host, so for those `path` is just the label.

When the context family was searched without a track selected or playing, the response's `stateKnown` is false: its hits' `enabled` / `checked` carry no observation and must not be filtered on.

```js
const { results } = await fb2k.invoke('discovery.searchCommands', { query: 'rating' });
```

## Discovery scope and execution rules

- Results enumerate services registered in the current foobar2000 process; counts and names vary with installed components and host configuration.
- `discovery.executeMainMenuCommand` and `discovery.executeContextMenuCommand` require a valid GUID. A context command applies to the now-playing item when available, otherwise to the active playlist selection.
- Components that build their main-menu subtree at runtime (`mainmenu_commands_v2`, for example ESLyric) are expanded by `discovery.getMainMenuCommands` and `discovery.searchCommands`. Expanded entries are identified by `guid` plus `subGuid`, and `discovery.executeMainMenuCommand` dispatches them through the dynamic execution path when `subGuid` is supplied.
- Dynamic submenus are a snapshot of the moment the call is made: their contents can depend on the current track, selection, or component state.
- `discovery.executeContextMenuByPath` requires `path`; optional `trackPath` is subject to media-read security. Without it, the runtime uses the same now-playing/selection fallback.
- `discovery.getContextMenuTree` is diagnostic output. It requires an active target item. Recursion and per-node child count are both bounded, and any clipping is reported through `truncated` / `depthExceeded` / `childrenExceeded` rather than left for the caller to notice; the applied limits come back as `maxDepth` and `maxChildrenPerNode`.
- `discovery.searchCommands` requires a non-empty `query` and performs a case-insensitive match over command names, descriptions, and menu paths, across both the main menu and the context menu. Case folding is ASCII-only, so UTF-8 multi-byte sequences (CJK labels, which have no case to fold) pass through unchanged.
