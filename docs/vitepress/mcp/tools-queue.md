# Queue 工具 

播放队列管理。共 8 个工具。

## fb2k_queue_get 

获取队列中的所有曲目。

- **参数**: 无
- **Bridge 方法**: `queue.get`

**返回值**:

```json
{
  "items": [
    {
      "queueIndex": 0,
      "path": "file://D:\\\\Music\\\\track.flac",
      "absolutePath": "D:\\\\Music\\\\track.flac",
      "subsong": 0,
      "fileSize": 28456789,
      "title": "天ノ弱",
      "artist": "164",
      "album": "天ノ弱",
      "albumArtist": "164",
      "genre": "Vocaloid",
      "date": "2011",
      "trackNumber": 1,
      "discNumber": 1,
      "duration": 263.5,
      "bitrate": 876,
      "sampleRate": 44100,
      "channels": 2,
      "codec": "FLAC",
      "playlist": 0,
      "playlistItem": 5
    }
  ],
  "count": 1
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| queueIndex | integer | 队列中的位置 |
| playlist | integer | 来源播放列表索引 |
| playlistItem | integer | 来源播放列表中的项索引 |
| 曲目字段 | — | 同 getCurrentTrack 的字段结构 |

## fb2k_queue_add 

添加曲目到队列。

- **Bridge 方法**: `queue.add`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | ? | 源播放列表索引（默认活动列表） |
| tracks | integer[] | ? | 曲目索引数组（批量添加） |
| track | integer | ? | 单个曲目索引（与 tracks 二选一） |

::: tip TIP
`tracks` 和 `track` 二选一。使用 `tracks` 一次添加多个，使用 `track` 添加单个。
:::

## fb2k_queue_add_paths 

按文件路径添加曲目到队列。

- **Bridge 方法**: `queue.addPaths`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | ? | 文件路径数组，支持 path\|subsong:N 格式 |
| useQueuePlaylist | boolean | ? | 是否使用专用队列播放列表（默认 true） |
| playlist | integer | ? | 目标播放列表索引（仅当 useQueuePlaylist=false 时有效） |

## fb2k_queue_remove 

移除队列中指定位置的曲目。

- **Bridge 方法**: `queue.remove`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | integer | ? | 单个队列索引（最小 0） |
| indices | integer[] | ? | 索引数组（与 index 二选一） |

## fb2k_queue_clear 

清空播放队列。

- **参数**: 无
- **Bridge 方法**: `queue.clear`

## fb2k_queue_get_count 

获取队列中曲目数。

- **参数**: 无
- **Bridge 方法**: `queue.getCount`

**返回值**:

```json
{ "count": 3, "hasItems": true }
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| count | integer | 队列曲目数 |
| hasItems | boolean | 队列是否非空 |

## fb2k_queue_move_to_top 

将指定曲目移至队列顶部。

- **Bridge 方法**: `queue.moveToTop`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | integer | ? | 队列中的曲目索引 |

## fb2k_queue_flush 

刷新播放队列（`queue.clear` 的别名）。

- **参数**: 无
- **Bridge 方法**: `queue.flush`
