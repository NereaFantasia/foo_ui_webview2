# fb.discovery 服务发现

`fb.discovery` 枚举 foobar2000 的服务、菜单、已安装组件、输入格式、UI 元素、DSP 条目、输出设备和首选项页，也能执行菜单命令：主菜单命令按 GUID 执行，右键菜单命令按 GUID 或菜单路径执行。

## getAllServices()

签名：`fb.discovery.getAllServices(): Promise<DiscoveryGetAllServicesResponse>`

返回 `services` 中各服务类别的数量，以及汇总后的 `totalServices`。`services.contextMenuCommands` 与 `getContextMenuCommands()` 走同一次枚举，并计入汇总；`contextMenuHiddenFiltered` 给出因隐藏而被排除的数量，无选中且无播放曲目时 `stateKnown` 为 `false`。汇总不含 `services.mainMenuDynamicCommands`，因为 `services.mainMenuCommands` 已经包括了它们。

```javascript
const res = await fb.discovery.getAllServices();
if (res.success === false) throw new Error(res.error);
console.log(res.totalServices, res.services.components);
```

## getMainMenuCommands(options?)

签名：`fb.discovery.getMainMenuCommands(options?: DiscoveryGetMainMenuCommandsParams): Promise<DiscoveryGetMainMenuCommandsResponse>`

返回 `{ commands, count, dynamicCount }`。每个命令包含 `name`、`description`、`guid`、`parentGuid` 与 `index`。

对在运行时构建子菜单的组件（`mainmenu_commands_v2`，例如 ESLyric），默认会展开其动态子树，因此结果中除静态父项外还包含子命令。展开出的子项带有 `subGuid`、`isDynamic: true` 以及形如 `ESLyric/搜索歌词` 的 `path`。该路径以所属静态命令为根，不含顶层菜单。传入 `{ expandDynamic: false }` 可只枚举静态注册表。

宿主不会显示的条目在真实菜单里点不到，默认不列出；传 `{ includeHidden: true }` 可取回未过滤的完整集合。

```javascript
const all = await fb.discovery.getMainMenuCommands();
if (all.success === false) throw new Error(all.error);
const dynamic = all.commands.filter((cmd) => cmd.isDynamic);
```

## executeMainMenuCommand(guid, subGuid?)

签名：`fb.discovery.executeMainMenuCommand(guid: string, subGuid?: string): Promise<DiscoveryExecuteMainMenuCommandResponse>`

按 GUID 执行主菜单命令。若目标是从动态子菜单展开出的条目，需同时传入其 `subGuid`；否则只会派发静态父命令。没有命令拥有该 GUID 时以 `NOT_FOUND` 失败。

返回值回显 `guid`，走动态路径时还回显 `subGuid`；`dynamic` 表示是否走了动态执行路径。

```javascript
const all = await fb.discovery.getMainMenuCommands();
if (all.success === false) throw new Error(all.error);
const cmd = all.commands.find((c) => c.executable);
if (cmd) await fb.discovery.executeMainMenuCommand(cmd.guid, cmd.subGuid);
```

## executeContextMenuCommand(options)

签名：`fb.discovery.executeContextMenuCommand(options: DiscoveryExecuteContextMenuCommandParams): Promise<DiscoveryExecuteContextMenuCommandResponse>`

按 `options.guid` 执行上下文菜单命令。命令作用于正在播放的曲目，没有播放时作用于活动播放列表的选中项；两者都没有时以 `NO_ACTIVE_ITEM` 失败。

`FORCE_OFF` 的命令会被拒绝而非派发：宿主从不绘制它，执行等于做了一件用户根本点不到的事。拒绝时返回 `success: false`，错误码为 `NOT_SUPPORTED`，并带 `hidden: true`；传 `{ force: true }` 可强制派发。`DEFAULT_OFF`（按 Shift 才显示）仍可调用，永不拒绝。

`hidden` 与 `resolved` 在两条路径上都会返回，因此「未被拒绝」与「无此字段」可以区分。成功时返回值还带命令的 `name` 和 `itemCount`（命令作用于多少首曲目）。

```javascript
const res = await fb.discovery.getContextMenuCommands();
if (res.success === false) throw new Error(res.error);
const cmd = res.commands.find((c) => c.executable);
if (cmd) await fb.discovery.executeContextMenuCommand({ guid: cmd.guid });
```

## executeContextMenuByPath(options)

签名：`fb.discovery.executeContextMenuByPath(options: DiscoveryExecuteContextMenuByPathParams): Promise<DiscoveryExecuteContextMenuByPathResponse>`

按菜单 `path` 执行上下文菜单项，也可通过 `trackPath` 指定曲目。省略 `trackPath` 时作用于正在播放的曲目，没有播放时作用于活动播放列表的选中项；都没有时以 `NO_ACTIVE_ITEM` 失败。`trackPath` 可带 `|subsong:N` 后缀，不带时指文件的第一首。

`path` 用斜杠分隔，如 `Playback Statistics/Rating/5`。每一段都要在规范化后恰好匹配一个菜单标签，规范化忽略助记符 `&`、末尾省略号、快捷键文本与 ASCII 大小写。匹配从不按子串比较，所以 `Rating/1` 不会匹配到 `Rating/10`。路径匹配到多个命令时不会随便挑一个，而是以 `INVALID_PARAMS` 拒绝，并在 `candidates` 里列出各候选的完整名；一个都匹配不到时以 `NOT_FOUND` 失败。需要稳定标识时，改用 `executeContextMenuCommand()` 按 GUID 执行。

成功时返回值带 `foundName`（实际执行的命令的完整名）和 `itemCount`（命令作用于多少首曲目）。

```javascript
const res = await fb.discovery.executeContextMenuByPath({
	path: 'Properties',
	trackPath: 'E:\\Music\\song.flac',
});
if (res.success === false) throw new Error(res.error);
console.log(res.foundName, res.itemCount);
```

## getInputFormats()

签名：`fb.discovery.getInputFormats(): Promise<DiscoveryGetInputFormatsResponse>`

返回 `{ fileTypes, count }`；每种文件类型包含 `name`、文件掩码 `mask` 与 `index`。

```javascript
const res = await fb.discovery.getInputFormats();
if (res.success === false) throw new Error(res.error);
const masks = res.fileTypes.map((type) => type.mask);
```

## getComponents()

签名：`fb.discovery.getComponents(): Promise<DiscoveryGetComponentsResponse>`

返回 `{ components, count }`。每个组件包含 `filename`、`name`、`version` 与 `about`。

```javascript
const res = await fb.discovery.getComponents();
if (res.success === false) throw new Error(res.error);
const { components } = res;
```

## getContextMenuCommands(options?)

签名：`fb.discovery.getContextMenuCommands(options?: DiscoveryGetContextMenuCommandsParams): Promise<DiscoveryGetContextMenuCommandsResponse>`

返回 `{ commands, count }`，即扁平的右键菜单命令列表，每项带 `enabled`、`checked`、`radioChecked`、`hidden`、`stateKnown`、原始 `flags`、`source`、`executable` 与 `unaddressableReason`。`selectionCount` 给出状态是针对多少首曲目求值的。

宿主不会显示的条目（`FORCE_OFF`，SDK 定义为仅出现在快捷键列表）默认被过滤，数量记在 `hiddenFiltered`；传 `{ includeHidden: true }` 可取回完整集合。

`enabled` / `checked` 只有在有选中曲目或正在播放时才可观测，因为 SDK 是针对一组曲目求值的。请先看响应的 `stateKnown`：为 `false` 时只有 `hidden` 有意义。

```javascript
const res = await fb.discovery.getContextMenuCommands();
if (res.success === false) throw new Error(res.error);
const runnable = res.stateKnown
    ? res.commands.filter((cmd) => cmd.enabled && cmd.executable)
    : res.commands.filter((cmd) => cmd.executable);
```

## getContextMenuTree()

签名：`fb.discovery.getContextMenuTree(): Promise<DiscoveryGetContextMenuTreeResponse>`

返回当前目标的递归 `tree`，节点类型为 `command` / `popup` / `separator` / `unknown`，另有 `itemCount`，即菜单是为多少首曲目构建的。非分隔符节点携带与列举端点一致的状态字段。

遍历在深度与每节点子项数上都有上限，任何裁剪都会上报：`popup` 节点同时给出 `childCount`（宿主真实子项数）与 `childrenReturned`（本次响应实际包含的数量）；子树被裁剪的节点带 `truncated`，并由 `depthExceeded` / `childrenExceeded` 说明原因。标记向上传播，因此顶层 `truncated` 覆盖整棵树，`maxDepth` 与 `maxChildrenPerNode` 回显生效上限。

```javascript
const result = await fb.discovery.getContextMenuTree();
if (result.success === false) throw new Error(result.error);
if (result.truncated) {
    console.warn('菜单树被裁剪', result.depthExceeded, result.childrenExceeded);
}
```

## getDspEntries()

签名：`fb.discovery.getDspEntries(): Promise<DiscoveryGetDspEntriesResponse>`

返回 `{ entries, count }`；每个条目带 DSP 的 `guid` 和显示名 `name`。

```javascript
const res = await fb.discovery.getDspEntries();
if (res.success === false) throw new Error(res.error);
const { entries } = res;
```

## getMainMenuGroups()

签名：`fb.discovery.getMainMenuGroups(): Promise<DiscoveryGetMainMenuGroupsResponse>`

返回 `{ groups, count }`。每个分组包含 `guid`、`parentGuid`、`name` 与 `sortPriority`；不是弹出菜单的分组 `name` 为空。

只列出静态注册的 `mainmenu_group` 服务。组件在运行时构建的子菜单是命令节点树，不是分组，要用 `getMainMenuCommands()` 列出。

```javascript
const res = await fb.discovery.getMainMenuGroups();
if (res.success === false) throw new Error(res.error);
const popups = res.groups.filter((group) => group.name !== '');
```

## getOutputDevices()

签名：`fb.discovery.getOutputDevices(): Promise<DiscoveryGetOutputDevicesResponse>`

返回 `{ devices, count }`。每个条目是一个已注册的输出后端，只带它的 `guid`。

```javascript
const res = await fb.discovery.getOutputDevices();
if (res.success === false) throw new Error(res.error);
const guids = res.devices.map((device) => device.guid);
```

## getPreferencePages()

签名：`fb.discovery.getPreferencePages(): Promise<DiscoveryGetPreferencePagesResponse>`

返回首选项对话框的页面 `{ pages, count }`；每个页面包含 `guid`、`parentGuid` 与 `name`。

```javascript
const res = await fb.discovery.getPreferencePages();
if (res.success === false) throw new Error(res.error);
const names = res.pages.map((page) => page.name);
```

## getUIElements()

签名：`fb.discovery.getUIElements(): Promise<DiscoveryGetUIElementsResponse>`

返回 `{ elements, count }`；每个元素包含 `guid`、`subclassGuid`、`name`、`description` 与 `isUserAddable`，后者表示用户能否把它加进布局。

```javascript
const res = await fb.discovery.getUIElements();
if (res.success === false) throw new Error(res.error);
const addable = res.elements.filter((element) => element.isUserAddable);
```

## searchCommands(query, options?)

签名：`fb.discovery.searchCommands(query: string, options?: Omit<DiscoverySearchCommandsParams, 'query'>): Promise<DiscoverySearchCommandsResponse>`

搜索主菜单与右键菜单两侧的命令，返回原始 `query`、`results`、`count`，以及 `mainMenuHits` / `contextMenuHits`；名称、描述与菜单路径均按不区分大小写匹配。大小写折叠仅作用于 ASCII，UTF-8 多字节序列原样通过。

结果的 `type` 为 `mainmenu` 或 `contextmenu`。运行时子菜单展开项带有 `subGuid` 与 `isDynamic: true`，动态父槽位会被跳过。每条结果还携带列举端点的状态字段，调用方无需再发一次请求即可判断能否执行。

传 `{ scope: 'mainmenu' }` 或 `{ scope: 'contextmenu' }` 可只搜单一菜单族，`{ expandDynamic: false }` 只搜静态注册表，`{ includeHidden: true }` 则连宿主不显示的条目一并搜索。

搜索包含右键菜单但无选中且无播放曲目时，响应的 `stateKnown` 为 `false`，此时结果里的 `enabled` / `checked` 不得用于过滤。

```javascript
const result = await fb.discovery.searchCommands('lyric', { scope: 'mainmenu' });
if (result.success === false) throw new Error(result.error);
const hit = result.results[0];
await fb.discovery.executeMainMenuCommand(hit.guid, hit.subGuid);
```
