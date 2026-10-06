# Queue 工具 

播放队列管理。共 11 个工具。

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
      "path": "file://D:\\Music\\track.flac",
      "absolutePath": "D:\\Music\\track.flac",
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

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| queueIndex | integer | 队列中的位置 |
| playlist | integer \| null | 来源播放列表索引；条目不带播放列表位置时为 `null` |
| playlistItem | integer \| null | 来源播放列表中的项索引；与 `playlist` 同时为 `null` |
| 曲目字段 | — | 同 getCurrentTrack 的字段结构 |

## fb2k_queue_add 

添加曲目到队列。

- **Bridge 方法**: `queue.add`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| playlist | integer | 否 | 源播放列表索引（默认活动列表） |
| tracks | integer[] | 条件必填 | 曲目索引数组（批量添加） |
| track | integer | 条件必填 | 单个曲目索引（与 tracks 二选一） |

::: tip
`tracks` 和 `track` 二选一。使用 `tracks` 一次添加多个，使用 `track` 添加单个。
:::

## fb2k_queue_add_paths 

按文件路径添加曲目到队列。

- **Bridge 方法**: `queue.addPaths`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | 是 | 文件路径数组，支持 path\|subsong:N 格式 |
| useQueuePlaylist | boolean | 否 | 是否使用专用队列播放列表（默认 true） |
| playlist | integer | 否 | 目标播放列表索引（仅当 useQueuePlaylist=false 时有效） |

## fb2k_queue_remove 

移除队列中指定位置的曲目。

- **Bridge 方法**: `queue.remove`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| index | integer | 条件必填 | 单个队列索引（最小 0） |
| indices | integer[] | 条件必填 | 索引数组（与 index 二选一） |

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
| index | integer | 是 | 从 0 起的队列曲目索引，最小值 0 |

## fb2k_queue_flush 

刷新播放队列（`queue.clear` 的别名）。

- **参数**: 无
- **Bridge 方法**: `queue.flush`

## fb2k_queue_set_contents 

用一份有序引用列表替换整个队列。

- **Bridge 方法**: `queue.setContents`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| items | `{ queueIndex }` 或 `{ playlist, item }` 的数组 | 是 | 有序引用列表；空数组清空队列 |

::: tip 整体失败，不会部分写入
任一条目形态无法识别都会让整次调用在写入前失败，队列保持不变——与逐项跳过无效条目的 `fb2k_queue_add` 不同。
:::

## fb2k_queue_insert_next 

插入曲目使其成为下一首播放，插入到当前队列所有曲目之前。条目可以按文件路径给、按播放列表位置给，或两者都给。

- **Bridge 方法**: `queue.insertNext`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| paths | string[] | 否¹ | 文件路径或 URL，支持 `path\|subsong:N` 格式；入队的条目不带播放列表位置 |
| items | `[{ playlist, item }]` | 否¹ | 播放列表位置（非负整数）；入队的条目带该位置，播放游标会跟随 |
| position | integer | 否 | 移除被移动条目之后的插入下标，默认 `0` |

¹ `paths` 与 `items` 至少一个非空。

已在队列中的曲目会被移动到 `position`，而不是重复入队。同一次调用里 `items` 块排在前、`paths` 块排在后。任一 `items` 条目有问题，整次调用在写入前失败；`items` 按曲目去重，同一曲目的两个位置只入队一条。返回结果除 `queueCount` 外还带 `insertedCount`、`movedCount` 与 `invalidCount`（只统计 `paths`）。

## fb2k_queue_play_now 

立即播放指定下标的队列曲目；若该曲目尚不在队首，会先将其移到队首。

- **Bridge 方法**: `queue.playNow`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| index | integer | 否 | 要播放的队列下标，默认 `0`（当前队首） |
