# fb.queue 播放队列

`fb.queue` 管理 foobar2000 的播放队列。它和 `fb.jitQueue` 提供的即时队列是两回事。

## setContents(items)

用一份有序的 `QueueContentRef` 列表替换整个队列——`{ queueIndex }` 保留/重排一个已在队列中的槽位，`{ playlist, item }` 添加一个尚未入队的播放列表曲目。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `QueueContentRef[]` | 有序的队列槽位或播放列表曲目引用列表 |

任一条目形态无法识别都会让整次调用在写入前整体失败——与逐项跳过越界下标的 `fb.queue.add` 不同。传入 `[]` 会清空队列，等价于 `fb.queue.clear()`。

应在用户松手结束拖拽重排手势时调用一次，不要在拖拽过程中逐帧调用：每次调用都会清空并重建整个队列。

```javascript
// 重排：把队列第 2 项移到队首，其余保持原序
await fb.queue.setContents([{ queueIndex: 2 }, { queueIndex: 0 }, { queueIndex: 1 }]);
```

## insertNext(entries, position?)

插入曲目使其成为下一首播放，插入到当前队列所有曲目之前。每个条目是文件路径（`string`）或播放列表位置（`QueueListRef`，即 `{ playlist, item }`）；已在队列中的曲目会被移动到 `position`，而不是重复入队。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `entries` | `Array<string \| QueueListRef>` | 文件路径或 URL（可带 `\|subsong:N` 后缀）、播放列表位置，或两者混合 |
| `position` | `number?` | 移除所有被移动条目之后的插入下标，默认 `0` |

新插入的路径条目不带播放列表位置，游标无法跟随该曲目的列表位置；按路径移动既有条目时，会保留其有效坐标。需要指定列表位置时，传入 `QueueListRef`。混合数组按类型拆分：坐标项在前，路径项在后，各块内部保序。曲目互不相同时，`[p1, I1, p2]` 入队后为 `I1, p1, p2`；需要精确交错顺序时，随后调用 `setContents`。所有条目按曲目去重；重复坐标中优先保留首个已在队列中的输入坐标，都不匹配时才保留首个输入坐标。曲目在输入块内的顺序仍取首次出现的位置。

返回值区分 `insertedCount`（新入队曲目数）与 `movedCount`（被移动的既有条目数）。`invalidCount` 是输入路径数减去解析所得的曲目数，负数按零返回；文件夹或容器展开可能抵消失败项，因此它不是精确的失败路径数。任一位置条目有问题，整次调用在写入前失败。

```javascript
const result = await fb.queue.insertNext(['C:\\Music\\a.flac'], 0);
if (result.success === false) throw new Error(result.error);
console.log(result.insertedCount, result.movedCount);
// 下一首播放列表 0 的第 12 项；游标会跟随
await fb.queue.insertNext([{ playlist: 0, item: 12 }]);
```

## playNow(index?)

立即播放队列中 `index` 处的曲目；若该曲目尚不在队首，会先将其移到队首。默认 `index: 0`，即当前队首。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `index` | `number?` | 要播放的队列下标，默认 `0` |

返回值中的 `queueCount` 是播放启动后立即读取的，不保证反映播放曲目被消费前还是消费后的状态——如需精确的播放后长度，请随后调用 `fb.queue.getCount()`。

```javascript
await fb.queue.playNow(); // 播放当前队首
await fb.queue.playNow(2); // 移到队首并播放第 3 项
```

## remove(target)

移除队列条目：传一个位置时作为宿主的 `index` 发出；传位置数组时作为 `indices` 发出，一次调用全部移除。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `target` | `number \| number[]` | 一个队列位置，或队列位置数组 |

传数组时，重复与越界的位置跳过，返回值带 `removedCount`；传单个位置时返回值带 `removedIndex`。两种都带 `queueCount`，即移除后的队列长度。给出的位置一个都不在范围内时以 `INVALID_INDEX` 失败。用数组一次移除，不会出现逐个移除同一批位置时的下标错位。

```javascript
const one = await fb.queue.remove(0);
if (one.success === false) throw new Error(one.error);
console.log(one.removedIndex, one.queueCount);

const many = await fb.queue.remove([4, 1, 2]);
if (many.success === false) throw new Error(many.error);
console.log(many.removedCount, many.queueCount);
```

## add(opts)

签名：`fb.queue.add(opts: QueueAddParams): Promise<QueueAddResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `opts.playlist` | `number` | 否 | 行号所在的播放列表；省略时为活动播放列表 |
| `opts.playlistGuid` | `string` | 否 | 播放列表的 `guid`（取自 `playlist.getAll`），用来代替 `playlist` |
| `opts.tracks` | `number[]` | 否 | 要入队的行，按顺序；优先于 `track` |
| `opts.track` | `number` | 否 | 要入队的单个行；只在没传 `tracks` 时读取 |

按播放列表中的行号把一首或多首曲目加入队列。超出最后一行的行号跳过；一个都不在范围内时以 `INVALID_INDEX` 失败，负数行号以 `INVALID_PARAMS` 失败。`playlist` 超出最后一个播放列表时以 `INVALID_INDEX` 失败；省略 `playlist` 且没有活动播放列表时以 `NO_ACTIVE_ITEM` 失败。同时给了 `playlist` 和 `playlistGuid`，或 GUID 格式不对，以 `INVALID_PARAMS` 失败；列表已不存在时以 `NOT_FOUND` 失败。

返回值带 `addedCount`（入队的行数）和 `queueCount`（之后的队列长度）。

```javascript
const result = await fb.queue.add({ playlist: 0, tracks: [3, 5] });
if (result.success === false) throw new Error(result.error);
console.log(result.addedCount, result.queueCount);
```

## addPaths(paths, opts?)

签名：`fb.queue.addPaths(paths: string[], opts?: Omit<QueueAddPathsParams, 'paths'>): Promise<QueueAddPathsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 文件路径或 URL，每条可带 `\|subsong:N` 后缀；至少一条 |
| `opts.useQueuePlaylist` | `boolean` | 否 | 追加到专用的 `[WebView Queue]` 播放列表，不存在则创建；默认 `true` |
| `opts.playlist` | `number` | 否 | 目标播放列表，只在 `useQueuePlaylist` 为 `false` 时读取；省略时用活动播放列表 |
| `opts.playlistGuid` | `string` | 否 | 目标播放列表的 `guid`（取自 `playlist.getAll`），用来代替 `playlist`；只在 `useQueuePlaylist` 为 `false` 时读取 |

按路径或 URL 把曲目加入队列。入队需要曲目先在某个播放列表里，所以路径会先追加到一个播放列表，默认是专用的 `[WebView Queue]` 列表。每条路径或 URL 最多 2048 字符，超长的条目跳过并计入 `invalidCount`。

目标播放列表已上锁时以 `LOCKED` 失败，不添加也不入队。同时给了 `playlist` 和 `playlistGuid`，或 GUID 格式不对，以 `INVALID_PARAMS` 失败；列表已不存在时以 `NOT_FOUND` 失败。路径解析完会重新查找目标列表：期间被挪动，曲目照样加进它，返回值里的 `playlist` 是它的新索引；期间被删掉，以 `OPERATION_FAILED` 失败，不入队。

返回值带 `addedCount`（追加并入队的曲目数）、`invalidCount`（`paths` 里没有解析出曲目的条数，含超长条目）、`playlist`（曲目被追加到的播放列表）和 `queueCount`。

```javascript
const result = await fb.queue.addPaths(['E:\\Music\\song.flac'], { useQueuePlaylist: true });
if (result.success === false) throw new Error(result.error);
console.log(result.addedCount, result.invalidCount, result.queueCount);
```

## clear()

签名：`fb.queue.clear(): Promise<QueueClearResponse>`

清空播放队列。返回值带 `clearedCount`，即队列原先的条目数。`flush()` 是同一个操作的旧名字。

```javascript
const result = await fb.queue.clear();
if (result.success === false) throw new Error(result.error);
console.log(result.clearedCount);
```

## flush()

签名：`fb.queue.flush(): Promise<QueueFlushResponse>`

清空播放队列。`flush()` 封装 `queue.flush`，它是 `queue.clear` 的旧名字；`clear()` 经 `queue.clear` 做同一个操作。返回值带 `clearedCount`，即队列原先的条目数。

```javascript
const result = await fb.queue.flush();
```

## get()

签名：`fb.queue.get(): Promise<QueueGetResponse>`

按播放顺序读取整个播放队列，不分页，返回 `{ items, count }`。每个 `QueueItem` 是共享的 `Track` 曲目行，加上 `queueIndex`（在队列中的位置，从 `0` 起），以及 `playlist`、`playlistGuid` 和 `playlistItem`（条目入队时所在的播放列表位置）。没有记下位置，或者记下的位置上已不是这首曲目时，三者都为 `null`：它是按路径入队的，或者之后行或列表被删、被挪动过。

```javascript
const queue = await fb.queue.get();
if (queue.success === false) throw new Error(queue.error);
for (const item of queue.items) {
	console.log(item.queueIndex, item.path, item.playlist, item.playlistItem);
}
```

## getCount()

签名：`fb.queue.getCount(): Promise<QueueGetCountResponse>`

以 `{ count, hasItems }` 返回当前队列长度；`hasItems` 等于 `count > 0`。

```javascript
const result = await fb.queue.getCount();
if (result.success === false) throw new Error(result.error);
console.log(result.count);
```

## moveToTop(index)

签名：`fb.queue.moveToTop(index: number): Promise<QueueMoveToTopResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `index` | `number` | 是 | 要移到队首的条目位置；`0` 会被拒绝 |

把队列中的一条移到队首，使它下一首播放。只改队列顺序，其余条目的相对顺序不变。`index` 为 `0`（已在队首）或超出队尾时以 `INVALID_INDEX` 失败。

返回值带 `movedIndex`（条目原来的位置）和 `queueCount`。

```javascript
const result = await fb.queue.moveToTop(3);
if (result.success === false) throw new Error(result.error);
console.log(result.movedIndex, result.queueCount);
```
