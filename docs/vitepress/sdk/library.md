# fb.library 媒体库 

> 枚举建议：全库曲目优先使用 `getCount()` + `getAll(start, count)` 分页遍历。 获取真实媒体库根目录请使用 `getRoots()`；浏览目录树使用 `browseTree()`；高层遍历使用 `enumerateTree()`。`browseDirectory()` / `enumerateDirectories()` 已弃用，仅保留为 legacy 目录投影视图。

## search(query, limit?) 

搜索媒体库。返回 `{items: [...], total, offset, limit, hasMore}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| query | string | 搜索查询（支持 foobar2000 查询语法） |
| limit | number | 最大返回数量（默认 100） |

> `items` 为主字段，`tracks` 为向后兼容别名（deprecated）。`hasMore` 表示是否有更多结果。

```javascript
const results = await fb.library.search('artist HAS Beatles', 100);
console.log(`找到 ${results.total} 首`);
if (results.hasMore) console.log('还有更多结果');
```

## getAlbums(limit?) 

获取所有专辑。

```javascript
const albums = await fb.library.getAlbums(50);
// [{album: "Abbey Road", artist: "The Beatles", count: 17}, ...]
```

## getArtists(limit?) 

获取所有艺术家。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| limit | number | 最大返回数量 |

```javascript
const artists = await fb.library.getArtists(100);
```

## getStats() 

获取媒体库统计信息。返回 `{totalTracks, totalDuration, totalSize, ...}`。

```javascript
const stats = await fb.library.getStats();
console.log(`${stats.totalTracks} 首，总时长 ${stats.totalDuration} 秒`);
```

## getGenres() 

获取流派列表。返回 `{genres: [{name, trackCount}]}`。

```javascript
const r = await fb.library.getGenres();
// {genres: [{name: 'Rock', trackCount: 5000}, ...]}
```

## getStatus() 

获取媒体库状态。返回 `{initialized, enabled, ...}`。

```javascript
const s = await fb.library.getStatus();
if (s.initialized) console.log('媒体库已初始化');
```

## getCount() 

获取媒体库曲目总数。返回 `{count}`。

```javascript
const { count } = await fb.library.getCount();
```

## getAll(start, count) 

获取所有曲目（支持分页）。返回 `{tracks: [...], items: [...], total, offset, limit, fromCache}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| start | number | 起始偏移 |
| count | number | 获取数量 |

```javascript
const r = await fb.library.getAll(0, 100);
console.log(`媒体库共 ${r.total} 首，本次返回 ${r.tracks.length} 首`);
```

> `tracks` 与 `items` 内容相同，`items` 为兼容别名。

高层分页枚举器（异步生成器），内部基于 `getCount()` + `getAll()`。

```javascript
for await (const page of fb.library.enumerateTracks({ pageSize: 500 })) {
  console.log(page.fetched, '/', page.total);
}
```

## refresh() 

刷新媒体库。返回 `{success}`。

## getByPath(path) 

通过文件路径在媒体库中搜索曲目。返回 `{found, path, title, artist, album, duration, ...}`（字段在顶层，无嵌套）。

```javascript
const r = await fb.library.getByPath('E:\\\\Music\\\\song.flac');
if (r.found) console.log(r.title, r.artist);
```

获取真实媒体库根目录列表。使用 `library_manager::get_relative_path()` 按段比较推导。

```javascript
const { roots, total, indexedTracks } = await fb.library.getRoots();
for (const root of roots) {
  console.log(root.displayName, root.absolutePath, root.trackCount);
}
```

| 响应字段 | 类型 | 说明 |
| --- | --- | --- |
| roots | LibraryRootInfo[] | 根目录列表，按 displayName 排序 |
| total | number | 根目录数量 |
| indexedTracks | number | 成功索引的条目数 |
| skippedTracks | number | 跳过的条目数 |
| enabled | boolean | 媒体库是否启用 |
| fromCache | boolean | 是否来自缓存 |

| 根字段 | 类型 | 说明 |
| --- | --- | --- |
| id | string | 稳定 root 标识；当前为规范化 absolutePath |
| displayName | string | 默认取目录名；同名冲突时回退完整路径 |
| rawPath | string | 当前与 absolutePath 相同 |
| absolutePath | string | 规范化本地绝对路径 |
| trackCount | number | 该根下媒体库条目数 |

> 仅可解析为稳定本地绝对路径的条目会进入根列表。`http://`、`file-relative://`、`unpack://` 等协议型条目会计入 `skippedTracks`。 首次调用同步构建索引，后续走缓存。媒体库变化或调用 `invalidateCache()` 时自动失效。

按 `rootId` + `pathId` 浏览 typed 目录树。先调用 `getRoots()` 获取可用的 `rootId`。

```javascript
const { roots } = await fb.library.getRoots();
const tree = await fb.library.browseTree({ rootId: roots[0].id });
for (const dir of tree.directories) {
  console.log(dir.name, dir.trackCount, dir.hasChildren);
}
// 展开子目录并包含文件
const sub = await fb.library.browseTree({
  rootId: roots[0].id,
  pathId: tree.directories[0].pathId,
  includeFiles: true
});
```

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| rootId | string | ✓ | 媒体库根 ID（来自 getRoots().roots[].id） |
| pathId | string | ✗ | 根下相对路径，/ 分隔（缺省 "" 表示根） |
| includeFiles | boolean | ✗ | 是否包含文件列表（默认 false） |
| recursiveFiles | boolean | ✗ | 递归包含后代文件（默认 false，仅 includeFiles=true 时生效） |

| 响应字段 | 类型 | 说明 |
| --- | --- | --- |
| root | LibraryRootInfo | 所属根信息 |
| pathId | string | 请求的 pathId |
| absolutePath | string | 当前目录绝对路径 |
| directories | LibraryDirectoryNodeInfo[] | 直接子目录，按 displayName 排序 |
| files | TrackInfo[] | 文件列表（includeFiles=false 时为空数组） |
| fromCache | boolean | 是否来自缓存 |

**错误**: `rootId` 缺失返回 `"rootId is required"`；不存在返回 `"Unknown rootId"`；`pathId` 不存在返回 `"Path not found"`。

Root-aware 异步树遍历器（异步生成器），基于 `browseTree()` 实现 BFS/DFS 遍历。

```javascript
for await (const batch of fb.library.enumerateTree({
  rootId: roots[0].id,
  strategy: 'bfs',
  includeFiles: true
})) {
  console.log(batch.pathId, batch.directories.length, batch.files.length);
  console.log(`进度: ${batch.visited} 已访问, ${batch.pending} 待访问`);
}
```

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| rootId | string | ✓ | 媒体库根 ID（来自 getRoots().roots[].id） |
| pathId | string | ✗ | 起始路径，缺省 "" 从根开始 |
| includeFiles | boolean | ✗ | 是否包含直接文件（默认 false） |
| strategy | `'bfs' \| 'dfs'` | ✗ | 遍历策略（默认 'bfs'） |
| signal | AbortSignal | ✗ | 中断信号 |
| onProgress | Function | ✗ | 进度回调 { rootId, pathId, absolutePath, visited, pending } |

| yield 字段 | 类型 | 说明 |
| --- | --- | --- |
| ...browseTree响应 | - | 包含 root, pathId, absolutePath, directories, files, fromCache |
| visited | number | 已访问节点数 |
| pending | number | 待访问节点数 |

| return 字段 | 类型 | 说明 |
| --- | --- | --- |
| rootId | string | 遍历的根 ID |
| visited | number | 总访问节点数 |
| aborted | boolean | 是否被 signal 中断 |

> 每个 yield 对应一次 `browseTree({ recursiveFiles: false })` 调用，`files` 只包含当前节点的直接文件，不重复。

> Legacy API，不推荐作为根入口，请使用 `getRoots()` + `browseTree()` + `enumerateTree()`。

浏览媒体库目录投影视图。返回 legacy 目录字符串和文件列表。

> @deprecated `path === ''` 只表示“投影后的顶层目录视图”，不等于 foobar2000 已配置的媒体库根目录列表。

```javascript
const root = await fb.library.browseDirectory('', false);
```

> @deprecated Legacy API，基于 `browseDirectory()`。请使用 `getRoots()` + `browseTree()` + `enumerateTree()` 获取真实根。

高层目录遍历器（异步生成器），支持 `bfs/dfs`。

```javascript
for await (const node of fb.library.enumerateDirectories({ rootPath: '', strategy: 'bfs' })) {
  console.log(node.path, node.directories.length);
}
```

获取指定专辑的所有曲目。

```javascript
const tracks = await fb.library.getAlbumTracks('Abbey Road', 'The Beatles');
```

获取媒体库中指定字段的所有唯一值。

```javascript
const years = await fb.library.getFieldValues('date', 50);
```

使用 foobar2000 查询语法搜索媒体库。与 `search()` 类似但支持自定义排序。

```javascript
const r = await fb.library.query('%rating% GREATER 3', '%rating%', 100);
```

## 补全方法参考

### addToPlaylist(query, playlistIndex?)

签名：`fb.library.addToPlaylist(query: string, playlistIndex?: number): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| query | string | 是 | foobar2000 查询语法 |
| playlistIndex | number | 否 | 目标播放列表索引；省略时使用当前播放列表 |

返回值：添加匹配曲目的操作结果。

```javascript
await fb.library.addToPlaylist('artist HAS Beatles', 0);
```

### getArtistAlbums(artist, limit?)

签名：`fb.library.getArtistAlbums(artist: string, limit?: number): Promise<LibraryAlbumsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| artist | string | 是 | 艺术家名称 |
| limit | number | 否 | 最大返回数量 |

返回值：指定艺术家的专辑列表。

```javascript
const albums = await fb.library.getArtistAlbums('The Beatles', 50);
```

### getArtistTracks(artist, limit?)

签名：`fb.library.getArtistTracks(artist: string, limit?: number): Promise<LibraryTracksResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| artist | string | 是 | 艺术家名称 |
| limit | number | 否 | 最大返回数量 |

返回值：指定艺术家的曲目列表。

```javascript
const tracks = await fb.library.getArtistTracks('The Beatles', 100);
```

### getCacheStats()

签名：`fb.library.getCacheStats(): Promise<LibraryCacheStatsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：媒体库缓存统计信息。

```javascript
const cache = await fb.library.getCacheStats();
```

### getRandomTracks(count?)

签名：`fb.library.getRandomTracks(count?: number): Promise<LibraryTracksResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| count | number | 否 | 随机曲目数量 |

返回值：随机曲目列表。

```javascript
const random = await fb.library.getRandomTracks(25);
```

### getRecentlyAdded(limit?)

签名：`fb.library.getRecentlyAdded(limit?: number): Promise<LibraryTracksResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| limit | number | 否 | 最大返回数量 |

返回值：最近加入媒体库的曲目列表。

```javascript
const recent = await fb.library.getRecentlyAdded(50);
```

### invalidateCache()

签名：`fb.library.invalidateCache(): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：缓存失效操作结果。

```javascript
await fb.library.invalidateCache();
```

### isEnabled()

签名：`fb.library.isEnabled(): Promise<{ enabled: boolean }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：媒体库功能是否启用。

```javascript
const { enabled } = await fb.library.isEnabled();
```

### refresh()

签名：`fb.library.refresh(): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：刷新媒体库操作结果。

```javascript
await fb.library.refresh();
```

### rescan(paths?)

签名：`fb.library.rescan(paths?: string[]): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string[] | 否 | 要重新扫描的路径；省略时由主机执行默认 rescan |

返回值：重新扫描操作结果。

```javascript
await fb.library.rescan(['E:\\Music']);
```
