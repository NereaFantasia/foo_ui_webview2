# Playlist API

播放列表管理、曲目操作、智能播放列表和工具函数。共 47 个 API。

> **参数兼容性**: 所有 Playlist API 同时支持 `playlist` 和 `index` 参数名指定播放列表索引。

## 列表管理

### playlist.getCount

获取播放列表数量。

- **参数**: 无
- **返回值**: `{ "count": 5 }`

```javascript
const { count } = await fb2k.invoke('playlist.getCount');
```

### playlist.getAll


获取所有播放列表信息。

- **参数**: 无

**返回值**:

```json
[
    {
        "index": 0,
        "name": "Default",
        "trackCount": 150,
        "isActive": true,
        "isPlaying": true,
        "isLocked": false,
        "isAutoplaylist": false
    }
]
```

::: warning Breaking Change (v1.1.18)
`playlist.getAll` 不再返回 `duration` 字段（避免 N 个播放列表 × M 首曲目的全量加载开销）。如需获取单个播放列表的 duration，请使用 `playlist.getActive` 或 `playlist.getPlaying`。
:::

::: tip v1.1.18+
`isAutoplaylist` 字段已内联到 `playlist.getAll` 返回值中，无需再逐个调用 `playlist.isAutoplaylist`。
:::

### playlist.getActive

获取当前激活的播放列表。包含 `duration` 字段。

- **参数**: 无

**返回值**: `{"duration":"...","found":true,"index":0,"isActive":true,"isLocked":true,"isPlaying":true,"name":"...","success":true,"trackCount":"..."}`


> 无激活播放列表时返回 `{ "success": true, "found": false }`。

### playlist.setActive


设置激活的播放列表。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 是 | — | 要激活的播放列表索引；不回退活动播放列表，省略即报错。 |

**返回值**: `{ "success": true, "error": "..." }`

```javascript
await fb2k.invoke('playlist.setActive', { playlist: 1 });
```

### playlist.getPlaying

获取当前正在播放的播放列表。包含 `duration` 字段。

- **参数**: 无

**返回值**: `{"duration":"...","found":true,"index":0,"isActive":true,"isLocked":true,"isPlaying":true,"name":"...","success":true,"trackCount":"..."}`


> 无正在播放的播放列表时返回 `{ "success": true, "found": false }`。

### playlist.create


创建新的播放列表。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `name` | `string` | 否 | `New Playlist` |  |
| `position` | `integer` | 否 | — | 插入位置，省略时追加到末尾。 |

**返回值**: `{ "success": true, "index": 2 }`

```javascript
const result = await fb2k.invoke('playlist.create', { name: 'Rock Music' });
```

### playlist.remove


删除播放列表。如果播放列表被锁定则无法删除。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{ "success": true, "error": "..." }`

### playlist.rename


重命名播放列表。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 是 | — | 要重命名的播放列表索引；不回退活动播放列表，省略即报错。 |
| `name` | `string` | 否 | — | 新名称。 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.rename', { playlist: 0, name: 'My Favorites' });
```

### playlist.clear


清空播放列表中的所有曲目。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**:

```json
{
    "success": true,
    "playlist": 0,
    "clearedCount": 22,
    "remainingCount": 0
}
```

### playlist.duplicate


复制播放列表。新列表插入到源列表后方。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `name` | `string` | 否 | 源名称 + ` (Copy)` |  |

**返回值**: `{ "success": true, "index": 1, "sourcePlaylist": 0, "newPlaylist": 1, "name": "Default (Copy)", "trackCount": 150 }`

## 曲目操作

### playlist.getTrackCount


获取播放列表中的曲目数量。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |

**返回值**: `{ "count": 150 }`

### playlist.getTracks


获取播放列表中的曲目列表（分页）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |
| `start` | `integer` | 否 | `0` | 起始偏移。 |
| `count` | `integer` | 否 | `100` | 返回条数。 |
| `formats` | `object` | 否 | `{}` | 追加 TitleFormat 动态列（见下方提示）。 |
| `fields` | `string[]` | 否 | 全部字段 | 只返回这些字段（见下方提示）。 |

**返回值**:

```json
{
    "playlist": 0,
    "start": 0,
    "count": 20,
    "total": 150,
    "tracks": [
        {
            "index": 0,
            "title": "Song 1",
            "artist": "Artist 1",
            "album": "Album 1",
            "albumArtist": "Artist 1",
            "genre": "Rock",
            "date": "2024",
            "trackNumber": 1,
            "discNumber": 1,
            "duration": 180.5,
            "path": "file://C:/Music/song1.flac",
            "absolutePath": "C:\\Music\\song1.flac",
            "fileSize": 25600000,
            "subsong": 0,
            "rating": 5,
            "codec": "FLAC",
            "bitrate": 1411,
            "sampleRate": 44100,
            "channels": 2,
            "composer": "Lennon/McCartney",
            "comment": "",
            "playCount": "15",
            "firstPlayed": "2024-01-15 10:30:00",
            "lastPlayed": "2026-02-10 20:00:00",
            "added": "2024-01-10 08:00:00"
        }
    ]
}
```

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

> `rating` 优先读取 `%rating%`，采用 `1` 至 `5` 的有效值。该值缺失或越界时，改读文件的 `RATING` 标签并限制到 `0` 至 `5`；两处都无评分时返回 `0`（未评分）。`rating.get` 使用相同规则：采用 `%rating%` 值时 `storage` 为 `'stats'`，其余情况为 `'file'`，包括文件标签不存在的情况。

::: tip 自定义动态列 (`formats` 参数)
`playlist.getTracks` 支持通过 `formats` 参数追加任意 TitleFormat 动态列：

```javascript
const result = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 50,
    formats: {
        myRating: '%rating%',
        codec: '%codec%'
    }
});
// 每个 track 对象会额外包含 myRating 和 codec 字段
```
:::

::: tip
`absolutePath` 是本地文件系统路径，可直接用于 `artwork.getForTrack` 等 API。`path` 是 foobar2000 内部格式。
:::

::: tip 字段投影（`fields` 参数）
传入 `fields` 后只返回所选字段，`index` 始终保留。

```javascript
const page = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 200,
    fields: ['title', 'artist', 'album', 'duration', 'path']
});
```

可用的名字是 `index`、`title`、`artist`、`artists`、`album`、`albumArtist`、`genre`、`date`、`trackNumber`、`discNumber`、`duration`、`path`、`absolutePath`、`fileSize`、`bitrate`、`sampleRate`、`channels`、`codec`、`subsong` 与 `rating`。名字精确匹配且区分大小写，出现未知名整次调用以 `INVALID_PARAMS` 失败，并在 `details.unknownFields` 里列出它们——拼错不会静默丢字段。`composer` 与 `comment` 不接受，它们只在完全不投影时返回。`artists` 恰好相反：本端点只在投影下返回它，内容是 `artist` 背后的原子值数组。

省略 `absolutePath` 和 `rating` 可以避免逐行解析文件系统路径及求值 `%rating%`。投影时也不返回 foo_playcount 的四列（`playCount`、`firstPlayed`、`lastPlayed`、`added`）；需要这些列时，通过 `formats` 请求。
:::

### playlist.getGroupRuns

对整个播放列表分组，只返回分组的边界，不返回行。一段游程是分组键相同的相邻曲目的最长连续段，因此游程顺序就是列表顺序——不做任何重排。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `patterns` | `string[]` | 是 | — | 一条 TitleFormat 模式；给两条则在每段游程内再分一级。 |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的别名，`playlist` 存在时被忽略。 |

**返回值**:

```json
{
    "success": true,
    "playlist": 0,
    "total": 150,
    "runs": [
        { "start": 0, "count": 12, "key": "甲专辑 | 甲艺术家" },
        { "start": 12, "count": 9, "key": "乙专辑 | 乙艺术家" }
    ]
}
```

上例只展示前两个游程。成功响应覆盖整个播放列表：非空列表的 `runs[0].start` 为 0，相邻游程首尾相接，`count` 之和等于 `total`。空列表返回 `total: 0` 和 `runs: []`。

给两条模式时每段游程还带 `sub`，且子游程的 `start` 是**绝对行号**，不是父游程内的偏移：

```json
{
    "start": 12,
    "count": 9,
    "key": "乙专辑 | 乙艺术家",
    "sub": [
        { "start": 12, "count": 5, "key": "1" },
        { "start": 17, "count": 4, "key": "2" }
    ]
}
```

键的比较只对 ASCII `A-Z`/`a-z` 不区分大小写，其余内容逐字节比较。空键也是普通分组键：模式求值为空字符串时仍会形成游程，占位文案由界面处理。

`patterns` 缺失、不是数组、为空、多于两条、含非字符串或空串，以及模式编译失败，都返回 `INVALID_PARAMS`。只有具体条目出错时，`details.pattern` 才包含其下标；数组整体不合法时没有该下标。`playlist` 越界也返回 `INVALID_PARAMS`，但 `-1` 表示使用活动播放列表。

::: tip 驱动分组虚拟列表
与 `playlist.getTracks` 搭配：游程提供全部组头位置，界面据此计算滚动总高度，可见页的曲目行另行获取。响应大小取决于游程数量、分组键长度及二级分组，而非完整曲目元数据。若模式让每行单独成组，`runs` 就会与列表一样长，宿主对此不设上限。与同一列表的页响应比较 `total`，只能发现曲目数量变化；两者相等不能排除两次独立读取之间的等量替换或重排。
:::

### playlist.playTrack


播放播放列表中的指定曲目。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | `0` | 曲目序号；与 `track` 同义，优先生效。 |
| `track` | `integer` | 否 | `0` | `index` 的旧别名。 |
| `deferred` | `boolean` | 否 | `false` | 延迟执行，流媒体场景推荐。 |
| `muted` | `boolean` | 否 | `false` |  |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 5 });

// 延迟执行（流媒体场景推荐）
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 0, deferred: true });
```

### playlist.removeTracks


从播放列表中删除指定曲目。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |
| `items` | `array<integer>` | 否 | `[]` | 要删除的曲目索引数组。 |

**返回值**: `{ "success": true, "error": "..." }`

### playlist.removeSelectedTracks


删除播放列表中当前选中的曲目。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |

**返回值**: `{ "success": true }`

### playlist.moveTracks


移动曲目（向上或向下）。当 `items` 非空时会先设置选区再移动；当 `items` 为空时直接移动当前选区（SMP 兼容语义）。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |
| `items` | `array<integer>` | 否 | `[]` | 为空时移动当前选区（SMP 兼容语义）。 |
| `delta` | `integer` | 否 | `0` | 位移量，负数向上移动。 |

**返回值**: `{ "success": true, "error": "..." }`

```javascript
await fb2k.invoke('playlist.moveTracks', { items: [0, 1, 2], delta: 3 });
await fb2k.invoke('playlist.moveTracks', { items: [5, 6], delta: -2 });
```

### playlist.addPaths


添加文件/文件夹到播放列表。使用 `playlist_incoming_item_filter` 同步解析，自动展开 CUE 文件。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `paths` | `array<string>` | 是 | — | 文件或文件夹路径，可带 `\|subsong:N` 后缀。传空数组会失败并返回 `No paths specified`。 |

::: tip 顺序由 foobar2000 的 incoming item filter 决定，不是入参顺序
普通路径走的是 foobar2000 自己"添加文件"时的同一过滤器：去重，并按用户的"Sort incoming files by"偏好排序（偏好为空时按指针序）。因此加入的曲目**不一定**按你传入的顺序出现，`playlist.replaceAllAndPlay` 的 `playIndex` 指的是结果列表中的位置而不是 `paths` 的下标。需要严格保持入参顺序请用 `playlist.addPathsSequential`。带 `\|subsong:N` 后缀的条目不经过滤器，插在过滤后的批次之前。
:::

**返回值**:

```json
{
    "success": true,
    "playlist": 0,
    "requestedPaths": 25,
    "addedCount": 25,
    "invalidCount": 0,
    "countBefore": 0,
    "totalCount": 25
}
```

## 其他公开 API


### playlist.addHandles


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `handles` | `array<object \| string>` | 是 | — | 条目可为 `{ path, subsong }` 对象或 `path\|subsong:N` 字符串。 |

**返回值**: `{"addedCount":"...","countBefore":"...","error":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
await fb2k.invoke('playlist.addHandles', { handles: ['C:\\Music\\song.flac'] });
```


### playlist.addPathsAsync


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `paths` | `array<string>` | 是 | — | 文件或文件夹路径，可带 `\|subsong:N` 后缀。传空数组会失败并返回 `No paths specified`。 |

**返回值**: `{"error":"...","invalidCount":"...","operationId":"...","status":"...","success":true,"totalCount":"..."}`

本地文件与流媒体直链同步加入、不弹进度框；只有 `.pls` / `.m3u` / `.cue` 等播放列表容器走异步展开。`\|subsong:N` 后缀选择多轨文件的指定子曲目，而非默认的 subsong 0。

```js
const { operationId } = await fb2k.invoke('playlist.addPathsAsync', { paths: ['C:\\Music\\Album'] });
```


### playlist.addPathsSequential


严格按给定顺序追加。与 `playlist.addPaths` 不同，这里绕开 incoming item filter：不重排、不去重，同一文件传两次就加两次，文件夹或 CUE 在它于 `paths` 中的位置原地展开。`order` 按输入顺序给出新条目落到的播放列表下标。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `paths` | `array<string>` | 是 | — | 文件或文件夹路径，可带 `\|subsong:N` 后缀。传空数组会失败并返回 `No paths specified`。 |

**返回值**: `{"addedCount":"...","error":"...","order":"...","playlist":"...","success":true}`

```js
await fb2k.invoke('playlist.addPathsSequential', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```


### playlist.convertToAutoplaylist


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `query` | `string` | 是 | — | 过滤表达式。传空值会失败并返回 `Query is required`。 |
| `sort` | `string` | 否 | — | TitleFormat 排序模式。 |
| `keepSorted` | `boolean` | 否 | `false` | 保持按 `sort` 持续排序。 |

**返回值**: `{"error":"...","playlist":"...","success":true}`

```js
await fb2k.invoke('playlist.convertToAutoplaylist', { playlist: 0, query: '%genre% IS Rock' });
```


### playlist.createAutoplaylist


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `name` | `string` | 否 | `New Autoplaylist` |  |
| `query` | `string` | 是 | — | 过滤表达式。传空值会失败并返回 `Query is required`。 |
| `sort` | `string` | 否 | — | TitleFormat 排序模式。 |
| `keepSorted` | `boolean` | 否 | `false` | 保持按 `sort` 持续排序。 |

**返回值**: `{"error":"...","index":"...","name":"...","playlist":"...","query":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.createAutoplaylist', { name: 'Rock', query: '%genre% IS Rock' });
```


### playlist.deselectAll


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"success":true}`

```js
await fb2k.invoke('playlist.deselectAll', { playlist: 0 });
```


### playlist.focusTrack


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | 目标曲目序号；与 `track` 同义，优先生效。省略时表示无焦点项。 |
| `track` | `integer` | 否 | — | `index` 的旧别名。 |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.focusTrack', { playlist: 0, index: 3 });
```


### playlist.getAutoplaylistInfo

查询播放列表是否为智能播放列表；是则同时返回其排序与来源信息。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值** —— 智能播放列表：`{ "isAutoplaylist": true, "playlist": 0, "keepSorted": false, "source": "sdk" }`；其他情况：`{ "isAutoplaylist": false, "playlist": 0 }`，且不含 `keepSorted` 与 `source`。

`source` 为 `"sdk"` 或 `"dui"`；来源为 `dui` 时 `keepSorted` 恒为 `false`。`lockName` **只**伴随 `dui` 来源出现——通过 SDK 创建的智能播放列表即便处于锁定状态也不会带该字段。两个分支都**不返回** `success`，请改判 `isAutoplaylist`。`playlist` 越界时返回 `{ "success": false, "error": "Invalid playlist index" }`。

```js
const info = await fb2k.invoke('playlist.getAutoplaylistInfo', { playlist: 0 });
if (info.isAutoplaylist) console.log(info.source, info.keepSorted);
```


### playlist.getAutoplaylistQuery

查询智能播放列表的元信息。查询表达式本身无法读回。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值** —— 智能播放列表：`{ "isAutoplaylist": true, "playlist": 0, "query": null, "keepSorted": false, "source": "sdk", "note": "Query string not exposed by SDK" }`；其他情况：`{ "isAutoplaylist": false, "playlist": 0, "query": null }`。

`query` **恒为** `null`——foobar2000 不暴露筛选表达式，因此本方法无法用于读回查询串。`keepSorted`、`source`、`note` 仅在智能播放列表分支出现，`lockName` 只伴随 `dui` 来源出现。两个分支都**不返回** `success`。`playlist` 越界时返回 `{ "success": false, "error": "Invalid playlist index" }`。

```js
const q = await fb2k.invoke('playlist.getAutoplaylistQuery', { playlist: 0 });
// 即便 q.isAutoplaylist 为 true，q.query 仍为 null
```


### playlist.getAvailableColumns

列出 Default UI 提供的列，可用作 titleformat 模式。

_无参数。_

**返回值**: 一个裸 JSON 数组——不是信封结构，因此没有 `success` 字段。每个元素含 `id`、`name`、`pattern`、`alignment`（`left` / `right` / `center`）与 `numeric`；仅当该列定义了独立排序脚本时才有 `sortPattern`。未注册任何 provider 时返回空数组。

```js
const columns = await fb2k.invoke('playlist.getAvailableColumns');
columns.forEach((c) => console.log(c.name, c.pattern));
```


### playlist.getFocusTrack


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"error":"...","index":"...","playlist":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.getFocusTrack', { playlist: 0 });
```


### playlist.getFocusedTrack


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"index":"...","playlist":"...","success":true}`

```js
const { index } = await fb2k.invoke('playlist.getFocusedTrack', { playlist: 0 });
```


### playlist.getLockInfo


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"error":"...","isLocked":"...","playlist":"...","success":true}`

```js
const { isLocked } = await fb2k.invoke('playlist.getLockInfo', { playlist: 0 });
```


### playlist.getSelectedTracks


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |

**返回值**: `{"error":"...","success":true,"tracks":"..."}`

```js
const result = await fb2k.invoke('playlist.getSelectedTracks');
```


### playlist.getSelection


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"count":"...","error":"...","items":"...","playlist":"...","success":true}`

```js
const { items, count } = await fb2k.invoke('playlist.getSelection', { playlist: 0 });
```


### playlist.insertTracks


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `position` | `integer` | 否 | `0` | 插入位置；未传时回退到 `index`。 |
| `index` | `integer` | 否 | — | `position` 的旧别名。 |
| `handles` | `array<object \| string>` | 是 | — | 条目可为 `{ path, subsong }` 对象或 `path\|subsong:N` 字符串。 |

**返回值**: `{"addedCount":"...","countBefore":"...","error":"...","insertIndex":"...","invalidCount":"...","playlist":"...","requestedCount":"...","success":true,"totalCount":"..."}`

```js
const result = await fb2k.invoke('playlist.insertTracks', {
    playlist: 0,
    position: 5,
    handles: ['C:\\Music\\song.flac'],
});
```


### playlist.isAutoplaylist

判断播放列表是否为智能播放列表。

| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{ "playlist": 0, "isAutoplaylist": true }`

只要播放列表带具名锁就会附 `lockName`，与判定结果无关——因此与上面两个方法不同，它可能与 `isAutoplaylist: false` 同时出现（普通的锁定播放列表）。成功路径**不返回** `success`，请改判 `isAutoplaylist`。`playlist` 越界时返回 `{ "success": false, "error": "Invalid playlist index" }`。

```js
const { isAutoplaylist } = await fb2k.invoke('playlist.isAutoplaylist', { playlist: 0 });
```


### playlist.isLocked


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"error":"...","isLocked":"...","success":true}`

```js
const { isLocked } = await fb2k.invoke('playlist.isLocked', { playlist: 0 });
```


### playlist.redo


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.redo', { playlist: 0 });
```


### playlist.removeAutoplaylist


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"error":"...","note":"...","playlist":"...","source":"...","success":true}`

```js
await fb2k.invoke('playlist.removeAutoplaylist', { playlist: 0 });
```


### playlist.reorder


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `newOrder` | `array<integer>` | 是 | — | 该播放列表全部曲目索引的一个完整排列，长度必须等于当前条目数。 |

**返回值**: `{"error":"...","expected":"...","got":"...","index":"...","itemCount":"...","playlist":"...","success":true}`

```js
// newOrder 必须是该播放列表当前全部曲目索引的一个完整排列
await fb2k.invoke('playlist.reorder', { playlist: 0, newOrder: [2, 0, 1] });
```


### playlist.reorderPlaylists


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `newOrder` | `array<integer>` | 是 | 全部播放列表索引的一个完整排列，长度必须等于播放列表总数。 |

**返回值**: `{"count":"...","error":"...","expected":"...","got":"...","index":"...","success":true}`

```js
// newOrder 必须是现有播放列表索引的一个完整排列
await fb2k.invoke('playlist.reorderPlaylists', { newOrder: [2, 0, 1] });
```


### playlist.replaceAllAndPlay


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `paths` | `array<string>` | 是 | — | 文件或文件夹路径。传空数组会失败并返回 `No paths specified`。 |
| `playIndex` | `integer` | 否 | `0` | 添加完成后开始播放的曲目序号。 |
| `stopFirst` | `boolean` | 否 | `true` | 添加前先停止当前播放。 |
| `autoPlay` | `boolean` | 否 | `true` | 添加后自动开始播放。 |

**返回值**: `{"addedCount":"...","clearedCount":"...","error":"...","invalidCount":"...","playIndex":"...","playlist":"...","success":true,"totalCount":"..."}`

```js
await fb2k.invoke('playlist.replaceAllAndPlay', { paths: ['C:\\Music\\song.flac'] });
```


### playlist.reverse


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"success":true}`

```js
await fb2k.invoke('playlist.reverse', { playlist: 0 });
```


### playlist.selectAll


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"success":true}`

```js
await fb2k.invoke('playlist.selectAll', { playlist: 0 });
```


### playlist.setFocusedTrack


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | 目标曲目序号，省略时表示无焦点项。 |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.setFocusedTrack', { playlist: 0, index: 3 });
```


### playlist.setSelection


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |
| `indices` | `array<integer>` | 否 | `[]` | 要选中的曲目索引数组。 |
| `clearOthers` | `boolean` | 否 | `true` | 先清除已有选区。 |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.setSelection', { playlist: 0, indices: [0, 1, 2] });
```


### playlist.shuffle


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.shuffle', { playlist: 0 });
```


### playlist.sort


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |
| `index` | `integer` | 否 | — | `playlist` 的旧别名，`playlist` 存在时被忽略。 |
| `pattern` | `string` | 否 | `%title%` | TitleFormat 排序模式。 |
| `descending` | `boolean` | 否 | `false` | 降序排序。 |
| `selectedOnly` | `boolean` | 否 | `false` | 仅排序选中的曲目。 |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.sort', { playlist: 0, pattern: '%artist% - %title%' });
```


### playlist.undo


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 活动播放列表 |  |

**返回值**: `{"error":"...","success":true}`

```js
await fb2k.invoke('playlist.undo', { playlist: 0 });
```

## 关联的播放列表事件

以下播放列表生命周期事件会被广播。JIT 队列 shadow playlist 的条目级事件被有意忽略。

| 事件 | 触发时机 | Payload keys |
| --- | --- | --- |
| `playlist:itemsAdded` | 条目插入播放列表后。 | `{ playlist, start, count }` |
| `playlist:itemsRemoved` | 条目从播放列表移除后。 | `{ playlist, oldCount, newCount }` |
| `playlist:itemsReordered` | 单个播放列表中的条目重排后。 | `{ playlist, count }` |
| `playlist:selectionChanged` | 播放列表选择变化后。 | `{ playlist }` |
| `playlist:focusChanged` | 播放列表条目焦点变化后。 | `{ playlist, from, to }` |
| `playlist:itemsReplaced` | 播放列表条目替换后。 | `{ playlist, count }` |
| `playlist:created` | 新建播放列表后。 | `{ index, name }` |
| `playlist:removed` | 删除播放列表后。 | `{ oldCount, newCount }` |
| `playlist:reordered` | 播放列表集合重排后。 | `{ count }` |
| `playlist:activated` | 活动播放列表变化后。 | `{ oldIndex, newIndex }` |
| `playlist:renamed` | 播放列表重命名后。 | `{ index, name }` |
| `playlist:lockChanged` | 播放列表锁状态变化后。 | `{ playlist, locked }` |
| `playlist:defaultFormatChanged` | 默认播放列表格式变化后。 | `{}` |
| `playlist:addComplete` | 异步路径添加操作完成后。 | `{ operationId, success, addedCount, totalCount }` |

当 foobar2000 回调没有具体的前一个或后一个索引时，`from`、`to`、`oldIndex` 与 `newIndex` 可以为 `-1`。
