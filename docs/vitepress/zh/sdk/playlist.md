# fb.playlist 播放列表

本页凡是接受播放列表（`index`、`playlistIndex`）的方法都接受 `PlaylistRef`：播放列表的序号，或 `getAll()`、`getActive()`、`create()`、`duplicate()` 报告的 `guid` 字符串。序号指的是宿主处理这次调用时位于该位置的列表；`guid` 在别的列表增删或重排时仍指向同一个列表，该列表不存在后以 `NOT_FOUND` 失败。`fb.library.addToPlaylist` 与 `fb.artwork.getByPlaylistItem` 同样接受 `PlaylistRef`。

## getAll()

获取所有播放列表，返回 `{ playlists, count }`，`playlists` 是按播放列表顺序排列的 `PlaylistInfo[]`；失败时返回失败信封。

```javascript
const res = await fb.playlist.getAll();
if (res.success === false) throw new Error(res.error);
// res.playlists: [{index: 0, guid: "{…}", name: "Default", trackCount: 100, isActive: true, isPlaying: false, isLocked: false, isAutoplaylist: false}, ...]
```

构建列表选择菜单或下拉项时，建议保存各播放列表的 `guid`，并在用户确认选择时传入：

```javascript
const res = await fb.playlist.getAll();
if (res.success === false) throw new Error(res.error);
const target = res.playlists.find((p) => p.name === '收藏');
// 跨异步生命周期调用时使用 GUID，避免因并发增删或重排导致索引错位
if (target) await fb.library.addToPlaylist(['E:\\Music\\song.flac'], target.guid);
```

::: tip `PlaylistInfo` 不含时长
`PlaylistInfo` 不包含 `duration` 字段。如需播放列表的总时长，请调用 `fb.playlist.getActive()` 或 `fb.playlist.getPlaying()`。
:::

## getActive() / setActive(index)

获取/设置当前活动播放列表。`getActive()` 返回 `PlaylistGetActiveResponse`：没有活动播放列表时 `found` 为 `false`，其余字段不出现。

```javascript
const info = await fb.playlist.getActive();
if (info.success === false) throw new Error(info.error);
// {found, index, name, trackCount, isActive, isPlaying, isLocked, duration}
if (info.found) console.log(info.index, info.name);

await fb.playlist.setActive(1);
```

## getTracks(index, start, count)

获取播放列表中的曲目（分页）。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| start | number | 起始位置（默认 0） |
| count | number | 获取数量（默认 100） |
| formats | Record<string, string>? | 具名的 Title Formatting 附加列；每行在 `formats` 下带上它们的值 |
| fields | string[]? | 要返回的曲目字段；每行包含所选字段和 `index` |

返回分页对象 `{ playlist, start, count, total, tracks }`，由于 `fields` 可以限定每行包含的键，`tracks` 的元素类型为 `PlaylistTrackPartial`。

```javascript
const page = await fb.playlist.getTracks(0, 200, 200, undefined, ['title', 'album']);
if (page.success === false) throw new Error(page.error);
console.log(`${page.tracks.length} / ${page.total}`);
```

`fields` 可用的名字与 `library.query` 相同，区分大小写。省略时返回整行，整行还带 `composer`、`comment` 与 foo_playcount 的播放统计。请求被拒（比如字段名未知）时返回 `{ success: false, code: 'INVALID_PARAMS' }`。

每页响应均包含 `contentVersion`，`getGroupRuns()` 的应答亦同。相同 `contentVersion` 的分页来自同一数据快照。若 `contentVersion` 发生变更，指示列表期间经历了插入、删除、重排或替换操作（即使曲目总数 `total` 保持未变）。

## getTracksPage(index, start, count, formats?, fields?)

已弃用：与 `getTracks()` 是同一个调用，返回同样的分页响应。请改用 `getTracks()`。

## getGroupRuns(patterns, index?)

按顺序计算播放列表的分组区间，仅返回分组游程边界与统计元数据（不包含具体曲目行）。

游程是相邻且分组键相同的最长一段，比较时按 ASCII 的 `A-Z`/`a-z` 折叠大小写。分组计算不重排原始曲目，游程在数组中的相对顺序与列表中曲目的物理顺序严格一致。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| patterns | string[] | 一到两个 Title Formatting 模式；第二个在每个游程内再分一层 |
| index | number? | 要分组的播放列表，默认活动列表 |

```javascript
const res = await fb.playlist.getGroupRuns([
  '%album artist% | %album%',
  "$if(%discnumber%,'Disc '%discnumber%,)",
]);
if (res.success === false) throw new Error(res.error);
const { runs, total } = res;
// runs[1].sub[0].start 是全表绝对行号，不是父游程内的偏移
```

成功时，`runs` 覆盖整份列表。非空列表的 `runs[0].start` 为 0，相邻游程首尾相接，`count` 之和等于 `total`。空列表返回 `total: 0` 和 `runs: []`。`sub` 仅在指定两个分组模式时返回，其 `start` 采用与父级游程相同的绝对行索引。

与 `getTracks()` 搭配可实现分组虚拟列表：由游程提供组头位置以计算滚动总高度，可见视口的曲目行按需分页获取。对于 10 万首曲目的列表，仅当每个游程平均约含 10 首时，顶层游程才约为 1 万个。响应开销主要取决于分组键长度与二级游程数量；完整曲目行的开销则取决于请求字段及元数据大小。

若分组模式导致每行单独成组，`runs` 数组长度将等同于 `total`。宿主对游程数量不设硬性限制，建议选用能够合理合并相邻曲目的模式。

`patterns` 格式无效或表达式语法错误，返回 `{ success: false, code: 'INVALID_PARAMS' }`；空模式或编译失败的模式由 `details.pattern` 指明错误下标。播放列表索引超出有效范围时返回 `INVALID_INDEX`；省略索引且当前无活动播放列表时返回 `NO_ACTIVE_ITEM`。

## getMatchingRows(query, index?) / getTracksAt(index, rows, formats?, fields?)

筛选大型播放列表采用两步操作：先由 `getMatchingRows()` 执行查询并返回匹配行号索引列表（`{ playlist, playlistGuid, total, items, count }`，`items` 为升序行号），再由 `getTracksAt()` 批量读取指定行号的曲目行数据。`getTracksAt()` 按传入顺序返回行对象（格式与 `getTracks()` 相同），超出有效范围的行号自动忽略，返回行均附带 `index`。

```javascript
const guid = '{A9624480-77F0-4A2B-A76F-7EAB0C2EEC1C}';
const hits = await fb.playlist.getMatchingRows('artist HAS nachi', guid);
if (hits.success === false) {
  // 查询本身被拒时 details.param 为 'query'
} else {
  const page = await fb.playlist.getTracksAt(guid, hits.items.slice(0, 50), undefined, ['title']);
}
```

无法解析的查询语法或包含 `SORT BY` 的表达式将以 `INVALID_PARAMS` 拒绝，并附带 `details.param: 'query'`；`error` 文本使用宿主的界面语言。查询中的小写字母可匹配标签中的任意大小写，大写字母只匹配大写。播放列表不存在时 `getMatchingRows()` 失败（返回 `INVALID_INDEX`、`NOT_FOUND` 或 `NO_ACTIVE_ITEM`）；`getTracksAt()` 则与 `getTracks()` 一致，返回包含空数组的成功响应。

返回的行索引属于静态快照。当收到 `playlist:itemsAdded`、`itemsRemoved`、`itemsReordered`、`itemsReplaced` 事件且其 `contentVersion` 不一致时，应废弃旧索引并重新查询；由于标签修改不会触发版本号变更，收到 `metadb:changed` 事件时亦须重新拉取。此外，包含时间计算的查询（如 `%added% DURING LAST 2 WEEKS`）其匹配集会随系统时间推移而动态漂移。

## playTrack(playlistIndex, trackIndex, options?)

播放指定曲目。返回 `{success}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| playlistIndex | number | 播放列表索引 |
| trackIndex | number | 曲目索引 |
| options.deferred | boolean | 延迟执行（默认 false） |
| options.muted | boolean | 播放前先静音（默认 false） |

```javascript
await fb.playlist.playTrack(0, 4); // 第一个播放列表的第 5 首
await fb.playlist.playTrack(0, 0, { muted: true }); // 播放但静音
```

## add(index, paths)

添加文件到播放列表。返回 `{success, playlist, requestedPaths, addedCount, countBefore, totalCount}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| paths | string[] | 文件路径数组 |

```javascript
const r = await fb.playlist.add(0, ['E:\\Music\\song1.flac', 'E:\\Music\\song2.mp3']);
if (r.success === false) throw new Error(r.error);
console.log(`添加了 ${r.addedCount} 首`);
```

## removeTracks(index, indices)

从播放列表移除指定索引的曲目。返回 `{success}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| indices | number[] | 要移除的曲目索引数组 |

```javascript
await fb.playlist.removeTracks(0, [0, 2, 5]);
```

## getPlaying()

获取当前正在播放的播放列表。返回结构同 `getActive()`，没有正在播放的列表时 `found` 为 `false`。

```javascript
const playing = await fb.playlist.getPlaying();
```

## getCount(index)

获取第 `index` 个播放列表的曲目数，返回 `{count}`；列表不存在时 `count` 为 0。它调用宿主的 `playlist.getTrackCount`。宿主的 `playlist.getCount` 返回的是播放列表的个数，在 SDK 里对应 [`getPlaylistCount()`](#getplaylistcount)。

```javascript
const r = await fb.playlist.getCount(0);
if (r.success === false) throw new Error(r.error);
console.log(r.count);
```

## create(name?)

创建新播放列表。返回 `{success, index}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| name | string | 播放列表名称（默认 "New Playlist"） |

```javascript
const r = await fb.playlist.create('Rock');
if (r.success === false) throw new Error(r.error);
console.log(`新播放列表索引: ${r.index}`);
```

## remove(index) / clear(index)

删除播放列表 / 清空曲目。返回 `{success}`。

```javascript
await fb.playlist.remove(2);
await fb.playlist.clear(0);  // 清空但保留列表
```

## rename(index, name)

重命名播放列表。返回 `PlaylistRenameResponse`；列表上的锁拒绝新名字时以 `LOCKED` 失败。

```javascript
await fb.playlist.rename(0, 'Favorites');
```

## duplicate(index, name?)

复制播放列表。返回 `{success, index, sourcePlaylist, newPlaylist, name, trackCount}`。当 `name` 省略或为空串时，新播放列表名称以原名称附加 ` (Copy)` 后缀命名。

```javascript
const r = await fb.playlist.duplicate(0);
if (r.success === false) throw new Error(r.error);
console.log(`复制到索引 ${r.newPlaylist}`);
```

## addAsync(index, paths) / addSequential(index, paths)

异步添加文件（不阻塞 UI）/ 顺序添加（保证文件顺序）。返回 `{success, addedCount}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| paths | string[] | 文件路径数组 |

```javascript
await fb.playlist.addAsync(0, ['E:\\Music\\*.flac']);
await fb.playlist.addSequential(0, paths); // 保证文件顺序
```

## addHandles(index, handles)

精确添加轨道，不自动展开 CUE。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| handles | PlaylistHandleRef[] | 每项是路径（可带 `\|subsong:N` 后缀）或 `{ path, subsong }` |

```javascript
await fb.playlist.addHandles(0, [
    { path: 'E:\\Music\\album.cue', subsong: 1 },
    { path: 'E:\\Music\\album.cue', subsong: 2 },
]);
```

## insertTracks(index, insertIndex, handles)

在 `insertIndex` 行之前插入轨道，超出最后一行时追加到末尾。返回 `PlaylistInsertTracksResponse`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| insertIndex | number | 插入位置 |
| handles | PlaylistHandleRef[] | 要插入的曲目，写法同 `addHandles` |

```javascript
await fb.playlist.insertTracks(0, 5, handles);
```

## removeSelectedTracks(index)

签名：`fb.playlist.removeSelectedTracks(index: PlaylistRef): Promise<PlaylistRemoveSelectedTracksResponse>`

从播放列表移除选中的行，事先保存撤销点。已上锁的列表以 `LOCKED` 失败。

```javascript
await fb.playlist.removeSelectedTracks(0);
```

## getFocused(index) / setFocused(index, trackIndex)

获取或设置焦点曲目（光标位置）。`getFocused` 返回 `{playlist, index}`，播放列表无焦点或列表不存在时 `index` 为 `-1`；`setFocused` 传入负数 `trackIndex` 可清除当前播放列表的焦点光标。

```javascript
const r = await fb.playlist.getFocused(0);
if (r.success === false) throw new Error(r.error);
console.log(`焦点在第 ${r.index} 首`);
await fb.playlist.setFocused(0, 10);
```

## getSelection(index) / getSelectedTracks(index)

获取选中曲目索引/详细信息。

- `getSelection` 返回 `{items: number[], count, playlist}`
- `getSelectedTracks` 返回 `{ playlist, count, tracks }`，每项为完整 `PlaylistTrack` 行对象（不包含播放统计）。选区为空时返回 `tracks: []`；若指定的播放列表不存在，则返回失败错误信封。

```javascript
const sel = await fb.playlist.getSelection(0); // {items: [0, 5, 10], count: 3}
const selected = await fb.playlist.getSelectedTracks(0);
if (selected.success === false) throw new Error(selected.error);
console.log(selected.tracks.length);
```

## setSelection(index, indices, clearOthers?) / selectAll(index) / deselectAll(index)

设置/全选/取消选中。返回 `{success}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| indices | number[] | 要选中的曲目索引 |
| clearOthers | boolean | 是否清除其他选中（默认 true） |

```javascript
await fb.playlist.setSelection(0, [1, 3, 5]);
await fb.playlist.selectAll(0);
await fb.playlist.deselectAll(0);
```

## moveTracks(index, indices, delta)

移动曲目。返回 `{success}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| indices | number[] | 要移动的曲目索引 |
| delta | number | 移动偏移量（正=下移，负=上移） |

```javascript
await fb.playlist.moveTracks(0, [0, 1], 3);  // 向下 3 位
await fb.playlist.moveTracks(0, [5, 6], -2); // 向上 2 位
```

## sort(index, pattern, descending?, selectedOnly?)

按 Title Formatting 模式排序。返回 `{success}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| pattern | string | 排序模式（Title Formatting） |
| descending | boolean | 是否降序（默认 false） |
| selectedOnly | boolean | 是否只排序选中项（默认 false） |

```javascript
await fb.playlist.sort(0, '%artist%');
await fb.playlist.sort(0, '%date%', true); // 按日期降序
```

## reorder(index, order) / shuffle(index) / reverse(index)

自定义顺序 / 随机打乱 / 反转。返回 `{success}`。

```javascript
await fb.playlist.reorder(0, [3, 1, 0, 2]); // 自定义顺序
await fb.playlist.shuffle(0);
await fb.playlist.reverse(0);
```

## undo(index) / redo(index)

撤销/重做播放列表操作。返回 `{success}`。

```javascript
await fb.playlist.undo(0);
await fb.playlist.redo(0);
```

## 智能播放列表

### isAutoplaylist(index)

检查是否为智能播放列表。返回 `{isAutoplaylist}`。

```javascript
const r = await fb.playlist.isAutoplaylist(0);
if (r.success === false) throw new Error(r.error);
if (r.isAutoplaylist) console.log('这是智能播放列表');
```

### createAutoplaylist(name, query, sort?, keepSorted?)

创建智能播放列表。返回 `{success, index, playlist, name, query}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| name | string | 播放列表名称 |
| query | string | 搜索查询表达式 |
| sort | string | 排序模式（可选） |
| keepSorted | boolean | 是否保持排序（默认 false） |

```javascript
await fb.playlist.createAutoplaylist(
    'Recent',
    '%added% DURING LAST 2 WEEKS',
    '%added%',
    true
);
```

### getAutoplaylistInfo(index)

读取智能播放列表状态和编辑器能力，不打开 UI。成功响应始终带 `editorAvailability` 和 `editorAvailabilityReason`，普通列表也一样：

| `editorAvailability` | `editorAvailabilityReason` | 含义 |
| --- | --- | --- |
| `available` | `supported` | 提供者声明支持编辑器 |
| `unavailable` | `noEditor` | 提供者明确表示没有编辑器 |
| `unknown` | `legacyClient` | client 没有能力探测接口 |
| `unknown` | `unverifiedLock` | 兼容识别的自动列表锁没有能力探测接口 |
| `unavailable` | `notAutoplaylist` | 常规静态播放列表 |

`available` 不保证调用后必定弹出窗口；`unknown` 允许发起调用尝试，但无法提前预判结果。查询提供者发生内部异常时，以 `OPERATION_FAILED` 失败，不归类为 `unavailable`。

`source:'sdk'` 表示 autoplaylist manager 管理的 client，也包括第三方 client。`source:'dui'` 表示按既有 `Auto` 锁名规则识别的列表，不代表当前 UI 模式或编辑器能力。这条锁路径的 `keepSorted:false` 是配置不可读时的占位值，不能当作实际设置。普通列表返回 `isAutoplaylist:false`，不带 `source` 或 `keepSorted`。

```javascript
const info = await fb.playlist.getAutoplaylistInfo(0);
if (info.success === false) throw new Error(info.error);
if (info.isAutoplaylist) {
    console.log(info.source, info.keepSorted);
}
```

### getAutoplaylistQuery(index)

签名：`fb.playlist.getAutoplaylistQuery(index: PlaylistRef): Promise<PlaylistGetAutoplaylistQueryResponse>`

读取智能播放列表配置，同时返回 `query` 与 `sort`。由 foobar2000 内置 query/sort 客户端驱动的列表，两者均为客户端当前生效的参数，每次调用重新读取；未配置排序模式时 `sort` 为空字符串。第三方提供者、本组件托管的列表（使用 `getAutoplaylistConfig()` 读取）或配置无法解析时，两者返回 `null`，并通过 `note` 描述原因。早期宿主版本可能不包含 `sort` 字段，读取时应先校验 `query` 再访问 `sort`。`source` 与不可读 `keepSorted` 的语义同前文所述；该方法不包含编辑器能力字段，需检测编辑器可用性时请使用 `getAutoplaylistInfo()`。

```javascript
const res = await fb.playlist.getAutoplaylistQuery(0);
```

### showAutoplaylistUI(playlist)

签名：`fb.playlist.showAutoplaylistUI(playlist: PlaylistRef): Promise<PlaylistShowAutoplaylistUIResponse>`

请求指定 `PlaylistRef` 的原生编辑器，目标必填，不切换活动列表、不启动播放。数字索引按宿主首次处理请求时的位置解析，后续操作固定跟随解析出的 GUID。需要在首次排队期间保持目标身份时，应传 GUID。

成功受理时 Promise 返回 `{ success:true, accepted:true, operationId, playlistGuid }`，不等待原生调用或窗口结束。受理前的业务失败返回 `{ success:false, error, code, details }`；受理后的失败只经 `onAutoplaylistUIResult` 发送。桥传输失败仍可能 reject Promise。错误分类和生命周期详见 [API 说明](../api/playlist.md#playlist-showautoplaylistui)。

foobar2000 进程内，同一个 GUID 只允许一个排队中或执行中的调用。原生调用返回时即解除限制；非模态编辑器此时可能仍然打开。窗口归属及后续调用是否复用窗口由提供者决定，SDK 不能指定 owner，也不保证复用。

### onAutoplaylistUIResult(callback)

签名：`fb.playlist.onAutoplaylistUIResult(callback: (event: PlaylistAutoplaylistUIResultPayload) => void): () => void`

订阅 `playlist:autoplaylistUIResult`，返回取消订阅函数。务必**先订阅，再调用** `showAutoplaylistUI`：事件可能早于页面处理受理响应。按 `operationId` 关联，并核对 `playlistGuid` 是否为预期目标。每个受理操作最多向原调用文档发一个结果；导航或销毁后丢弃，不转发给主窗口。

`success:true` 只表示原生调用正常返回；锁入口还须返回 `true`。它不证明窗口出现、保存了修改、取消了编辑或关闭了窗口。失败事件带 `error`、`code` 和 `reason`。原文档仍有效但目标在调用中删除时，`NOT_FOUND` / `targetGone` 优先于同时发生的提供者异常；提供者已执行的动作不会因此撤销。

下面的示例统一管理一个页面的编辑器请求，最多跟踪 16 个请求、暂存 16 个早到结果。视图卸载时调用 `dispose()`；`pagehide` 也会清理页面资源。停止观察不会关闭提供者的编辑器。页面上的编辑器请求应共用这一个消费方，以免其他调用的结果占满暂存空间。

```typescript
import { playlist, type PlaylistRef } from 'foo-webview-sdk/bridge';
import type { FBEventPayloadMap } from 'foo-webview-sdk';

type EditorResult = FBEventPayloadMap['playlist:autoplaylistUIResult'];
const MAX_PENDING = 16;
const pending = new Map<string, string>();
const early = new Map<string, EditorResult>();
let accepting = 0;
let disposed = false;

function handleResult(event: EditorResult) {
  if (pending.get(event.operationId) !== event.playlistGuid) return;
  pending.delete(event.operationId);
  if (event.success) {
    console.log('原生编辑器调用已返回', event.playlistGuid);
  } else {
    console.error(event.code, event.reason, event.error);
  }
}

const off = playlist.onAutoplaylistUIResult((event) => {
  if (disposed) return;
  if (pending.has(event.operationId)) {
    handleResult(event);
  } else if (accepting > 0) {
    if (early.size >= MAX_PENDING) {
      const oldest = early.keys().next().value;
      if (oldest !== undefined) early.delete(oldest);
      console.warn('已丢弃一条无法关联的编辑器结果');
    }
    early.set(event.operationId, event);
  }
});

async function openEditor(target: PlaylistRef) {
  if (disposed || pending.size + accepting >= MAX_PENDING) {
    throw new Error('当前无法跟踪新的编辑器请求');
  }
  accepting++;
  try {
    const accepted = await playlist.showAutoplaylistUI(target);
    if (disposed) return accepted;
    if (accepted.success === false) {
      console.error(accepted.code, accepted.details, accepted.error);
      return accepted;
    }
    pending.set(accepted.operationId, accepted.playlistGuid);
    const cached = early.get(accepted.operationId);
    early.delete(accepted.operationId);
    if (cached) handleResult(cached);
    return accepted;
  } finally {
    accepting--;
    if (accepting === 0) early.clear();
  }
}

function dispose() {
  disposed = true;
  off();
  pending.clear();
  early.clear();
  window.removeEventListener('pagehide', dispose);
}
window.addEventListener('pagehide', dispose, { once: true });

void openEditor('{12345678-1111-2222-3031-323334353637}').catch(console.error);
// 视图卸载时也要调用 dispose()
```

通过实际的 `playlist:itemsAdded`、`playlist:itemsRemoved`、`playlist:itemsReordered`、`playlist:itemsReplaced` 和 `playlist:lockChanged` 事件刷新相关状态。调用结果可以触发一次重读，但不表示配置变化，也不能覆盖非模态窗口后续的编辑。不要据此合成 changed 事件，也不要等待 saved、cancelled 或 closed 事件。

### convertToAutoplaylist(index, query, sort?, keepSorted?)

将普通播放列表转换为智能播放列表。参数同 `createAutoplaylist`。返回 `{success}`。

```javascript
await fb.playlist.convertToAutoplaylist(0, '%rating% GREATER 3', '%rating%', true);
```

### removeAutoplaylist(index)

签名：`fb.playlist.removeAutoplaylist(index: PlaylistRef): Promise<PlaylistRemoveAutoplaylistResponse>`

解除 autoplaylist manager 管理的 client，包括第三方 client，保留当前曲目并变回普通列表，此时响应的 `source` 为 `sdk`。只有在没有 manager client、仅由兼容规则识别到自动列表锁时，调用返回 `source: 'dui'` 及说明 `note`，且不修改列表状态。若需清理该列表，请调用 `remove()`。不是智能播放列表时以 `NOT_FOUND` 失败。

```javascript
await fb.playlist.removeAutoplaylist(0);
```

## isLocked(index) / getLockInfo(index)

获取播放列表锁定状态。

```javascript
const locked = await fb.playlist.isLocked(0);   // {isLocked}
const info = await fb.playlist.getLockInfo(0);   // {playlist, isLocked}
```

## replaceAllAndPlay(options)

清空播放列表，按传入顺序加入 `paths`，把它设为活动列表，再播放或聚焦 `playIndex`。返回值与失败情形见 [`playlist.replaceAllAndPlay`](../api/playlist.md#playlist-replaceallandplay)。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options.playlist | number | 播放列表索引 |
| options.paths | string[] | 文件路径数组，各行按这个顺序排列 |
| options.playIndex | number | 要播放的行，按 `paths` 的顺序计（默认 0） |
| options.stopFirst | boolean | 是否先停止当前播放（默认 true） |
| options.autoPlay | boolean | 是否自动播放（默认 true），false 只装载不播放 |

```javascript
await fb.playlist.replaceAllAndPlay({
    playlist: 0,
    paths: ['E:\\Music\\album\\*.flac'],
    playIndex: 0
});
```

## reorderPlaylists(order)

重新排序播放列表。`order` 指定新的排列顺序，所有播放列表必须且仅能出现一次：可全量传入当前物理索引，或全量传入 `getAll()` 获取的 `guid`。传入索引时按宿主处理请求时的实时清单解析，若并发增删可能导致目标播放列表错位；使用 GUID 则精确锁定目标播放列表，引用不存在的 GUID 时以 `NOT_FOUND` 失败。

```javascript
await fb.playlist.reorderPlaylists([2, 0, 1]); // 将第3个移到最前

const all = await fb.playlist.getAll();
if (all.success) {
  await fb.playlist.reorderPlaylists(all.playlists.map((p) => p.guid).reverse());
}
```

## focusTrack(playlistIndex, trackIndex)

签名：`fb.playlist.focusTrack(playlistIndex: PlaylistRef, trackIndex: number): Promise<PlaylistFocusTrackResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| playlistIndex | number | 是 | 播放列表索引 |
| trackIndex | number | 是 | 曲目索引 |

已弃用：与 `setFocused()` 是同一个操作。

```javascript
await fb.playlist.focusTrack(0, 12);
```

## getAvailableColumns()

签名：`fb.playlist.getAvailableColumns(): Promise<PlaylistGetAvailableColumnsResponse>`

返回 `{ columns, count }`：foobar2000 与已装组件为默认界面播放列表视图定义的列。

```javascript
const res = await fb.playlist.getAvailableColumns();
if (res.success === false) throw new Error(res.error);
console.log(res.columns.map((c) => c.name));
```

## getFocusTrack(index)

签名：`fb.playlist.getFocusTrack(index: PlaylistRef): Promise<PlaylistGetFocusTrackResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| index | number | 是 | 播放列表索引 |

已弃用：与 `getFocused()` 一样返回 `{ playlist, index }`，区别是播放列表不存在时以 `INVALID_INDEX` 失败。

```javascript
const focus = await fb.playlist.getFocusTrack(0);
```

## getPlaylistCount()

签名：`fb.playlist.getPlaylistCount(): Promise<PlaylistGetCountResponse>`

以 `count` 报告有多少个播放列表。

```javascript
const res = await fb.playlist.getPlaylistCount();
if (res.success === false) throw new Error(res.error);
const { count } = res;
```

## createManagedAutoplaylist(name, query, sort?, keepSorted?)

签名：`fb.playlist.createManagedAutoplaylist(name: string, query: string, sort?: string, keepSorted?: boolean): Promise<PlaylistCreateManagedAutoplaylistResponse>`

创建由本组件持有规则并支持编辑的智能列表，**需要持续安装 foo_ui_webview2**。已有 `createAutoplaylist` 和 `convertToAutoplaylist` 仍使用 foobar2000 自身的提供者，不能用新配置方法读取它们的规则。

```javascript
const created = await fb.playlist.createManagedAutoplaylist(
  'Favourites', '%rating% GREATER 3', '%artist%|%album%|%tracknumber%', true
);
if (created.success === false) throw new Error(created.error);
console.log(created.playlistGuid, created.revision);
```

## getAutoplaylistConfig(playlist)

签名：`fb.playlist.getAutoplaylistConfig(playlist: PlaylistRef): Promise<PlaylistGetAutoplaylistConfigResponse>`

读取提供者的查询原文、独立排序、当前保持排序标志和不透明配置版本。传入 GUID 可以在列表重排后继续指向同一列表。`getAutoplaylistInfo().configAvailability` 表示主题是否能提供自绘配置表单；原生编辑器可用性是另一个维度。其他提供者返回 `NOT_SUPPORTED`；由内置 query/sort 客户端驱动的列表，用 `getAutoplaylistQuery` 读取其查询与排序。

## setAutoplaylistConfig(playlist, options)

签名：`fb.playlist.setAutoplaylistConfig(playlist: PlaylistRef, options: Omit<PlaylistSetAutoplaylistConfigParams, 'playlist' | 'playlistGuid'>): Promise<PlaylistSetAutoplaylistConfigResponse>`

编辑前先读取配置，把快照的 `revision` 作为 `expectedRevision`。至少传入 `query`、`sort`、`keepSorted` 中的一个。省略字段保留当前值；`sort: ''` 清除独立排序，`keepSorted: false` 允许手动调整顺序。查询内显式 `SORT BY` 不能与非空的独立排序并用。

```javascript
const all = await fb.playlist.getAll();
if (all.success) {
  const target = all.playlists.find((p) => p.name === 'Favourites');
  if (target) {
    const current = await fb.playlist.getAutoplaylistConfig(target.guid);
    if (current.success) {
      const result = await fb.playlist.setAutoplaylistConfig(target.guid, {
        expectedRevision: current.revision,
        query: '%rating% GREATER 4',
        sort: '',
        keepSorted: false,
      });
      if (result.success === false) console.error(result.code, result.error, result.details);
    }
  }
}
```

旧版本返回 `OPERATION_FAILED`，`details.reason` 为 `revisionConflict`。重新读取后让用户决定如何处理冲突，再提交修改。重启或替换提供者会使旧版本失效；它不是列表内容版本。

宿主失败可能留下部分修改。`details.state` 只比较调用前后的配置，为 `unchanged`、`changed` 或 `unknown`；能确认当前状态时附带 `details.current` 快照。成功和失败都不证明曲目重建或配置写盘已完成。业务失败仍返回失败信封，桥接传输失败可能 reject。

## onAutoplaylistConfigChanged(callback)

签名：`fb.playlist.onAutoplaylistConfigChanged(callback: (event: PlaylistAutoplaylistConfigChangedPayload) => void): () => void`

订阅组件各窗口中已确认的配置变化，包括外部改动的保持排序标志。载荷包含 `playlistGuid` 和 `revision`。页面断开期间可能错过事件，编辑前仍须重读。曲目变化继续使用 items 系列事件。

```javascript
const unsubscribe = fb.playlist.onAutoplaylistConfigChanged((event) => {
  console.log('Configuration changed', event.playlistGuid, event.revision);
});
// 消费者销毁时：
unsubscribe();
```
