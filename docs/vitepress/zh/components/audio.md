# G. 评分与音频设置

## `<fb-rating>` {#fb-rating}

星级曲目评分组件。点击当前已选中的星级可重置取消评分为 0。

```html
<fb-rating max="5"></fb-rating>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `max` | string | `'5'` | 最大评分星数（observed） |
| `readonly` | boolean | false | 只读模式；开启后禁用点击、键盘与悬停预览交互（observed） |
| `value` | string | — | 只读反映当前评分数值 |

**CSS Parts:** `stars`, `star`

每个 `star` 上暴露 `data-value` 与 `aria-checked` 属性，已填充或悬停预览的星级带有 `data-filled`。

**事件：**

```js
el.addEventListener('fb-rating-change', e => {
  console.log('评分:', e.detail.value, '路径:', e.detail.path);
});
```

## `<fb-output-selector>` {#fb-output-selector}

音频输出设备下拉选择器组件。

```html
<fb-output-selector></fb-output-selector>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `device-name` | string | — | 只读反映当前音频输出设备名称 |

**CSS Parts:** `select`

**事件：**

```js
el.addEventListener('fb-output-change', e => {
  console.log('输出设备:', e.detail.index, e.detail.name);
  console.log('outputId:', e.detail.outputId, 'deviceId:', e.detail.deviceId);
});
```

## `<fb-dsp-preset-selector>` {#fb-dsp-preset-selector}

DSP 效果预设下拉选择器组件。

```html
<fb-dsp-preset-selector></fb-dsp-preset-selector>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `preset-name` | string | — | 只读反映当前生效的 DSP 预设名称 |

**CSS Parts:** `select`

**事件：**

```js
el.addEventListener('fb-dsp-change', e => {
  console.log('DSP 预设:', e.detail.index, e.detail.name);
});
```

## `<fb-replaygain-selector>` {#fb-replaygain-selector}

回放增益（ReplayGain）处理模式下拉选择器组件。

```html
<fb-replaygain-selector></fb-replaygain-selector>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `mode` | string | — | 只读反映当前模式：`none`、`track`、`album` 或 `auto` |

**CSS Parts:** `select`

**事件：**

```js
el.addEventListener('fb-replaygain-change', e => {
  console.log('ReplayGain 模式:', e.detail.mode);
});
```
