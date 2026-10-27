# Queue 播放队列 API

管理待播队列读取、单曲与批量入队、顺序调整及插播控制。

## Queue API - 播放队列

### queue.addPaths

<!-- api-schema:begin queue.addPaths -->
按文件路径或 URL 将曲目加入待播队列；宿主会先将曲目追加至播放列表（默认专用 `[WebView Queue]` 列表，不存在则自动创建）后再入队；目标列表已锁定时以 `LOCKED` 失败且不执行追加与入队；路径解析后重新定位目标列表，若期间被删除则以 `OPERATION_FAILED` 失败；未通过媒体根目录安全检查时整次调用以 `PERMISSION_DENIED` 失败；解析失败的无效路径自动丢弃，无任一有效曲目时以 `NOT_FOUND` 失败并在 `invalidCount` 中注明

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待追加的文件路径或 URL 列表，单项可附加 `\|subsong:N` 后缀；超过 2048 字符的条目将被丢弃并计入 `invalidCount`不能为空。 |
| `useQueuePlaylist` | `boolean` | 否 | 是否追加至专用的 `[WebView Queue]` 播放列表（不存在则自动创建）；设为 `false` 时使用 `playlist` 或当前活动列表默认 `true`。 |
| `playlist` | `integer` | 否 | 目标播放列表索引；仅在 `useQueuePlaylist: false` 时读取；越界以 `INVALID_INDEX` 失败，缺省且无活动列表时以 `NO_ACTIVE_ITEM` 失败不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 目标播放列表的唯一标识 `guid`；仅在 `useQueuePlaylist: false` 时读取 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `addedCount` | `integer` | 成功追加并加入待播队列的曲目总数 |
| `invalidCount` | `integer` | 丢弃的无效路径数量（包含空路径、超长路径、无对应曲目的 subsong 或全量解析失败项） |
| `playlist` | `integer` | 曲目最终追加到的目标播放列表索引 |
| `playlistGuid` | `string` | 播放列表的唯一标识，格式为 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。后续可作为 `playlistGuid` 稳定引用该列表。 |
| `queueCount` | `integer` | 本次操作完成后待播队列的总长度 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

一步添加 URL/本地路径到播放队列。自动处理添加到播放列表和入队操作。

```javascript
await fb2k.invoke('queue.addPaths', {
    paths: ['C:/Music/song.mp3', 'http://stream.example.com/audio.mp3']
});
```

### queue.add

<!-- api-schema:begin queue.add -->
按播放列表行号将一首或多首曲目追加至待播队列；超出列表范围的索引自动忽略，无任一有效索引时以 `INVALID_INDEX` 失败，负数索引以 `INVALID_PARAMS` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 索引所引用的来源播放列表；省略时默认操作活动播放列表；越界以 `INVALID_INDEX` 失败，缺省且无活动列表时以 `NO_ACTIVE_ITEM` 失败不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的持久唯一标识（带花括号 GUID，大小写均可）。用于替代易变序号。与序号互斥，同时传入或格式非法以 `INVALID_PARAMS` 失败。列表不存在时以 `NOT_FOUND` 失败，不回退至其他列表。格式非法或列表不存在时，错误详情均带回 `details.playlistGuid`。 |
| `tracks` | `integer[]` | 否 | 待入队的播放列表行号数组，按传入顺序排列；优先级高于 `track`每一项不小于 `0`。 |
| `track` | `integer` | 否 | 待入队的单个播放列表行号；仅在未提供 `tracks` 时生效不小于 `0`。 |
| `expectedPlaylistVersion` | `string` | 否 | 播放列表预期内容版本号；仅在行所属播放列表的版本与此匹配时执行变更，版本冲突时不修改并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本），省略时不校验 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `addedCount` | `integer` | 成功加入待播队列的曲目行数 |
| `queueCount` | `integer` | 本次操作完成后待播队列的总长度 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

将播放列表中的曲目添加到队列。支持单个曲目或批量添加。

`tracks` 与 `track` 二选一——`tracks` 为数组时优先，`track` 被忽略。越界下标会被跳过，因此一条都没匹配上时以 `INVALID_INDEX` 失败并带 `addedCount: 0`；空 `tracks` 数组同理。`playlist` 无效时以 `INVALID_INDEX` 失败（`Invalid playlist index`），两个字段都没传是 `INVALID_PARAMS`。`tracks` 的元素必须是整数——非数字元素由参数校验拒绝，不会被当作可跳过的条目。

```javascript
// 添加单个曲目
await fb2k.invoke('queue.add', { track: 5 });

// 批量添加
await fb2k.invoke('queue.add', { tracks: [0, 1, 2], playlist: 0 });
```

### queue.get

<!-- api-schema:begin queue.get -->
按播放顺序读取完整待播队列；不分页一次性返回全量条目

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `QueueItem[]` | 队列条目列表，按播放先后顺序排列 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].queueIndex` | `integer` | 在待播队列中的顺序索引（从 `0` 开始） |
| `items[].playlist` | `integer \| null` | 条目入队时的来源播放列表索引；通过路径入队或原列表位置已失效时为 `null` |
| `items[].playlistGuid` | `string \| null` | 来源播放列表的持久唯一标识；当且仅当 `playlist` 为 `null` 时返回 `null` |
| `items[].playlistItem` | `integer \| null` | 条目在来源播放列表中的行号索引；当且仅当 `playlist` 为 `null` 时返回 `null` |
| `count` | `integer` | 待播队列当前包含的条目总数 |
| `contentVersion` | `string` | 队列当前内容版本号；后续位置操作可作为 `expectedVersion` 传入以避免并发冲突 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

获取当前播放队列内容。

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

`playlist`、`playlistGuid` 与 `playlistItem` 三个键始终存在。记下的位置上仍是这首曲目时照实报出；否则三者同为 `null`：经 `queue.insertNext` 的 `paths` 入队的条目，以及行或列表被删、被挪动之后，记下的位置上已不是这首曲目的条目，都属于这种情形。所以报出的位置不会指向别的曲目。判断只需一句 `item.playlist == null`。

### queue.remove

<!-- api-schema:begin queue.remove -->
按队列索引 `index` 或索引列表 `indices` 从待播队列中移除条目；队列为空时一律以 `NOT_FOUND` 失败；单项 `index` 越界以 `INVALID_INDEX` 失败；批量移除时跳过重复或越界索引，无任一有效索引时以 `INVALID_INDEX` 失败（错误详情附带 `removedCount: 0` 与 `queueCount`）；未提供索引或传入负数时以 `INVALID_PARAMS` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 待移除的单条队列位置索引；优先级高于 `indices`不小于 `0`。 |
| `indices` | `integer[]` | 否 | 待批量移除的队列位置索引数组；仅在未提供 `index` 时读取每一项不小于 `0`。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `removedIndex` | `integer` | 成功移除的队列位置索引；仅在传入 `index` 时返回 |
| `removedCount` | `integer` | 成功移除的队列条目总数；仅在传入 `indices` 时返回 |
| `queueCount` | `integer` | 本次操作完成后待播队列的总长度 |
| `contentVersion` | `string` | 变更后的队列最新内容版本号；可作为后续调用的 `expectedVersion` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

从队列中移除指定项。支持单个索引或批量移除。

```javascript
// 移除第一项
await fb2k.invoke('queue.remove', { index: 0 });

// 批量移除
await fb2k.invoke('queue.remove', { indices: [0, 2, 4] });
```

### queue.clear

<!-- api-schema:begin queue.clear -->
清空待播队列中的所有条目

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clearedCount` | `integer` | 清空前队列中原有的条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

清空整个播放队列。

### queue.flush

<!-- api-schema:begin queue.flush -->
清空待播队列中的所有条目（旧名别名，功能同 `queue.clear`）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clearedCount` | `integer` | 清空前队列中原有的条目总数 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`queue.clear` 的别名。清空整个播放队列。

### queue.getCount

<!-- api-schema:begin queue.getCount -->
获取待播队列当前容纳的条目总数

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 待播队列当前包含的条目总数 |
| `hasItems` | `boolean` | 待播队列是否非空（等同于 `count > 0`） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`hasItems` 就是 `count > 0`，仅为方便判断而提供。

```javascript
const result = await fb2k.invoke('queue.getCount');
if (result.success === false) throw new Error(result.error);
console.log(`队列中有 ${result.count} 项`);
```

### queue.moveToTop

<!-- api-schema:begin queue.moveToTop -->
将指定队列位置的条目移动至队首以便下一首播放；仅调整队列排序，保持其他条目的相对顺序；传入 `index: 0`、越界索引或队列为空时均以 `INVALID_INDEX` 失败；队列重建失败时以 `OPERATION_FAILED` 失败（附带 `queueCount`）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 是 | 待提升至队首的队列条目位置索引；`0` 已在队首，传入将直接拒绝不小于 `0`。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `movedIndex` | `integer` | 被移动条目在调整前的原始位置索引 |
| `queueCount` | `integer` | 本次操作完成后待播队列的总长度 |
| `contentVersion` | `string` | 变更后的队列最新内容版本号；可作为后续调用的 `expectedVersion` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

将队列中的指定项移动到队首（下一首播放）。内部通过清空队列并重建实现。

```javascript
// 将队列第 3 项移到队首
await fb2k.invoke('queue.moveToTop', { index: 3 });
```

### queue.setContents

<!-- api-schema:begin queue.setContents -->
使用指定的有序引用列表全量替换当前待播队列；任一引用项校验失败整次调用立即终止且不写入变更；传入空列表等同清空队列；播放列表索引以调用到达时的状态为准，使用 `playlistGuid` 可稳定引用目标列表；单项同时传入 `playlist` 与 `playlistGuid` 或 GUID 格式非法时以 `INVALID_PARAMS` 失败，GUID 未匹配到列表时以 `NOT_FOUND` 失败

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `QueueContentRef[]` | 是 | 新的队列条目有序列表；最多支持 `max(256, 当前队列长度)` 项，超出上限以 `INVALID_PARAMS` 失败 |
| `items[].queueIndex` | `integer` | 否 | 现有待播队列中待保留条目的位置索引不小于 `0`。 |
| `items[].playlist` | `integer` | 否 | 待引入曲目所在的来源播放列表索引；须与 `item` 同时提供不小于 `0`。 |
| `items[].playlistGuid` | `string` | 否 | 待引入曲目所在播放列表的 `guid`；须与 `item` 同时提供 |
| `items[].item` | `integer` | 否 | 待引入曲目在目标播放列表中的行号索引；须与 `playlist` 或 `playlistGuid` 同时提供不小于 `0`。 |
| `items[].expectedPlaylistVersion` | `string` | 否 | 目标播放列表预期内容版本号；仅在引入播放列表行时有效，与 `queueIndex` 一同传入时以 `INVALID_PARAMS` 失败 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `queueCount` | `integer` | 本次操作完成后待播队列的总长度 |
| `contentVersion` | `string` | 变更后的队列最新内容版本号；可作为后续调用的 `expectedVersion` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

用一份有序引用列表替换整个队列。

任一条目形态无法识别时，整次调用在写入前就以 `INVALID_PARAMS` 整体失败——与 `queue.add` 逐项跳过越界下标不同，这里任何一条无效引用都会拒绝整次调用并保持队列不变。`queueCount` 无论成功还是失败都会带上。传入空数组会清空队列，等价于 `queue.clear`。`items.length` 上限为 `max(256, 当前队列长度)`，超限会失败且不改变队列。

::: tip 拖拽重排的调用方式
应在用户**松手时**调用一次 `setContents`，不要在拖拽过程中逐帧调用——每次调用都会清空并重建整个队列，拖拽期间逐帧调用是无意义的额外开销，还会造成明显卡顿。
:::

```javascript
// 重排：把队列第 2 项移到队首，其余保持原序
await fb2k.invoke('queue.setContents', {
    items: [{ queueIndex: 2 }, { queueIndex: 0 }, { queueIndex: 1 }],
});
// 清空队列
await fb2k.invoke('queue.setContents', { items: [] });
```

### queue.insertNext

<!-- api-schema:begin queue.insertNext -->
将指定曲目插入待播队列前端以便下一首播放；已存在于队列中的曲目直接前移而不重复入队；`items` 列表排在 `paths` 列表之前，各自保持输入顺序；未提供 `paths` 与 `items` 时以 `INVALID_PARAMS` 失败；在解析路径前预检所有 `items` 引用，任一项非法立即失败且不写入变更；路径未通过媒体根目录安全检查以 `PERMISSION_DENIED` 失败；无效路径自动丢弃并计入 `invalidCount`，无任一有效曲目插入时以 `NOT_FOUND` 失败；路径解析期间播放列表发生变动或队列重建失败以 `OPERATION_FAILED` 失败；在路径解析完成后写入队列前校验 `expectedVersion`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 否 | 待插入的文件路径或 URL 列表，可附加 `\|subsong:N` 后缀；由此入队的条目不携带播放列表位置信息 |
| `items` | `QueueListRef[]` | 否 | 待插入的播放列表行引用列表；由此入队的条目附带列表位置且播放游标会跟随；最多支持 `max(256, 当前队列长度)` 项 |
| `items[].playlist` | `integer` | 否 | 目标播放列表的索引序号；与 `playlistGuid` 二选一传入不小于 `0`。 |
| `items[].playlistGuid` | `string` | 否 | 目标播放列表的唯一标识 `guid`，替代易变序号 |
| `items[].item` | `integer` | 是 | 目标播放列表中的行号索引不小于 `0`。 |
| `items[].expectedPlaylistVersion` | `string` | 否 | 播放列表预期内容版本号；仅在行所属播放列表的版本与此匹配时执行变更，版本冲突时不修改并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本），省略时不校验 |
| `position` | `integer` | 否 | 插入的目标队列位置（按移出既有移动条目后的队列计算）；`0` 表示队首不小于 `0`。默认 `0`。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `insertedCount` | `integer` | 全新加入待播队列的曲目总数 |
| `movedCount` | `integer` | 既已存在于队列中并前移位置的条目总数 |
| `queueCount` | `integer` | 本次操作完成后待播队列的总长度 |
| `invalidCount` | `integer` | 输入路径数减去解析得出的曲目数（最小截断至 0）；文件夹展开等多曲目解析不反映为精确失败数 |
| `contentVersion` | `string` | 变更后的队列最新内容版本号；可作为后续调用的 `expectedVersion` |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

插入曲目使其成为下一首播放，插入到当前队列所有曲目之前。条目可以按文件路径给、按播放列表位置给，或两者都给。

¹ `paths` 与 `items` 至少一个非空。

已在队列中的曲目会被移动到 `position`，而不是重复入队；每首只移动一个条目，选择规则见下文。同一次调用里 `items` 块排在前、`paths` 块排在后，各块内部保持传入顺序。`insertedCount` 统计新入队的曲目数，`movedCount` 统计被移动的既有条目数。`invalidCount` 是输入路径数减去路径解析所得的曲目数，负数按零返回；它不是精确的失败路径数，因为文件夹或容器展开出的多首曲目可能抵消失败项。`items` 不计入 `invalidCount`：任一条目不是对象、缺少 `playlist` 或 `item`、位置越界时，整次调用失败，`error` 指明 `items[i]`，队列不变，同一次调用的 `paths` 也不会入队。

所有输入条目按曲目去重，不按位置。重复 `items` 中，优先保留首个已在队列中的输入坐标；都不匹配时才保留首个输入坐标，曲目在输入块内的顺序仍取首次出现的位置。移动时优先选择坐标完全匹配的既有条目，否则移动该曲目的首个队列条目并赋予所选坐标。仅按路径移动时保留既有条目的有效坐标，只有新插入的路径条目不带坐标。`queue.add` 和 `queue.setContents` 保留重复引用。`insertedCount + movedCount` 少于输入条目数，可能是去重或无效路径所致；文件夹和容器路径也可能展开为多首曲目。

两个数组都为空或都未传时以 `INVALID_PARAMS` 失败（`No paths or items specified`）；`items` 不是数组时报 `items must be an array`；没有 `items` 且 `paths` 一个有效曲目都解析不出时以 `NOT_FOUND` 失败（`No valid tracks found`）并带 `invalidCount`。坐标越界是 `INVALID_INDEX`，取不到该列表项是 `NOT_FOUND`，解析路径期间列表被改是 `OPERATION_FAILED`，错误都指明 `items[i]`。

::: warning 已知限定：旧版 `|subsong:N` 匹配
经旧版 `queue.addPaths` 的 `path|subsong:N` 语法进入队列的曲目，再次传给 `insertNext` 时可能匹配不上"已在队列中"——结果是重复入队而不是被移动。这只影响升级前已入队的曲目，新调用不受影响。多 subsong 文件的裸路径与该路径显式带 `|subsong:0` 后缀也被视为两个不同的身份，而非等价——不要假设它们会互相去重。
:::

::: warning 已知限定：播放游标不会跟随按路径插播的曲目
经 `paths` 入队的条目不带播放列表位置（它们不写入任何播放列表，这正是保持播放列表干净的代价）。foobar2000 因而把它们视为"列表外播放"：播放期间 `playback.getCurrentTrackIndex` 查不到位置；播完后从**插播前正在播放的那一项的下一项**继续，而不是从插播曲目在列表中的位置继续。当插播曲目本身也在正在播放的列表里时有两个可见后果：顺序在原处接续（如第 8 项 → 插播项 → 第 9 项），且顺序播到该曲目时它会**再播一次**。经 `items` 入队的条目与 `queue.add({ playlist, tracks })` 入队的条目一样带位置，没有这一行为——游标会跳到被消费项处继续。已知曲目在列表中的位置时，请改传 `items`。已入队条目若其列表位置失效（所引用的列表被清空），经 `moveToTop` / `setContents` / `playNow` 重建队列后也会退化为同样的无位置形态。

`playback.previous` 受同一机制影响。foobar2000 的"上一曲"按游标在列表中的位置推算，不按播放历史，已消费的队列条目也不会放回。无位置条目播放期间按上一曲，落到**被打断那一项的前一项**（被打断的曲目本身被跳过）；带位置条目播放期间按上一曲，落到该条目自身列表位置的前一项。两种情况都不会回到被打断的曲目。
:::

```javascript
// 按路径：条目不带播放列表位置
await fb2k.invoke('queue.insertNext', { paths: ['C:\\Music\\a.flac'], position: 0 });
// 按播放列表位置：播放时游标跟随该条目
await fb2k.invoke('queue.insertNext', { items: [{ playlist: 0, item: 12 }] });
```

### queue.playNow

<!-- api-schema:begin queue.playNow -->
立即起播队列中指定索引的条目；若目标不在队首则先行移至队首；队列为空时以 `NOT_FOUND` 失败，索引越界以 `INVALID_INDEX` 失败；条目移动后队列重建失败时以 `OPERATION_FAILED` 失败且不起播

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 待起播的队列条目位置索引不小于 `0`。默认 `0`。 |
| `expectedVersion` | `string` | 否 | 预期内容版本，用于乐观并发控制。版本不匹配时不执行变更并以 `STALE_TARGET` 失败（`details.current` 返回当前最新版本）。省略时不校验。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playedIndex` | `integer` | 实际执行起播的队列条目位置索引 |
| `queueCount` | `integer` | 起播后立即读取的队列长度；宿主消费已播条目可能发生在读取之前或之后 |
| `contentVersion` | `string` | 与 `queueCount` 同时读取的队列内容版本号 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

立即播放队列中 `index` 处的曲目；若该曲目尚不在队首，会先把它移到队首。

队列为空时以 `NOT_FOUND` 失败（`Queue is empty`）；`index` 越界时以 `INVALID_INDEX` 失败（`Invalid queue index`）；负数或非整数由参数校验拒绝。

::: warning `queueCount` 的时序不作保证
`queueCount` 是在播放启动后立即读取的。宿主不保证队首曲目的消费与这次读取同步发生，因此该字段可能反映的是播放曲目被移除**之前**或**之后**的队列状态。如果需要精确的播放后长度，请随后调用 `queue.getCount` 确认。
:::

```javascript
// 播放当前队首曲目
await fb2k.invoke('queue.playNow');
// 播放第 3 项，先移到队首再播放
await fb2k.invoke('queue.playNow', { index: 2 });
```

### 显式起播会清空队列

`playlist.playTrack`、`playlist.replaceAllAndPlay`、`playback.playPath`、`playback.playPaths`，以及 `jitQueue.preloadBatch` 在实际起播时（首次调用或 `replace: true`），都走 foobar2000 的默认动作路径，该路径会**先清空整个播放队列**。这是核心行为，不是组件附加的：核心把"播放这一项"实现为*清空队列 → 目标项入队 → 立即消费*，所以你会连续收到三次 `playback:queueChanged`（`user_removed` 且 `count: 0`、`user_added`、`playback_advance`）。这些调用都没有保留队列的选项。

**播放列表项且保留队列。** 先用 `setContents` 把目标项放到队首，再用 `playNow` 消费它：

```javascript
const res = await fb2k.invoke('queue.getCount');
if (res.success === false) throw new Error(res.error);
const { count } = res;
await fb2k.invoke('queue.setContents', {
    items: [
        { playlist, item },                                            // 现在要播的曲目
        ...Array.from({ length: count }, (_, i) => ({ queueIndex: i })), // 原有条目，保持原序
    ],
});
await fb2k.invoke('queue.playNow'); // 消费队首，其余条目留在队列里
```

查询条目数之后，再用两次调用完成一次队列重建和起播。被播放的曲目带有列表位置，游标会跟随它，上文“游标不会跟随”的限定不适用。

**恢复被清空的队列。** 宿主不保存被清掉的条目，所以要在起播调用**之前**用 `queue.get` 取得快照。`playlist` 不为 `null` 的条目可通过 `setContents` 或 `insertNext({ items })` 加入；`playlist: null` 的条目需要通过 `insertNext({ paths })` 重建。两种形态可放在同一次 `insertNext` 调用中，但坐标项会排在路径项之前，且按曲目去重。随后可用 `setContents` 恢复顺序和重复次数，但不保证恢复原来的坐标形态：同一曲目原先若同时有带坐标项和无坐标项，复制剩余条目的 `queueIndex` 也会复制它现有的坐标。全部按路径重建会丢失列表坐标，也可能合并重复项。
