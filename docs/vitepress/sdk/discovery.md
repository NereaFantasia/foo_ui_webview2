# fb.discovery Service Discovery

`fb.discovery` enumerates foobar2000 services, menus, installed components, input formats, UI elements, DSP entries, output devices, and preference pages.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK Methods

> This block provides SDK-level method coverage and may later be expanded with complete examples and best practices.

### getAllServices()

Signature: `fb.discovery.getAllServices(): Promise<DiscoveryGetAllServicesResponse>`

Returns category counts in `services` and their sum in `totalServices`. `services.contextMenuCommands` is counted through the same walk `getContextMenuCommands()` performs and is included in the sum; `contextMenuHiddenFiltered` reports how many entries were excluded for being hidden, and `stateKnown` is false when nothing was selected or playing.

### getMainMenuCommands(options?)

Signature: `fb.discovery.getMainMenuCommands(options?: { expandDynamic?: boolean }): Promise<DiscoveryGetMainMenuCommandsResponse>`

Returns `{ commands, count, dynamicCount }`. Each command includes `name`, `description`, `guid`, `parentGuid`, and `index`.

Components that build their submenu at runtime (`mainmenu_commands_v2`, for example ESLyric) are expanded by default, so their child commands appear alongside the static parent slot. Expanded children carry `subGuid`, `isDynamic: true`, and a `path` such as `ESLyric/Search lyrics`. The path is rooted at the owning static command, not at the top-level menu. Pass `{ expandDynamic: false }` to enumerate the static registry only.

```javascript
const all = await fb.discovery.getMainMenuCommands();
const dynamic = all.commands.filter((cmd) => cmd.isDynamic);
```

### getMainMenuGroups()

Signature: `fb.discovery.getMainMenuGroups(): Promise<DiscoveryGetMainMenuGroupsResponse>`

Returns `{ groups, count }`. Group descriptors include `guid`, `parentGuid`, `name`, and `sortPriority`.

### executeMainMenuCommand(guid, subGuid?)

Signature: `fb.discovery.executeMainMenuCommand(guid: string, subGuid?: string): Promise<BaseResponse & { subGuid?: string; dynamic?: boolean }>`

Executes a main-menu command by GUID. For an entry expanded from a dynamic submenu, pass its `subGuid` as well; otherwise only the static parent command is dispatched.

### getContextMenuCommands(options?)

Signature: `fb.discovery.getContextMenuCommands(options?: DiscoveryGetContextMenuCommandsParams): Promise<DiscoveryGetContextMenuCommandsResponse>`

Returns a flat list of discoverable context-menu commands, each with `enabled`, `checked`, `radioChecked`, `hidden`, `stateKnown`, raw `flags`, `source`, `executable`, and `unaddressableReason`.

Entries the host would not show — `FORCE_OFF`, which the SDK defines as shortcut-list-only — are omitted by default; `hiddenFiltered` counts them and `{ includeHidden: true }` restores the superset.

`enabled` / `checked` are only observable with a track selected or playing, because the SDK evaluates display data against a track set. Check the response's `stateKnown` before trusting them: when it is false, only `hidden` is meaningful.

```javascript
const res = await fb.discovery.getContextMenuCommands();
const runnable = res.stateKnown
    ? res.commands.filter((cmd) => cmd.enabled && cmd.executable)
    : res.commands.filter((cmd) => cmd.executable);
```

### executeContextMenuCommand(options)

Signature: `fb.discovery.executeContextMenuCommand(options: DiscoveryExecuteContextMenuCommandParams): Promise<BaseResponse & { itemCount?: number; name?: string; hidden?: boolean; resolved?: boolean; force?: boolean }>`

Executes a context-menu command by `options.guid`.

A `FORCE_OFF` command is refused rather than dispatched: the host never draws it, so running it would perform something the user could not have clicked. The refusal comes back as `success: false` with `hidden: true`; pass `{ force: true }` to dispatch anyway. `DEFAULT_OFF` commands (hidden unless Shift is held) are still invocable and are never refused.

`hidden` and `resolved` are returned on both paths, so "was not refused" is distinguishable from "field absent".

### executeContextMenuByPath(options)

Signature: `fb.discovery.executeContextMenuByPath(options: DiscoveryExecuteContextMenuByPathParams): Promise<BaseResponse & { foundName?: string; itemCount?: number }>`

Executes a context-menu item by menu `path`, optionally for `trackPath`.

### getContextMenuTree()

Signature: `fb.discovery.getContextMenuTree(): Promise<DiscoveryGetContextMenuTreeResponse>`

Returns a recursive `tree` of `command`, `popup`, `separator`, or `unknown` nodes, plus an optional `itemCount`. Non-separator nodes carry the same state vocabulary as the enumeration endpoints.

The walk is bounded in depth and in children per node, and any clipping is reported: a `popup` node gives both `childCount` (the host's real count) and `childrenReturned` (what this response contains), and a node whose subtree was clipped carries `truncated` with `depthExceeded` / `childrenExceeded` naming the cause. The flags propagate upward, so the response's top-level `truncated` covers the whole tree; `maxDepth` and `maxChildrenPerNode` echo the applied limits.

### getInputFormats()

Signature: `fb.discovery.getInputFormats(): Promise<DiscoveryGetInputFormatsResponse>`

Returns `{ fileTypes, count }`; each file type includes `name`, file-mask `mask`, and `index`.

### getComponents()

Signature: `fb.discovery.getComponents(): Promise<DiscoveryGetComponentsResponse>`

Returns `{ components, count }`. Components include `filename`, `name`, `version`, and `about`.

```javascript
const { components } = await fb.discovery.getComponents();
```

### getDspEntries()

Signature: `fb.discovery.getDspEntries(): Promise<DiscoveryGetDspEntriesResponse>`

Returns `{ entries, count }`; each entry has a DSP `guid` and display `name`.

```javascript
const { entries } = await fb.discovery.getDspEntries();
```

### getOutputDevices()

Signature: `fb.discovery.getOutputDevices(): Promise<DiscoveryGetOutputDevicesResponse>`

Returns `{ devices, count }`. Discovery device descriptors currently contain only `guid`.

### getPreferencePages()

Signature: `fb.discovery.getPreferencePages(): Promise<DiscoveryGetPreferencePagesResponse>`

Returns `{ pages, count }`; each page includes `guid`, `parentGuid`, and `name`.

### getUIElements()

Signature: `fb.discovery.getUIElements(): Promise<DiscoveryGetUIElementsResponse>`

Returns `{ elements, count }`; each element includes `guid`, `subclassGuid`, `name`, `description`, and `isUserAddable`.

### searchCommands(query, options?)

Signature: `fb.discovery.searchCommands(query: string, options?: { expandDynamic?: boolean; scope?: 'all' | 'mainmenu' | 'contextmenu'; includeHidden?: boolean }): Promise<DiscoverySearchCommandsResponse>`

Searches both menu families and returns the echoed `query`, `results`, and `count`, plus `mainMenuHits` / `contextMenuHits`. Names, descriptions, and menu paths are matched case-insensitively; case folding is ASCII-only, so UTF-8 multi-byte sequences pass through unchanged.

The result `type` is `mainmenu` or `contextmenu`. Entries expanded from a runtime submenu carry `subGuid` and `isDynamic: true`; dynamic parent slots are skipped. Every hit also carries the enumeration state fields, so a caller can tell whether it is invocable without a second round trip.

Pass `{ scope: 'mainmenu' }` or `{ scope: 'contextmenu' }` to search one family only, `{ expandDynamic: false }` for the static registry only, or `{ includeHidden: true }` to include entries the host would not show.

When the context family was searched without a track selected or playing, the response's `stateKnown` is false and its hits' `enabled` / `checked` must not be filtered on.

```javascript
const result = await fb.discovery.searchCommands('lyric', { scope: 'mainmenu' });
const hit = result.results[0];
await fb.discovery.executeMainMenuCommand(hit.guid, hit.subGuid);
```

<!-- END AUTO-GENERATED SDK STUBS -->
