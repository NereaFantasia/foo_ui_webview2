# Library API

检索与浏览媒体库曲目、专辑、艺术家，管理根目录索引、增量同步与全库缓存。

> ⚠️ 绝大多数 Library API 依赖 foobar2000 媒体库处于启用状态；未配置媒体库目录时返回空结果或报错 `error: "Library not enabled"`。枚举物理根目录推荐使用 `library.getRoots`；`library.browseDirectory` 仅保留为旧版路径前缀投影视图。

## 媒体库状态

### library.isEnabled

<!-- api-schema:begin library.isEnabled -->
查询 foobar2000 媒体库是否已启用（即是否配置了媒体库文件夹）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 媒体库是否已配置文件夹并启用 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('library.isEnabled');
if (res.success === false) throw new Error(res.error);
const { enabled } = res;
if (!enabled) console.warn('媒体库未启用');
```

### library.getStatus

<!-- api-schema:begin library.getStatus -->
查询媒体库启用状态及当前曲目数；查询结果缓存至媒体库变更，媒体库未启用时不缓存

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 媒体库是否已配置文件夹并启用 |
| `initialized` | `boolean` | 启动初始化是否已完成媒体库载入；完成前收到的 `library:itemsAdded` 表示存量载入曲目而非新增 |
| `scanning` | `boolean` | 是否正在扫描媒体库；当前实现固定为 `false`（宿主不报告扫描状态） |
| `itemCount` | `integer` | 媒体库曲目总数；未启用时为 `0` |
| `count` | `integer` | 与 `itemCount` 完全一致的兼容别名 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### library.getStats

<!-- api-schema:begin library.getStats -->
获取全媒体库汇总统计计数；每次调用均实时遍历全库，媒体库未启用时各项计数均为 `0`

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `totalTracks` | `integer` | 媒体库曲目总数 |
| `totalAlbums` | `integer` | 专辑总数（按专辑名加专辑艺术家归组，口径与 `library.getAlbums` 一致） |
| `totalArtists` | `integer` | 参与艺术家总数（多值 `artist` 逐项计数，与 `library.getArtists` 条目数一致） |
| `totalDuration` | `number` | 全库曲目总时长（秒） |
| `totalSize` | `integer` | 全库媒体文件总大小（字节） |
| `cacheValid` | `boolean` | 宿主是否持有有效的媒体库内存缓存 |
| `lastModified` | `integer` | 上次清理或丢弃缓存的时间戳（自 Unix 纪元起的毫秒数） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `totalArtists` 按参与艺术家计——一首多艺术家曲目会计进其中每一位——因此与 `library.getArtists` 的条目数一致。

```javascript
const stats = await fb2k.invoke('library.getStats');
if (stats.success === false) throw new Error(stats.error);
console.log(`${stats.totalTracks} 首曲目, ${stats.totalAlbums} 张专辑`);
```

### library.getCount

<!-- api-schema:begin library.getCount -->
获取媒体库中的曲目总数

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 媒体库曲目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('library.getCount');
if (res.success === false) throw new Error(res.error);
const { count } = res;
```

## 浏览媒体库

### library.getAll

<!-- api-schema:begin library.getAll -->
按媒体库原生顺序分页获取全库曲目；覆盖全库的起始分页请求（`offset: 0` 且容纳全部曲目）将缓存至媒体库变更，后续指定 `useCache: true` 的调用直接复用缓存；传入 `asyncResult: true` 且未命中缓存时在后台线程异步构建，立即返回 `{ pending: true, requestId }` 并通过 `library:getAllResult` 事件将该页数据推送至调用窗口

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `offset` | `integer` | 否 | 跳过的曲目偏移量不小于 `0`。默认 `0`。 |
| `limit` | `integer` | 否 | 最多返回的曲目数量上限不小于 `0`。默认 `100`。 |
| `useCache` | `boolean` | 否 | 从 `offset: 0` 开始时是否复用全库缓存；无论此项为何值，覆盖全库的起始请求均会被宿主缓存默认 `true`。 |
| `asyncResult` | `boolean` | 否 | 未命中缓存时是否在后台线程异步构建并通过 `library:getAllResult` 事件投递结果；仅在 `useCache: true` 时生效默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pending` | `boolean` | 结果是否将在后台线程异步构建并通过 `library:getAllResult` 事件投递；为 `true` 时不返回本页曲目字段 |
| `requestId` | `string` | 异步结果事件所携带的匹配请求标识符；与 `pending: true` 配套返回 |
| `tracks` | `LibraryTrack[]` | 当前分页曲目列表，按媒体库原生顺序排列；`index` 为在媒体库中的绝对位置 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `items` | `LibraryTrack[]` | 与 `tracks` 完全一致的兼容曲目列表 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `total` | `integer` | 媒体库曲目总数 |
| `offset` | `integer` | 实际生效的分页起始偏移量 |
| `limit` | `integer` | 实际生效的分页大小上限 |
| `fromCache` | `boolean` | 当前返回结果是否直接命中全库缓存 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

> `artists` 是 `artist` 的原子值数组：`artists.join(', ')` 恰好等于 `artist`，多值标签只能从 `artists` 精确还原，从 `artist` 还原不了。每个共享的 `Track` 行都带它（媒体库的曲目 API、`playlist.getTracks`、`playback.getCurrentTrack`、`queue.get` 与曲目事件），`library.getByPath` 的扁平结果也带；artwork 载荷不带。凡返回 `Track` 行的地方，`albumArtists` 对 `albumArtist` 同理：它的第一个值（为空时取 `artists[0]`）就是 `library.getAlbums` 给这首曲目归组用的专辑艺术家。

```javascript
// 分页获取
const page1 = await fb2k.invoke('library.getAll', { offset: 0, limit: 50 });
const page2 = await fb2k.invoke('library.getAll', { offset: 50, limit: 50 });
```

### library.getByPath

<!-- api-schema:begin library.getByPath -->
在媒体库中按路径检索单个文件并以扁平对象返回主要字段；文件未收录于媒体库时返回 `found: false`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 目标媒体文件路径；按首个子曲目检索，不支持 `\|subsong:N` 后缀不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 媒体库中是否收录该文件；为 `false` 时仅返回 `path` |
| `path` | `string` | 收录时返回 foobar2000 存储格式的路径，未收录时返回传入的原样路径 |
| `absolutePath` | `string` | 曲目的规范化本地物理文件路径 |
| `title` | `string` | 首个 `title` 标签值；缺失时为空字符串 |
| `artist` | `string` | 所有 `artist` 标签值按原顺序以 `, ` 连接 |
| `artists` | `string[]` | 所有 `artist` 标签值的数组列表 |
| `album` | `string` | 首个 `album` 标签值；缺失时为空字符串 |
| `duration` | `number` | 曲目时长（秒） |
| `trackNumber` | `string` | 首个 `tracknumber` 标签值（保留标签原始书写格式，如 `2` 或 `02/12`）；缺失时为空字符串 |
| `genre` | `string` | 所有 `genre` 标签值以 `, ` 连接 |
| `date` | `string` | 首个 `date` 标签值；缺失时为空字符串 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

> `artists` 是 `artist` 的原子值数组：`artists.join(', ')` 恰好等于 `artist`，多值标签只能从 `artists` 精确还原，从 `artist` 还原不了。每个共享的 `Track` 行都带它（媒体库的曲目 API、`playlist.getTracks`、`playback.getCurrentTrack`、`queue.get` 与曲目事件），`library.getByPath` 的扁平结果也带；artwork 载荷不带。凡返回 `Track` 行的地方，`albumArtists` 对 `albumArtist` 同理：它的第一个值（为空时取 `artists[0]`）就是 `library.getAlbums` 给这首曲目归组用的专辑艺术家。本 API 返回扁平对象，因此只多 `artists` 这一个数组键。

```javascript
const result = await fb2k.invoke('library.getByPath', { path: 'C:\\Music\\song.flac' });
if (result.success === false) throw new Error(result.error);
if (result.found) {
    console.log(`找到: ${result.title} - ${result.artist}`);
}
```

### library.getAlbums

<!-- api-schema:begin library.getAlbums -->
获取全媒体库专辑列表，按专辑名加专辑艺术家归组并跳过缺失 `album` 标签的曲目；完整查询结果（`offset: 0`、未传 `includeTracks` 且全部专辑均在 `limit` 内）将缓存至媒体库变更，后续参数一致的调用直接复用缓存；归组缓存同时共享给 `library.getAlbumTracks`，避免重复全库遍历；媒体库未启用时返回空列表

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sort` | `string` | 否 | 排序字段，支持 `name`、`artist`（专辑艺术家）、`year` 或 `trackCount`；传入其他值时默认按名称排序默认 `"name"`。 |
| `query` | `string` | 否 | 过滤关键词；仅保留名称或艺术家包含该文本的专辑（ASCII 字母不区分大小写），为空字符串时不过滤默认 `""`。 |
| `offset` | `integer` | 否 | 跳过的专辑偏移量不小于 `0`。默认 `0`。 |
| `limit` | `integer` | 否 | 本页返回的专辑数量上限不小于 `0`。默认 `100`。 |
| `includeTracks` | `boolean` | 否 | 是否在每张专辑行中附带曲目列表 `tracks`默认 `false`。 |
| `includeCover` | `boolean` | 否 | 是否在每张专辑行中附带首曲正面封面 `coverDataUrl`默认 `false`。 |
| `coverMaxSize` | `integer` | 否 | 内联封面的最大尺寸上限（KiB）；超出上限的封面不附带；`0` 或负数表示不限尺寸默认 `500`。 |
| `useCache` | `boolean` | 否 | 是否在存在有效缓存时直接复用；为 `false` 时强制重新扫描媒体库，扫描后依然写入缓存默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `albums` | `AlbumInfo[]` | 当前分页的专辑列表，按 `sort` 规则排序 |
| `albums[].albumId` | `string` | 原始专辑名或专辑艺术家包含非法 UTF-8 字符时的不透明身份标识；传给 `library.getAlbumTracks` 精确匹配（普通专辑或 `library.getArtistAlbums` 分组行不包含此字段） |
| `albums[].name` | `string` | 专辑名称 |
| `albums[].artist` | `string` | 专辑艺术家；未设置时回退至专辑内第一首曲目的艺术家 |
| `albums[].albumArtist` | `string` | 专辑内第一首曲目的 `album artist` 标签值，缺失时回退至首曲 `artist` 标签，两者均无时为空字符串 |
| `albums[].trackCount` | `integer` | 归入该专辑行的曲目数量 |
| `albums[].discCount` | `integer` | 该专辑内包含的不同碟号总数；曲目均无碟号时为 `1` |
| `albums[].duration` | `number` | 该专辑所有曲目的总时长（秒） |
| `albums[].year` | `string` | 专辑内曲目中首个读取到的 `date` 标签值；均无时为空字符串 |
| `albums[].genre` | `string` | 专辑内曲目中首个读取到的 `genre` 标签值；均无时为空字符串 |
| `albums[].label` | `string` | 专辑内曲目中首个读取到的 `publisher` 标签值，缺失时读取首个 `label` 标签值，均无时为空字符串 |
| `albums[].firstTrackPath` | `string` | 归入该专辑的首首曲目路径（foobar2000 内部存储格式，可能包含 `file-relative://` 等协议） |
| `albums[].firstTrackAbsolutePath` | `string` | `firstTrackPath` 对应的规范化本地物理文件路径，可直接传给 `artwork.getForTrack`；首曲无物理路径时缺省 |
| `albums[].coverDataUrl` | `string` | 专辑正面封面 Data URL（`data:image/...`）；仅在 `library.getAlbums` 传入 `includeCover: true` 且存在有效封面时返回 |
| `albums[].tracks` | `AlbumTrackRef[]` | 按曲号排序的曲目列表；`library.getAlbums` 传入 `includeTracks: true` 时填充，`library.getArtistAlbums` 填充该艺术家参与的曲目 |
| `albums[].tracks[].subsong` | `integer` | 子曲目索引；与 `path` 配合唯一定位多子曲目文件中的单曲 |
| `albums[].tracks[].trackNumber` | `integer` | 曲号序号；未标记时为 `0` |
| `albums[].tracks[].path` | `string` | 曲目路径（foobar2000 内部存储格式） |
| `albums[].tracks[].absolutePath` | `string` | 曲目的规范化本地物理文件路径 |
| `total` | `integer` | 满足 `query` 过滤条件的专辑总数（分页前） |
| `offset` | `integer` | 实际生效的分页起始偏移量 |
| `limit` | `integer` | 实际生效的分页大小上限 |
| `hasMore` | `boolean` | 当前页之后是否仍有未返回的专辑 |
| `includeCover` | `boolean` | 实际生效的封面内联配置 |
| `fromCache` | `boolean` | 当前返回结果是否直接命中缓存 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->


> 专辑按「专辑名 + `album artist` 首值」分组，`album artist` 标签不存在时回退到 `artist` 首值。行里的 `artist` 与 `albumArtist` 装的都是宿主解析出的那一个值，所以在没有 `album artist` 标签的专辑上，两个键报的都是首见曲目的第一位署名艺术家。两个键都不是逐曲目的署名，而多值 `album artist` 只有首值参与。要列出某位艺术家参与的专辑，用 `library.getArtistAlbums`。

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

<!-- api-schema:begin library.getAlbumTracks -->
获取指定专辑所包含的全部曲目，通过 `library.getAlbums` 返回的 `name` 与 `albumArtist` 指定（若存在 `albumId` 建议一并传入且优先级高于名称）；曲目归组逻辑与 `library.getAlbums` 完全一致，`total` 等于专辑行的 `trackCount`；曲目依次按碟号、曲号及媒体库顺序排序并共享归组缓存；未找到对应专辑或媒体库未启用时返回空曲目列表且不含 `row`，传入畸形 `albumId` 时以 `INVALID_PARAMS` 失败（`details.param` 为 `albumId`）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `albumId` | `string` | 否 | 专辑行的不透明唯一标识；提供时优先匹配且空串/畸形格式直接报错，不回退至名称查询；JSON null 视作未提供 |
| `album` | `string` | 是 | 专辑名称（对应曲目的首个 `album` 标签值，逐字节严格比对） |
| `albumArtist` | `string` | 是 | 专辑艺术家（对应曲目的首个 `album artist` 标签值，缺失时比对首个 `artist`，均无时为 `""`，逐字节严格比对） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `album` | `string` | 请求传入的原样专辑名称 |
| `albumArtist` | `string` | 请求传入的原样专辑艺术家 |
| `row` | `AlbumInfo` | 该专辑在 `library.getAlbums` 中的完整归组行（不含 `coverDataUrl` 与 `tracks`）；未匹配到对应专辑时不出现 |
| `row.albumId` | `string` | 原始专辑名或专辑艺术家包含非法 UTF-8 字符时的不透明身份标识；传给 `library.getAlbumTracks` 精确匹配（普通专辑或 `library.getArtistAlbums` 分组行不包含此字段） |
| `row.name` | `string` | 专辑名称 |
| `row.artist` | `string` | 专辑艺术家；未设置时回退至专辑内第一首曲目的艺术家 |
| `row.albumArtist` | `string` | 专辑内第一首曲目的 `album artist` 标签值，缺失时回退至首曲 `artist` 标签，两者均无时为空字符串 |
| `row.trackCount` | `integer` | 归入该专辑行的曲目数量 |
| `row.discCount` | `integer` | 该专辑内包含的不同碟号总数；曲目均无碟号时为 `1` |
| `row.duration` | `number` | 该专辑所有曲目的总时长（秒） |
| `row.year` | `string` | 专辑内曲目中首个读取到的 `date` 标签值；均无时为空字符串 |
| `row.genre` | `string` | 专辑内曲目中首个读取到的 `genre` 标签值；均无时为空字符串 |
| `row.label` | `string` | 专辑内曲目中首个读取到的 `publisher` 标签值，缺失时读取首个 `label` 标签值，均无时为空字符串 |
| `row.firstTrackPath` | `string` | 归入该专辑的首首曲目路径（foobar2000 内部存储格式，可能包含 `file-relative://` 等协议） |
| `row.firstTrackAbsolutePath` | `string` | `firstTrackPath` 对应的规范化本地物理文件路径，可直接传给 `artwork.getForTrack`；首曲无物理路径时缺省 |
| `row.coverDataUrl` | `string` | 专辑正面封面 Data URL（`data:image/...`）；仅在 `library.getAlbums` 传入 `includeCover: true` 且存在有效封面时返回 |
| `row.tracks` | `AlbumTrackRef[]` | 按曲号排序的曲目列表；`library.getAlbums` 传入 `includeTracks: true` 时填充，`library.getArtistAlbums` 填充该艺术家参与的曲目 |
| `row.tracks[].subsong` | `integer` | 子曲目索引；与 `path` 配合唯一定位多子曲目文件中的单曲 |
| `row.tracks[].trackNumber` | `integer` | 曲号序号；未标记时为 `0` |
| `row.tracks[].path` | `string` | 曲目路径（foobar2000 内部存储格式） |
| `row.tracks[].absolutePath` | `string` | 曲目的规范化本地物理文件路径 |
| `tracks` | `LibraryTrack[]` | 专辑曲目列表，依次按碟号、曲号及媒体库顺序排列；`index` 为在当前列表中的索引 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `items` | `LibraryTrack[]` | 与 `tracks` 完全一致的兼容曲目列表 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `total` | `integer` | 专辑包含的曲目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 用 library.getAlbums 的一行指定专辑
const page = await fb2k.invoke('library.getAlbums', { query: 'Abbey Road', limit: 1 });
if (page.success === false) throw new Error(page.error);
const album = page.albums[0];
if (album) {
    const res = await fb2k.invoke('library.getAlbumTracks', {
        album: album.name,
        albumArtist: album.albumArtist
    });
    if (res.success === false) throw new Error(res.error);
    console.log(res.total === album.trackCount); // true
}
```

没有标 `album artist` 的合辑在 `library.getAlbums` 里是好几行，每个首位 `artist` 值一行，这里每一行只取回它自己的曲目。要跨专辑取某位艺术家的曲目，用 `library.getArtistTracks`。

### library.getArtists

<!-- api-schema:begin library.getArtists -->
获取全媒体库所有署名艺术家及其参与统计；多值 `artist` 的每个取值单独成行；全库扫描结果将缓存至媒体库变更；媒体库未启用时以 `LIBRARY_DISABLED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sort` | `string` | 否 | 排序规则：`name` 按名称升序，`trackCount` 或 `albumCount` 按曲目数或专辑数降序；传入其他值时保持名称排序默认 `"name"`。 |
| `limit` | `integer` | 否 | 返回的艺术家数量上限不小于 `0`。默认 `1000`。 |
| `includeAlbums` | `boolean` | 否 | 是否为每位艺术家附带完整的专辑列表 `albums`默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `ArtistInfo[]` | 排序后的艺术家列表 |
| `items[].name` | `string` | 艺术家名称 |
| `items[].albumCount` | `integer` | 该艺术家曲目中包含的不同专辑名称总数 |
| `items[].trackCount` | `integer` | 该艺术家署名的曲目总数 |
| `items[].totalDuration` | `number` | 该艺术家曲目的总时长（秒） |
| `items[].albums` | `ArtistAlbumRef[]` | 该艺术家署名的专辑列表，按专辑名升序再按艺术家升序排列；仅在传入 `includeAlbums: true` 时返回且不受 `limit` 截断 |
| `items[].albums[].albumId` | `string` | 原始分组字段包含非法 UTF-8 时对应专辑行的不透明身份标识 |
| `items[].albums[].name` | `string` | 专辑名称 |
| `items[].albums[].artist` | `string` | 首个 `album artist` 标签值，缺失时为首个 `artist` 标签值 |
| `count` | `integer` | 实际返回的艺术家数量（受 `limit` 截断影响，满页表示可能仍有更多） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 每位参与艺术家各成一个条目，一首多艺术家曲目会计进其中每一位。`trackCount` 是参与曲目数，各条目相加会大于曲目总数；`albumCount` 与 `totalDuration` 同样按每位艺术家重复计入。

> 条目的 `name` 是单个原子值，可直接传给 `library.getArtistAlbums` 或 `library.getArtistTracks`。曲目对象上拼接后的 `artist` 串不行：宿主按每个原子值比较，多值曲目上拼接串命中不了；单值曲目上它恰好等于那个原子值，看着能用，别依赖。

> `albums` 在本端点本来就要做的那一遍扫描里给出整个媒体库的「艺术家 → 专辑」映射；逐位艺术家调 `library.getArtistAlbums` 则是每位艺术家各扫一遍。每个元素的 `(name, artist)` 就是 `library.getAlbums` 分组用的身份——`artist` 取 `album artist` 首值，缺则取 `artist` 首值——没有 `albumId` 时，这对值恰好对上 `library.getAlbums` 的一行；存在标识时改按标识匹配，再由对应行的 `firstTrackAbsolutePath` 取封面。没有 `album` 标签的曲目在这里不计，与 `library.getAlbums` 一致。元素按原始 `(name, artist)` 字节去重，先按 `name`、再按 `artist` 的字节序排列。`albumCount` 保持自己的口径，只按专辑名去重：同名但专辑艺术家不同的两张专辑，`albumCount` 算 1，`albums` 里是 2 条。不传 `includeAlbums` 时响应与旧版完全相同——不会出现这个键。

```javascript
// 按曲目数量排序
const artists = await fb2k.invoke('library.getArtists', { sort: 'trackCount' });

// 一次拿到「艺术家 → 专辑」映射（limit 提到艺术家数之上）
const res = await fb2k.invoke('library.getArtists', {
    includeAlbums: true,
    limit: 100000
});
if (res.success === false) throw new Error(res.error);
const { items } = res;
for (const artist of items) {
    for (const album of artist.albums) {
        // album.name / album.artist 与 library.getAlbums 的一行一一对应
    }
}
```

### library.getArtistTracks

<!-- api-schema:begin library.getArtistTracks -->
按媒体库顺序获取指定艺术家参与的所有曲目；艺术家匹配规则与 `library.getArtistAlbums` 的 `match: 'exact'` 一致；`artist` 为空或媒体库未启用时均返回空列表

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `artist` | `string` | 否 | 目标艺术家名称，与曲目中的每个 `artist` 标签值逐字节精确比对；为空字符串时返回空列表默认 `""`。 |
| `limit` | `integer` | 否 | 最多返回的曲目数量上限；按媒体库顺序取前若干首不小于 `0`。默认 `500`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artist` | `string` | 请求传入的原样艺术家名称 |
| `tracks` | `LibraryTrack[]` | 匹配的曲目列表，按媒体库顺序排列；`index` 为在当前列表中的索引 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `items` | `LibraryTrack[]` | 与 `tracks` 完全一致的兼容曲目列表 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `total` | `integer` | 实际返回的曲目总数（与 `count` 相同，不提供 `limit` 截断前的总数） |
| `count` | `integer` | 实际返回的曲目数量 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('library.getArtistTracks', { artist: 'The Beatles' });
if (res.success === false) throw new Error(res.error);
const { items } = res;
```

### library.getArtistAlbums

<!-- api-schema:begin library.getArtistAlbums -->
获取指定艺术家参与的所有专辑；`trackCount`、`duration` 与 `discCount` 仅统计该艺术家所属曲目（部分参与的专辑统计值会小于 `library.getAlbums`）；返回行仅按专辑名归组，缺失 `album` 标签的曲目归入 `(Unknown Album)`；媒体库未启用时以 `LIBRARY_DISABLED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `artist` | `string` | 是 | 目标艺术家名称；与曲目中的每个 `artist` 标签值进行逐字节匹配（多位艺术家的非首位名字亦可命中）不能为空。 |
| `limit` | `integer` | 否 | 返回的专辑数量上限（在归组完成后截断，无 offset 偏移）不小于 `0`。默认 `100`。 |
| `sort` | `string` | 否 | 排序字段，支持 `name`、`artist`、`year` 或 `trackCount`（规则与 `library.getAlbums` 一致）；其他值默认按名称升序排列默认 `"name"`。 |
| `match` | `"exact" \| "substring"` | 否 | 匹配规则：`exact` 精确匹配整个标签值，`substring` 包含子串匹配（小写字母忽略大小写，大写字母严格区分）默认 `"exact"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artist` | `string` | 请求传入的原样艺术家名称 |
| `albums` | `AlbumInfo[]` | 排序后的专辑列表，各行的 `tracks` 仅引用该艺术家参与的曲目；不包含 `coverDataUrl` 与 `albumId` |
| `albums[].albumId` | `string` | 原始专辑名或专辑艺术家包含非法 UTF-8 字符时的不透明身份标识；传给 `library.getAlbumTracks` 精确匹配（普通专辑或 `library.getArtistAlbums` 分组行不包含此字段） |
| `albums[].name` | `string` | 专辑名称 |
| `albums[].artist` | `string` | 专辑艺术家；未设置时回退至专辑内第一首曲目的艺术家 |
| `albums[].albumArtist` | `string` | 专辑内第一首曲目的 `album artist` 标签值，缺失时回退至首曲 `artist` 标签，两者均无时为空字符串 |
| `albums[].trackCount` | `integer` | 归入该专辑行的曲目数量 |
| `albums[].discCount` | `integer` | 该专辑内包含的不同碟号总数；曲目均无碟号时为 `1` |
| `albums[].duration` | `number` | 该专辑所有曲目的总时长（秒） |
| `albums[].year` | `string` | 专辑内曲目中首个读取到的 `date` 标签值；均无时为空字符串 |
| `albums[].genre` | `string` | 专辑内曲目中首个读取到的 `genre` 标签值；均无时为空字符串 |
| `albums[].label` | `string` | 专辑内曲目中首个读取到的 `publisher` 标签值，缺失时读取首个 `label` 标签值，均无时为空字符串 |
| `albums[].firstTrackPath` | `string` | 归入该专辑的首首曲目路径（foobar2000 内部存储格式，可能包含 `file-relative://` 等协议） |
| `albums[].firstTrackAbsolutePath` | `string` | `firstTrackPath` 对应的规范化本地物理文件路径，可直接传给 `artwork.getForTrack`；首曲无物理路径时缺省 |
| `albums[].coverDataUrl` | `string` | 专辑正面封面 Data URL（`data:image/...`）；仅在 `library.getAlbums` 传入 `includeCover: true` 且存在有效封面时返回 |
| `albums[].tracks` | `AlbumTrackRef[]` | 按曲号排序的曲目列表；`library.getAlbums` 传入 `includeTracks: true` 时填充，`library.getArtistAlbums` 填充该艺术家参与的曲目 |
| `albums[].tracks[].subsong` | `integer` | 子曲目索引；与 `path` 配合唯一定位多子曲目文件中的单曲 |
| `albums[].tracks[].trackNumber` | `integer` | 曲号序号；未标记时为 `0` |
| `albums[].tracks[].path` | `string` | 曲目路径（foobar2000 内部存储格式） |
| `albums[].tracks[].absolutePath` | `string` | 曲目的规范化本地物理文件路径 |
| `total` | `integer` | 截断前匹配到的专辑总数 |
| `hasMore` | `boolean` | 是否因达到 `limit` 上限而截断了后续专辑 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 行的键集与 `library.getAlbums` 的行相同，不带 `coverDataUrl` 与 canonical 专辑的 `albumId`，另有 `tracks` 引用该艺人在本组中的成员：`name`、`artist`、`albumArtist`、`trackCount`、`discCount`、`duration`、`year`、`genre`、`label`、`firstTrackPath` 与 `firstTrackAbsolutePath`。要封面就把 `firstTrackAbsolutePath` 交给 `artwork.getForTrack`——`firstTrackPath` 可能是 `file-relative://` 形态，那种形态该端点不受理。端点自己报的失败（媒体库未启用、查询出错）仍带空的 `albums`；参数错误不带。

> **`trackCount`、`duration` 与 `discCount` 只统计该艺术家参与的曲目**，不是整张专辑。同一张专辑在 `library.getAlbums` 那边给的是整张的数字，所以只要该艺术家只参与了一部分，两边就不一致——在一张 20 轨合辑上客串一首，这里报的是 `trackCount: 1`。拿本端点的行当专辑卡渲染会把数字显示得偏小。

> `total` 是截断前的专辑数。本端点没有 `offset`：`hasMore` 为真时，唯一的办法是加大 `limit` 重取。

> `library.getArtists` 给出的名字在这里能命中，即使它不是某曲目多位艺术家中的第一位——宿主是拿每个原子标签值逐个比较的。曲目对象上拼接后的 `artist` 串不能当键用，请传单个艺术家名。`substring` 下短名还会带回名字里含它的其他艺术家的专辑，且它的大小写规则不对称：查询里的小写字母能匹配标签里的任意大小写，大写字母则要求标签同一位置也是大写——`camellia` 命中 `Camellia`，`CAMELLIA` 命不中。`exact` 不受这条影响。

> 本端点只按专辑名分组，因此同名不同艺术家的专辑会并成一行。`album` 标签缺失的曲目归入 `(Unknown Album)`；标签存在但值为空串时自成一组、组名为空串。`library.getAlbums` 这两点都不同：它按专辑名加专辑艺术家分组，并且跳过专辑名缺失或为空的曲目。

```javascript
const res = await fb2k.invoke('library.getArtistAlbums', { artist: 'The Beatles' });
if (res.success === false) throw new Error(res.error);
const { albums } = res;

// 按年份从新到旧，最多 20 张
const recent = await fb2k.invoke('library.getArtistAlbums', {
    artist: 'The Beatles',
    sort: 'year',
    limit: 20
});
```

### library.getGenres

<!-- api-schema:begin library.getGenres -->
按名称升序列出全媒体库所有流派及其曲目数；多值 `genre` 的每个取值单独成项，包含多个流派的曲目重复计入各所属流派；媒体库未启用时以 `LIBRARY_DISABLED` 失败

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `genres` | `LibraryValueCount[]` | 全媒体库所有流派及其曲目数列表 |
| `genres[].name` | `string` | 标签取值内容 |
| `genres[].trackCount` | `integer` | 带有该取值的曲目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('library.getGenres');
if (res.success === false) throw new Error(res.error);
const { genres } = res;
genres.sort((a, b) => b.trackCount - a.trackCount); // 按曲目数排序
```

### library.getRandomTracks

<!-- api-schema:begin library.getRandomTracks -->
从全媒体库中随机抽取互不重复的曲目列表；每次调用均重新独立抽取；媒体库未启用或为空库时返回空列表

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `count` | `integer` | 否 | 随机抽取的曲目数量上限；最多返回媒体库曲目总数不小于 `0`。默认 `10`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `LibraryTrack[]` | 随机抽取的曲目列表；`index` 为在当前列表中的索引 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `count` | `integer` | 实际返回的曲目数量 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('library.getRandomTracks', { count: 20 });
if (res.success === false) throw new Error(res.error);
const { tracks } = res;
```

### library.getRecentlyAdded

<!-- api-schema:begin library.getRecentlyAdded -->
获取媒体库中最新添加的曲目；默认按 foo_playcount 的 `%added%` 字段降序排序，全库曲目均缺失该字段时降级按文件修改时间排序并在 `fallback` 中注明；每次调用均实时遍历全库

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `limit` | `integer` | 否 | 最多返回的曲目数量上限不小于 `0`。默认 `50`。 |
| `sortBy` | `"added" \| "modified"` | 否 | 排序依据：`added` 按 `%added%` 时间降序（缺失曲目排在最后），`modified` 按文件物理修改时间降序默认 `"added"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `RecentLibraryTrack[]` | 最近添加或修改的曲目列表（新曲在前）；`index` 为在媒体库中的绝对位置 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `tracks[].added` | `string` | foo_playcount 格式化输出的 `%added%` 时间字符串（如 `2024-05-01 12:34:56`）；仅在按此字段排序且曲目存在记录时返回 |
| `tracks[].modified` | `integer` | 文件物理修改时间戳（自 Unix 纪元起的秒数）；仅在按修改时间排序且时间有效时返回 |
| `total` | `integer` | 媒体库曲目总数（非本次返回数量） |
| `limit` | `integer` | 实际生效的分页大小上限 |
| `sortBy` | `"added" \| "modified"` | 最终实际采用的排序依据；若请求 `added` 但全库曲目均无 `%added%` 记录则回退为 `modified` |
| `fallback` | `boolean` | 是否因全库缺失 `%added%` 字段而自动降级为 `modified` 排序 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 优先使用 foo_playcount 的 %added% 时间
const recent = await fb2k.invoke('library.getRecentlyAdded', { limit: 20 });
if (recent.success === false) throw new Error(recent.error);
if (recent.fallback) console.warn('foo_playcount 未安装，使用文件修改时间');

// 强制使用文件修改时间（无需 foo_playcount）
const recent2 = await fb2k.invoke('library.getRecentlyAdded', { limit: 20, sortBy: 'modified' });
```

### library.getRoots

<!-- api-schema:begin library.getRoots -->
获取媒体库已配置的根文件夹列表；仅解析为稳定本地路径的曲目计入根目录，`http://`、`file-relative://`、`unpack://`、`archive://` 等协议路径计入 `skippedTracks`；首次调用同步构建索引后复用，直至媒体库变更或调用 `library.invalidateCache` 为止；媒体库未启用时返回 `enabled: false` 且 roots 为空，构建失败时以 `OPERATION_FAILED` 失败

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 媒体库是否已配置文件夹并启用 |
| `roots` | `LibraryRootInfo[]` | 媒体库根文件夹列表 |
| `roots[].id` | `string` | 根目录的稳定唯一标识（当前实现为其规范化物理路径），可传给 `library.browseTree` |
| `roots[].displayName` | `string` | 目录显示名称；存在重名根目录时显示完整物理路径 |
| `roots[].rawPath` | `string` | 根目录原始物理路径（当前与 `absolutePath` 相同） |
| `roots[].absolutePath` | `string` | 根目录的规范化本地物理路径 |
| `roots[].trackCount` | `integer` | 归入该根目录下的媒体库曲目总数 |
| `total` | `integer` | 根文件夹总数 |
| `indexedTracks` | `integer` | 归入物理根目录下的有效曲目数 |
| `skippedTracks` | `integer` | 因缺少稳定本地物理路径而跳过的曲目数 |
| `fromCache` | `boolean` | 是否直接命中已构建的目录树索引缓存 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 仅可解析为稳定本地绝对路径的条目会进入 `roots`。`http://`、`file-relative://`、`unpack://`、`archive://` 等协议型条目会计入 `skippedTracks`。 首次调用同步构建索引，后续走缓存。媒体库变化或调用 `library.invalidateCache` 时自动失效。

```javascript
const res = await fb2k.invoke('library.getRoots');
if (res.success === false) throw new Error(res.error);
const { roots, total } = res;
for (const root of roots) {
    console.log(`${root.displayName}: ${root.trackCount} 曲目`);
}
```

### library.browseTree

<!-- api-schema:begin library.browseTree -->
浏览媒体库根目录下的指定文件夹及其子目录；指定 `includeFiles: true` 时一并返回包含的曲目；读取与 `library.getRoots` 共用的目录树索引并在必要时同步构建；未知的 `rootId` 或 `pathId` 以 `NOT_FOUND` 失败，索引构建失败以 `OPERATION_FAILED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `rootId` | `string` | 是 | 根目录唯一标识（取自 `library.getRoots` 返回的 `id`，不区分大小写比对）不能为空。 |
| `pathId` | `string` | 否 | 待浏览的目标文件夹 `pathId`（以 `/` 分隔各级子目录，不支持 `\` 分隔符）；为空字符串时打开根目录默认 `""`。 |
| `includeFiles` | `boolean` | 否 | 是否在返回数据中附带当前目录下的曲目列表 `files`默认 `false`。 |
| `recursiveFiles` | `boolean` | 否 | 配合 `includeFiles` 使用，是否递归包含所有层级子目录中的全部曲目默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `root` | `LibraryRootInfo` | 所在根目录的配置信息 |
| `root.id` | `string` | 根目录的稳定唯一标识（当前实现为其规范化物理路径），可传给 `library.browseTree` |
| `root.displayName` | `string` | 目录显示名称；存在重名根目录时显示完整物理路径 |
| `root.rawPath` | `string` | 根目录原始物理路径（当前与 `absolutePath` 相同） |
| `root.absolutePath` | `string` | 根目录的规范化本地物理路径 |
| `root.trackCount` | `integer` | 归入该根目录下的媒体库曲目总数 |
| `pathId` | `string` | 当前浏览的目标文件夹 `pathId` |
| `absolutePath` | `string` | 文件夹的规范化本地物理路径 |
| `directories` | `LibraryDirectoryNodeInfo[]` | 直接下属的子文件夹列表，依次按 `displayName` 与 `absolutePath` 忽略大小写排序 |
| `directories[].id` | `string` | 文件夹节点复合标识符（格式为 `${rootId}::${pathId}`） |
| `directories[].rootId` | `string` | 文件夹所属的根目录 `id` |
| `directories[].pathId` | `string` | 相对于根目录的路径（以 `/` 分隔各级目录名），可直接传给 `library.browseTree` 打开该目录 |
| `directories[].parentPathId` | `string` | 父级目录的 `pathId`；直接位于根目录下时为空字符串 `""` |
| `directories[].name` | `string` | 文件夹名称 |
| `directories[].displayName` | `string` | 文件夹显示名称（当前与 `name` 相同） |
| `directories[].rawPath` | `string` | 根目录原始物理路径（当前与 `absolutePath` 相同） |
| `directories[].absolutePath` | `string` | 文件夹的规范化本地物理路径 |
| `directories[].relativePath` | `string` | 相对路径（当前与 `pathId` 相同） |
| `directories[].depth` | `integer` | 目录在 `pathId` 中的层级深度；直接位于根目录下时为 `1` |
| `directories[].trackCount` | `integer` | 该文件夹及其所有子目录中包含的媒体库曲目总数 |
| `directories[].childDirectoryCount` | `integer` | 直接下属的子文件夹数量 |
| `directories[].hasChildren` | `boolean` | 是否包含直接子文件夹（即 `childDirectoryCount > 0`） |
| `files` | `LibraryTrack[]` | 包含的曲目列表，按媒体库顺序排列（启用 `recursiveFiles` 时后接子目录曲目，顺序不固定）；未指定 `includeFiles` 时为空列表；`index` 为构建索引时的媒体库位置 |
| `files[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `files[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `fromCache` | `boolean` | 是否直接命中构建完毕的目录树索引缓存 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 先获取根列表
const res = await fb2k.invoke('library.getRoots');
if (res.success === false) throw new Error(res.error);
const { roots } = res;
// 浏览第一个根的顶层目录
const tree = await fb2k.invoke('library.browseTree', { rootId: roots[0].id });
if (tree.success === false) throw new Error(tree.error);
// 展开子目录
const sub = await fb2k.invoke('library.browseTree', {
    rootId: roots[0].id,
    pathId: tree.directories[0].pathId,
    includeFiles: true
});
```

### library.browseDirectory

<!-- api-schema:begin library.browseDirectory -->
按路径前缀列出媒体库下一层目录；指定 `includeFiles: true` 时递归返回该路径下任意深度的所有曲目；前缀直接匹配 foobar2000 内部存储路径，本地目录须使用 `file://` 协议前缀（传入纯绝对路径无法匹配任何结果）；浏览物理根目录推荐使用 `library.browseTree`；媒体库未启用时以 `LIBRARY_DISABLED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 路径前缀（ASCII 字母不区分大小写）；为空字符串时匹配全库曲目默认 `""`。 |
| `includeFiles` | `boolean` | 否 | 是否在返回数据中附带匹配路径下的曲目列表 `files`；默认开启，`path` 为空时将返回全库曲目默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `directories` | `string[]` | `path` 下一层的子文件夹路径列表（foobar2000 存储格式，按字节序排列） |
| `items` | `string[]` | 与 `directories` 完全一致的兼容目录列表 |
| `files` | `LibraryTrack[]` | 匹配路径下的曲目列表，按媒体库顺序排列；未指定 `includeFiles` 时为空列表；`index` 为在媒体库中的绝对位置 |
| `files[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `files[].index` | `integer` | 当前上下文列表中的行索引序号 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const root = await fb2k.invoke('library.browseDirectory', { includeFiles: false });
```

## 搜索

### library.search

<!-- api-schema:begin library.search -->
分页检索符合 foobar2000 查询表达式的曲目，固定按媒体库原生顺序排列（支持传入 `SORT BY` 子句但不生效）；查询解析失败时以 `INVALID_PARAMS` 失败（`details.param` 为 `query`）；`query` 为空或媒体库未启用时均返回空列表；数据在后台线程序列化写出

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 否 | foobar2000 检索查询表达式；为空字符串时直接返回空列表默认 `""`。 |
| `offset` | `integer` | 否 | 跳过的匹配曲目偏移量不小于 `0`。默认 `0`。 |
| `limit` | `integer` | 否 | 本页返回的曲目行数上限不小于 `0`。默认 `100`。 |
| `fields` | `string[]` | 否 | 每行曲目返回的字段白名单（用法同 `library.query`）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `LibraryTrackPartial[]` | 当前分页匹配的曲目列表，按媒体库原生顺序排列；`index` 为在全部匹配曲目中的位置；各行仅包含 `fields` 要求的字段 |
| `tracks[].handle` | `string` | 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `\|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。 |
| `tracks[].path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `tracks[].absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `tracks[].subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `tracks[].title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `tracks[].artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `tracks[].album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `tracks[].albumId` | `string` | 原始专辑名或专辑艺术家含非法 UTF-8 时提供的不透明身份标识。此时用它匹配专辑行，不用显示名称；普通名称不带此字段。 |
| `tracks[].albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].albumArtists` | `string[]` | 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。存在 `albumId` 时应改用它匹配，因为替换字符可能让不同原始名称显示相同。 |
| `tracks[].genre` | `string` | 全部 GENRE 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].date` | `string` | 第一个 DATE 值，按标签原样，如 `2019` 或 `2019-05-01`；没有标签时为空。 |
| `tracks[].trackNumber` | `integer` | TRACKNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].discNumber` | `integer` | DISCNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].duration` | `number` | 时长，单位秒；未知时为 `0`。 |
| `tracks[].fileSize` | `integer` | 文件大小，单位字节；未知（如网络流）时为 `-1`。 |
| `tracks[].bitrate` | `integer` | 平均码率，单位 kbit/s；未知时为 `0`。 |
| `tracks[].sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `tracks[].channels` | `integer` | 声道数；未知时为 `0`。 |
| `tracks[].codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |
| `tracks[].rating` | `integer` | 评分 0 到 5：`%rating%` 统计值（foo_playcount）在 1 到 5 之间时用它，否则用 RATING 标签并夹到该范围；两处都没有评分时为 `0`。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `total` | `integer` | 满足查询条件的匹配曲目总数（分页前） |
| `offset` | `integer` | 实际生效的分页起始偏移量 |
| `limit` | `integer` | 实际生效的分页大小上限 |
| `hasMore` | `boolean` | 当前页之后是否仍有未返回的匹配曲目 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
// 简单关键词搜索
await fb2k.invoke('library.search', { query: 'love' });

// foobar2000 查询语法
await fb2k.invoke('library.search', { query: 'artist HAS beatles' });
await fb2k.invoke('library.search', { query: 'artist HAS beatles AND year GREATER 1968' });

// 分页
const page2 = await fb2k.invoke('library.search', { query: 'rock', offset: 100, limit: 50 });

// 字段投影：tracks 中每行只含请求的两个键
const albums = await fb2k.invoke('library.search', {
    query: 'artist HAS beatles',
    limit: 500,
    fields: ['absolutePath', 'album']
});
```

### library.query

<!-- api-schema:begin library.query -->
检索符合 foobar2000 查询表达式的曲目列表，支持通过 `sort` 排序并限制返回最多 `limit` 行；查询解析失败或包含 `SORT BY` 子句时以 `INVALID_PARAMS` 失败（`details.param` 为 `query`）；部分畸形语法不会报错但将匹配不到任何结果；数据在后台线程序列化写出；媒体库未启用时以 `LIBRARY_DISABLED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | foobar2000 检索查询表达式（不支持附带 `SORT BY` 子句）不能为空。 |
| `sort` | `string` | 否 | 用于在应用 `limit` 截断前进行排序的 Title Formatting 表达式；为空字符串时保持媒体库顺序，语法编译失败时忽略该项默认 `""`。 |
| `limit` | `integer` | 否 | 最多返回的曲目行数上限；`total` 依然统计全部匹配数不小于 `0`。默认 `100`。 |
| `fields` | `string[]` | 否 | 每行曲目返回的字段白名单（严格区分大小写）；省略时返回曲目所有字段；传入未知字段名以 `INVALID_PARAMS` 失败（列入 `details.unknownFields`）；无 `albumId` 的曲目即使请求了该键也会自动省略不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `LibraryTrackPartial[]` | 匹配的曲目列表，按 `sort` 或媒体库顺序排列；`index` 为在当前结果列表中的位置；各行仅包含 `fields` 要求的字段 |
| `tracks[].handle` | `string` | 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `\|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。 |
| `tracks[].path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `tracks[].absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `tracks[].subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `tracks[].title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `tracks[].artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `tracks[].album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `tracks[].albumId` | `string` | 原始专辑名或专辑艺术家含非法 UTF-8 时提供的不透明身份标识。此时用它匹配专辑行，不用显示名称；普通名称不带此字段。 |
| `tracks[].albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].albumArtists` | `string[]` | 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。存在 `albumId` 时应改用它匹配，因为替换字符可能让不同原始名称显示相同。 |
| `tracks[].genre` | `string` | 全部 GENRE 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].date` | `string` | 第一个 DATE 值，按标签原样，如 `2019` 或 `2019-05-01`；没有标签时为空。 |
| `tracks[].trackNumber` | `integer` | TRACKNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].discNumber` | `integer` | DISCNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].duration` | `number` | 时长，单位秒；未知时为 `0`。 |
| `tracks[].fileSize` | `integer` | 文件大小，单位字节；未知（如网络流）时为 `-1`。 |
| `tracks[].bitrate` | `integer` | 平均码率，单位 kbit/s；未知时为 `0`。 |
| `tracks[].sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `tracks[].channels` | `integer` | 声道数；未知时为 `0`。 |
| `tracks[].codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |
| `tracks[].rating` | `integer` | 评分 0 到 5：`%rating%` 统计值（foo_playcount）在 1 到 5 之间时用它，否则用 RATING 标签并夹到该范围；两处都没有评分时为 `0`。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `total` | `integer` | 满足查询条件的曲目总数（`limit` 截断前） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

// 字段投影：每行只含 absolutePath 一个键
const paths = await fb2k.invoke('library.query', {
    query: '%codec% IS FLAC',
    limit: 100000,
    fields: ['absolutePath']
});
```

## 本地副本的增量同步 {#keeping-a-local-copy}

页面可在本地（如 IndexedDB）以曲目 `handle` 为主键缓存媒体库副本，仅拉取增量变更。`handle` 与 `library.getChanges` 返回的修订号均跨进程重启持久有效。

### library.getChanges

<!-- api-schema:begin library.getChanges -->
获取当前媒体库修订号及指定修订号 `since` 之后的变化曲目，用于前端增量同步；修订版本号跨进程重启持久化保存；`epoch` 匹配且 `since` 落在有效历史范围内时返回 `changed` 与 `removed` 列表；超出范围时返回 `resync: true` 及全量 `handles` 由前端全量重建；媒体库初始载入完成后的所有变更（含 foo_playcount 等组件写入的统计数据）均会递增修订号；启动载入期间调用以 `OPERATION_PENDING` 失败（`details.reason: "libraryLoading"`）；媒体库未启用时返回 `resync: true` 且 `handles` 为空

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `epoch` | `string` | 否 | 前端副本建立时的服务周期标识符 `epoch`；省略时直接触发全量同步（`resync: true`） |
| `since` | `integer` | 否 | 前端副本当前所处的修订版本号 `revision`；省略时直接触发全量同步（`resync: true`）不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `epoch` | `string` | 宿主当前的记录周期标识；宿主异常退出或重置记录时会变更，前端持有旧 epoch 必须全量重建副本 |
| `revision` | `integer` | 本次同步后最新达到的修订版本号；下次增量查询时作为 `since` 传入 |
| `resync` | `boolean` | 是否需要根据 `handles` 全量重建前端数据副本 |
| `changed` | `string[]` | 自 `since` 版本以来新增或修改且当前仍在库中的曲目 handle 列表（可调用 `library.getTracks` 获取详情）；全量同步（`resync: true`）时缺省 |
| `removed` | `string[]` | 自 `since` 版本以来从媒体库中移除的曲目 handle 列表；全量同步（`resync: true`）时缺省 |
| `handles` | `string[]` | 当前媒体库的全量曲目 handle 列表；增量同步（`resync: false`）时缺省 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### library.getTracks

<!-- api-schema:begin library.getTracks -->
按传入的 handle 顺序批量获取媒体库曲目行；数据直接从内存读取，不访问媒体文件；未收录或媒体库禁用时的 handle 归入 `missing` 列表；请求超过 5000 个 handle 时以 `INVALID_PARAMS` 失败；数据在后台线程序列化写出

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `handles` | `string[]` | 是 | 待批量查询的曲目 handle 列表（最多 5000 项） |
| `fields` | `string[]` | 否 | 每行曲目返回的字段白名单（用法同 `library.query`）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `LibraryTrackPartial[]` | 查找到的曲目列表，保持与输入 `handles` 严格一致的顺序；`index` 为在当前返回列表中的位置；各行仅包含 `fields` 要求的字段 |
| `tracks[].handle` | `string` | 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `\|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。 |
| `tracks[].path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `tracks[].absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `tracks[].subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `tracks[].title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `tracks[].artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `tracks[].album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `tracks[].albumId` | `string` | 原始专辑名或专辑艺术家含非法 UTF-8 时提供的不透明身份标识。此时用它匹配专辑行，不用显示名称；普通名称不带此字段。 |
| `tracks[].albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].albumArtists` | `string[]` | 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。存在 `albumId` 时应改用它匹配，因为替换字符可能让不同原始名称显示相同。 |
| `tracks[].genre` | `string` | 全部 GENRE 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].date` | `string` | 第一个 DATE 值，按标签原样，如 `2019` 或 `2019-05-01`；没有标签时为空。 |
| `tracks[].trackNumber` | `integer` | TRACKNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].discNumber` | `integer` | DISCNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].duration` | `number` | 时长，单位秒；未知时为 `0`。 |
| `tracks[].fileSize` | `integer` | 文件大小，单位字节；未知（如网络流）时为 `-1`。 |
| `tracks[].bitrate` | `integer` | 平均码率，单位 kbit/s；未知时为 `0`。 |
| `tracks[].sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `tracks[].channels` | `integer` | 声道数；未知时为 `0`。 |
| `tracks[].codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |
| `tracks[].rating` | `integer` | 评分 0 到 5：`%rating%` 统计值（foo_playcount）在 1 到 5 之间时用它，否则用 RATING 标签并夹到该范围；两处都没有评分时为 `0`。 |
| `tracks[].index` | `integer` | 当前上下文列表中的行索引序号 |
| `missing` | `string[]` | 未在媒体库中找到或媒体库禁用时的 handle 列表，保持输入顺序 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### 用法

```js @ts-nocheck
const LIMIT = 5000;

async function fetchRows(handles) {
    for (let i = 0; i < handles.length; i += LIMIT) {
        const res = await fb2k.invoke('library.getTracks', { handles: handles.slice(i, i + LIMIT) });
        if (res.success === false) throw new Error(res.error);
        await store.put(res.tracks);
        await store.delete(res.missing);
    }
}

async function sync() {
    const { epoch, revision } = await store.getMark();
    const res = await fb2k.invoke('library.getChanges', { epoch, since: revision });
    if (res.success === false) throw new Error(res.error);
    if (res.resync) {
        await store.clear();
        await fetchRows(res.handles);
    } else {
        await store.delete(res.removed);
        await fetchRows(res.changed);
    }
    await store.setMark(res.epoch, res.revision);
}
```

- 冷启动时先读取本地副本渲染界面，随后调用 `sync()`。媒体库加载完成前调用会以 `OPERATION_PENDING` 失败；可先检查 `library.getStatus` 的 `initialized` 字段，或等待 `library:initialized` 事件触发。
- 运行期间，监听到任意 `library:items*` 事件后防抖调用 `sync()`。同步机制不依赖事件携带的 `handles`，即使批量变更事件被截断也不会遗漏。
- 批量拉取曲目行期间产生的新变更，其修订号高于本次同步应答，会在下次 `sync()` 中再次返回；覆盖写入同一行数据幂等无害。
- foobar2000 启动加载期间会广播全量 `library:itemsAdded`，此阶段不推进修订号。关闭期间产生的离线变更将在 `library:initialized` 之后陆续分发。

不可感知的变更场景：

- 未加载本组件时产生的变更，以及在进程关闭时恢复备份配置（Profile）。恢复后修订号可能被重复使用，此时应重建本地副本。
- 宿主未感知的变更。网络共享或挂载的 WebDAV 盘通常缺少文件变更通知，须待 foobar2000 启动扫描或手动调用 `library.rescan`。
- 未正常退出时 `epoch` 变更，下次调用将直接返回 `resync: true`。

## 字段投影 {#field-projection}

`library.query` 与 `library.search` 支持可选参数 `fields`，用来限定每行返回哪些曲目字段。省略 `fields` 或传 `null` 时，每行输出媒体库曲目行的全部键。

传 `fields` 时，每行只包含请求的键，不附带任何未请求字段；元数据容器读取失败的条目仍输出请求的非可选键，取自元数据的字段以类型默认值填充（空串、0）。可选的 `albumId` 在没有标识时省略，即使请求了该键也是如此。响应信封不受影响：`library.query` 仍返回 `success` / `tracks` / `total`，`library.search` 仍返回 `success` / `tracks` / `total` / `offset` / `limit` / `hasMore`。

**可用字段名**（精确匹配、大小写敏感）：

`index`、`handle`、`title`、`artist`、`artists`、`album`、`albumId`、`albumArtist`、`albumArtists`、`genre`、`date`、`trackNumber`、`discNumber`、`duration`、`path`、`absolutePath`、`fileSize`、`bitrate`、`sampleRate`、`channels`、`codec`、`subsong`、`rating`

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

> `artists` 是 `artist` 的原子值数组：`artists.join(', ')` 恰好等于 `artist`，多值标签只能从 `artists` 精确还原，从 `artist` 还原不了。每个共享的 `Track` 行都带它（媒体库的曲目 API、`playlist.getTracks`、`playback.getCurrentTrack`、`queue.get` 与曲目事件），`library.getByPath` 的扁平结果也带；artwork 载荷不带。凡返回 `Track` 行的地方，`albumArtists` 对 `albumArtist` 同理：它的第一个值（为空时取 `artists[0]`）就是 `library.getAlbums` 给这首曲目归组用的专辑艺术家。数组与对应的拼接串可单独投影其一，每一对都同源于一次取值；元数据容器读取失败的损坏条目上，被请求的 `artists` 或 `albumArtists` 返回 `[]`。

重复字段名会去重。`rating` 仅在被请求（或省略 `fields`）时才计算——不请求 `rating` 的投影查询因此省下大部分开销。

**校验**一律 **resolve**，不会 reject Promise。列表外的名字得到：

```javascript
const bad = await fb2k.invoke('library.query', {
    query: 'artist HAS beatles',
    fields: ['absolutepath', 'Rating']   // 大小写不符
});
// {
//   success: false,
//   error: 'fields contains unknown field names',
//   code: 'INVALID_PARAMS',
//   details: { unknownFields: ['absolutepath', 'Rating'] }
// }
```

不是数组、空数组或含非字符串元素，与其他格式不对的参数一样以 `INVALID_PARAMS` 失败，不带 `details`。

**使用建议**

| 场景 | 建议的 `fields` |
| --- | --- |
| 过滤数万命中，只要路径 | `['absolutePath']` |
| 搜索结果要在界面上展示（数百行、要全部列） | 省略 `fields` |
| 过滤 + 按专辑统计 | `['absolutePath', 'album']` |

以 8 万行结果集实测：单字段投影使 wire 载荷从 45.1MB 降到 8.4MB，页面侧 `JSON.parse` 从 147ms 降到 37ms。

### 大结果集

**宿主主线程被占用的时长与响应体量成正比。** 成本不在产生这些行——那是在 worker 线程上做的——而在把成品响应交给页面，实测每 MiB 15–27ms。因此控制单次调用返回多少字节是唯一有效手段，有两个：

| 手段 | 做法 | 效果 |
| --- | --- | --- |
| 投影 | `fields: [...]` | 全字段降到 `['absolutePath']` 后载荷约缩到 1/4.6（8 万行时 44.3 → 9.6 MiB） |
| 分页 | `offset` / `limit` | 占用只与该页行数成正比，可压到任意目标以下 |

**受支持的访问模式**：分页、投影任一或并用后，每次调用约 ≤2 万行，此时单次调用的主线程占用低于 100ms。超出这个量级请分页，不要靠一次调用拿全部。

::: warning 32 位（x86）宿主须规避大结果集
32 位进程的用户态地址空间约 2–4 GB，而一次全库全字段响应在解析期间会同时以多种形态驻留（宿主侧 UTF-8 串、宽串、渲染进程侧物化值）。估算的瞬时峰值：

| 行数 | 全字段 | 投影到 `['absolutePath']` |
| ---: | ---: | ---: |
| 80,000 | ≈ 178 MB | ≈ 39 MB |
| 165,306 | ≈ 367 MB | ≈ 80 MB |

这些数值本身未必致命，但它们叠加在宿主既有占用之上。在 32 位宿主上，查询可能命中上万行时请投影或分页，**不要**对全库发全字段请求。64 位宿主没有这个地址空间限制，但上面的主线程占用同样存在。

峰值是按解析模型推算的上界，不是实测工作集——请当作使用指引，而不是预算。
:::

## 媒体库操作

### library.addToPlaylist

<!-- api-schema:begin library.addToPlaylist -->
按文件路径将曲目追加至指定播放列表；路径无需存在于媒体库中，不存在的文件亦照常添加；目标播放列表已锁定时以 `LOCKED` 失败且不添加任何曲目

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待追加的文件路径列表，按传入顺序依次添加；可附加 `\|subsong:N` 后缀指定子曲目不能为空。 |
| `playlist` | `integer` | 否 | 目标播放列表的索引序号；省略时默认操作活动播放列表；越界索引以 `INVALID_INDEX` 失败，缺省且无活动列表时以 `NO_ACTIVE_ITEM` 失败不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `added` | `integer` | 成功追加到播放列表的曲目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('library.search', { query: 'artist HAS Beatles' });
if (res.success === false) throw new Error(res.error);
const { tracks } = res;
const paths = tracks.map(t => t.path);
await fb2k.invoke('library.addToPlaylist', { paths, playlist: 0 });
```

### library.rescan

<!-- api-schema:begin library.rescan -->
调用 `library_manager::rescan()` 请求 foobar2000 重新扫描媒体库文件夹（foobar2000 SDK 已将该接口标为废弃且不建议调用，媒体库本身会自动监视跟踪文件变更）

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('library.rescan');
```

### library.refresh

<!-- api-schema:begin library.refresh -->
同 `library.rescan`

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 缓存控制

> 自动失效: 媒体库变化时缓存自动失效（通过 `library_callback_v2` 监听）。

### library.invalidateCache

<!-- api-schema:begin library.invalidateCache -->
丢弃宿主缓存的媒体库查询结果与目录树索引；后续调用对应端点时按需重建，媒体库内容变更时亦会自动清空

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `timestamp` | `integer` | 缓存被清空丢弃的时间戳（自 Unix 纪元起的毫秒数） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('library.invalidateCache');
const albums = await fb2k.invoke('library.getAlbums', { useCache: true });
```

### library.getCacheStats

<!-- api-schema:begin library.getCacheStats -->
获取宿主媒体库缓存与目录树索引的运行计数，供状态诊断使用

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `valid` | `boolean` | 自上次清空缓存以来是否存在有效的媒体库缓存结果 |
| `lastModified` | `integer` | 上次清理或丢弃缓存的时间戳（自 Unix 纪元起的毫秒数） |
| `albumsCacheEntries` | `integer` | 缓存的 `library.getAlbums` 列表数量（按不同 `query`、`sort`、`includeCover` 组合分别缓存） |
| `tracksCached` | `boolean` | 是否已缓存全量 `library.getAll` 结果 |
| `artistsCached` | `boolean` | 是否已缓存 `library.getArtists` 全库扫描结果 |
| `genresCached` | `boolean` | 是否缓存流派列表；固定为 `false`（流派不缓存） |
| `statsCached` | `boolean` | 是否已缓存 `library.getStatus` 查询结果 |
| `coversCached` | `integer` | 缓存的封面数量；固定为 `0`（封面不缓存） |
| `coverCacheBytes` | `integer` | 缓存的封面数量；固定为 `0`（封面不缓存） |
| `coverCacheMB` | `number` | 缓存的封面数量；固定为 `0`（封面不缓存） |
| `cacheHits` | `integer` | 宿主启动以来命中内存缓存的查询总次数 |
| `cacheMisses` | `integer` | 宿主启动以来未命中内存缓存的查询总次数 |
| `treeIndexValid` | `boolean` | 目录树索引是否已构建有效 |
| `rootsCached` | `integer` | 目录树索引中的根目录数量；未构建时为 `0` |
| `treeIndexedTracks` | `integer` | 目录树索引上次构建时归入的曲目总数 |
| `treeSkippedTracks` | `integer` | 目录树索引上次构建时跳过的曲目总数 |
| `treeLastBuilt` | `integer` | 目录树索引上次构建的时间戳（自 Unix 纪元起的毫秒数）；未构建时为 `0` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 事件

| 事件 | 描述 | 数据 |
| --- | --- | --- |
| library:itemsAdded | 媒体库新增曲目 | { count, handles, truncated, timestamp } |
| library:itemsRemoved | 媒体库删除曲目 | { count, handles, truncated, timestamp } |
| library:itemsModified | 媒体库曲目元数据变化 | { count, handles, truncated, fromHook, timestamp } |
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

<!-- api-schema:begin library.getFieldValues -->
获取指定标签在全媒体库中的所有不同取值及其曲目数，按出现频次降序排列；媒体库未启用时以 `LIBRARY_DISABLED` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `field` | `string` | 是 | 目标元数据标签名称（如 `genre`）不能为空。 |
| `separator` | `string` | 否 | 拆分多值标签的分隔符；依此字符串拆分为多个独立取值并去除各段前后空白，为空字符串时不拆分按整值统计默认 `""`。 |
| `limit` | `integer` | 否 | 最多返回的取值数量上限不小于 `0`。默认 `5000`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `values` | `LibraryValueCount[]` | 标签取值列表，按包含曲目数降序排列 |
| `values[].name` | `string` | 标签取值内容 |
| `values[].trackCount` | `integer` | 带有该取值的曲目总数 |
| `total` | `integer` | 截断前匹配到的不同取值总数 |
| `field` | `string` | 请求传入的原样标签名称 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
// 最小调用：只传必填的 field
const res = await fb2k.invoke('library.getFieldValues', { field: 'genre' });
if (res.success === false) throw new Error(res.error);
const { values } = res;

// 多值字段可用 separator 拆分，并限制返回条数
const res2 = await fb2k.invoke('library.getFieldValues', {
    field: 'artist',
    separator: ';',
    limit: 50
});
if (res2.success === false) throw new Error(res2.error);
const { values: artists } = res2;
```

## 使用说明

- `asyncResult` 默认是 `false`。完整媒体库请求启用该项后，立即返回 `{ pending, requestId }`；完成后的 `{ requestId, tracks, items, total, offset, limit, fromCache }` 会通过 `library:getAllResult` 发送给发起调用的 WebView。
- `library.getRoots` 和 `library.browseTree` 是类型化的媒体库导航 API。`library.browseDirectory` 是旧的路径前缀投影视图，不代表真实根目录集合。
- `library.getAlbums` 仅在 `includeCover` 启用且存在封面时添加 `coverDataUrl`。该字段是 `data:image/...` URL，而不是 `fb2k://` URL。
- `library.search` 和 `library.query` 使用 foobar2000 查询语法，底层实现使用 `search_filter_v2`；客户端不应自行解析语法，并应处理表达式非法时 handler 返回的错误。
- `library.getStatus` 和 `library.getCount` 通过 `enum_items` 枚举，不会返回 `metadb_handle_list`。`library_callback_v2` 回调会先使缓存失效，再广播下列事件。`library:itemsModified` 保留 `library.getRoots` 与 `library.browseTree` 用的目录索引，因为路径不变；`fromHook: true` 的只清掉保留的 `library.getAll` 列表。每个事件都用 `handles` 列出变化的曲目，`truncated` 时不列。

## 媒体库事件 Contract

四个事件均广播到每个 WebView。

| 事件 | Payload |
| --- | --- |
| `library:itemsAdded` | `{ count, handles, truncated, timestamp }` |
| `library:itemsRemoved` | `{ count, handles, truncated, timestamp }` |
| `library:itemsModified` | `{ count, handles, truncated, fromHook, timestamp }` |
| `library:initialized` | `{ timestamp }` |
