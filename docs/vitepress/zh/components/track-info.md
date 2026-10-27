# C. 曲目信息

## `<fb-track-text>` {#fb-track-text}

通用曲目文本显示组件，替代旧版 `fb-title`、`fb-artist` 与 `fb-album` 元素；支持内置元数据字段或自定义 TitleFormatting 表达式。

```html
<fb-track-text field="title"></fb-track-text>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `field` | string | `'title'` | 待展示的元数据字段名称（observed） |
| `tf` | string | — | TitleFormatting 格式化表达式（observed，优先级高于 field） |
| `placeholder` | string | — | 无数据或未播放时的占位文本（observed） |

**CSS Parts:** `text`

## `<fb-cover-art>` {#fb-cover-art}

封面图片展示组件。设置 `use-fb2k` 属性时优先调用 SDK `getFb2kUrl()` 辅助接口；未找到可用图像时自动回退至 `artwork.getCurrent()` 标准路径。

```html
<fb-cover-art use-fb2k>
  <span slot="placeholder">🎵</span>
</fb-cover-art>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `type` | string | `'front'` | 封面图类型：`front`、`back`、`disc`、`icon` 或 `artist`（observed） |
| `use-fb2k` | boolean | false | 优先尝试 SDK URL 辅助接口查询（observed，推荐） |
| `loaded` | boolean | — | 只读反映图片是否加载成功 |
| `src` | string | — | 只读反映当前非 data URI 的图片地址 |

**CSS Parts:** `container`, `image`  
**Slots:** `placeholder`

**事件：**

```js
el.addEventListener('fb-cover-load', e => { /* 封面加载成功 */ });
el.addEventListener('fb-cover-error', e => { /* 封面加载失败 */ });
```

## `<fb-time-current>` {#fb-time-current}

当前播放时间展示组件，输出格式为 `m:ss` 或 `h:mm:ss`。

```html
<fb-time-current></fb-time-current>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `seconds` | string | — | 只读反映当前播放秒数整型字符串 |

**CSS Parts:** `text`

## `<fb-time-total>` {#fb-time-total}

当前曲目总时长展示组件，输出格式为 `m:ss` 或 `h:mm:ss`。

```html
<fb-time-total></fb-time-total>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `seconds` | string | — | 只读反映总时长秒数整型字符串 |

**CSS Parts:** `text`

## `<fb-time-remaining>` {#fb-time-remaining}

当前曲目剩余时间展示组件，以负号 `-` 为前缀展示。

```html
<fb-time-remaining></fb-time-remaining>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `seconds` | string | — | 只读反映剩余秒数整型字符串 |

**CSS Parts:** `text`

## `<fb-tech-info>` {#fb-tech-info}

曲目技术规格参数展示组件。`field="all"` 时输出格式示例：`FLAC | 1411 kbps | 44.1 kHz | Stereo`。

```html
<fb-tech-info field="all"></fb-tech-info>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `field` | string | `'all'` | 待展示的技术参数：`all`、`codec`、`bitrate`、`samplerate` 或 `channels`（observed） |

**CSS Parts:** `text`
