# Playback 工具 

播放控制、状态获取、音量管理。共 12 个工具。

## 播放控制 

### fb2k_playback_play 

开始播放。如果已暂停则恢复播放，如果已停止则从头开始。

- **参数**: 无
- **Bridge 方法**: `playback.play`

### fb2k_playback_pause 

暂停播放。

- **参数**: 无
- **Bridge 方法**: `playback.pause`

### fb2k_playback_stop 

停止播放。

- **参数**: 无
- **Bridge 方法**: `playback.stop`

### fb2k_playback_next 

播放下一首。

- **参数**: 无
- **Bridge 方法**: `playback.next`

### fb2k_playback_previous 

播放上一首。

- **参数**: 无
- **Bridge 方法**: `playback.previous`

### fb2k_playback_play_pause 

切换播放/暂停状态。

- **参数**: 无
- **Bridge 方法**: `playback.playPause`

## 状态获取 

### fb2k_playback_get_state 

获取当前播放状态。

- **参数**: 无
- **Bridge 方法**: `playback.getState`

**返回值**:

```json
{
  "state": "playing",
  "canSeek": true,
  "canPause": true
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| state | string | "playing" / "paused" / "stopped" |
| canSeek | boolean | 是否支持跳转 |
| canPause | boolean | 是否支持暂停 |

### fb2k_playback_get_current_track 

获取当前播放曲目的详细信息。无播放时 `found` 为 `false`。

- **参数**: 无
- **Bridge 方法**: `playback.getCurrentTrack`

**返回值**（正在播放时）:

```json
{
  "id": "D:\\\\Music\\\\164\\\\天ノ弱.flac",
  "title": "天ノ弱",
  "artist": "164 feat. GUMI",
  "album": "天ノ弱",
  "albumArtist": "164",
  "genre": "Vocaloid",
  "date": "2011",
  "trackNumber": 1,
  "discNumber": 1,
  "duration": 263.5,
  "path": "file://D:\\\\Music\\\\164\\\\天ノ弱.flac",
  "absolutePath": "D:\\\\Music\\\\164\\\\天ノ弱.flac",
  "fullPath": "D:\\\\Music\\\\164\\\\天ノ弱.flac",
  "subsong": 0,
  "fileSize": 28456789,
  "bitrate": 876,
  "sampleRate": 44100,
  "channels": 2,
  "codec": "FLAC"
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| id | string | 完整路径标识（含 subsong 后缀） |
| title | string | 标题 |
| artist | string | 艺术家 |
| album | string | 专辑 |
| albumArtist | string | 专辑艺术家 |
| genre | string | 流派 |
| date | string | 发行日期 |
| trackNumber | integer | 曲目序号 |
| discNumber | integer | 碟片序号 |
| duration | number | 时长（秒） |
| path | string | 原始 foobar2000 路径 |
| absolutePath | string | 标准化绝对路径（无 subsong） |
| fullPath | string | 绝对路径 + 可选 \|subsong:N |
| subsong | integer | 子曲目索引（如 CUE） |
| fileSize | integer | 文件大小（字节） |
| bitrate | integer | 比特率（kbps） |
| sampleRate | integer | 采样率（Hz） |
| channels | integer | 声道数 |
| codec | string | 编解码器名称 |

**返回值**（未播放时）:

```json
{ "success": true, "found": false, "playing": false }
```

### fb2k_playback_get_position 

获取播放位置和总时长。

- **参数**: 无
- **Bridge 方法**: `playback.getPosition`

**返回值**:

```json
{
  "position": 45.2,
  "duration": 263.5,
  "subsong": 0,
  "path": "D:\\\\Music\\\\164\\\\天ノ弱.flac"
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| position | number | 当前播放位置（秒） |
| duration | number | 总时长（秒） |
| subsong | integer | 子曲目索引 |
| path | string | 当前文件路径 |

## 音量控制 

### fb2k_playback_set_position 

跳转到指定播放位置。

- **Bridge 方法**: `playback.setPosition`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| seconds | number | ? | 目标位置（秒） |

### fb2k_playback_get_volume 

获取当前音量和静音状态。

- **参数**: 无
- **Bridge 方法**: `playback.getVolume`

**返回值**:

```json
{
  "volume": 75,
  "volumeDb": -6.2,
  "muted": false,
  "isMuted": false
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| volume | number | 音量 0–100（线性百分比） |
| volumeDb | number | 音量 dB 值 |
| muted | boolean | 是否静音 |
| isMuted | boolean | 静音状态（muted 的别名） |

### fb2k_playback_set_volume 

设置音量。

- **Bridge 方法**: `playback.setVolume`

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| volume | number | ? | 音量值 (0–100) |
