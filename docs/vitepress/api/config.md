# Config API 

配置存储、系统信息、输出设备、高级配置、DSP 预设、播放行为配置。共 29 个 API。

## 配置存储 

### config.get 

获取配置值。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| key | string | ✓ | 配置键名 |
| default | any | ✗ | 默认值 |

**返回值**: `{ "success": true, "key": "theme", "value": "dark", "found": true }`

> 键不存在时 `found` 为 `false`，若提供了 `default` 参数则返回默认值。

```javascript
const theme = await fb2k.invoke('config.get', { key: 'theme', default: 'light' });
```

### config.set 

设置配置值。持久化存储在 foobar2000 配置系统中。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| key | string | ✓ | 配置键名 |
| value | any | ✓ | 配置值 |

**返回值**: `{ "success": true, "key": "volume" }`

```javascript
await fb2k.invoke('config.set', { key: 'volume', value: 0.8 });
```

### config.remove 

删除配置项。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| key | string | ✓ | 配置键名 |

**返回值**: `{ "success": true, "key": "theme", "existed": true }`

### config.getAll 

获取所有配置项。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "items": {
        "theme": "dark",
        "volume": 0.8
    },
    "configs": { },
    "count": 2
}
```

> `items` 和 `configs` 内容相同（后者为兼容别名）。

```javascript
const cfg = await fb2k.invoke('config.getAll');
console.log(`配置项数: ${cfg.count}`, cfg.items);
```

### config.export 

导出所有配置为 JSON。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "data": { "theme": "dark" },
    "json": "{\\"theme\\":\\"dark\\"}",
    "count": 1
}
```

> `data` 为 JSON 对象，`json` 为序列化字符串。

```javascript
const exported = await fb2k.invoke('config.export');
localStorage.setItem('fb2k_backup', exported.json);
```

## 系统信息 

### config.getVersionInfo 

获取 foobar2000 版本信息。

- **参数**: 无

**返回值**:

```json
{
    "version": "foobar2000 v2.0",
    "foobar2000": "foobar2000 v2.0",
    "versionFull": "foobar2000 v2.0 (x64)",
    "is64bit": true,
    "isPortable": false,
    "plugin": {
        "name": "foo_ui_webview2",
        "version": "1.0.0"
    },
    "profilePath": "C:\\\\Users\\\\user\\\\AppData\\\\Roaming\\\\foobar2000-v2"
}
```

```javascript
const info = await fb2k.invoke('config.getVersionInfo');
console.log(`${info.versionFull} | ${info.is64bit ? '64-bit' : '32-bit'}`);
```

### config.getComponents 

获取已安装的组件列表。返回数组（不是对象）。

- **参数**: 无

**返回值**:

```json
[
    {
        "name": "foo_ui_webview2",
        "version": "1.1.0",
        "fileName": "foo_ui_webview2.dll",
        "filename": "foo_ui_webview2.dll"
    }
]
```

> `fileName` 和 `filename` 相同（后者为兼容别名）。

### config.getOutputDevices 

获取可用的音频输出设备。返回数组。

- **参数**: 无

**返回值**:

```json
[
    {
        "name": "DS: Speakers (Realtek)",
        "id": "{GUID}",
        "outputId": "{GUID}",
        "deviceId": "{GUID}",
        "isCurrent": true
    }
]
```

```javascript
const devices = await fb2k.invoke('config.getOutputDevices');
const current = devices.find(d => d.isCurrent);
console.log(`当前输出: ${current.name}`);
```

### config.setOutputDevice 

设置音频输出设备。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| outputId | string | ✓ | 输出模块 ID |
| deviceId | string | ✓ | 设备 ID |

```javascript
await fb2k.invoke('config.setOutputDevice', { outputId: '{...}', deviceId: '{...}' });
```

### config.getOutputConfig 

获取当前输出配置。

- **参数**: 无

**返回值**:

```json
{
    "outputId": "{GUID}",
    "deviceId": "{GUID}",
    "outputName": "DS",
    "deviceName": "Speakers",
    "bufferLength": 1.0,
    "bitDepth": 0,
    "useDither": false,
    "useFades": true
}
```

### config.setOutputBuffer 

设置输出缓冲区大小。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| bufferLength | number | ✓ | 缓冲区大小（秒，0.05-2.0），兼容别名: milliseconds（自动转换） |

**返回值**: `{ "success": true }`

## 高级配置 

### config.getAdvancedConfig 

获取高级配置项树。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| parentGuid | string | ✗ | 父节点 GUID（默认为根节点） |

**返回值**: 数组，每个元素包含 `name`、`guid`、`type`（`"branch"`/`"checkbox"`/`"radio"`/`"string"`/`"integer"`）、`value`、`children`（分支时）。

### config.getAdvancedConfigValue 

获取指定高级配置项的值。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| guid | string | ✓ | 配置项 GUID |

**返回值**: `{ "name": "...", "guid": "...", "type": "checkbox", "value": true }`

### config.setAdvancedConfigValue 

设置指定高级配置项的值。checkbox 类型要求 boolean，string/integer 类型要求 string。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| guid | string | ✓ | 配置项 GUID |
| value | any | ✓ | 新值（类型必须与配置项匹配） |

**返回值**: `{ "success": true }`

### config.resetAdvancedConfig 

重置指定高级配置项为默认值。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| guid | string | ✓ | 配置项 GUID |

**返回值**: `{ "success": true }`

```javascript
// 获取、修改、重置高级配置项
const val = await fb2k.invoke('config.getAdvancedConfigValue', { guid: '{...}' });
await fb2k.invoke('config.setAdvancedConfigValue', { guid: '{...}', value: true });
await fb2k.invoke('config.resetAdvancedConfig', { guid: '{...}' });
```

## 偏好设置 

### config.getPreferencesPages 

获取所有偏好设置页面列表，包含页面和分支。

- **参数**: 无

**返回值**: 数组，每项包含 `name`、`guid`、`parentGuid`、`sortPriority`，分支额外包含 `isBranch: true`。

### config.getPreferencesStandardGuids 

获取标准偏好设置页面的 GUID 列表。

- **参数**: 无

**返回值**:

```json
{
    "root": "{GUID}",
    "core": "{GUID}",
    "display": "{GUID}",
    "playback": "{GUID}",
    "output": "{GUID}",
    "mediaLibrary": "{GUID}",
    "advanced": "{GUID}",
    "components": "{GUID}",
    "dsp": "{GUID}",
    "tagging": "{GUID}",
    "tagWriting": "{GUID}",
    "input": "{GUID}",
    "visualisations": "{GUID}",
    "shell": "{GUID}",
    "keyboardShortcuts": "{GUID}",
    "tools": "{GUID}",
    "hidden": "{GUID}"
}
```

## 媒体库配置 

### config.getLibraryStatus 

获取媒体库状态。

- **参数**: 无

**返回值**:

```json
{
    "enabled": true,
    "itemCount": 5000,
    "initialized": true
}
```

### config.getLibraryFilePatterns 

获取媒体库新文件模式配置。

- **参数**: 无

**返回值**:

```json
{
    "tracks": { "directory": "...", "format": "..." },
    "images": { "directory": "...", "format": "..." }
}
```

### config.showLibraryPreferences 

打开媒体库偏好设置页。

- **参数**: 无
- **返回值**: `{ "success": true }`

## DSP 预设 

### config.getDspPresets 

获取所有 DSP 预设列表。

- **参数**: 无

**返回值**: 数组，每项包含 `index` 和 `name`。

```javascript
const presets = await fb2k.invoke('config.getDspPresets');
presets.forEach(p => console.log(`${p.index}: ${p.name}`));
```

### config.getActiveDspPreset 

获取当前激活的 DSP 预设。

- **参数**: 无

**返回值**: `{ "index": 0, "name": "My Preset", "isActive": true }`

> 无激活预设时 `index` 和 `name` 为 `null`，`isActive` 为 `false`。

### config.setActiveDspPreset 

设置激活的 DSP 预设。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | number | ✓ | 预设索引 |

**返回值**: `{ "success": true }`

## 播放行为配置 

### config.getCursorFollowPlayback 

获取「光标跟随播放」设置。

- **参数**: 无

**返回值**: `{ "enabled": true, "value": true }`

### config.setCursorFollowPlayback 

设置「光标跟随播放」。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| enabled | boolean | ✓ | 是否启用 |

**返回值**: `{ "success": true, "enabled": true }`

### config.getPlaybackFollowCursor 

获取「播放跟随光标」设置。

- **参数**: 无

**返回值**: `{ "enabled": false, "value": false }`

### config.setPlaybackFollowCursor 

设置「播放跟随光标」。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| enabled | boolean | ✓ | 是否启用 |

**返回值**: `{ "success": true, "enabled": false }`

### config.getReplaygainMode 

获取 ReplayGain source mode。

- **参数**: 无

**返回值**: `{ "mode": 0, "value": 0 }`

| mode 值 | 含义 |
| --- | --- |
| 0 | none |
| 1 | track |
| 2 | album |
| 3 | byPlaybackOrder (auto) |

### config.setReplaygainMode 

设置 ReplayGain source mode。支持数字索引或字符串名称。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| mode | number | - | source mode 索引 (0-3)，兼容别名: value |
| sourceMode | string | - | "none" / "track" / "album" / "auto" / "byPlaybackOrder"（"auto" 与 "byPlaybackOrder" 等价） |

```javascript
await fb2k.invoke('config.setReplaygainMode', { mode: 1 });
// 或
await fb2k.invoke('config.setReplaygainMode', { sourceMode: 'track' });
```
