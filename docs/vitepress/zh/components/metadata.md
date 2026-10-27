# H. 元数据与搜索

## `<fb-properties-panel>` {#fb-properties-panel}

只读曲目属性面板组件，按 metadata、technical 与 location 分组展示曲目详细规格。

```html
<fb-properties-panel></fb-properties-panel>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `path` | string | 当前曲目 | 显式指定的曲目路径；指定后不再随播放曲目切换自动更新 |
| `groups` | string | `'metadata,technical,location'` | 以逗号分隔的待展示属性分组子集 |
| `track-path` | string | — | 只读反映当前已加载展示的曲目路径 |

**CSS Parts:** `container`, `group`, `group-title`, `row`, `label`, `value`

**分组内容：**

- **metadata** — TITLE, ARTIST, ALBUM, ALBUMARTIST, DATE, GENRE, TRACKNUMBER, COMMENT
- **technical** — 编解码器（codec）、比特率（bitrate）、采样率（sample rate）、声道数（channels）、时长（duration）
- **location** — 音频文件绝对路径

## `<fb-search-bar>` {#fb-search-bar}

带输入去抖（debounce）与最小字符过滤的媒体库搜索栏。按 Enter 键立即发起搜索并跳过去抖等待；空查询将被忽略。

```html
<fb-search-bar placeholder="搜索曲目..." debounce="300" min-length="2">
  <span slot="icon">🔍</span>
</fb-search-bar>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `placeholder` | string | `'Search library...'` | 输入框提示占位文本 |
| `debounce` | string | `'300'` | 按键输入去抖延迟毫秒数 |
| `min-length` | string | `'2'` | 触发去抖搜索所需的最小查询字符数 |

**CSS Parts:** `container`, `input`  
**Slots:** `icon`（默认 🔍）

**事件：**

```js
el.addEventListener('fb-search', e => {
  console.log('搜索:', e.detail.query);
});
el.addEventListener('fb-search-result', e => {
  console.log('结果:', e.detail.count, '首匹配', e.detail.query);
  console.log('曲目:', e.detail.tracks);
});
```

## `<fb-console>` {#fb-console}

foobar2000 控制台实时日志查看器，定时轮询宿主控制台输出并对日志行实施安全 HTML 转义。

```html
<fb-console></fb-console>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `max-lines` | string | `'200'` | 每次轮询获取的最大日志行数 |
| `auto-scroll` | string | enabled | 自动滚动到底部；设置为 exact 字符串 `false` 时禁用自动滚动 |
| `refresh` | string | `'1000'` | 轮询更新间隔毫秒数 |

**CSS Parts:** `container`, `line`

**事件：**

```js
el.addEventListener('fb-console-update', e => {
  console.log('日志更新:', e.detail.lineCount, '行');
});
```
