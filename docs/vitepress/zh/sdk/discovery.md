# fb.discovery 服务发现

本页是 `fb.discovery` 的 SDK 视角文档入口。

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### getAllServices()

签名：`fb.discovery.getAllServices(): Promise<DiscoveryGetAllServicesResponse>`

返回 `services` 中各服务类别的数量，以及汇总后的 `totalServices`。`services.contextMenuCommands` 与 `getContextMenuCommands()` 走同一次枚举，并计入汇总；`contextMenuHiddenFiltered` 给出因隐藏而被排除的数量，无选中且无播放曲目时 `stateKnown` 为 `false`。

### getMainMenuCommands(options?)

签名：`fb.discovery.getMainMenuCommands(options?: { expandDynamic?: boolean }): Promise<DiscoveryGetMainMenuCommandsResponse>`

返回 `{ commands, count, dynamicCount }`。每个命令包含 `name`、`description`、`guid`、`parentGuid` 与 `index`。

对在运行时构建子菜单的组件（`mainmenu_commands_v2`，例如 ESLyric），默认会展开其动态子树，因此结果中除静态父项外还包含子命令。展开出的子项带有 `subGuid`、`isDynamic: true` 以及形如 `ESLyric/搜索歌词` 的 `path`。该路径以所属静态命令为根，不含顶层菜单。传入 `{ expandDynamic: false }` 可只枚举静态注册表。

```javascript
const all = await fb.discovery.getMainMenuCommands();
const dynamic = all.commands.filter((cmd) => cmd.isDynamic);
```

### executeMainMenuCommand(guid, subGuid?)

签名：`fb.discovery.executeMainMenuCommand(guid: string, subGuid?: string): Promise<BaseResponse & { subGuid?: string; dynamic?: boolean }>`

按 GUID 执行主菜单命令。若目标是从动态子菜单展开出的条目，需同时传入其 `subGuid`；否则只会派发静态父命令。

### executeContextMenuCommand(options)

签名：`fb.discovery.executeContextMenuCommand(options: DiscoveryExecuteContextMenuCommandParams): Promise<BaseResponse & { itemCount?: number; name?: string; hidden?: boolean; resolved?: boolean; force?: boolean }>`

按 `options.guid` 执行上下文菜单命令。

`FORCE_OFF` 的命令会被拒绝而非派发：宿主从不绘制它，执行等于做了一件用户根本点不到的事。拒绝时返回 `success: false` 且 `hidden: true`；传 `{ force: true }` 可强制派发。`DEFAULT_OFF`（按 Shift 才显示）仍可调用，永不拒绝。

`hidden` 与 `resolved` 在两条路径上都会返回，因此「未被拒绝」与「无此字段」可以区分。

### executeContextMenuByPath(options)

签名：`fb.discovery.executeContextMenuByPath(options: DiscoveryExecuteContextMenuByPathParams): Promise<BaseResponse & { foundName?: string; itemCount?: number }>`

按菜单 `path` 执行上下文菜单项，也可通过 `trackPath` 指定曲目。

### getInputFormats()

签名：`fb.discovery.getInputFormats(): Promise<DiscoveryGetInputFormatsResponse>`

返回 `{ fileTypes, count }`；每种文件类型包含 `name`、文件掩码 `mask` 与 `index`。

### getComponents()

签名：`fb.discovery.getComponents(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `discovery.getComponents` 调用结果。

```javascript
const result = await fb.discovery.getComponents();
```

### getContextMenuCommands()

签名：`fb.discovery.getContextMenuCommands(options?: DiscoveryGetContextMenuCommandsParams): Promise<DiscoveryGetContextMenuCommandsResponse>`

返回扁平的右键菜单命令列表，每项带 `enabled`、`checked`、`radioChecked`、`hidden`、`stateKnown`、原始 `flags`、`source`、`executable` 与 `unaddressableReason`。

宿主不会显示的条目（`FORCE_OFF`，SDK 定义为仅出现在快捷键列表）默认被过滤，数量记在 `hiddenFiltered`；传 `{ includeHidden: true }` 可取回完整集合。

`enabled` / `checked` 只有在有选中曲目或正在播放时才可观测，因为 SDK 是针对一组曲目求值的。请先看响应的 `stateKnown`：为 `false` 时只有 `hidden` 有意义。

```javascript
const res = await fb.discovery.getContextMenuCommands();
const runnable = res.stateKnown
    ? res.commands.filter((cmd) => cmd.enabled && cmd.executable)
    : res.commands.filter((cmd) => cmd.executable);
```

### getContextMenuTree()

签名：`fb.discovery.getContextMenuTree(): Promise<DiscoveryGetContextMenuTreeResponse>`

返回递归的 `tree`，节点类型为 `command` / `popup` / `separator` / `unknown`，另有可选 `itemCount`。非分隔符节点携带与列举端点一致的状态字段。

遍历在深度与每节点子项数上都有上限，任何裁剪都会上报：`popup` 节点同时给出 `childCount`（宿主真实子项数）与 `childrenReturned`（本次响应实际包含的数量）；子树被裁剪的节点带 `truncated`，并由 `depthExceeded` / `childrenExceeded` 说明原因。标记向上传播，因此顶层 `truncated` 覆盖整棵树，`maxDepth` 与 `maxChildrenPerNode` 回显生效上限。

```javascript
const result = await fb.discovery.getContextMenuTree();
if (result.truncated) {
    console.warn('菜单树被裁剪', result.depthExceeded, result.childrenExceeded);
}
```

### getDspEntries()

签名：`fb.discovery.getDspEntries(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `discovery.getDspEntries` 调用结果。

```javascript
const result = await fb.discovery.getDspEntries();
```

### getMainMenuGroups()

签名：`fb.discovery.getMainMenuGroups(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `discovery.getMainMenuGroups` 调用结果。

```javascript
const result = await fb.discovery.getMainMenuGroups();
```

### getOutputDevices()

签名：`fb.discovery.getOutputDevices(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `discovery.getOutputDevices` 调用结果。

```javascript
const result = await fb.discovery.getOutputDevices();
```

### getPreferencePages()

签名：`fb.discovery.getPreferencePages(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `discovery.getPreferencePages` 调用结果。

```javascript
const result = await fb.discovery.getPreferencePages();
```

### getUIElements()

签名：`fb.discovery.getUIElements(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `discovery.getUIElements` 调用结果。

```javascript
const result = await fb.discovery.getUIElements();
```

### searchCommands(query, options?)

签名：`fb.discovery.searchCommands(query: string, options?: { expandDynamic?: boolean; scope?: 'all' | 'mainmenu' | 'contextmenu'; includeHidden?: boolean }): Promise<DiscoverySearchCommandsResponse>`

搜索主菜单与右键菜单两侧的命令，返回原始 `query`、`results`、`count`，以及 `mainMenuHits` / `contextMenuHits`；名称、描述与菜单路径均按不区分大小写匹配。大小写折叠仅作用于 ASCII，UTF-8 多字节序列原样通过。

结果的 `type` 为 `mainmenu` 或 `contextmenu`。运行时子菜单展开项带有 `subGuid` 与 `isDynamic: true`，动态父槽位会被跳过。每条结果还携带列举端点的状态字段，调用方无需再发一次请求即可判断能否执行。

传 `{ scope: 'mainmenu' }` 或 `{ scope: 'contextmenu' }` 可只搜单一菜单族，`{ expandDynamic: false }` 只搜静态注册表，`{ includeHidden: true }` 则连宿主不显示的条目一并搜索。

搜索包含右键菜单但无选中且无播放曲目时，响应的 `stateKnown` 为 `false`，此时结果里的 `enabled` / `checked` 不得用于过滤。

```javascript
const result = await fb.discovery.searchCommands('lyric', { scope: 'mainmenu' });
const hit = result.results[0];
await fb.discovery.executeMainMenuCommand(hit.guid, hit.subGuid);
```

<!-- END AUTO-GENERATED SDK STUBS -->
