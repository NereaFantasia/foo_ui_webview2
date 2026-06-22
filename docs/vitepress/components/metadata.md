# H. 元数据与搜索 

## `<fb-properties-panel>` {#fb-properties-panel}

曲目属性面板。分组显示 metadata、technical、location 信息。

```html
<fb-properties-panel></fb-properties-panel>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| path | string | — | 指定路径，默认当前曲目 |
| groups | string | 'metadata,technical,location' | 显示分组 |
| track-path | string | — | 只读反映：当前曲目路径 |

**CSS Parts:** `container`, `group`, `group-title`, `row`, `label`, `value`

**分组内容：**

- **metadata** — TITLE, ARTIST, ALBUM, ALBUMARTIST, DATE, GENRE, TRACKNUMBER, COMMENT
- **technical** — 编解码器, 比特率, 采样率, 声道, 时长
- **location** — 文件路径

## `<fb-search-bar>` {#fb-search-bar}

搜索栏，支持去抖与最小长度过滤。Enter 键立即搜索（无视去抖）。

```html
<fb-search-bar placeholder="搜索曲目..." debounce="300" min-length="2">
  <span slot="icon">🔍</span>
</fb-search-bar>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| placeholder | string | '搜索媒体库...' | 输入框占位文本 |
| debounce | string | '300' | 去抖延迟毫秒数 |
| min-length | string | '2' | 最小查询长度 |

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

控制台日志面板，自动轮询更新，所有日志行经 `_escHtml` 转义。

```html
<fb-console></fb-console>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| max-lines | string | '200' | 最大行数 |
| auto-scroll | boolean | true | 自动滚动到底部 |
| refresh | string | '1000' | 轮询间隔毫秒数 |

**CSS Parts:** `container`, `line`

**事件：**

```js
el.addEventListener('fb-console-update', e => {
  console.log('日志更新:', e.detail.lineCount, '行');
});
```
