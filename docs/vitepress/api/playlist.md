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

::: tip v1.1.18 新增
`isAutoplaylist` 字段已内联到 `playlist.getAll` 返回值中，无需再逐个调用 `playlist.isAutoplaylist`。
:::

### playlist.getActive 

获取当前激活的播放列表。包含 `duration` 字段。

- **参数**: 无

**返回值**:

```json
{
    "index": 0,
    "name": "Default",
    "trackCount": 150,
    "isActive": true,
    "isPlaying": true,
    "isLocked": false,
    "duration": 36000.5
}
```

> 无激活播放列表时返回 `{ "success": true, "found": false }`。

### playlist.setActive 

设置激活的播放列表。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✓ | 播放列表索引 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.setActive', { playlist: 1 });
```

### playlist.getPlaying 

获取当前正在播放的播放列表。包含 `duration` 字段。

- **参数**: 无

**返回值**:

```json
{
    "index": 0,
    "name": "Default",
    "trackCount": 150,
    "isActive": false,
    "isPlaying": true,
    "isLocked": false,
    "duration": 36000.5
}
```

> 无正在播放的播放列表时返回 `{ "success": true, "found": false }`。

### playlist.create 

创建新的播放列表。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| name | string | ✗ | 播放列表名称（默认 "New Playlist"） |
| position | number | ✗ | 插入位置（默认末尾） |

**返回值**: `{ "success": true, "index": 2 }`

```javascript
const result = await fb2k.invoke('playlist.create', { name: 'Rock Music' });
```

### playlist.remove 

删除播放列表。如果播放列表被锁定则无法删除。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✓ | 播放列表索引 |

**返回值**: `{ "success": true }`

### playlist.rename 

重命名播放列表。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✓ | 播放列表索引 |
| name | string | ✓ | 新名称 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.rename', { playlist: 0, name: 'My Favorites' });
```

### playlist.clear 

清空播放列表中的所有曲目。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引（默认当前激活） |

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

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 源播放列表索引（默认当前激活） |
| name | string | ✗ | 新列表名称（默认 "原名 (Copy)"） |

**返回值**: `{ "success": true, "index": 1, "sourcePlaylist": 0, "newPlaylist": 1, "name": "Default (Copy)", "trackCount": 150 }`

## 曲目操作 

### playlist.getTrackCount 

获取播放列表中的曲目数量。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引（默认当前激活），兼容别名: index |

**返回值**: `{ "count": 150 }`

### playlist.getTracks 

获取播放列表中的曲目列表（分页）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引（默认当前激活），兼容别名: index |
| start | number | ✗ | 起始位置（默认 0） |
| count | number | ✗ | 获取数量（默认 100） |
| formats | object | ✗ | 自定义 TitleFormat 动态列映射（键为字段名，值为 TitleFormat 模式串） |

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
            "absolutePath": "C:\\\\Music\\\\song1.flac",
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

::: tip TIP
`absolutePath` 是本地文件系统路径，可直接用于 `artwork.getForTrack` 等 API。`path` 是 foobar2000 内部格式。
:::

### playlist.playTrack 

播放播放列表中的指定曲目。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引 |
| index | number | ✓ | 曲目索引，兼容旧参数: track |
| deferred | boolean | ✗ | 是否延迟执行（默认 false），可避免 HTTP 流解码器阻塞 |
| muted | boolean | ✗ | 播放前先静音（默认 false），消除两次调用之间的音频漏声 |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 5 });

// 延迟执行（流媒体场景推荐）
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 0, deferred: true });
```

### playlist.removeTracks 

从播放列表中删除指定曲目。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引（默认当前激活），兼容别名: index |
| items | number[] | ✓ | 要删除的曲目索引数组 |

**返回值**: `{ "success": true }`

### playlist.removeSelectedTracks 

删除播放列表中当前选中的曲目。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引（默认当前激活），兼容别名: index |

**返回值**: `{ "success": true }`

### playlist.moveTracks 

移动曲目（向上或向下）。当 `items` 非空时会先设置选区再移动；当 `items` 为空时直接移动当前选区（SMP 兼容语义）。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引（默认当前激活），兼容别名: index |
| items | number[] | ✗ | 要移动的曲目索引数组 |
| delta | number | ✓ | 移动偏移量（正数向下，负数向上） |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playlist.moveTracks', { items: [0, 1, 2], delta: 3 });
await fb2k.invoke('playlist.moveTracks', { items: [5, 6], delta: -2 });
```

### playlist.addPaths 

添加文件/文件夹到播放列表。使用 `playlist_incoming_item_filter` 同步解析，自动展开 CUE 文件。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | number | ✗ | 播放列表索引（默认当前激活） |
| paths | string[] | ✓ | 文件/文件夹路径数组 |

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
