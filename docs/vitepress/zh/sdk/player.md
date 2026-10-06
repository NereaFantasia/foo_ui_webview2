# fb.player 播放控制

## 底层端点对照

`fb.player` 的方法名是 SDK 侧别名，不一定与底层 `fb2k.invoke` 端点同名。直接调用 bridge，或者 `Method not found` 报出 `playback.prev` 这类名字时，按此表查端点。

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

返回 `PlaybackGetCurrentTrackResponse`：`found` 表示是否有曲目载入，有时才带 `track`，即共享的 [Track](../reference/types.md#track) 行。宿主从不返回 `null`。

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

获取当前播放位置。返回 `{position, duration, subsong, path, hostTime}`。`hostTime` 是宿主读到这个位置时的系统时间，Unix 毫秒，与 `Date.now()` 是同一个时钟：播放时 `pos.position + (Date.now() - pos.hostTime) / 1000` 就是现在播放到的位置。[`PlaybackClock`](#playbackclock) 会持续更新这个估计。

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

报告最近一次起播所在的播放列表：`found`、`playlist`（序号）、`playlistGuid` 与 `name`。没有已知的列表时 `found` 为 `false`，`playlist` 与 `playlistGuid` 为 `null`，不带 `name`。foobar2000 在停止后仍保留这个指针，所以播放过的实例在停止态也会报告。

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

翻转静音状态，返回 `{ muted }`，即翻转后是否静音。之后广播 `playback:volumeChanged`，载荷带 `muted`。

```javascript
const res = await fb.player.toggleMute();
if (res.success) console.log(res.muted ? '已静音' : '已取消静音');
```

## toggleStopAfterCurrent()

签名：`fb.player.toggleStopAfterCurrent(): Promise<PlaybackToggleStopAfterCurrentResponse>`

翻转「播完当前曲目后停止」，返回 `{ enabled }`，即翻转后的值。选项变化时广播 `playback:stopAfterCurrentChanged`，载荷为 `{ enabled }`。

```javascript
const res = await fb.player.toggleStopAfterCurrent();
if (res.success) console.log(res.enabled ? '本曲结束后停止' : '继续播放');
```

## PlaybackClock

`import { PlaybackClock } from 'foo-webview-sdk/bridge'`；用 `<script>` 包时写 `new fb.PlaybackClock()`。

估计任一时刻 foobar2000 播放到当前曲目的哪里，供两次位置推送之间也要跟着音频走的东西用：进度条、逐字歌词、跟随播放的静音 `<video>`。宿主给每个位置都带上 `hostTime`，即读到它时的系统时间，所以位置到达时时钟知道它已经过去多久，之后用 `performance.now()` 往前推。

- 它跟随 `playback:timeHighRes`、`playback:seeked`、`playback:stateChanged` 与 `playback:trackChanged`，并在创建时、页面重新可见时和调用 `resync()` 时读一次 `playback.getState` 与 `playback.getPosition`。`ready` 在第一次读取结束后兑现，不会拒绝。
- `resync()` 默认忽略读取失败。播放必须等到新快照时，使用 `resync({ rejectOnFailure: true })`：两次调用都成功后才应用读数。宿主失败以保留 `code` 的 `ApiCallError` 拒绝，桥调用异常也会拒绝。重叠请求导致无法取得完整新快照时，严格请求可能被拒绝，恢复媒体播放前须处理失败。
- `position(at?)` 返回 `at` 时刻的估计，单位秒；`at` 用 `performance.now()` 的刻度，省略时取现在。已知时长时不超出 `duration`。`state` 为 `paused` 时位置不动，为 `stopped` 时为 `0`。
- 推送与估计相差不大时，估计只向推送挪四分之一，免得位置随宿主读数的抖动而跳。相差超过 `resyncThresholdSeconds`（默认 0.25）时直接换成推送的位置。跳转刚发生后，仍带着旧位置的推送会被忽略。
- `onChange(listener)` 报告每一次跳变：第一次读取为 `start`，另有 `seek`、`state`、`track`，以及估计被换掉或 `resync()` 读取之后的 `resync`。跟随播放的静音视频在这些时刻跳转，其余时间只调整播放速率。
- 比 `maxDeliveryDelayMs`（默认 1000）还旧的推送，以及因为系统时间中途被改而显得过旧或来自未来的推送，一律忽略。宿主不发 `hostTime` 时，推送按到达时刻算作读取时刻，估计会比音频晚一个投递延迟。
- 用完调用 `dispose()`。

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
// 之后
off();
clock.dispose();
```
