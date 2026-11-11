# fb.config 配置

## get(key, defaultValue?)

获取配置值。返回 `{success, key, value, found}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| key | string | 配置键名 |
| defaultValue | JsonValue（可选） | 键不存在时返回的默认值，作为宿主参数 `default` 发送；为 `undefined` 时不传递 |

> 键不存在不视为调用失败：此时 `found` 为 `false`，`value` 返回 `defaultValue`；未指定默认值时返回 `null`。`0`、`false` 与 `''` 等假值（falsy）默认值均正常回传。

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

读取所有配置项。返回 `{ items, configs, count }`：`items` 为键值映射表，`configs` 为向后兼容别名，`count` 为键总数。存储读取失败时返回 `OPERATION_FAILED`。

```javascript
const res = await fb.config.getAll();
if (res.success === false) throw new Error(res.error);
for (const [key, value] of Object.entries(res.items)) console.log(key, value);
```

## export()

读取所有配置项并返回对象映射与序列化 JSON 文本。返回 `{ data, json, count }`：`data` 等同于 `getAll()` 的 `items` 映射，`json` 为 `data` 序列化后的紧凑 JSON 文本。存储读取失败时返回 `OPERATION_FAILED`。

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

获取当前生效的输出配置 `OutputConfig`：`outputId` 与 `deviceId`（GUID 字符串）、`bufferLength`（秒）、`bitDepth`（位深度）、`useDither`、`useFades`，以及可选的 `outputName`、`deviceName`。若未安装对应 `outputId` 的输出插件则省略 `outputName`；若插件未为设备命名则省略 `deviceName`。

```javascript
const out = await fb.config.getOutputConfig();
if (out.success === false) throw new Error(out.error);
console.log(out.deviceName ?? out.deviceId, `${out.bufferLength * 1000} ms`);
```

### setOutputDevice(outputId, deviceId)

设置输出设备。返回 `{success}`。

### setOutputBuffer(ms)

设置输出缓冲区大小。传入数字时解析为毫秒。对象参数形式接受 `milliseconds` 或原生的 `bufferLength`（秒）；若两者同时传入，宿主优先采用 `milliseconds`。解析后的有效取值范围为 `0.05` 至 `2.0` 秒。返回 `{success}`。

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

列出 foobar2000 的全部首选项页面与分支。返回 `{ pages, count }`，其中 `pages` 为 `PreferencesPage[]` 数组，首部列出所有常规页面，尾部列出所有分支。每个条目包含 `name`、`guid`、`parentGuid`（所属上级页面或分支的 GUID）与 `sortPriority`（数值越小排序越靠前，`0` 表示按名称排序）；分支条目额外包含 `isBranch: true`。

### getPreferencesStandardGuids()

获取 foobar2000 标准首选项页面的内置 GUID 集合（包含 17 个预置键，如 `root`、`core`、`display`、`playback`、`output`、`mediaLibrary`、`advanced`、`tools`、`dsp`、`keyboardShortcuts`）。顶层页面的 `parentGuid` 对应其中之一。

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

获取 foobar2000 针对新转码、复制或移动文件的路径归档规则。返回可选的 `tracks`（曲目）与 `images`（封面图片）规则，结构均为 `{ directory, format }`：`directory` 为目标基础目录，`format` 为其下子文件夹与文件名的标题格式化模式。未配置对应规则时字段缺省；两者均未配置时结果仅包含 `success: true`。

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

按索引激活指定的 DSP 预设，使用该预设完整替换当前的活跃 DSP 链。选中预设后不支持直接恢复为“无预设”状态。

```javascript
const res = await fb.config.getDspPresets();
if (res.success === false) throw new Error(res.error);
console.log(res.presets.map(p => p.name));
await fb.config.setActiveDspPreset(0);

const active = await fb.config.getActiveDspPreset();
if (active.success && active.isActive) console.log(active.name);
```

### getCursorFollowPlayback() / setCursorFollowPlayback(enabled)

读取或设置 foobar2000 的「光标跟随播放」配置。读取方法返回 `{ enabled, value }`（`value` 为 `enabled` 的兼容别名）；设置方法返回写入的 `enabled`。配置项变更时广播 `playback:cursorFollowChanged` 事件并携带 `{ enabled }`。

### getPlaybackFollowCursor() / setPlaybackFollowCursor(enabled)

读取或设置 foobar2000 的「播放跟随光标」配置，返回值结构同上。配置项变更时广播 `playback:followCursorChanged` 事件并携带 `{ enabled }`。

### getReplaygainMode() / setReplaygainMode(mode)

获取或设置 ReplayGain 模式。读取方法返回数值 `mode` 与兼容别名 `value`。设置方法接受 `0..3` 数值，或 `'none' | 'track' | 'album' | 'byPlaybackOrder' | 'auto'` 字符串枚举（其中 `'auto'` 解析为模式 3）。

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

