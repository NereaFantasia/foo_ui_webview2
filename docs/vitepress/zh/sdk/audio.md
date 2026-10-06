# fb.audio 音频分析

## subscribeSpectrum(callback, options?)

订阅实时频谱数据。自动启动 C++ 频谱采集流，返回取消订阅函数（同时停止采集）。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| callback | function | 只接收本订阅的帧，类型为 `AudioSpectrumPayload`：`spectrum` 加上 [`audio.subscribeSpectrum`](../api/audio.md#audio-subscribespectrum) 列出的帧字段（`subscriptionId`、`bands`、`fftSize`、`scale`、`sampleRate`、`state`、`streamTime`、`hostTime` 等）。旧版宿主只发 `spectrum` |
| options.fftSize | number | FFT 大小，2 的幂（256–65536），默认 1024。65536 是组件自己的封顶，只在需要原始 bin 分辨率时才有意义 |
| options.bands | number | 订阅时的输出频段数，默认 48 |
| options.fps | number | 刷新率 (1-60)，默认 30 |
| options.scale | `'weighted' \| 'db'` | `'weighted'`（默认）是取值 `[0, 1]` 的显示用曲线；`'db'` 是频带功率 dB，满幅正弦读 0 dB |
| options.backgroundThrottle | boolean | 默认 `true`：别的程序在前台期间每秒至多 12 帧，通常在 10 到 12 帧之间。传 `false` 保持原帧率 |
| options.minFrequency | number | 频带范围的下限（Hz），不小于 1，默认 20 |
| options.maxFrequency | number | 上限（Hz），须大于 `minFrequency`；按流采样率的一半截，不给时也取这个值 |
| options.event | string | 自定义事件名，默认 "audio:spectrum"。多面板场景可用不同事件名隔离数据 |

返回的取消函数带一个 `ready` 属性：宿主答复后兑现的 Promise，永不 reject。登记成功时得到 `{ ok: true, subscriptionId, fftSize, bands, fps, scale, backgroundThrottle, minFrequency, maxFrequency, streamReady }`（范围跟随采样率一半时 `maxFrequency` 为 `null`），否则得到 `{ ok: false, code, error }`：参数被拒是 `INVALID_PARAMS`，没有宿主是 `NOT_SUPPORTED`，调用本身失败是 `UNKNOWN_ERROR`。

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

// 频带功率 dB、保持原帧率，并确认登记结果
const meter = fb.audio.subscribeSpectrum(
    (frame) => drawMeter(frame.spectrum, frame.state),
    { bands: 64, fftSize: 8192, scale: 'db', backgroundThrottle: false }
);
const outcome = await meter.ready;
if (!outcome.ok) console.warn('spectrum subscription failed', outcome.code, outcome.error);

// 取消订阅（同时停止 C++ 采集，释放可视化资源）
unsubscribe();
unsubscribeCustom();
```

::: warning 注意
取消订阅时会调用 `audio.unsubscribeSpectrum` 释放 C++ 可视化资源。如果不再需要频谱数据，务必调用返回的取消函数。
:::

::: tip low-level 分工
SDK `fb.audio.subscribeSpectrum()` 自己生成并管理 `subscriptionId`，需要时从 `ready` 读出，例如拿它调 `getSpectrum` 轮询这个订阅。如果你要自己指定订阅 ID、覆盖同名订阅，或做 caller 粒度调试，请改用 low-level `fb2k.invoke('audio.subscribeSpectrum', ...)`。
:::

## getSpectrum(options?)

单次获取当前频谱数据（轮询模式）。仍需要先调用 `subscribeSpectrum` 启动可视化流。结果类型为 `AudioGetSpectrumResponse`：一帧加 `success`；暂停或停止时是静音帧，还没有数据时是 `success: false`。画面要跟显示器逐帧对齐时，传入 `ready` 里的 `subscriptionId`，在 `requestAnimationFrame` 里调用，同时只发一个请求，频率不超过需要的帧率；详见 [`audio.getSpectrum`](../api/audio.md#audio-getspectrum)。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options.subscriptionId | string | 按这个订阅的参数算这一帧（此时忽略 `bands`、`scale` 与频率范围） |
| options.bands | number | 本次轮询期望返回的频段数；未传或为 `0` 时取各订阅中最大的频段数 |
| options.scale | `'weighted' \| 'db'` | 不给 `subscriptionId` 时本次轮询的档位，默认 `'weighted'` |
| options.minFrequency / options.maxFrequency | number | 不给 `subscriptionId` 时本次轮询的频带范围，规则同 `subscribeSpectrum` |

```javascript
// 先启动频谱流
const unsubscribe = fb.audio.subscribeSpectrum(() => {}, { bands: 96 });

// 单次轮询当前频谱
const result = await fb.audio.getSpectrum({ bands: 96 });
console.log(result.spectrum);
console.log(result.bands);

unsubscribe();
```

## getWaveform(options?)

获取当前播放流的短波形片段。需要先调用 `subscribeSpectrum` 启动可视化流。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options.duration | number | 窗口时长（秒），大于 0 且不超过 1。默认 `0.05` |
| options.signed | boolean | 保留 PCM 极性，夹到 `[-1, 1]`。默认 `false` |
| options.channels | `'mix' \| 'stereo'` | `'mix'`（默认）返回平均成一路的 `waveform`；`'stereo'` 把前两路分别放进 `left` 与 `right`，单声道流的 `right` 等于 `left` |
| options.points | number | 等距取这么多个样本，不做平均；取 `[2, 65536]` 内的整数。不给时返回全部样本 |

应答另带可视化流的 `channels`、`sampleRate` 与 `channelCount`。参数不合法时回 `code: 'INVALID_PARAMS'`。

::: warning 注意
此方法用于获取**当前播放流**的实时波形片段，不是离线文件波形。如需生成完整文件波形，请使用 `generateFullWaveform`。
:::

```javascript
// 先订阅频谱以启动可视化流
fb.audio.subscribeSpectrum(() => {});

// 获取 0.1 秒的波形数据
const result = await fb.audio.getWaveform({ duration: 0.1 });
console.log('波形数据:', result.waveform);

// 两路分开，各取 256 个样本
const { left, right } = await fb.audio.getWaveform({
    duration: 0.05,
    signed: true,
    channels: 'stereo',
    points: 256,
});
```

## generateFullWaveform(path, options?)

生成完整文件波形数据，支持后台解码、缓存和异步事件通知。适用于进度条概览、波形卡片和章节预览。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径，支持 path\|subsong:N 格式 |
| options.resolution | number | 波形分辨率（数据点数量），范围 64-4096，默认 256 |
| options.method | string | 采样方法："peak"（峰值）或 "rms"（均方根），默认 "rms" |
| options.scale | string | 波形刻度："linear" 或 "db"，默认 "linear" |
| options.signed | boolean | 保留正负极性，默认 false |
| options.preferCache | boolean | 是否优先使用缓存，默认 true |
| options.cueIndex | number | CUE 索引（用于 CUE 分轨），优先级高于 path\|subsong:N |
| options.timeout | number | 等待 pending 任务的超时（毫秒），默认 60000；`<= 0` 表示不设超时 |
| options.signal | AbortSignal | 提前结束等待，见下文 |

> 除 `timeout` 与 `signal` 由 SDK 自己消费外，其余选项透传到 `audio.generateFullWaveform`。底层会先解析 `path|subsong:N`，再 canonicalize 路径，并在 cached info 技术字段不足时自动尝试 direct file read。

**返回值**: Promise，resolve 时返回波形数据对象。缓存命中立即以 `status: 'ready'` 返回，未命中时等待对应的完成事件。`result.maxAmplitude` 是归一化前所选序列的最大值（线性满幅）：`linear` 档用 `waveform[i] * maxAmplitude` 还原电平，`db` 档 dBFS = `(v * 60 - 60) + 20 * log10(maxAmplitude)`。

**提前结束等待**: `signal` 中止时以名为 `AbortError` 的 `DOMException` reject，超时以 `{ success: false, error: 'TIMEOUT' }` reject，两者都会经 `cancelFullWaveform` 取消宿主任务：排队中的直接丢弃，解码中的在解码器察觉中止后让出解码槽位。调用前已中止的 `signal` 直接 reject、不调宿主。缓存命中时 `signal` 不影响结果。

**失败形态**: 宿主当场拒绝的请求（缺路径、路径无效、无权限、退出中）以 `{ success: false, error, code }` resolve，要检查 `success`；解码过程中失败以 `audio:fullWaveformFailed` 的载荷 reject；桥层错误以 `Error` reject。

```javascript
// 基础用法（缓存命中时立即返回）
const result = await fb.audio.generateFullWaveform('E:\\Music\\song.flac', {
    resolution: 256,
    method: 'peak'
});

console.log('波形数据:', result.waveform);
console.log('时长:', result.duration);
console.log('是否来自缓存:', result.cached);

// 支持 subsong 格式
const result2 = await fb.audio.generateFullWaveform('E:\\Music\\disc.flac|subsong:2', {
    resolution: 512
});

// 使用 cueIndex（优先级高于路径中的 subsong）
const result3 = await fb.audio.generateFullWaveform('E:\\Music\\album.cue', {
    cueIndex: 3,
    resolution: 256
});

// RMS 模式（更平滑的能量包络）
const result4 = await fb.audio.generateFullWaveform('E:\\Music\\song.flac', {
    resolution: 512,
    method: 'rms'
});

// 用户切到别的曲目时丢掉这次请求
const controller = new AbortController();
fb.audio.generateFullWaveform('E:\\Music\\song.flac', { signal: controller.signal })
    .catch((err) => {
        if (err.name !== 'AbortError') throw err;
    });
controller.abort();
```

**采样方法说明**:

- `peak`: 取窗口内所有声道绝对值的最大值，适合波形概览和进度条
- `rms`: 取窗口内所有声道的均方根值，提供更平滑的能量包络

**缓存机制**:

- 缓存键包含：规范化路径、subsong、resolution、文件大小、修改时间；条目存一次解码得到的原始值，任何 `method` / `signed` / `scale` 都由它现算
- 最大缓存条目数：50（LRU 淘汰）
- 文件修改后自动失效

**排队机制**:

- 缓存未命中时，同一曲目、同一 resolution 的在途请求共用一次解码，各自拿到自己的结果
- 宿主同时最多解码 2 首，其余按提交顺序排队；排队时间也计入 `timeout`，不再需要的请求要取消

::: tip 异步处理
此方法内部自动处理缓存命中/未命中的情况：
- 缓存命中：立即返回结果
- 缓存未命中：等待 `audio:fullWaveformReady`；收到 `audio:fullWaveformFailed` 或超时则 reject

调用者无需手动监听事件，直接 `await` 即可。
:::

## cancelFullWaveform(taskId)

按 `pending` 应答里的 `taskId` 取消一个 `generateFullWaveform` 请求，只认发起请求的调用方。`generateFullWaveform` 在 `signal` 中止和超时时已经自动调用它；手里拿的是 `fb2k.invoke` 返回的 `taskId` 时直接调它。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| taskId | string | `pending` 应答里的 `taskId` |

**返回值**: Promise，resolve 为 `{ success: true, cancelled: boolean }`。`cancelled: false` 表示该任务已结束、不存在，或不归本调用方。被取消的任务会收到一次 `audio:fullWaveformFailed`（`code: 'CANCELLED'`），可能先于本应答到达。1.14 之前的宿主没有这个端点，调用会以 `Method not found` reject。

```javascript
const pending = await fb2k.invoke('audio.generateFullWaveform', { path: 'E:\\Music\\song.flac' });
if (pending.status === 'pending') {
    const { cancelled } = await fb.audio.cancelFullWaveform(pending.taskId);
}
```

## analyzeBPM(path, options?)

分析文件 BPM。返回 `{bpm}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径 |
| options.forceAnalysis | boolean | 强制重新分析（忽略已有的 BPM 标签） |

```javascript
const result = await fb.audio.analyzeBPM('E:\\Music\\song.flac');
console.log(`BPM: ${result.bpm}`);

// 强制重新分析
const result2 = await fb.audio.analyzeBPM('E:\\Music\\song.flac', { forceAnalysis: true });
```

## setChannelMode(mode)

设置声道模式。无效的 `mode` 值会自动规范化为 `"default"`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| mode | string | "default", "mono", "front", "back" |

```javascript
await fb.audio.setChannelMode('mono');
// 无效值自动回退
const result = await fb.audio.setChannelMode('invalid'); // result.mode === "default"
```

## getSpectrumDebugState()

获取频谱系统内部调试状态。返回当前订阅列表（每项带 `scale`、`backgroundThrottle` 与频率范围）、分发目标、各订阅请求值中最大的 FFT/FPS/Bands、`framesComputed`、推帧计时线程的状态（`timerRunning`、`beatSource`、`beatIntervalMs`、`beatsCoalesced`）等诊断信息。

```javascript
const debug = await fb.audio.getSpectrumDebugState();
console.log(debug.subscriptions, debug.effectiveFps);
```

## 其余方法

### getOutputInfo()

签名：`fb.audio.getOutputInfo(): Promise<AudioOutputInfoResponse>`

无参数。

```javascript
const output = await fb.audio.getOutputInfo();
console.log(output.deviceName, output.sampleRate);
```

### getStreamInfo()

签名：`fb.audio.getStreamInfo(): Promise<AudioStreamInfoResponse>`

无参数。

```javascript
const stream = await fb.audio.getStreamInfo();
console.log(stream.channels, stream.sampleRate);
```

### isVisualizationAvailable()

签名：`fb.audio.isVisualizationAvailable(): Promise<{ available: boolean }>`

无参数。

```javascript
const { available } = await fb.audio.isVisualizationAvailable();
```

### subscribeStream(callback, options?)

签名：`fb.audio.subscribeStream(callback: StreamCallback, options?: AudioSubscribeStreamParams): () => void`

启动已弃用的 raw stream 订阅并返回取消订阅函数。当前宿主会拒绝 `audio.subscribeStream`，因此在宿主接入流捕获能力之前回调不会触发。

```javascript
const unsubscribe = fb.audio.subscribeStream((chunk) => {
    console.log(chunk);
});
unsubscribe();
```

### generateWaveform(path, options?)

签名：`fb.audio.generateWaveform(path: string, options?: Omit<AudioGenerateWaveformParams, 'path'>): Promise<AudioGenerateWaveformResponse>`

已弃用的旧 `audio.generateWaveform` 端点别名。完整曲目波形生成请使用 `fb.audio.generateFullWaveform()`。

```javascript
const result = await fb.audio.generateWaveform('C:\\Music\\song.flac');
```

### unsubscribeStream()

签名：`fb.audio.unsubscribeStream(): Promise<BaseResponse>`

无参数。

::: warning 已弃用且宿主尚未实现
`subscribeStream()` 与 `unsubscribeStream()` 已弃用。当前宿主的 `audio.subscribeStream` 返回 `success: false`，且不会发出 `audio:stream`，因此在宿主接入流捕获能力之前，订阅回调不会触发。
:::

```javascript
await fb.audio.unsubscribeStream();
```
