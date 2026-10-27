# Playlist API

播放列表管理、曲目操作、智能播放列表和工具函数。

## 指定播放列表

操作单个播放列表的 API 支持通过 `playlist`（从 0 开始的序号）或 `playlistGuid`（唯一标识）指定目标。两者互斥；同时传入将以 `INVALID_PARAMS` 失败。

- **序号（`playlist`）**：表示调用执行时列表所在的瞬时位置。列表增删或重排可能导致序号漂移。
- **GUID（`playlistGuid`）**：在播放列表创建时分配并全局唯一。即使列表重命名或位置变动，GUID 始终跟随列表；目标不存在时以 `NOT_FOUND` 失败。

返回播放列表信息的方法（`getAll`、`getActive`、`getPlaying`、`create`、`duplicate`、`createAutoplaylist`）均携带 `guid`；按序号指明列表的结果另附 `playlistGuid`。`playlist:*` 事件在载荷中以 GUID 标识列表。跨命名空间接口（如 `library.addToPlaylist`、`queue.add`）同样支持传入 `playlistGuid`。

```javascript
const res = await fb2k.invoke('playlist.getAll');
if (res.success === false) throw new Error(res.error);
const target = res.playlists.find((p) => p.name === '收藏');
if (target) {
    await fb2k.invoke('library.addToPlaylist', {
        paths: ['E:\\Music\\song.flac'],
        playlistGuid: target.guid,
    });
}
```

## 列表管理

### playlist.getCount

<!-- api-schema:begin playlist.getCount -->
报告有多少个播放列表。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 播放列表个数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const res = await fb2k.invoke('playlist.getCount');
if (res.success === false) throw new Error(res.error);
const { count } = res;
```

### playlist.getAll

<!-- api-schema:begin playlist.getAll -->
按顺序列出全部播放列表。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlists` | `PlaylistInfo[]` | 全部播放列表，按顺序。 |
| `playlists[].index` | `integer` | 在播放列表清单中的位置，从 `0` 起。 |
| `playlists[].guid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `playlists[].name` | `string` | 播放列表名称。 |
| `playlists[].trackCount` | `integer` | 曲目数。 |
| `playlists[].isActive` | `boolean` | 是否为活动播放列表。 |
| `playlists[].isPlaying` | `boolean` | 是否为正在播放的播放列表。 |
| `playlists[].isLocked` | `boolean` | 播放列表是否带锁。 |
| `playlists[].isAutoplaylist` | `boolean` | 是否为自动播放列表，口径同 `playlist.isAutoplaylist`。 |
| `count` | `integer` | `playlists` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

::: warning Breaking Change (v1.1.18)
`playlist.getAll` 不再返回 `duration` 字段（避免 N 个播放列表 × M 首曲目的全量加载开销）。如需获取单个播放列表的 duration，请使用 `playlist.getActive` 或 `playlist.getPlaying`。
:::

::: tip v1.1.18+
`isAutoplaylist` 字段已内联到 `playlist.getAll` 返回值中，无需再逐个调用 `playlist.isAutoplaylist`。
:::

### playlist.getActive

<!-- api-schema:begin playlist.getActive -->
描述活动播放列表，即用户正在看的那个。没有时 `found` 为 `false`，其余字段不出现。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 是否有活动播放列表。 |
| `index` | `integer` | 活动播放列表的索引。 |
| `guid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `name` | `string` | 它的名称。 |
| `trackCount` | `integer` | 曲目数。 |
| `isActive` | `boolean` | 这里总是 `true`。 |
| `isPlaying` | `boolean` | 它是否同时是正在播放的播放列表。 |
| `isLocked` | `boolean` | 它是否带锁。 |
| `duration` | `number` | 曲目总时长，单位秒；时长未知的曲目按 `0` 计。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### playlist.setActive

<!-- api-schema:begin playlist.setActive -->
把一个播放列表设为活动列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 要设为活动的播放列表的索引。给它或 `playlistGuid` 之一；两个都没给时以 `INVALID_PARAMS` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('playlist.setActive', { playlist: 1 });
```

### playlist.getPlaying

<!-- api-schema:begin playlist.getPlaying -->
描述正在播放的播放列表，即播放从中取下一首的那个。没有时 `found` 为 `false`，其余字段不出现。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `found` | `boolean` | 是否有正在播放的播放列表。 |
| `index` | `integer` | 正在播放的播放列表的索引。 |
| `guid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `name` | `string` | 它的名称。 |
| `trackCount` | `integer` | 曲目数。 |
| `isActive` | `boolean` | 它是否同时是活动播放列表。 |
| `isPlaying` | `boolean` | 这里总是 `true`。 |
| `isLocked` | `boolean` | 它是否带锁。 |
| `duration` | `number` | 曲目总时长，单位秒；时长未知的曲目按 `0` 计。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### playlist.create

<!-- api-schema:begin playlist.create -->
新建一个空播放列表。foobar2000 拒绝创建时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 否 | 新播放列表的名称。默认 `"New Playlist"`。 |
| `position` | `integer` | 否 | 插入到播放列表清单中的位置；省略时放在末尾。不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `integer` | 新播放列表的索引。 |
| `guid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const result = await fb2k.invoke('playlist.create', { name: 'Rock Music' });
```

### playlist.remove

<!-- api-schema:begin playlist.remove -->
删除一个播放列表。列表上的锁拒绝删除时以 `LOCKED` 失败，foobar2000 因其他原因拒绝时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 要删除的播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### playlist.rename

<!-- api-schema:begin playlist.rename -->
重命名一个播放列表。列表上的锁拒绝新名字时以 `LOCKED` 失败，foobar2000 因其他原因拒绝时以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 要重命名的播放列表的索引。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败。给它或 `playlistGuid` 之一；两个都没给时以 `INVALID_PARAMS` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `name` | `string` | 是 | 新名称。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('playlist.rename', { playlist: 0, name: 'My Favorites' });
```

### playlist.clear

<!-- api-schema:begin playlist.clear -->
移除播放列表里的全部曲目，事先保存撤销点。已上锁的列表在任何改动之前以 `LOCKED` 失败；清除后仍有曲目时以 `OPERATION_FAILED` 失败，失败里带与成功时相同的字段。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 要清空的播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `clearedCount` | `integer` | 清空前的曲目数。 |
| `remainingCount` | `integer` | 剩下的曲目数；成功时为 `0`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### playlist.duplicate

<!-- api-schema:begin playlist.duplicate -->
把一个播放列表连同曲目复制成新列表，放在原列表紧后面。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 要复制的播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `name` | `string` | 否 | 副本的名称；省略或为空时为原名称后加 ` (Copy)`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `integer` | 副本的索引。 |
| `sourcePlaylist` | `integer` | 被复制的播放列表的索引。 |
| `sourcePlaylistGuid` | `string` | 被复制的播放列表的 GUID，写法与 `playlistGuid` 接受的一致。 |
| `newPlaylist` | `integer` | 副本的索引；与 `index` 相同。 |
| `guid` | `string` | 副本的 GUID，写法与 `playlistGuid` 接受的一致。 |
| `name` | `string` | 副本的名称。 |
| `trackCount` | `integer` | 复制的曲目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 曲目操作

### playlist.getTrackCount

<!-- api-schema:begin playlist.getTrackCount -->
报告一个播放列表有多少首曲目。超出最后一个播放列表的索引、没有列表持有的 `playlistGuid`，或两者都省略且没有活动播放列表时，报告 `0` 而不是失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识，用于替代序号。目标不存在时返回空结果而非报错。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 曲目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### playlist.getTracks

<!-- api-schema:begin playlist.getTracks -->
播放列表的一页行。不带 `fields` 时每行是完整的播放列表行，并带上 foo_playcount 提供的播放统计；`fields` 把每行收窄到点名的键（可用的名字与 `library.query` 相同），`index` 总会保留。播放列表索引超出最后一个、没有列表持有的 `playlistGuid`，以及两者都省略且没有活动播放列表时，回答一个空页。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表索引；省略时为活动播放列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识，用于替代序号。目标不存在时返回空结果而非报错。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。 |
| `start` | `integer` | 否 | 返回的第一行。不小于 `0`。默认 `0`。 |
| `count` | `integer` | 否 | 最多返回的行数。不小于 `0`。默认 `100`。 |
| `formats` | `Record<string, string>` | 否 | 额外的列，每列是一个 Title Formatting 模式，列名自定；每行（包括投影的行）在 `formats` 下带上它们的值。 |
| `fields` | `string[]` | 否 | 每行除 `index` 外带的键；省略时为整行。可用的名字与 `library.query` 相同；未知的名字以 `INVALID_PARAMS` 失败，并列在 `details.unknownFields` 里。没有可选的 `albumId` 时省略该键，即使请求了它也是如此。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 读取的播放列表（给定的或解析出的）。没有可指的列表时为 `-1`：省略 `playlist` 且没有活动播放列表，或者没有列表持有 `playlistGuid`。超出最后一个播放列表的索引原样返回。 |
| `playlistGuid` | `string` | 读取的播放列表的 GUID，写法与 `playlistGuid` 接受的一致；列表不存在时不出现，凭它缺席就能把「列表不存在」与「列表为空」分开。 |
| `start` | `integer` | 生效的 `start`。 |
| `count` | `integer` | 返回的行数。 |
| `total` | `integer` | 播放列表的行数；列表不存在时为 `0`。 |
| `tracks` | `PlaylistTrackPartial[]` | 从 `start` 起的行：整行，或 `index` 加上 `fields` 要求的键。 |
| `tracks[].handle` | `string` | 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `\|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。 |
| `tracks[].path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `tracks[].absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `tracks[].subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `tracks[].title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `tracks[].artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `tracks[].album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `tracks[].albumId` | `string` | 原始专辑名或专辑艺术家含非法 UTF-8 时提供的不透明身份标识。此时用它匹配专辑行，不用显示名称；普通名称不带此字段。 |
| `tracks[].albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].albumArtists` | `string[]` | 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。存在 `albumId` 时应改用它匹配，因为替换字符可能让不同原始名称显示相同。 |
| `tracks[].genre` | `string` | 全部 GENRE 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].date` | `string` | 第一个 DATE 值，按标签原样，如 `2019` 或 `2019-05-01`；没有标签时为空。 |
| `tracks[].trackNumber` | `integer` | TRACKNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].discNumber` | `integer` | DISCNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].duration` | `number` | 时长，单位秒；未知时为 `0`。 |
| `tracks[].fileSize` | `integer` | 文件大小，单位字节；未知（如网络流）时为 `-1`。 |
| `tracks[].bitrate` | `integer` | 平均码率，单位 kbit/s；未知时为 `0`。 |
| `tracks[].sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `tracks[].channels` | `integer` | 声道数；未知时为 `0`。 |
| `tracks[].codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |
| `tracks[].rating` | `integer` | 评分 0 到 5：`%rating%` 统计值（foo_playcount）在 1 到 5 之间时用它，否则用 RATING 标签并夹到该范围；两处都没有评分时为 `0`。 |
| `tracks[].index` | `integer` | 在播放列表中的行号，从 `0` 起。 |
| `tracks[].composer` | `string` | 全部 COMPOSER 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].comment` | `string` | 第一个 COMMENT 值；没有标签时为空。 |
| `tracks[].playCount` | `integer` | foo_playcount 的 `%play_count%`；它给不出数字时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].firstPlayed` | `string` | foo_playcount 格式化的 `%first_played%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].lastPlayed` | `string` | foo_playcount 格式化的 `%last_played%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].added` | `string` | foo_playcount 格式化的 `%added%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].formats` | `Record<string, string>` | `playlist.getTracks` 的 `formats` 列，按给定的列名；只在传了 `formats` 时出现。 |
| `contentVersion` | `string` | 各页的此值相同才能拼接。列表不存在时，应答里没有此字段。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

> `rating` 优先读取 `%rating%`，采用 `1` 至 `5` 的有效值。该值缺失或越界时，改读文件的 `RATING` 标签并限制到 `0` 至 `5`；两处都无评分时返回 `0`（未评分）。`rating.get` 使用相同规则：采用 `%rating%` 值时 `storage` 为 `'stats'`，其余情况为 `'file'`，包括文件标签不存在的情况。

::: tip 附加列（`formats` 参数）
`formats` 把列名映射到 Title Formatting 模式；每行（包括投影的行）在自己的 `formats` 对象里带上求值结果：

```javascript
const result = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 50,
    formats: {
        myRating: '%rating%',
        codec: '%codec%'
    }
});
// 每行多出 formats: { myRating: '5', codec: 'FLAC' }
```
:::

::: tip
`absolutePath` 是本地文件系统路径，可直接用于 `artwork.getForTrack` 等 API。`path` 是 foobar2000 内部格式。
:::

::: tip 字段投影（`fields` 参数）
传入 `fields` 后只返回所选字段，`index` 始终保留。

```javascript
const page = await fb2k.invoke('playlist.getTracks', {
    start: 0, count: 200,
    fields: ['title', 'artist', 'album', 'duration', 'path']
});
```

可用的名字是 `index`、`handle`、`title`、`artist`、`artists`、`album`、`albumArtist`、`albumArtists`、`albumId`、`genre`、`date`、`trackNumber`、`discNumber`、`duration`、`path`、`absolutePath`、`fileSize`、`bitrate`、`sampleRate`、`channels`、`codec`、`subsong` 与 `rating`。名字精确匹配且区分大小写，出现未知名整次调用以 `INVALID_PARAMS` 失败，并在 `details.unknownFields` 里列出它们——拼错不会静默丢字段。请求了可选的 `albumId`，但曲目没有该标识时，结果会省略这个键。`composer`、`comment` 与播放统计不接受，它们只随整行返回。

不要 `handle`、`absolutePath` 与 `rating` 可以省掉逐行解析文件系统路径和求值 `%rating%`。投影时要播放统计，通过 `formats` 请求。
:::

### playlist.getGroupRuns

<!-- api-schema:begin playlist.getGroupRuns -->
把整个播放列表切成相邻且分组键相等的若干游程，ASCII 字母不区分大小写；第二个模式在每个游程内再分组。不会重排行，只回游程边界，所以回答随游程数而不是行数增长。分组键在主线程之外求值。多于两个模式、空模式或编译失败的模式以 `INVALID_PARAMS` 失败，后两者带 `details.pattern` 指出位置。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败并以 `details.playlist` 带回请求的索引，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败并带 `details.playlistGuid`；两者都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。求分组键时出错以 `OPERATION_FAILED` 失败，失败里带空的 `runs` 与为 `0` 的 `total`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `patterns` | `string[]` | 是 | 一个或两个 Title Formatting 模式；第二个在每个游程内再分组。不能为空。 |
| `playlist` | `integer` | 否 | 播放列表索引；省略时为活动播放列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 分组的播放列表。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `total` | `integer` | 播放列表的行数；各游程的 `count` 相加等于它。 |
| `runs` | `PlaylistGroupRun[]` | 各游程，按列表顺序，第一个从第 `0` 行开始；空列表时为空。 |
| `runs[].start` | `integer` | 游程的第一行。 |
| `runs[].count` | `integer` | 游程的行数。 |
| `runs[].key` | `string` | 分组键，取游程第一行的写法。 |
| `runs[].sub` | `PlaylistGroupSubRun[]` | 这一游程内的第二级游程；只在给了两个模式时出现。 |
| `runs[].sub[].start` | `integer` | 游程的第一行。 |
| `runs[].sub[].count` | `integer` | 游程的行数。 |
| `runs[].sub[].key` | `string` | 第二级分组键，取游程第一行的写法。 |
| `contentVersion` | `string` | 这些游程对应列表处于此版本时的各行。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

给两条模式时每段游程还带 `sub`，且子游程的 `start` 是**绝对行号**，不是父游程内的偏移：

```json
{
    "start": 12,
    "count": 9,
    "key": "乙专辑 | 乙艺术家",
    "sub": [
        { "start": 12, "count": 5, "key": "1" },
        { "start": 17, "count": 4, "key": "2" }
    ]
}
```

键的比较只对 ASCII `A-Z`/`a-z` 不区分大小写，其余内容逐字节比较。空键也是普通分组键：模式求值为空字符串时仍会形成游程，占位文案由界面处理。

::: tip 驱动分组虚拟列表
与 `playlist.getTracks` 搭配：游程提供全部组头位置，界面据此计算滚动总高度，可见页的曲目行另行获取。响应大小取决于游程数量、分组键长度及二级分组，而非完整曲目元数据。若模式让每行单独成组，`runs` 就会与列表一样长，宿主对此不设上限。与同一列表的页响应比较 `total`，只能发现曲目数量变化；两者相等不能排除两次独立读取之间的等量替换或重排。
:::

### playlist.getMatchingRows

<!-- api-schema:begin playlist.getMatchingRows -->
播放列表里曲目符合 foobar2000 查询的行，按列表顺序；不管列表多长都只要一次调用，各行本身用 `playlist.getTracksAt` 读取。查询用媒体库搜索的语法，不收 `SORT BY`。解析器拒绝的查询以 `INVALID_PARAMS` 失败并带 `details.param` 为 `query`，带 `SORT BY` 的同样如此；很多畸形查询并不会被拒绝，只是什么也匹配不到。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败，两者都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。回答是在主线程上取的快照：改动这张列表或曲目的标签都可能改变它，`%added% DURING LAST 2 WEEKS` 这类查询还会随时间变化，这一种没有事件通知。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `query` | `string` | 是 | foobar2000 查询，写法同媒体库搜索；不收 `SORT BY`。不能为空。 |
| `playlist` | `integer` | 否 | 播放列表索引；省略时为活动播放列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 搜索的播放列表。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `total` | `integer` | 查询时播放列表的行数。与之后 `playlist.getTracks` 的 `total` 相等不代表列表没变：重排与改标签都不改变行数。 |
| `items` | `integer[]` | 命中的行，升序。 |
| `count` | `integer` | `items` 的条目数。 |
| `contentVersion` | `string` | 列表仍是此版本时，`items` 中的行号才有效。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

不管列表有多少行，筛选都只要两次调用：一次取命中的行号，再用 `playlist.getTracksAt` 取屏幕上那几行。

```javascript
// 屏幕上那张列表，用 playlist.getAll 报告的 guid 指定
const playlistGuid = '{A9624480-77F0-4A2B-A76F-7EAB0C2EEC1C}';
const hits = await fb2k.invoke('playlist.getMatchingRows', {
    playlistGuid,
    query: 'artist HAS nachi OR title HAS nachi',
});
if (hits.success) {
    const page = await fb2k.invoke('playlist.getTracksAt', {
        playlistGuid,
        rows: hits.items.slice(0, 50),
        fields: ['title', 'artist', 'duration'],
    });
}
```

查询里的小写字母匹配标签里的任意大小写，大写字母只匹配同样的大写，所以用户输入的词先转成小写。`HAS` 把 `*` 和 `?` 当普通字符，`IS` 把它们当通配符。带引号的值里写不了双引号。

### playlist.getTracksAt

<!-- api-schema:begin playlist.getTracksAt -->
按行号挑出的播放列表行，按传入顺序，例如 `playlist.getMatchingRows` 回答的那些不一定相邻的行。每行的形状与 `playlist.getTracks` 相同并带 `index`；超出最后一行的行号跳过，给了两次的行返回两次。播放列表索引超出最后一个、没有列表持有的 `playlistGuid`，以及两者都省略且没有活动播放列表时，与 `playlist.getTracks` 一样回答空结果。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `rows` | `integer[]` | 是 | 要读的行，按返回的顺序。每一项不小于 `0`。 |
| `playlist` | `integer` | 否 | 播放列表索引；省略时为活动播放列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识，用于替代序号。目标不存在时返回空结果而非报错。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。 |
| `formats` | `Record<string, string>` | 否 | 额外的列，同 `playlist.getTracks`。 |
| `fields` | `string[]` | 否 | 每行除 `index` 外带的键，同 `playlist.getTracks`；省略时为整行。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 读取的播放列表，同 `playlist.getTracks` 结果里的 `playlist`。 |
| `playlistGuid` | `string` | 读取的播放列表的 GUID；列表不存在时不出现，同 `playlist.getTracks`。 |
| `total` | `integer` | 播放列表的行数；列表不存在时为 `0`。 |
| `count` | `integer` | 返回的行数。 |
| `tracks` | `PlaylistTrackPartial[]` | 按请求顺序排列的行，超出最后一行的已跳过。 |
| `tracks[].handle` | `string` | 跨端点识别同一首曲目的键：`absolutePath`，`subsong` 不为 `0` 时带 `\|subsong:N` 后缀。同一首曲目在这些方法里得到同一个 handle；因为带后缀，它不是普通文件路径，接收路径的方法会各自说明是否接受它。 |
| `tracks[].path` | `string` | foobar2000 存储的路径：本地文件是 `file://`，相对 foobar2000 目录存储的路径（便携安装）是 `file-relative://`，网络流是 URL。不带子曲目后缀。 |
| `tracks[].absolutePath` | `string` | 不带子曲目后缀的原生文件系统路径；网络流与 `path` 相同。 |
| `tracks[].subsong` | `integer` | 解码器在文件内分配的子曲目标识，不一定是连续序号；整个文件和网络流都是 `0`。 |
| `tracks[].title` | `string` | 第一个 TITLE 值；没有标签时为空。 |
| `tracks[].artist` | `string` | 全部 ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].artists` | `string[]` | 全部 ARTIST 值，按标签顺序；没有标签时为空数组。 |
| `tracks[].album` | `string` | 第一个 ALBUM 值；没有标签时为空。 |
| `tracks[].albumId` | `string` | 原始专辑名或专辑艺术家含非法 UTF-8 时提供的不透明身份标识。此时用它匹配专辑行，不用显示名称；普通名称不带此字段。 |
| `tracks[].albumArtist` | `string` | 全部 ALBUM ARTIST 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].albumArtists` | `string[]` | 全部 ALBUM ARTIST 值，按标签顺序，`albumArtists.join(", ")` 等于 `albumArtist`；没有标签时为空数组。`library.getAlbums` 把有 `album` 的曲目归入名为 `album`、专辑艺术家为 `albumArtists[0]` 的专辑，本数组为空时取 `artists[0]`（两者都空时为 `""`）；这两个值就是该专辑行的 `name` 与 `albumArtist`。存在 `albumId` 时应改用它匹配，因为替换字符可能让不同原始名称显示相同。 |
| `tracks[].genre` | `string` | 全部 GENRE 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].date` | `string` | 第一个 DATE 值，按标签原样，如 `2019` 或 `2019-05-01`；没有标签时为空。 |
| `tracks[].trackNumber` | `integer` | TRACKNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].discNumber` | `integer` | DISCNUMBER 按整数读取；缺失或不是数字时为 `0`。 |
| `tracks[].duration` | `number` | 时长，单位秒；未知时为 `0`。 |
| `tracks[].fileSize` | `integer` | 文件大小，单位字节；未知（如网络流）时为 `-1`。 |
| `tracks[].bitrate` | `integer` | 平均码率，单位 kbit/s；未知时为 `0`。 |
| `tracks[].sampleRate` | `integer` | 采样率，单位 Hz；未知时为 `0`。 |
| `tracks[].channels` | `integer` | 声道数；未知时为 `0`。 |
| `tracks[].codec` | `string` | 解码器报告的编码名，如 `FLAC`、`MP3`；未知时为空。 |
| `tracks[].rating` | `integer` | 评分 0 到 5：`%rating%` 统计值（foo_playcount）在 1 到 5 之间时用它，否则用 RATING 标签并夹到该范围；两处都没有评分时为 `0`。 |
| `tracks[].index` | `integer` | 在播放列表中的行号，从 `0` 起。 |
| `tracks[].composer` | `string` | 全部 COMPOSER 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].comment` | `string` | 第一个 COMMENT 值；没有标签时为空。 |
| `tracks[].playCount` | `integer` | foo_playcount 的 `%play_count%`；它给不出数字时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].firstPlayed` | `string` | foo_playcount 格式化的 `%first_played%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].lastPlayed` | `string` | foo_playcount 格式化的 `%last_played%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].added` | `string` | foo_playcount 格式化的 `%added%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].formats` | `Record<string, string>` | `playlist.getTracks` 的 `formats` 列，按给定的列名；只在传了 `formats` 时出现。 |
| `contentVersion` | `string` | 列表仍是此版本时，行号才有效。列表不存在时，应答里没有此字段。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### playlist.playTrack

<!-- api-schema:begin playlist.playTrack -->
播放播放列表中的一行，与双击它相同。超出最后一行的行号以 `INVALID_INDEX` 失败。给了 `deferred` 时，开始播放前会再比对一次 `expectedVersion`；那时行已变过就不播放。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `index` | `integer` | 否 | 要播放的行。不小于 `0`。默认 `0`。 |
| `deferred` | `boolean` | 否 | 在当前消息处理完之后再开始播放，而不是在调用返回之前。开始播放时按 GUID 重新找这张列表：期间被挪动，照样播放这张列表；期间被删掉，或那时已不到 `index` 行，就什么也不播。默认 `false`。 |
| `muted` | `boolean` | 否 | 开始播放前先静音，让紧接着设置音量的页面不留下能听见的间隙。本调用不会取消这次静音。默认 `false`。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 5 });

// 延迟执行（流媒体场景推荐）
await fb2k.invoke('playlist.playTrack', { playlist: 0, index: 0, deferred: true });
```

### playlist.removeTracks

<!-- api-schema:begin playlist.removeTracks -->
从播放列表移除行，事先保存撤销点。超出最后一行的行号忽略，负数行号以 `INVALID_PARAMS` 失败。已上锁的列表以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `items` | `integer[]` | 是 | 要移除的行。每一项不小于 `0`。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `contentVersion` | `string` | 已包含这次修改，可作为下一次调用的 `expectedVersion`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### playlist.removeSelectedTracks

<!-- api-schema:begin playlist.removeSelectedTracks -->
从播放列表移除选中的行，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### playlist.moveTracks

<!-- api-schema:begin playlist.moveTracks -->
把播放列表中选中的行移动 `delta` 个位置，事先保存撤销点。给了 `items` 时先把选中换成这些行，调用方原有的选中随之丢失。已上锁的列表以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `items` | `integer[]` | 否 | 要移动的行，先用它们替换当前选中。省略或为空时移动当前选中的行。每一项不小于 `0`。 |
| `delta` | `integer` | 是 | 移动的位置数：负数向上，正数向下。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `contentVersion` | `string` | 已包含这次修改，可作为下一次调用的 `expectedVersion`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('playlist.moveTracks', { items: [0, 1, 2], delta: 3 });
await fb2k.invoke('playlist.moveTracks', { items: [5, 6], delta: -2 });
```

### playlist.addPaths

<!-- api-schema:begin playlist.addPaths -->
把文件、文件夹或 URL 追加到播放列表，事先保存撤销点。foobar2000 按它自己的「添加文件」处理这批路径：展开文件夹与播放列表文件、去掉重复项，并按用户设置的新增项顺序排序，所以不保持传入顺序（要保序用 `playlist.addPathsSequential`）。超过 2048 个字符的项与解析不出任何曲目的项计入 `invalidCount`。一首都没加上时以 `NOT_FOUND` 失败。已上锁的列表以 `LOCKED` 失败。foobar2000 解析路径时可能显示进度框，解析完会重新查找这张列表：期间被挪动，曲目照样加进它，结果里的 `playlist` 是它的新索引；期间被删掉，以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `paths` | `string[]` | 是 | 文件、文件夹或 URL；`\|subsong:N` 后缀选子曲目。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `requestedPaths` | `integer` | `paths` 的条目数。 |
| `addedCount` | `integer` | 加入的曲目数；文件夹按其中的每首曲目计。 |
| `invalidCount` | `integer` | 不可用的条目数。 |
| `countBefore` | `integer` | 加入前的曲目数。 |
| `totalCount` | `integer` | 加入后的曲目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## 其他公开 API


### playlist.addHandles

<!-- api-schema:begin playlist.addHandles -->
把曲目追加到播放列表，事先保存撤销点。`handles` 的项与 `playlist.insertTracks` 相同；一项都不可用时以 `NOT_FOUND` 失败。已上锁的列表以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `handles` | `any[]` | 是 | 要追加的曲目：每项是路径（可带 `\|subsong:N` 后缀）或 `{ path, subsong }`。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `requestedCount` | `integer` | `handles` 的条目数。 |
| `addedCount` | `integer` | 追加的曲目数。 |
| `invalidCount` | `integer` | 不可用的条目数。 |
| `countBefore` | `integer` | 追加前的曲目数。 |
| `totalCount` | `integer` | 追加后的曲目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.addHandles', { handles: ['C:\\Music\\song.flac'] });
```


### playlist.addPathsAsync

<!-- api-schema:begin playlist.addPathsAsync -->
开始向播放列表追加路径，不等它完成：调用返回一张回执，完成情况由带同一 `operationId` 的 `playlist:addComplete` 报告。普通文件与 URL 在调用返回前就已加入；播放列表文件（`.pls`、`.m3u`、`.cue` 等）在后台展开。为空或超过 2048 个字符的项计入 `invalidCount`；一项都不剩时以 `INVALID_PARAMS` 失败。已上锁的列表以 `LOCKED` 失败。展开出的曲目加进同一张列表，期间它被挪动也一样；期间它被删掉或上锁，这些曲目就丢弃。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `paths` | `string[]` | 是 | 文件、文件夹、URL 或播放列表文件；`\|subsong:N` 后缀选子曲目。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `operationId` | `string` | 操作的 id；`playlist:addComplete` 带同一个 id。 |
| `playlistGuid` | `string` | 路径要加入的播放列表的 GUID；`playlist:addComplete` 带同一个。 |
| `status` | `"pending"` | 总是 `pending`。 |
| `totalCount` | `integer` | 接受处理的条目数。 |
| `invalidCount` | `integer` | 处理前就拒绝的条目数：为空或超过 2048 个字符。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.addPathsAsync', { paths: ['C:\\Music\\Album'] });
if (res.success === false) throw new Error(res.error);
const { operationId } = res;
```


### playlist.addPathsSequential

<!-- api-schema:begin playlist.addPathsSequential -->
按传入顺序把路径追加到播放列表，事先保存撤销点；展开成多首曲目的路径（文件夹、cue）占住它自己的位置。超过 2048 个字符的项与解析不出任何曲目的项跳过。已上锁的列表以 `LOCKED` 失败。解析路径期间列表被挪动或删掉时，处理同 `playlist.addPaths`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `paths` | `string[]` | 是 | 文件、文件夹或 URL，按要加入的顺序；`\|subsong:N` 后缀选子曲目。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `addedCount` | `integer` | 加入的曲目数；什么都没解析出来时为 `0`，调用仍然成功。 |
| `order` | `integer[]` | 每首加入的曲目所在的行，按加入的顺序。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.addPathsSequential', { paths: ['C:\\Music\\a.flac', 'C:\\Music\\b.flac'] });
```


### playlist.convertToAutoplaylist

<!-- api-schema:begin playlist.convertToAutoplaylist -->
把已有的播放列表变成自动播放列表，曲目换成查询结果。foobar2000 拒绝时（例如它已是自动播放列表）以 `OPERATION_FAILED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 要转换的播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `query` | `string` | 是 | 选出曲目的媒体库查询，使用 foobar2000 的查询语法。不能为空。 |
| `sort` | `string` | 否 | 结果的排序依据，Title Formatting 模式；为空时按媒体库顺序。默认 `""`。 |
| `keepSorted` | `boolean` | 否 | 让播放列表始终按 `sort` 排序：设置后用户不能手动调整曲目顺序。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.convertToAutoplaylist', { playlist: 0, query: '%genre% IS Rock' });
```


### playlist.createAutoplaylist

<!-- api-schema:begin playlist.createAutoplaylist -->
新建一个由 foobar2000 按媒体库查询填充并保持更新的播放列表。foobar2000 拒绝建立自动播放列表时以 `OPERATION_FAILED` 失败，新建的列表随之删除。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 否 | 新播放列表的名称。默认 `"New Autoplaylist"`。 |
| `query` | `string` | 是 | 选出曲目的媒体库查询，使用 foobar2000 的查询语法。不能为空。 |
| `sort` | `string` | 否 | 结果的排序依据，Title Formatting 模式；为空时按媒体库顺序。默认 `""`。 |
| `keepSorted` | `boolean` | 否 | 让播放列表始终按 `sort` 排序：设置后用户不能手动调整曲目顺序。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `integer` | 新播放列表的索引。 |
| `playlist` | `integer` | 新播放列表的索引；与 `index` 相同。 |
| `guid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `name` | `string` | 新播放列表的名称。 |
| `query` | `string` | 查询语句，原样返回。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.createAutoplaylist', { name: 'Rock', query: '%genre% IS Rock' });
if (res.success === false) throw new Error(res.error);
const { index } = res;
```


### playlist.deselectAll

<!-- api-schema:begin playlist.deselectAll -->
清除播放列表中的选中。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.deselectAll', { playlist: 0 });
```


### playlist.focusTrack

<!-- api-schema:begin playlist.focusTrack -->
移动播放列表的焦点；与 `playlist.setFocusedTrack` 是同一个操作，这是它的旧名字。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `index` | `integer` | 否 | 要聚焦的行；省略或为负数时播放列表失去焦点。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.focusTrack', { playlist: 0, index: 3 });
```


### playlist.getAutoplaylistInfo

<!-- api-schema:begin playlist.getAutoplaylistInfo -->
描述播放列表的自动列表状态及原生编辑器可用性，不打开 UI。提供者查询异常以 `OPERATION_FAILED` 失败；能力未知本身不是失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isAutoplaylist` | `boolean` | 播放列表是否为自动列表。`keepSorted`、`source` 与 `lockName` 仅在它是时出现；成功响应始终带编辑器可用性。 |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `editorAvailability` | `"available" \| "unavailable" \| "unknown"` | `available` 表示提供者声明支持 UI；`unknown` 表示不尝试原生调用就无法判定。两者都不保证某次调用会显示窗口。 |
| `editorAvailabilityReason` | `"supported" \| "noEditor" \| "legacyClient" \| "unverifiedLock" \| "notAutoplaylist"` | 可用、不可用或未知的原因。旧 client 或兼容识别的自动列表锁没有无副作用的编辑器探测接口。 |
| `configAvailability` | `"available" \| "unavailable"` | 本组件是否能读取和编辑该提供者的配置。 |
| `configAvailabilityReason` | `"supported" \| "externalProvider" \| "unverifiedLock" \| "notAutoplaylist"` | 支持本组件提供者；外部提供者和未经确认的锁不支持此处的配置协议。 |
| `keepSorted` | `boolean` | manager 管理的自动列表是否保持排序。`source:'dui'` 的锁配置不可读，沿用的 `false` 是占位值。 |
| `source` | `"sdk" \| "dui"` | `sdk` 表示 autoplaylist manager 路径，也包括第三方 client；`dui` 表示既有锁名启发式识别。两者都不表示当前界面类型或编辑器能力。 |
| `lockName` | `string` | 播放列表的锁的名字，`source` 为 `dui` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const info = await fb2k.invoke('playlist.getAutoplaylistInfo', { playlist: 0 });
if (info.success === false) throw new Error(info.error);
if (info.isAutoplaylist) console.log(info.source, info.keepSorted);
```


### playlist.showAutoplaylistUI

<!-- api-schema:begin playlist.showAutoplaylistUI -->
受理打开指定自动列表原生编辑器的请求，`playlist` 与 `playlistGuid` 必须恰好给一个。先回受理，再排队调用原生入口；调用正常返回或失败经 `playlist:autoplaylistUIResult` 仅发给原调用文档。事件可能早于受理 Promise 的处理，须先订阅再调用。请求不切换活动列表、不启动播放；原生调用正常返回不表示窗口已出现，也不表示用户保存、取消或关闭了编辑器。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 宿主首次处理请求时的列表索引。不传 `playlistGuid` 时必填；需要跨首次排队稳定身份时应使用 GUID。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `accepted` | `boolean` | 成功时始终为 `true`：请求已受理，原生调用未必已执行。 |
| `operationId` | `string` | 用于关联单次原生调用结果事件的不透明标识。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

宿主在主线程首次处理请求时解析目标并固定 GUID。数字索引指向首次解析时位于该位置的列表；需要在首次排队期间保持身份时应传 `playlistGuid`。后续重排不会把请求转向其他列表。原生调用前会重新查询当前提供者，所以固定的是列表身份，不是某个提供者实例。自动列表的曲目锁本身不阻止打开编辑器。

**失败分类**

受理前的业务失败通过响应返回，具体原因放在 `details.reason`。受理后只通过 [`playlist:autoplaylistUIResult`](events.md#playlist-autoplaylistuiresult) 报告失败，事件直接带 `reason`，不再回复原请求。

| 情况 | `code` | `reason` |
| --- | --- | --- |
| 未给目标、同时给两键、格式错误或负索引 | `INVALID_PARAMS` | — |
| 合法非负索引越界 | `INVALID_INDEX` | — |
| 目标 GUID 已不存在 | `NOT_FOUND` | `targetGone` |
| 普通列表 | `NOT_SUPPORTED` | `notAutoplaylist` |
| 提供者明确表示没有编辑器 | `NOT_SUPPORTED` | `noEditor` |
| 同一 GUID 已在排队或原生调用中 | `OPERATION_FAILED` | `requestInProgress` |
| 查询提供者失败 | `OPERATION_FAILED` | `queryFailed` |
| 无法调度原生调用 | `OPERATION_FAILED` | `dispatchFailed` |
| 原生调用抛异常，或锁入口返回 `false` | `OPERATION_FAILED` | `providerFailed` |

锁入口返回 `false` 不能证明编辑器不可用。提供者明确无编辑器、查询异常或原生调用异常时，都不会换一条提供者路径重试。

**生命周期与重复请求**

每个 GUID 的进行中记录在整个进程内共享，保留至原生调用返回或抛异常。调用已进入提供者后，页面导航也不会提前释放记录。非模态编辑器可能在调用返回后继续存在，其窗口归属和复用方式由提供者决定。API 不能指定 owner、保证激活已有编辑器，也不能在页面释放时关闭该编辑器。

页面导航、销毁或宿主重建会取消旧文档尚未执行的调用，并丢弃给旧文档的结果。结果不会发给替代文档，也不会回落到主窗口。原生调用结束时，若原文档仍有效但目标已经删除，结果为 `NOT_FOUND` / `targetGone`，即使提供者同时抛异常也一样；提供者已执行的动作不会撤销。

[SDK 完整示例](../sdk/playlist.md#onautoplaylistuiresult-callback) 展示了先订阅再调用、有数量上限的早到结果缓存、按 `operationId` 关联以及卸载清理。受理和调用成功都不提供 saved、cancelled、closed 或 changed 通知。应根据实际的 `playlist:itemsAdded`、`playlist:itemsRemoved`、`playlist:itemsReordered`、`playlist:itemsReplaced` 和 `playlist:lockChanged` 事件刷新状态。调用结果可以触发一次重读，但无法覆盖非模态窗口后续的编辑。

### playlist.getAutoplaylistQuery

<!-- api-schema:begin playlist.getAutoplaylistQuery -->
描述播放列表的自动列表状态，不包含编辑器能力字段。foobar2000 不公开自动播放列表的查询语句，所以 `query` 总是 `null`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isAutoplaylist` | `boolean` | 播放列表是否为自动播放列表；`keepSorted`、`source`、`note` 与 `lockName` 只在它是时出现。 |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `query` | `string \| null` | 总是 `null`：foobar2000 不公开查询语句。 |
| `keepSorted` | `boolean` | manager 管理的自动列表是否保持排序。`source:'dui'` 的 `false` 是不可读锁配置的占位值。 |
| `source` | `"sdk" \| "dui"` | `sdk` 表示 autoplaylist manager 路径，也包括第三方 client；`dui` 表示既有锁名启发式识别。两者都不表示当前界面类型或编辑器能力。 |
| `note` | `string` | 说明查询语句不可得。 |
| `lockName` | `string` | 播放列表的锁的名字，`source` 为 `dui` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const q = await fb2k.invoke('playlist.getAutoplaylistQuery', { playlist: 0 });
// 即便 q.isAutoplaylist 为 true，q.query 仍为 null
```


### playlist.getAvailableColumns

<!-- api-schema:begin playlist.getAvailableColumns -->
列出 foobar2000 与已装组件为默认界面播放列表视图定义的列。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `columns` | `PlaylistColumnDefinition[]` | 全部列，按组件依次排列。 |
| `columns[].id` | `string` | 列的 GUID，写成大写、不带花括号的 `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`，与播放列表 GUID 的写法不同。 |
| `columns[].name` | `string` | 显示名称。 |
| `columns[].pattern` | `string` | 单元格文字的 Title Formatting 模式。 |
| `columns[].alignment` | `"left" \| "right" \| "center"` | 单元格文字的对齐方式。 |
| `columns[].numeric` | `boolean` | 该列是否为数字。 |
| `columns[].sortPattern` | `string` | 按该列排序时使用的 Title Formatting 模式；排序使用 `pattern` 时不出现。 |
| `count` | `integer` | `columns` 的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getAvailableColumns');
if (res.success === false) throw new Error(res.error);
const { columns } = res;
columns.forEach((c) => console.log(c.name, c.pattern));
```


### playlist.getFocusTrack

<!-- api-schema:begin playlist.getFocusTrack -->
报告播放列表的焦点行；与 `playlist.getFocusedTrack` 相同，这是它的旧名字，区别是列表不存在时失败而不是回答 `index` 为 `-1`：超出最后一个播放列表的索引以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败，两者都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。成功时恒带 `playlist` 与 `playlistGuid`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `index` | `integer` | 焦点行；播放列表没有焦点时为 `-1`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getFocusTrack', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { index } = res;
```


### playlist.getFocusedTrack

<!-- api-schema:begin playlist.getFocusedTrack -->
报告播放列表的焦点行。超出最后一个播放列表的索引、没有列表持有的 `playlistGuid`，或两者都省略且没有活动播放列表时，报告 `index` 为 `-1` 且不带 `playlist` 与 `playlistGuid`，而不是失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识，用于替代序号。目标不存在时返回空结果而非报错。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引；没有这个播放列表时不出现。 |
| `playlistGuid` | `string` | 播放列表的 GUID，写法与 `playlistGuid` 接受的一致；没有这个播放列表时不出现。 |
| `index` | `integer` | 焦点行；播放列表没有焦点或不存在时为 `-1`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getFocusedTrack', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { index } = res;
```


### playlist.getLockInfo

<!-- api-schema:begin playlist.getLockInfo -->
报告一个播放列表是否带锁，例如自动播放列表的锁。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `isLocked` | `boolean` | 它是否带锁。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getLockInfo', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { isLocked } = res;
```


### playlist.getSelectedTracks

<!-- api-schema:begin playlist.getSelectedTracks -->
播放列表里选中的行，按列表顺序，不带播放统计。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败；两者都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。这些失败，以及两个键都给或 GUID 格式不对，都带空的 `tracks`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表索引；省略时为活动播放列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 读取的播放列表。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `tracks` | `PlaylistTrack[]` | 选中的行，按列表顺序。 |
| `tracks[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `tracks[].index` | `integer` | 在播放列表中的行号，从 `0` 起。 |
| `tracks[].composer` | `string` | 全部 COMPOSER 值用 `", "` 连接；没有标签时为空。 |
| `tracks[].comment` | `string` | 第一个 COMMENT 值；没有标签时为空。 |
| `tracks[].playCount` | `integer` | foo_playcount 的 `%play_count%`；它给不出数字时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].firstPlayed` | `string` | foo_playcount 格式化的 `%first_played%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].lastPlayed` | `string` | foo_playcount 格式化的 `%last_played%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].added` | `string` | foo_playcount 格式化的 `%added%`；为空时不出现，`playlist.getSelectedTracks` 的行与带 `fields` 的请求的行里也不出现。 |
| `tracks[].formats` | `Record<string, string>` | `playlist.getTracks` 的 `formats` 列，按给定的列名；只在传了 `formats` 时出现。 |
| `count` | `integer` | 返回的行数。 |
| `contentVersion` | `string` | 列表仍是此版本时，`tracks` 中的行号才有效。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playlist.getSelectedTracks');
```


### playlist.getSelection

<!-- api-schema:begin playlist.getSelection -->
列出播放列表中被选中的行。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `integer[]` | 选中的行，按播放列表顺序。 |
| `count` | `integer` | `items` 的条目数。 |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `contentVersion` | `string` | 列表仍是此版本时，`items` 中的行号才有效。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.getSelection', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { items, count } = res;
```


### playlist.insertTracks

<!-- api-schema:begin playlist.insertTracks -->
在播放列表的某个位置插入曲目，事先保存撤销点。`handles` 的每一项是路径（可带 `|subsong:N` 后缀）或 `{ path, subsong }`；两者都不是或指不到任何东西的项计入 `invalidCount`。一项都不可用时以 `NOT_FOUND` 失败。已上锁的列表以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `position` | `integer` | 否 | 曲目插在这一行之前；超出最后一行时追加到末尾。不小于 `0`。默认 `0`。 |
| `handles` | `any[]` | 是 | 要插入的曲目：每项是路径（可带 `\|subsong:N` 后缀）或 `{ path, subsong }`。不能为空。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `insertIndex` | `integer` | 请求的 `position`，未按列表末尾截短。 |
| `requestedCount` | `integer` | `handles` 的条目数。 |
| `addedCount` | `integer` | 插入的曲目数。 |
| `invalidCount` | `integer` | 不可用的条目数。 |
| `countBefore` | `integer` | 插入前的曲目数。 |
| `totalCount` | `integer` | 插入后的曲目数。 |
| `contentVersion` | `string` | 已包含这次修改，可作为下一次调用的 `expectedVersion`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const result = await fb2k.invoke('playlist.insertTracks', {
    playlist: 0,
    position: 5,
    handles: ['C:\\Music\\song.flac'],
});
```


### playlist.isAutoplaylist

<!-- api-schema:begin playlist.isAutoplaylist -->
报告播放列表是否有 autoplaylist manager client（包括第三方 client），或符合名字含 `Auto` 的兼容锁识别规则。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `isAutoplaylist` | `boolean` | 它是否为自动播放列表。 |
| `lockName` | `string` | 播放列表没有 autoplaylist manager client 且带有具名锁时返回锁名，不论这把锁是否使它符合自动列表识别规则。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.isAutoplaylist', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { isAutoplaylist } = res;
```


### playlist.isLocked

<!-- api-schema:begin playlist.isLocked -->
报告一个播放列表是否带锁。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败，没有列表持有的 `playlistGuid` 以 `NOT_FOUND` 失败。找不到列表的各种失败，以及两个键都给或 GUID 格式不对，失败里都带 `isLocked` 为 `false`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `isLocked` | `boolean` | 播放列表是否带锁。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
const res = await fb2k.invoke('playlist.isLocked', { playlist: 0 });
if (res.success === false) throw new Error(res.error);
const { isLocked } = res;
```


### playlist.redo

<!-- api-schema:begin playlist.redo -->
重新应用上一次 `playlist.undo` 撤销的改动。没有可重做的改动时以 `NOT_FOUND` 失败，列表上的锁拒绝改动时以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.redo', { playlist: 0 });
```


### playlist.removeAutoplaylist

<!-- api-schema:begin playlist.removeAutoplaylist -->
解除 autoplaylist manager client（包括第三方 client），保留当前曲目。没有 manager client、仅按兼容规则识别的自动列表锁不能这样解除：调用成功、`source` 为 `dui`，但什么也不改；删除列表用 `playlist.remove`。不是自动播放列表时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `source` | `"sdk" \| "dui"` | 解除 manager client（包括第三方 client）时为 `sdk`；仅找到兼容识别的自动列表锁、没有改动时为 `dui`。 |
| `note` | `string` | 说明，`source` 为 `dui` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.removeAutoplaylist', { playlist: 0 });
```


### playlist.reorder

<!-- api-schema:begin playlist.reorder -->
重排播放列表的曲目，事先保存撤销点：`newOrder[i]` 是移到第 `i` 行的那首曲目的当前行号。已上锁的列表以 `LOCKED` 失败，长度与曲目数不同或同一行出现两次时以 `INVALID_PARAMS` 失败，某一项超出最后一行时以 `INVALID_INDEX` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `newOrder` | `integer[]` | 是 | 每个新行上放的那首曲目的当前行号；每一行恰好出现一次。每一项不小于 `0`。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `itemCount` | `integer` | 重排的曲目数。 |
| `contentVersion` | `string` | 已包含这次修改，可作为下一次调用的 `expectedVersion`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
// newOrder 必须是该播放列表当前全部曲目索引的一个完整排列
await fb2k.invoke('playlist.reorder', { playlist: 0, newOrder: [2, 0, 1] });
```


### playlist.reorderPlaylists

<!-- api-schema:begin playlist.reorderPlaylists -->
重排播放列表的顺序，为每个新位置给出放到那里的播放列表的当前索引（`newOrder`）或它的 GUID（`newOrderGuids`）；两者恰好给一个，否则以 `INVALID_PARAMS` 失败。索引按调用到达时的清单解读，所以在别的改动之前从 `playlist.getAll` 取来的索引，只要个数还对得上，就会不报错地排错列表；GUID 始终指向读取时的那些列表。长度与播放列表个数不同时以 `INVALID_PARAMS` 失败，同一个列表出现两次或 GUID 格式不对也是；某一项索引超出最后一个播放列表时以 `INVALID_INDEX` 失败，没有列表持有某个 GUID 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `newOrder` | `integer[]` | 否 | 每个新位置上放的那个播放列表的当前索引；每个播放列表恰好出现一次。每一项不小于 `0`。 |
| `newOrderGuids` | `string[]` | 否 | 每个新位置上放的那个播放列表的 `guid`（取 `playlist.getAll` 报告的值）；每个播放列表恰好出现一次。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 播放列表个数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
// newOrder 必须是现有播放列表索引的一个完整排列
await fb2k.invoke('playlist.reorderPlaylists', { newOrder: [2, 0, 1] });
```


### playlist.replaceAllAndPlay

<!-- api-schema:begin playlist.replaceAllAndPlay -->
替换播放列表的全部内容并播放：停止播放，清空列表（保存撤销点），按 `playlist.addPaths` 的方式加入路径，把列表设为活动列表，再播放或聚焦 `playIndex`。一首都没加上时以 `NOT_FOUND` 失败，列表保持为空。已上锁的列表在任何改动之前以 `LOCKED` 失败。路径在清空之后解析，处理同 `playlist.addPaths`：期间被挪动的列表照样被填充和播放；期间被删掉时以 `OPERATION_FAILED` 失败；期间被上锁时以 `LOCKED` 失败，列表保持为空。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `paths` | `string[]` | 是 | 新的内容：文件、文件夹或 URL。不能为空。 |
| `playIndex` | `integer` | 否 | 要播放的行，`autoPlay` 为 `false` 时是要聚焦的行；超出最后一行时取第一行。各行不保持 `paths` 的顺序，所以它指的是位置，而不是传入的某个路径。不小于 `0`。默认 `0`。 |
| `stopFirst` | `boolean` | 否 | 正在播放时先停止播放。默认 `true`。 |
| `autoPlay` | `boolean` | 否 | 播放 `playIndex`；为 `false` 时只聚焦它。默认 `true`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlist` | `integer` | 播放列表的索引。 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `clearedCount` | `integer` | 移除的曲目数。 |
| `addedCount` | `integer` | 加入的曲目数。 |
| `totalCount` | `integer` | 之后的曲目数。 |
| `playIndex` | `integer` | 播放或聚焦的行。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.replaceAllAndPlay', { paths: ['C:\\Music\\song.flac'] });
```


### playlist.reverse

<!-- api-schema:begin playlist.reverse -->
把播放列表的曲目顺序倒过来，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.reverse', { playlist: 0 });
```


### playlist.selectAll

<!-- api-schema:begin playlist.selectAll -->
选中播放列表中的全部行。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.selectAll', { playlist: 0 });
```


### playlist.setFocusedTrack

<!-- api-schema:begin playlist.setFocusedTrack -->
把播放列表的焦点移到一行，或去掉焦点。超出最后一行的行号以 `INVALID_INDEX` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `index` | `integer` | 否 | 要聚焦的行；省略或为负数时播放列表失去焦点。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.setFocusedTrack', { playlist: 0, index: 3 });
```


### playlist.setSelection

<!-- api-schema:begin playlist.setSelection -->
选中播放列表中的行。超出最后一行的行号忽略，负数行号以 `INVALID_PARAMS` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `indices` | `integer[]` | 是 | 要选中的行；空列表表示一行都不选。每一项不小于 `0`。 |
| `clearOthers` | `boolean` | 否 | 取消选中其余所有行；为 `false` 时把 `indices` 加到当前选中里。默认 `true`。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `contentVersion` | `string` | 选中行不会改变此值。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.setSelection', { playlist: 0, indices: [0, 1, 2] });
```


### playlist.shuffle

<!-- api-schema:begin playlist.shuffle -->
把播放列表的曲目打乱成随机顺序，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.shuffle', { playlist: 0 });
```


### playlist.sort

<!-- api-schema:begin playlist.sort -->
按 Title Formatting 模式给播放列表排序，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `pattern` | `string` | 否 | 排序依据，Title Formatting 模式。默认 `"%title%"`。 |
| `descending` | `boolean` | 否 | 排序后把整个播放列表倒过来。倒序覆盖全部曲目，即使设了 `selectedOnly`，未选中的也一样。默认 `false`。 |
| `selectedOnly` | `boolean` | 否 | 只在选中的曲目之间排序，其余曲目位置不变。默认 `false`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.sort', { playlist: 0, pattern: '%artist% - %title%' });
```


### playlist.undo

<!-- api-schema:begin playlist.undo -->
把播放列表恢复到上一个撤销点。没有撤销点时以 `NOT_FOUND` 失败，列表上的锁拒绝改动时以 `LOCKED` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表的索引；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```js
await fb2k.invoke('playlist.undo', { playlist: 0 });
```

## 关联的播放列表事件

以下播放列表生命周期事件会被广播。JIT 队列 shadow playlist 的条目级事件被有意忽略。

| 事件 | 触发时机 | Payload keys |
| --- | --- | --- |
| `playlist:itemsAdded` | 条目插入播放列表后。 | `{ playlist, start, count }` |
| `playlist:itemsRemoved` | 条目从播放列表移除后。 | `{ playlist, oldCount, newCount }` |
| `playlist:itemsReordered` | 单个播放列表中的条目重排后。 | `{ playlist, count }` |
| `playlist:selectionChanged` | 播放列表选择变化后。 | `{ playlist }` |
| `playlist:focusChanged` | 播放列表条目焦点变化后。 | `{ playlist, from, to }` |
| `playlist:itemsReplaced` | 播放列表条目替换后。 | `{ playlist, count }` |
| `playlist:created` | 新建播放列表后。 | `{ index, name }` |
| `playlist:removed` | 删除播放列表后。 | `{ oldCount, newCount }` |
| `playlist:reordered` | 播放列表集合重排后。 | `{ count }` |
| `playlist:activated` | 活动播放列表变化后。 | `{ oldIndex, newIndex }` |
| `playlist:renamed` | 播放列表重命名后。 | `{ index, name }` |
| `playlist:lockChanged` | 播放列表锁状态变化后。 | `{ playlist, locked }` |
| `playlist:defaultFormatChanged` | 默认播放列表格式变化后。 | `{}` |
| `playlist:addComplete` | 异步路径添加操作完成后。 | `{ operationId, success, addedCount, totalCount }` |

当 foobar2000 回调没有具体的前一个或后一个索引时，`from`、`to`、`oldIndex` 与 `newIndex` 可以为 `-1`。
## playlist.createManagedAutoplaylist

<!-- api-schema:begin playlist.createManagedAutoplaylist -->
创建由本组件托管查询与排序配置的自动播放列表（依赖本组件持续安装）。配置非法时在建表前即拒绝。成功返回配置快照，不代表曲目重建或写盘已完成。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `name` | `string` | 否 | 新列表名称，最多 65536 个 UTF-8 字节，不含 NUL。默认 `"New Autoplaylist"`。 |
| `query` | `string` | 是 | foobar2000 媒体库查询，最多 65536 个 UTF-8 字节；拒绝空白文本和 NUL。 |
| `sort` | `string` | 否 | 独立的 Title Formatting 排序。空值使用查询内排序或宿主默认顺序。非空值不能与查询内显式 SORT BY 并用。最多 65536 个 UTF-8 字节，不含 NUL。默认 `""`。 |
| `keepSorted` | `boolean` | 否 | 持续保持排序，并禁止手动调整曲目顺序。默认 `false`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `query` | `string` | 提供者持有的查询原文。 |
| `sort` | `string` | 独立排序原文；查询内的排序仍保留在 query 中。 |
| `keepSorted` | `boolean` | 从 autoplaylist manager 读取的当前保持排序标志。 |
| `revision` | `string` | 当前提供者实例内的不透明配置版本，不是曲目内容版本。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## playlist.getAutoplaylistConfig

<!-- api-schema:begin playlist.getAutoplaylistConfig -->
读取由本组件托管的自动播放列表配置。必须且仅能指定一个目标。普通列表或其他提供者管理的列表以 `NOT_SUPPORTED` 失败（附带 `details.reason`）。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表序号；未传 playlistGuid 时必填，不默认使用活动列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `query` | `string` | 提供者持有的查询原文。 |
| `sort` | `string` | 独立排序原文；查询内的排序仍保留在 query 中。 |
| `keepSorted` | `boolean` | 从 autoplaylist manager 读取的当前保持排序标志。 |
| `revision` | `string` | 当前提供者实例内的不透明配置版本，不是曲目内容版本。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

## playlist.setAutoplaylistConfig

<!-- api-schema:begin playlist.setAutoplaylistConfig -->
基于预期版本号修改托管自动播放列表的配置。省略字段保持原值，须提供至少一个修改项。新值相同时不推进版本号亦不重建曲目。版本过期时以 `OPERATION_FAILED` 失败（`details.reason: 'revisionConflict'`）。配置先编译校验再写入。宿主失败可能留下部分修改：`details.state`（`unchanged`、`changed` 或 `unknown`）仅针对配置，能确认当前状态时才附带 `details.current`。调用成功或失败均不代表曲目重建或写盘已完成。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 播放列表序号；未传 playlistGuid 时必填，不默认使用活动列表。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `expectedRevision` | `string` | 是 | 最近配置快照中的不透明版本，重启或替换提供者后失效。 |
| `query` | `string` | 否 | 新查询，最多 65536 个 UTF-8 字节；拒绝空白文本和 NUL，省略时保留查询。 |
| `sort` | `string` | 否 | 新独立排序；空值清除，省略时保留。非空时不能与查询内 SORT BY 并用。最多 65536 个 UTF-8 字节，不含 NUL。 |
| `keepSorted` | `boolean` | 否 | 新的保持排序设置，省略时保留 manager 当前标志。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `query` | `string` | 提供者持有的查询原文。 |
| `sort` | `string` | 独立排序原文；查询内的排序仍保留在 query 中。 |
| `keepSorted` | `boolean` | 从 autoplaylist manager 读取的当前保持排序标志。 |
| `revision` | `string` | 当前提供者实例内的不透明配置版本，不是曲目内容版本。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->
