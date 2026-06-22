# Library API 

媒体库浏览、搜索、缓存控制。共 25 个 API。

> ⚠️ 大部分 Library API 需要 foobar2000 媒体库已启用。未启用时返回空结果或 `error: "Library not enabled"`。 根目录枚举请优先使用 `library.getRoots`；`library.browseDirectory` 仅保留为 legacy 目录投影视图。

## 媒体库状态 

### library.isEnabled 

检查媒体库是否启用。

- **参数**: 无
- **返回值**: `{ "enabled": true }`

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
| enabled | boolean | 媒体库是否启用 |
| initialized | boolean | 与 enabled 相同（兼容别名） |
| scanning | boolean | 始终为 false（SDK 未暴露扫描状态） |
| itemCount | number | 媒体库曲目总数 |
| count | number | 与 itemCount 相同（兼容别名） |

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
| totalTracks | number | 曲目总数 |
| totalAlbums | number | 专辑总数（按 album + album artist 组合去重） |
| totalArtists | number | 艺术家总数 |
| totalDuration | number | 总时长（秒） |
| totalSize | number | 总文件大小（字节） |
| cacheValid | boolean | 缓存是否有效 |
| lastModified | number | 缓存最后修改时间戳 |

```javascript
const stats = await fb2k.invoke('library.getStats');
console.log(`${stats.totalTracks} 首曲目, ${stats.totalAlbums} 张专辑`);
```

### library.getCount 

获取媒体库曲目总数。使用 `enum_items()` 遍历计数，避免分配 `metadb_handle_list` 内存。

- **参数**: 无
- **返回值**: `{ "count": 15000 }`

```javascript
const { count } = await fb2k.invoke('library.getCount');
```

## 浏览媒体库 

### library.getAll 

获取所有曲目（支持分页和缓存）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| offset | number | ✗ | 起始偏移（默认 0），兼容别名: start |
| limit | number | ✗ | 最大返回数量（默认 100），兼容别名: count |
| useCache | boolean | ✗ | 是否使用缓存（默认 true） |

**返回值**:

```json
{
    "tracks": [ { "index": 0, "title": "...", "artist": "...", ... } ],
    "items": [ ... ],
    "total": 15000,
    "offset": 0,
    "limit": 100,
    "fromCache": false
}
```

> `tracks` 和 `items` 内容相同，`items` 为兼容别名。

每个曲目对象包含以下字段：

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| index | number | 曲目在媒体库中的索引 |
| title | string | 标题 |
| artist | string | 艺术家 |
| album | string | 专辑 |
| albumArtist | string | 专辑艺术家 |
| genre | string | 流派 |
| date | string | 日期 |
| trackNumber | number | 曲目号 |
| discNumber | number | 碟号 |
| duration | number | 时长（秒） |
| path | string | foobar2000 内部路径 |
| absolutePath | string | 本地文件系统绝对路径 |
| fileSize | number | 文件大小（字节） |
| bitrate | number | 比特率 (kbps) |
| sampleRate | number | 采样率 (Hz) |
| channels | number | 声道数 |
| codec | string | 编码格式 |
| subsong | number | 子轨道索引 |
| rating | number | 评分 (0-5)，优先读取 foo_playcount，回退到文件标签 |

```javascript
// 分页获取
const page1 = await fb2k.invoke('library.getAll', { offset: 0, limit: 50 });
const page2 = await fb2k.invoke('library.getAll', { offset: 50, limit: 50 });

// 兼容旧参数名
const page = await fb2k.invoke('library.getAll', { start: 0, count: 50 });
```

### library.getByPath 

通过文件路径在媒体库中查找曲目。使用 O(log n) handle 创建 + O(1) hash 查找，性能优异。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✓ | 文件路径 |

**返回值（找到时）**:

```json
{
    "found": true,
    "path": "file://C:/Music/song.flac",
    "absolutePath": "C:\\\\Music\\\\song.flac",
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
const result = await fb2k.invoke('library.getByPath', { path: 'C:\\\\Music\\\\song.flac' });
if (result.found) {
    console.log(`找到: ${result.title} - ${result.artist}`);
}
```

### library.getAlbums 

获取媒体库中的专辑列表。支持过滤、排序、分页、封面、缓存。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ✗ | 过滤关键词（按专辑名或艺术家名模糊匹配） |
| sort | string | ✗ | 排序：name（默认）、artist、year、trackCount |
| offset | number | ✗ | 起始偏移（默认 0） |
| limit | number | ✗ | 最大返回数量（默认 100） |
| includeTracks | boolean | ✗ | 是否包含曲目列表（默认 false） |
| includeCover | boolean | ✗ | 是否返回封面 dataUrl（默认 false） |
| coverMaxSize | number | ✗ | 封面最大大小 KB（默认 500） |
| useCache | boolean | ✗ | 是否使用缓存（默认 true） |

**返回值**:

```json
{
    "total": 500,
    "offset": 0,
    "limit": 100,
    "hasMore": true,
    "includeCover": false,
    "fromCache": false,
    "albums": [
        {
            "name": "Abbey Road",
            "artist": "The Beatles",
            "albumArtist": "The Beatles",
            "trackCount": 17,
            "discCount": 1,
            "duration": 2835.0,
            "year": "1969",
            "genre": "Rock",
            "label": "Apple Records",
            "firstTrackPath": "file://C:/Music/Beatles/01.flac",
            "firstTrackAbsolutePath": "C:\\\\Music\\\\Beatles\\\\01.flac",
            "coverDataUrl": "data:image/jpeg;base64,..."
        }
    ]
}
```

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| album | string | ✓ | 专辑名称 |
| artist | string | ✗ | 专辑艺术家（用于区分同名专辑） |

**返回值**:

```json
{
    "items": [ { "index": 0, "title": "Come Together", ... } ],
    "tracks": [ ... ],
    "total": 17,
    "album": "Abbey Road",
    "artist": "The Beatles"
}
```

> `items` 和 `tracks` 内容相同，`tracks` 为向后兼容的旧字段。

```javascript
const { items } = await fb2k.invoke('library.getAlbumTracks', {
    album: 'Abbey Road', artist: 'The Beatles'
});
```

### library.getArtists 

获取媒体库中的艺术家列表。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| sort | string | ✗ | 排序：name（默认）、trackCount、albumCount |
| limit | number | ✗ | 最大返回数量（默认 1000） |

**返回值**:

```json
{
    "success": true,
    "items": [
        {
            "name": "The Beatles",
            "albumCount": 12,
            "trackCount": 213,
            "totalDuration": 42000.0
        }
    ],
    "count": 200
}
```

```javascript
// 按曲目数量排序
const artists = await fb2k.invoke('library.getArtists', { sort: 'trackCount' });
```

### library.getArtistTracks 

获取指定艺术家的所有曲目。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| artist | string | ✓ | 艺术家名称（精确匹配） |
| limit | number | ✗ | 最大返回数量（默认 500） |

**返回值**:

```json
{
    "items": [ { "index": 0, "title": "...", ... } ],
    "tracks": [ ... ],
    "total": 213,
    "count": 213,
    "artist": "The Beatles"
}
```

> `items` 和 `tracks` 内容相同。`count` 和 `total` 值相同，均为兼容字段。

```javascript
const { items } = await fb2k.invoke('library.getArtistTracks', { artist: 'The Beatles' });
```

### library.getArtistAlbums 

获取指定艺术家的专辑列表。使用大小写不敏感的模糊匹配。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| artist | string | ✓ | 艺术家名称 |
| limit | number | ✗ | 最大返回数量（默认 100） |

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

 通用字段值聚合 API，获取任意元数据字段的唯一值列表及其曲目数。是 `library.getGenres` 的泛化版本，支持多值字段和分隔符拆分。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| field | string | ✓ | 元数据字段名（如 "genre", "artist", "mood", "style" 等） |
| separator | string | ✗ | 值分隔符（如 ";" 或 ","），为空时整个值作为一个标签 |
| limit | number | ✗ | 最大返回数量（默认 5000） |

**返回值**:

```json
{
    "success": true,
    "values": [
        { "name": "Rock", "trackCount": 5000 },
        { "name": "Pop", "trackCount": 3200 }
    ],
    "total": 150,
    "field": "genre"
}
```

> 结果按 `trackCount` 降序排列。`total` 为截断前的唯一值总数。

```javascript
// 获取所有流派（等价于 getGenres）
const { values } = await fb2k.invoke('library.getFieldValues', { field: 'genre' });

// 获取心情标签，按分号拆分多值
const moods = await fb2k.invoke('library.getFieldValues', {
    field: 'mood', separator: ';'
});

// 获取作曲家列表
const composers = await fb2k.invoke('library.getFieldValues', { field: 'composer' });
```

### library.getRandomTracks 

从媒体库中随机获取曲目。使用 Fisher-Yates 洗牌算法。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| count | number | ✗ | 曲目数量（默认 10） |

**返回值**:

```json
{
    "tracks": [ { "index": 0, "title": "...", ... } ],
    "count": 10
}
```

```javascript
const { tracks } = await fb2k.invoke('library.getRandomTracks', { count: 20 });
```

### library.getRecentlyAdded 

获取最近添加的曲目。支持两种排序模式。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| limit | number | ✗ | 返回数量（默认 50） |
| sortBy | string | ✗ | "added"（默认）或 "modified" |

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

获取真实媒体库根目录列表。使用 `library_manager::get_relative_path()` 按路径段比较推导根目录，不依赖字符串前缀匹配。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "enabled": true,
    "roots": [
        {
            "id": "D:\\\\Music",
            "displayName": "Music",
            "rawPath": "D:\\\\Music",
            "absolutePath": "D:\\\\Music",
            "trackCount": 5000
        }
    ],
    "total": 1,
    "indexedTracks": 5000,
    "skippedTracks": 3,
    "fromCache": false
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| roots | LibraryRootInfo[] | 根目录列表，按 displayName 排序 |
| roots[].id | string | 稳定标识，当前为规范化 absolutePath |
| roots[].displayName | string | 目录名；同名冲突时回退完整路径 |
| roots[].trackCount | number | 该根下条目数（媒体库 item 计数） |
| total | number | 根目录数量 |
| indexedTracks | number | 成功索引条目数 |
| skippedTracks | number | 跳过条目数（协议型/不可推导） |
| fromCache | boolean | 是否来自缓存 |

> 仅可解析为稳定本地绝对路径的条目会进入 `roots`。`http://`、`file-relative://`、`unpack://`、`archive://` 等协议型条目会计入 `skippedTracks`。 首次调用同步构建索引，后续走缓存。媒体库变化或调用 `library.invalidateCache` 时自动失效。

```javascript
const { roots, total } = await fb2k.invoke('library.getRoots');
for (const root of roots) {
    console.log(`${root.displayName}: ${root.trackCount} 曲目`);
}
```

按 `rootId` + `pathId` 浏览 typed 目录树。先调用 `library.getRoots` 获取可用的 `rootId`，再用本 API 逐层展开目录。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| rootId | string | ✓ | 媒体库根 ID（来自 getRoots().roots[].id） |
| pathId | string | ✗ | 根下相对路径，/ 分隔（缺省为 "" 表示根） |
| includeFiles | boolean | ✗ | 是否包含文件列表（默认 false） |
| recursiveFiles | boolean | ✗ | 是否递归包含后代目录文件（默认 false，仅 includeFiles=true 时生效） |

**返回值**:

```json
{
    "success": true,
    "root": {
        "id": "D:\\\\Music",
        "displayName": "Music",
        "rawPath": "D:\\\\Music",
        "absolutePath": "D:\\\\Music",
        "trackCount": 12456
    },
    "pathId": "Anime/OST",
    "absolutePath": "D:\\\\Music\\\\Anime\\\\OST",
    "directories": [
        {
            "id": "D:\\\\Music::Anime/OST/Album A",
            "rootId": "D:\\\\Music",
            "pathId": "Anime/OST/Album A",
            "parentPathId": "Anime/OST",
            "name": "Album A",
            "displayName": "Album A",
            "rawPath": "D:\\\\Music\\\\Anime\\\\OST\\\\Album A",
            "absolutePath": "D:\\\\Music\\\\Anime\\\\OST\\\\Album A",
            "relativePath": "Anime/OST/Album A",
            "depth": 3,
            "trackCount": 12,
            "childDirectoryCount": 0,
            "hasChildren": false
        }
    ],
    "files": [],
    "fromCache": true
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| root | LibraryRootInfo | 所属根信息 |
| pathId | string | 请求的 pathId |
| absolutePath | string | 当前目录的绝对路径 |
| directories | LibraryDirectoryNodeInfo[] | 直接子目录列表，按 displayName 排序 |
| files | TrackInfo[] | 文件列表（includeFiles=false 时为空数组） |
| fromCache | boolean | 是否来自缓存 |

**目录节点字段**:

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| id | string | 唯一标识: "rootId::pathId" |
| rootId | string | 所属根 ID |
| pathId | string | 根下相对路径 |
| parentPathId | string | 父目录 pathId |
| name | string | 当前目录名 |
| displayName | string | 显示名称 |
| rawPath | string | 原始路径（当前与 absolutePath 相同） |
| absolutePath | string | 规范化本地绝对路径 |
| relativePath | string | 相对路径（同 pathId） |
| depth | number | 目录深度 |
| trackCount | number | 聚合条目数（含子目录） |
| childDirectoryCount | number | 直接子目录数 |
| hasChildren | boolean | 是否有子目录 |

**错误返回**:

| 条件 | 错误信息 |
| --- | --- |
| rootId 缺失 | "rootId is required" |
| rootId 不存在 | "Unknown rootId" |
| pathId 不存在 | "Path not found" |

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

按 raw metadb path 前缀投影目录视图。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ✗ | 目录路径前缀（为空时返回顶级目录） |
| includeFiles | boolean | ✗ | 是否包含文件列表（默认 true） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ✓ | 搜索关键词或查询表达式 |
| offset | number | ✗ | 起始偏移（默认 0） |
| limit | number | ✗ | 最大结果数量（默认 100） |

**返回值**:

```json
{
    "items": [ { "index": 0, "title": "...", ... } ],
    "tracks": [ ... ],
    "total": 150,
    "offset": 0,
    "limit": 100,
    "hasMore": true
}
```

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| query | string | ✓ | foobar2000 查询语法表达式 |
| limit | number | ✗ | 最大返回数量（默认 100） |
| sort | string | ✗ | TitleFormat 排序模式（如 %album artist%） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ✓ | 曲目路径数组（foobar2000 内部路径格式） |
| playlist | number | ✗ | 目标播放列表索引（默认当前活动播放列表） |

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

**返回值**:

```json
{
    "valid": true,
    "lastModified": 1736064000000,
    "albumsCacheEntries": 3,
    "tracksCached": true,
    "coversCached": 250,
    "coverCacheBytes": 52428800,
    "coverCacheMB": 50.0,
    "cacheHits": 42,
    "cacheMisses": 3,
    "treeIndexValid": true,
    "rootsCached": 2,
    "treeIndexedTracks": 15000,
    "treeSkippedTracks": 3,
    "treeLastBuilt": 1736064000000
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| valid | boolean | 缓存是否有效 |
| lastModified | number | 缓存最后修改时间戳（毫秒） |
| albumsCacheEntries | number | 专辑缓存条目数 |
| tracksCached | boolean | 曲目缓存是否存在 |
| coversCached | number | 缓存的封面数量 |
| coverCacheBytes | number | 封面缓存大小（字节） |
| coverCacheMB | number | 封面缓存大小（MB） |
| cacheHits | number | 缓存命中次数 |
| cacheMisses | number | 缓存未命中次数 |
| treeIndexValid | boolean | 目录树索引是否有效 |
| rootsCached | number | 缓存的根目录数 |
| treeIndexedTracks | number | 成功索引条目数 |
| treeSkippedTracks | number | 跳过条目数 |
| treeLastBuilt | number | 索引上次构建时间戳 |

## 事件 

| 事件 | 描述 | 数据 |
| --- | --- | --- |
| library:itemsAdded | 媒体库新增曲目 | { count, timestamp } |
| library:itemsRemoved | 媒体库删除曲目 | { count, timestamp } |
| library:itemsModified | 媒体库曲目元数据变化 | { count, timestamp } |
| library:initialized | 媒体库初始化完成 | { timestamp } |

```javascript
fb2k.on('library:itemsAdded', (data) => {
    console.log(`新增 ${data.count} 首曲目`);
    // 缓存已自动失效，重新加载数据
    await fb2k.invoke('library.getStats');
});
```
