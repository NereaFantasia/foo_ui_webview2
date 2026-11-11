# I. 媒体库

## `<fb-library-tree>` {#fb-library-tree}

按艺术家、专辑或流派分组浏览媒体库的树形组件。支持 Ctrl/Cmd 点击切换、Shift 连续范围多选以及子节点异步按需懒加载。

```html
<fb-library-tree view="artist"></fb-library-tree>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `view` | `'artist' \| 'album' \| 'genre'` | `'artist'` | 分组浏览视图模式（observed 与 reflected） |
| `sort` | string | `'name'` | 组件传递的排序字段（observed） |
| `filter` | string | `''` | 组件传递的过滤条件字符串（observed） |
| `item-count` | string | — | 只读反映顶级条目总数 |

**CSS Parts:** `container`, `header`, `node`, `child`, `node-toggle`, `node-label`, `node-count`, `node-children`

node 元素上的状态属性：`expanded`（已展开）、`selected`（已选中）。

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

// 请求右键菜单；组件本身不会弹出菜单
el.addEventListener('fb-library-context', e => {
  const { key, type, view, x, y, selected } = e.detail;
  // 如有需要，由主题代码调用 fb.ui.showCustomMenu(...) 弹出菜单。
});

// 添加到播放列表完成
el.addEventListener('fb-library-added', e => {
  console.log('添加完成:', e.detail.count, e.detail.type, e.detail.key);
});
```

### 视图模式

- **artist**: 艺术家列表 → 展开显示专辑（子节点类型 `album`）
- **album**: 专辑列表 → 展开显示曲目（子节点类型 `track`）
- **genre**: 流派列表 → 展开显示匹配曲目

事件 detail 中的 `artist` 取自该行的 `data-artist`。在艺术家视图中，`album` 子节点对应其所属艺术家，该行代表该艺术家在对应专辑中的曲目；在专辑视图中，分组节点对应专辑艺术家。事件及 `selected` 中的条目还可能包含 `albumId` 或确切的成员 `tracks`。调用 `el.addToPlaylist(key, type, artist, detail)` 时，若将事件 detail 传入第四个参数，可保留专辑标识与子曲目清单。显式空 `tracks` 数组表示无成员曲目，不再执行按名回退查找。原始元数据标签包含非法 UTF-8 字符时，仅依赖显示名称可能导致同名专辑无法区分。

## `<fb-library-filesystem-tree>` {#fb-library-filesystem-tree}

基于文件系统结构的媒体库树形组件。基于 `library.getRoots()` 获取根目录并通过 `library.browseTree()` 递归按需懒加载目录层级。支持 Ctrl/Cmd 点击切换与 Shift 连续范围多选。

```html
<fb-library-filesystem-tree></fb-library-filesystem-tree>
```

| 属性 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `root-id` | string | — | 锁定展示指定的单个根目录 ID 并隐藏多根标题栏（observed） |
| `include-files` | string | `'false'` | 是否显示音频文件节点（observed，仅取值 `true` 时展示文件） |

**CSS Parts:** `container`, `header`, `node`, `root`, `directory`, `file`, `node-toggle`, `node-label`, `node-count`, `node-children`

node 上的 `data-node-type` 标识节点类型：`header`、`root`、`directory`、`file`。

node 上的数据属性：`data-root-id`、`data-path-id`、`data-absolute-path`。

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

// 请求右键菜单；组件本身不会弹出菜单
el.addEventListener('fb-library-fs-context', e => {
  const { type, rootId, pathId, absolutePath, x, y } = e.detail;
  // 如有需要，由主题代码调用 fb.ui.showCustomMenu(...) 弹出菜单。
});
```

### 节点类型

| `data-node-type` | 说明 |
| --- | --- |
| `header` | 顶级全部音乐标题栏 |
| `root` | 媒体库根目录 |
| `directory` | 子目录文件夹 |
| `file` | 音频文件节点（仅在 `include-files="true"` 时展示） |
