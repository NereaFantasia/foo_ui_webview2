# fb.player 播放控制

## 底层端点对照

`fb.player` 提供的方法为面向开发者的便利封装别名，部分方法名与底层 `fb2k.invoke` 端点存在映射差异。直接使用 bridge 通信或进行错误排查时，可参考下表映射关系：

| 方法 | 调用的端点 |
| --- | --- |
| `play()` | `playback.play` |
| `pause()` | `playback.pause` |
| `stop()` | `playback.stop` |
| `next()` | `playback.next` |
| `prev()` | `playback.previous` |
| `random()` | `playback.random` |
| `toggle()` | `playback.playOrPause` |
| `playPause()` | `playback.playPause` |
| `seek(seconds)` | `playback.setPosition` |
| `getVolume()` | `playback.getVolume` |
| `setVolume(volume)` | `playback.setVolume` |
| `volumeUp()` | `playback.volumeUp` |
| `volumeDown()` | `playback.volumeDown` |
| `mute()` | `playback.mute` |
| `toggleMute()` | `playback.toggleMute` |
| `getState()` | `playback.getState` |
| `getCurrentTrack()` | `playback.getCurrentTrack` |
| `getPosition()` | `playback.getPosition` |
| `getOrder()` | `playback.getPlaybackOrder` |
| `setOrder(order)` | `playback.setPlaybackOrder` |
| `getStopAfterCurrent()` | `playback.getStopAfterCurrent` |
| `setStopAfterCurrent(enabled)` | `playback.setStopAfterCurrent` |
| `toggleStopAfterCurrent()` | `playback.toggleStopAfterCurrent` |
| `getCurrentTrackIndex(includeTrackInfo?)` | `playback.getCurrentTrackIndex` |
| `getPlayingPlaylist()` | `playback.getPlayingPlaylist` |
| `playPath(path)` | `playback.playPath` |
| `playPaths(paths, options?)` | `playback.playPaths` |

## play()

开始播放。如果已暂停则恢复播放，已停止则从头开始。成功时返回 `{ success: true }`。

```javascript
await fb.player.play();
```

## pause()

暂停播放。成功时返回 `{ success: true }`。

```javascript
await fb.player.pause();
```

## stop()

停止播放。成功时返回 `{ success: true }`。

```javascript
await fb.player.stop();
```

## next() / prev()

播放下一首/上一首。成功时返回 `{ success: true }`。

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
if (info.success === false) throw new Error(info.error);
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

设置静音状态。参数 `muted` 默认为 `true`；传 `false` 取消静音。要在两种状态间切换，用 `fb.player.toggleMute()`。

```javascript
await fb.player.mute();
```

## toggle()

切换播放/暂停。返回 `{success, isPlaying}`。

```javascript
const r = await fb.player.toggle();
if (r.success === false) throw new Error(r.error);
console.log(r.isPlaying ? '正在播放' : '已暂停');
```

## random()

随机播放一曲。成功时返回 `{ success: true }`。

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

返回 `PlaybackGetCurrentTrackResponse`：`found` 指示当前是否载入曲目；仅在 `found` 为 `true` 时包含 `track` 字段（格式参见 [Track](../reference/types.md#track)）。当未载入曲目时，`track` 字段省略而非返回 `null`。

> `artist`、`albumArtist`、`genre` 的多值标签按 `, ` 原序拼接；`artists`、`albumArtists` 分别是 `artist`、`albumArtist` 背后的各个原始值。

```javascript
const result = await fb.player.getCurrentTrack();
if (result.success === false) throw new Error(result.error);
if (!result.found) {
    console.log('当前没有曲目');
} else {
    const t = result.track;
    console.log(`${t.artist} - ${t.title} [${t.codec} ${t.bitrate}kbps]`);
}
```

## getPosition()

获取当前播放位置。返回 `{position, duration, subsong, path, hostTime}`。`hostTime` 表示宿主采样该位置时的系统时间戳（Unix 毫秒），与 `Date.now()` 共享相同时钟基准：播放状态下的实时位置可通过 `pos.position + (Date.now() - pos.hostTime) / 1000` 计算得出。[`PlaybackClock`](#playbackclock) 在两次更新之间持续推算这一位置。

```javascript
const pos = await fb.player.getPosition();
if (pos.success === false) throw new Error(pos.error);
console.log(`${pos.position} / ${pos.duration}`);
// pos.subsong — 子曲目索引
// pos.path — 当前播放文件路径
```

## getOrder() / setOrder(order)

获取/设置播放顺序。`setOrder()` 接受序号或 `PlaybackOrder` 名称，其余输入被宿主拒绝；返回生效的顺序（`order`、`orderName`）。

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
if (r.success === false) throw new Error(r.error);
console.log(r.order, r.orderName); // 0, 'default'
await fb.player.setOrder(2);  // 单曲循环
```

## getStopAfterCurrent() / setStopAfterCurrent(enabled)

获取/设置“当前曲目后停止”状态。

```javascript
const r = await fb.player.getStopAfterCurrent();
if (r.success === false) throw new Error(r.error);
console.log(r.enabled); // false
await fb.player.setStopAfterCurrent(true);
```

## getCurrentTrackIndex(includeTrackInfo?)

获取当前播放曲目在播放列表中的位置，返回 `{ found, playlist, index, track? }`；曲目没有播放列表位置时 `playlist` 与 `index` 为 `null`。传入 `true` 附带 `track` 行。

```javascript
const r = await fb.player.getCurrentTrackIndex();
if (r.success === false) throw new Error(r.error);
console.log(`播放列表 ${r.playlist}，曲目 ${r.index}`);
```

## playPath(path)

直接播放指定路径的文件。CUE 条目可使用宿主的 subsong 后缀格式。

```javascript
await fb.player.playPath('E:\\Music\\song.flac');
```

## playPaths(paths, options?)

添加并播放多个文件路径。第二个参数可以是数字 `startIndex`，也可以是 `{ startIndex?, replace? }`；`replace: true` 会在插入前清空当前播放列表，默认行为则为追加。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `paths` | `string[]` | 绝对路径数组；条目可带有 `\|subsong:N` 后缀 |
| `options` | `number \| { startIndex?: number; replace?: boolean }` | 起始偏移量或扩展选项 |

```javascript
await fb.player.playPaths(paths, { startIndex: 2, replace: true });
```

## volumeUp() / volumeDown()

音量增大/减小一档。

```javascript
await fb.player.volumeUp();
await fb.player.volumeDown();
```

## getPlayingPlaylist()

签名：`fb.player.getPlayingPlaylist(): Promise<PlaybackGetPlayingPlaylistResponse>`

返回最近一次发起播放的播放列表信息：包含 `found`、`playlist`（索引）、`playlistGuid` 与 `name`。若无历史播放列表，`found` 为 `false`，`playlist` 与 `playlistGuid` 为 `null` 且不附带 `name`。foobar2000 宿主在停止播放后仍保留该播放列表引用，因此停止状态下仍可读取最近播放来源。

```javascript
const res = await fb.player.getPlayingPlaylist();
if (res.success && res.found) console.log(`正在播放：${res.name}`);
```

## playPause()

签名：`fb.player.playPause(): Promise<PlaybackPlayPauseResponse>`

在播放与暂停之间切换，停止状态下开始播放。与 `toggle()` 是同一个操作，只是底层方法名不同（`playback.playPause`）。返回 `{ isPlaying }`，即切换后是否在播放。

```javascript
const res = await fb.player.playPause();
if (res.success) console.log(res.isPlaying ? '正在播放' : '已暂停');
```

## toggleMute()

签名：`fb.player.toggleMute(): Promise<PlaybackToggleMuteResponse>`

切换静音状态，返回 `{ muted }` 指示切换后的静音状态。随后广播 `playback:volumeChanged` 事件，载荷携带 `muted`。

```javascript
const res = await fb.player.toggleMute();
if (res.success) console.log(res.muted ? '已静音' : '已取消静音');
```

## toggleStopAfterCurrent()

签名：`fb.player.toggleStopAfterCurrent(): Promise<PlaybackToggleStopAfterCurrentResponse>`

切换「当前曲目播放完毕后停止」设置，返回 `{ enabled }` 指示切换后的生效状态。选项变化时广播 `playback:stopAfterCurrentChanged` 事件，载荷为 `{ enabled }`。

```javascript
const res = await fb.player.toggleStopAfterCurrent();
if (res.success) console.log(res.enabled ? '本曲结束后停止' : '继续播放');
```

## PlaybackClock

`import { PlaybackClock } from 'foo-webview-sdk/bridge'`；用 `<script>` 包时写 `new fb.PlaybackClock()`。

在位置事件推送间隙估算当前曲目的实时播放进度，适用于高频渲染场景（如进度条动画、卡拉 OK 逐字歌词以及静音伴随视频渲染）。宿主在每个位置采样上附带 `hostTime`，即读取该位置时的系统时间；时钟据此判断采样到达时已过去多久，再以 `performance.now()` 向前推算。

- 它跟随 `playback:timeHighRes`、`playback:seeked`、`playback:stateChanged` 与 `playback:trackChanged` 事件，并在创建时、页面重新可见时和调用 `resync()` 时读取 `playback.getState` 与 `playback.getPosition`。`ready` 在首次读取完成后兑现，不会被拒绝。
- `resync()` 默认忽略读取失败。若必须等待新快照方可恢复播放，可使用 `resync({ rejectOnFailure: true })`：两次调用均成功后才应用读数。宿主失败将以保留 `code` 的 `ApiCallError` 拒绝，bridge 通信异常亦会拒绝。当存在并发读取导致无法取得完整新快照时，严格请求可能被拒绝，在恢复媒体播放前须妥善处理该失败。
- `position(at?)` 返回指定时间戳的估算位置（单位秒）；`at` 基于 `performance.now()` 刻度，省略时取当前时间。已知时长时不超出 `duration`。`state` 为 `paused` 时位置保持静态，为 `stopped` 时固定为 `0`。
- 当推送值与估算值偏差较小时，时钟只将估算值向推送值修正偏差的四分之一，避免宿主采样抖动引发渲染跳变。当偏差超过 `resyncThresholdSeconds`（默认 0.25 秒）时，立即对齐为最新推送值。寻轨跳转生效后到达的滞后推送将被自动忽略。
- `onChange(listener)` 监听位置发生不连续跳变的事件：初次就绪时触发 `start`，其余触发原因包含 `seek`、`state`、`track`，以及偏差校正或 `resync()` 主动同步。跟随播放的静音伴随视频可在这些关键时刻执行精准对齐，其余时间仅调整播放速率。
- 延迟超过 `maxDeliveryDelayMs`（默认 1000 毫秒）的过期推送、或因系统时钟调整导致的异常时间戳推送均被自动忽略。若宿主未提供 `hostTime`，将以事件到达时刻作为采样基准，估算值将比实际音频输出滞后一个投递延迟。
- 实例不再使用时，调用 `dispose()` 释放资源。

```javascript
import { PlaybackClock } from 'foo-webview-sdk/bridge';

const clock = new PlaybackClock();
await clock.ready;
const bar = document.querySelector('#progress');
const draw = () => {
    if (bar instanceof HTMLElement && clock.duration > 0) {
        bar.style.width = `${(clock.position() / clock.duration) * 100}%`;
    }
    requestAnimationFrame(draw);
};
requestAnimationFrame(draw);
const off = clock.onChange((change) => console.log(change.reason, change.position));
// 清理注销
off();
clock.dispose();
```
