# D. 播放列表 

## `<fb-playlist-tabs>` {#fb-playlist-tabs}

播放列表标签栏，支持键盘 ArrowLeft/ArrowRight 切换。

```html
<fb-playlist-tabs></fb-playlist-tabs>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| active-index | string | — | 只读反映：当前激活索引 |
| count | string | — | 只读反映：播放列表数量 |
| native-context | boolean | — | 设置时启用原生右键菜单 |

**宿主属性（只读状态）：** `dragging`（拖拽进行中）、`overflow`（标签栏溢出时）

**CSS Parts:** `tabs-container`, `tab`, `tab-name`, `tab-count`, `drop-indicator`, `add-button`

tab 上 `locked` 属性标识锁定的播放列表。

**事件：**

```js
el.addEventListener('fb-playlist-select', e => {
  console.log('选中播放列表:', e.detail.index);
});
el.addEventListener('fb-playlist-context', e => {
  console.log('右键菜单:', e.detail.index, e.detail.x, e.detail.y);
});
el.addEventListener('fb-playlist-add', e => {
  console.log('点击新建');
});
el.addEventListener('fb-playlist-reorder', e => {
  console.log('拖拽重排:', e.detail);
});
```

## `<fb-resizable-header>` {#fb-resizable-header}

可调列宽表头，支持拖拽调整列宽、拖拽列重排序、点击列排序。通常与 `<fb-playlist-view>` 配合使用。

```html
<fb-resizable-header columns='[{"id":"title","label":"Title","width":"2fr"}]'></fb-resizable-header>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| columns | string (JSON) | [] | 列定义数组，每项含 id、label、width（如 '1fr'）、sortable（默认 true）、fixed（不可拖拽重排） |
| sort-column | string | — | 当前排序列 ID（observed） |
| sort-direction | string | 'asc' | 排序方向 'asc'/'desc'（observed） |

**宿主属性（只读状态）：** `resizing`（正在调整列宽时）、`reordering`（正在拖拽重排时）

**CSS Parts:** `header`, `cell`, `cell-{id}`, `resize-handle`, `sort-icon`, `sort-active`, `drop-indicator`, `drag-ghost`, `label`

**只读属性（JS）：**

- `gridTemplate` — 当前 CSS `grid-template-columns` 值（同步到 playlist-view）
- `columnIds` — 当前列 ID 顺序数组

**事件：**

```js
// 列宽调整
el.addEventListener('fb-column-resize', e => {
  console.log('列宽:', e.detail.column, e.detail.width);
  // 同步到 playlist-view
  playlistView.setAttribute('grid-template', e.detail.gridTemplate);
});

// 列重排序
el.addEventListener('fb-column-reorder', e => {
  console.log('重排:', e.detail.fromIndex, '→', e.detail.toIndex);
  console.log('新顺序:', e.detail.columns);
});

// 点击列排序
el.addEventListener('fb-column-sort', e => {
  console.log('排序:', e.detail.column, e.detail.direction);
});

// 右键列头
el.addEventListener('fb-header-context', e => {
  console.log('列头右键:', e.detail.x, e.detail.y);
});
```

## `<fb-playlist-view>` {#fb-playlist-view}

虚拟滚动播放列表视图，支持 rAF + DOM 池、Ctrl/Shift 多选、Enter 播放、Delete 删除、Ctrl+A 全选。

```html
<fb-playlist-view row-height="24"></fb-playlist-view>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| playlist | string | — | 播放列表索引，默认活动列表（observed） |
| columns | string | 'index,title,artist,album,duration' | 逗号分隔的列名（observed） |
| row-height | string | '32' | 行高像素（observed） |
| grid-template | string | — | CSS grid-template-columns 值，用于同步 fb-resizable-header 的列宽（observed） |
| formats | string (JSON) | — | 自定义列的 Titleformat 表达式映射（observed） |
| track-count | string | — | 只读反映：曲目总数 |
| selected-count | string | — | 只读反映：选中曲目数 |

**CSS Parts:** `viewport`, `scroll-content`, `row`, `row-{column}`

`row` 上的状态属性（可用于 CSS 选择器）：`selected`、`focused`、`playing`

**事件：**

```js
el.addEventListener('fb-track-select', e => {
  console.log('选中:', e.detail.index, e.detail.indices);
});
el.addEventListener('fb-track-play', e => {
  console.log('双击播放:', e.detail.index);
});
el.addEventListener('fb-track-context', e => {
  console.log('右键菜单:', e.detail.indices, e.detail.x, e.detail.y);
});
```

## `<fb-queue-view>` {#fb-queue-view}

播放队列视图，双击条目移除。

```html
<fb-queue-view columns="index,title,artist,duration">
  <span slot="empty">队列为空</span>
</fb-queue-view>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| columns | string | 'index,title,artist,duration' | 逗号分隔的列名 |
| empty | boolean | — | 只读反映：队列是否为空 |
| count | string | — | 只读反映：队列条目数 |

**CSS Parts:** `container`, `row`, `row-{column}`, `empty`
**Slots:** `empty`

**事件：**

```js
el.addEventListener('fb-queue-context', e => {
  console.log('右键菜单:', e.detail.index, e.detail.x, e.detail.y);
});
el.addEventListener('fb-queue-remove', e => {
  console.log('移除:', e.detail.index);
});
```

## `<fb-playlist-selector>` {#fb-playlist-selector}

播放列表下拉选择器。

```html
<fb-playlist-selector></fb-playlist-selector>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| selected-index | string | — | 只读反映：当前选中索引 |
| selected-name | string | — | 只读反映：当前选中名称 |

**CSS Parts:** `select`

**事件：**

```js
el.addEventListener('fb-playlist-pick', e => {
  console.log('选择:', e.detail.index, e.detail.name);
});
```
