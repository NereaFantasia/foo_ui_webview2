# Playlist 工具 

播放列表基础操作。共 7 个工具。

## 查询 

### fb2k_playlist_get_all 

获取所有播放列表。

- **参数**: 无
- **Bridge 方法**: `playlist.getAll`

**返回值**:

```json
[
  {
    "index": 0,
    "name": "Default",
    "trackCount": 128,
    "isActive": true,
    "isPlaying": true,
    "isLocked": false,
    "isAutoplaylist": false
  },
  {
    "index": 1,
    "name": "Favorites",
    "trackCount": 42,
    "isActive": false,
    "isPlaying": false,
    "isLocked": false,
    "isAutoplaylist": true
  }
]
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| index | integer | 播放列表索引 |
| name | string | 播放列表名称 |
| trackCount | integer | 曲目数 |
| isActive | boolean | 是否为当前活动列表 |
| isPlaying | boolean | 是否正在播放 |
| isLocked | boolean | 是否已锁定 |
| isAutoplaylist | boolean | 是否为自动播放列表 |

### fb2k_playlist_get_active 

获取当前活动播放列表。

- **参数**: 无
- **Bridge 方法**: `playlist.getActive`

**返回值**:

```json
{
  "index": 0,
  "name": "Default",
  "trackCount": 128,
  "isActive": true,
  "isPlaying": true,
  "isLocked": false,
  "duration": 34567.8
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| index | integer | 播放列表索引 |
| name | string | 播放列表名称 |
| trackCount | integer | 曲目数 |
| isActive | boolean | 是否为活动列表 |
| isPlaying | boolean | 是否正在播放 |
| isLocked | boolean | 是否已锁定 |
| duration | number | 总时长（秒） |

## 管理 

### fb2k_playlist_set_active 

切换活动播放列表。

- **Bridge 方法**: `playlist.setActive`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

### fb2k_playlist_create 

创建新播放列表。

- **Bridge 方法**: `playlist.create`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| name | string | ? | 播放列表名称 |

### fb2k_playlist_remove 

删除播放列表。

- **Bridge 方法**: `playlist.remove`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引 |

## 曲目操作 

### fb2k_playlist_get_tracks 

获取播放列表中的曲目（支持分页）。

- **Bridge 方法**: `playlist.getTracks`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 播放列表索引（默认活动列表） |
| start | integer | ? | 起始索引（默认 0） |
| count | integer | ? | 返回数量（默认全部） |

**返回值**:

```json
{
  "playlist": 0,
  "start": 0,
  "count": 2,
  "total": 128,
  "tracks": [
    {
      "index": 0,
      "title": "天ノ弱",
      "artist": "164 feat. GUMI",
      "album": "天ノ弱",
      "albumArtist": "164",
      "genre": "Vocaloid",
      "date": "2011",
      "trackNumber": 1,
      "discNumber": 1,
      "duration": 263.5,
      "path": "D:\\\\Music\\\\164\\\\天ノ弱.flac",
      "absolutePath": "D:\\\\Music\\\\164\\\\天ノ弱.flac",
      "fileSize": 28456789,
      "subsong": 0,
      "rating": 5,
      "codec": "FLAC",
      "bitrate": 876,
      "sampleRate": 44100,
      "channels": 2,
      "composer": "164",
      "comment": "",
      "playCount": "12",
      "firstPlayed": "2024-01-15",
      "lastPlayed": "2025-06-01",
      "added": "2024-01-10"
    }
  ]
}
```

::: tip 分页
对于大型播放列表，建议使用 `start` 和 `count` 进行分页获取。返回值中的 `total` 字段表示曲目总数。
:::

### fb2k_playlist_play_track 

播放指定曲目。

- **Bridge 方法**: `playlist.playTrack`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | integer | ? | 曲目索引 |
| playlist | integer | ? | 播放列表索引 |
| deferred | boolean | ? | 是否延迟播放 |
