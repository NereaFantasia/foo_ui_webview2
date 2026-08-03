# Library API

媒体库浏览、搜索、缓存控制。共 25 个 API。

> ⚠️ 大部分 Library API 需要 foobar2000 媒体库已启用。未启用时返回空结果或 `error: "Library not enabled"`。 根目录枚举请优先使用 `library.getRoots`；`library.browseDirectory` 仅保留为 legacy 目录投影视图。

## 媒体库状态

### library.isEnabled

检查媒体库是否启用。

- **参数**: 无
**返回值**: `{"enabled":true,"success":true}`

```javascript
const { enabled } = await fb2k.invoke('library.isEnabled');
if (!enabled) console.warn('媒体库未启用');
```

### library.getStatus

获取媒体库状态信息。

- **参数**: 无

**返回值**:

```json
{
    "enabled": true,
    "initialized": true,
    "scanning": false,
    "itemCount": 15000,
    "count": 15000
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `enabled` | boolean | 媒体库是否启用 |
| `initialized` | boolean | 与 enabled 相同（兼容别名） |
| `scanning` | boolean | 是否正在扫描 |
| `itemCount` | number | 媒体库曲目总数 |
| `count` | number | 与 itemCount 相同（兼容别名） |

### library.getStats

获取媒体库统计信息。

- **参数**: 无

**返回值**:

```json
{
    "totalTracks": 15000,
    "totalAlbums": 500,
    "totalArtists": 200,
    "totalDuration": 3600000.0,
    "totalSize": 150000000000,
    "cacheValid": true,
    "lastModified": 1736064000000
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `totalTracks` | number | 曲目总数 |
| `totalAlbums` | number | 专辑总数（按 album + album artist 组合去重） |
| `totalArtists` | number | 艺术家总数 |
| `totalDuration` | number | 总时长（秒） |
| `totalSize` | number | 总大小（字节） |
| `cacheValid` | boolean | 缓存是否有效 |
| `lastModified` | number | 缓存最后修改时间戳 |

```javascript
const stats = await fb2k.invoke('library.getStats');
console.log(`${stats.totalTracks} 首曲目, ${stats.totalAlbums} 张专辑`);
```

### library.getCount

获取媒体库曲目总数。使用 `enum_items()` 遍历计数，避免分配 `metadb_handle_list` 内存。

- **参数**: 无
**返回值**: `{"count":0,"success":true}`

```javascript
const { count } = await fb2k.invoke('library.getCount');
```

## 浏览媒体库

### library.getAll

获取所有曲目（支持分页和缓存）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `asyncResult` | `boolean` | 否 | 可选；默认 false。 |
| `count` | `integer` | 否 | 可选；默认 100。 |
| `limit` | `integer` | 否 | 可选；默认 100。 |
| `offset` | `integer` | 否 | 可选；默认 0。 |
| `start` | `integer` | 否 | 可选；默认 0。 |
| `useCache` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"error":"...","fromCache":"...","items":[],"limit":"...","offset":"...","pending":"...","requestId":"...","total":"...","tracks":[]}`


> `tracks` 和 `items` 内容相同，`items` 为兼容别名。

每个曲目对象包含以下字段：

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `index` | number | 索引 |
| `title` | string | 标题 |
| `artist` | string | 艺术家 |
| `album` | string | 专辑 |
| `albumArtist` | string | 专辑艺术家 |
| `genre` | string | 流派 |
| `date` | string | 日期 |
| `trackNumber` | number | 曲目号 |
| `discNumber` | number | 碟片号 |
| `duration` | number | 时长（秒） |
| `path` | string | 路径 |
| `absolutePath` | string | 本地文件系统绝对路径 |
| `fileSize` | number | 文件大小 |
| `bitrate` | number | 比特率 (kbps) |
| `sampleRate` | number | 采样率 |
| `channels` | number | 声道数 |
| `codec` | string | 编解码器 |
| `subsong` | number | 子轨道索引 |
| `rating` | number | 评分 (0-5)，优先读取 foo_playcount，回退到文件标签 |

```javascript
// 分页获取
const page1 = await fb2k.invoke('library.getAll', { offset: 0, limit: 50 });
const page2 = await fb2k.invoke('library.getAll', { offset: 50, limit: 50 });

// 兼容旧参数名
const page = await fb2k.invoke('library.getAll', { start: 0, count: 50 });
```

### library.getByPath

通过文件路径在媒体库中查找曲目。使用 O(log n) handle 创建 + O(1) hash 查找，性能优异。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 可选。 |


**返回值**: `{"absolutePath":"...","album":"...","artist":"...","date":"...","duration":"...","found":true,"genre":"...","path":"...","success":true,"title":"...","trackNumber":"..."}`

**返回值（找到时）**:

```json
{
    "found": true,
    "path": "file://C:/Music/song.flac",
    "absolutePath": "C:\\Music\\song.flac",
    "title": "Song Title",
    "artist": "Artist",
    "album": "Album",
    "duration": 245.5,
    "trackNumber": "1",
    "genre": "Rock",
    "date": "2024"
}
```

**返回值（未找到时）**: `{ "found": false, "path": "..." }`

```javascript
const result = await fb2k.invoke('library.getByPath', { path: 'C:\\Music\\song.flac' });
if (result.found) {
    console.log(`找到: ${result.title} - ${result.artist}`);
}
```

### library.getAlbums

获取媒体库中的专辑列表。支持过滤、排序、分页、封面、缓存。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `coverMaxSize` | `integer` | 否 | 可选；默认 500。 |
| `includeCover` | `boolean` | 否 | 可选；默认 false。 |
| `includeTracks` | `boolean` | 否 | 可选；默认 false。 |
| `limit` | `integer` | 否 | 可选；默认 100。 |
| `offset` | `integer` | 否 | 可选；默认 0。 |
| `query` | `string` | 否 | 可选。 |
| `sort` | `string` | 否 | 可选；默认 name。 |
| `useCache` | `boolean` | 否 | 可选；默认 true。 |

**返回值**: `{"albums":[],"fromCache":"...","hasMore":true,"includeCover":"...","limit":"...","offset":"...","success":true,"total":"..."}`


> `coverDataUrl` 仅当 `includeCover: true` 且封面不超过 `coverMaxSize` KB 时返回。

::: tip 性能优化
使用 `includeCover: true` 可一次性获取所有封面，避免逐个调用 `artwork.getForTrack`。封面支持 JPEG/PNG/GIF/WebP 格式，自动检测 MIME 类型。
:::

```javascript
// 获取所有专辑（含封面）
const albums = await fb2k.invoke('library.getAlbums', {
    includeCover: true, coverMaxSize: 300
});

// 按年份排序
const recent = await fb2k.invoke('library.getAlbums', { sort: 'year', limit: 20 });

// 搜索专辑
const results = await fb2k.invoke('library.getAlbums', { query: 'Beatles' });
```

### library.getAlbumTracks

获取指定专辑的曲目列表。按曲目号排序返回。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `album` | `string` | 否 | 可选。 |
| `artist` | `string` | 否 | 可选。 |

**返回值**: `{"album":"...","artist":"...","items":[],"success":true,"total":"...","tracks":[]}`


> `items` 和 `tracks` 内容相同，`tracks` 为向后兼容的旧字段。

```javascript
const { items } = await fb2k.invoke('library.getAlbumTracks', {
    album: 'Abbey Road', artist: 'The Beatles'
});
```

### library.getArtists

获取媒体库中的艺术家列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `limit` | `integer` | 否 | 可选；默认 1000。 |
| `sort` | `string` | 否 | 可选；默认 name。 |

**返回值**: `{"count":0,"error":"...","items":[],"success":true}`


```javascript
// 按曲目数量排序
const artists = await fb2k.invoke('library.getArtists', { sort: 'trackCount' });
```

### library.getArtistTracks

获取指定艺术家的所有曲目。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `artist` | `string` | 否 | 可选。 |
| `limit` | `integer` | 否 | 可选；默认 500。 |

**返回值**: `{"artist":"...","count":0,"items":[],"success":true,"total":"...","tracks":[]}`


> `items` 和 `tracks` 内容相同。`count` 和 `total` 值相同，均为兼容字段。

```javascript
const { items } = await fb2k.invoke('library.getArtistTracks', { artist: 'The Beatles' });
```

### library.getArtistAlbums

获取指定艺术家的专辑列表。使用大小写不敏感的模糊匹配。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `artist` | `string` | 否 | 可选。 |
| `limit` | `integer` | 否 | 可选；默认 100。 |

**返回值**:

```json
{
    "success": true,
    "albums": [
        { "name": "Abbey Road", "artist": "The Beatles", "year": "1969", "trackCount": 17 }
    ]
}
```

```javascript
const { albums } = await fb2k.invoke('library.getArtistAlbums', { artist: 'Beatles' });
```

### library.getGenres

获取流派列表（含每个流派的曲目数）。

> 此 API 注册了两次，后注册的版本（含 `trackCount`）覆盖先注册的版本。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "genres": [
        { "name": "Rock", "trackCount": 5000 },
        { "name": "Jazz", "trackCount": 1200 }
    ]
}
```

```javascript
const { genres } = await fb2k.invoke('library.getGenres');
genres.sort((a, b) => b.trackCount - a.trackCount); // 按曲目数排序
```

### library.getRandomTracks

从媒体库中随机获取曲目。使用 Fisher-Yates 洗牌算法。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `count` | `integer` | 否 | 可选；默认 10。 |

**返回值**: `{"count":0,"success":true,"tracks":[]}`


```javascript
const { tracks } = await fb2k.invoke('library.getRandomTracks', { count: 20 });
```

### library.getRecentlyAdded

获取最近添加的曲目。支持两种排序模式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `limit` | `integer` | 否 | 可选；默认 50。 |
| `sortBy` | `string` | 否 | 可选；默认 added。 |

- `"added"` — 使用 `%added%` titleformat（需安装 foo_playcount 组件）
- `"modified"` — 使用文件修改时间（SDK 原生，无额外依赖）
- 若 foo_playcount 不可用，`"added"` 模式自动降级为 `"modified"`，并在返回中设 `fallback: true`

> 此 API 注册了两次，后注册的版本（支持 `sortBy` 参数）覆盖先注册的版本。

**返回值**:

```json
{
    "success": true,
    "tracks": [ { "title": "...", "added": "2026-02-09 12:00:00", ... } ],
    "total": 1000,
    "limit": 50,
    "sortBy": "added",
    "fallback": false
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| tracks[].added | string | sortBy="added" 时附加，foo_playcount 返回的时间字符串 |
| tracks[].modified | number | sortBy="modified" 时附加，Unix 时间戳（秒） |
| fallback | boolean | true 表示 foo_playcount 不可用，已降级到 "modified" |

```javascript
// 优先使用 foo_playcount 的 %added% 时间
const recent = await fb2k.invoke('library.getRecentlyAdded', { limit: 20 });
if (recent.fallback) console.warn('foo_playcount 未安装，使用文件修改时间');

// 强制使用文件修改时间（无需 foo_playcount）
const recent2 = await fb2k.invoke('library.getRecentlyAdded', { limit: 20, sortBy: 'modified' });
```

### library.getRoots

获取真实媒体库根目录列表。使用 `library_manager::get_relative_path()` 按路径段比较推导根目录，不依赖字符串前缀匹配。

- **参数**: 无

**返回值**: `{"fromCache":"..."}`


| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `roots` | array | 根目录列表 |
| `id` | string | 稳定标识，当前为规范化 absolutePath |
| `roots[].displayName` | string | 目录名；同名冲突时回退完整路径 |
| `roots[].trackCount` | number | 该根下条目数（媒体库 item 计数） |
| `total` | number | 总数 |
| `indexedTracks` | number | 成功索引条目数 |
| `skippedTracks` | number | 跳过条目数 |
| `fromCache` | boolean | 是否来自缓存 |

> 仅可解析为稳定本地绝对路径的条目会进入 `roots`。`http://`、`file-relative://`、`unpack://`、`archive://` 等协议型条目会计入 `skippedTracks`。 首次调用同步构建索引，后续走缓存。媒体库变化或调用 `library.invalidateCache` 时自动失效。

```javascript
const { roots, total } = await fb2k.invoke('library.getRoots');
for (const root of roots) {
    console.log(`${root.displayName}: ${root.trackCount} 曲目`);
}
```

### library.browseTree

按 `rootId` + `pathId` 浏览 typed 目录树。先调用 `library.getRoots` 获取可用的 `rootId`，再用本 API 逐层展开目录。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeFiles` | `boolean` | 否 | 可选；默认 false。 |
| `pathId` | `string` | 否 | 可选。 |
| `recursiveFiles` | `boolean` | 否 | 可选；默认 false。 |
| `rootId` | `string` | 是 | 必填。 |

**返回值**: `{"files":[]}`


| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `root` | object | 根节点信息
| `pathId` | string | 请求的 pathId |
| `absolutePath` | string | 绝对路径
| `files` | array | 文件列表
| `fromCache` | boolean | 是否来自缓存 |

**目录节点字段**:

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `id` | string | 标识
| `rootId` | string | 所属根 ID |
| `pathId` | string | 否 默认值：空字符串。 parentPathId string 父目录 pathId |
| `name` | string | 显示名
| `displayName` | string | 显示名称 |
| `rawPath` | string | 原始路径
| `absolutePath` | string | 规范化本地绝对路径 |
| `relativePath` | string | 相对路径
| `depth` | number | 目录深度 |
| `trackCount` | number | 曲目数
| `childDirectoryCount` | number | 直接子目录数 |
| `hasChildren` | boolean | 是否有子节点
| `---` | --- |  |
| `rootId 缺失` | "rootId is required" |  |
| `rootId 不存在` | "json rootId" |  |
| `pathId 不存在` | "Path not found" |  |

```javascript
// 先获取根列表
const { roots } = await fb2k.invoke('library.getRoots');
// 浏览第一个根的顶层目录
const tree = await fb2k.invoke('library.browseTree', { rootId: roots[0].id });
// 展开子目录
const sub = await fb2k.invoke('library.browseTree', {
    rootId: roots[0].id,
    pathId: tree.directories[0].pathId,
    includeFiles: true
});
```

> ⚠️ **Legacy API**。不推荐作为媒体库根入口，请使用 `library.getRoots` + `library.browseTree`。

### library.browseDirectory

按 raw metadb path 前缀投影目录视图。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeFiles` | `boolean` | 否 | 可选；默认 true。 |
| `path` | `string` | 否 | 可选。 |

**返回值**:

```json
{
    "success": true,
    "directories": [ "file://C:/Music/Rock", "file://C:/Music/Jazz" ],
    "files": [ { "index": 0, "title": "...", ... } ],
    "items": [ ... ]
}
```

> `items` 是 `directories` 的兼容别名。`path === ''` 只返回顶层投影视图，不等于真实媒体库根列表。

```javascript
const root = await fb2k.invoke('library.browseDirectory', { includeFiles: false });
```

## 搜索

### library.search

在媒体库中搜索曲目。使用 foobar2000 原生 `search_filter_v2` 查询语法，支持分页。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `limit` | `integer` | 否 | 可选；默认 100。 |
| `offset` | `integer` | 否 | 可选；默认 0。 |
| `query` | `string` | 否 | 可选。 |

**返回值**: `{"error":"...","hasMore":true,"items":[],"limit":"...","offset":"...","success":true,"total":"...","tracks":[]}`


> `items` 和 `tracks` 内容相同，`tracks` 为向后兼容的旧字段（deprecated）。

**搜索语法示例**:

```javascript
// 简单关键词搜索
await fb2k.invoke('library.search', { query: 'love' });

// foobar2000 查询语法
await fb2k.invoke('library.search', { query: 'artist HAS beatles' });
await fb2k.invoke('library.search', { query: 'artist HAS beatles AND year GREATER 1968' });

// 分页
const page2 = await fb2k.invoke('library.search', { query: 'rock', offset: 100, limit: 50 });
```

### library.query

使用 foobar2000 查询语法搜索媒体库，支持 TitleFormat 排序。与 `library.search` 的区别：`query` 支持通过 `sort` 参数指定 TitleFormat 排序模式。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `limit` | `integer` | 否 | 可选；默认 100。 |
| `query` | `string` | 否 | 可选。 |
| `sort` | `string` | 否 | 可选。 |

**返回值**:

```json
{
    "success": true,
    "tracks": [ { "index": 0, "title": "...", ... } ],
    "total": 500
}
```

```javascript
// 查询评分大于 3 的曲目，按添加时间排序
const result = await fb2k.invoke('library.query', {
    query: '%rating% GREATER 3',
    sort: '%added%',
    limit: 50
});

// 查询 FLAC 格式的曲目
const flacs = await fb2k.invoke('library.query', {
    query: '%codec% IS FLAC',
    limit: 200
});
```

## 媒体库操作

### library.addToPlaylist

将媒体库曲目添加到播放列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `array` | 否 | 可选；默认 []。 |
| `playlist` | `integer` | 否 | 可选；默认 SIZE_MAX。 |

**返回值**: `{ "success": true, "added": 5 }`

```javascript
const { items } = await fb2k.invoke('library.search', { query: 'artist HAS Beatles' });
const paths = items.map(t => t.path);
await fb2k.invoke('library.addToPlaylist', { paths, playlist: 0 });
```

### library.rescan

重新扫描媒体库。调用 foobar2000 内部的 `library_manager::rescan()`。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('library.rescan');
```

### library.refresh

刷新媒体库。功能与 `library.rescan` 完全相同，为兼容别名。

- **参数**: 无
- **返回值**: `{ "success": true }`

## 缓存控制

> 自动失效: 媒体库变化时缓存自动失效（通过 `library_callback_v2` 监听）。

### library.invalidateCache

手动清除媒体库缓存。

- **参数**: 无
- **返回值**: `{ "success": true, "timestamp": 1736064000000 }`

```javascript
await fb2k.invoke('library.invalidateCache');
const albums = await fb2k.invoke('library.getAlbums', { useCache: true });
```

### library.getCacheStats

获取缓存统计信息。包含目录树索引统计字段。

- **参数**: 无

**返回值**: `{"albumsCacheEntries":"...","artistsCached":"...","cacheHits":"...","cacheMisses":"...","coverCacheBytes":"...","coverCacheMB":"...","coversCached":"...","genresCached":"...","lastModified":"...","rootsCached":"...","statsCached":"...","tracksCached":"...","treeIndexValid":"...","treeIndexedTracks":"...","treeLastBuilt":"...","treeSkippedTracks":"...","valid":"..."}`


| 字段 | 类型 | 描述 |
| --- | --- | --- |
| `valid` | boolean | 是否有效
| `lastModified` | number | 缓存最后修改时间戳（毫秒） |
| `albumsCacheEntries` | json | albumsCacheEntries |
| `tracksCached` | boolean | 曲目缓存是否存在 |
| `coversCached` | json | coversCached |
| `coverCacheBytes` | number | 封面缓存大小（字节） |
| `coverCacheMB` | json | coverCacheMB |
| `cacheHits` | number | 缓存命中次数 |
| `cacheMisses` | json | cacheMisses |
| `treeIndexValid` | boolean | 目录树索引是否有效 |
| `rootsCached` | json | rootsCached |
| `treeIndexedTracks` | number | 成功索引条目数 |
| `treeSkippedTracks` | json | treeSkippedTracks |
| `treeLastBuilt` | number | 索引上次构建时间戳 |

## 事件

| 事件 | 描述 | 数据 |
| --- | --- | --- |
| library:itemsAdded | 媒体库新增曲目 | { count, timestamp } |
| library:itemsRemoved | 媒体库删除曲目 | { count, timestamp } |
| library:itemsModified | 媒体库曲目元数据变化 | { count, timestamp } |
| library:initialized | 媒体库初始化完成 | { timestamp } |

```javascript
fb2k.on('library:itemsAdded', async (data) => {
    console.log(`新增 ${data.count} 首曲目`);
    // 缓存已自动失效，重新加载数据
    await fb2k.invoke('library.getStats');
});
```

## 其他公开 API


### library.getFieldValues


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `field` | `string` | 是 | 要统计的元数据字段名，缺失时返回 `field is required`。 |
| `limit` | `integer` | 否 | 可选；默认 5000。 |
| `separator` | `string` | 否 | 可选。用于把单个字段值再拆分为多个值，例如 `;`。 |

**返回值**: `{"error":"...","field":"...","success":true,"total":"...","values":"..."}`

```js
// 最小调用：只传必填的 field
const { values } = await fb2k.invoke('library.getFieldValues', { field: 'genre' });

// 多值字段可用 separator 拆分，并限制返回条数
const { values: artists } = await fb2k.invoke('library.getFieldValues', {
    field: 'artist',
    separator: ';',
    limit: 50
});
```

## 使用说明

- `library.getAll` 可使用 `start` 或 `offset`，也可使用 `count` 或 `limit`；同一组同时提供时，`start` 和 `count` 优先。四个默认值依次为 `0`、`0`、`100`、`100`，`useCache` 默认是 `true`。
- `asyncResult` 默认是 `false`。完整媒体库请求启用该项后，立即返回 `{ pending, requestId }`；完成后的 `{ requestId, tracks, items, total, offset, limit, fromCache }` 会通过 `library:getAllResult` 发送给发起调用的 WebView。
- `library.getRoots` 和 `library.browseTree` 是类型化的媒体库导航 API。`library.browseDirectory` 是旧的路径前缀投影视图，不代表真实根目录集合。
- `library.getAlbums` 仅在 `includeCover` 启用且存在封面时添加 `coverDataUrl`。该字段是 `data:image/...` URL，而不是 `fb2k://` URL。
- `library.search` 和 `library.query` 使用 foobar2000 查询语法，底层实现使用 `search_filter_v2`；客户端不应自行解析语法，并应处理无效表达式的 handler 错误。
- `library.getStatus` 和 `library.getCount` 通过 `enum_items` 枚举，不会返回 `metadb_handle_list`。`library_callback_v2` 回调会先使缓存失效，再广播下列事件。

## 媒体库事件 Contract

四个事件均广播到每个 WebView。

| 事件 | Payload |
| --- | --- |
| `library:itemsAdded` | `{ count, timestamp }` |
| `library:itemsRemoved` | `{ count, timestamp }` |
| `library:itemsModified` | `{ count, timestamp }` |
| `library:initialized` | `{ timestamp }` |
