# Playback API

`playback` 系列 API 参考。

本页是下列命名空间的主文档。方法名、参数键与返回字段均与运行时实现保持一致。

## playback

### playback.getCurrentTrack


_无参数。_

**返回值**: `{"found":"...","playing":"...","success":true}`

```js
const result = await fb2k.invoke('playback.getCurrentTrack');
```

### playback.getCurrentTrackIndex


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `includeTrackInfo` | `boolean` | 否 | `false` | 附带 `track` 曲目信息对象。 |

**返回值**: `{"found":true,"index":0,"playlist":0,"success":true,"track":{}}`

```js
const { playlist, index } = await fb2k.invoke('playback.getCurrentTrackIndex');
```

### playback.getPlaybackOrder


_无参数。_

**返回值**: `{"name":"...","order":"...","orderIndex":"...","orderName":"..."}`

```js
const result = await fb2k.invoke('playback.getPlaybackOrder');
```

### playback.getPlayingPlaylist


_无参数。_

**返回值**: `{"found":"...","name":"...","playlist":"...","success":true}`

```js
const result = await fb2k.invoke('playback.getPlayingPlaylist');
```

### playback.getPosition


_无参数。_

**返回值**: `{"duration":"...","path":"...","position":"...","subsong":"..."}`

```js
const result = await fb2k.invoke('playback.getPosition');
```

### playback.getState


_无参数。_

**返回值**: `{"canPause":"...","canSeek":"...","state":"..."}`

```js
const result = await fb2k.invoke('playback.getState');
```

### playback.getStopAfterCurrent


_无参数。_

**返回值**: `{"enabled":"..."}`

```js
const result = await fb2k.invoke('playback.getStopAfterCurrent');
```

### playback.getVolume


_无参数。_

**返回值**: `{"isMuted":"...","muted":"...","volume":"...","volumeDb":"..."}`

```js
const result = await fb2k.invoke('playback.getVolume');
```

### playback.mute


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `muted` | `boolean` | 否 | `true` | `false` 表示取消静音。 |

**返回值**: `{"success":true}`

```js
await fb2k.invoke('playback.mute', { muted: true });
```

### playback.next


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.next');
```

### playback.pause


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.pause');
```

### playback.play


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.play');
```

### playback.playOrPause


_无参数。_

**返回值**: `{"isPlaying":"...","success":true}`

```js
const result = await fb2k.invoke('playback.playOrPause');
```

### playback.playPath


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要播放的文件路径；缺失或为空时返回 `path is required`。 |

**返回值**: `{"error":"...","path":"...","subsong":"...","success":true,"tracksAdded":"..."}`

```js
await fb2k.invoke('playback.playPath', { path: 'C:\\Music\\song.flac' });
```

用 `path|subsong:N` 形式可以指定 CUE 中的某个子曲目。handler 会先把文件路径和可选的 subsong 后缀拆开，再开始播放。

### playback.playPaths


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `paths` | `array<string>` | 是 | — | 文件路径或 URL 数组。 |
| `startIndex` | `integer` | 否 | `0` | 添加后开始播放的下标。 |
| `replace` | `boolean` | 否 | `false` | 为 `true` 时替换现有内容而非追加。 |

**返回值**: `{"error":"...","startedAt":"...","success":true,"tracksAdded":"..."}`

```js
await fb2k.invoke('playback.playPaths', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```

### playback.playPause


_无参数。_

**返回值**: `{"isPlaying":"...","success":true}`

```js
const result = await fb2k.invoke('playback.playPause');
```

### playback.previous


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.previous');
```

### playback.random


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.random');
```

### playback.setPlaybackOrder


| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `order` | `integer \| string` | 否 | 顺序索引或名称，默认 `0`（`default`）。 |

**返回值**: `{ "success": true, "order": 3, "orderName": "random" }`

索引与名称均可传入：`0` default、`1` repeat-playlist、`2` repeat-track、`3` random、`4` shuffle-tracks、`5` shuffle-albums、`6` shuffle-folders。无法识别的名称回退为 `0`。越界索引不做校验，原样回显于 `order`，而 `orderName` 报告为 `default`——需要确认实际生效值时请读取返回的这一对字段。

```js
await fb2k.invoke('playback.setPlaybackOrder', { order: 'random' });
```

### playback.setPosition


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `position` | `number` | 否 | `0` | 目标位置（秒）。 |
| `seconds` | `number` | 否 | `0` | `position` 的别名，`position` 存在时被忽略。 |

**返回值**: `{"actualPosition":"...","duration":"...","error":"...","newPosition":"...","oldPosition":"...","requestedPosition":"...","subsong":"...","success":true}`

```js
await fb2k.invoke('playback.setPosition', { position: 42.5 });
```

### playback.setStopAfterCurrent


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `enabled` | `boolean` | 否 | `false` |  |

**返回值**: `{"enabled":"...","success":true}`

```js
await fb2k.invoke('playback.setStopAfterCurrent', { enabled: true });
```

### playback.setVolume


| 参数 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `volume` | `number` | 否 | `100` | 音量百分比（0–100）。 |

**返回值**: `{"success":true}`

```js
await fb2k.invoke('playback.setVolume', { volume: 80 });
```

### playback.stop


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.stop');
```

### playback.toggleMute


_无参数。_

**返回值**: `{"muted":"...","success":true}`

```js
const result = await fb2k.invoke('playback.toggleMute');
```

### playback.toggleStopAfterCurrent


_无参数。_

**返回值**: `{"enabled":"..."}`

```js
const result = await fb2k.invoke('playback.toggleStopAfterCurrent');
```

### playback.volumeDown


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.volumeDown');
```

### playback.volumeUp


_无参数。_

**返回值**: `{"success":true}`

```js
const result = await fb2k.invoke('playback.volumeUp');
```

## 相关事件

事件 `playback:stopAfterCurrentChanged` 的 payload 为 `{ enabled }`，字段名与 API 返回值相同。
