# fb.discovery Service Discovery

`fb.discovery` enumerates foobar2000 services, menus, installed components, input formats, UI elements, DSP entries, output devices, and preference pages.

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK Methods

> This block provides SDK-level method coverage and may later be expanded with complete examples and best practices.

### getAllServices()

Signature: `fb.discovery.getAllServices(): Promise<DiscoveryGetAllServicesResponse>`

Returns category counts in `services` and their sum in `totalServices`.

### getMainMenuCommands(options?)

Signature: `fb.discovery.getMainMenuCommands(options?: { expandDynamic?: boolean }): Promise<DiscoveryGetMainMenuCommandsResponse>`

Returns `{ commands, count, dynamicCount }`. Each command includes `name`, `description`, `guid`, `parentGuid`, and `index`.

Components that build their submenu at runtime (`mainmenu_commands_v2`, for example ESLyric) are expanded by default, so their child commands appear alongside the static parent slot. Expanded children carry `subGuid`, `isDynamic: true`, and a `path` such as `View/ESLyric/Search lyrics`. Pass `{ expandDynamic: false }` to enumerate the static registry only.

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

### getContextMenuCommands()

Signature: `fb.discovery.getContextMenuCommands(): Promise<DiscoveryGetContextMenuCommandsResponse>`

Returns a flat list of discoverable context-menu commands.

### executeContextMenuCommand(options)

Signature: `fb.discovery.executeContextMenuCommand(options: DiscoveryExecuteContextMenuCommandParams): Promise<BaseResponse & { itemCount?: number }>`

Executes a context-menu command by `options.guid`.

### executeContextMenuByPath(options)

Signature: `fb.discovery.executeContextMenuByPath(options: DiscoveryExecuteContextMenuByPathParams): Promise<BaseResponse & { foundName?: string; itemCount?: number }>`

Executes a context-menu item by menu `path`, optionally for `trackPath`.

### getContextMenuTree()

Signature: `fb.discovery.getContextMenuTree(): Promise<DiscoveryGetContextMenuTreeResponse>`

Returns a recursive `tree` of `command`, `popup`, `separator`, or `unknown` nodes, plus an optional `itemCount`.

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

Signature: `fb.discovery.searchCommands(query: string, options?: { expandDynamic?: boolean }): Promise<DiscoverySearchCommandsResponse>`

Searches main-menu commands and returns the echoed `query`, `results`, and `count`. Names, descriptions, and menu paths are matched case-insensitively. The result `type` taxonomy uses `mainmenu` for static commands and `mainmenu-dynamic` for entries expanded from a runtime submenu; those entries also carry `subGuid` and `path`. Pass `{ expandDynamic: false }` to search the static registry only.

```javascript
const result = await fb.discovery.searchCommands('lyric');
const hit = result.results[0];
await fb.discovery.executeMainMenuCommand(hit.guid, hit.subGuid);
```

<!-- END AUTO-GENERATED SDK STUBS -->
