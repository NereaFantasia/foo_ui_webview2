# Queue 播放队列 API

`queue` 命名空间的方法：读取与编辑播放队列。

## Queue API - 播放队列

### queue.addPaths

<!-- api-schema:begin queue.addPaths -->
按路径把曲目加入队列。入队需要曲目先在某个播放列表里，所以路径会先追加到一个播放列表：默认是专用的 `[WebView Queue]` 列表，不存在则创建。目标播放列表已上锁时以 `LOCKED` 失败，不添加也不入队。路径解析完会重新查找目标列表，处理同 `playlist.addPaths`：期间被挪动，曲目照样加进它，结果里的 `playlist` 是它的新索引；期间被删掉，以 `OPERATION_FAILED` 失败，不入队。有一条路径没通过媒体根检查，整次调用以 `PERMISSION_DENIED` 失败。解析不出曲目的路径被丢弃；一条都解析不出时以 `NOT_FOUND` 失败，失败里带 `invalidCount`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 文件路径或 URL，每条可带 `\|subsong:N` 后缀。超过 2048 字符的条目丢弃并计入 `invalidCount`。不能为空。 |
| `useQueuePlaylist` | `boolean` | 否 | 追加到专用的 `[WebView Queue]` 播放列表，不存在则创建。`false` 时用 `playlist`，它也省略时用活动播放列表。默认 `true`。 |
| `playlist` | `integer` | 否 | 目标播放列表；只在 `useQueuePlaylist` 为 `false` 时读取。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；它与 `playlistGuid` 都省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 目标播放列表的 `guid`，用来代替 `playlist`；和 `playlist` 一样只在 `useQueuePlaylist` 为 `false` 时读取，否则完全不检查。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `addedCount` | `integer` | 追加并入队的曲目数。 |
| `invalidCount` | `integer` | 被丢弃的 `paths` 条数：空的、超过 2048 字符的、建不出曲目的 `\|subsong:N`，以及一条都没解析出来时的全部普通路径。同一次调用里别的普通路径解析出了曲目时，解析不出的普通路径不计入，所以成功的调用实际丢弃的可能比这个数多。 |
| `playlist` | `integer` | 曲目被追加到的播放列表。 |
| `playlistGuid` | `string` | 播放列表的 GUID，写成 `{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}`。作为 `playlistGuid` 传入，播放列表清单变了之后仍能指定这个列表。 |
| `queueCount` | `integer` | 之后的队列长度。 |

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
按播放列表中的位置把一首或多首曲目加入队列。超出最后一行的位置跳过；一个都不在范围内时以 `INVALID_INDEX` 失败。负数位置以 `INVALID_PARAMS` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `playlist` | `integer` | 否 | 位置所指的播放列表；省略时为活动播放列表。超出最后一个播放列表的索引以 `INVALID_INDEX` 失败；省略且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。不小于 `0`。 |
| `playlistGuid` | `string` | 否 | 播放列表的 GUID，用来代替序号；写法带花括号，与 `playlist.getAll` 以及所有指明播放列表的结果、事件报出的一致；十六进制大小写均可。别的列表增删或重排时它仍指向同一个列表，序号做不到。同时给了它和序号，或 GUID 格式不对，以 `INVALID_PARAMS` 失败。列表已不存在时以 `NOT_FOUND` 失败，不会改用别的列表。GUID 格式不对与列表不存在这两种失败都以 `details.playlistGuid` 带回传入的 GUID。 |
| `tracks` | `integer[]` | 否 | 要入队的行，按顺序。优先于 `track`。每一项不小于 `0`。 |
| `track` | `integer` | 否 | 要入队的单个行；只在没传 `tracks` 时读取。不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `addedCount` | `integer` | 入队的行数。 |
| `queueCount` | `integer` | 之后的队列长度。 |

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
按播放顺序读取整个播放队列。不分页，全部条目一次返回。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `QueueItem[]` | 条目，按播放顺序。 |
| `items[].…` | [Track](../reference/types.md#track) | 包含 [Track](../reference/types.md#track) 的全部字段。 |
| `items[].queueIndex` | `integer` | 在队列中的位置，从 `0` 起。 |
| `items[].playlist` | `integer \| null` | 条目入队时所在的播放列表；不带播放列表位置时为 `null`：按路径入队，或 foobar2000 为它记的位置上已不是这首曲目（行或列表被删掉，或各行挪动了）。 |
| `items[].playlistGuid` | `string \| null` | 该播放列表的 GUID，写法与 `playlistGuid` 接受的一致；恰在 `playlist` 为 `null` 时为 `null`。 |
| `items[].playlistItem` | `integer \| null` | 在该播放列表中的行号；恰在 `playlist` 为 `null` 时为 `null`。 |
| `count` | `integer` | 条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

获取当前播放队列内容。

> `artist` / `albumArtist` / `genre` / `composer`（仅指该 API 实际返回的字段）的多值标签按 `, ` 原序拼接，不去重。

`playlist`、`playlistGuid` 与 `playlistItem` 三个键始终存在。记下的位置上仍是这首曲目时照实报出；否则三者同为 `null`：经 `queue.insertNext` 的 `paths` 入队的条目，以及行或列表被删、被挪动之后，记下的位置上已不是这首曲目的条目，都属于这种情形。所以报出的位置不会指向别的曲目。判断只需一句 `item.playlist == null`。

### queue.remove

<!-- api-schema:begin queue.remove -->
按 `index` 移除一条，或按 `indices` 移除多条。队列为空时不论给了什么都以 `NOT_FOUND` 失败。`index` 超出队尾以 `INVALID_INDEX` 失败。批量时重复的下标与超出队尾的下标跳过；一个都不在范围内时以 `INVALID_INDEX` 失败，失败里带 `removedCount` 为 `0` 与 `queueCount`。两个键都没给或下标为负数，以 `INVALID_PARAMS` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 要移除的一个队列位置。优先于 `indices`。不小于 `0`。 |
| `indices` | `integer[]` | 否 | 要移除的队列位置；只在没传 `index` 时读取。每一项不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `removedIndex` | `integer` | 被移除的位置；只在传了 `index` 时返回。 |
| `removedCount` | `integer` | 移除的条数；只在传了 `indices` 时返回。 |
| `queueCount` | `integer` | 之后的队列长度。 |

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
清空队列。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clearedCount` | `integer` | 队列原先的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

清空整个播放队列。

### queue.flush

<!-- api-schema:begin queue.flush -->
清空队列；与 `queue.clear` 是同一个操作，这是它的旧名字。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clearedCount` | `integer` | 队列原先的条目数。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

`queue.clear` 的别名。清空整个播放队列。

### queue.getCount

<!-- api-schema:begin queue.getCount -->
报告队列里有多少条目。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `count` | `integer` | 条目数。 |
| `hasItems` | `boolean` | 队列是否非空；等于 `count > 0`。 |

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
把队列中的一条移到队首，使其下一首播放。只改队列顺序，其余条目的相对顺序不变。`index` 为 `0`、超出队尾，以及队列为空时的任何下标，都以 `INVALID_INDEX` 失败。队列重建不成时以 `OPERATION_FAILED` 失败，失败里带 `queueCount`。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 是 | 要提到队首的条目位置；`0` 已经在队首，会被拒绝。不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `movedIndex` | `integer` | 条目原来的位置。 |
| `queueCount` | `integer` | 之后的队列长度。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

将队列中的指定项移动到队首（下一首播放）。内部通过清空队列并重建实现。

```javascript
// 将队列第 3 项移到队首
await fb2k.invoke('queue.moveToTop', { index: 3 });
```

### queue.setContents

<!-- api-schema:begin queue.setContents -->
用一份有序引用列表替换整个队列。任一条引用不可用，整次调用在写入前失败。空列表即清空队列。播放列表索引按调用到达时的清单解读，所以在列表清单变化之前取来的索引会把另一张列表的行加进队列；`playlistGuid` 始终指向读取时的那张列表。某一条同时给了 `playlist` 与 `playlistGuid`、或 GUID 格式不对，以 `INVALID_PARAMS` 失败；没有列表持有该 GUID 时以 `NOT_FOUND` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `items` | `QueueContentRef[]` | 是 | 新的队列内容，按顺序。最多 `max(256, 当前队列长度)` 条，多了以 `INVALID_PARAMS` 失败。 |
| `items[].queueIndex` | `integer` | 否 | 队列中已有条目的位置。不小于 `0`。 |
| `items[].playlist` | `integer` | 否 | 要加入的行所在的播放列表；须与 `item` 同时给出。不小于 `0`。 |
| `items[].playlistGuid` | `string` | 否 | 用 `guid` 指定要加入的行所在的播放列表，代替 `playlist`；须与 `item` 同时给出。 |
| `items[].item` | `integer` | 否 | 该播放列表中的行号；须与 `playlist` 或 `playlistGuid` 同时给出。不小于 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `queueCount` | `integer` | 之后的队列长度。 |

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
插入曲目使其下一首播放，排在已入队的所有条目之前。已在队列中的曲目被移动而不是再入队一次。`items` 块排在 `paths` 块之前，各块内部保持传入顺序。`paths` 与 `items` 都没给时以 `INVALID_PARAMS` 失败。`items` 的每一条都在解析路径之前检查，任一条不合格整次调用失败，什么也不写：一条用 `playlist` 与 `playlistGuid` 两个都没给或都给了、或 GUID 格式不对，以 `INVALID_PARAMS` 失败；没有列表持有该 GUID 时以 `NOT_FOUND` 失败；播放列表序号或行号越界以 `INVALID_INDEX` 失败。有一条路径没通过媒体根检查，整次调用以 `PERMISSION_DENIED` 失败；解析不出曲目的路径被丢弃并计入 `invalidCount`，什么也没剩下可插入时以 `NOT_FOUND` 失败。解析路径期间某个播放列表行变了，以 `OPERATION_FAILED` 失败；队列重建不成同样如此。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 否 | 文件路径或 URL，每条可带 `\|subsong:N` 后缀。由此入队的条目不带播放列表位置。 |
| `items` | `QueueListRef[]` | 否 | 播放列表行；由此入队的条目带该位置，播放游标会跟随。最多 `max(256, 当前队列长度)` 条，多了以 `INVALID_PARAMS` 失败。 |
| `items[].playlist` | `integer` | 否 | 播放列表索引；与 `playlistGuid` 给一个。不小于 `0`。 |
| `items[].playlistGuid` | `string` | 否 | 播放列表的 `guid`，代替 `playlist`。 |
| `items[].item` | `integer` | 是 | 该播放列表中的行号。不小于 `0`。 |
| `position` | `integer` | 否 | 插入位置，按移出被移动条目之后的队列计算；`0` 是队首。不小于 `0`。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `insertedCount` | `integer` | 新入队的曲目数。 |
| `movedCount` | `integer` | 已在队列中、被移动的条目数。 |
| `queueCount` | `integer` | 之后的队列长度。 |
| `invalidCount` | `integer` | 输入路径数减去由路径解析出的曲目数，最低为零。文件夹或播放列表文件会解析出多首曲目，所以它不是失败路径的条数。 |

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
立即播放队列中 `index` 处的条目；它不在队首时先移到队首。队列为空时以 `NOT_FOUND` 失败，`index` 超出队尾以 `INVALID_INDEX` 失败。需要移动条目而队列重建不成时以 `OPERATION_FAILED` 失败，不起播。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `integer` | 否 | 要播放的队列位置。不小于 `0`。默认 `0`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `playedIndex` | `integer` | 被播放的位置。 |
| `queueCount` | `integer` | 播放启动后立即读到的队列长度；宿主消费被播放条目可能在这次读取之前或之后。 |

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
