# D. 播放列表

## `<fb-playlist-tabs>` {#fb-playlist-tabs}

播放列表选项卡标签栏组件，支持 ArrowLeft 与 ArrowRight 键盘切换、边缘拖拽调宽、拖拽重排与溢出滚动。

```html
<fb-playlist-tabs></fb-playlist-tabs>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `active-index` | string | — | 只读反映当前激活播放列表索引 |
| `count` | string | — | 只读反映播放列表总数 |
| `native-context` | boolean | false | 开启时同时呼出原生播放列表右键菜单（observed） |

**只读宿主状态：** 拖拽标签时为 `dragging`，标签栏内容溢出时为 `overflow`

**CSS Parts:** `tabs-container`, `tab`, `tab-name`, `tab-count`, `drop-indicator`, `add-button`

`tab` 上 `locked` 属性标识锁定的播放列表，`data-guid` 是该列表的 GUID。

标签栏按 GUID 指定列表：标签画出来之后别的列表再增删，点击激活的仍是这个标签的列表；列表已被删掉的标签什么也不激活。`fb-playlist-select` 与 `fb-playlist-reorder` 在宿主完成之后才派发。

**事件：**

```js
el.addEventListener('fb-playlist-select', e => {
  console.log('选中播放列表:', e.detail.index, e.detail.guid);
});
el.addEventListener('fb-playlist-context', e => {
  // 菜单关闭后按 e.detail.guid 操作；到那时序号可能已指向别的列表。
  console.log('右键菜单:', e.detail.guid, e.detail.x, e.detail.y);
});
el.addEventListener('fb-playlist-add', e => {
  console.log('点击新建');
});
el.addEventListener('fb-playlist-reorder', e => {
  console.log('拖拽重排:', e.detail.fromIndex, '→', e.detail.toIndex);
  console.log('新顺序:', e.detail.newOrderGuids);
});
```

## `<fb-resizable-header>` {#fb-resizable-header}

可调列宽表头组件，支持鼠标拖拽调宽、列拖拽重排与点击表头排序。通常与 `<fb-playlist-view>` 配合协同渲染。

```html
<fb-resizable-header columns='[{"id":"title","label":"Title","width":"2fr"}]'></fb-resizable-header>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `columns` | string (JSON) | `[]` | 列定义数组（每项包含 `id`、`label`/`name`、`width` 如 `1fr`、`sortable` 与 `fixed`） |
| `sort-column` | string | — | 当前排序列 ID（observed 与 reflected） |
| `sort-direction` | `'asc' \| 'desc'` | `'asc'` | 当前排序方向（observed 与 reflected） |

带有 `fixed` 的列不可调整宽度或重排。**只读宿主状态：** `resizing`（正在调宽）、`reordering`（正在重排）

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

基于 requestAnimationFrame 与 DOM 池虚拟滚动的播放列表核心视图组件。支持 Ctrl/Cmd 与 Shift 连续多选、Enter 键播放、Delete 键删除以及 Ctrl+A 全选。

```html
<fb-playlist-view row-height="24"></fb-playlist-view>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `playlist` | string | — | 播放列表的 `guid`（以 `{` 开头的值）或序号；省略时自动跟随当前活动列表（observed），见下文 |
| `columns` | string | `'index,title,artist,album,duration'` | 以逗号分隔的列名列表（observed） |
| `row-height` | string | `'32'` | 行高像素值（observed） |
| `grid-template` | string | — | CSS `grid-template-columns` 属性值，用于与 fb-resizable-header 保持列宽同步（observed） |
| `formats` | string (JSON) | `{}` | 自定义列 ID 与 TitleFormatting 表达式的映射字典（observed） |
| `track-count` | string | — | 只读反映当前列表曲目总数 |
| `selected-count` | string | — | 只读反映当前选中的曲目总数 |

`play_count`、`first_played`、`last_played`、`added` 四列显示 foo_playcount 的字段，`rating` 列显示五颗可点击的星。要显示 [`fb.playlist.getAvailableColumns()`](../sdk/playlist.md#getavailablecolumns) 列出的其他列，把该列的 `pattern` 放进 `formats`，键用你自己取的列 ID，再把这个 ID 写进 `columns`。不要拿列的 `name` 当 ID：它是随 foobar2000 语言变化的显示文字。

```html
<fb-playlist-view columns="index,title,play_count,rating,mood"
  formats='{"mood":"%mood%"}'></fb-playlist-view>
```

**CSS Parts:** `viewport`, `scroll-content`, `row`, `row-{column}`

`row` 上的状态属性（可用于 CSS 选择器）：`selected`、`focused`、`playing`。

`playlist` 给 GUID 时，别的列表增删或重排，视图仍显示这个列表；它被删掉后视图变空。给序号时显示位于该位置的列表，列表增删或重排后重新查一次。不给时跟随活动列表，没有活动列表就什么也不显示。视图知道自己显示哪个列表之后，每次调用宿主都按 GUID 指定它，下面各事件的 detail 也以 `playlistGuid` 带上这个 GUID。

**事件：**

```js
el.addEventListener('fb-track-select', e => {
  console.log('选中:', e.detail.index, e.detail.indices, e.detail.playlistGuid);
});
el.addEventListener('fb-track-play', e => {
  console.log('双击播放:', e.detail.index, e.detail.playlistGuid);
});
el.addEventListener('fb-track-context', e => {
  console.log('右键菜单:', e.detail.indices, e.detail.playlistGuid, e.detail.x, e.detail.y);
});
```

## `<fb-queue-view>` {#fb-queue-view}

播放队列条目视图组件，双击队列中行条目可将其从队列中移除。

```html
<fb-queue-view columns="index,title,artist,duration">
  <span slot="empty">队列为空</span>
</fb-queue-view>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `columns` | string | `'index,title,artist,duration'` | 以逗号分隔的列名列表 |
| `empty` | boolean | — | 只读反映当前播放队列是否为空 |
| `count` | string | — | 只读反映队列条目总数 |

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

播放列表下拉选择器组件。

```html
<fb-playlist-selector></fb-playlist-selector>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `selected-index` | string | — | 只读反映当前选中的播放列表索引 |
| `selected-name` | string | — | 只读反映当前选中的播放列表名称 |

**CSS Parts:** `select`

**事件：**

```js
el.addEventListener('fb-playlist-pick', e => {
  console.log('选择:', e.detail.index, e.detail.guid, e.detail.name);
});
```
