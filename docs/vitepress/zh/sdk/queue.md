# fb.queue 播放队列

`fb.queue` 管理 foobar2000 原生待播队列（与 `fb.jitQueue` 提供的即时动态队列属于独立机制）。

## setContents(items)

用一份有序的 `QueueContentRef` 列表替换整个队列——`{ queueIndex }` 保留/重排一个已在队列中的槽位，`{ playlist, item }` 添加一个尚未入队的播放列表曲目。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `items` | `QueueContentRef[]` | 有序的队列槽位或播放列表曲目引用列表 |

若存在格式无法识别的条目，整次调用将在写入前直接拒绝并失败（与逐项跳过无效索引的 `fb.queue.add` 行为不同）。传入空数组 `[]` 将清空当前队列，等价于调用 `fb.queue.clear()`。

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

通过文件路径插入的新条目不关联播放列表坐标，播放光标无法跟随到该曲目的位置；按路径移动既有队列条目时，保留其原有的有效播放列表坐标。如需关联播放列表上下文，应传入 `QueueListRef`。包含不同类型的混合数组在处理时将按类型归类：坐标条目排在首块，路径条目排在其后，各自保持原输入顺序。若所有曲目互不相同，输入序列 `[p1, I1, p2]` 处理后顺序为 `[I1, p1, p2]`；如需严格的交错顺序，应在插入后调用 `setContents()` 重排。输入条目基于曲目实体执行去重；若存在重复坐标，优先保留已存在于队列中的首个匹配坐标，均不匹配时保留首个输入坐标。各曲目在对应分块内的位置均以其首次出现为准。

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

从待播队列中移除条目：传入单个数字时按 `index` 指定单项；传入数字数组时按 `indices` 批量移除。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `target` | `number \| number[]` | 一个队列位置，或队列位置数组 |

传入数组时，重复与越界的索引将被自动忽略，响应包含 `removedCount`；传入单个位置时响应包含 `removedIndex`。两种形式均返回操作后的队列长度 `queueCount`。若指定的位置全部超出有效范围，以 `INVALID_INDEX` 失败。使用数组批量移除可避免因逐项删除导致的索引动态漂移。

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
| `opts.track` | `number` | 否 | 单曲行号；仅在未指定 `tracks` 时生效 |

按播放列表中的行号把一首或多首曲目加入队列。超出有效范围的行号将被忽略；若全部行号均无效，以 `INVALID_INDEX` 失败，负数行号以 `INVALID_PARAMS` 失败。`playlist` 索引超出范围以 `INVALID_INDEX` 失败；省略 `playlist` 且无活动播放列表时以 `NO_ACTIVE_ITEM` 失败。若同时指定 `playlist` 与 `playlistGuid`，或 GUID 格式无效，以 `INVALID_PARAMS` 失败；目标列表不存在时以 `NOT_FOUND` 失败。

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

目标播放列表已锁定时以 `LOCKED` 失败，不执行添加与入队。若同时指定 `playlist` 与 `playlistGuid`，或 GUID 格式无效，以 `INVALID_PARAMS` 失败；目标列表不存在时以 `NOT_FOUND` 失败。解析路径后重新定位目标播放列表：若期间列表被移动，曲目仍将追加至该列表并返回更新后的 `playlist` 索引；若期间列表被删除，调用以 `OPERATION_FAILED` 失败且不入队。

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

按播放顺序读取待播队列的全量数据（不执行分页），返回 `{ items, count }`。每个 `QueueItem` 包含共享的 `Track` 曲目属性、当前队列索引 `queueIndex`（从 `0` 开始），以及关联的播放列表信息（`playlist`、`playlistGuid` 和 `playlistItem`）。若条目通过路径直接入队，或入队后所引用的行或播放列表被删除、移动，上述播放列表关联字段将返回 `null`。

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

签名：`fb.queue.moveToTop(index: number, expectedVersion?: string): Promise<QueueMoveToTopResponse>`

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
