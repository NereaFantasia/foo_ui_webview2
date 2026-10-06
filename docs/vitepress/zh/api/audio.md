# Audio & DSP & Output API

音频分析、频谱可视化、DSP 效果器管理、音频输出、ReplayGain。

## Audio API - 音频分析

### audio.subscribeSpectrum

订阅实时频谱数据。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `subscriptionId` | `string` | 否 | — | 省略时复用按窗口派生的 legacy 标识。用同一个 id 再订阅会替换原订阅。 |
| `fftSize` | `integer` | 否 | `1024` | 须为 256–65536 之间的 2 的幂。 |
| `bands` | `integer` | 否 | `48` | 截断到 8–`fftSize / 2`。 |
| `fps` | `integer` | 否 | `30` | 截断到 1–60。 |
| `scale` | `string` | 否 | `weighted` | `weighted`：显示用曲线，取值 `[0, 1]`。`db`：频带功率 dB，把频带内各 FFT 频点的功率相加，满幅正弦读 0 dB。 |
| `backgroundThrottle` | `boolean` | 否 | `true` | 为 `true` 时，别的程序在前台期间这个订阅每秒至多 12 帧，通常在 10 到 12 帧之间；为 `false` 时保持原帧率。 |
| `minFrequency` | `number` | 否 | `20` | 频带范围的下限（Hz），频带按对数等分这段范围。须为不小于 1 的有限数。 |
| `maxFrequency` | `number` | 否 | `sampleRate / 2` | 上限（Hz），须大于 `minFrequency`。出帧时按流采样率的一半截；不给时就取它。整段范围都高于这个频率时，出的是静音帧。 |
| `event` | `string` | 否 | `audio:spectrum` | 频谱数据的事件名。 |

**返回值**:

```json
{
    "success": true,
    "subscriptionId": "spectrum_main",
    "fftSize": 1024,
    "bands": 48,
    "fps": 30,
    "scale": "weighted",
    "backgroundThrottle": true,
    "minFrequency": 20,
    "maxFrequency": null,
    "event": "audio:spectrum",
    "streamReady": true
}
```

每个订阅各用自己的参数：别的订阅请求了更大的 FFT、更多的频带或更高的帧率，不会改变这个订阅收到的帧。登记成功恒回 `success: true`；`streamReady` 表示可视化流已建立，停止状态下也为 `true`。`fftSize` 不是范围内的 2 的幂、`scale` 不是 `weighted` / `db`、`backgroundThrottle` 不是布尔值、`minFrequency` 小于 1、`maxFrequency` 不大于 `minFrequency` 时，不登记任何订阅，回 `success: false`、`code: "INVALID_PARAMS"` 与 `details: { param, value }`。

每帧只发给发起订阅的窗口，事件名是订阅的 `event`：

| 字段 | 说明 |
| --- | --- |
| `subscriptionId` | 帧所属的订阅。同一事件名上有多个订阅时各收各的帧，按这个字段过滤。 |
| `spectrum` | 每个频带一个值，按订阅的 `scale` 给出。`weighted` 取值 `[0, 1]`；`db` 下限为 `-160`。 |
| `bands` | `spectrum` 的长度。 |
| `fftSize` | 实际使用的 FFT 大小。频带数达到 32、64 时分别至少提升到 4096、8192。 |
| `scale` | `weighted` 或 `db`。 |
| `sampleRate` | 可视化流的采样率（Hz）；未知时为 `0`。 |
| `minFrequency`、`maxFrequency` | 频带按对数等分的频率范围：从订阅的 `minFrequency`（缺省 20 Hz）到它的 `maxFrequency` 截到 `sampleRate / 2` 之后的值；没给上限时就是 `sampleRate / 2`。订阅应答回的是请求值，没给上限时 `maxFrequency` 为 `null`。 |
| `state` | `playing`、`paused` 或 `stopped`。播放暂停或停止时，每个订阅收到一帧带新状态的静音帧，之后到恢复播放前不再发帧。 |
| `streamTime` | 可视化流自上一次起播以来的秒数。起播、seek、手动换曲都会让它从 `0` 重新开始，并在 `0` 上停约 200 ms，这期间的帧是同一段音频；自然换曲不重置。 |
| `hostTime` | 宿主算出这一帧时的系统时间，Unix 纪元毫秒。`Date.now() - frame.hostTime` 就是投递延迟。 |

::: warning
`fftSize` 必须是 2 的幂（256 到 65536），否则返回错误。65536 是组件自己的封顶，不是 foobar2000 的限制；这一档每帧要处理约 1.5 秒的 PCM，只在需要原始 bin 分辨率时使用。
:::

::: tip 低频分辨率
C++ 层会根据请求的频段数自动提升 FFT 大小（≥64 bands → 8192，≥32 bands → 4096），以确保低频区域有足够的 bin 分辨率。频谱处理流水线包括：对数频率映射、sub-bin 线性插值、三角滤波器 RMS 平滑、频率倾斜补偿（+1.5 dB/octave）、dB 归一化和 gamma 校正（0.8）。
:::

::: tip 订阅与事件语义

- `subscriptionId` 只在 low-level `fb2k.invoke('audio.*')` 场景下需要；SDK `fb.audio.subscribeSpectrum()` 不公开它。
- 若重复传入同一个 `subscriptionId`，语义是更新/覆盖该订阅。
- 默认 `audio:spectrum` 与自定义 `event` 的 payload 都是上表的帧。

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | 否 | 省略时取消当前调用方的全部频谱订阅；调用方不带窗口时会取消所有调用方的全部订阅。 |

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

按需算一帧频谱（轮询模式）。至少要有一个频谱订阅。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `subscriptionId` | `string` | 否 | — | 按这个订阅的参数算这一帧，此时忽略 `bands`、`scale` 与频率范围。id 不存在时回 `NOT_FOUND`。 |
| `bands` | `integer` | 否 | `0` | 不给 `subscriptionId` 时生效。`0` 取所有订阅中最大的频带数；最多为 `fftSize / 2`。 |
| `scale` | `string` | 否 | `weighted` | 不给 `subscriptionId` 时生效。`weighted` 或 `db`，含义同 `audio.subscribeSpectrum`。 |
| `minFrequency` | `number` | 否 | `20` | 不给 `subscriptionId` 时生效。频带范围，规则同 `audio.subscribeSpectrum`。 |
| `maxFrequency` | `number` | 否 | `sampleRate / 2` | 不给 `subscriptionId` 时生效。频带范围，规则同 `audio.subscribeSpectrum`。 |

**返回值**:

```json
{
    "success": true,
    "subscriptionId": "spectrum_main",
    "spectrum": [0.1, 0.3, 0.5, ...],
    "bands": 96,
    "fftSize": 8192,
    "scale": "weighted",
    "sampleRate": 44100,
    "minFrequency": 20,
    "maxFrequency": 22050,
    "state": "playing",
    "streamTime": 12.34,
    "hostTime": 1790194482588.4
}
```

应答是一帧，字段同 [`audio.subscribeSpectrum`](#audio-subscribespectrum) 的帧，外加 `success`；`fftSize` 是实际使用的 FFT 大小。暂停或停止时回一帧静音帧，`state` 标明是哪种。没有任何订阅，或新建的流还没出数据（首个订阅后约 0.7 秒内），回 `success: false` 与 `error`；参数错误另带 `code` 与 `details`。

::: tip 拉取还是推送
两种方式拿到的是同一种帧。画面要跟显示器逐帧对齐、或每秒要 60 帧以上时用拉取：订阅时 `fps` 给 1 保活，在 `requestAnimationFrame` 里带 `subscriptionId` 调 `audio.getSpectrum`，上一次返回之后才发下一次，`streamTime` 没变的帧跳过。每次调用都在宿主主线程上算一次 FFT，开销与推送一帧相当，所以按需要的帧率限频，不要跟着显示器刷新率走。每秒 60 帧以内时，用推送同样可以。
:::

**示例**:

```javascript
// 在 requestAnimationFrame 里拉取：同时只发一个请求，每秒至多约 60 次
const { subscriptionId } = await fb2k.invoke('audio.subscribeSpectrum', { fps: 1 });
let pending = false;
let lastCall = 0;
let lastStreamTime;
function tick(now) {
    requestAnimationFrame(tick);
    if (pending || now - lastCall < 15) return;
    pending = true;
    lastCall = now;
    fb2k.invoke('audio.getSpectrum', { subscriptionId })
        .then((frame) => {
            if (frame.success && frame.state === 'playing' && frame.streamTime !== lastStreamTime) {
                lastStreamTime = frame.streamTime;
                draw(frame.spectrum);
            }
        })
        .finally(() => {
            pending = false;
        });
}
requestAnimationFrame(tick);
```

### audio.getWaveform

获取当前播放流的短波形片段。需要先调用 `subscribeSpectrum` 启动可视化流。

::: warning 注意
此 API 用于获取**当前播放流**的实时波形片段，不是离线文件波形。如需生成完整文件波形，请使用 `audio.generateFullWaveform`。
:::

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `duration` | `number` | 否 | `0.05` | 窗口时长（秒），大于 0 且不超过 1。窗口从当前播放位置开始，取的是已进输出缓冲的音频。 |
| `signed` | `boolean` | 否 | `false` | 为 `true` 时保留 PCM 极性，夹到 `[-1, 1]`；为 `false` 时取幅度，把 −70…0 dB 映射到 `[0, 1]`。 |
| `channels` | `string` | 否 | `'mix'` | `'mix'` 把各声道平均成一路放进 `waveform`；`'stereo'` 把前两路分别放进 `left` 与 `right`。 |
| `points` | `integer` | 否 | — | 等距取这么多个样本，不做平均。取 `[2, 65536]` 内的整数；不给时返回全部样本。 |

**返回值**:

```json
{ "success": true, "waveform": [0.01, 0.18, ...], "duration": 0.05, "signed": false,
  "channels": "mix", "sampleRate": 48000, "channelCount": 2 }
```

`channels: 'stereo'` 时没有 `waveform`，换成 `left` 与 `right`：

```json
{ "success": true, "left": [0.12, -0.08, ...], "right": [0.11, -0.07, ...], "duration": 0.05,
  "signed": true, "channels": "stereo", "sampleRate": 48000, "channelCount": 2 }
```

::: tip 返回值范围

- `signed: false`（默认）：各值是 dB 映射后的幅度，范围 `0..1`
- `signed: true`：各值保留 PCM 极性，夹到 `[-1, 1]`，适合绘制对称波形

:::

- 只有一路时 `right` 等于 `left`；多于两路时只取前两路，其余声道不混进来。`audio.setChannelMode` 设成 `'mono'` 之后可视化流只有一路。
- `sampleRate` 与 `channelCount` 是可视化流的采样率与声道数。
- `duration`、`channels`、`points` 不合法时回 `INVALID_PARAMS`。窗口长过输出缓冲里已有的音频时回 `No waveform data available`，没有频谱订阅时也是这一条。

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

// 两路分开、各等距取 256 个样本（声场图用）
const { left, right } = await fb2k.invoke('audio.getWaveform', {
    duration: 0.05,
    signed: true,
    channels: 'stereo',
    points: 256,
});
```

### audio.setChannelMode

设置频谱分析的声道模式。无效的 `mode` 值会自动规范化为 `"default"`，返回值中的 `mode` 反映规范化后的结果。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `mode` | `string` | 否 | `default` | 可取 `default` / `mono` / `front` / `back`，其他值归一为 `default`。 |

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `path` | `string` | 是 | — |  |
| `forceAnalysis` | `boolean` | 否 | `false` | 跳过既有 `BPM` 标签，直接进入流派估算。 |

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
    path: 'E:\\Music\\song.flac'
});
console.log(`BPM: ${result.bpm}, 来源: ${result.source}`);

// 强制重新分析
const result2 = await fb2k.invoke('audio.analyzeBPM', {
    path: 'E:\\Music\\song.flac',
    forceAnalysis: true
});
```

### audio.generateWaveform

::: danger 已废弃
此 API 为历史遗留接口，当前仅返回文件基本信息（duration、sampleRate、channels），不包含实际波形数据。**请使用 `audio.generateFullWaveform` 代替**，它提供完整的后台解码、缓存和事件通知功能。
:::

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `path` | `string` | 是 | — |  |
| `resolution` | `integer` | 否 | `800` | 截断到 50–4000。 |

**返回值**: `{"channels":"...","duration":"...","error":"...","requestedResolution":"...","sampleRate":"...","success":true}`


### audio.generateFullWaveform

生成完整文件波形数据，支持后台解码、缓存和异步事件通知。适用于进度条概览、波形卡片和章节预览。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `path` | `string` | 是 | — | 支持 `路径\|subsong:N`。 |
| `cueIndex` | `integer` | 否 | `-1` | 显式指定容器内曲目序号，优先级高于路径后缀。 |
| `resolution` | `integer` | 否 | `256` | 截断到 64–4096。 |
| `method` | `string` | 否 | `rms` | `rms` 或 `peak`。 |
| `scale` | `string` | 否 | `linear` | `linear` 或 `db`；`signed` 模式下被忽略。 |
| `signed` | `boolean` | 否 | `false` | 保留 PCM 极性，输出 `[-1, 1]`。 |
| `preferCache` | `boolean` | 否 | `true` | 优先返回缓存结果。 |


**返回值**: `{"cached":"...","channels":"...","duration":"...","maxAmplitude":"...","method":"...","path":"...","resolution":"...","sampleRate":"...","scale":"...","signed":"...","status":"...","success":true,"taskId":"...","waveform":{}}`

::: tip 路径与 fallback 语义

- C++ 层会先解析 `path|subsong:N`，再 canonicalize 路径，并把同一 canonical path 用于 `handle_create()`、解码器和缓存键。
- 若同时提供 `cueIndex` 与 `path|subsong:N`，最终以 `cueIndex` 为准。
- 若 cached info 的 `duration`、`samplerate`、`channels` 不完整，会先尝试 direct file read；只有 direct read 仍不足时才会进入失败事件。
- **归一化**: 按所选序列的最大值归一化（`rms` 取窗口 RMS 最大值、`peak` 取峰值最大值、`signed` 取绝对值最大值），该最大值以线性满幅单位随应答与就绪事件的 `maxAmplitude` 返回。`linear` 档用 `waveform[i] * maxAmplitude` 还原绝对电平；`db` 档 dBFS = `(v·60 − 60) + 20·log10(maxAmplitude)`。同一曲目、同一 `resolution` 只解码一次，其余 `method` / `signed` / `scale` 由缓存现算。
:::

**示例**:

```javascript
const result = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\Music\\song.flac',
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
    path: 'E:\\Music\\disc.flac|subsong:2',
    resolution: 512
});

// 使用 cueIndex（优先级高于路径中的 subsong）
const result3 = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\Music\\album.cue',
    cueIndex: 3,
    resolution: 256
});

// RMS 模式（更平滑的能量包络）
const result4 = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\Music\\song.flac',
    resolution: 512,
    method: 'rms'
});

// signed 模式：输出 [-1, 1] 对称波形（适合绘制上下对称波形图）
const result5 = await fb2k.invoke('audio.generateFullWaveform', {
    path: 'E:\\Music\\song.flac',
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

- 缓存键包含：规范化路径、subsong、resolution、文件大小、修改时间；条目存一次解码得到的三组原始窗口值，`method` / `signed` / `scale` 由缓存现算
- 最大缓存条目数：50（LRU 淘汰）
- 文件修改后自动失效

**在途去重与并发**:

- 缓存未命中时，解码键相同的在途请求并入同一次解码，各自拿自己的 `taskId`、按自己的 `method` / `signed` / `scale` 收到就绪事件
- 同时最多解码 2 首，其余按提交顺序排队；排队时间也计入调用方自己的超时
- 每个 `taskId` 恰好收到一次终态事件（就绪或失败，被取消时 `code` 为 `"CANCELLED"`）；缓存命中的同步应答没有事件
- 发起请求的弹窗关闭、或 foobar2000 退出时，未完成的请求直接丢弃、不发事件

### audio.cancelFullWaveform

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `taskId` | `string` | 是 | — | `audio.generateFullWaveform` 的 `pending` 应答里的 `taskId`。 |

**返回值**: `{"cancelled":"...","success":true}`

- 只认发起请求的调用方。`cancelled: false` 表示该任务已结束、不存在，或不归本调用方，三者故意不区分
- 取消成功时该 `taskId` 恰好收到一次 `audio:fullWaveformFailed`（`code: "CANCELLED"`），此后不再有它的事件；这条事件可能先于本应答到达
- 同一曲目还有别的请求在等时解码照常进行；没人等了，排队中的移出队列，解码中的被中止
- 缺 `taskId` 回 `REQUIRED_PARAM`；非字符串或空串回 `INVALID_PARAMS`

```javascript
const pending = await fb2k.invoke('audio.generateFullWaveform', { path: 'E:\\Music\\song.flac' });
if (pending.status === 'pending') {
    await fb2k.invoke('audio.cancelFullWaveform', { taskId: pending.taskId });
}
```

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `event` | `string` | 否 | `audio:stream` | 流数据的事件名。 |
| `interval` | `number` | 否 | `0.05` | 采样间隔（秒）。 |


**返回值**: `{"error":"...","event":"...","interval":"...","success":true}`

::: warning
此功能需要 `playback_stream_capture` 集成，当前未完整实现。
:::

### audio.unsubscribeStream

取消音频流捕获。

- **参数**: 无
- **返回值**: `{ "success": true }`

### audio.getSpectrumDebugState

获取频谱系统内部调试状态，包含当前订阅列表、分发目标、推帧计时线程的状态等。主要用于诊断频谱订阅问题。

- **参数**: 无
**返回值**: `{"active":true,"beatIntervalMs":"...","beatSource":"...","beatsCoalesced":"...","callerHwnd":"...","callerOwnsSubscription":"...","callerWindowId":"...","dispatchTargetCount":"...","dispatchTargets":[],"effectiveBands":"...","effectiveFftSize":"...","effectiveFps":"...","foregroundHwnd":"...","foregroundIsExternal":"...","foregroundPid":"...","foregroundTitle":"...","framesComputed":"...","instanceCount":"...","skipFrames":"...","streamReady":"...","subscriptionCount":"...","subscriptions":[],"success":true,"timerHwnd":"...","timerRunning":"..."}`

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `active` | boolean | 频谱系统是否活动 |
| `timerRunning` | boolean | 推帧的计时线程是否在运行 |
| `beatSource` | string \| null | 计时线程用的定时器：`high-resolution`，Windows 10 1803 之前的系统上为 `standard`；线程没在运行时为 `null` |
| `beatIntervalMs` | number \| null | 当前拍长（毫秒），即 1000 / `effectiveFps`；线程没在运行时为 `null` |
| `beatsCoalesced` | number | 上一拍还没到宿主主线程、因而被丢掉的拍数，累计值 |
| `timerHwnd` | number | 已废弃，恒为 `0` |
| `effectiveFftSize` | number | 所有订阅请求的 FFT 大小中的最大值；每个订阅仍按自己的参数计算 |
| `effectiveFps` | number | 所有订阅请求的帧率中的最大值，也是计时线程的节拍 |
| `effectiveBands` | number | 所有订阅请求的频带数中的最大值 |
| `framesComputed` | number | 宿主启动以来算过的 FFT 次数；暂停、停止期间不增加 |
| `streamReady` | boolean | 可视化流是否已建立（停止状态下也为 `true`） |
| `subscriptionCount` | number | 当前订阅数量 |
| `subscriptions` | array | 订阅详情列表，每项带 `scale`、`backgroundThrottle`、`minFrequency` 与 `maxFrequency`（跟随 `sampleRate / 2` 时为 `null`） |
| `callerOwnsSubscription` | boolean | 调用者是否拥有订阅 |
| `foregroundIsExternal` | boolean | 前台是否是别的程序，即 `backgroundThrottle: true` 的订阅此刻是否限帧 |

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

> `activePreset` 与 `activePresetIndex` 始终存在，无需判断键是否缺失：当前链不对应任何预设（或宿主不支持预设）时分别为 `null` 和 `-1`。手工改链的操作（`addDsp` / `removeDsp` / `moveDsp` / `setChain`）会使活动链脱离预设，此后这两个字段即为 `null` / `-1`。

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

> 未选中任何预设时 `selectedIndex` 为 `-1`。预设本身持久化在 profile 的 `dsp-presets\<名称>.fb2k-dsp`。

### dsp.applyPreset

应用指定的 DSP 预设。通过名称或索引指定。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 预设索引，来自 `dsp.getPresets`；与 `name` 同时提供时优先。 |
| `name` | `string` | 否 | 预设名称，来自 `dsp.getPresets`。 |

> `name` 和 `index` 至少提供一个；同时提供时 `index` 优先。

**返回值**: `{ "success": true, "appliedPreset": "My Preset", "appliedIndex": 0 }`

```javascript
// 按名称
await fb2k.invoke('dsp.applyPreset', { name: 'My Preset' });
// 按索引
await fb2k.invoke('dsp.applyPreset', { index: 0 });
```

> 应用预设会整条替换当前活动链（含各 DSP 的参数），因此它也是把链恢复到某个已知状态的最可靠方式。预设文件本身不会被改写——本接口只写活动链。

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

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `guid` | `string` | 是 | — | 已安装 DSP 的 GUID，来自 `dsp.getAvailable`。 |
| `position` | `integer` | 否 | `-1` | `-1` 表示追加到链尾。 |

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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 是 | 要移除的链内下标。 |

**返回值**: `{ "success": true, "removedDsp": "Equalizer", "removedIndex": 2 }`

### dsp.moveDsp

移动 DSP 效果器在链中的位置。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `from` | `integer` | 是 | 移动前的下标。 |
| `to` | `integer` | 是 | 移动后的最终下标。 |

**返回值**: `{"from":"...","message":"...","movedDsp":"...","success":true,"to":"..."}`

> `from` 是移动**前**的下标；`to` 是该项移动后的最终下标，升序、降序都与传入值一致，返回的 `to` 即为该值。`from === to` 时不做改动，返回 `message: "No change needed"`。需要重排链请用本接口，不要用 `setChain`（后者只接受 `guid`，不承诺保留参数）。

### dsp.setChain

设置完整的 DSP 效果器链（高级用法，替换整个链）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `dsps` | `array` | 是 | 有序链条目，每项为含 `guid` 的对象。 |

`dsps` 的每一项必须是含 `guid` 的对象。任意一项无法解析时整次调用被拒绝且**活动链保持不变**，错误信息带出错下标：

| 情况 | `error` |
| --- | --- |
| `dsps` 缺失或不是数组 | `dsps array is required` |
| 元素不是对象 | `dsps[0] must be an object` |
| 缺 `guid`、为空串，或不是字符串 | `dsps[0]: guid is required` |
| GUID 格式非法 | `dsps[0]: Invalid GUID format: <值>` |
| GUID 合法但该 DSP 未安装 | `dsps[0]: DSP not found or no default preset: <值>` |

传入空数组是合法的，表示清空整条链，返回 `count: 0`。

**返回值**: `{ "success": true, "count": 3 }`

```javascript
await fb2k.invoke('dsp.setChain', {
    dsps: [
        { guid: '{EQ-GUID-HERE}' },
        { guid: '{LIMITER-GUID-HERE}' }
    ]
});
```

> 传空数组会清空整条链。本接口只接受 `guid`，因此每个 DSP 都按其默认预设加入——参数是否得以保留**取决于该 DSP 的实现**：多数 foobar2000 内置 DSP 把设置存在全局配置里，参数会保留；而按预设实例存参的 DSP（VST 包装器、部分第三方 DSP）会回到默认值。不要依赖此行为，也不要把 `getChain` 的输出直接回灌 `setChain` 来做重排序，重排请用 `dsp.moveDsp`。

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

> **`guid` 在本端点内不唯一。** foobar2000 用全零 GUID
> `{00000000-0000-0000-0000-000000000000}` 表示某个输出模块的「默认设备」，因此多个模块下会各出现一次全零 GUID。
> 请用 `(entryGuid, guid)` 组合作为设备的唯一键，不要只用 `guid`。

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
    "note": "Output settings are managed through foobar2000 Preferences > Playback > Output. availableOutputs lists display names only and cannot disambiguate backends that share a name; use output.getEntries for name + GUID pairs.",
    "availableOutputs": ["WASAPI (event)", "WASAPI (push)", "DirectSound", "Primary Sound Driver"]
}
```

> 实际输出设置通过 foobar2000 首选项管理。如需切换输出设备，请使用 `config.setOutputDevice`。

> **不建议在新代码中使用 `availableOutputs`。** 它只是一个显示名数组，存在两个已实测的问题：
> 同名模块无法区分（多个后端都叫「默认」），以及某些模块的名称为空字符串。
> 此外该数组的顺序来自服务枚举，**多次调用之间并不稳定**，因此不能依赖数组下标定位模块。
> 需要可编程地识别输出模块时请改用 `output.getEntries`，它为每个名称附带 GUID。

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
| `sourceMode` | string | 音源模式 |
| `processingMode` | string | "none" / "gain" / "gain_and_peak" / "peak" |
| `preampWithRg` | number | 有 RG 时的预增益 (dB) |
| `preampWithoutRg` | number | 无 RG 信息时的预增益 (dB) |
| `active` | boolean | RG 是否激活 |

### replaygain.getMode

获取当前 ReplayGain 模式。

- **参数**: 无
- **返回值**: `{ "sourceMode": "track", "processingMode": "gain" }`

### replaygain.setMode

设置 ReplayGain 模式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sourceMode` | `string` | 否 | 可取 `none` / `track` / `album` / `auto`（别名 `byPlaybackOrder`）。 |
| `processingMode` | `string` | 否 | 可取 `none` / `gain` / `gain_and_peak` / `peak`。 |

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
| `withRg` | number | 有 RG 时的预增益 (dB) |
| `withoutRg` | number | 无 RG 信息时的预增益 (dB) |

### replaygain.setPreamp

设置预增益值。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `withRg` | `number` | 否 | 有 RG 信息时的预增益（dB），截断到 -24..+24。 |
| `withoutRg` | `number` | 否 | 无 RG 信息时的预增益（dB），截断到 -24..+24。 |

**返回值**: `{ "success": true, "withRg": 3.0, "withoutRg": 0.0, "changed": true }`

```javascript
await fb2k.invoke('replaygain.setPreamp', { withRg: 3.0, withoutRg: -3.0 });
```

### replaygain.get

获取指定文件的 ReplayGain 信息。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 是 | 文件路径数组。 |

**返回值**:

```json
{
    "success": true,
    "count": 1,
    "results": [
        {
            "path": "C:\\Music\\song.flac",
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

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 是 | 文件路径数组。 |

**返回值**: `{ "success": true, "clearedCount": 5 }`

### replaygain.scan

扫描文件的 ReplayGain（通过右键菜单触发，扫描结果自动写入文件）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `paths` | `array` | 是 | — | 要扫描的文件路径数组。 |
| `mode` | `string` | 否 | `track` | `track` 或 `album`。 |

**返回值**: `{ "success": true, "scannedCount": 10, "mode": "track", "note": "Scan started. Results will be written to files automatically." }`

```javascript
// 扫描单曲 track gain
await fb2k.invoke('replaygain.scan', {
    paths: ['C:\\Music\\song.flac'],
    mode: 'track'
});
// 扫描整张专辑
await fb2k.invoke('replaygain.scan', {
    paths: ['C:\\Music\\01.flac', 'C:\\Music\\02.flac'],
    mode: 'album'
});
```

## 运行时行为说明

- `audio.subscribeSpectrum` 会创建或更新由调用方拥有的订阅。省略 `subscriptionId` 时，运行时按调用方生成一个旧式标识；监听配置的 `event`，默认值为 `audio:spectrum`。
- 频谱订阅之间不共享参数。宿主的计时线程用高精度定时器，按请求中最高的 `fps` 走拍，每个订阅到了自己的间隔才出帧，所以推送的实际帧率接近请求值；实际 FFT 大小、频带数、`scale` 与频率范围都相同的订阅每拍共用一次 FFT。宿主主线程的开销随帧数增加，每帧约 0.5 ms。
- `db` 档反映 ReplayGain 与 DSP 处理之后、音量控制之前的信号，调节音量不改变读数。
- `audio.getSpectrum` 与 `audio.getWaveform` 读取可视化流。在存在频谱订阅且有可用音频数据前，它们会返回错误。
- `audio.generateWaveform` 当前会返回文件元数据以及“尚未实现基于解码器的波形生成”的失败结果。异步且带缓存的流程应使用 `audio.generateFullWaveform`。
- `audio.generateFullWaveform` 命中缓存时返回带数据的 `status: "ready"`，否则返回带 `taskId` 的 `status: "pending"`。调用方会收到 `audio:fullWaveformReady` 或 `audio:fullWaveformFailed`；非负的 `cueIndex` 优先于 `path|subsong:N` 后缀。
- `audio.subscribeStream` 是能力 stub：在集成 `playback_stream_capture` 前始终返回 `success: false`。调用 `audio.unsubscribeStream` 仍然安全。
- 每个构建都会注册 DSP 方法。若 foobar2000 未提供 DSP SDK 接口，全部 `dsp.*` 方法都会返回 runtime 的 "DSP API not available in this build" 失败，不会模拟 DSP 链。
- `output.getSettings` 仅提供只读发现信息。输出配置由 foobar2000 Preferences 管理，而非本 API。
- `replaygain.get` 读取每个传入媒体路径；`replaygain.clear` 通过 foobar2000 异步写入 ReplayGain 元数据。`replaygain.scan` 只是请求宿主扫描器开工，不同步返回分析结果。
