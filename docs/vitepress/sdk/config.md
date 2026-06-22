# fb.config 配置 

## get(key) 

获取配置值。返回 `{success, key, value, found}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| key | string | 配置键名 |

> 键不存在时 `found` 为 `false`，`value` 为 `null`。底层 API 支持 `default` 参数，但 SDK 封装未暴露。

```javascript
const result = await fb.config.get('theme');
console.log(result.value);   // 'dark'
console.log(result.found);   // true
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

获取所有配置项。

## export() 

导出配置。

## 系统信息 

### getVersionInfo() 

获取 foobar2000 版本信息。返回 `{version, versionFull, is64bit, isPortable, plugin, profilePath}`。

```javascript
const ver = await fb.config.getVersionInfo();
console.log(ver.versionFull, ver.is64bit ? 'x64' : 'x86');
```

### getComponents() 

获取已加载的组件列表。返回 `[{name, version, fileName}, ...]`。

```javascript
const comps = await fb.config.getComponents();
```

## 输出设备 

### getOutputDevices() 

获取可用输出设备列表。返回数组 `[{name, outputId, deviceId, isCurrent}, ...]`。

### getOutputConfig() 

获取当前输出配置。返回 `{outputId, deviceId, bufferLength, ...}`。

### setOutputDevice(outputId, deviceId) 

设置输出设备。返回 `{success}`。

### setOutputBuffer(ms) 

设置输出缓冲区大小。返回 `{success}`。

```javascript
const devices = await fb.config.getOutputDevices();
const current = devices.find(d => d.isCurrent);
await fb.config.setOutputDevice(devices[0].outputId, devices[0].deviceId);
await fb.config.setOutputBuffer(1000); // 毫秒
```

## 高级配置 

### getAdvancedConfig() 

获取所有高级配置项。返回数组 `[{guid, name, type, value, defaultValue?, children?}, ...]`。

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

获取偏好设置页面列表。返回数组 `[{guid, name, parentGuid, sortPriority}, ...]`。

### getPreferencesStandardGuids() 

获取标准偏好设置页面的 GUID 列表。返回 `{root, core, display, playback, output, mediaLibrary, advanced, ...}`。

## 媒体库配置 

### getLibraryStatus() 

获取媒体库状态。返回 `{enabled, initialized, itemCount}`。

### getLibraryFilePatterns() 

获取媒体库文件模式。返回 `{tracks: {directory, format}, images: {directory, format}}`。

### showLibraryPreferences() 

打开媒体库偏好设置页。返回 `{success}`。

## DSP 预设 

### getDspPresets() 

获取所有 DSP 预设列表。返回数组 `[{index, name}, ...]`。

### getActiveDspPreset() 

获取当前活动的 DSP 预设。返回 `{index, name}`。

### setActiveDspPreset(index) 

设置活动的 DSP 预设。返回 `{success}`。

```javascript
const presets = await fb.config.getDspPresets();
console.log(presets.map(p => p.name));
await fb.config.setActiveDspPreset(0);
```

### getCursorFollowPlayback() / setCursorFollowPlayback(enabled) 

获取/设置"光标跟随播放"。

### getPlaybackFollowCursor() / setPlaybackFollowCursor(enabled) 

获取/设置"播放跟随光标"。

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
console.log(r.mode); // 0
await fb.config.setReplaygainMode(2); // Album 模式

await fb.config.setCursorFollowPlayback(true);
await fb.config.setPlaybackFollowCursor(false);
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### export()

签名：`fb.config.export(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.export` 调用结果。

```javascript
const result = await fb.config.export();
```

### getActiveDspPreset()

签名：`fb.config.getActiveDspPreset(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getActiveDspPreset` 调用结果。

```javascript
const result = await fb.config.getActiveDspPreset();
```

### getAll()

签名：`fb.config.getAll(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getAll` 调用结果。

```javascript
const result = await fb.config.getAll();
```

### getCursorFollowPlayback()

签名：`fb.config.getCursorFollowPlayback(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getCursorFollowPlayback` 调用结果。

```javascript
const result = await fb.config.getCursorFollowPlayback();
```

### getLibraryFilePatterns()

签名：`fb.config.getLibraryFilePatterns(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getLibraryFilePatterns` 调用结果。

```javascript
const result = await fb.config.getLibraryFilePatterns();
```

### getLibraryStatus()

签名：`fb.config.getLibraryStatus(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getLibraryStatus` 调用结果。

```javascript
const result = await fb.config.getLibraryStatus();
```

### getOutputConfig()

签名：`fb.config.getOutputConfig(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getOutputConfig` 调用结果。

```javascript
const result = await fb.config.getOutputConfig();
```

### getPlaybackFollowCursor()

签名：`fb.config.getPlaybackFollowCursor(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getPlaybackFollowCursor` 调用结果。

```javascript
const result = await fb.config.getPlaybackFollowCursor();
```

### getPreferencesPages()

签名：`fb.config.getPreferencesPages(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getPreferencesPages` 调用结果。

```javascript
const result = await fb.config.getPreferencesPages();
```

### getPreferencesStandardGuids()

签名：`fb.config.getPreferencesStandardGuids(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.getPreferencesStandardGuids` 调用结果。

```javascript
const result = await fb.config.getPreferencesStandardGuids();
```

### showLibraryPreferences()

签名：`fb.config.showLibraryPreferences(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `config.showLibraryPreferences` 调用结果。

```javascript
const result = await fb.config.showLibraryPreferences();
```

<!-- END AUTO-GENERATED SDK STUBS -->
