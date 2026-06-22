# fb.audio 音频分析 

## subscribeSpectrum(callback, options?) 

订阅实时频谱数据。自动启动 C++ 频谱采集流，返回取消订阅函数（同时停止采集）。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| callback | function | 接收频谱数据的回调，参数为 {spectrum: number[]} |
| options.fftSize | number | FFT 大小，2 的幂 (256-16384)，默认 1024 |
| options.bands | number | 订阅时的输出频段数，默认 48 |
| options.fps | number | 刷新率 (1-60)，默认 30 |
| options.event | string | 自定义事件名，默认 "audio:spectrum"。多面板场景可用不同事件名隔离数据 |

```javascript
// 基本用法
fb.audio.subscribeSpectrum((data) => {
    renderVisualizer(data.spectrum);
});

// 自定义参数
const unsubscribe = fb.audio.subscribeSpectrum(
    (data) => renderVisualizer(data.spectrum),
    { fftSize: 2048, bands: 96, fps: 60 }
);

// 自定义事件名
const unsubscribeCustom = fb.audio.subscribeSpectrum(
    (data) => renderVisualizer(data.spectrum),
    { fftSize: 1024, bands: 48, fps: 30, event: 'audio:panelSpectrum' }
);

// 取消订阅（同时停止 C++ 采集，释放可视化资源）
unsubscribe();
unsubscribeCustom();
```

::: warning 注意
取消订阅时会调用 `audio.unsubscribeSpectrum` 释放 C++ 可视化资源。如果不再需要频谱数据，务必调用返回的取消函数。
:::

::: tip low-level 分工
SDK `fb.audio.subscribeSpectrum()` 不暴露 `subscriptionId`。如果你需要显式控制订阅 ID、覆盖同名订阅，或做 caller 粒度调试，请改用 low-level `fb2k.invoke('audio.subscribeSpectrum', ...)`。
:::

单次获取当前频谱数据（轮询模式）。仍需要先调用 `subscribeSpectrum` 启动可视化流。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options.bands | number | 本次轮询期望返回的频段数；未传时沿用当前订阅配置（默认 48） |

```javascript
// 先启动频谱流
const unsubscribe = fb.audio.subscribeSpectrum(() => {}, { bands: 96 });

// 单次轮询当前频谱
const result = await fb.audio.getSpectrum({ bands: 96 });
console.log(result.spectrum);
console.log(result.bands);

unsubscribe();
```

获取当前播放流的短波形片段。需要先调用 `subscribeSpectrum` 启动可视化流。

::: warning 注意
此方法用于获取**当前播放流**的实时波形片段，不是离线文件波形。如需生成完整文件波形，请使用 `generateFullWaveform`。
:::

```javascript
// 先订阅频谱以启动可视化流
fb.audio.subscribeSpectrum(() => {});

// 获取 0.1 秒的波形数据
const result = await fb.audio.getWaveform({ duration: 0.1 });
console.log('波形数据:', result.waveform);
```

生成完整文件波形数据，支持后台解码、缓存和异步事件通知。适用于进度条概览、波形卡片和章节预览。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径，支持 path\|subsong:N 格式 |
| options.resolution | number | 波形分辨率（数据点数量），范围 64-4096，默认 256 |
| options.method | string | 采样方法："peak"（峰值）或 "rms"（均方根），默认 "rms" |
| options.scale | string | 波形刻度："linear" 或 "db"，默认 "linear" |
| options.preferCache | boolean | 是否优先使用缓存，默认 true |
| options.cueIndex | number | CUE 索引（用于 CUE 分轨），优先级高于 path\|subsong:N |

> SDK 只是透传到 `audio.generateFullWaveform`。底层会先解析 `path|subsong:N`，再 canonicalize 路径，并在 cached info 技术字段不足时自动尝试 direct file read。

**返回值**: Promise，resolve 时返回波形数据对象。

```javascript
// 基础用法（缓存命中时立即返回）
const result = await fb.audio.generateFullWaveform('E:\\\\Music\\\\song.flac', {
    resolution: 256,
    method: 'peak'
});

console.log('波形数据:', result.waveform);
console.log('时长:', result.duration);
console.log('是否来自缓存:', result.cached);

// 支持 subsong 格式
const result2 = await fb.audio.generateFullWaveform('E:\\\\Music\\\\disc.flac|subsong:2', {
    resolution: 512
});

// 使用 cueIndex（优先级高于路径中的 subsong）
const result3 = await fb.audio.generateFullWaveform('E:\\\\Music\\\\album.cue', {
    cueIndex: 3,
    resolution: 256
});

// RMS 模式（更平滑的能量包络）
const result4 = await fb.audio.generateFullWaveform('E:\\\\Music\\\\song.flac', {
    resolution: 512,
    method: 'rms'
});
```

**采样方法说明**:

- `peak`: 取窗口内所有声道绝对值的最大值，适合波形概览和进度条
- `rms`: 取窗口内所有声道的均方根值，提供更平滑的能量包络

**缓存机制**:

- 缓存键包含：路径、subsong、resolution、method、文件大小、修改时间
- 最大缓存条目数：50（LRU 淘汰）
- 文件修改后自动失效

::: tip 异步处理
此方法内部自动处理缓存命中/未命中的情况：
- 缓存命中：立即返回结果
- 缓存未命中：启动后台任务，监听 `audio:fullWaveformReady` 事件，完成后 resolve

调用者无需手动监听事件，直接 `await` 即可。
:::

分析文件 BPM。返回 `{bpm}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径 |
| options.forceAnalysis | boolean | 强制重新分析（忽略已有的 BPM 标签） |

```javascript
const result = await fb.audio.analyzeBPM('E:\\\\Music\\\\song.flac');
console.log(`BPM: ${result.bpm}`);

// 强制重新分析
const result2 = await fb.audio.analyzeBPM('E:\\\\Music\\\\song.flac', { forceAnalysis: true });
```

设置声道模式。无效的 `mode` 值会自动规范化为 `"default"`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| mode | string | "default", "mono", "front", "back" |

```javascript
await fb.audio.setChannelMode('mono');
// 无效值自动回退
const result = await fb.audio.setChannelMode('invalid'); // result.mode === "default"
```

获取频谱系统内部调试状态。返回当前订阅列表、分发目标、FFT/FPS/Bands 参数、定时器状态等诊断信息。

```javascript
const debug = await fb.audio.getSpectrumDebugState();
console.log(debug.subscriptions, debug.effectiveFps);
```

## 补全方法参考

### getOutputInfo()

签名：`fb.audio.getOutputInfo(): Promise<AudioOutputInfoResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：当前输出设备、声道和采样相关信息。

```javascript
const output = await fb.audio.getOutputInfo();
console.log(output.deviceName, output.sampleRate);
```

### getStreamInfo()

签名：`fb.audio.getStreamInfo(): Promise<AudioStreamInfoResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：当前播放流信息，如声道数、采样率或可视化流状态。

```javascript
const stream = await fb.audio.getStreamInfo();
console.log(stream.channels, stream.sampleRate);
```

### isVisualizationAvailable()

签名：`fb.audio.isVisualizationAvailable(): Promise<{ available: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：当前主机是否可提供可视化数据。

```javascript
const { available } = await fb.audio.isVisualizationAvailable();
```

### unsubscribeStream()

签名：`fb.audio.unsubscribeStream(): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：停止 raw audio stream 订阅的操作结果。

```javascript
await fb.audio.unsubscribeStream();
```
