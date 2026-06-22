# Playback API 

播放控制、状态获取、音量控制和播放顺序管理。共 27 个 API（含别名 `playback.playPause` → `playback.playOrPause`）。

## 播放控制 

### playback.play 

开始播放。如果已暂停则恢复播放，如果已停止则从头开始。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.play');
```

### playback.pause 

暂停播放。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.pause');
```

### playback.stop 

停止播放。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.stop');
```

### playback.playOrPause 

切换播放/暂停状态。

- **参数**: 无
- **别名**: `playback.playPause`

**返回值**:

```json
{
    "success": true,
    "isPlaying": true
}
```

```javascript
const result = await fb2k.invoke('playback.playOrPause');
console.log(result.isPlaying ? '正在播放' : '已暂停');
```

### playback.next 

播放下一曲。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.next');
```

### playback.previous 

播放上一曲。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.previous');
```

### playback.random 

随机播放一曲。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.random');
```

## 状态获取 

### playback.getState 

获取当前播放状态。

- **参数**: 无

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
| state | string | "playing", "paused", "stopped" |
| canSeek | boolean | 是否支持跳转 |
| canPause | boolean | 是否支持暂停 |

::: tip TIP
如需获取播放位置和时长，请使用 `playback.getPosition`。
:::

```javascript
const state = await fb2k.invoke('playback.getState');
if (state.state === 'playing') {
    const pos = await fb2k.invoke('playback.getPosition');
    console.log(`正在播放: ${pos.position}/${pos.duration}`);
}
```

### playback.getCurrentTrack 

获取当前播放曲目的详细信息。无正在播放的曲目时返回 `{ "success": true, "found": false, "playing": false }`。

- **参数**: 无

**返回值**:

```json
{
    "id": "C:\\\\Music\\\\song.flac",
    "title": "Song Name",
    "artist": "Artist Name",
    "album": "Album Name",
    "albumArtist": "Artist Name",
    "genre": "Rock",
    "date": "2024",
    "trackNumber": 1,
    "discNumber": 1,
    "duration": 180.5,
    "path": "file://C:/Music/song.flac",
    "absolutePath": "C:\\\\Music\\\\song.flac",
    "fullPath": "C:\\\\Music\\\\song.flac",
    "subsong": 0,
    "fileSize": 25600000,
    "bitrate": 1411,
    "sampleRate": 44100,
    "channels": 2,
    "codec": "FLAC"
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| absolutePath | string | 规范化的本地文件路径，不含 subsong 后缀 |
| fullPath | string | 完整路径。当 subsong > 0 时格式为 absolutePath\|subsong:N，否则与 absolutePath 相同 |
| id | string | 唯一标识，与 fullPath 一致 |

> CUE 子曲的 `subsong > 0`，`fullPath` 会包含 `|subsong:N` 后缀。`absolutePath` 始终是纯文件路径。

```javascript
const track = await fb2k.invoke('playback.getCurrentTrack');
if (track.title) {
    console.log(`${track.artist} - ${track.title} [${track.codec}]`);
}
```

## 音量控制 

### playback.getVolume 

获取当前音量。

**返回值**:

```json
{
    "volume": 80,
    "volumeDb": -1.94,
    "muted": false,
    "isMuted": false
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| volume | number | 音量百分比 (0-100) |
| volumeDb | number | 音量 dB 值 (-100..0) |
| muted | boolean | 是否静音 |
| isMuted | boolean | 是否静音（别名） |

```javascript
const vol = await fb2k.invoke('playback.getVolume');
console.log(`音量: ${vol.volume}%, ${vol.volumeDb}dB`);
```

### playback.setVolume 

设置音量。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| volume | number | ✓ | 音量百分比 (0-100) |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.setVolume', { volume: 80 });
```

### playback.mute 

设置静音状态。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| muted | boolean | - | 是否静音（默认 true） |

**返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.mute');           // 静音
await fb2k.invoke('playback.mute', { muted: false }); // 取消静音
```

### playback.toggleMute 

切换静音状态。

- **参数**: 无

**返回值**:

```json
{
    "success": true,
    "muted": true
}
```

```javascript
const result = await fb2k.invoke('playback.toggleMute');
console.log(result.muted ? '已静音' : '已取消静音');
```

### playback.volumeUp 

音量增加一级（foobar2000 内置步进）。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.volumeUp');
```

### playback.volumeDown 

音量降低一级（foobar2000 内置步进）。

- **参数**: 无
- **返回值**: `{ "success": true }`

```javascript
await fb2k.invoke('playback.volumeDown');
```

## 播放位置 

### playback.getPosition 

获取当前播放位置。

- **参数**: 无

**返回值**:

```json
{
    "position": 60.5,
    "duration": 180.0,
    "subsong": 0,
    "path": "file://C:/Music/song.flac"
}
```

| 字段 | 类型 | 描述 |
| --- | --- | --- |
| position | number | 当前播放位置（秒） |
| duration | number | 曲目总时长（秒） |
| subsong | number | 子轨索引（0 = 普通文件） |
| path | string | foobar2000 内部路径 |

```javascript
const pos = await fb2k.invoke('playback.getPosition');
console.log(`${pos.position.toFixed(1)}s / ${pos.duration.toFixed(1)}s`);
```

### playback.setPosition 

跳转到指定播放位置。自动进行边界检查。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| position | number | ✓ | 目标位置（秒），兼容别名: seconds |

**返回值**:

```json
{
    "success": true,
    "requestedPosition": 60.0,
    "actualPosition": 60.0,
    "newPosition": 60.0,
    "oldPosition": 30.0,
    "duration": 180.0,
    "subsong": 0
}
```

> 当前不可 seek 时返回 `{ "success": false, "error": "Cannot seek in current track" }`。

```javascript
await fb2k.invoke('playback.setPosition', { position: 60 });
// 或使用别名
await fb2k.invoke('playback.setPosition', { seconds: 60 });
```

## 当前曲目后停止 

### playback.getStopAfterCurrent 

获取“当前曲目后停止”状态。

- **参数**: 无

**返回值**:

```json
{
    "enabled": false
}
```

```javascript
const result = await fb2k.invoke('playback.getStopAfterCurrent');
console.log('当前曲目后停止:', result.enabled);
```

### playback.setStopAfterCurrent 

设置“当前曲目后停止”状态。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| enabled | boolean | ✓ | 是否启用 |

**返回值**: `{ "success": true, "enabled": true }`

```javascript
await fb2k.invoke('playback.setStopAfterCurrent', { enabled: true });
```

::: tip TIP
对应事件 `playback:stopAfterCurrentChanged`，数据字段为 `{value}`（注意与 API 返回值字段名 `enabled` 不同）。
:::

### playback.toggleStopAfterCurrent 

切换「当前曲目后停止」状态。

- **参数**: 无

**返回值**:

```json
{
    "enabled": true
}
```

```javascript
const result = await fb2k.invoke('playback.toggleStopAfterCurrent');
console.log('当前曲目后停止:', result.enabled);
```

## 播放顺序 

### playback.getPlaybackOrder 

获取当前播放顺序。

- **参数**: 无

**返回值**:

```json
{
    "order": 0,
    "orderName": "default",
    "name": "default",
    "orderIndex": 0
}
```

| 值 | 名称 | 描述 |
| --- | --- | --- |
| 0 | Default | 顺序播放 |
| 1 | Repeat (playlist) | 列表循环 |
| 2 | Repeat (track) | 单曲循环 |
| 3 | Random | 随机播放 |
| 4 | Shuffle (tracks) | 随机不重复 |
| 5 | Shuffle (albums) | 专辑随机 |
| 6 | Shuffle (directories) | 目录随机 |

```javascript
const order = await fb2k.invoke('playback.getPlaybackOrder');
console.log(`播放顺序: ${order.orderName} (${order.order})`);
```

### playback.setPlaybackOrder 

设置播放顺序。支持数字索引或字符串名称。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| order | number \| string | ✓ | 播放顺序索引 (0-6) 或字符串名称 |

**字符串值**: `"default"`, `"repeat-playlist"`, `"repeat-track"`, `"random"`, `"shuffle-tracks"`, `"shuffle-albums"`, `"shuffle-folders"`

**返回值**: `{ "success": true, "order": 2, "orderName": "repeat-track" }`

```javascript
// 使用数字
await fb2k.invoke('playback.setPlaybackOrder', { order: 2 });
// 使用字符串
await fb2k.invoke('playback.setPlaybackOrder', { order: 'repeat-track' });
```

## 直接路径播放 

### playback.playPath 

通过文件路径直接播放单个曲目。会将曲目添加到当前播放列表末尾并开始播放。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| path | string | Y | 文件路径，支持 subsong 格式 `path\|subsong:N` |

**返回值**:

```json
{
    "success": true,
    "tracksAdded": 1,
    "path": "C:\\\\Music\\\\album.cue",
    "subsong": 2
}
```

```javascript
// 普通文件
await fb2k.invoke('playback.playPath', { path: 'C:\\\\Music\\\\song.flac' });

// CUE 子曲（第3轨，0-based）
await fb2k.invoke('playback.playPath', { path: 'C:\\\\Music\\\\album.cue|subsong:2' });
```

### playback.playPaths 

通过文件路径数组播放多个曲目。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| paths | string[] | Y | 文件路径数组 |
| startIndex | number | - | 从第几首开始播放（默认 0） |
| replace | boolean | - | 是否替换当前播放列表（默认 false，追加） |

**返回值**:

```json
{
    "success": true,
    "tracksAdded": 10,
    "startedAt": 0
}
```

```javascript
await fb2k.invoke('playback.playPaths', {
    paths: ['C:\\\\Music\\\\a.flac', 'C:\\\\Music\\\\b.flac'],
    startIndex: 1
});
```

### playback.playPause 

`playback.playOrPause` 的别名。行为完全一致。

- **参数**: 无
- **返回值**: `{ "success": true, "isPlaying": true }`

```javascript
const result = await fb2k.invoke('playback.playPause');
console.log(result.isPlaying ? '正在播放' : '已暂停');
```

## 播放位置查询 

### playback.getCurrentTrackIndex 

获取当前播放曲目在播放列表中的位置。

| 参数 | 类型 | 必填 | 描述 |
| --- | --- | --- | --- |
| includeTrackInfo | boolean | - | 是否附带曲目详情（默认 false） |

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

::: warning 注意
当 `found` 为 `false` 时，`playlist` 和 `index` 均为 `null` 而非数字。前端使用时务必先检查 `found` 字段。
:::

若 `includeTrackInfo: true`，返回值额外包含 `track` 字段（与 `playback.getCurrentTrack` 格式一致）。

```javascript
const pos = await fb2k.invoke('playback.getCurrentTrackIndex', { includeTrackInfo: true });
if (pos.found) {
    console.log(`播放列表 ${pos.playlist}，第 ${pos.index} 首`);
}
```

### playback.getPlayingPlaylist 

获取当前正在播放的播放列表信息。

- **参数**: 无

**返回值**（找到时）:

```json
{
    "success": true,
    "found": true,
    "playlist": 0,
    "name": "Default"
}
```

**返回值**（未找到时）:

```json
{
    "success": true,
    "found": false,
    "playlist": null
}
```
