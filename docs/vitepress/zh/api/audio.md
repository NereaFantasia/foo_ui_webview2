# Audio 音频 API

`audio` 命名空间的方法：音频分析、频谱、波形与 PCM 数据。

## Audio API - 音频分析

### audio.subscribeSpectrum

<!-- api-schema:begin audio.subscribeSpectrum -->
订阅正在播放的音频的频谱帧。每个订阅保留自己的参数；帧只发给调用方窗口，事件名为 `event`，播放时每秒最多 `fps` 帧，暂停或停止时再发一帧静音帧。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | 否 | 订阅的键；用同一 id 再订阅会替换该订阅。缺省时由调用方窗口与 `event` 推出。不能为空。 |
| `fftSize` | `integer` | 否 | FFT 点数，须为 2 的幂，否则返回 `INVALID_PARAMS`。频带输出在 32 个频带及以上时会提升它；频点输出按给定值用。65536 是本组件自定的上限，不是 foobar2000 的限制，每帧要约 1.5 s 的 PCM。取值 `256` 到 `65536`（含端点）。默认 `1024`。 |
| `bands` | `integer` | 否 | 仅频带输出：频带数，夹到 8 到 `fftSize / 2`。默认 `48`。 |
| `fps` | `integer` | 否 | 每秒帧数，夹到 1 到 60。默认 `30`。 |
| `scale` | `"weighted" \| "db"` | 否 | 频带刻度，缺省为 `weighted`。`output: 'bins'` 时不传或传 `db`，其他值返回 `INVALID_PARAMS`。 |
| `output` | `"bands" \| "bins"` | 否 | 帧里给什么；频点输出时忽略 `bands`。默认 `"bands"`。 |
| `channels` | `"mix" \| "stereo"` | 否 | 仅频点输出；频带输出配 `stereo` 返回 `INVALID_PARAMS`。默认 `"mix"`。 |
| `backgroundThrottle` | `boolean` | 否 | 为 `true` 时，别的应用在前台期间本订阅每秒最多 12 帧，通常 10 到 12；`false` 保持全速。默认 `true`。 |
| `minFrequency` | `number` | 否 | 频率范围下限，Hz；频带在范围内按对数划分。不小于 `1`。默认 `20`。 |
| `maxFrequency` | `number` | 否 | 上限，Hz，须大于 `minFrequency`。出帧时截到流采样率的一半，缺省时就取它。整个范围都高于它时出静音帧。 |
| `event` | `string` | 否 | 帧所用的事件名。不能为空。默认 `"audio:spectrum"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `subscriptionId` | `string` | 订阅的键，调用方给的或推出的。 |
| `fftSize` | `integer` | 请求的 FFT 点数；帧里报实际用的点数。 |
| `bands` | `integer` | 夹取后的频带数。 |
| `fps` | `integer` | 夹取后的帧率。 |
| `scale` | `"weighted" \| "db"` | 使用的刻度；频点输出恒为 `db`。 |
| `backgroundThrottle` | `boolean` | 同请求。 |
| `minFrequency` | `number` | 范围下限，Hz，同请求。 |
| `maxFrequency` | `number \| null` | 请求的上限，Hz；范围跟随流采样率一半时为 `null`。 |
| `output` | `"bands" \| "bins"` | 登记的输出。 |
| `channels` | `"mix" \| "stereo"` | 登记的声道布局。 |
| `event` | `string` | 帧所用的事件名。 |
| `streamReady` | `boolean` | 可视化流建好后为 `true`，停止态也算；有没有音频看帧的 `state`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

别的订阅请求了更大的 FFT、更多的频带或更高的帧率，不会改变这个订阅收到的帧。登记成功恒回 `success: true`；`streamReady` 表示可视化流已建立，停止状态下也为 `true`。参数不合法时不登记任何订阅，回 `INVALID_PARAMS`；其中 `fftSize` 不是 2 的幂、`maxFrequency` 不大于 `minFrequency`、频带输出配 `channels: 'stereo'`、频点输出配 `db` 以外的 `scale` 这几种还带 `details: { param, value }`。

每帧只发给发起订阅的窗口，事件名是订阅的 `event`：

| 字段 | 说明 |
| --- | --- |
| `subscriptionId` | 帧所属的订阅。同一事件名上有多个订阅时各收各的帧，按这个字段过滤。 |
| `output` | `bands` 或 `bins`。旧版宿主不带这个字段，发的是频带帧。 |
| `spectrum` | 频带帧：每个频带一个值，按订阅的 `scale` 给出，`weighted` 取值 `[0, 1]`，`db` 下限为 `-160`。`channels: 'mix'` 的频点帧：每个频点一个 dB 功率值。 |
| `left`、`right` | `channels: 'stereo'` 的频点帧用它们代替 `spectrum`：可视化流的前两路；单声道流时 `right` 等于 `left`。 |
| `bands` | 频带帧：`spectrum` 的长度。频点帧不带。 |
| `firstBin` | 频点帧：第一个值的 FFT 频点序号；第 `i` 个值的中心频率是 `(firstBin + i) * sampleRate / fftSize` Hz。区间里没有频点时为 `0`，数组为空。 |
| `fftSize` | 实际使用的 FFT 大小。频带输出在频带数达到 32、64 时分别至少提升到 4096、8192；频点输出按请求值。 |
| `scale` | `weighted` 或 `db`；频点帧恒为 `db`。 |
| `channels`、`channelCount` | 频点帧：订阅的 `channels` 与可视化流的声道数，未知时为 `0`。 |
| `sampleRate` | 可视化流的采样率（Hz）；未知时为 `0`。 |
| `minFrequency`、`maxFrequency` | 频率范围，频带按对数等分它，频点的中心频率须落在其中：从订阅的 `minFrequency`（缺省 20 Hz）到它的 `maxFrequency` 截到 `sampleRate / 2` 之后的值；没给上限时就是 `sampleRate / 2`。订阅应答回的是请求值，没给上限时 `maxFrequency` 为 `null`。 |
| `state` | `playing`、`paused` 或 `stopped`。播放暂停或停止时，每个订阅收到一帧带新状态的静音帧，之后到恢复播放前不再发帧。 |
| `streamTime` | 可视化流自上一次起播以来的秒数；这一帧由截至这一时刻的最近 `fftSize` 个样本算出。起播、seek、手动换曲都会让它从 `0` 重新开始，并在 `0` 上停约 200 ms，这期间是静音帧；走满 `fftSize / sampleRate` 秒之前，窗口里早于重新计时的部分按静音算。自然换曲不重置。可视化流比听到的声音晚十几毫秒（本机实测 13 ms）。 |
| `hostTime` | 宿主算出这一帧时的系统时间，Unix 纪元毫秒。`Date.now() - frame.hostTime` 就是投递延迟。 |

#### 频点输出

`output: 'bins'` 时每帧给出 FFT 的线性频点，不经过频带那一套处理：不对窄的低频带插值，不加权，也不提升 `fftSize`。怎么分组、插值、平滑都由页面决定。

- 每个值是 `10 · log10(p) − 2.75` dB，`p` 是该频点幅度的平方（`stereo`）或它在各声道上的平均（`mix`），按 0.01 dB 取整，下限 `-160`。满幅正弦主瓣所落各频点的功率之和是 0 dB，所以把频点按功率相加，即 `10 · log10(Σ 10^(v / 10))`，就得到同一区间的 `db` 频带值。单看峰值频点会低一些，因为 foobar2000 的 FFT 用高斯窗，正弦的能量会分到相邻频点上：48 kHz、8192 点下，0 dBFS 的 1 kHz 正弦在峰值频点读 -3.18 dB，距峰值 40 dB 以内的有 6 个频点。
- 频点 `k`（从 1 起，直流频点不发）的中心频率 `k · sampleRate / fftSize` 落在 `[minFrequency, maxFrequency)` 内时才出。要 20 Hz 以下的频点，把 `minFrequency` 设得更低。
- 静音帧沿用该订阅上一帧的长度与 `firstBin`，值全为 `-160`；之前没有帧时为空。
- 要自选窗函数、补零或换别的变换，就用 `audio.subscribeStream` 的 PCM 自己算，见[自己算频谱](../sdk/audio.md#自己算频谱)。

帧走 JSON，每个值约 7 字节。下表是 48 kHz、缺省区间下的体积与拉取往返时间，按同样大小的 JSON 应答在一台机器上实测后估算：

| `fftSize` | `channels` | 值个数 | 体积 | 往返 |
| --- | --- | --- | --- | --- |
| 4096 | `mix` | 2046 | 14 KB | 0.5 ms |
| 16384 | `mix` | 8185 | 56 KB | 1.1 ms |
| 16384 | `stereo` | 16370 | 112 KB | 1.9 ms |
| 65536 | `stereo` | 65480 | 448 KB | 6.6 ms |

这些时间大多花在 foobar2000 的主线程上，帧率要按体积选。推送的一拍耗时超过帧间隔时，所有订阅都会跳过下一拍。

::: warning
`fftSize` 必须是 2 的幂（256 到 65536），否则返回错误。65536 是组件自己的封顶，不是 foobar2000 的限制；这一档每帧要处理约 1.5 秒的 PCM，只在需要原始 bin 分辨率时使用。
:::

::: tip 频带输出
频带输出（`output: 'bands'`）为兼容保留，数值不变。它把频点映射到对数等分的频带，比一个频点还窄的频带用相邻频点插值，32 带、64 带以上把 `fftSize` 至少提升到 4096、8192；`weighted` 另加倾斜补偿、200 Hz 以下的低频衰减与显示曲线。新代码请用频点输出。
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

// 左右两路的线性频点，单位 dB
await fb2k.invoke('audio.subscribeSpectrum', {
    subscriptionId: 'spectrum_bins',
    output: 'bins',
    channels: 'stereo',
    fftSize: 16384,
    fps: 60
});
```

### audio.unsubscribeSpectrum

<!-- api-schema:begin audio.unsubscribeSpectrum -->
移除本页面的频谱订阅。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | 否 | 要移除的订阅；缺省移除本页面的全部频谱订阅。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `removed` | `integer` | 移除的订阅数：带 `subscriptionId` 时为 0 或 1，不带时为本页面的全部。别的页面的订阅不计也不动。 |
| `subscriptionId` | `string` | 请求里的 id；没给时为空串。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

<!-- api-schema:begin audio.getSpectrum -->
按需计算一帧频谱。至少要有一个频谱订阅。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | 否 | 用这个订阅的参数出帧，含频点输出；此时其余参数不起作用。未知 id 返回 `NOT_FOUND`。不能为空。 |
| `bands` | `integer` | 否 | 不带 `subscriptionId` 时用。`0` 取所有订阅里最大的频带数；不超过 `fftSize / 2`。不小于 `0`。默认 `0`。 |
| `scale` | `"weighted" \| "db"` | 否 | 不带 `subscriptionId` 时用，同 subscribeSpectrum。默认 `"weighted"`。 |
| `minFrequency` | `number` | 否 | 不带 `subscriptionId` 时用，同 subscribeSpectrum。不小于 `1`。默认 `20`。 |
| `maxFrequency` | `number` | 否 | 不带 `subscriptionId` 时用，同 subscribeSpectrum。 |
| `output` | `"bands" \| "bins"` | 否 | 不带 `subscriptionId` 时只接受 `bands`：这时 FFT 点数跟随其他订阅，频点输出要有自己的订阅。默认 `"bands"`。 |
| `channels` | `"mix" \| "stereo"` | 否 | 不带 `subscriptionId` 时只接受 `mix`。默认 `"mix"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `subscriptionId` | `string` | 帧所属的订阅；请求没指定订阅时不带此键。 |
| `output` | `"bands" \| "bins"` | 帧里给的是什么。 |
| `spectrum` | `number[]` | 频带帧：每个频带一个值，刻度为帧的 `scale`；`weighted` 在 `[0, 1]` 内，`db` 下限 `-160`。`channels: 'mix'` 的频点帧：每个频点一个功率值，dB。 |
| `left` | `number[]` | `channels: 'stereo'` 的频点帧：第一个声道，每个频点一个 dB 值。 |
| `right` | `number[]` | `channels: 'stereo'` 的频点帧：第二个声道，单声道流时重复第一个。 |
| `bands` | `integer` | 频带帧：`spectrum` 的长度。 |
| `firstBin` | `integer` | 频点帧：第一个值的 FFT 频点序号；第 `i` 个值的中心频率为 `(firstBin + i) * sampleRate / fftSize` Hz。范围内没有频点时为 `0`，数组为空。 |
| `fftSize` | `integer` | 实际用的 FFT 点数。 |
| `scale` | `"weighted" \| "db"` | 值的刻度；频点帧恒为 `db`。 |
| `channels` | `"mix" \| "stereo"` | 频点帧：订阅的声道布局。 |
| `channelCount` | `integer` | 频点帧：可视化流的声道数；未知时为 `0`。 |
| `sampleRate` | `integer` | 可视化流的采样率，Hz；未知时为 `0`。 |
| `minFrequency` | `number` | 频率范围下限，Hz。 |
| `maxFrequency` | `number` | 上限，Hz：请求的上限截到 `sampleRate / 2`，没给时就是 `sampleRate / 2`。 |
| `state` | `"playing" \| "paused" \| "stopped"` | 暂停或停止时的帧是静音帧。 |
| `streamTime` | `number` | 可视化流上次起播以来的秒数；帧由截至此刻的 `fftSize` 个样本算出。 |
| `hostTime` | `number` | 宿主算出这一帧时的系统时间，Unix 纪元毫秒。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

应答是一帧，字段同 [`audio.subscribeSpectrum`](#audio-subscribespectrum) 的帧。暂停或停止时回一帧静音帧，`state` 标明是哪种。没有任何订阅，或新建的流还没出数据（首个订阅后约 0.7 秒内），回 `OPERATION_FAILED`。

::: tip 拉取还是推送
两种方式拿到的是同一种帧。画面要跟显示器逐帧对齐、或每秒要 60 帧以上时用拉取：订阅时 `fps` 给 1 保活，在 `requestAnimationFrame` 里带 `subscriptionId` 调 `audio.getSpectrum`，上一次返回之后才发下一次，`streamTime` 没变的帧跳过。每次调用都在宿主主线程上算一次 FFT，开销与推送一帧相当，所以按需要的帧率限频，不要跟着显示器刷新率走。每秒 60 帧以内时，用推送同样可以。
:::

**示例**:

```javascript
// 在 requestAnimationFrame 里拉取：同时只发一个请求，每秒至多约 60 次
const res = await fb2k.invoke('audio.subscribeSpectrum', { fps: 1 });
if (res.success === false) throw new Error(res.error);
const { subscriptionId } = res;
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

<!-- api-schema:begin audio.getWaveform -->
返回可视化流最近 `duration` 秒的波形，终点是它的当前时刻（即频谱帧的 `streamTime`）。需要有频谱订阅。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `duration` | `number` | 否 | 窗口长度，秒，须大于 0。不大于 `1`。默认 `0.05`。 |
| `signed` | `boolean` | 否 | 为 `true` 时保留 PCM 正负，夹到 `[-1, 1]`；为 `false` 时把 -70 到 0 dB 的幅度映射到 `[0, 1]`。默认 `false`。 |
| `channels` | `"mix" \| "stereo"` | 否 | `mix` 把各声道平均成 `waveform`；`stereo` 把前两个声道分别给成 `left` 与 `right`。默认 `"mix"`。 |
| `points` | `integer` | 否 | 把窗口抽成这么多个等距样本，不做平均；缺省返回全部样本。取值 `2` 到 `65536`（含端点）。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `waveform` | `number[]` | `mix` 应答：每个样本一个值。 |
| `left` | `number[]` | `stereo` 应答：第一个声道。 |
| `right` | `number[]` | `stereo` 应答：第二个声道；流只有一个声道时与 `left` 相同。 |
| `duration` | `number` | 窗口长度，秒，同请求。 |
| `signed` | `boolean` | 同请求。 |
| `channels` | `"mix" \| "stereo"` | 同请求。 |
| `sampleRate` | `integer` | 可视化流的采样率，Hz。 |
| `channelCount` | `integer` | 可视化流的声道数；setChannelMode 设为 `mono` 后为 `1`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning 注意
此 API 用于获取**当前播放流**的实时波形片段，不是离线文件波形。如需生成完整文件波形，请使用 `audio.generateFullWaveform`。
:::

- 只有一路时 `right` 等于 `left`；多于两路时只取前两路，其余声道不混进来。`audio.setChannelMode` 设成 `'mono'` 之后可视化流只有一路。
- 起播、seek、手动换曲之后的头 `duration` 秒里，窗口中早于重新计时的部分是零。
- 没有频谱订阅、或可视化流给不出这段窗口时回 `OPERATION_FAILED`。

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
const res = await fb2k.invoke('audio.getWaveform', {
    duration: 0.05,
    signed: true,
    channels: 'stereo',
    points: 256,
});
if (res.success === false) throw new Error(res.error);
const { left, right } = res;
```

### audio.setChannelMode

<!-- api-schema:begin audio.setChannelMode -->
选择可视化流带哪些声道。作用于频谱帧与 getWaveform，不影响播放出来的声音。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `mode` | `"default" \| "mono" \| "front" \| "back"` | 否 | 可视化流带的声道。默认 `"default"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `mode` | `"default" \| "mono" \| "front" \| "back"` | 现在生效的模式。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

**示例**:

```javascript
// 设置为单声道模式
await fb2k.invoke('audio.setChannelMode', { mode: 'mono' });

// 设置为前置声道
await fb2k.invoke('audio.setChannelMode', { mode: 'front' });

// 恢复全部声道
await fb2k.invoke('audio.setChannelMode', { mode: 'default' });
```

### audio.analyzeBPM

<!-- api-schema:begin audio.analyzeBPM -->
读曲目的 `BPM` 标签，宿主不做节拍检测。foobar2000 缓存的信息齐全时用缓存，否则读文件本身，所以不在任何播放列表或媒体库里的曲目也行。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；带 `\|subsong:N` 后缀时选子曲目。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `bpm` | `number` | `BPM` 标签的值，在 0 与 500 之间（不含两端）；超出这个范围返回 `NOT_FOUND`。 |
| `confidence` | `number` | 恒为 `1`。 |
| `source` | `"metadata"` | 恒为 `metadata`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

曲目没有 `BPM` 标签或标签值超出范围时回 `NOT_FOUND`；打不开文件或读不到文件信息（例如文件不存在）时回 `INVALID_HANDLE` 或 `NO_INFO`；其他错误回 `OPERATION_FAILED`。

**示例**:

```javascript
const result = await fb2k.invoke('audio.analyzeBPM', {
    path: 'E:\\Music\\song.flac'
});
if (result.success === true) {
    console.log(`BPM: ${result.bpm}`);
} else if (result.code === 'NOT_FOUND') {
    console.log('没有 BPM 标签');
}
```

### audio.generateFullWaveform

<!-- api-schema:begin audio.generateFullWaveform -->
计算整首曲目的波形。命中缓存时直接回 `ready` 与波形；否则回 `pending` 与任务 id，结果随 `audio:fullWaveformReady` 或 `audio:fullWaveformFailed` 到达。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；带 `\|subsong:N` 后缀时选子曲目。不能为空。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，从 0 起；优先于路径里的子曲目。不小于 `0`。 |
| `resolution` | `integer` | 否 | 点数，夹到 64 到 4096。每个点数只解码一次，其余选项从缓存算。默认 `256`。 |
| `method` | `"rms" \| "peak"` | 否 | 每个点的取值方式。默认 `"rms"`。 |
| `scale` | `"linear" \| "db"` | 否 | 点的刻度：`linear`，或 `db`（比曲目自身最大值低 60 dB 处映射为 0）；`signed` 时忽略。默认 `"linear"`。 |
| `signed` | `boolean` | 否 | 保留 PCM 正负；点在 `[-1, 1]` 内。默认 `false`。 |
| `preferCache` | `boolean` | 否 | 能用缓存时直接用缓存回答。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `status` | `"ready" \| "pending"` | `ready`：波形在本应答里。`pending`：它随 `taskId` 的事件到达。 |
| `cached` | `boolean` | 应答来自缓存。 |
| `taskId` | `string` | 在事件与 cancelFullWaveform 里标识任务；只有 `pending` 应答带。 |
| `waveform` | `number[]` | `ready` 应答：各点，按 `maxAmplitude` 归一化。 |
| `maxAmplitude` | `number` | `ready` 应答：归一化前所选序列的最大值，线性满幅单位。`linear` 刻度下 `waveform[i] * maxAmplitude` 还原电平；`db` 刻度下 dBFS 为 `(v * 60 - 60) + 20 * log10(maxAmplitude)`。 |
| `duration` | `number` | `ready` 应答：曲目长度，秒。 |
| `sampleRate` | `integer` | `ready` 应答：采样率，Hz。 |
| `channels` | `integer` | `ready` 应答：声道数。 |
| `resolution` | `integer` | 使用的点数。 |
| `method` | `"rms" \| "peak"` | 同请求。 |
| `scale` | `"linear" \| "db"` | 同请求。 |
| `signed` | `boolean` | 同请求。 |
| `path` | `string` | 请求里的路径。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

请求时能判定的错误在应答里回：`INVALID_PARAMS`、`INVALID_PATH`（找不到或读不了文件）与 `OPERATION_FAILED`（foobar2000 正在退出，或其他错误）；之后解码失败的以 `audio:fullWaveformFailed` 结束。

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
if (result.success === false) throw new Error(result.error);

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

<!-- api-schema:begin audio.cancelFullWaveform -->
取消本页面发起、尚未结束的 generateFullWaveform 请求。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `taskId` | `string` | 是 | generateFullWaveform 的 `pending` 应答里的任务 id。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `cancelled` | `boolean` | 为 `false` 表示任务已结束、不存在或属于别的页面，三者不区分。被取消的任务收到一次 `code: 'CANCELLED'` 的 `audio:fullWaveformFailed`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

- 取消成功时该 `taskId` 恰好收到一次 `audio:fullWaveformFailed`（`code: "CANCELLED"`），此后不再有它的事件；这条事件可能先于本应答到达
- 同一曲目还有别的请求在等时解码照常进行；没人等了，排队中的移出队列，解码中的被中止

```javascript
const pending = await fb2k.invoke('audio.generateFullWaveform', { path: 'E:\\Music\\song.flac' });
if (pending.success === false) throw new Error(pending.error);
if (pending.status === 'pending') {
    await fb2k.invoke('audio.cancelFullWaveform', { taskId: pending.taskId });
}
```

### audio.decodePcm

<!-- api-schema:begin audio.decodePcm -->
实验性 API，后续版本可能发生变化。

把曲目或其中一段解成 float32 平面 PCM，经共享缓冲交给页面。先回任务 id；样本随 `chrome.webview` 的 `sharedbufferreceived` 事件与 `audio:pcmReady` 一起到达，失败时收到 `audio:pcmFailed`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要解码的曲目路径；带 `\|subsong:N` 后缀时选子曲目。不能为空。 |
| `cueIndex` | `integer` | 否 | cue 子曲目序号，从 0 起；优先于路径里的子曲目。不小于 `0`。 |
| `start` | `number` | 否 | 起点，秒。不小于 `0`。 |
| `end` | `number` | 否 | 终点，秒；缺省到曲目末尾，超过曲目长度时按末尾截。必须大于 start。不小于 `0`。 |
| `sampleRate` | `integer` | 否 | 用 foobar2000 的重采样器转换到的采样率；缺省用源采样率。取值 `8000` 到 `192000`（含端点）。 |
| `mono` | `boolean` | 否 | 各声道等权平均成单声道。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `taskId` | `string` | 在事件、共享缓冲附带数据与 cancelDecodePcm 里标识这个任务，形如 `pcm_N`。 |
| `status` | `"pending"` | 恒为 `pending`：解码结果不缓存。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

样本不走 JSON 通道。解码完成后，页面针对这个 `taskId` 收到两条消息，先后不定：

- `window.chrome.webview` 上的 `sharedbufferreceived` 事件：`e.getBuffer()` 是样本，`e.additionalData` 为 `{ purpose: 'audio.decodePcm', taskId, sampleRate, channels, frames, headerBytes: 64 }`；
- `audio:pcmReady` 事件：`{ taskId, path, sampleRate, channels, frames, start, end, duration, truncated, resampled }`，其中 `start`、`end` 是实际解出的区间，单位秒。

任务失败时改为收到一次 `audio:pcmFailed`（`{ taskId, path, code, error }`）。SDK 的 `fb.audio.decodePcm` 会替你配对两条消息并返回 `PcmBuffer`；用了它的页面不要再自己处理这些缓冲。

缓冲开头是 64 字节的小端头部，其后是 float32 样本，每个声道占连续的 `capacityFrames` 个：声道 `c` 的第 `i` 个样本在字节 `64 + (c × capacityFrames + i) × 4`。`capacityFrames` 是头部字节 24 处的 u32；每段只有前 `frames` 个样本是音频（`frames` 也是头部字节 32 处的 u32）。

- 缓冲是**只读共享内存**。写入会让页面的渲染进程崩溃，整个 foobar2000 窗口随之失效。要改样本，先拷出来，例如拷进 `AudioBuffer`。
- 同一个 `sharedbufferreceived` 事件的所有监听者拿到的是同一个 `ArrayBuffer`。最后一处用完后调用一次 `chrome.webview.releaseBuffer(buffer)`，之后再访问会抛 `TypeError`。
- 单块缓冲上限：64 位 foobar2000 为 256 MiB，32 位为 64 MiB。宿主在解码前按曲目长度加一秒余量估算大小，超出上限时回 `INVALID_PARAMS`，并在 `details.estimatedBytes`、`details.limitBytes` 给出估算值与上限。五分钟 44.1 kHz 立体声约 106 MB；降低 `sampleRate`、设 `mono` 或分段解码可以压到上限以内。
- `truncated` 为 `true` 表示音频超出了上述估算（文件比它报出的时长长），或中途换了格式（如链式 Ogg）；缓冲里是截断处之前的部分。
- 同一时间只解一个任务，其余按提交顺序排队。相同的请求（同一文件、区间、`sampleRate` 与 `mono`）在已有任务解码期间到达时共用那次解码，各自仍有自己的 `taskId`、缓冲与事件。
- 请求时就能判定的错误在应答里返回：`INVALID_PARAMS`、`INVALID_PATH`、`INVALID_HANDLE`、`NO_INFO`、`NOT_SUPPORTED`（WebView2 运行时不支持共享缓冲，或找不到发起调用的页面）、`OPERATION_FAILED`（foobar2000 正在退出）。之后的错误经 `audio:pcmFailed` 到达：`CANCELLED`、`INVALID_PARAMS`（`start` 不小于曲目长度）、`NOT_SUPPORTED`（没有重采样器支持这次转换）、`DECODER_FAILED`、`DECODE_FAILED`、`ORIGIN_DENIED`（页面来源不再可信）、`OPERATION_FAILED`（缓冲分配或投递失败）与 `UNKNOWN_ERROR`。
- 结果就绪前页面已导航走、所在窗口或面板已关闭，或 foobar2000 退出时，不发任何消息。

```js
const pending = await fb2k.invoke('audio.decodePcm', {
    path: 'C:\\Music\\song.flac',
    start: 30,
    end: 60,
    sampleRate: 22050,
});
if (pending.success === false) throw new Error(pending.error);
window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
    const info = e.additionalData;
    if (info?.purpose !== 'audio.decodePcm' || info.taskId !== pending.taskId) return;
    const buffer = e.getBuffer();
    const capacityFrames = new DataView(buffer).getUint32(24, true);
    const channel = (c) => new Float32Array(buffer, 64 + c * capacityFrames * 4, info.frames);
    console.log(info.sampleRate, channel(0).length);
    window.chrome.webview.releaseBuffer(buffer);
});
```

### audio.cancelDecodePcm

<!-- api-schema:begin audio.cancelDecodePcm -->
实验性 API，后续版本可能发生变化。

取消本页面发起、尚未结束的 decodePcm 任务。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `taskId` | `string` | 是 | decodePcm 应答里的任务 id。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `cancelled` | `boolean` | 为 `false` 表示任务已结束、不存在或属于别的页面，三者不区分。被取消的任务收到一次 `code: 'CANCELLED'` 的 `audio:pcmFailed`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

取消成功时该 `taskId` 恰好收到一次 `audio:pcmFailed`（`code: "CANCELLED"`），此后不再有它的消息；这条事件可能先于本应答到达。还有相同的请求在等时解码照常进行；没人等了，排队中的任务移出队列，解码中的被中止。

```js
const pending = await fb2k.invoke('audio.decodePcm', { path: 'C:\\Music\\song.flac' });
if (pending.success === false) throw new Error(pending.error);
await fb2k.invoke('audio.cancelDecodePcm', { taskId: pending.taskId });
```

### audio.getOutputInfo

<!-- api-schema:begin audio.getOutputInfo -->
报告 foobar2000 的音量。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `volume` | `number` | 音量，dB，`-100`（静音）到 `0`（最大）。 |
| `volumePercent` | `number` | 同一音量的线性百分比，`100 * 10^(volume / 20)`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

**示例**:

```javascript
const info = await fb2k.invoke('audio.getOutputInfo');
if (info.success === false) throw new Error(info.error);
console.log(`音量: ${info.volume} dB (${info.volumePercent}%)`);
```

### audio.getStreamInfo

<!-- api-schema:begin audio.getStreamInfo -->
报告正在播放的曲目的格式。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playing` | `boolean` | 有曲目在播放或暂停；只有这时才带其余字段。 |
| `sampleRate` | `integer` | 采样率，Hz。 |
| `channels` | `integer` | 声道数。 |
| `bitrate` | `integer` | 码率，kbps。 |
| `codec` | `string` | foobar2000 报的编码名，没有时为 `unknown`。 |
| `duration` | `number` | 曲目长度，秒。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const info = await fb2k.invoke('audio.getStreamInfo');
if (info.success === false) throw new Error(info.error);
if (info.playing) {
    console.log(`${info.codec} ${info.sampleRate}Hz ${info.channels}ch`);
}
```

### audio.isVisualizationAvailable

<!-- api-schema:begin audio.isVisualizationAvailable -->
报告 foobar2000 是否提供可视化流。频谱帧与 getWaveform 都要靠它。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `available` | `boolean` | foobar2000 提供可视化流。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

**示例**:

```javascript
const result = await fb2k.invoke('audio.isVisualizationAvailable');
if (result.success === false) throw new Error(result.error);
if (result.available) {
    console.log('可视化功能可用');
    // 可以安全地调用 subscribeSpectrum
    await fb2k.invoke('audio.subscribeSpectrum');
}
```

### audio.subscribeStream

<!-- api-schema:begin audio.subscribeStream -->
实验性 API，后续版本可能发生变化。

订阅 foobar2000 正在播放的音频：核心播出的每一块都写进与页面共享的环形缓冲，一个订阅一块。缓冲随第一块音频以 `chrome.webview` 的 `sharedbufferreceived` 事件到达；采样率或声道数变化时再投一块新的（`epoch` 加一），并以 `audio:stream` 通知旧的一代结束。停止或暂停期间没有任何投递。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | 否 | 调用方给的订阅 id；缺省时宿主生成（`pcmstream_N`）。同一页面用同一 id 再订阅会替换先前的订阅。不能为空。 |
| `interval` | `number` | 否 | 向 foobar2000 请求的回调间隔，秒。核心按自己约 16 ms 的节拍取整，最长 200 ms；一个订阅要短间隔，全组件的流订阅都会跟着变短。缺省用核心的 200 ms。取值 `0.01` 到 `0.2`（含端点）。 |
| `bufferSeconds` | `number` | 否 | 环形缓冲长度，秒。整块缓冲超过 64 MiB 时按 64 MiB 折算。取值 `0.1` 到 `10`（含端点）。默认 `1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `subscriptionId` | `string` | 订阅 id，调用方给的或宿主生成的。 |
| `interval` | `number` | 实际请求核心的间隔；交给核心缺省时不带此键。 |
| `bufferSeconds` | `number` | 登记的环形缓冲长度，秒。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

样本不走 JSON 通道。播放一开始，页面就会在 `window.chrome.webview` 上收到 `sharedbufferreceived` 事件：`e.getBuffer()` 是环形缓冲，`e.additionalData` 是 `{ purpose: 'audio.subscribeStream', subscriptionId, epoch, sampleRate, channels, capacityFrames, headerBytes: 64 }`。采样率或声道数变化时（换到别的格式的曲目，或加了改声道数的 DSP）同一事件再来一次、`epoch` 加一；随后一条 `audio:stream` 事件 `{ subscriptionId, type: 'ended', epoch, reason: 'format-change' }` 指出结束的那一代，它的缓冲头也置了 `ended` 位。

缓冲开头是与 `audio.decodePcm` 相同的 64 字节头，其后是**交错**的 float32 帧：槽位 `s` 的帧、声道 `c` 在字节 `64 + (s × channels + c) × 4`。头部里 `capacityFrames`（字节 24 的 u32）是槽位数，`writeFrames`（字节 32）是累计写入帧数、2^32 回绕，`writeSlot`（字节 60）是下一帧要写的槽位，`seq`（字节 28）在宿主写入期间为奇数，`flags`（字节 36）的 bit 0 在宿主停止写入后置位。读法：先读 `seq`，是奇数就稍后再来；拷出要读的帧，最新一帧在 `writeSlot` 之前；再读一次 `seq`，变了就重来。落后超过 `capacityFrames` 帧就丢了最旧的帧：`dropped = writeFrames − readFrames − capacityFrames`（取正）。SDK 的 `fb.audio.subscribeStream` 把这些都做了，返回按声道分开的 `Float32Array`；用它的页面不要再自己处理这些缓冲。

- 样本取自 DSP 链之后、ReplayGain 与音量之前：是 DSP 链的输出、满幅电平。与 `audio:spectrum` 不同，不含 ReplayGain。
- 缓冲是**只读共享内存**，写入会让页面的渲染进程崩溃，整个 foobar2000 窗口随之一起。用完一代就调 `chrome.webview.releaseBuffer(buffer)`：`audio:stream` 报告结束的那一代，以及退订后的最后一代。
- 块缺省约每 200 ms 到一次，最短约 16 ms（`interval`）。起播或续播后的首块最多带 0.8 s 音频，`bufferSeconds` 小于它就会在每次起播时丢帧。
- 停止或暂停期间什么都不来，`writeFrames` 不再增长。同一格式下 seek 与换曲不会重置缓冲。
- 每个页面最多 8 个流订阅，第 9 个回 `OPERATION_FAILED`。用同一 `subscriptionId` 再订阅会替换先前的订阅：旧缓冲置 `ended` 位、不发事件，下一代接着数。
- 请求时能判定的错误在应答里回：越界或未知键回 `INVALID_PARAMS`，WebView2 运行时不支持共享缓冲或定位不到调用方页面回 `NOT_SUPPORTED`，foobar2000 正在退出或本页面已有 8 个订阅回 `OPERATION_FAILED`。
- 页面导航、窗口或面板关闭、foobar2000 退出时订阅直接结束，不发事件。

```js
const sub = await fb2k.invoke('audio.subscribeStream', { subscriptionId: 'meter', bufferSeconds: 0.5 });
if (sub.success === false) throw new Error(sub.error);
window.chrome.webview.addEventListener('sharedbufferreceived', (e) => {
    const info = e.additionalData;
    if (info?.purpose !== 'audio.subscribeStream' || info.subscriptionId !== sub.subscriptionId) return;
    const buffer = e.getBuffer();
    const header = new DataView(buffer);
    console.log(info.epoch, header.getUint32(16, true), header.getUint32(20, true)); // epoch、sampleRate、channels
});
```

### audio.unsubscribeStream

<!-- api-schema:begin audio.unsubscribeStream -->
实验性 API，后续版本可能发生变化。

移除本页面的流订阅。它们的缓冲置 `ended` 位并在宿主侧关闭；不发事件。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `subscriptionId` | `string` | 否 | 要移除的订阅；缺省移除本页面的全部流订阅。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `removed` | `integer` | 移除的订阅数：带 `subscriptionId` 时为 0 或 1，不带时为本页面的全部。别的页面的订阅不计也不动。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

被移除订阅的缓冲先置 `ended` 位（字节 36 的 u32 的 bit 0），宿主再关掉自己这一侧；页面的视图在自己释放之前仍可读。移除组件里最后一个流订阅时，同时向 foobar2000 注销捕获回调。

```js
await fb2k.invoke('audio.unsubscribeStream', { subscriptionId: 'meter' });
```

### audio.getSpectrumDebugState

<!-- api-schema:begin audio.getSpectrumDebugState -->
报告宿主眼中的频谱运行状态，供测试与排查用。形状不保证稳定。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `active` | `boolean` | 至少登记了一个频谱订阅。 |
| `timerRunning` | `boolean` | 推送帧的计时线程在运行。 |
| `timerHwnd` | `integer` | 已废弃，恒为 `0`。 |
| `beatSource` | `"high-resolution" \| "standard" \| null` | 计时线程用的定时器；线程没在运行时为 `null`。 |
| `beatIntervalMs` | `number \| null` | 当前拍长，毫秒，即 1000 / `effectiveFps`；计时线程没在运行时为 `null`。 |
| `beatsCoalesced` | `integer` | 因上一拍还没到主线程而丢掉的拍数，累计值。 |
| `effectiveFftSize` | `integer` | 所有订阅请求的最大 FFT 点数；各订阅仍按自己的算。 |
| `effectiveFps` | `integer` | 所有订阅请求的最大帧率。 |
| `effectiveBands` | `integer` | 所有订阅请求的最大频带数。 |
| `skipFrames` | `integer` | 因上一拍超时而要跳过的推送拍数。 |
| `framesComputed` | `integer` | 宿主启动以来算过的 FFT 次数；暂停或停止时不增长。 |
| `streamReady` | `boolean` | 可视化流已建好。 |
| `subscriptionCount` | `integer` | `subscriptions` 的长度。 |
| `dispatchTargetCount` | `integer` | `dispatchTargets` 的长度。 |
| `subscriptions` | `SpectrumDebugSubscription[]` | 所有页面的全部频谱订阅。 |
| `subscriptions[].token` | `string` | 订阅的键。 |
| `subscriptions[].windowId` | `string` | 订阅页面的窗口 id。 |
| `subscriptions[].ownerHwnd` | `integer` | 拥有该订阅的窗口句柄，数值。 |
| `subscriptions[].event` | `string` | 帧的事件名。 |
| `subscriptions[].fftSize` | `integer` | 请求的 FFT 点数。 |
| `subscriptions[].fps` | `integer` | 夹取后的帧率。 |
| `subscriptions[].bands` | `integer` | 夹取后的频带数。 |
| `subscriptions[].scale` | `"weighted" \| "db"` | 使用的刻度。 |
| `subscriptions[].backgroundThrottle` | `boolean` | 同请求。 |
| `subscriptions[].minFrequency` | `number` | 范围下限，Hz。 |
| `subscriptions[].maxFrequency` | `number \| null` | 请求的上限，Hz；跟随流采样率一半时为 `null`。 |
| `subscriptions[].output` | `"bands" \| "bins"` | 使用的输出。 |
| `subscriptions[].channels` | `"mix" \| "stereo"` | 使用的声道布局。 |
| `dispatchTargets` | `SpectrumDebugTarget[]` | 推送帧的投递目标窗口。 |
| `dispatchTargets[].windowId` | `string` | 帧投递到的窗口 id。 |
| `dispatchTargets[].ownerHwnd` | `integer` | 该窗口的句柄，数值。 |
| `dispatchTargets[].event` | `string` | 帧的事件名。 |
| `instanceCount` | `integer` | 组件里的 WebView2 实例数。 |
| `callerHwnd` | `integer` | 调用方页面的窗口句柄，数值；未知时为 `0`。 |
| `callerWindowId` | `string` | 调用方页面的窗口 id；未知时为空串。 |
| `callerOwnsSubscription` | `boolean` | 调用方页面至少拥有一个订阅。 |
| `foregroundHwnd` | `integer` | 前台窗口的句柄，数值。 |
| `foregroundPid` | `integer` | 前台窗口所属进程的 id。 |
| `foregroundIsExternal` | `boolean` | 前台窗口属于别的进程，后台限帧此时生效。 |
| `foregroundTitle` | `string` | 前台窗口的标题。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const debug = await fb2k.invoke('audio.getSpectrumDebugState');
if (debug.success === false) throw new Error(debug.error);
console.log(debug.subscriptions);
```

### audio.getPcmDebugState

<!-- api-schema:begin audio.getPcmDebugState -->
实验性 API，后续版本可能发生变化。

报告调用方页面的共享缓冲支持情况、解码任务队列与流订阅，供测试与排查用。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `runtime` | `PcmRuntimeState` | 调用方页面的共享缓冲支持情况；找不到页面时两个标志都为 `false`。 |
| `runtime.version` | `string` | 本机安装的 WebView2 运行时版本，如 `153.0.4234.48`；读不到时为空串。 |
| `runtime.environment12` | `boolean` | 页面所在环境能建共享缓冲（`ICoreWebView2Environment12`）。 |
| `runtime.webview17` | `boolean` | 页面的 webview 能接收共享缓冲（`ICoreWebView2_17`）。 |
| `decode` | `PcmDecodeState` | decodePcm 任务队列。 |
| `decode.active` | `integer` | 解码中的任务数，含已发中止、worker 还没返回的。 |
| `decode.queued` | `integer` | 排队等空位的任务数。 |
| `decode.openBufferBytes` | `integer` | 宿主侧还没关闭的解码缓冲字节数合计。 |
| `stream` | `PcmStreamState` | 流捕获回调与所有页面的全部流订阅。 |
| `stream.callbackRegistered` | `boolean` | 组件在核心登记了捕获回调；没有订阅时为 `false`。 |
| `stream.interval` | `number` | 当前向核心请求的间隔，秒；交给核心缺省时不带此键。 |
| `stream.chunkCount` | `integer` | 组件加载以来回调收到的块数。 |
| `stream.chunkCycles` | `integer` | 回调累计消耗的 CPU 周期，按 `QueryThreadCycleTime` 计。 |
| `stream.subscriptions` | `PcmStreamDebugEntry[]` | 全部在途的流订阅。 |
| `stream.subscriptions[].subscriptionId` | `string` | 订阅 id。 |
| `stream.subscriptions[].windowId` | `string` | 订阅页面的窗口 id。 |
| `stream.subscriptions[].epoch` | `integer` | 缓冲代数，首块音频到达前为 `0`。 |
| `stream.subscriptions[].capacityFrames` | `integer` | 当前环的容量（帧）；首块前为 `0`。 |
| `stream.subscriptions[].writeFrames` | `integer` | 当前环累计写入的帧数，2^32 回绕。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

已完成的任务交付完毕、宿主侧关闭缓冲后，`decode.openBufferBytes` 回到 `0`；页面手里的视图在页面释放前一直有效。

```js
const res = await fb2k.invoke('audio.getPcmDebugState');
if (res.success === false) throw new Error(res.error);
const { runtime, decode } = res;
```

## 运行时行为说明

- `audio.subscribeSpectrum` 为调用方窗口创建订阅，`subscriptionId` 相同时替换原有的那个。不给 `subscriptionId` 时，键由调用方窗口与 `event` 推出；帧以 `event` 为事件名送达，默认 `audio:spectrum`。SDK 的 `fb.audio.subscribeSpectrum()` 建立的是同一种订阅。
- 频谱订阅之间不共享参数。宿主的计时线程用高精度定时器，按请求中最高的 `fps` 走拍，每个订阅到了自己的间隔才出帧，所以推送的实际帧率接近请求值；实际 FFT 大小、频带数、`scale` 与频率范围都相同的订阅每拍共用一次 FFT。宿主主线程的开销随帧数增加，每帧约 0.5 ms。
- `db` 档反映 ReplayGain 与 DSP 处理之后、音量控制之前的信号，调节音量不改变读数。
- `audio.getSpectrum` 与 `audio.getWaveform` 读取可视化流。没有任何频谱订阅时两者都以 `OPERATION_FAILED` 失败；`audio.getSpectrum` 在播放中但流还没有数据时同样失败，暂停或停止时回一帧静音帧。
- `audio.generateFullWaveform` 命中缓存时返回带数据的 `status: "ready"`，否则返回带 `taskId` 的 `status: "pending"`。调用方会收到 `audio:fullWaveformReady` 或 `audio:fullWaveformFailed`；`cueIndex` 优先于 `path|subsong:N` 后缀。
