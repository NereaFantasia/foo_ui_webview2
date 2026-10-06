# fb.config 配置 

## get(key, defaultValue?) 

获取配置值。返回 `{success, key, value, found}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| key | string | 配置键名 |
| defaultValue | JsonValue（可选） | 键不存在时用来回答的值，作为宿主的 `default` 发出；为 `undefined` 时不发 |

> 键不存在不算错误：`found` 为 `false`，`value` 为 `defaultValue`，没给默认值时为 `null`。`0`、`false`、`''` 这类假值默认值照样发出。

```javascript
const result = await fb.config.get('theme');
if (result.success === false) throw new Error(result.error);
console.log(result.value);   // 'dark'
console.log(result.found);   // true

const layout = await fb.config.get('layout', { columns: 3 });
if (layout.success === false) throw new Error(layout.error);
console.log(layout.found);   // 键不存在时为 false
console.log(layout.value);   // 键不存在时为 { columns: 3 }
```

## set(key, value) 

设置配置值（持久化存储）。

```javascript
await fb.config.set('theme', 'dark');
await fb.config.set('volume', 0.8);
```

## remove(key) 

删除配置项。

```javascript
await fb.config.remove('theme');
```

## getAll() 

读取整个存储。返回 `{ items, configs, count }`：`items` 是每个键及其值，`configs` 是同一个映射的旧名字，`count` 是键数。存储读不出来时以 `OPERATION_FAILED` 失败。

```javascript
const res = await fb.config.getAll();
if (res.success === false) throw new Error(res.error);
for (const [key, value] of Object.entries(res.items)) console.log(key, value);
```

## export() 

读取整个存储，同时给出映射与 JSON 文本。返回 `{ data, json, count }`：`data` 与 `getAll()` 的 `items` 是同一个映射，`json` 是把 `data` 序列化成的一段紧凑 JSON 文本。存储读不出来时以 `OPERATION_FAILED` 失败。

```javascript
const res = await fb.config.export();
if (res.success === false) throw new Error(res.error);
await navigator.clipboard.writeText(res.json);
```

## 系统信息 

### getVersionInfo() 

获取 foobar2000 版本信息。返回 `{version, versionFull, is64bit, isPortable, plugin, profilePath}`。

```javascript
const ver = await fb.config.getVersionInfo();
if (ver.success === false) throw new Error(ver.error);
console.log(ver.versionFull, ver.is64bit ? 'x64' : 'x86');
```

### getComponents() 

获取已加载的组件列表。返回 `{ components, count }`，`components` 为 `[{name, version, fileName}, ...]`；失败时返回失败信封。

```javascript
const res = await fb.config.getComponents();
if (res.success === false) throw new Error(res.error);
console.log(res.components.length);
```

## 输出设备 

### getOutputDevices() 

获取可用输出设备列表。返回 `{ devices, count }`，每个设备为 `{name, outputId, deviceId, isCurrent}`。

### getOutputConfig() 

报告当前生效的输出设置 `OutputConfig`：`outputId` 与 `deviceId`（形如 `{...}` 的 GUID）、`bufferLength`（秒）、`bitDepth`（位）、`useDither`、`useFades`，以及可选的 `outputName`、`deviceName`。没有已安装的模块对应 `outputId` 时不带 `outputName`，模块不给设备命名时不带 `deviceName`。

```javascript
const out = await fb.config.getOutputConfig();
if (out.success === false) throw new Error(out.error);
console.log(out.deviceName ?? out.deviceId, `${out.bufferLength * 1000} ms`);
```

### setOutputDevice(outputId, deviceId) 

设置输出设备。返回 `{success}`。

### setOutputBuffer(ms) 

设置输出缓冲区大小。返回 `{success}`。

```javascript
const res = await fb.config.getOutputDevices();
if (res.success === false) throw new Error(res.error);
const current = res.devices.find(d => d.isCurrent);
const first = res.devices[0];
if (first) await fb.config.setOutputDevice(first.outputId, first.deviceId);
await fb.config.setOutputBuffer(1000); // 毫秒
```

## 高级配置 

### getAdvancedConfig() 

获取所有高级配置项。返回 `{ entries, count }`，`entries` 为 `[{guid, name, type, value, defaultValue?, children?}, ...]`。

### getAdvancedConfigValue(guid) 

获取指定高级配置值。返回 `{guid, value, type}`。

### setAdvancedConfigValue(guid, value) 

设置高级配置值。返回 `{success}`。

### resetAdvancedConfig(guid) 

重置高级配置项为默认值。返回 `{success}`。

```javascript
const all = await fb.config.getAdvancedConfig();
const val = await fb.config.getAdvancedConfigValue('{some-guid}');
await fb.config.setAdvancedConfigValue('{some-guid}', 'new-value');
await fb.config.resetAdvancedConfig('{some-guid}');
```

## 偏好设置 

### getPreferencesPages() 

列出 foobar2000 的全部首选项页面与分支。返回 `{ pages, count }`，`pages` 是 `PreferencesPage[]`，先列所有页面、再列所有分支。每项有 `name`、`guid`、`parentGuid`（上级页面或分支的 GUID）与 `sortPriority`（越小越靠前，`0` 按名称排序）；分支另带 `isBranch: true`。

### getPreferencesStandardGuids() 

报告 foobar2000 标准首选项页面的 GUID，形如 `{...}`，共 17 个键，例如 `root`、`core`、`display`、`playback`、`output`、`mediaLibrary`、`advanced`、`tools`、`dsp`、`keyboardShortcuts`。顶层页面的 `parentGuid` 是其中之一。

```javascript
const [pagesRes, std] = await Promise.all([
  fb.config.getPreferencesPages(),
  fb.config.getPreferencesStandardGuids(),
]);
if (pagesRes.success === false || std.success === false) throw new Error('preferences unavailable');
const displayPages = pagesRes.pages.filter((p) => p.parentGuid === std.display);
```

## 媒体库配置 

### getLibraryStatus() 

报告媒体库状态 `LibraryStatus`：`enabled`（至少配置了一个媒体库文件夹）、`initialized`（是否已载入完成，宿主无法判断时为 `true`）与 `itemCount`。`itemCount` 在每次调用时遍历整个媒体库计数，大库上不宜频繁调用。

### getLibraryFilePatterns() 

报告 foobar2000 把新编码、复制或移动的文件放到哪里。返回可选的 `tracks`（曲目）与 `images`（专辑图片），各为 `{ directory, format }`：`directory` 是目标文件夹，`format` 是其下子文件夹与文件名所用的标题格式化模式。没有配置的模式不出现，两者都没配置时结果只有 `success: true`。

### showLibraryPreferences() 

打开 foobar2000 的媒体库首选项页面。

```javascript
const status = await fb.config.getLibraryStatus();
if (status.success === false) throw new Error(status.error);
if (!status.enabled) {
  await fb.config.showLibraryPreferences();
} else {
  const patterns = await fb.config.getLibraryFilePatterns();
  if (patterns.success && patterns.tracks) console.log(patterns.tracks.directory);
}
```

## DSP 预设 

### getDspPresets() 

获取所有 DSP 预设列表。返回 `{ presets, count }`，`presets` 为 `[{index, name}, ...]`。

### getActiveDspPreset() 

报告当前选中的 DSP 预设。返回 `{ index, name, isActive }`；没有选中或预设不可用时 `index` 与 `name` 为 `null`，`isActive` 为 `false`。

### setActiveDspPreset(index) 

按索引选中一个 DSP 预设，用它整条替换当前 DSP 链。选中之后无法再回到「未选中」。

```javascript
const res = await fb.config.getDspPresets();
if (res.success === false) throw new Error(res.error);
console.log(res.presets.map(p => p.name));
await fb.config.setActiveDspPreset(0);

const active = await fb.config.getActiveDspPreset();
if (active.success && active.isActive) console.log(active.name);
```

### getCursorFollowPlayback() / setCursorFollowPlayback(enabled) 

读写 foobar2000 的「光标跟随播放」设置。getter 返回 `{ enabled, value }`，`value` 是 `enabled` 的旧名字；setter 返回写入的 `enabled`。设置改变时广播 `playback:cursorFollowChanged`，载荷为 `{ enabled }`。

### getPlaybackFollowCursor() / setPlaybackFollowCursor(enabled) 

读写 foobar2000 的「播放跟随光标」设置，返回形状同上。设置改变时广播 `playback:followCursorChanged`，载荷为 `{ enabled }`。

### getReplaygainMode() / setReplaygainMode(mode) 

获取/设置 ReplayGain 模式。

| 值 | 模式 |
| --- | --- |
| 0 | None |
| 1 | Track |
| 2 | Album |
| 3 | Track/Album by playback order |

```javascript
const r = await fb.config.getReplaygainMode();
if (r.success === false) throw new Error(r.error);
console.log(r.mode); // 0
await fb.config.setReplaygainMode(2); // Album 模式

const cursor = await fb.config.getCursorFollowPlayback();
if (cursor.success && !cursor.enabled) await fb.config.setCursorFollowPlayback(true);
const follow = await fb.config.getPlaybackFollowCursor();
if (follow.success && follow.enabled) await fb.config.setPlaybackFollowCursor(false);
```

