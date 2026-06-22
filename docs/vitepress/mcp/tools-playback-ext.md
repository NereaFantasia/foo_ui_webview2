# Playback Extended 工具 

播放扩展：静音、播放顺序、路径播放。共 13 个工具。

## 静音控制 

### fb2k_playback_mute 

设置静音状态。

- **Bridge 方法**: `playback.mute`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| muted | boolean | ? | 是否静音（默认 true） |

### fb2k_playback_toggle_mute 

切换静音状态。

- **参数**: 无
- **Bridge 方法**: `playback.toggleMute`

### fb2k_playback_volume_up 

音量增加一档。

- **参数**: 无
- **Bridge 方法**: `playback.volumeUp`

### fb2k_playback_volume_down 

音量减少一档。

- **参数**: 无
- **Bridge 方法**: `playback.volumeDown`

## 播放顺序 

### fb2k_playback_get_playback_order 

获取当前播放顺序模式。

- **参数**: 无
- **Bridge 方法**: `playback.getPlaybackOrder`

**返回值**:

```json
{
  "order": 0,
  "orderName": "default",
  "name": "default",
  "orderIndex": 0
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| order | integer | 播放顺序索引（0–6） |
| orderName | string | 播放顺序名称 |
| name | string | orderName 的别名 |
| orderIndex | integer | order 的别名 |

### fb2k_playback_set_playback_order 

设置播放顺序模式。

- **Bridge 方法**: `playback.setPlaybackOrder`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| order | string | ? | 播放顺序模式 |

可选值：

| 值 | 说明 |
| --- | --- |
| default | 默认顺序 |
| repeat-playlist | 重复播放列表 |
| repeat-track | 单曲循环 |
| random | 随机播放 |
| shuffle-tracks | 随机曲目 |
| shuffle-albums | 随机专辑 |
| shuffle-folders | 随机文件夹 |

## 停止后当前曲目 

### fb2k_playback_get_stop_after_current 

获取"当前曲目后停止"状态。

- **参数**: 无
- **Bridge 方法**: `playback.getStopAfterCurrent`**返回值**:

```json
{ "enabled": false }
```

### fb2k_playback_set_stop_after_current 

设置"当前曲目后停止"。

- **Bridge 方法**: `playback.setStopAfterCurrent`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| enabled | boolean | ? | 是否启用 |

### fb2k_playback_toggle_stop_after_current 

切换"当前曲目后停止"。

- **参数**: 无
- **Bridge 方法**: `playback.toggleStopAfterCurrent`

## 曲目定位 

### fb2k_playback_get_current_track_index 

获取当前播放曲目的索引。

- **Bridge 方法**: `playback.getCurrentTrackIndex`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| includeTrackInfo | boolean | ? | 是否包含曲目详情（默认 false） |

**返回值**（找到时）:

```json
{
  "success": true,
  "found": true,
  "playlist": 0,
  "index": 5
}
```

**返回值**（未找到时）:

```json
{
  "success": true,
  "found": false,
  "playlist": null,
  "index": null
}
```

::: tip TIP
设置 `includeTrackInfo: true` 时，返回值会额外包含 `track` 字段，包含完整曲目信息（同 `playback.getCurrentTrack` 的字段）。
:::

### fb2k_playback_get_playing_playlist 

获取正在播放的播放列表索引。

- **参数**: 无
- **Bridge 方法**: `playback.getPlayingPlaylist`

**返回值**（有播放列表时）:

```json
{
  "success": true,
  "found": true,
  "playlist": 0,
  "name": "Default"
}
```

**返回值**（无播放列表时）:

```json
{ "success": true, "found": false, "playlist": null }
```

### fb2k_playback_play_path 

播放指定文件路径。支持 subsong 格式。

- **Bridge 方法**: `playback.playPath`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | ? | 文件路径，支持 path\|subsong:N 格式 |

::: tip subsong 格式
对于 CUE 文件或多曲目容器，可以指定子曲目索引：`C:\\music\\album.flac|subsong:2`
:::

### fb2k_playback_random 

随机播放一首曲目。

- **参数**: 无
- **Bridge 方法**: `playback.random`
