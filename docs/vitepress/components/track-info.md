# C. 曲目信息 

## `<fb-track-text>` {#fb-track-text}

通用曲目文本显示，替代旧版 `fb-title`/`fb-artist`/`fb-album`。

```html
<fb-track-text field="title"></fb-track-text>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| field | string | 'title' | 显示的字段名（observed） |
| tf | string | — | Title Formatting 表达式，优先于 field（observed） |
| placeholder | string | — | 无数据时的占位文本（observed） |

**CSS Parts:** `text`

## `<fb-cover-art>` {#fb-cover-art}

封面图显示，推荐使用 `use-fb2k` 属性启用高性能加载路径。

```html
<fb-cover-art use-fb2k>
  <span slot="placeholder">🎵</span>
</fb-cover-art>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| type | string | 'front' | 封面类型 front/back/disc/icon/artist（observed） |
| use-fb2k | boolean | false | 使用 fb2k:// 高性能协议（observed，推荐） |
| loaded | boolean | — | 只读反映：是否已加载 |
| src | string | — | 只读反映：当前图片 src |

**CSS Parts:** `container`, `image`
**Slots:** `placeholder`

**事件：**

```js
el.addEventListener('fb-cover-load', e => { /* 封面加载成功 */ });
el.addEventListener('fb-cover-error', e => { /* 封面加载失败 */ });
```

## `<fb-time-current>` {#fb-time-current}

当前播放时间，格式 `m:ss` 或 `h:mm:ss`。

```html
<fb-time-current></fb-time-current>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| seconds | string | — | 只读反映：当前秒数 |

**CSS Parts:** `text`

## `<fb-time-total>` {#fb-time-total}

总时长。

```html
<fb-time-total></fb-time-total>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| seconds | string | — | 只读反映：总秒数 |

**CSS Parts:** `text`

## `<fb-time-remaining>` {#fb-time-remaining}

剩余时间，显示前缀 `-`。

```html
<fb-time-remaining></fb-time-remaining>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| seconds | string | — | 只读反映：剩余秒数 |

**CSS Parts:** `text`

## `<fb-tech-info>` {#fb-tech-info}

技术信息显示。`field="all"` 时输出格式：`FLAC | 1411 kbps | 44.1 kHz | Stereo`。

```html
<fb-tech-info field="all"></fb-tech-info>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| field | string | 'all' | 显示字段：all/codec/bitrate/samplerate/channels（observed） |

**CSS Parts:** `text`
