# 音频扩展

涵盖 `fb.dsp`、`fb.output`、`fb.replaygain` 三个命名空间。

## fb.dsp DSP 管理 

### getChain() 

获取当前 DSP 处理链。返回当前激活的 DSP 列表。

```javascript
const chain = await fb.dsp.getChain();
// chain.dsps: [{ guid, name, ... }, ...]
```

### setChain(dsps) 

设置 DSP 处理链。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| dsps | array | DSP 配置数组 |

```javascript
await fb.dsp.setChain([{ guid: '...', enabled: true }]);
```

### getPresets() 

获取所有 DSP 预设列表。

```javascript
const r = await fb.dsp.getPresets();
// r.presets: [{ name, index }, ...]
```

### applyPreset(indexOrName) 

应用 DSP 预设。支持按索引（number）或名称（string）。

```javascript
await fb.dsp.applyPreset(0);           // 按索引
await fb.dsp.applyPreset('My Preset'); // 按名称
```

### getAvailable() 

获取所有可用的 DSP 插件列表。

```javascript
const r = await fb.dsp.getAvailable();
// r.dsps: [{ guid, name }, ...]
```

### addDsp(guid, position?) 

添加 DSP 到处理链。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| guid | string | DSP 插件 GUID |
| position | number | 可选，插入位置（默认追加到末尾） |

```javascript
await fb.dsp.addDsp('{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}');
```

### removeDsp(index) 

从处理链移除指定位置的 DSP。

```javascript
await fb.dsp.removeDsp(2); // 移除第3个DSP
```

### moveDsp(from, to) 

移动 DSP 在处理链中的位置。

```javascript
await fb.dsp.moveDsp(0, 2); // 将第1个DSP移到第3位
```

## fb.output 输出设备 

### getDevices() 

获取所有可用的音频输出设备。

```javascript
const r = await fb.output.getDevices();
// r.devices: [{ id, name, isDefault }, ...]
```

### getEntries() 

获取输出插件条目。

```javascript
const r = await fb.output.getEntries();
```

### getSettings() 

获取当前输出设置。

```javascript
const settings = await fb.output.getSettings();
```

## fb.replaygain ReplayGain 

### get(paths) 

获取指定文件的 ReplayGain 信息。支持单个路径或路径数组。

```javascript
const r = await fb.replaygain.get('E:\\\\Music\\\\song.flac');
const r2 = await fb.replaygain.get(['song1.flac', 'song2.flac']);
```

### getMode() 

获取当前 ReplayGain 模式。

```javascript
const r = await fb.replaygain.getMode();
// r.sourceMode, r.processingMode
```

### setMode(sourceMode, processingMode?) 

设置 ReplayGain 模式。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| sourceMode | string | 来源模式 |
| processingMode | string | 可选，处理模式 |

```javascript
await fb.replaygain.setMode('track');
```

### getPreamp() 

获取 ReplayGain 前置增益。

```javascript
const r = await fb.replaygain.getPreamp();
// r.withRg, r.withoutRg (dB)
```

### setPreamp(withRg?, withoutRg?) 

设置 ReplayGain 前置增益。

```javascript
await fb.replaygain.setPreamp(6.0, 0.0);
```

### getSettings() 

获取完整的 ReplayGain 设置。

```javascript
const settings = await fb.replaygain.getSettings();
```

### scan(paths) 

扫描文件的 ReplayGain 值。

```javascript
await fb.replaygain.scan(['E:\\\\Music\\\\song1.flac', 'E:\\\\Music\\\\song2.flac']);
```

### clear(paths) 

清除文件的 ReplayGain 值。

```javascript
await fb.replaygain.clear(['E:\\\\Music\\\\song.flac']);
```
