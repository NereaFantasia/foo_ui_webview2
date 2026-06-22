# 菜单与杂项

涵盖 `fb.menu`、`fb.console`、`fb.log`、`fb.lyrics`、`fb.notification`、`fb.panel`、`fb.misc`、`fb.dnd` 八个命名空间。

## fb.menu 菜单命令 

### getMainMenu(root?) 

获取主菜单树。可选 `root` 参数限定子树范围。

```javascript
const menu = await fb.menu.getMainMenu();
const fileMenu = await fb.menu.getMainMenu('File');
```

### getContextMenu(options?) 

获取右键菜单。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options.mode | string | 'auto' / 'playlist' / 'nowPlaying' / 'handles' |
| options.handles | array | mode 为 'handles' 时提供 |

```javascript
const ctx = await fb.menu.getContextMenu({ mode: 'nowPlaying' });
```

### runMainMenuCommand(command) 

执行主菜单命令（按路径）。

```javascript
await fb.menu.runMainMenuCommand('File/Preferences');
```

### runContextCommand(command) 

执行右键菜单命令。使用默认上下文项。

```javascript
await fb.menu.runContextCommand('Properties');
```

### runContextCommandById(id, options?) 

通过 ID 执行右键菜单命令。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| id | number | 命令 ID |
| options | object | 可选，{ mode, handles } |

### showNativePopup(options) 

显示原生弹出菜单。

```javascript
await fb.menu.showNativePopup({ x: 100, y: 200, items: [...] });
```

## fb.console 控制台 

输出到 foobar2000 控制台。

### log(message) 

```javascript
fb.console.log('调试信息');
```

### warn(message) 

```javascript
fb.console.warn('警告信息');
```

### error(message) 

```javascript
fb.console.error('错误信息');
```

## fb.log 日志文件 

### write(message, options?) 

写入日志文件。

```javascript
await fb.log.write('操作记录', { level: 'info' });
```

### read(lines?) 

读取日志。可选指定行数。

```javascript
const r = await fb.log.read(100); // 最近100行
```

### clear() 

清空日志文件。

```javascript
await fb.log.clear();
```

## fb.lyrics 歌词 

### get(path?, options?) 

获取歌词。不传 `path` 时获取当前播放曲目的歌词。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 可选，文件路径（空则取当前播放曲目） |
| options | object | 可选，筛选参数 |
| options.source | string | 'embedded' / 'file' / 'any'（默认） |
| options.type | string | 'synced' / 'unsynced' / 'any'（默认） |
| options.format | string | 'lrc' / 'txt' / 'any'（默认，仅 source=file 时生效） |

```javascript
const r = await fb.lyrics.get();                                 // 当前曲目
const r2 = await fb.lyrics.get(path, { source: 'embedded' });   // 仅嵌入歌词
const r3 = await fb.lyrics.get(undefined, { type: 'synced' });  // 仅同步歌词
```

### exists(path) 

检查文件是否存在歌词。

```javascript
const r = await fb.lyrics.exists('E:\\\\Music\\\\song.flac');
console.log(r.exists);
```

### save(path, lyrics, options?) 

保存歌词到文件、标签或两者。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 文件路径 |
| lyrics | string | 歌词文本 |
| options.target | string \| string[] | 'file' / 'embedded' / 'config' / 'all'，或数组 ['file','config'] |
| options.filename | string | 可选，自定义文件名 |
| options.tagName | string | 嵌入标签名（默认 "LYRICS"，仅 target 含 embedded） |
| options.format | string | 'lrc'（默认）/ 'txt'（仅 target 含 file/config） |

```javascript
await fb.lyrics.save('E:\\\\Music\\\\song.flac', '[00:00.00]歌词内容...');
await fb.lyrics.save(path, text, { target: 'all' });             // 三合一：文件+标签+配置文件夹
await fb.lyrics.save(path, text, { target: 'config' });          // 保存到 %profile%\\lyrics\\
await fb.lyrics.save(path, text, { target: ['file', 'config'] }); // 数组组合
await fb.lyrics.save(path, text, { target: 'embedded', tagName: 'SYNCEDLYRICS' });
```

> 返回值与 `lyrics.save` API 保持一致：单目标返回扁平结果，多目标返回 `{success, results:{file, embedded, config}}`。

## fb.notification 通知 

### show(options) 

显示通知。

```javascript
await fb.notification.show({ title: '提示', message: '操作完成' });
```

### hide() 

隐藏当前通知。

### showCustomMenu(options) 

显示自定义菜单。

```javascript
await fb.notification.showCustomMenu({
    items: [
        { text: '选项A', id: 'a' },
        { text: '选项B', id: 'b' }
    ]
});
```

### showToast(options) 

显示 Toast 提示。

```javascript
await fb.notification.showToast({ message: '已添加到播放列表' });
```

## fb.panel 面板配置 

### getConfig() 

获取当前面板配置。

```javascript
const config = await fb.panel.getConfig();
```

### setConfig(options) 

设置面板配置。

```javascript
await fb.panel.setConfig({ theme: 'dark', layout: 'compact' });
```

## fb.misc 杂项工具 

### exit() 

退出 foobar2000。

### restart() 

重启 foobar2000。

```javascript
await fb.misc.restart();
```

### getComponentPath() 

获取组件 DLL 所在路径。

```javascript
const r = await fb.misc.getComponentPath();
console.log(r.path); // 组件路径
```

### getFoobarPath() 

获取 foobar2000.exe 所在路径。

### getProfilePath() 

获取配置文件目录路径。

### showConsole() 

显示 foobar2000 控制台窗口。

### showLibrarySearch(query?) 

打开媒体库搜索。可选传入初始查询。

```javascript
await fb.misc.showLibrarySearch('artist IS Beatles');
```

### showPopupMessage(message, title?) 

显示弹出消息框。

```javascript
await fb.misc.showPopupMessage('操作完成', '提示');
```

### showPreferences() 

打开 foobar2000 偏好设置。

## fb.dnd 拖放 

### registerDropZone(options) 

注册拖放区域。

```javascript
await fb.dnd.registerDropZone({
    zoneId: 'my-zone',
    accept: ['audio/*'],
    element: '#drop-area'
});
```

### unregisterDropZone(zoneId) 

注销拖放区域。注意 C++ 端参数键为 `zoneId`。

```javascript
await fb.dnd.unregisterDropZone('my-zone');
```

### startDrag(type, options?) 

开始拖动操作。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| type | string | 拖动类型 |
| options | object | 可选配置 |

### getDropZones() 

获取所有已注册的拖放区域。

```javascript
const zones = await fb.dnd.getDropZones();
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### exit()

签名：`fb.misc.exit(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `misc.exit` 调用结果。

```javascript
const result = await fb.misc.exit();
```

### getFoobarPath()

签名：`fb.misc.getFoobarPath(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `misc.getFoobarPath` 调用结果。

```javascript
const result = await fb.misc.getFoobarPath();
```

### getProfilePath()

签名：`fb.misc.getProfilePath(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `misc.getProfilePath` 调用结果。

```javascript
const result = await fb.misc.getProfilePath();
```

### showConsole()

签名：`fb.misc.showConsole(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `misc.showConsole` 调用结果。

```javascript
const result = await fb.misc.showConsole();
```

### showPreferences()

签名：`fb.misc.showPreferences(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `misc.showPreferences` 调用结果。

```javascript
const result = await fb.misc.showPreferences();
```

<!-- END AUTO-GENERATED SDK STUBS -->
