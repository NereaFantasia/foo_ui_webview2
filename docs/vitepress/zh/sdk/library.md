# fb.library 媒体库

> 枚举建议：全库曲目优先使用 `getCount()` + `getAll(start, count)` 分页遍历。 获取真实媒体库根目录请使用 `getRoots()`；浏览目录树使用 `browseTree()`；高层遍历使用 `enumerateTree()`。`browseDirectory()` / `enumerateDirectories()` 已弃用，仅保留为 legacy 目录投影视图。

## search(query, limit?)

搜索媒体库。返回 `{tracks: [...], total, offset, limit, hasMore}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| query | string | 搜索查询（支持 foobar2000 查询语法） |
| limit | number | 最大返回数量（默认 100） |
| options | `Omit<LibrarySearchParams, 'query' \| 'limit'>?` | 其余原生参数（`offset`、`fields`） |
| options.fields | `string[]?` | 要投影的曲目字段名，见[字段投影](#field-projection) |

> `tracks` 承载曲目行。`hasMore` 表示是否有更多结果。

```javascript
const results = await fb.library.search('artist HAS Beatles', 100);
console.log(`找到 ${results.total} 首`);
if (results.hasMore) console.log('还有更多结果');

// 字段投影：tracks 中每行只含请求的两个键
const narrow = await fb.library.search('artist HAS Beatles', 500, {
    fields: ['absolutePath', 'album'],
});
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

> 每位参与艺术家各成一个条目，一首多艺术家曲目会计进其中每一位。`trackCount` 是参与曲目数，各条目相加会大于曲目总数；`albumCount` 与 `totalDuration` 同样按每位艺术家重复计入。

```javascript
const artists = await fb.library.getArtists(100);
```

## getStats()

获取媒体库统计信息。返回 `{totalTracks, totalDuration, totalSize, ...}`。

> `totalArtists` 按参与艺术家计——一首多艺术家曲目会计进其中每一位——因此与 `getArtists()` 的条目数一致。

```javascript
const stats = await fb.library.getStats();
console.log(`${stats.totalTracks} 首，总时长 ${stats.totalDuration} 秒`);
```

## getGenres()

获取流派列表。返回 `{genres: [{name, trackCount}]}`。

> 多值 `genre` 的每个值各成一个条目，`trackCount` 是参与曲目数：一首标了多个流派的曲目会计进其中每一个。

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

获取所有曲目（支持分页），返回 `LibraryPagedTracksResponse`。当宿主把全库请求交给后台工作线程时，wrapper 会等待匹配的 `library:getAllResult` 事件，并仍解析为相同最终结构。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| start | number | 起始偏移 |
| count | number | 获取数量 |
| opts.timeout | number | 客户端超时毫秒数，默认 `60000`；设为 `0` 可禁用 |

```javascript
const r = await fb.library.getAll(0, 100);
console.log(`媒体库共 ${r.total} 首，本次返回 ${r.tracks.length} 首`);
```

> `tracks` 与 `items` 内容相同，`items` 为兼容别名。

> `artists` 是 `artist` 的原子值数组：`artists.join(', ')` 恰好等于 `artist`，多值标签只能从 `artists` 精确还原，从 `artist` 还原不了。该字段由媒体库侧返回曲目对象的 API 提供（`getAll()` / `query()` / `search()` / `getByPath()` 等）；`fb.playlist.getTracks` / `fb.player.getCurrentTrack` / `fb.queue.get`、artwork 载荷与事件载荷里的 track 对象不含此字段。

## enumerateTracks(options?)

高层分页枚举器（异步生成器），内部基于 `getCount()` + `getAll()`。

```javascript
for await (const page of fb.library.enumerateTracks({ pageSize: 500 })) {
  console.log(page.fetched, '/', page.total);
}
```

## refresh()

刷新媒体库。返回 `{success}`。

## getByPath(path)

通过文件路径在媒体库中搜索曲目。返回 `{found, path, title, artist, artists, album, duration, ...}`（字段在顶层，无嵌套）。

```javascript
const r = await fb.library.getByPath('E:\\Music\\song.flac');
if (r.found) console.log(r.title, r.artist);
```

> 扁平结果里 `artists` 与 `artist` 并列，含义与 `getAll()` 的行一致：`artists.join(', ')` 恰好等于 `artist`。本方法不返回 `albumArtist` / `composer`，因此只多 `artists` 这一个数组键。

## getRoots()

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

## browseTree(params)

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

## enumerateTree(options)

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

## browseDirectory(path, includeFiles?)

> Legacy API，不推荐作为根入口，请使用 `getRoots()` + `browseTree()` + `enumerateTree()`。

浏览媒体库目录投影视图。返回 legacy 目录字符串和文件列表。

> **已弃用**：`path === ''` 只表示“投影后的顶层目录视图”，不等于 foobar2000 已配置的媒体库根目录列表。

```javascript
const root = await fb.library.browseDirectory('', false);
```

## enumerateDirectories(options?)

> **已弃用**：Legacy API，基于 `browseDirectory()`。请使用 `getRoots()` + `browseTree()` + `enumerateTree()` 获取真实根。

高层目录遍历器（异步生成器），支持 `bfs/dfs`。

```javascript
for await (const node of fb.library.enumerateDirectories({ rootPath: '', strategy: 'bfs' })) {
  console.log(node.path, node.directories.length);
}
```

## getAlbumTracks(album, artist?)

获取指定专辑的所有曲目。

```javascript
const tracks = await fb.library.getAlbumTracks('Abbey Road', 'The Beatles');
```

## getFieldValues(field, limit?, separator?)

获取媒体库中指定字段的所有唯一值与曲目计数。`enumerateFieldValues(field, options?)` 是语义别名，接受 `{ limit?, separator? }`。

```javascript
const years = await fb.library.getFieldValues('date', 50);
const moreYears = await fb.library.enumerateFieldValues('date', { limit: 50 });
```

## query(query, sort?, limit?, fields?)

使用 foobar2000 查询语法搜索媒体库。与 `search()` 类似但支持自定义排序。排序发生在按 `limit` 截断之前，`total` 是截断前的全命中数。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| query | string | 是 | foobar2000 查询语法；空值返回 `success: false` |
| sort | string | 否 | TitleFormat 排序模式；省略则保持库序 |
| limit | number | 否 | 返回条数上限（宿主默认 100） |
| fields | `string[]` | 否 | 要投影的曲目字段名，见[字段投影](#field-projection) |

```javascript
const r = await fb.library.query('%rating% GREATER 3', '%rating%', 100);

// 大结果集只要路径
const paths = await fb.library.query('%codec% IS FLAC', undefined, 100000, [
    'absolutePath',
]);
```

## 字段投影 {#field-projection}

`query(..., fields)` 与 `search(query, limit, { fields })` 支持可选的曲目字段名列表。省略即现状行为：每行输出全部 20 键。

传了列表时，每行**恰好**包含请求的那几个键，不附带任何未请求字段——因此运行时 `TrackInfo` 是部分视图，而声明的类型仍为完整形状。元数据容器读取失败的损坏条目同样输出全部请求键，缺失值以类型默认值填充（空串、0）。响应信封不受影响。

**可用字段名**（精确匹配、大小写敏感）：

`index`、`title`、`artist`、`artists`、`album`、`albumArtist`、`genre`、`date`、`trackNumber`、`discNumber`、`duration`、`path`、`absolutePath`、`fileSize`、`bitrate`、`sampleRate`、`channels`、`codec`、`subsong`、`rating`

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

> `artists` 是 `artist` 的原子值数组：`artists.join(', ')` 恰好等于 `artist`。该字段由媒体库侧返回曲目对象的 API 提供（`getAll()` / `query()` / `search()` / `getByPath()` 等）；`fb.playlist.getTracks` / `fb.player.getCurrentTrack` / `fb.queue.get`、artwork 载荷与事件载荷里的 track 对象不含此字段。`artists` 与 `artist` 可单独投影其一，两者同源于一次取值；元数据容器读取失败的损坏条目上，被请求的 `artists` 返回 `[]`。

重复字段名会去重。`rating` 仅在被请求（或省略列表）时才计算。

**校验**为 fail-closed，且一律 resolve，不会 reject Promise。非数组（含显式 `null`）、空数组、含非字符串元素、或任何白名单外的名字都会得到：

```javascript
const bad = await fb.library.query('artist HAS Beatles', undefined, 100, [
    'absolutepath',
    'Rating',
]); // 大小写不符
// {
//   success: false,
//   error: 'fields contains unknown field names',
//   code: 'INVALID_PARAMS',
//   details: { unknownFields: ['absolutepath', 'Rating'] }
// }
```

`details.unknownFields` 只在"未知字段名"这一类出现；其余非法形状只返回 `success` / `error` / `code` 三键。

**使用建议**

| 场景 | 建议的列表 |
| --- | --- |
| 过滤数万命中，只要路径 | `['absolutePath']` |
| 搜索结果要在界面上展示（数百行、要全部列） | 省略该参数 |
| 过滤 + 按专辑统计 | `['absolutePath', 'album']` |

以 8 万行结果集实测：单字段投影使 wire 载荷从 45.1MB 降到 8.4MB，页面侧 `JSON.parse` 从 147ms 降到 37ms。

### 大结果集

**宿主主线程被占用的时长与响应体量成正比。** 行是在 worker 线程上构建的，成本在于把成品响应交给页面，实测每 MiB 15–27ms。唯一有效的方向是控制单次返回的字节数，做法有两个：投影（`fields`，从全字段降到 `['absolutePath']` 后载荷约缩到 1/4.6）与分页（`offset` / `limit`，占用只与该页行数成正比）。

**受支持的访问模式**：分页、投影任一或并用后，每次调用约 ≤2 万行，此时单次调用的主线程占用低于 100ms。超出这个量级请分页。

::: warning 32 位（x86）宿主须规避大结果集
32 位进程的用户态地址空间约 2–4 GB，而一次全库全字段响应在解析期间会同时以多种形态驻留。估算瞬时峰值：8 万行全字段 ≈178 MB、投影到 `['absolutePath']` ≈39 MB；165,306 行 ≈367 MB / ≈80 MB。这些叠加在宿主既有占用之上，而 `search()` 会再翻一倍。在 32 位宿主上，查询可能命中上万行时请投影或分页，**不要**对全库发全字段请求。64 位宿主没有这个限制，但主线程占用同样存在。

峰值是按解析模型推算的上界，不是实测工作集——请当作使用指引，而不是预算。
:::

## 其余方法

### addToPlaylist(paths, playlist?)

签名：`fb.library.addToPlaylist(paths: string[], playlist?: number): Promise<LibraryAddToPlaylistResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string[] | 是 | 要添加的文件路径数组 |
| playlist | number | 否 | 目标播放列表索引；省略时使用活动播放列表 |

```javascript
const { tracks } = await fb.library.search('artist HAS Beatles');
await fb.library.addToPlaylist(tracks.map(t => t.path), 0);
```

### getArtistAlbums(artist, limit?)

签名：`fb.library.getArtistAlbums(artist: string, limit?: number): Promise<LibraryAlbumsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| artist | string | 是 | 艺术家名称 |
| limit | number | 否 | 最大返回数量 |

```javascript
const albums = await fb.library.getArtistAlbums('The Beatles', 50);
```

### getArtistTracks(artist, limit?)

签名：`fb.library.getArtistTracks(artist: string, limit?: number): Promise<LibraryTracksResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| artist | string | 是 | 艺术家名称 |
| limit | number | 否 | 最大返回数量 |

```javascript
const tracks = await fb.library.getArtistTracks('The Beatles', 100);
```

### getCacheStats()

签名：`fb.library.getCacheStats(): Promise<LibraryCacheStatsResponse>`

无参数。

```javascript
const cache = await fb.library.getCacheStats();
```

### getRandomTracks(count?)

签名：`fb.library.getRandomTracks(count?: number): Promise<LibraryTracksResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| count | number | 否 | 随机曲目数量 |

```javascript
const random = await fb.library.getRandomTracks(25);
```

### getRecentlyAdded(limit?)

签名：`fb.library.getRecentlyAdded(limit?: number): Promise<LibraryTracksResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| limit | number | 否 | 最大返回数量 |

```javascript
const recent = await fb.library.getRecentlyAdded(50);
```

### invalidateCache()

签名：`fb.library.invalidateCache(): Promise<BaseResponse>`

无参数。

```javascript
await fb.library.invalidateCache();
```

### isEnabled()

签名：`fb.library.isEnabled(): Promise<{ enabled: boolean }>`

无参数。

```javascript
const { enabled } = await fb.library.isEnabled();
```

### refresh()

签名：`fb.library.refresh(): Promise<BaseResponse>`

无参数。

```javascript
await fb.library.refresh();
```

### rescan()

签名：`fb.library.rescan(): Promise<BaseResponse>`

无参数，触发宿主对媒体库监视目录的重新扫描。

```javascript
await fb.library.rescan();
```
