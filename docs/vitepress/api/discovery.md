# Discovery API

English API reference for the `discovery` family.

This page is the primary owner for the namespaces listed below. Method names, parameter keys, and return fields follow the C++ `RegisterApi` handlers.

## discovery

### discovery.executeContextMenuByPath

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1018`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `path` | `string` | No | Optional; default . |
| `trackPath` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","foundName":"...","itemCount":"...","path":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.executeContextMenuByPath', { path: /* value */, trackPath: /* value */ });
```

### discovery.executeContextMenuCommand

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1017`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | No | Optional; default . |

**Returns**: `{"error":"...","guid":"...","itemCount":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.executeContextMenuCommand', { guid: /* value */ });
```

### discovery.executeMainMenuCommand

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1015`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `guid` | `string` | Yes | Command GUID. For a dynamic child, pass its parent command GUID. |
| `subGuid` | `string` | No | `subGuid` of a dynamic child; dispatches via `mainmenu_commands::g_execute_dynamic`. |

**Returns**: `{"error":"...","guid":"...","subGuid":"...","dynamic":true,"success":true}`

```js
const result = await fb2k.invoke('discovery.executeMainMenuCommand', { guid: /* value */ });
```

### discovery.getAllServices

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1012`.

_No parameters._

**Returns**: `{"services":[],"success":true,"totalServices":"..."}`

```js
const result = await fb2k.invoke('discovery.getAllServices');
```

### discovery.getComponents

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1021`.

_No parameters._

**Returns**: `{"components":"...","count":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getComponents');
```

### discovery.getContextMenuCommands

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1016`.

_No parameters._

**Returns**: `{"commands":"...","count":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getContextMenuCommands');
```

### discovery.getContextMenuTree

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1019`.

_No parameters._

**Returns**: `{"error":"...","itemCount":"...","success":true,"tree":"..."}`

```js
const result = await fb2k.invoke('discovery.getContextMenuTree');
```

### discovery.getDspEntries

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1023`.

_No parameters._

**Returns**: `{"count":"...","entries":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getDspEntries');
```

### discovery.getInputFormats

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1020`.

_No parameters._

**Returns**: `{"count":"...","fileTypes":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getInputFormats');
```

### discovery.getMainMenuCommands

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1013`.

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

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1014`.

_No parameters._

**Returns**: `{"count":"...","groups":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getMainMenuGroups');
```

### discovery.getOutputDevices

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1024`.

_No parameters._

**Returns**: `{"count":"...","devices":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getOutputDevices');
```

### discovery.getPreferencePages

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1025`.

_No parameters._

**Returns**: `{"count":"...","pages":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getPreferencePages');
```

### discovery.getUIElements

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1022`.

_No parameters._

**Returns**: `{"count":"...","elements":"...","success":true}`

```js
const result = await fb2k.invoke('discovery.getUIElements');
```

### discovery.searchCommands

Public API method. Runtime authority: `src/api/DiscoveryApi.cpp:1026`.

| Parameter | Type | Required | Description |
| --- | --- | --- | --- |
| `query` | `string` | No | Optional; default . |
| `expandDynamic` | `boolean` | No | Default `true`. Also searches commands expanded from `mainmenu_commands_v2` dynamic submenus. |

**Returns**: `{"count":"...","error":"...","query":"...","results":"...","success":true}`

Each result carries `name`, `description`, `guid`, `type`, `path`, and `isDynamic`. Dynamic parent slots are skipped, so a hit is always executable. Entries produced by a dynamic submenu additionally carry `subGuid`; pass both `guid` and `subGuid` to `discovery.executeMainMenuCommand`.

```js
const result = await fb2k.invoke('discovery.searchCommands', { query: /* value */ });
```

## Discovery scope and execution rules

- Results enumerate services registered in the current foobar2000 process; counts and names vary with installed components and host configuration.
- `discovery.executeMainMenuCommand` and `discovery.executeContextMenuCommand` require a valid GUID. A context command applies to the now-playing item when available, otherwise to the active playlist selection.
- Components that build their main-menu subtree at runtime (`mainmenu_commands_v2`, for example ESLyric) are expanded by `discovery.getMainMenuCommands` and `discovery.searchCommands`. Expanded entries are identified by `guid` plus `subGuid`, and `discovery.executeMainMenuCommand` dispatches them through the dynamic execution path when `subGuid` is supplied.
- Dynamic submenus are a snapshot of the moment the call is made: their contents can depend on the current track, selection, or component state.
- `discovery.executeContextMenuByPath` requires `path`; optional `trackPath` is subject to media-read security. Without it, the runtime uses the same now-playing/selection fallback.
- `discovery.getContextMenuTree` is diagnostic output. It requires an active target item, limits recursion to 10 levels and limits each popup to 50 children.
- `discovery.searchCommands` requires a non-empty `query` and performs a case-insensitive match over main-menu command names, descriptions, and menu paths.
