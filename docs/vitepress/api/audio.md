# Audio & DSP & Output API 

音频分析、频谱可视化、DSP 效果器管理、音频输出、ReplayGain。

## Audio API - 音频分析 

### audio.subscribeSpectrum 

订阅实时频谱数据。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| subscriptionId | string | ✗ | 显式订阅 ID；重复使用同一 ID 会更新/覆盖现有订阅 |
| fftSize | number | ✗ | FFT 大小（必须为 2 的幂，256-16384），默认 1024 |
| bands | number | ✗ | 输出频段数，默认 48 |
| fps | number | ✗ | 刷新率 (1-60)，默认 30 |
| event | string | ✗ | 事件名称，默认 "audio:spectrum" |

**返回值**:

```json
{
    "success": true,
    "subscriptionId": "spectrum_main",
    "fftSize": 1024,
    "bands": 48,
    "fps": 30,
    "event": "audio:spectrum"
}
```

::: warning WARNING
`fftSize` 必须是 2 的幂（256, 512, 1024, 2048, 4096, 8192, 16384），否则返回错误。
:::

::: tip 低频分辨率
C++ 层会根据请求的频段数自动提升 FFT 大小（≥64 bands → 8192，≥32 bands → 4096），以确保低频区域有足够的 bin 分辨率。频谱处理流水线包括：对数频率映射、sub-bin 线性插值、三角滤波器 RMS 平滑、频率倾斜补偿（+1.5 dB/octave）、dB 归一化和 gamma 校正（0.8）。
:::

::: tip 订阅与事件语义

- `subscriptionId` 只在 low-level `fb2k.invoke('audio.*')` 场景下需要；SDK `fb.audio.subscribeSpectrum()` 不公开它。
- 若重复传入同一个 `subscriptionId`，语义是更新/覆盖该订阅。
- 默认 `audio:spectrum` 与自定义 `event` 的 payload 都是 `{ spectrum: number[] }`。

:::

```javascript
const subscriptionId = 'spectrum_main';

await fb2k.invoke('audio.subscribeSpectrum', {
    subscriptionId,
    fftSize: 1024,
    bands: 96,
    fps: 30,
    event: 'audio:spectrum'
});

fb2k.on('audio:spectrum', (data) => {
    renderVisualizer(data.spectrum); // 归一化值数组
});

await fb2k.invoke('audio.unsubscribeSpectrum', { subscriptionId });
```

### audio.unsubscribeSpectrum 

取消订阅频谱数据，释放可视化流资源。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| subscriptionId | string | ✗ | 要取消的订阅 ID。省略时默认取消当前 caller 的全部频谱订阅；仅在缺少 caller 上下文时才退化为更宽的清理范围 |

**返回值**: `{ "success": true, "removed": 1, "subscriptionId": "spectrum_main" }`

**示例**:

```javascript
// 精确取消指定订阅
await fb2k.invoke('audio.unsubscribeSpectrum', {
    subscriptionId: 'my-sub-id'
});

// 省略 subscriptionId 时，默认取消当前 caller 的全部频谱订阅
await fb2k.invoke('audio.unsubscribeSpectrum');
```

### audio.getSpectrum 

手动获取当前频谱数据（轮询模式）。需要先调用 `subscribeSpectrum`。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| bands | number | ✗ | 本次轮询期望返回的频段数；未传时沿用当前订阅配置（默认 48） |

**返回值**:

```json
{
    "success": true,
    "spectrum": [0.1, 0.3, 0.5, ...],
    "fftSize": 1024,
    "bands": 96
}
```

::: tip 轮询建议
手动轮询是支持的，适合低频 UI 刷新和调试；如果需要高频实时可视化，优先使用 `audio:spectrum` 事件流。
:::

**示例**:

```javascript
// 先订阅频谱
await fb2k.invoke('audio.subscribeSpectrum', { fftSize: 1024, bands: 96, fps: 30 });

// 手动获取当前频谱数据
const result = await fb2k.invoke('audio.getSpectrum', { bands: 96 });
if (result.success) {
    console.log('频谱数据:', result.spectrum);
    console.log('频段数:', result.bands);
    console.log('FFT 大小:', result.fftSize);
}
```

### audio.getWaveform 

获取当前播放流的短波形片段。需要先调用 `subscribeSpectrum` 启动可视化流。

::: warning 注意
此 API 用于获取**当前播放流**的实时波形片段，不是离线文件波形。如需生成完整文件波形，请使用 `audio.generateFullWaveform`。
:::

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| duration | number | ✗ | 持续时间（秒），默认 0.05 |
| signed | boolean | ✗ | 是否输出带符号波形 [-1, 1]，默认 false |

**返回值**:

```json
{ "success": true, "waveform": [0.01, 0.18, ...], "duration": 0.05, "signed": false }
```

::: tip 返回值范围

- `signed: false`（默认）：`waveform` 为 dB 归一化幅度数组，范围 `0..1`
- `signed: true`：`waveform` 保留 PCM 极性，线性归一化到 `[-1, 1]`，适合绘制对称波形

:::

**示例**:

```javascript
// 先订阅频谱以启动可视化流
await fb2k.invoke('audio.subscribeSpectrum');

// 获取 0.1 秒的波形数据
const result = await fb2k.invoke('audio.getWaveform', { duration: 0.1 });
if (result.success) {
    console.log('波形数据点数:', result.waveform.length);
    console.log('持续时间:', result.duration);
}

// signed 模式：获取带正负极性的对称波形（适合绘制上下对称波形图）
const signed = await fb2k.invoke('audio.getWaveform', { duration: 0.1, signed: true });
// signed.waveform 范围 [-1, 1]
```

### audio.setChannelMode 

设置频谱分析的声道模式。无效的 `mode` 值会自动规范化为 `"default"`，返回值中的 `mode` 反映规范化后的结果。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| mode | string | ✗ | "default", "mono", "front", "back"（默认 "default"） |

- **返回值**: `{ "success": true, "mode": "mono" }`

**示例**:

```javascript
// 设置为单声道模式
await fb2k.invoke('audio.setChannelMode', { mode: 'mono' });

// 设置为前置声道
await fb2k.invoke('audio.setChannelMode', { mode: 'front' });

// 非法模式自动回退为 default
const result = await fb2k.invoke('audio.setChannelMode', { mode: 'invalid' });
// result.mode === "default"
```

### audio.analyzeBPM 

分析曲目的 BPM。首先从元数据 `BPM` 标签读取，若不存在则尝试流派估算。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |
| forceAnalysis | boolean | ✗ | 强制重新分析（忽略已有的 BPM 标签） |

**返回值**:

```json
{ "success": true, "bpm": 128, "source": "metadata", "confidence": 1.0 }
```

| source 值 | 含义 |
| --- | --- |
| "metadata" | 来自文件 BPM 标签 |
| "estimate" | 来自流派估算（confidence 较低） |

**示例**:

```javascript
// 从元数据读取 BPM
const result = await fb2k.invoke('audio.analyzeBPM', {
    path: 'E:\\\\Music\\\\song.flac'
});
console.log(`BPM: ${result.bpm}, 来源: ${result.source}`);

// 强制重新分析
const result2 = await fb2k.invoke('audio.analyzeBPM', {
    path: 'E:\\\\Music\\\\song.flac',
    forceAnalysis: true
});
```

::: danger 已废弃
此 API 为历史遗留接口，当前仅返回文件基本信息（duration、sampleRate、channels），不包含实际波形数据。**请使用 `audio.generateFullWaveform` 代替**，它提供完整的后台解码、缓存和事件通知功能。
:::

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径 |
| resolution | number | ✗ | 数据点数量 (50-4000)，默认 800 |

**返回值**:

```json
{
    "success": false,
    "error": "Waveform generation requires audio decoding (not yet implemented)"
}
```

### audio.generateFullWaveform 

生成完整文件波形数据，支持后台解码、缓存和异步事件通知。适用于进度条概览、波形卡片和章节预览。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 音频文件路径，支持 path\|subsong:N 格式 |
| resolution | number | ✗ | 波形分辨率（数据点数量），范围 64-4096，默认 256 |
| method | string | ✗ | 采样方法："peak"（峰值）或 "rms"（均方根），默认 "rms" |
| scale | string | ✗ | 波形刻度："linear" 或 "db"（-60dB 地板映射到 0..1），默认 "linear" |
| signed | boolean | ✗ | 是否输出带符号波形 [-1, 1]，默认 false。启用时忽略 scale 参数 |
| preferCache | boolean | ✗ | 是否优先使用缓存，默认 true |
| cueIndex | number | ✗ | CUE 索引（用于 CUE 分轨），优先级高于 path\|subsong:N |

::: tip 路径与 fallback 语义

- C++ 层会先解析 `path|subsong:N`，再 canonicalize 路径，并把同一 canonical path 用于 `handle_create()`、解码器和缓存键。
- 若同时提供 `cueIndex` 与 `path|subsong:N`，最终以 `cueIndex` 为准。
- 若 cached info 的 `duration`、`samplerate`、`channels` 不完整，会先尝试 direct file read；只有 direct read 仍不足时才会进入失败事件。
- **归一化策略**: 采用 95th percentile 归一化 — 收集所有非零采样窗口值并排序，取第 95% 分位数作为归一化参考值，超出部分 clamp 到 1.0。这使 95% 的窗口使用完整动态范围，仅极端峰值被裁剪。含防极端数据保护：若 95th percentile < maxValue × 0.3，则回退到 maxValue 归一化。
:::

**示例**:

```javascript
const result = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\\\Music\\\\song.flac',
    resolution: 256
});

if (result.taskId) {
    fb2k.on('audio:fullWaveformReady', (e) => {
        if (e.taskId === result.taskId) {
            console.log('波形生成完成:', e.waveform);
        }
    });

    // 同时监听失败事件以便处理错误（路径无法解码、解析失败等）
    fb2k.on('audio:fullWaveformFailed', (e) => {
        if (e.taskId === result.taskId) {
            console.error('波形生成失败:', e.error);
        }
    });
}

// 支持 subsong 格式
const result2 = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\\\Music\\\\disc.flac|subsong:2',
    resolution: 512
});

// 使用 cueIndex（优先级高于路径中的 subsong）
const result3 = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\\\Music\\\\album.cue',
    cueIndex: 3,
    resolution: 256
});

// RMS 模式（更平滑的能量包络）
const result4 = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\\\Music\\\\song.flac',
    resolution: 512,
    method: 'rms'
});

// signed 模式：输出 [-1, 1] 对称波形（适合绘制上下对称波形图）
const result5 = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\\\Music\\\\song.flac',
    resolution: 512,
    signed: true
});
// result5.waveform 包含正负值，signed 模式下 scale 参数被忽略
```

**采样方法说明**:

- `peak`: 取窗口内所有声道绝对值的最大值，适合波形概览和进度条
- `rms`: 取窗口内所有声道的均方根值，提供更平滑的能量包络

**signed 模式说明**:

启用 `signed: true` 时，波形数据保留 PCM 极性（正/负），归一化到 `[-1, 1]`：

- 多声道取算术平均（保留符号），窗口内选绝对值最大的采样点（保留符号）
- dB 刻度在 signed 模式下自动忽略（负值取对数无意义）
- 适用于绘制上下对称的波形可视化
- 事件和返回值均包含 `"signed": true` 字段

**缓存机制**:

- 缓存键包含：路径、subsong、resolution、method、文件大小、修改时间
- 最大缓存条目数：50（LRU 淘汰）
- 文件修改后自动失效

### audio.getOutputInfo 

获取音频输出信息（当前音量）。

- **参数**: 无

**返回值**:

```json
{ "success": true, "volume": -5.0, "volumePercent": 56.2 }
```

**示例**:

```javascript
const info = await fb2k.invoke('audio.getOutputInfo');
console.log(`音量: ${info.volume} dB (${info.volumePercent}%)`);
```

### audio.getStreamInfo 

获取当前播放流信息（采样率、声道、编码等）。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "playing": true,
    "sampleRate": 44100,
    "channels": 2,
    "bitrate": 1411,
    "codec": "FLAC",
    "duration": 234.5
}
```

> 未播放时返回 `{ "success": true, "playing": false }`。

```javascript
const info = await fb2k.invoke('audio.getStreamInfo');
if (info.playing) {
    console.log(`${info.codec} ${info.sampleRate}Hz ${info.channels}ch`);
}
```

### audio.isVisualizationAvailable 

检查可视化功能是否可用。

- **参数**: 无
- **返回值**: `{ "success": true, "available": true }`

**示例**:

```javascript
const result = await fb2k.invoke('audio.isVisualizationAvailable');
if (result.available) {
    console.log('可视化功能可用');
    // 可以安全地调用 subscribeSpectrum
    await fb2k.invoke('audio.subscribeSpectrum');
}
```

### audio.subscribeStream 

订阅音频流捕获（用于录音/流媒体）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| event | string | ✗ | 事件名称，默认 "audio:stream" |
| interval | number | ✗ | 采样间隔（秒），默认 0.05 |

::: warning WARNING
此功能需要 `playback_stream_capture` 集成，当前未完整实现。
:::

### audio.unsubscribeStream 

取消音频流捕获。

- **参数**: 无
- **返回值**: `{ "success": true }`

### audio.getSpectrumDebugState 

获取频谱系统内部调试状态，包含当前订阅列表、分发目标、内部定时器状态等。主要用于诊断频谱订阅问题。

- **参数**: 无
- **返回值**:

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| active | boolean | 频谱系统是否活跃 |
| timerRunning | boolean | 内部定时器是否运行 |
| effectiveFftSize | number | 当前生效的 FFT 大小 |
| effectiveFps | number | 当前生效的刷新率 |
| effectiveBands | number | 当前生效的频段数 |
| streamReady | boolean | 音频流是否就绪 |
| subscriptionCount | number | 当前订阅数量 |
| subscriptions | array | 订阅详情列表 |
| callerOwnsSubscription | boolean | 调用者是否拥有订阅 |

```javascript
const debug = await fb2k.invoke('audio.getSpectrumDebugState');
console.log(debug.subscriptions);
```

## DSP API - 效果器管理 

> 注意: `dsp.getActivePreset` / `dsp.setActivePreset` 未在 C++ 层注册，请改用 `config.getActiveDspPreset` / `config.setActiveDspPreset`。

### dsp.getChain 

获取当前 DSP 效果器链配置。

- **参数**: 无

**返回值**:

```json
{
    "dsps": [
        { "index": 0, "guid": "{...}", "name": "Equalizer" }
    ],
    "activePreset": "My Preset",
    "activePresetIndex": 0
}
```

> `activePreset` 和 `activePresetIndex` 仅在 DSP v2 API 可用时返回。

### dsp.getPresets 

获取所有 DSP 预设列表。

- **参数**: 无

**返回值**:

```json
{
    "presets": [
        { "index": 0, "name": "Default", "active": true }
    ],
    "count": 3,
    "selectedIndex": 0
}
```

### dsp.applyPreset 

应用指定的 DSP 预设。通过名称或索引指定。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| name | string | ✗ | 预设名称 |
| index | number | ✗ | 预设索引 |

> `name` 和 `index` 至少提供一个。

**返回值**: `{ "success": true, "appliedPreset": "My Preset", "appliedIndex": 0 }`

```javascript
// 按名称
await fb2k.invoke('dsp.applyPreset', { name: 'My Preset' });
// 按索引
await fb2k.invoke('dsp.applyPreset', { index: 0 });
```

### dsp.getAvailable 

获取所有可用的 DSP 处理器列表（已安装的 DSP 组件）。

- **参数**: 无

**返回值**:

```json
{
    "dsps": [
        { "guid": "{...}", "name": "Equalizer", "hasConfig": true }
    ],
    "count": 18
}
```

### dsp.addDsp 

添加 DSP 效果器到链中。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| guid | string | ✓ | DSP 处理器 GUID（从 dsp.getAvailable 获取） |
| position | number | ✗ | 插入位置（默认 -1 = 末尾） |

**返回值**: `{ "success": true, "addedDsp": "Equalizer", "position": 2 }`

```javascript
// 获取可用 DSP 列表，然后添加
const available = await fb2k.invoke('dsp.getAvailable');
const eq = available.dsps.find(d => d.name === 'Equalizer');
if (eq) {
    await fb2k.invoke('dsp.addDsp', { guid: eq.guid });
}
```

### dsp.removeDsp 

从链中移除 DSP 效果器。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | number | ✓ | 链中的索引位置 |

**返回值**: `{ "success": true, "removedDsp": "Equalizer", "removedIndex": 2 }`

### dsp.moveDsp 

移动 DSP 效果器在链中的位置。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| from | number | ✓ | 原始索引 |
| to | number | ✓ | 目标索引 |

**返回值**: `{ "success": true, "movedDsp": "Equalizer", "from": 0, "to": 2 }`

### dsp.setChain 

设置完整的 DSP 效果器链（高级用法，替换整个链）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| dsps | array | ✓ | DSP 数组，每个元素含 guid 字段 |

**返回值**: `{ "success": true, "count": 3 }`

```javascript
await fb2k.invoke('dsp.setChain', {
    dsps: [
        { guid: '{EQ-GUID-HERE}' },
        { guid: '{LIMITER-GUID-HERE}' }
    ]
});
```

## Output API - 音频输出 

### output.getDevices 

获取所有可用的音频输出设备。

- **参数**: 无

**返回值**:

```json
{
    "devices": [
        {
            "guid": "{...}",
            "name": "Speakers (Realtek)",
            "entry": "WASAPI (event)",
            "entryGuid": "{...}"
        }
    ],
    "count": 5
}
```

### output.getEntries 

获取输出模块列表（WASAPI, DirectSound 等）。

- **参数**: 无

**返回值**:

```json
{
    "entries": [
        {
            "guid": "{...}",
            "name": "WASAPI (event)",
            "needsBitdepthConfig": false,
            "needsDitherConfig": false,
            "supportsMultipleStreams": false,
            "isHighLatency": false,
            "isLowLatency": true
        }
    ],
    "count": 4
}
```

### output.getSettings 

获取当前输出设置信息（只读）。

- **参数**: 无

**返回值**:

```json
{
    "note": "Output settings are managed through foobar2000 Preferences > Playback > Output",
    "availableOutputs": ["WASAPI (event)", "WASAPI (push)", "DirectSound", "Primary Sound Driver"]
}
```

> 实际输出设置通过 foobar2000 首选项管理。如需切换输出设备，请使用 `config.setOutputDevice`。

## ReplayGain API 

ReplayGain 音量标准化设置。

### replaygain.getSettings 

获取所有 ReplayGain 设置。

- **参数**: 无

**返回值**:

```json
{
    "sourceMode": "track",
    "processingMode": "gain",
    "preampWithRg": 0.0,
    "preampWithoutRg": 0.0,
    "active": true
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| sourceMode | string | "none" / "track" / "album" / "auto" |
| processingMode | string | "none" / "gain" / "gain_and_peak" / "peak" |
| preampWithRg | number | 有 RG 信息时的预增益 (dB) |
| preampWithoutRg | number | 无 RG 信息时的预增益 (dB) |
| active | boolean | RG 是否激活 |

### replaygain.getMode 

获取当前 ReplayGain 模式。

- **参数**: 无
- **返回值**: `{ "sourceMode": "track", "processingMode": "gain" }`

### replaygain.setMode 

设置 ReplayGain 模式。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| sourceMode | string | ✗ | "none" / "track" / "album" / "auto" / "byPlaybackOrder" |
| processingMode | string | ✗ | "none" / "gain" / "gain_and_peak" / "peak" |

**返回值**: `{ "success": true, "sourceMode": "track", "processingMode": "gain", "changed": true }`

```javascript
await fb2k.invoke('replaygain.setMode', { sourceMode: 'album', processingMode: 'gain' });
```

### replaygain.getPreamp 

获取预增益设置。

- **参数**: 无

**返回值**:

```json
{ "withRg": 0.0, "withoutRg": 0.0 }
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| withRg | number | 有 RG 信息时的预增益 (dB) |
| withoutRg | number | 无 RG 信息时的预增益 (dB) |

### replaygain.setPreamp 

设置预增益值。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| withRg | number | ✗ | 有 RG 信息时的预增益（-24 ~ +24 dB） |
| withoutRg | number | ✗ | 无 RG 信息时的预增益（-24 ~ +24 dB） |

**返回值**: `{ "success": true, "withRg": 3.0, "withoutRg": 0.0, "changed": true }`

```javascript
await fb2k.invoke('replaygain.setPreamp', { withRg: 3.0, withoutRg: -3.0 });
```

### replaygain.get 

获取指定文件的 ReplayGain 信息。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 文件路径数组 |

**返回值**:

```json
{
    "success": true,
    "count": 1,
    "results": [
        {
            "path": "C:\\\\Music\\\\song.flac",
            "success": true,
            "trackGain": "-5.20 dB",
            "trackGainRaw": -5.2,
            "trackPeak": "0.987654",
            "trackPeakRaw": 0.987654,
            "albumGain": "-4.80 dB",
            "albumGainRaw": -4.8,
            "albumPeak": "1.000000",
            "albumPeakRaw": 1.0,
            "hasReplayGain": true
        }
    ]
}
```

> 缺少 RG 信息的字段不会出现在结果中。`hasReplayGain` 表示是否有任何 track 或 album gain 数据。

### replaygain.clear 

移除文件中的 ReplayGain 信息。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 文件路径数组 |

**返回值**: `{ "success": true, "clearedCount": 5 }`

### replaygain.scan 

扫描文件的 ReplayGain（通过右键菜单触发，扫描结果自动写入文件）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 文件路径数组 |
| mode | string | ✗ | "track"（逐文件扫描）或 "album"（整体扫描），默认 "track" |

**返回值**: `{ "success": true, "scannedCount": 10, "mode": "track", "note": "Scan started. Results will be written to files automatically." }`

```javascript
// 扫描单曲 track gain
await fb2k.invoke('replaygain.scan', {
    paths: ['C:\\\\Music\\\\song.flac'],
    mode: 'track'
});
// 扫描整张专辑
await fb2k.invoke('replaygain.scan', {
    paths: ['C:\\\\Music\\\\01.flac', 'C:\\\\Music\\\\02.flac'],
    mode: 'album'
});
```
