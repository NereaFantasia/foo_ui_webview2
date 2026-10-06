# fb.queue 播放队列

本页是 `fb.queue` 的 SDK 视角文档入口。

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

<!-- BEGIN AUTO-GENERATED SDK STUBS -->

## 其余方法

### flush()

封装 `queue.flush`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.queue.flush(/* 参数见 TypeScript 声明 */);
```

### getCount()

封装 `queue.getCount`。参数与返回类型以 `foo-webview-sdk` 的 TypeScript 声明为准（IDE 悬浮提示或包内 `bridge.d.ts`），行为契约见 API 文档对应条目。

```javascript
await fb.queue.getCount(/* 参数见 TypeScript 声明 */);
```

<!-- END AUTO-GENERATED SDK STUBS -->
