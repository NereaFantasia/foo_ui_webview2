# Library API

媒体库浏览、搜索、缓存控制。

> ⚠️ 大部分 Library API 需要 foobar2000 媒体库已启用。未启用时返回空结果或 `error: "Library not enabled"`。 根目录枚举请优先使用 `library.getRoots`；`library.browseDirectory` 仅保留为 legacy 目录投影视图。

## 媒体库状态

### library.isEnabled

<!-- api-schema:begin library.isEnabled -->
foobar2000 媒体库是否启用，即是否配置了媒体库文件夹。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 媒体库是否启用。 |

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
媒体库是否启用及曲目数。结果保留到媒体库变化为止；媒体库未启用时不保留。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 媒体库是否启用。 |
| `initialized` | `boolean` | 恒与 `enabled` 相同。 |
| `scanning` | `boolean` | 恒为 `false`；宿主不报告扫描状态。 |
| `itemCount` | `integer` | 媒体库曲目数；未启用时为 `0`。 |
| `count` | `integer` | 与 `itemCount` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### library.getStats

<!-- api-schema:begin library.getStats -->
整个媒体库的汇总计数。媒体库未启用时各计数都是 `0`。每次调用都会遍历整个媒体库。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `totalTracks` | `integer` | 媒体库曲目数。 |
| `totalAlbums` | `integer` | 专辑数，按专辑名加专辑艺术家计，与 `library.getAlbums` 的分组一致。 |
| `totalArtists` | `integer` | 参与艺术家数：多值 `artist` 的每个值都计入，与 `library.getArtists` 的条目数一致。 |
| `totalDuration` | `number` | 全部曲目的总时长，单位秒。 |
| `totalSize` | `integer` | 文件总大小，单位字节。 |
| `cacheValid` | `boolean` | 宿主是否持有自上次丢弃缓存以来写入的媒体库缓存结果。 |
| `lastModified` | `integer` | 缓存上次被丢弃的时间，自 Unix 纪元起的毫秒数。 |

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
媒体库里的曲目数。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 媒体库曲目数。 |

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
整个媒体库的曲目，按媒体库顺序分页返回。从 `offset` 0 开始且覆盖全部曲目的请求会保留到媒体库变化为止，之后带 `useCache` 从 `offset` 0 开始的调用直接用它回答。带 `asyncResult` 时，这样的请求若没有用保留的列表回答，就在主线程之外构建：调用立即返回 `{ pending: true, requestId }`，这一页随后以 `library:getAllResult` 事件发到发起调用的窗口。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `offset` | `integer` | 否 | 跳过的曲目数。不小于 `0`。默认 `0`。 |
| `limit` | `integer` | 否 | 最多返回的曲目数。不小于 `0`。默认 `100`。 |
| `useCache` | `boolean` | 否 | 从 `offset` 0 开始时，有保留的列表就直接用它回答。无论取值如何，从 `offset` 0 开始且覆盖全部曲目的请求都会被保留。默认 `true`。 |
| `asyncResult` | `boolean` | 否 | 从 `offset` 0 开始且覆盖全部曲目的请求在主线程之外构建，以 `library:getAllResult` 事件送达；只在带 `useCache` 时生效，由保留的列表回答时也不生效。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pending` | `boolean` | 这一页将以 `library:getAllResult` 事件送达时为 `true`，此时没有这一页的各字段。 |
| `requestId` | `string` | `library:getAllResult` 事件带的标识；随 `pending` 出现。 |
| `tracks` | `LibraryTrack[]` | 这一页，按媒体库顺序；`index` 是在媒体库中的位置。 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `items` | `LibraryTrack[]` | 与 `tracks` 相同的列表。 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `total` | `integer` | 媒体库曲目数。 |
| `offset` | `integer` | 生效的 `offset`。 |
| `limit` | `integer` | 生效的 `limit`。 |
| `fromCache` | `boolean` | 这一页来自保留的列表时为 `true`。 |

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
在媒体库中查找一个文件，以扁平对象返回它的主要字段。不在媒体库里的文件成功返回 `found: false`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 文件路径。按文件的第一个子曲目查找，不认 `\|subsong:N` 后缀。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 文件是否在媒体库里；除 `path` 外的其他字段只在找到时出现。 |
| `path` | `string` | 找到时为 foobar2000 存储的曲目路径；否则为原样的路径。 |
| `absolutePath` | `string` | 曲目路径的本地文件路径形式。 |
| `title` | `string` | 第一个 `title` 值；没有时为空。 |
| `artist` | `string` | 全部 `artist` 值按原顺序以 `, ` 连接。 |
| `artists` | `string[]` | 全部 `artist` 值；`artists.join(', ')` 等于 `artist`。 |
| `album` | `string` | 第一个 `album` 值；没有时为空。 |
| `duration` | `number` | 时长，单位秒。 |
| `trackNumber` | `string` | 第一个 `tracknumber` 值，保持标签里的写法（如 `2` 或 `02/12`）；没有时为空。 |
| `genre` | `string` | 全部 `genre` 值以 `, ` 连接。 |
| `date` | `string` | 第一个 `date` 值；没有时为空。 |

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
整个媒体库的专辑，按专辑名加专辑艺术家分组；没有 `album` 标签的曲目跳过。完整的列表（`offset` 为 0、不带 `includeTracks`、全部专辑都在 `limit` 之内）会保留到媒体库变化为止，之后 `query`、`sort`、`includeCover` 相同、从 `offset` 0 开始且不带 `includeTracks` 的调用直接用它回答。列表背后的归组结果同样保留，并与 `library.getAlbumTracks` 共用，所以媒体库变化之前，其他调用也不会再读一遍媒体库。媒体库未启用时成功返回空列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sort` | `string` | 否 | `name`、`artist`（专辑艺术家）、`year` 或 `trackCount`；其他值按名称排序。默认 `"name"`。 |
| `query` | `string` | 否 | 只保留名称或艺术家包含这段文本的专辑，ASCII 字母不区分大小写；为空时保留全部。默认 `""`。 |
| `offset` | `integer` | 否 | 跳过的专辑数。不小于 `0`。默认 `0`。 |
| `limit` | `integer` | 否 | 最多返回的专辑数。不小于 `0`。默认 `100`。 |
| `includeTracks` | `boolean` | 否 | 为每张专辑附上 `tracks`。默认 `false`。 |
| `includeCover` | `boolean` | 否 | 为每张专辑附上 `coverDataUrl`（取自首曲的正面封面）。默认 `false`。 |
| `coverMaxSize` | `integer` | 否 | 内联封面的上限，单位 KiB；更大的封面不附带。`0` 或负数不限大小。默认 `500`。 |
| `useCache` | `boolean` | 否 | 有保留的列表时直接用它回答；为 `false` 时总是扫描媒体库，结果照样保留。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `albums` | `AlbumInfo[]` | 这一页的专辑，按 `sort` 排序。 |
| `albums[].name` | `string` | 专辑名。 |
| `albums[].artist` | `string` | `albumArtist` 有值时为它，否则为这些曲目里找到的第一个 `artist` 值。 |
| `albums[].albumArtist` | `string` | 这些曲目里找到的第一个 `album artist` 值，没有时回退到曲目的 `artist`；两者都没有时为空。 |
| `albums[].trackCount` | `integer` | 计入该行的曲目数。 |
| `albums[].discCount` | `integer` | 这些曲目里不同碟号的个数；都没有碟号时为 `1`。 |
| `albums[].duration` | `number` | 这些曲目的总时长，单位秒。 |
| `albums[].year` | `string` | 这些曲目里找到的第一个 `date` 值；都没有时为空。 |
| `albums[].genre` | `string` | 这些曲目里找到的第一个 `genre` 值；都没有时为空。 |
| `albums[].label` | `string` | 这些曲目里找到的第一个 `publisher` 值，没有时取第一个 `label` 值；都没有时为空。 |
| `albums[].firstTrackPath` | `string` | 计入的第一首曲目的路径（foobar2000 存储的形式），可能是 `file-relative://` URI。 |
| `albums[].firstTrackAbsolutePath` | `string` | `firstTrackPath` 的本地文件路径形式，交给 `artwork.getForTrack` 的应是它；没有首曲路径时不出现。 |
| `albums[].coverDataUrl` | `string` | 正面封面的 `data:image/...` URL；只有带 `includeCover` 的 `library.getAlbums` 会填，且仅在有封面时。 |
| `albums[].tracks` | `AlbumTrackRef[]` | 专辑的曲目，按曲号排序；只有带 `includeTracks` 的 `library.getAlbums` 会填。 |
| `albums[].tracks[].trackNumber` | `integer` | 曲号；没有时为 `0`。 |
| `albums[].tracks[].path` | `string` | 曲目路径（foobar2000 存储的形式）。 |
| `albums[].tracks[].absolutePath` | `string` | `path` 的本地文件路径形式。 |
| `total` | `integer` | 符合 `query` 的专辑数（`offset` 与 `limit` 之前）。 |
| `offset` | `integer` | 生效的 `offset`。 |
| `limit` | `integer` | 生效的 `limit`。 |
| `hasMore` | `boolean` | 这一页之后是否还有专辑。 |
| `includeCover` | `boolean` | 生效的 `includeCover`。 |
| `fromCache` | `boolean` | 结果来自保留的列表时为 `true`。 |

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
一行 `library.getAlbums` 专辑的曲目，用该行的 `name` 与 `albumArtist` 指定。曲目的归组与 `library.getAlbums` 完全相同，所以 `total` 等于该行的 `trackCount`。排序先按碟号，再按曲号，再按媒体库顺序。归组结果保留到媒体库变化为止，`library.getAlbums` 建立并读取的也是这一份，所以在它之后或前一次调用之后，不必再读一遍媒体库。没有哪一行是这个名称与专辑艺术家时，以及媒体库未启用时，都成功返回空列表，且没有 `row`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `album` | `string` | 是 | 该行的 `name`：其曲目的第一个 `album` 值，逐字节比较。 |
| `albumArtist` | `string` | 是 | 该行的 `albumArtist`：其曲目的第一个 `album artist` 值；没有 `album artist` 时为第一个 `artist` 值；两者都没有时为 `""`。逐字节比较，所以不能用该行的 `artist` 代替。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `album` | `string` | 原样的专辑名。 |
| `albumArtist` | `string` | 原样的专辑艺术家。 |
| `row` | `AlbumInfo` | 该专辑在 `library.getAlbums` 里的那一行，不带 `coverDataUrl` 与 `tracks`；没有哪一行是这个名称与专辑艺术家时没有这个键。 |
| `row.name` | `string` | 专辑名。 |
| `row.artist` | `string` | `albumArtist` 有值时为它，否则为这些曲目里找到的第一个 `artist` 值。 |
| `row.albumArtist` | `string` | 这些曲目里找到的第一个 `album artist` 值，没有时回退到曲目的 `artist`；两者都没有时为空。 |
| `row.trackCount` | `integer` | 计入该行的曲目数。 |
| `row.discCount` | `integer` | 这些曲目里不同碟号的个数；都没有碟号时为 `1`。 |
| `row.duration` | `number` | 这些曲目的总时长，单位秒。 |
| `row.year` | `string` | 这些曲目里找到的第一个 `date` 值；都没有时为空。 |
| `row.genre` | `string` | 这些曲目里找到的第一个 `genre` 值；都没有时为空。 |
| `row.label` | `string` | 这些曲目里找到的第一个 `publisher` 值，没有时取第一个 `label` 值；都没有时为空。 |
| `row.firstTrackPath` | `string` | 计入的第一首曲目的路径（foobar2000 存储的形式），可能是 `file-relative://` URI。 |
| `row.firstTrackAbsolutePath` | `string` | `firstTrackPath` 的本地文件路径形式，交给 `artwork.getForTrack` 的应是它；没有首曲路径时不出现。 |
| `row.coverDataUrl` | `string` | 正面封面的 `data:image/...` URL；只有带 `includeCover` 的 `library.getAlbums` 会填，且仅在有封面时。 |
| `row.tracks` | `AlbumTrackRef[]` | 专辑的曲目，按曲号排序；只有带 `includeTracks` 的 `library.getAlbums` 会填。 |
| `row.tracks[].trackNumber` | `integer` | 曲号；没有时为 `0`。 |
| `row.tracks[].path` | `string` | 曲目路径（foobar2000 存储的形式）。 |
| `row.tracks[].absolutePath` | `string` | `path` 的本地文件路径形式。 |
| `tracks` | `LibraryTrack[]` | 曲目，先按碟号、再按曲号、再按媒体库顺序排序；`index` 是在这个列表中的位置。 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `items` | `LibraryTrack[]` | 与 `tracks` 相同的列表。 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `total` | `integer` | 曲目数。 |

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
每位署名艺术家及其参与计数：多值 `artist` 的每个值各成一行。扫描结果保留到媒体库变化为止。媒体库未启用时以 `LIBRARY_DISABLED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `sort` | `string` | 否 | `name`、`trackCount` 或 `albumCount`（后两者从大到小）；其他值保持名称顺序。默认 `"name"`。 |
| `limit` | `integer` | 否 | 最多返回的艺术家数。不小于 `0`。默认 `1000`。 |
| `includeAlbums` | `boolean` | 否 | 为每位艺术家附上 `albums`。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `ArtistInfo[]` | 艺术家，按 `sort` 排序。 |
| `items[].name` | `string` | 艺术家名。 |
| `items[].albumCount` | `integer` | 该艺术家曲目中不同专辑名的个数。 |
| `items[].trackCount` | `integer` | 该艺术家署名的曲目数。 |
| `items[].totalDuration` | `number` | 这些曲目的总时长，单位秒。 |
| `items[].albums` | `ArtistAlbumRef[]` | 该艺术家署名的专辑，按名称再按艺术家排序；只在带 `includeAlbums` 时出现，且不受 `limit` 截断。 |
| `items[].albums[].name` | `string` | 专辑名。 |
| `items[].albums[].artist` | `string` | 第一个 `album artist` 值；没有时为第一个 `artist` 值。 |
| `count` | `integer` | 返回的行数（`limit` 截断之后）；没有总数，满页即表示可能还有更多。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 每位参与艺术家各成一个条目，一首多艺术家曲目会计进其中每一位。`trackCount` 是参与曲目数，各条目相加会大于曲目总数；`albumCount` 与 `totalDuration` 同样按每位艺术家重复计入。

> 条目的 `name` 是单个原子值，可直接传给 `library.getArtistAlbums` 或 `library.getArtistTracks`。曲目对象上拼接后的 `artist` 串不行：宿主按每个原子值比较，多值曲目上拼接串命中不了；单值曲目上它恰好等于那个原子值，看着能用，别依赖。

> `albums` 在本端点本来就要做的那一遍扫描里给出整个媒体库的「艺术家 → 专辑」映射；逐位艺术家调 `library.getArtistAlbums` 则是每位艺术家各扫一遍。每个元素的 `(name, artist)` 就是 `library.getAlbums` 分组用的身份——`artist` 取 `album artist` 首值，缺则取 `artist` 首值——因此恰好对上 `library.getAlbums` 的一行，再由那一行的 `firstTrackAbsolutePath` 取封面。没有 `album` 标签的曲目在这里不计，与 `library.getAlbums` 一致。元素按 `(name, artist)` 去重，先按 `name`、再按 `artist` 的字节序排列。`albumCount` 保持自己的口径，只按专辑名去重：同名但专辑艺术家不同的两张专辑，`albumCount` 算 1，`albums` 里是 2 条。不传 `includeAlbums` 时响应与旧版完全相同——不会出现这个键。

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
某位艺术家署名的曲目，按媒体库顺序。`artist` 的匹配方式与 `library.getArtistAlbums` 在 `match: 'exact'` 下相同。`artist` 为空与媒体库未启用时都成功返回空列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `artist` | `string` | 否 | 艺术家名，与 `artist` 标签的每个值逐字节比较；为空时成功返回空列表。默认 `""`。 |
| `limit` | `integer` | 否 | 最多返回的曲目数；保留按媒体库顺序的前若干首。不小于 `0`。默认 `500`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artist` | `string` | 原样的艺术家名。 |
| `tracks` | `LibraryTrack[]` | 曲目，按媒体库顺序；`index` 是在这个列表中的位置。 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `items` | `LibraryTrack[]` | 与 `tracks` 相同的列表。 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `total` | `integer` | 与 `count` 相同；没有 `limit` 之前的总数，满页即表示可能还有更多。 |
| `count` | `integer` | 返回的曲目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('library.getArtistTracks', { artist: 'The Beatles' });
if (res.success === false) throw new Error(res.error);
const { items } = res;
```

### library.getArtistAlbums

<!-- api-schema:begin library.getArtistAlbums -->
某位艺术家参与的专辑。`trackCount`、`duration` 与 `discCount` 只统计该艺术家的曲目，所以该艺术家只参与了一部分的专辑，数字会比 `library.getAlbums` 给的小。行只按专辑名分组；没有 `album` 标签的曲目归入 `(Unknown Album)`。媒体库未启用时以 `LIBRARY_DISABLED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `artist` | `string` | 是 | 艺术家名，与 `artist` 标签的每个值逐字节比较，所以 `library.getArtists` 给出的名字即使不是多位艺术家中的第一位也能命中。不能为空。 |
| `limit` | `integer` | 否 | 最多返回的专辑数，在分组之后截断；没有 offset。不小于 `0`。默认 `100`。 |
| `sort` | `string` | 否 | `name`、`artist`、`year` 或 `trackCount`，与 `library.getAlbums` 相同；其他值按名称排序。默认 `"name"`。 |
| `match` | `"exact" \| "substring"` | 否 | `exact` 要求某个标签值整个等于 `artist`；`substring` 匹配包含它的标签值，此时 `artist` 里的小写字母匹配任意大小写，大写字母只匹配大写。默认 `"exact"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `artist` | `string` | 原样的艺术家名。 |
| `albums` | `AlbumInfo[]` | 专辑，按 `sort` 排序。行里不会有 `coverDataUrl` 与 `tracks`。 |
| `albums[].name` | `string` | 专辑名。 |
| `albums[].artist` | `string` | `albumArtist` 有值时为它，否则为这些曲目里找到的第一个 `artist` 值。 |
| `albums[].albumArtist` | `string` | 这些曲目里找到的第一个 `album artist` 值，没有时回退到曲目的 `artist`；两者都没有时为空。 |
| `albums[].trackCount` | `integer` | 计入该行的曲目数。 |
| `albums[].discCount` | `integer` | 这些曲目里不同碟号的个数；都没有碟号时为 `1`。 |
| `albums[].duration` | `number` | 这些曲目的总时长，单位秒。 |
| `albums[].year` | `string` | 这些曲目里找到的第一个 `date` 值；都没有时为空。 |
| `albums[].genre` | `string` | 这些曲目里找到的第一个 `genre` 值；都没有时为空。 |
| `albums[].label` | `string` | 这些曲目里找到的第一个 `publisher` 值，没有时取第一个 `label` 值；都没有时为空。 |
| `albums[].firstTrackPath` | `string` | 计入的第一首曲目的路径（foobar2000 存储的形式），可能是 `file-relative://` URI。 |
| `albums[].firstTrackAbsolutePath` | `string` | `firstTrackPath` 的本地文件路径形式，交给 `artwork.getForTrack` 的应是它；没有首曲路径时不出现。 |
| `albums[].coverDataUrl` | `string` | 正面封面的 `data:image/...` URL；只有带 `includeCover` 的 `library.getAlbums` 会填，且仅在有封面时。 |
| `albums[].tracks` | `AlbumTrackRef[]` | 专辑的曲目，按曲号排序；只有带 `includeTracks` 的 `library.getAlbums` 会填。 |
| `albums[].tracks[].trackNumber` | `integer` | 曲号；没有时为 `0`。 |
| `albums[].tracks[].path` | `string` | 曲目路径（foobar2000 存储的形式）。 |
| `albums[].tracks[].absolutePath` | `string` | `path` 的本地文件路径形式。 |
| `total` | `integer` | 找到的专辑数（`limit` 截断之前）。 |
| `hasMore` | `boolean` | `limit` 是否截掉了一部分。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> 行的键集与 `library.getAlbums` 的行相同，只少 `coverDataUrl` 与 `tracks`：`name`、`artist`、`albumArtist`、`trackCount`、`discCount`、`duration`、`year`、`genre`、`label`、`firstTrackPath` 与 `firstTrackAbsolutePath`。要封面就把 `firstTrackAbsolutePath` 交给 `artwork.getForTrack`——`firstTrackPath` 可能是 `file-relative://` 形态，那种形态该端点不受理。端点自己报的失败（媒体库未启用、查询出错）仍带空的 `albums`；参数错误不带。

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
媒体库里的全部流派及各自的曲目数，按名称排序。多值 `genre` 的每个值各成一个条目，标了多个流派的曲目会计进其中每一个。媒体库未启用时以 `LIBRARY_DISABLED` 失败。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `genres` | `LibraryValueCount[]` | 全部流派及各自的曲目数。 |
| `genres[].name` | `string` | 取值。 |
| `genres[].trackCount` | `integer` | 带有该取值的曲目数。 |

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
从整个媒体库随机抽取、互不重复的曲目，每次调用重新抽取。媒体库未启用或为空时成功返回空列表。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `count` | `integer` | 否 | 抽取的曲目数；最多返回媒体库的曲目总数。不小于 `0`。默认 `10`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `LibraryTrack[]` | 抽到的曲目；`index` 是在这个列表中的位置。 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `count` | `integer` | 返回的曲目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('library.getRandomTracks', { count: 20 });
if (res.success === false) throw new Error(res.error);
const { tracks } = res;
```

### library.getRecentlyAdded

<!-- api-schema:begin library.getRecentlyAdded -->
媒体库里最新的曲目。`added` 按 foo_playcount 的 `%added%` 字段排序；没有任何曲目带这个字段时改按文件修改时间排序，并以 `fallback` 说明。每次调用都会遍历整个媒体库。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `limit` | `integer` | 否 | 最多返回的曲目数。不小于 `0`。默认 `50`。 |
| `sortBy` | `"added" \| "modified"` | 否 | `added` 按 `%added%` 从新到旧排序，没有该值的曲目排在最后；`modified` 按文件修改时间从新到旧排序。默认 `"added"`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `RecentLibraryTrack[]` | 曲目，从新到旧；`index` 是在媒体库中的位置。 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `tracks[].added` | `string` | foo_playcount 格式化的 `%added%` 值，如 `2024-05-01 12:34:56`；只在列表按它排序且该曲目有值时出现。 |
| `tracks[].modified` | `integer` | 文件修改时间，自 Unix 纪元起的秒数；只在列表按它排序且时间已知时出现。 |
| `total` | `integer` | 媒体库曲目数，不是返回的曲目数。 |
| `limit` | `integer` | 生效的 `limit`。 |
| `sortBy` | `"added" \| "modified"` | 实际采用的排序：请求 `added` 而没有任何曲目带 `%added%` 时为 `modified`。 |
| `fallback` | `boolean` | 是否从 `added` 回退到了 `modified`。 |

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
媒体库的根文件夹。只有能解析为稳定本地路径的曲目才计入根目录；`http://`、`file-relative://`、`unpack://`、`archive://` 等计入 `skippedTracks`。首次调用同步构建索引，之后复用，直到媒体库变化或调用 `library.invalidateCache`。媒体库未启用时成功返回 `enabled: false` 与空根目录；构建失败以 `OPERATION_FAILED` 失败。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 媒体库是否启用。 |
| `roots` | `LibraryRootInfo[]` | 根文件夹。 |
| `roots[].id` | `string` | 根目录的稳定标识，当前就是 `absolutePath`；`library.browseTree` 用它。 |
| `roots[].displayName` | `string` | 文件夹名；两个根目录同名时为完整路径。 |
| `roots[].rawPath` | `string` | 当前与 `absolutePath` 相同。 |
| `roots[].absolutePath` | `string` | 文件夹的规范化本地路径。 |
| `roots[].trackCount` | `integer` | 该文件夹下的媒体库曲目数。 |
| `total` | `integer` | 根目录数。 |
| `indexedTracks` | `integer` | 归入某个根目录的曲目数。 |
| `skippedTracks` | `integer` | 因没有稳定本地路径而跳过的曲目数。 |
| `fromCache` | `boolean` | 本次调用之前索引已存在时为 `true`。 |

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
媒体库根目录下的一个文件夹：它的子文件夹，带 `includeFiles` 时还有它的曲目。读的是 `library.getRoots` 所用的目录树索引，需要时先构建。未知的 `rootId` 或 `pathId` 以 `NOT_FOUND` 失败；索引构建失败以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `rootId` | `string` | 是 | `library.getRoots` 给出的根目录 `id`，不区分大小写比较。不能为空。 |
| `pathId` | `string` | 否 | 要打开的文件夹的 `pathId`，取 `directories` 条目给出的值；为空时打开根目录。文件夹名之间用 `/` 分隔，所以用 `\` 写的路径找不到。默认 `""`。 |
| `includeFiles` | `boolean` | 否 | 附上文件夹的曲目 `files`。默认 `false`。 |
| `recursiveFiles` | `boolean` | 否 | 带 `includeFiles` 时，连同其下所有文件夹的曲目一并附上。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `root` | `LibraryRootInfo` | 根目录。 |
| `root.id` | `string` | 根目录的稳定标识，当前就是 `absolutePath`；`library.browseTree` 用它。 |
| `root.displayName` | `string` | 文件夹名；两个根目录同名时为完整路径。 |
| `root.rawPath` | `string` | 当前与 `absolutePath` 相同。 |
| `root.absolutePath` | `string` | 文件夹的规范化本地路径。 |
| `root.trackCount` | `integer` | 该文件夹下的媒体库曲目数。 |
| `pathId` | `string` | 打开的 `pathId`。 |
| `absolutePath` | `string` | 文件夹的本地路径。 |
| `directories` | `LibraryDirectoryNodeInfo[]` | 直接包含的子文件夹，按 `displayName` 再按 `absolutePath` 排序，不区分大小写。 |
| `directories[].id` | `string` | `rootId` 加 `::` 再加 `pathId`。 |
| `directories[].rootId` | `string` | 文件夹所在的根目录。 |
| `directories[].pathId` | `string` | 相对根目录的路径，文件夹名之间用 `/` 分隔；传给 `library.browseTree` 即可打开这个文件夹。 |
| `directories[].parentPathId` | `string` | 父文件夹的 `pathId`；直接位于根目录下时为 `""`。 |
| `directories[].name` | `string` | 文件夹名。 |
| `directories[].displayName` | `string` | 当前与 `name` 相同。 |
| `directories[].rawPath` | `string` | 当前与 `absolutePath` 相同。 |
| `directories[].absolutePath` | `string` | 文件夹的本地路径。 |
| `directories[].relativePath` | `string` | 当前与 `pathId` 相同。 |
| `directories[].depth` | `integer` | `pathId` 里的文件夹层数：直接位于根目录下时为 `1`。 |
| `directories[].trackCount` | `integer` | 该文件夹及其下所有文件夹里的媒体库曲目数。 |
| `directories[].childDirectoryCount` | `integer` | 直接包含的子文件夹数。 |
| `directories[].hasChildren` | `boolean` | `childDirectoryCount` 是否大于 `0`。 |
| `files` | `LibraryTrack[]` | 文件夹的曲目，按媒体库顺序；带 `recursiveFiles` 时其后接下层文件夹的曲目，顺序不固定；不带 `includeFiles` 时为空。`index` 是构建目录树索引时在媒体库中的位置。 |
| `files[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `files[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `fromCache` | `boolean` | 本次调用之前目录树索引已存在时为 `true`。 |

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
按路径前缀列出媒体库：`path` 下一层的文件夹，带 `includeFiles` 时还有它下面任意深度的全部曲目。前缀与 foobar2000 存储的路径比较，所以本地文件夹要写成 `file://` 加路径；普通绝对路径什么也匹配不到，成功返回空列表。要按真实的媒体库根目录浏览，用 `library.browseTree`。媒体库未启用时以 `LIBRARY_DISABLED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 路径前缀，ASCII 字母不区分大小写；为空时匹配全部曲目。默认 `""`。 |
| `includeFiles` | `boolean` | 否 | 附上 `path` 下的全部曲目 `files`。默认开启，所以 `path` 为空时返回整个媒体库。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `directories` | `string[]` | `path` 下一层的文件夹，是存储形式的路径，按字节序排列。 |
| `items` | `string[]` | 与 `directories` 相同的列表。 |
| `files` | `LibraryTrack[]` | `path` 下的曲目，按媒体库顺序；`index` 是在媒体库中的位置。不带 `includeFiles` 时为空。 |
| `files[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `files[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const root = await fb2k.invoke('library.browseDirectory', { includeFiles: false });
```

## 搜索

### library.search

<!-- api-schema:begin library.search -->
符合 foobar2000 查询表达式的曲目中的一页，按媒体库顺序：`SORT BY` 子句会被接受但不生效。解析器拒绝的表达式以 `INVALID_PARAMS` 失败并带 `details.param` 为 `query`。`query` 为空与媒体库未启用时都成功返回空列表。各行在主线程之外写出。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 否 | foobar2000 查询表达式；为空时成功返回空列表。默认 `""`。 |
| `offset` | `integer` | 否 | 跳过的匹配数。不小于 `0`。默认 `0`。 |
| `limit` | `integer` | 否 | 最多返回的行数。不小于 `0`。默认 `100`。 |
| `fields` | `string[]` | 否 | 每行带的键，与 `library.query` 相同。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `LibraryTrackPartial[]` | 这一页匹配的曲目，按媒体库顺序；`index` 是在全部匹配中的位置。每行恰好带 `fields` 要求的键。 |
| `tracks[].handle` | `string` | 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `\|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。 |
| `tracks[].path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `tracks[].absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `tracks[].subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `tracks[].title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `tracks[].artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `tracks[].album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `tracks[].albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].albumArtists` | `string[]` | 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。 |
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
| `tracks[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `total` | `integer` | 匹配数（`offset` 与 `limit` 之前）。 |
| `offset` | `integer` | 生效的 `offset`。 |
| `limit` | `integer` | 生效的 `limit`。 |
| `hasMore` | `boolean` | 这一页之后是否还有匹配。 |

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
符合 foobar2000 查询表达式的曲目，可按 `sort` 排序，最多 `limit` 行。解析器拒绝的表达式以 `INVALID_PARAMS` 失败并带 `details.param` 为 `query`，带 `SORT BY` 的同样如此；很多畸形表达式并不会被拒绝，只是什么也匹配不到。各行在主线程之外写出。媒体库未启用时以 `LIBRARY_DISABLED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | foobar2000 查询表达式。不能为空。 |
| `sort` | `string` | 否 | 在截断到 `limit` 之前用来排序的 Title Formatting 表达式；为空时保持媒体库顺序，编译失败的表达式被忽略。默认 `""`。 |
| `limit` | `integer` | 否 | 最多返回的行数；`total` 仍计入全部匹配。不小于 `0`。默认 `100`。 |
| `fields` | `string[]` | 否 | 每行带的键；省略时为媒体库曲目行的全部键。名字与这些键按大小写精确比较：`index`、`handle`、`title`、`artist`、`artists`、`album`、`albumArtist`、`albumArtists`、`genre`、`date`、`trackNumber`、`discNumber`、`duration`、`path`、`absolutePath`、`fileSize`、`bitrate`、`sampleRate`、`channels`、`codec`、`subsong`、`rating`。未知的名字以 `INVALID_PARAMS` 失败，并列在 `details.unknownFields` 里。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracks` | `LibraryTrackPartial[]` | 匹配的曲目，按 `sort` 排序或按媒体库顺序；`index` 是在这个列表中的位置。每行恰好带 `fields` 要求的键。 |
| `tracks[].handle` | `string` | 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `\|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。 |
| `tracks[].path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `tracks[].absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `tracks[].subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `tracks[].title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `tracks[].artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `tracks[].album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `tracks[].albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].albumArtists` | `string[]` | 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。 |
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
| `tracks[].index` | `integer` | 行号；数的是什么由所在的列表说明。 |
| `total` | `integer` | 匹配数（`limit` 截断之前）。 |

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

## 字段投影 {#field-projection}

`library.query` 与 `library.search` 支持可选参数 `fields`，用来限定每行返回哪些曲目字段。省略 `fields` 或传 `null` 时，每行输出媒体库曲目行的全部键。

传 `fields` 时，每行**恰好**包含请求的那几个键，不附带任何未请求字段；元数据容器读取失败的条目同样输出全部请求键，取自元数据的字段以类型默认值填充（空串、0），保证"请求键必在"。响应信封不受影响：`library.query` 仍返回 `success` / `tracks` / `total`，`library.search` 仍返回 `success` / `tracks` / `total` / `offset` / `limit` / `hasMore`。

**可用字段名**（精确匹配、大小写敏感）：

`index`、`handle`、`title`、`artist`、`artists`、`album`、`albumArtist`、`albumArtists`、`genre`、`date`、`trackNumber`、`discNumber`、`duration`、`path`、`absolutePath`、`fileSize`、`bitrate`、`sampleRate`、`channels`、`codec`、`subsong`、`rating`

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
按路径把曲目追加到播放列表。路径不必在媒体库里，不存在的文件也照样添加。已上锁的列表以 `LOCKED` 失败，不添加任何曲目。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要添加的路径，按此顺序；`\|subsong:N` 后缀选子曲目。不能为空。 |
| `playlist` | `integer` | 否 | 目标播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的 GUID，用来代替序号；写法带花括号，与 `playlist.getAll` 以及所有指明播放列表的结果、事件报出的一致；十六进制大小写均可。别的列表增删或重排时它仍指向同一个列表，序号做不到。同时给了它和序号，或 GUID 格式不对，以 `INVALID_PARAMS` 失败。列表已不存在时以 `NOT_FOUND` 失败，不会改用别的列表。GUID 格式不对与列表不存在这两种失败都以 `details.playlistGuid` 带回传入的 GUID。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `added` | `integer` | 添加的曲目数。 |

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
调用 `library_manager::rescan()` 请求 foobar2000 重新扫描媒体库文件夹；foobar2000 SDK 已把该方法标为过时、不应调用。媒体库本身会跟踪文件变化。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('library.rescan');
```

### library.refresh

<!-- api-schema:begin library.refresh -->
同 `library.rescan`。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 缓存控制

> 自动失效: 媒体库变化时缓存自动失效（通过 `library_callback_v2` 监听）。

### library.invalidateCache

<!-- api-schema:begin library.invalidateCache -->
丢弃宿主缓存的媒体库结果与目录树索引，下次调用用到它们的端点时重建。媒体库发生变化时也会自动丢弃。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `timestamp` | `integer` | 缓存被丢弃的时间，自 Unix 纪元起的毫秒数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('library.invalidateCache');
const albums = await fb2k.invoke('library.getAlbums', { useCache: true });
```

### library.getCacheStats

<!-- api-schema:begin library.getCacheStats -->
宿主媒体库缓存与目录树索引的计数，供诊断用。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `valid` | `boolean` | 自上次丢弃缓存以来是否保留过任何媒体库结果。 |
| `lastModified` | `integer` | 缓存上次被丢弃的时间，自 Unix 纪元起的毫秒数。 |
| `albumsCacheEntries` | `integer` | 保留的 `library.getAlbums` 列表数，每种 `query`、`sort`、`includeCover` 组合一份。 |
| `tracksCached` | `boolean` | 是否保留了完整的 `library.getAll` 结果。 |
| `artistsCached` | `boolean` | 是否保留了 `library.getArtists` 的扫描结果。 |
| `genresCached` | `boolean` | 恒为 `false`；流派结果不保留。 |
| `statsCached` | `boolean` | 是否保留了 `library.getStatus` 的结果。 |
| `coversCached` | `integer` | 恒为 `0`；封面不保留。 |
| `coverCacheBytes` | `integer` | 恒为 `0`；封面不保留。 |
| `coverCacheMB` | `number` | 恒为 `0`；封面不保留。 |
| `cacheHits` | `integer` | 自宿主启动以来由保留结果回答的查找次数。 |
| `cacheMisses` | `integer` | 自宿主启动以来没有找到保留结果的查找次数。 |
| `treeIndexValid` | `boolean` | 目录树索引是否已构建。 |
| `rootsCached` | `integer` | 目录树索引里的根目录数；未构建时为 `0`。 |
| `treeIndexedTracks` | `integer` | 目录树索引上次构建时归入的曲目数。 |
| `treeSkippedTracks` | `integer` | 目录树索引上次构建时跳过的曲目数。 |
| `treeLastBuilt` | `integer` | 目录树索引上次构建的时间，自 Unix 纪元起的毫秒数；首次构建之前为 `0`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

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

<!-- api-schema:begin library.getFieldValues -->
某个标签在整个媒体库里的全部不同取值及各自的曲目数，用得最多的在前。媒体库未启用时以 `LIBRARY_DISABLED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `field` | `string` | 是 | 标签名，如 `genre`。不能为空。 |
| `separator` | `string` | 否 | 按此字符串把每个标签值再拆成多个值，并去掉各段前后的空格与制表符；为空时整值计入。默认 `""`。 |
| `limit` | `integer` | 否 | 最多返回的取值数。不小于 `0`。默认 `5000`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `values` | `LibraryValueCount[]` | 各取值，曲目数多的在前。 |
| `values[].name` | `string` | 取值。 |
| `values[].trackCount` | `integer` | 带有该取值的曲目数。 |
| `total` | `integer` | 找到的不同取值数（`limit` 截断之前）。 |
| `field` | `string` | 原样的标签名。 |

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
- `library.getStatus` 和 `library.getCount` 通过 `enum_items` 枚举，不会返回 `metadb_handle_list`。`library_callback_v2` 回调会先使缓存失效，再广播下列事件。

## 媒体库事件 Contract

四个事件均广播到每个 WebView。

| 事件 | Payload |
| --- | --- |
| `library:itemsAdded` | `{ count, timestamp }` |
| `library:itemsRemoved` | `{ count, timestamp }` |
| `library:itemsModified` | `{ count, timestamp }` |
| `library:initialized` | `{ timestamp }` |
