# F. 歌词与可视化

## `<fb-lyrics-panel>` {#fb-lyrics-panel}

歌词面板组件，支持同步滚动歌词（LRC）与纯文本歌词。同步歌词高亮当前行，点击对应时间戳行可直接跳转定位。

```html
<fb-lyrics-panel></fb-lyrics-panel>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `source` | string | `'any'` | 传递给 SDK 封装层的歌词来源类型 |
| `scroll` | string | `'smooth'` | 滚动模式：仅取值 `smooth` 时启用平滑滚动，其余情况为自动即时滚动 |
| `has-lyrics` | boolean | — | 只读反映当前曲目是否成功渲染出歌词 |
| `current-line` | string | — | 只读反映当前高亮行索引（从 0 起始，无高亮时为 -1） |

**CSS Parts:** `container`, `line`

每个 `line` 对应的 CSS class：`past`（已播放行）、`current`（当前行）、`future`（未播放行）。

**事件：**

```js
el.addEventListener('fb-lyrics-loaded', e => {
  console.log('歌词加载:', e.detail.lineCount, '行, 同步:', e.detail.synced);
});
el.addEventListener('fb-lyrics-seek', e => {
  console.log('点击歌词跳转:', e.detail.time);
});
el.addEventListener('fb-lyrics-line-change', e => {
  console.log('当前行:', e.detail.index, e.detail.text, e.detail.time);
});
```

## `<fb-spectrum-visualizer>` {#fb-spectrum-visualizer}

实时频谱可视化组件，支持屏幕像素比（DPR）自适应与 ResizeObserver 尺寸监听。`mode="raw"` 时派发频带数值事件而非在 Canvas 上绘制。各实例维护独立的宿主频带订阅并仅绘制对应帧，多个不同 `bands` 频带数的元素可共存于同一页面。

```html
<fb-spectrum-visualizer bands="64" fps="30" mode="bars"></fb-spectrum-visualizer>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `bands` | string | `'64'` | 目标频带数 |
| `fps` | string | `'30'` | 宿主频谱数据流刷新帧率 |
| `mode` | `'raw' \| 'bars' \| 'wave'` | `'bars'` | 呈现渲染模式 |
| `fft-size` | string | 自动 | FFT 计算窗口大小；低于 256 向上补齐，向上取整为 2 的幂，超过 65536 自动截断 |
| `dpr` | string | `devicePixelRatio` | 像素比覆盖（降低数值如 `dpr="1"` 可大幅降低 Canvas 渲染开销） |
| `fall-speed` | string | `'0.28'` | 柱状图回落插值系数（0 到 1 之间，数值越大回落越快，observed） |
| `rise-speed` | string | `'0.65'` | 柱状图上升插值系数（0 到 1 之间，数值越大上升越快，observed） |

**CSS Parts:** `canvas`

**JS API：**

```js
const ctx = el.getContext(); // 获取 CanvasRenderingContext2D
```

**事件：**

```js
el.addEventListener('fb-spectrum-data', e => {
  // 仅 mode='raw' 时触发
  console.log('频谱数据:', e.detail.bands); // Float32Array
});
```

## `<fb-waveform>` {#fb-waveform}

整首曲目音频波形概览组件。点击波形可跳转定位，游标随当前播放进度实时更新。

播放曲目切换或元素被移除时，若波形仍在解码中，组件会自动取消宿主请求，避免无效请求长期占用宿主仅有的两个解码并发槽位之一。页面级监听 `audio:fullWaveformFailed` 事件可捕获 `code: "CANCELLED"` 结果。

```html
<fb-waveform resolution="200"></fb-waveform>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `src` | string | 当前曲目 | 显式指定的音频文件路径；指定时不再自动监听曲目切换与播放进度更新 |
| `resolution` | string | `'200'` | 目标波形采样点数量 |
| `mode` | `'peak' \| 'rms'` | `'rms'` | 宿主端波形采样算法 |
| `cue-index` | string | — | 可选的 CUE 单轨子曲目索引 |
| `normalize` | `'adaptive' \| 'gamma' \| 'histogram'` | `'adaptive'` | 视觉归一化算法模式（observed） |
| `gamma` | string | `'1.5'` | 响度伽马曲线指数，取值范围 (0, 2]（observed，仅在 `normalize="gamma"` 时生效） |
| `status` | `'pending' \| 'ready' \| 'failed'` | — | 只读反映当前波形生成状态 |
| `progress` | string | — | 只读反映游标进度比例（0 到 1） |
| `error` | string | — | 状态为 `failed` 时的只读失败错误码 |

**CSS Parts:** `canvas`, `cursor`

**事件：**

```js
el.addEventListener('fb-seek', e => {
  console.log('波形 seek:', e.detail.position, e.detail.progress);
});
```
