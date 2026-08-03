# 队列与发现

涵盖 `fb.queue`、`fb.jitQueue`、`fb.discovery`、`fb.keyboard` 四个命名空间。

## fb.queue 播放队列

### get()

获取当前播放队列内容。

```javascript
const q = await fb.queue.get();
// q.items: [{ path, playlist, index }, ...]
```

### getCount()

`fb.queue.getCount(): Promise<{ count: number }>` 获取队列中的曲目数量，返回 `{ count }`。

### add(options)

向队列添加曲目。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options | object | 包含 playlist、index 等标识信息 |

```javascript
await fb.queue.add({ playlist: 0, index: 5 });
```

### addPaths(paths, options?)

通过路径添加曲目到队列。

```javascript
await fb.queue.addPaths(['E:\\Music\\song.flac']);
```

### remove(index)

移除队列中指定位置的曲目。

```javascript
await fb.queue.remove(0); // 移除队列第一项
```

### moveToTop(index)

将队列中指定项移到顶部。

```javascript
await fb.queue.moveToTop(3);
```

### flush()

`fb.queue.flush()` 通过 `queue.flush` 清空宿主播放队列，返回值可能包含 `clearedCount`。

### clear()

清空播放队列。

```javascript
await fb.queue.clear();
```

## fb.jitQueue JIT 即时队列

JIT 队列是一种即时播放机制，插入的曲目会在当前曲目结束后立即播放。

### getState()

获取 JIT 队列状态。

```javascript
const state = await fb.jitQueue.getState();
```

### enqueueNext(options)

将曲目排入"下一首播放"。

```javascript
await fb.jitQueue.enqueueNext({ playlist: 0, index: 3 });
```

### playNow(options)

立即播放指定曲目。

```javascript
await fb.jitQueue.playNow({ playlist: 0, index: 5 });
```

### preloadBatch(options)

`fb.jitQueue.preloadBatch(options)` 从可选的 `startIndex` 开始预加载 `urls`；`replace` 决定是否替换现有预加载缓冲区。超过 2048 字符的 URL 会被跳过，并计入 `invalidCount`。

```javascript
await fb.jitQueue.preloadBatch({
	urls: ['https://example.invalid/audio/one.flac'],
	startIndex: 0,
	replace: true,
});
```

### skip()

跳过当前 JIT 队列项。

### stop()

停止 JIT 队列播放。

### clear()

清空 JIT 队列。

### notifyEmpty()

通知 JIT 队列已清空（用于回调通知）。

### JIT 队列事件

通过 `fb.on()` 订阅 `jitQueue:needNext`、`jitQueue:trackChanged`、`jitQueue:listExhausted`、`jitQueue:preloadComplete` 和 `jitQueue:error`。对应的 payload 类型均从包根导出。

```javascript
const off = fb.on('jitQueue:needNext', ({ currentTrackId, reason }) => {
	console.log(currentTrackId, reason);
});
```

## fb.discovery 服务发现

提供 foobar2000 内部服务、菜单命令、组件的枚举与执行能力。

### getAllServices()

获取所有已注册的服务列表。

```javascript
const { discovery } = fb;
const r = await discovery.getAllServices();
```

### getMainMenuCommands(options?)

获取所有主菜单命令。默认会展开运行时构建子菜单的组件（`mainmenu_commands_v2`，如 ESLyric），因此结果中除父命令外还包含其子命令；传 `{ expandDynamic: false }` 则只返回静态注册表。

```javascript
const cmds = await fb.discovery.getMainMenuCommands();
// cmds.commands: [{ guid, subGuid?, name, path, isDynamic, kind }, ...]
// cmds.dynamicCount: 展开出来的子命令数量
```

### getMainMenuGroups()

获取主菜单分组。仅枚举静态注册的 `mainmenu_group` 服务；运行时子菜单是命令节点树，不在此列表中，请用 `getMainMenuCommands()`。

### executeMainMenuCommand(guid, subGuid?)

执行指定 GUID 的主菜单命令。对展开自动态子菜单的条目，必须同时传该条目的 `subGuid`，否则只会派发静态父命令。

```javascript
await fb.discovery.executeMainMenuCommand('{xxxxxxxx-...}');

// 动态子命令：guid + subGuid 成对使用
const lyric = cmds.commands.find((c) => c.isDynamic && c.name.includes('ESLyric'));
await fb.discovery.executeMainMenuCommand(lyric.guid, lyric.subGuid);
```

### getContextMenuCommands()

枚举所有可用的右键菜单命令。

### executeContextMenuCommand(options)

执行右键菜单命令。

```javascript
const { discovery } = fb;
await discovery.executeContextMenuCommand({ guid: '...' });
```

### executeContextMenuByPath(options)

通过菜单路径执行右键菜单命令。省略 `trackPath` 时作用于当前正在播放/选择的曲目。

```javascript
await fb.discovery.executeContextMenuByPath({
    path: 'Properties',
    trackPath: 'C:\\Music\\song.flac',
});
```

### getContextMenuTree()

获取当前上下文（正在播放/选择）的右键菜单树。

### getInputFormats()

获取所有支持的输入格式（解码器）。

```javascript
const r = await fb.discovery.getInputFormats();
// r.formats: [{ name, extensions }, ...]
```

### getComponents()

获取所有已加载的组件列表，返回 `{ components, count }`。

```javascript
const components = await fb.discovery.getComponents();
```

### getUIElements()

获取所有 UI 元素。

### getDspEntries()

获取所有 DSP 插件条目。

### getOutputDevices()

获取所有输出设备。

### getPreferencePages()

获取所有偏好设置页面。

### searchCommands(query)

搜索菜单命令。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| query | string | 搜索关键词 |

```javascript
const { discovery } = fb;
const r = await discovery.searchCommands('convert');
```

## fb.keyboard 全局快捷键

### registerHotkey(key, action, options?)

注册全局快捷键。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| key | string | 快捷键组合，如 'Ctrl+Shift+P' |
| action | string | 触发的动作标识 |
| options | object | 可选配置 |

```javascript
await fb.keyboard.registerHotkey('Ctrl+Shift+P', 'playPause');
```

### registerShortcut(key, action)

注册快捷键（简化版）。

```javascript
await fb.keyboard.registerShortcut('Space', 'toggle');
```

### unregisterHotkey(options)

注销快捷键。

```javascript
await fb.keyboard.unregisterHotkey({ key: 'Ctrl+Shift+P' });
```

### getRegisteredHotkeys()

获取所有已注册的快捷键列表。

```javascript
const r = await fb.keyboard.getRegisteredHotkeys();
```

### keyboard:hotkey

`keyboard:hotkey` 事件携带 `KeyboardHotkeyPayload`，其中包含数字 `id`、`key` 和 `action`。

```javascript
fb.on('keyboard:hotkey', ({ id, key, action }) => {
	console.log(id, key, action);
});
```
