# I. 媒体库 

## `<fb-library-tree>` {#fb-library-tree}

按 Artist / Album / Genre 分组浏览媒体库的树形组件。支持 Ctrl/Shift 多选、懒加载子节点。

```html
<fb-library-tree view="artist"></fb-library-tree>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| view | string | 'artist' | 浏览视图：'artist' / 'album' / 'genre'（observed） |
| sort | string | 'name' | 排序方式（observed） |
| filter | string | — | 过滤字符串（observed） |
| item-count | string | — | 只读反映：顶级项目数量 |

**CSS Parts:** `container`, `header`, `node`, `child`, `node-toggle`, `node-label`, `node-count`, `node-children`

node 上的状态属性：`expanded`（已展开）、`selected`（已选中）

**事件：**

```js
// 选中
el.addEventListener('fb-library-select', e => {
  console.log('选中:', e.detail.key, e.detail.type, e.detail.view);
  console.log('当前选中项:', e.detail.selected);
});

// 双击播放
el.addEventListener('fb-library-play', e => {
  console.log('播放:', e.detail.key, e.detail.type, e.detail.view);
});

// 右键菜单
el.addEventListener('fb-library-context', e => {
  const { key, type, view, x, y, selected } = e.detail;
  // 调用 ui.showCustomMenu 弹出菜单
});

// 添加到播放列表完成
el.addEventListener('fb-library-added', e => {
  console.log('添加完成');
});
```

### 视图模式 

- **artist**: 艺术家列表 → 展开显示专辑（子节点类型 `album`）
- **album**: 专辑列表 → 展开显示曲目（子节点类型 `track`）
- **genre**: 流派列表 → 展开显示匹配曲目

## `<fb-library-filesystem-tree>` {#fb-library-filesystem-tree}

基于文件系统根树的媒体库浏览组件，通过 `getRoots` + `browseTree` API 按目录结构懒加载。支持 Ctrl/Shift 多选。

```html
<fb-library-filesystem-tree></fb-library-filesystem-tree>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| root-id | string | — | 指定单个 root ID，省略则显示所有根目录（observed） |
| include-files | string | 'false' | 是否显示文件节点（observed） |

**CSS Parts:** `container`, `header`, `node`, `root`, `directory`, `file`, `node-toggle`, `node-label`, `node-count`, `node-children`

node 上的 `data-node-type` 标识节点类型：`header` / `root` / `directory` / `file`

node 上的数据属性：`data-root-id`、`data-path-id`、`data-absolute-path`

**事件：**

```js
// 选中节点
el.addEventListener('fb-library-fs-select', e => {
  console.log('选中:', e.detail.type, e.detail.absolutePath);
  if (e.detail.file) console.log('文件信息:', e.detail.file);
});

// 双击播放
el.addEventListener('fb-library-fs-play', e => {
  console.log('播放:', e.detail.type, e.detail.absolutePath);
});

// 右键菜单
el.addEventListener('fb-library-fs-context', e => {
  const { type, rootId, pathId, absolutePath, x, y } = e.detail;
  // 调用 ui.showCustomMenu 弹出菜单
});
```

### 节点类型 

| data-node-type | 说明 |
| --- | --- |
| header | 顶级标题（"所有音乐"） |
| root | 媒体库根目录 |
| directory | 子目录 |
| file | 音频文件（仅在 include-files="true" 时显示） |
