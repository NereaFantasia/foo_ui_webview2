# fb.player 播放控制 

## play() 

开始播放。如果已暂停则恢复播放，已停止则从头开始。返回 `{success}`。

```javascript
await fb.player.play();
```

## pause() 

暂停播放。返回 `{success}`。

```javascript
await fb.player.pause();
```

## stop() 

停止播放。返回 `{success}`。

```javascript
await fb.player.stop();
```

## next() / prev() 

播放下一首/上一首。返回 `{success}`。

```javascript
await fb.player.next();
await fb.player.prev();
```

## seek(seconds) 

跳转到指定位置。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| seconds | number | 目标位置（秒） |

```javascript
await fb.player.seek(90); // 跳转到 1 分 30 秒
```

## getVolume() 

获取当前音量。返回 `Promise<{volume, volumeDb, muted}>`，其中 `volume` 范围 0-100。

```javascript
const info = await fb.player.getVolume();
console.log(info.volume);   // 0-100
console.log(info.volumeDb); // -100..0 dB
console.log(info.muted);    // boolean
```

## setVolume(volume) 

设置音量。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| volume | number | 音量百分比 (0-100) |

```javascript
await fb.player.setVolume(80); // 80%
await fb.player.setVolume(0);  // 静音
```

## mute() 

静音。已静音时无操作。如需切换静音，请使用底层 API `playback.toggleMute`。

```javascript
await fb.player.mute();
```

## toggle() 

切换播放/暂停。返回 `{success, isPlaying}`。

```javascript
const r = await fb.player.toggle();
console.log(r.isPlaying ? '正在播放' : '已暂停');
```

## random() 

随机播放一曲。返回 `{success}`。

```javascript
await fb.player.random();
```

## getState() 

获取播放状态。返回 `{state, canSeek, canPause}`。

```javascript
const state = await fb.player.getState();
// state.state: 'playing' | 'paused' | 'stopped'
```

## getCurrentTrack() 

获取当前播放曲目信息。无正在播放时返回 `{success: true, found: false, playing: false}`。

| 返回字段 | 类型 | 说明 |
| --- | --- | --- |
| title | string | 曲目标题 |
| artist | string | 艺术家 |
| album | string | 专辑 |
| albumArtist | string | 专辑艺术家 |
| genre | string | 流派 |
| date | string | 日期 |
| trackNumber | number | 曲目序号 |
| discNumber | number | 光盘序号 |
| duration | number | 时长（秒） |
| path | string | foobar 原始路径 |
| absolutePath | string | 规范化本地文件路径（不含 subsong 后缀） |
| fullPath | string | 完整路径（含 subsong 后缀，如 file.cue//1） |
| id | string | 唯一标识符（path + subsong 组合） |
| subsong | number | 子曲目索引（0 = 普通文件） |
| fileSize | number | 文件大小（字节） |
| bitrate | number | 比特率 (kbps) |
| sampleRate | number | 采样率 (Hz) |
| channels | number | 声道数 |
| codec | string | 编码格式 |

```javascript
const track = await fb.player.getCurrentTrack();
if (track.found !== false) {
    console.log(`${track.artist} - ${track.title} [${track.codec} ${track.bitrate}kbps]`);
}
```

## getPosition() 

获取当前播放位置。返回 `{position, duration, subsong, path}`。

```javascript
const pos = await fb.player.getPosition();
console.log(`${pos.position} / ${pos.duration}`);
// pos.subsong — 子曲目索引
// pos.path — 当前播放文件路径
```

## getOrder() / setOrder(order) 

获取/设置播放顺序。

| 值 | 名称 |
| --- | --- |
| 0 | Default |
| 1 | Repeat (playlist) |
| 2 | Repeat (track) |
| 3 | Random |
| 4 | Shuffle (tracks) |
| 5 | Shuffle (albums) |
| 6 | Shuffle (directories) |

```javascript
const r = await fb.player.getOrder();
console.log(r.order, r.name); // 0, 'Default'
await fb.player.setOrder(2);  // 单曲循环
```

## getStopAfterCurrent() / setStopAfterCurrent(enabled) 

获取/设置“当前曲目后停止”状态。

```javascript
const r = await fb.player.getStopAfterCurrent();
console.log(r.enabled); // false
await fb.player.setStopAfterCurrent(true);
```

获取当前播放曲目在播放列表中的索引。返回 `{playlist, index, ...}`。

```javascript
const r = await fb.player.getCurrentTrackIndex();
console.log(`播放列表 ${r.playlist}，曲目 ${r.index}`);
```

直接播放指定路径的文件。支持 CUE subsong 格式。

```javascript
await fb.player.playPath('E:\\Music\\song.flac');
```

播放多个文件路径。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| paths | string[] | 文件路径数组 |
| startIndex | number | 从第几首开始播放（默认 0） |

音量增大/减小一档。

```javascript
await fb.player.volumeUp();
await fb.player.volumeDown();
```

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## SDK 方法 stub

> 由 `scripts/gen_vitepress_sdk_doc.mjs` 生成。该区块用于补齐 SDK 视角方法覆盖，后续可人工扩展为完整示例与最佳实践。

### getPlayingPlaylist()

签名：`fb.player.getPlayingPlaylist(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `playback.getPlayingPlaylist` 调用结果。

```javascript
const result = await fb.player.getPlayingPlaylist();
```

### playPause()

签名：`fb.player.playPause(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `playback.playPause` 调用结果。

```javascript
const result = await fb.player.playPause();
```

### toggleMute()

签名：`fb.player.toggleMute(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `playback.toggleMute` 调用结果。

```javascript
const result = await fb.player.toggleMute();
```

### toggleStopAfterCurrent()

签名：`fb.player.toggleStopAfterCurrent(...args): Promise<unknown>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| ...args | unknown[] | 视方法而定 | 透传给 SDK wrapper；详细类型以 `sdk/src/bridge/namespaces/` 源码和生成类型为准 |

返回值：底层 `playback.toggleStopAfterCurrent` 调用结果。

```javascript
const result = await fb.player.toggleStopAfterCurrent();
```

<!-- END AUTO-GENERATED SDK STUBS -->
