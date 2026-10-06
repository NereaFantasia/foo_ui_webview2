# fb.audio 音频分析

## subscribeSpectrum(callback, options?)

订阅实时频谱数据。自动启动 C++ 频谱采集流，返回取消订阅函数（同时停止采集）。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| callback | function | 只接收本订阅的帧。频带输出时类型为 `AudioSpectrumPayload`：`spectrum` 加上 [`audio.subscribeSpectrum`](../api/audio.md#audio-subscribespectrum) 列出的帧字段（`subscriptionId`、`bands`、`fftSize`、`scale`、`sampleRate`、`state`、`streamTime`、`hostTime` 等），旧版宿主只发 `spectrum`。`output: 'bins'` 时类型为 `SpectrumBinsFrame`：`firstBin` 加上 `channels: 'mix'` 时的 `spectrum`，或 `'stereo'` 时的 `left` 与 `right` |
| options.fftSize | number | FFT 大小，2 的幂（256–65536），默认 1024。65536 是组件自己的封顶，只在需要原始 bin 分辨率时才有意义。频点输出按给定值算；频带输出在 32 带以上会提升点数 |
| options.bands | number | 频带输出的频段数，默认 48 |
| options.fps | number | 刷新率 (1-60)，默认 30 |
| options.scale | `'weighted' \| 'db'` | `'weighted'`（默认）是取值 `[0, 1]` 的显示用曲线；`'db'` 是频带功率 dB，满幅正弦读 0 dB。频点输出恒为 dB：不给或给 `'db'` |
| options.output | `'bands' \| 'bins'` | `'bins'` 不出频带，改出 FFT 的线性频点，逐频点给 dB 功率，见[频点输出](../api/audio.md#频点输出)。`'bands'`（默认）为兼容保留 |
| options.channels | `'mix' \| 'stereo'` | 只用于频点输出：`'mix'`（默认）把各声道平均进 `spectrum`，`'stereo'` 分出 `left` 与 `right` |
| options.backgroundThrottle | boolean | 默认 `true`：别的程序在前台期间每秒至多 12 帧，通常在 10 到 12 帧之间。传 `false` 保持原帧率 |
| options.minFrequency | number | 频带范围的下限（Hz），不小于 1，默认 20 |
| options.maxFrequency | number | 上限（Hz），须大于 `minFrequency`；按流采样率的一半截，不给时也取这个值 |
| options.event | string | 自定义事件名，默认 "audio:spectrum"。多面板场景可用不同事件名隔离数据 |

返回的取消函数带一个 `ready` 属性：宿主答复后兑现的 Promise，永不 reject。登记成功时得到 `{ ok: true, subscriptionId, fftSize, bands, fps, scale, output, channels, backgroundThrottle, minFrequency, maxFrequency, streamReady }`（范围跟随采样率一半时 `maxFrequency` 为 `null`；不支持频点输出的宿主对频点请求报 `output: 'bands'`，这时回调收不到帧），否则得到 `{ ok: false, code, error }`：参数被拒是 `INVALID_PARAMS`，没有宿主是 `NOT_SUPPORTED`，调用本身失败是 `UNKNOWN_ERROR`。

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

// 左右两路的线性频点，单位 dB
const bins = fb.audio.subscribeSpectrum(
    (frame) => {
        if (frame.channels === 'stereo') console.log(frame.firstBin, frame.left.length, frame.right.length);
    },
    { output: 'bins', channels: 'stereo', fftSize: 16384, backgroundThrottle: false }
);
const binsOutcome = await bins.ready;
if (binsOutcome.ok && binsOutcome.output !== 'bins') console.warn('this host has no bin output');

// 取消订阅（同时停止 C++ 采集，释放可视化资源）
unsubscribe();
unsubscribeCustom();
bins();
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
| options.subscriptionId | string | 按这个订阅的参数算这一帧，包括频点输出（此时忽略 `bands`、`scale`、`output`、`channels` 与频率范围） |
| options.bands | number | 本次轮询期望返回的频段数；未传或为 `0` 时取各订阅中最大的频段数 |
| options.scale | `'weighted' \| 'db'` | 不给 `subscriptionId` 时本次轮询的档位，默认 `'weighted'` |
| options.minFrequency / options.maxFrequency | number | 不给 `subscriptionId` 时本次轮询的频带范围，规则同 `subscribeSpectrum` |
| options.output / options.channels | string | 不给 `subscriptionId` 时只接受 `'bands'` 与 `'mix'`；频点输出要有自己的订阅 |

```javascript
// 先启动频谱流
const unsubscribe = fb.audio.subscribeSpectrum(() => {}, { bands: 96 });

// 单次轮询当前频谱
const result = await fb.audio.getSpectrum({ bands: 96 });
if (result.success === false) throw new Error(result.error);
console.log(result.spectrum);
console.log(result.bands);

unsubscribe();
```

## getWaveform(options?)

获取当前播放流的短波形片段。需要先调用 `subscribeSpectrum` 启动可视化流。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options.duration | number | 窗口时长（秒），大于 0 且不超过 1，取截至可视化流当前时刻的最近 `duration` 秒。默认 `0.05` |
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
if (result.success === false) throw new Error(result.error);
console.log('波形数据:', result.waveform);

// 两路分开，各取 256 个样本
const res = await fb.audio.getWaveform({
    duration: 0.05,
    signed: true,
    channels: 'stereo',
    points: 256,
});
if (res.success === false) throw new Error(res.error);
const { left, right } = res;
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
if (pending.success === false) throw new Error(pending.error);
if (pending.status === 'pending') {
    const res = await fb.audio.cancelFullWaveform(pending.taskId);
    if (res.success === false) throw new Error(res.error);
    const { cancelled } = res;
}
```

## decodePcm(path, options?)

实验性 API，后续版本可能发生变化。

把曲目或其中一段解成 float32 PCM，返回一个原地读取样本的 `PcmBuffer`，用法类似 Web Audio 的 `decodeAudioData` 交回 `AudioBuffer`。宿主经共享缓冲投递样本，SDK 负责把它与 `audio:pcmReady` 事件配对；用了这个方法的页面不要再自己处理 `audio.decodePcm` 的 `sharedbufferreceived`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 曲目路径；带 `\|subsong:N` 后缀时选子曲目 |
| options.cueIndex | number | cue 子曲目序号，从 0 起；优先于路径里的子曲目 |
| options.start | number | 起点，秒；缺省为 `0` |
| options.end | number | 终点，秒；缺省到曲目末尾，超过曲目长度时按末尾截 |
| options.sampleRate | number | 要转换到的采样率，8000 至 192000；缺省用源采样率 |
| options.mono | boolean | 各声道等权平均成单声道 |
| options.signal | AbortSignal | 中止时取消宿主任务，并以名为 `AbortError` 的 `DOMException` reject |
| options.timeoutMs | number | 等待上限，毫秒，含排队时间；缺省 `120000`，`0` 表示一直等 |

**返回值：** `PcmBuffer` 的 Promise：

| 成员 | 说明 |
| --- | --- |
| `sampleRate`、`channels`、`frames` | 样本格式；设了 `mono` 时 `channels` 为 1 |
| `duration` | `frames / sampleRate`，秒 |
| `start` | 第一帧对应的曲目时刻，秒 |
| `truncated` | 音频超出了按曲目长度估算的大小，或中途换了格式；样本到截断处为止 |
| `resampled` | 经 foobar2000 的重采样器转换到了 `sampleRate` |
| `getChannelView(c)` | 声道 `c` 的零拷贝 `Float32Array`，长 `frames` |
| `toAudioBuffer()` | 把样本拷进一个新的 `AudioBuffer` |
| `transfer()` | 交出底层 `ArrayBuffer`，供 `worker.postMessage(buffer, [buffer])` |
| `release()` | 释放共享内存；重复调用无妨 |
| `available` | `release()` 或 `transfer()` 之后为 `false` |

::: danger 只读内存
`getChannelView()` 返回的是只读共享内存上的视图。写入会让渲染进程崩溃，整个 foobar2000 窗口随之失效。这个方法刻意不叫 `getChannelData`：Web Audio 的同名方法返回可以修改的数组。要可改的副本，用 `toAudioBuffer()`。
:::

用完调用 `release()`。`transfer()` 之后这个 `PcmBuffer` 不再给出视图，`release()` 也不起作用；此前取得的视图仍可读，直到缓冲真被 `postMessage` 转走，之后这些视图分离（`byteLength` 为 0）。Worker 里释放不了缓冲，想早点释放，就把它转回页面再交给 `chrome.webview.releaseBuffer`。缓冲开头仍是它的头部，可以用 SDK 的 `readPcmHeader()` 读。

**失败**时以 `PcmDecodeError` reject，其 `code` 取宿主的错误码：`INVALID_PARAMS`（区间超出单块缓冲上限时也是它：64 位 foobar2000 为 256 MiB，32 位为 64 MiB）、`INVALID_PATH`、`NO_INFO`、`NOT_SUPPORTED`（不在 WebView2 宿主里、运行时不支持共享缓冲，或没有重采样器支持这次转换）、`CANCELLED`、`DECODE_FAILED`，以及 [`audio.decodePcm`](../api/audio.md#audio-decodepcm) 列出的其他错误码。超时以 `code: 'TIMEOUT'` reject。中止与超时都会取消宿主任务。

同一时间只解一个任务，其余排队，排队时间计入 `timeoutMs`。相同的请求在途时共用一次解码。

```javascript
const pcm = await fb.audio.decodePcm('E:\\Music\\song.flac', {
    start: 30,
    end: 60,
    sampleRate: 22050,
    mono: true,
});
try {
    const samples = pcm.getChannelView(0);
    let sum = 0;
    for (const s of samples) sum += s * s;
    console.log('RMS', Math.sqrt(sum / pcm.frames));
} finally {
    pcm.release();
}
```

## cancelDecodePcm(taskId)

实验性 API，后续版本可能发生变化。

按 `taskId` 取消尚未结束的 `audio.decodePcm` 任务。`decodePcm()` 在中止与超时时已经会调用它；只有用 `fb2k.invoke` 发起的任务才需要直接调用。`cancelled: false` 表示任务已结束、不存在或属于别的页面。被取消的任务收到一次 `code: 'CANCELLED'` 的 `audio:pcmFailed`。

```javascript
const pending = await fb2k.invoke('audio.decodePcm', { path: 'E:\\Music\\song.flac' });
if (pending.success === false) throw new Error(pending.error);
const res = await fb.audio.cancelDecodePcm(pending.taskId);
if (res.success === false) throw new Error(res.error);
const { cancelled } = res;
```

## subscribeStream(options?)

实验性 API，后续版本可能发生变化。

签名：`fb.audio.subscribeStream(options?: PcmStreamOptions): PcmStream`

订阅 foobar2000 正在播放的音频，立即返回 `PcmStream` 句柄。宿主把播出的每一块写进与页面共享的环形缓冲；`read()` 把上次调用之后写入的帧拷出来，每个声道一个 `Float32Array`，并带 `dropped`——还没读就被环覆盖掉的帧数。在 `requestAnimationFrame` 或定时器里轮询它：宿主无法在帧到达时通知页面。

- 样本是 DSP 链的输出，在 ReplayGain 与音量之前；与 `audio:spectrum` 不同，不含 ReplayGain。
- `ready` 以宿主的应答 resolve，从不 reject：`{ ok: true, subscriptionId }`，或 `{ ok: false, code, error }`——不在 WebView2 宿主页面里或运行时不支持共享缓冲时 `NOT_SUPPORTED`，本页面已有 8 个流订阅时 `OPERATION_FAILED`，参数越界时 `INVALID_PARAMS`。
- `format` 在首块音频到达前为 `null`，之后是 `{ sampleRate, channels, capacityFrames, epoch }`；`onFormat(listener)` 在首块缓冲到达时触发，采样率或声道数变化时再触发。换格式前还没读走的旧格式帧随旧缓冲一起丢弃，计入下一次返回帧的 `read()` 的 `dropped`。
- 没有新写入（停止、暂停）、首块缓冲未到、流已结束时 `read()` 返回 `null`。
- 结果里还有 `segments`：在媒体时间重新起算处（起播、seek、换曲）把帧切开，每段为 `{ offset, segment, startSeconds, reason, estimated }`，`offset` 是这一段在各声道数组里的起始下标。写版本 1 缓冲的宿主没有段表，恒为一段、`segment: null`。
- `unsubscribe()` 之后、宿主拒绝订阅时、或宿主停止写入且一秒内没有新缓冲跟上时，`ended` 变为 `true`，`onEnded(listener)` 触发一次。
- `options.interval` 向核心请求回调间隔，秒（0.01 至 0.2）。核心按约 16 ms 的步长取整，缺省 200 ms，并对全组件的流订阅采用其中最短的请求值。`options.bufferSeconds`（0.1 至 10，缺省 1）决定环的长度；起播后的首块最多带 0.8 s 音频。
- 用完调 `unsubscribe()`：在宿主移除订阅，并释放页面对共享内存的视图。

```javascript
const stream = fb.audio.subscribeStream({ bufferSeconds: 0.5 });
const outcome = await stream.ready;
if (outcome.ok === false) throw new Error(outcome.code);
const tick = () => {
    const chunk = stream.read();
    if (chunk) {
        const left = chunk.planes[0];
        let peak = 0;
        for (const sample of left) peak = Math.max(peak, Math.abs(sample));
        console.log(stream.format?.sampleRate, chunk.frames, chunk.dropped, peak);
    }
    if (!stream.ended) requestAnimationFrame(tick);
};
requestAnimationFrame(tick);
// 之后
stream.unsubscribe();
```

### 自己算频谱

频点输出用的是 foobar2000 的 FFT，窗函数固定为高斯窗。要自选窗函数、补零、组合多种 FFT 点数或换别的变换，就从实时 PCM 自己算：

- 请求最短的间隔 `interval: 0.016`；缺省 200 ms 时频谱每秒只变五次。这个间隔对全组件的流订阅生效。
- 这时平均每 16 ms 来一块，但间隔不匀（实测 3 到 50 ms），有些显示帧拿不到新样本，重画上一帧频谱或做时间平滑即可。
- 样本取在 ReplayGain 之前，电平与 `audio:spectrum` 差一个 ReplayGain 增益。它比听到的声音晚的时间与可视化流相当，实测十几毫秒，另加等下一块的时间。
- `read()` 只返回上次调用之后写入的帧，最近 `N` 个样本要自己留着，FFT 库也要自带。seek 或换曲后 `segments` 标出媒体时间重新开始的位置；不想让一帧频谱跨过这里，就在这里清空历史。

```javascript
/**
 * 左声道的 4096 点频谱，每个显示帧重画一次。
 * @param {(samples: Float32Array) => Float32Array} fft 你的 FFT 库算出的幅度
 * @param {(spectrum: Float32Array) => void} draw
 */
function startLiveSpectrum(fft, draw) {
    const N = 4096;
    const history = new Float32Array(N); // 最近 N 个样本，旧的在前
    const hann = Float32Array.from({ length: N }, (_, i) => 0.5 - 0.5 * Math.cos((2 * Math.PI * i) / (N - 1)));
    const live = fb.audio.subscribeStream({ interval: 0.016, bufferSeconds: 1 });
    const tick = () => {
        const chunk = live.read();
        if (chunk) {
            const fresh = chunk.planes[0];
            const keep = Math.max(0, N - fresh.length);
            history.copyWithin(0, N - keep); // 丢掉最旧的样本
            history.set(fresh.subarray(Math.max(0, fresh.length - N)), keep);
            draw(fft(history.map((s, i) => s * hann[i])));
        }
        if (!live.ended) requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
    return live;
}
```

## unsubscribeStream(subscriptionId?)

实验性 API，后续版本可能发生变化。

签名：`fb.audio.unsubscribeStream(subscriptionId?: string): Promise<AudioUnsubscribeStreamResponse>`

在宿主移除本页面的流订阅：给了 `subscriptionId` 就移除那一个，不给就全部移除。`PcmStream.unsubscribe()` 已经会为自己的订阅做这件事；只有用 `fb2k.invoke` 发起的订阅才需要直接调用。resolve 值里的 `removed` 是移除的订阅数；别的页面的订阅不计也不动。

```javascript
const res = await fb.audio.unsubscribeStream();
if (res.success === false) throw new Error(res.error);
const { removed } = res;
```

## analyzeBPM(path, options?)

读取文件的 `BPM` 标签，宿主不做节拍检测。没有标签、或标签不是 0 到 500 之间（不含两端）的数字时，返回 `success: false` 与 `code: 'NOT_FOUND'`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| path | string | 音频文件路径 |
| options.forceAnalysis | boolean | 已废弃，不起作用 |

```javascript
const result = await fb.audio.analyzeBPM('E:\\Music\\song.flac');
if (result.success) {
    console.log(`BPM: ${result.bpm}`);
}
```

## setChannelMode(mode)

选择可视化流带哪些声道，作用于频谱帧与 `getWaveform`，不影响播放出来的声音。其他 `mode` 值以 `INVALID_PARAMS` 失败。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| mode | `'default' \| 'mono' \| 'front' \| 'back'` | 可视化流带的声道 |

```javascript
const res = await fb.audio.setChannelMode('mono'); // mode === 'mono';
if (res.success === false) throw new Error(res.error);
const { mode } = res;
```

## getSpectrumDebugState()

获取频谱系统内部调试状态。返回当前订阅列表（每项带 `scale`、`backgroundThrottle` 与频率范围）、分发目标、各订阅请求值中最大的 FFT/FPS/Bands、`framesComputed`、推帧计时线程的状态（`timerRunning`、`beatSource`、`beatIntervalMs`、`beatsCoalesced`）等诊断信息。

```javascript
const debug = await fb.audio.getSpectrumDebugState();
if (debug.success === false) throw new Error(debug.error);
console.log(debug.subscriptions, debug.effectiveFps);
```

## getPcmDebugState()

实验性 API，后续版本可能发生变化。

返回本页面能否接收共享缓冲（`runtime.environment12`、`runtime.webview17` 与本机安装的 WebView2 版本）以及解码队列的状态（`decode.active`、`decode.queued`、`decode.openBufferBytes`），供测试与排查用。

```javascript
const res = await fb.audio.getPcmDebugState();
if (res.success === false) throw new Error(res.error);
const { runtime, decode } = res;
console.log(runtime.version, decode.active);
```

## getOutputInfo()

签名：`fb.audio.getOutputInfo(): Promise<AudioGetOutputInfoResponse>`

返回当前的原生音量（dB）与 0–100 的 `volumePercent`。

```javascript
const output = await fb.audio.getOutputInfo();
if (output.success === false) throw new Error(output.error);
console.log(output.volume, output.volumePercent);
```

## getStreamInfo()

签名：`fb.audio.getStreamInfo(): Promise<AudioGetStreamInfoResponse>`

返回 `{ playing }`；正在播放时还带上能取到的 `sampleRate`、`channels`、`bitrate`、`codec` 与 `duration`。

```javascript
const stream = await fb.audio.getStreamInfo();
if (stream.success === false) throw new Error(stream.error);
console.log(stream.channels, stream.sampleRate);
```

## isVisualizationAvailable()

签名：`fb.audio.isVisualizationAvailable(): Promise<AudioIsVisualizationAvailableResponse>`

以 `available` 报告 foobar2000 是否提供可视化流；频谱帧与 `getWaveform()` 都要靠它。

```javascript
const res = await fb.audio.isVisualizationAvailable();
if (res.success === false) throw new Error(res.error);
const { available } = res;
```
