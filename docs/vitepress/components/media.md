# F. 歌词与可视化 

## `<fb-lyrics-panel>` {#fb-lyrics-panel}

歌词面板。同步歌词高亮当前行，点击歌词行 seek 到对应时间。

```html
<fb-lyrics-panel></fb-lyrics-panel>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| source | string | 'any' | 歌词来源 |
| scroll | string | 'smooth' | 滚动模式 smooth / auto |
| has-lyrics | boolean | — | 只读反映：是否有歌词 |
| current-line | string | — | 只读反映：当前行索引 |

**CSS Parts:** `container`, `line`

每个 `line` 上的 CSS class：`past`（已播放）/ `current`（当前行）/ `future`（未播放）。

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

频谱可视化。mode='raw' 时通过事件输出原始频谱数据，支持 DPR 自适应 + ResizeObserver。

```html
<fb-spectrum-visualizer bands="64" fps="30" mode="bars"></fb-spectrum-visualizer>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| bands | string | '64' | 频带数 |
| fps | string | '30' | 帧率 |
| mode | string | 'raw' | 模式 raw / bars / wave |
| fft-size | string | 自动 | FFT 大小（覆盖自动计算，必须为 2 的幂） |
| dpr | string | devicePixelRatio | 像素比（降低此值可减少 GPU 开销，如背景频谱用 dpr="1"） |
| fall-speed | string | '0.28' | 回落系数（0..1），值越大回落越快 |
| rise-speed | string | '0.65' | 上升系数（0..1），值越大上升越快 |

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

波形图显示。点击波形 seek，cursor 跟随播放进度更新。

```html
<fb-waveform resolution="200"></fb-waveform>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| resolution | string | '200' | 分辨率 |
| progress | string | — | 只读反映：当前进度 |
| mode | string | 'rms' | 采样方法：'peak' 或 'rms' |
| normalize | string | 'adaptive' | 归一化模式：'adaptive'（百分位+幂曲线，推荐）、'gamma'（纯 gamma 映射）、'histogram'（直方图均衡化） |
| gamma | string | '1.5' | 响度伽马曲线指数，范围 (0, 2]。仅在 normalize="gamma" 时生效。值 > 1 拉开高密度段的视觉对比度；值 < 1 提升弱信号可见度 |

**CSS Parts:** `canvas`, `cursor`

**事件：**

```js
el.addEventListener('fb-seek', e => {
  console.log('波形 seek:', e.detail.position, e.detail.progress);
});
```
