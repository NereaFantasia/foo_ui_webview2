# fb.discovery Service Discovery

`fb.discovery` enumerates foobar2000 services, menus, installed components, input formats, UI elements, DSP entries, output devices, and preference pages. It also runs main-menu commands by GUID, and context-menu commands by GUID or by menu path.

## getAllServices()

Signature: `fb.discovery.getAllServices(): Promise<DiscoveryGetAllServicesResponse>`

Returns category counts in `services` and their sum in `totalServices`. `services.contextMenuCommands` is counted through the same walk `getContextMenuCommands()` performs and is included in the sum; `contextMenuHiddenFiltered` reports how many entries were excluded for being hidden, and `stateKnown` is false when nothing was selected or playing. The sum leaves out `services.mainMenuDynamicCommands`, which `services.mainMenuCommands` already includes.

```javascript
const res = await fb.discovery.getAllServices();
if (res.success === false) throw new Error(res.error);
console.log(res.totalServices, res.services.components);
```

## getMainMenuCommands(options?)

Signature: `fb.discovery.getMainMenuCommands(options?: DiscoveryGetMainMenuCommandsParams): Promise<DiscoveryGetMainMenuCommandsResponse>`

Returns `{ commands, count, dynamicCount }`. Each command includes `name`, `description`, `guid`, `parentGuid`, and `index`.

Components that build their submenu at runtime (`mainmenu_commands_v2`, for example ESLyric) are expanded by default, so their child commands appear alongside the static parent slot. Expanded children carry `subGuid`, `isDynamic: true`, and a `path` such as `ESLyric/Search lyrics`. The path is rooted at the owning static command, not at the top-level menu. Pass `{ expandDynamic: false }` to enumerate the static registry only.

Entries the host would not show are omitted by default, because the real menu cannot reach them; pass `{ includeHidden: true }` for the unfiltered superset.

```javascript
const all = await fb.discovery.getMainMenuCommands();
if (all.success === false) throw new Error(all.error);
const dynamic = all.commands.filter((cmd) => cmd.isDynamic);
```

## getMainMenuGroups()

Signature: `fb.discovery.getMainMenuGroups(): Promise<DiscoveryGetMainMenuGroupsResponse>`

Returns `{ groups, count }`. Group descriptors include `guid`, `parentGuid`, `name`, and `sortPriority`; `name` is empty for a group that is not a popup.

Only statically registered `mainmenu_group` services are listed. Submenus that components build at runtime are trees of command nodes, not groups; list those with `getMainMenuCommands()`.

```javascript
const res = await fb.discovery.getMainMenuGroups();
if (res.success === false) throw new Error(res.error);
const popups = res.groups.filter((group) => group.name !== '');
```

## executeMainMenuCommand(guid, subGuid?)

Signature: `fb.discovery.executeMainMenuCommand(guid: string, subGuid?: string): Promise<DiscoveryExecuteMainMenuCommandResponse>`

Executes a main-menu command by GUID. For an entry expanded from a dynamic submenu, pass its `subGuid` as well; otherwise only the static parent command is dispatched. A GUID no command owns fails with `NOT_FOUND`.

The response echoes `guid`, and `subGuid` on the dynamic path; `dynamic` tells whether the dynamic execution path was taken.

```javascript
const all = await fb.discovery.getMainMenuCommands();
if (all.success === false) throw new Error(all.error);
const cmd = all.commands.find((c) => c.executable);
if (cmd) await fb.discovery.executeMainMenuCommand(cmd.guid, cmd.subGuid);
```

## getContextMenuCommands(options?)

Signature: `fb.discovery.getContextMenuCommands(options?: DiscoveryGetContextMenuCommandsParams): Promise<DiscoveryGetContextMenuCommandsResponse>`

Returns `{ commands, count }`: a flat list of discoverable context-menu commands, each with `enabled`, `checked`, `radioChecked`, `hidden`, `stateKnown`, raw `flags`, `source`, `executable`, and `unaddressableReason`. `selectionCount` reports how many tracks the state was evaluated against.

Entries the host would not show — `FORCE_OFF`, which the SDK defines as shortcut-list-only — are omitted by default; `hiddenFiltered` counts them and `{ includeHidden: true }` restores the superset.

`enabled` / `checked` are only observable with a track selected or playing, because the SDK evaluates display data against a track set. Check the response's `stateKnown` before trusting them: when it is false, only `hidden` is meaningful.

```javascript
const res = await fb.discovery.getContextMenuCommands();
if (res.success === false) throw new Error(res.error);
const runnable = res.stateKnown
    ? res.commands.filter((cmd) => cmd.enabled && cmd.executable)
    : res.commands.filter((cmd) => cmd.executable);
```

## executeContextMenuCommand(options)

Signature: `fb.discovery.executeContextMenuCommand(options: DiscoveryExecuteContextMenuCommandParams): Promise<DiscoveryExecuteContextMenuCommandResponse>`

Executes a context-menu command by `options.guid`. The command runs against the playing track, or the active playlist selection when nothing is playing; with neither, the call fails with `NO_ACTIVE_ITEM`.

A `FORCE_OFF` command is refused rather than dispatched: the host never draws it, so running it would perform something the user could not have clicked. The refusal comes back as `success: false` with the code `NOT_SUPPORTED` and `hidden: true`; pass `{ force: true }` to dispatch anyway. `DEFAULT_OFF` commands (hidden unless Shift is held) are still invocable and are never refused.

`hidden` and `resolved` are returned on both paths, so "was not refused" is distinguishable from "field absent". On success the response also carries the command's `name` and `itemCount`, the number of tracks it ran against.

```javascript
const res = await fb.discovery.getContextMenuCommands();
if (res.success === false) throw new Error(res.error);
const cmd = res.commands.find((c) => c.executable);
if (cmd) await fb.discovery.executeContextMenuCommand({ guid: cmd.guid });
```

## executeContextMenuByPath(options)

Signature: `fb.discovery.executeContextMenuByPath(options: DiscoveryExecuteContextMenuByPathParams): Promise<DiscoveryExecuteContextMenuByPathResponse>`

Executes a context-menu item by menu `path`, optionally for `trackPath`. Without `trackPath` the command runs against the playing track, or the active playlist selection when nothing is playing; with no target at all the call fails with `NO_ACTIVE_ITEM`. `trackPath` accepts a `|subsong:N` suffix; without one the file's first subsong is used.

`path` is slash-separated, such as `Playback Statistics/Rating/5`. Each segment must match one menu label exactly after normalization: the mnemonic `&`, a trailing ellipsis, accelerator text, and ASCII case are ignored. Matching is never a substring test, so `Rating/1` cannot resolve to `Rating/10`. A path that matches several commands is refused with `INVALID_PARAMS` and a `candidates` list of full names instead of being guessed; one that matches none fails with `NOT_FOUND`. Use `executeContextMenuCommand()` with a GUID when you need a stable identifier.

On success the response carries `foundName`, the full name of the command that ran, and `itemCount`, the number of tracks it ran against.

```javascript
const res = await fb.discovery.executeContextMenuByPath({
	path: 'Properties',
	trackPath: 'E:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
console.log(res.foundName, res.itemCount);
```

## getContextMenuTree()

Signature: `fb.discovery.getContextMenuTree(): Promise<DiscoveryGetContextMenuTreeResponse>`

Returns a recursive `tree` of `command`, `popup`, `separator`, or `unknown` nodes for the current target, plus `itemCount`, the number of tracks the menu was built for. Non-separator nodes carry the same state vocabulary as the enumeration endpoints.

The walk is bounded in depth and in children per node, and any clipping is reported: a `popup` node gives both `childCount` (the host's real count) and `childrenReturned` (what this response contains), and a node whose subtree was clipped carries `truncated` with `depthExceeded` / `childrenExceeded` naming the cause. The flags propagate upward, so the response's top-level `truncated` covers the whole tree; `maxDepth` and `maxChildrenPerNode` echo the applied limits.

```javascript
const result = await fb.discovery.getContextMenuTree();
if (result.success === false) throw new Error(result.error);
if (result.truncated) {
    console.warn('Menu tree was clipped', result.depthExceeded, result.childrenExceeded);
}
```

## getInputFormats()

Signature: `fb.discovery.getInputFormats(): Promise<DiscoveryGetInputFormatsResponse>`

Returns `{ fileTypes, count }`; each file type includes `name`, file-mask `mask`, and `index`.

```javascript
const res = await fb.discovery.getInputFormats();
if (res.success === false) throw new Error(res.error);
const masks = res.fileTypes.map((type) => type.mask);
```

## getComponents()

Signature: `fb.discovery.getComponents(): Promise<DiscoveryGetComponentsResponse>`

Returns `{ components, count }`. Components include `filename`, `name`, `version`, and `about`.

```javascript
const res = await fb.discovery.getComponents();
if (res.success === false) throw new Error(res.error);
const { components } = res;
```

## getDspEntries()

Signature: `fb.discovery.getDspEntries(): Promise<DiscoveryGetDspEntriesResponse>`

Returns `{ entries, count }`; each entry has a DSP `guid` and display `name`.

```javascript
const res = await fb.discovery.getDspEntries();
if (res.success === false) throw new Error(res.error);
const { entries } = res;
```

## getOutputDevices()

Signature: `fb.discovery.getOutputDevices(): Promise<DiscoveryGetOutputDevicesResponse>`

Returns `{ devices, count }`. Each entry is a registered output backend and carries only its `guid`.

```javascript
const res = await fb.discovery.getOutputDevices();
if (res.success === false) throw new Error(res.error);
const guids = res.devices.map((device) => device.guid);
```

## getPreferencePages()

Signature: `fb.discovery.getPreferencePages(): Promise<DiscoveryGetPreferencePagesResponse>`

Returns `{ pages, count }` for the pages of the Preferences dialog; each page includes `guid`, `parentGuid`, and `name`.

```javascript
const res = await fb.discovery.getPreferencePages();
if (res.success === false) throw new Error(res.error);
const names = res.pages.map((page) => page.name);
```

## getUIElements()

Signature: `fb.discovery.getUIElements(): Promise<DiscoveryGetUIElementsResponse>`

Returns `{ elements, count }`; each element includes `guid`, `subclassGuid`, `name`, `description`, and `isUserAddable`, which tells whether a user may add the element to a layout.

```javascript
const res = await fb.discovery.getUIElements();
if (res.success === false) throw new Error(res.error);
const addable = res.elements.filter((element) => element.isUserAddable);
```

## searchCommands(query, options?)

Signature: `fb.discovery.searchCommands(query: string, options?: Omit<DiscoverySearchCommandsParams, 'query'>): Promise<DiscoverySearchCommandsResponse>`

Searches both menu families and returns the echoed `query`, `results`, and `count`, plus `mainMenuHits` / `contextMenuHits`. Names, descriptions, and menu paths are matched case-insensitively; case folding is ASCII-only, so UTF-8 multi-byte sequences pass through unchanged.

The result `type` is `mainmenu` or `contextmenu`. Entries expanded from a runtime submenu carry `subGuid` and `isDynamic: true`; dynamic parent slots are skipped. Every hit also carries the enumeration state fields, so a caller can tell whether it is invocable without a second round trip.

Pass `{ scope: 'mainmenu' }` or `{ scope: 'contextmenu' }` to search one family only, `{ expandDynamic: false }` for the static registry only, or `{ includeHidden: true }` to include entries the host would not show.

When the context family was searched without a track selected or playing, the response's `stateKnown` is false and its hits' `enabled` / `checked` must not be filtered on.

```javascript
const result = await fb.discovery.searchCommands('lyric', { scope: 'mainmenu' });
if (result.success === false) throw new Error(result.error);
const hit = result.results[0];
await fb.discovery.executeMainMenuCommand(hit.guid, hit.subGuid);
```
