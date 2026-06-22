# G. 评分与音频设置 

## `<fb-rating>` {#fb-rating}

星级评分。点击已选星取消评分（toggle 到 0）。

```html
<fb-rating max="5"></fb-rating>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| max | string | '5' | 最大星数（observed） |
| readonly | boolean | false | 只读模式（observed） |
| value | string | — | 只读反映：当前评分 |

**CSS Parts:** `stars`, `star`

每个 `star` 上有 `data-value` 和 `aria-checked` 属性。

**事件：**

```js
el.addEventListener('fb-rating-change', e => {
  console.log('评分:', e.detail.value, '路径:', e.detail.path);
});
```

## `<fb-output-selector>` {#fb-output-selector}

音频输出设备选择器。

```html
<fb-output-selector></fb-output-selector>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| device-name | string | — | 只读反映：当前设备名称 |

**CSS Parts:** `select`

**事件：**

```js
el.addEventListener('fb-output-change', e => {
  console.log('输出设备:', e.detail.index, e.detail.name);
  console.log('outputId:', e.detail.outputId, 'deviceId:', e.detail.deviceId);
});
```

## `<fb-dsp-preset-selector>` {#fb-dsp-preset-selector}

DSP 预设选择器。

```html
<fb-dsp-preset-selector></fb-dsp-preset-selector>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| preset-name | string | — | 只读反映：当前预设名称 |

**CSS Parts:** `select`

**事件：**

```js
el.addEventListener('fb-dsp-change', e => {
  console.log('DSP 预设:', e.detail.index, e.detail.name);
});
```

## `<fb-replaygain-selector>` {#fb-replaygain-selector}

ReplayGain 模式选择器。

```html
<fb-replaygain-selector></fb-replaygain-selector>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| mode | string | — | 只读反映：当前模式 none / track / album / auto |

**CSS Parts:** `select`

**事件：**

```js
el.addEventListener('fb-replaygain-change', e => {
  console.log('ReplayGain 模式:', e.detail.mode);
});
```
