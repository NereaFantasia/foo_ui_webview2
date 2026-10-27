# Playback API

`playback` 系列 API 参考。

本页是下列命名空间的主文档。方法名、参数键与返回字段均与运行时实现保持一致。

## 播放位置与时钟

- **延迟补偿与阶梯推进**：`playback.getPosition`、`playback:timeHighRes` 与 `playback:stateChanged` 报告的播放位置均已扣除输出缓冲与 DSP 延迟，同步歌词或音视频时**无需重复扣减**。底层音频引擎的读数呈阶梯式更新（通常每 10–30 ms 前进一步），此更新步长不同于 `playback:timeHighRes` 约每 100 ms 派发一次的广播频率。
- **采样基准与时间外推**：`hostTime` 记录宿主读取位置的系统时间（Unix 毫秒，与前端 `Date.now()` 同源）。两次采样之间可通过 `hostTime` 线性外推估算位置，但无法消除起播、跳转、恢复播放或交叉渐变时的瞬态停滞与跳变；视频同步推荐使用 SDK 的 [MediaElementFollower](../sdk/media.md)。
- **时间事件判据**：`playback:time` 仅提供粗粒度的显示用位置（约每秒一次），不包含采样时间戳。`playback:seeked.position` 与 `playback.setPosition` 返回的 `actualPosition` 为设定的跳转目标；跳转后瞬时采集到的 `newPosition` 或收到的 `playback:timeHighRes` 因处于引擎稳定期，读数仍可能为旧位置。

## playback

### playback.getCurrentTrack

<!-- api-schema:begin playback.getCurrentTrack -->
报告当前载入（播放中或暂停）的曲目。恒成功，有没有曲目看 `found`。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 是否有曲目载入。 |
| `track` | [Track](../reference/types.md#track) | 载入的曲目；`found` 为 `false` 时没有这个键。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getCurrentTrack');
```

### playback.getCurrentTrackIndex

<!-- api-schema:begin playback.getCurrentTrackIndex -->
定位当前播放曲目在所在播放列表中的索引位置。仅当曲目自播放列表项起播时可用；自其他来源起播的曲目无索引位置（即使存在播放中列表）。当播放行从源列表中移除（或源列表本身被删除）后，曲目虽继续播放但索引位置丢失；停止状态下亦无索引。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | 否 | `true` 时附带该位置的曲目行。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 正在播放的曲目是否有播放列表位置。 |
| `playlist` | `integer \| null` | 播放列表序号；没有位置时为 `null`。 |
| `playlistGuid` | `string \| null` | 该播放列表的 GUID，写法与 `playlistGuid` 接受的一致；没有位置时为 `null`。 |
| `index` | `integer \| null` | 在该播放列表中的行号；没有位置时为 `null`。 |
| `track` | [Track](../reference/types.md#track) | 该位置的曲目；只在 `includeTrackInfo` 为真且该行仍存在时给出。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playback.getCurrentTrackIndex');
if (res.success === false) throw new Error(res.error);
const { playlist, index } = res;
```

### playback.getPlaybackOrder

<!-- api-schema:begin playback.getPlaybackOrder -->
报告当前播放顺序，同时给出序号与名称。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `order` | `integer` | 当前顺序的序号，`0` 到 `6`。 |
| `orderName` | `"default" \| "repeat-playlist" \| "repeat-track" \| "random" \| "shuffle-tracks" \| "shuffle-albums" \| "shuffle-folders"` | 当前顺序的名称。 |
| `name` | `"default" \| "repeat-playlist" \| "repeat-track" \| "random" \| "shuffle-tracks" \| "shuffle-albums" \| "shuffle-folders"` | 与 `orderName` 相同。 |
| `orderIndex` | `integer` | 与 `order` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getPlaybackOrder');
```

### playback.getPlayingPlaylist

<!-- api-schema:begin playback.getPlayingPlaylist -->
报告最近一次起播所在的播放列表。foobar2000 在停止后仍保留这个指针，所以播放过的实例在停止态也会报告。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 是否有已知的正在播放的列表。 |
| `playlist` | `integer \| null` | 播放列表序号；没有时为 `null`。 |
| `playlistGuid` | `string \| null` | 该播放列表的 GUID，写法与 `playlistGuid` 接受的一致；没有时为 `null`。 |
| `name` | `string` | 该播放列表的名称；没有时没有这个键。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getPlayingPlaylist');
```

### playback.getPosition

<!-- api-schema:begin playback.getPosition -->
报告播放位置，以及所在曲目的时长与身份。没有曲目在播放时全部为零或空。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `position` | `number` | 距曲目开头的可听位置，单位秒，已扣除输出缓冲与 DSP 延迟，不要再次扣减。宿主读数呈阶梯变化，通常每 10–30 ms 更新一次；起播、定位、恢复播放后及交叉渐变期间可能短暂停滞或跳变，不是采样级精确时钟。 |
| `duration` | `number` | 曲目时长，单位秒；没有曲目在播放或时长未知时为 `0`。 |
| `subsong` | `integer` | 曲目的子曲目标识；整个文件为 `0`。 |
| `path` | `string` | foobar2000 存储形式的路径，不带子曲目后缀；没有曲目在播放时为空。 |
| `hostTime` | `number` | 读 `position` 时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。播放且未暂停时，`position + (Date.now() - hostTime) / 1000` 可估出当前位置。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getPosition');
```

### playback.getState

<!-- api-schema:begin playback.getState -->
报告播放状态以及当前曲目允许的操作。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `state` | `"stopped" \| "playing" \| "paused"` | 播放状态。 |
| `canSeek` | `boolean` | 当前曲目能否定位；停止态与不支持定位的流为 `false`。 |
| `canPause` | `boolean` | 当前曲目能否暂停；恒为 `true`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getState');
```

### playback.getStopAfterCurrent

<!-- api-schema:begin playback.getStopAfterCurrent -->
报告是否在当前曲目结束后停止。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 是否在当前曲目结束后停止。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getStopAfterCurrent');
```

### playback.getVolume

<!-- api-schema:begin playback.getVolume -->
以宿主用的两种刻度报告输出音量，以及是否静音。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `volume` | `number` | 音量百分比，`0` 到 `100`，由分贝值换算；静音时为 `0`。 |
| `volumeDb` | `number` | 音量分贝值，`-100`（静音）到 `0`（最大）。 |
| `muted` | `boolean` | 是否静音；音量为 `0` 也算静音。 |
| `isMuted` | `boolean` | 与 `muted` 相同。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.getVolume');
```

### playback.mute

<!-- api-schema:begin playback.mute -->
静音或取消静音；要求的状态已经生效时什么也不做。取消静音恢复静音前的音量。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `muted` | `boolean` | 否 | `true` 静音，`false` 取消静音。默认 `true`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.mute', { muted: true });
```

### playback.next

<!-- api-schema:begin playback.next -->
按当前播放顺序跳到下一首。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.next');
```

### playback.pause

<!-- api-schema:begin playback.pause -->
暂停播放；停止状态下什么也不做。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.pause');
```

### playback.play

<!-- api-schema:begin playback.play -->
开始播放；暂停中则继续。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.play');
```

### playback.playOrPause

<!-- api-schema:begin playback.playOrPause -->
与 `playPause` 相同。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isPlaying` | `boolean` | 切换后是否在播放。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.playOrPause');
```

### playback.playPath

<!-- api-schema:begin playback.playPath -->
追加单个文件至活动播放列表并立即播放；无活动列表时自动创建。支持以 `|subsong:N` 后缀指定子曲目序号。若活动列表已锁定，以 `LOCKED` 失败且不修改列表或起播。路径未通过媒体根安全检查时以 `PERMISSION_DENIED` 失败；若宿主无法解析为有效曲目，以 `NOT_FOUND` 失败（附带 `path` 与 `subsong`）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 文件路径，可带 `\|subsong:N` 后缀。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracksAdded` | `integer` | 加入播放列表的行数；为 `1`。 |
| `path` | `string` | 去掉子曲目后缀的文件路径。 |
| `subsong` | `integer` | 实际播放曲目的子曲目标识：用 `\|subsong:N` 请求的就是它，即便 foobar2000 没把它列为文件的子曲目；不带后缀时是文件的第一个。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.playPath', { path: 'C:\\Music\\song.flac' });
```

用 `path|subsong:N` 形式可以指定 CUE 中的某个子曲目。handler 会先把文件路径和可选的 subsong 后缀拆开，再开始播放。

### playback.playPaths

<!-- api-schema:begin playback.playPaths -->
批量添加文件至活动播放列表并起播其中一首。非字符串或未通过媒体根检查的项被跳过并计入 `skippedPaths`（仅成功响应包含此项）；全部项被跳过时以 `INVALID_PARAMS` 失败。其余路径按字面添加为曲目（不展开目录或播放列表文件，亦不校验文件是否存在）。若无法生成任何曲目以 `NOT_FOUND` 失败。活动列表锁定则以 `LOCKED` 失败，不执行任何清空、添加或播放。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 文件路径，每条可带 `\|subsong:N` 后缀。不能为空。 |
| `startIndex` | `integer` | 否 | 从加入的第几首起播，按解析成功的曲目计数；超出范围时从第一首起播。不小于 `0`。默认 `0`。 |
| `replace` | `boolean` | 否 | `true` 先清空活动播放列表再加入；`false` 追加。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `tracksAdded` | `integer` | 加入播放列表的曲目数。 |
| `startedAt` | `integer` | 传入的 `startIndex` 原样回显；它超出范围、实际从第一首起播时也是如此。 |
| `skippedPaths` | `integer` | 调用前被路径安全检查丢弃的路径数；只在至少丢弃一条时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.playPaths', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```

### playback.playPause

<!-- api-schema:begin playback.playPause -->
在播放与暂停之间切换；停止状态下开始播放。报告切换后的状态。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isPlaying` | `boolean` | 切换后是否在播放。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.playPause');
```

### playback.previous

<!-- api-schema:begin playback.previous -->
按当前播放顺序跳到上一首。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.previous');
```

### playback.random

<!-- api-schema:begin playback.random -->
从活动播放列表里随机起播一首。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.random');
```

### playback.setPlaybackOrder

<!-- api-schema:begin playback.setPlaybackOrder -->
按序号或名称选择播放顺序；两者必须恰好给一个。报告设置之后生效的顺序。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `order` | `integer` | 否 | 按序号选择，`0` 到 `6`，顺序同 `PlaybackOrderName`。取值 `0` 到 `6`（含端点）。 |
| `name` | `"default" \| "repeat-playlist" \| "repeat-track" \| "random" \| "shuffle-tracks" \| "shuffle-albums" \| "shuffle-folders"` | 否 | 按名称选择。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `order` | `integer` | 调用后生效的顺序序号。 |
| `orderName` | `"default" \| "repeat-playlist" \| "repeat-track" \| "random" \| "shuffle-tracks" \| "shuffle-albums" \| "shuffle-folders"` | 调用后生效的顺序名称。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`order`（序号）与 `name`（名称）必须恰好给一个；序号 `0` default、`1` repeat-playlist、`2` repeat-track、`3` random、`4` shuffle-tracks、`5` shuffle-albums、`6` shuffle-folders。序号越界、名称不存在、两个键同时给或都不给，都以 `INVALID_PARAMS` 拒绝且顺序不变。响应报告调用后生效的顺序，不是回显输入。

```js
await fb2k.invoke('playback.setPlaybackOrder', { name: 'random' });
```

### playback.setPosition

<!-- api-schema:begin playback.setPosition -->
在当前曲目内定位，播放中或暂停中均可。位置会被夹到曲目范围内：负数定位到开头，超过时长的值停在结尾稍前，免得跳到下一首；时长未知的曲目没有上限。没在播放时以 `NO_ACTIVE_ITEM` 失败，不能定位的曲目（大多数网络流）以 `NOT_SUPPORTED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `position` | `number` | 是 | 目标位置，单位秒。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `requestedPosition` | `number` | 请求的位置，未夹取。 |
| `actualPosition` | `number` | 夹到曲目范围内后交给宿主的位置。 |
| `oldPosition` | `number` | 定位前读到的位置，单位秒，与 `playback.getPosition` 一样已补偿输出延迟。 |
| `newPosition` | `number` | 定位后立即读到的位置，单位秒，与 `playback.getPosition` 一样已补偿输出延迟；引擎可能尚未稳定，因此仍可能是旧位置。 |
| `hostTime` | `number` | 读 `newPosition` 时宿主的系统时间，Unix 纪元毫秒，带小数，与 `Date.now()` 是同一个时钟。 |
| `duration` | `number` | 曲目时长，单位秒；时长未知时为 `0` 或更小。 |
| `subsong` | `integer` | 曲目的子曲目标识。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.setPosition', { position: 42.5 });
```

### playback.setStopAfterCurrent

<!-- api-schema:begin playback.setStopAfterCurrent -->
设置或取消在当前曲目结束后停止。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `enabled` | `boolean` | 是 | `true` 表示当前曲目结束后停止。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 现在生效的值。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.setStopAfterCurrent', { enabled: true });
```

### playback.setVolume

<!-- api-schema:begin playback.setVolume -->
按百分比设置输出音量；超出 `0` 到 `100` 的值被夹到范围内。`0` 即静音。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `volume` | `number` | 是 | 音量百分比；夹到 `0` 到 `100`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playback.setVolume', { volume: 80 });
```

### playback.stop

<!-- api-schema:begin playback.stop -->
停止播放。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.stop');
```

### playback.toggleMute

<!-- api-schema:begin playback.toggleMute -->
翻转静音状态并报告新状态。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `muted` | `boolean` | 翻转后是否静音。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.toggleMute');
```

### playback.toggleStopAfterCurrent

<!-- api-schema:begin playback.toggleStopAfterCurrent -->
翻转「当前曲目后停止」并报告新值。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `enabled` | `boolean` | 翻转后的值。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.toggleStopAfterCurrent');
```

### playback.volumeDown

<!-- api-schema:begin playback.volumeDown -->
按宿主的步长调低音量：一分贝，落在整数分贝上。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.volumeDown');
```

### playback.volumeUp

<!-- api-schema:begin playback.volumeUp -->
按宿主的步长调高音量：一分贝，落在整数分贝上。

无参数。

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playback.volumeUp');
```

## 相关事件

- `playback:stopAfterCurrentChanged`：单曲播放结束停止选项变更事件，payload 为 `{ enabled }`，字段名与 API 返回值一致。
- 完整的播放事件清单与参数定义参见[全局事件参考](events.md#playback-事件)。
