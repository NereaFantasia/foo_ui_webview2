# fb.playlist 播放列表 

本页凡是接受播放列表（`index`、`playlistIndex`）的方法都接受 `PlaylistRef`：播放列表的序号，或 `getAll()`、`getActive()`、`create()`、`duplicate()` 报告的 `guid` 字符串。序号指的是宿主处理这次调用时位于该位置的列表；`guid` 在别的列表增删或重排时仍指向同一个列表，该列表不存在后以 `NOT_FOUND` 失败。`fb.library.addToPlaylist` 与 `fb.artwork.getByPlaylistItem` 同样接受 `PlaylistRef`。

## getAll() 

获取所有播放列表，返回 `{ playlists, count }`，`playlists` 是按播放列表顺序排列的 `PlaylistInfo[]`；失败时返回失败信封。

```javascript
const res = await fb.playlist.getAll();
if (res.success === false) throw new Error(res.error);
// res.playlists: [{index: 0, guid: "{…}", name: "Default", trackCount: 100, isActive: true, isPlaying: false, isLocked: false, isAutoplaylist: false}, ...]
```

菜单或选择器列出播放列表时记下各自的 `guid`，用户选定后原样传回：

```javascript
const res = await fb.playlist.getAll();
if (res.success === false) throw new Error(res.error);
const target = res.playlists.find((p) => p.name === '收藏');
// 稍后菜单关闭时再调用；这期间播放列表清单可能已经变了。
if (target) await fb.library.addToPlaylist(['E:\\Music\\song.flac'], target.guid);
```

::: tip
v1.1.18 起 `getAll()` 不再返回 `duration` 字段。如需 duration，请使用 `fb.playlist.getActive()` 或 `fb.playlist.getPlaying()`。
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

返回分页响应 `{ playlist, start, count, total, tracks }`；`fields` 能收窄行，所以行的类型是 `PlaylistTrackPartial`。

```javascript
const page = await fb.playlist.getTracks(0, 200, 200, undefined, ['title', 'album']);
if (page.success === false) throw new Error(page.error);
console.log(`${page.tracks.length} / ${page.total}`);
```

`fields` 可用的名字与 `library.query` 相同，区分大小写。省略时返回整行，整行还带 `composer`、`comment` 与 foo_playcount 的播放统计。请求被拒（比如字段名未知）时返回 `{ success: false, code: 'INVALID_PARAMS' }`。

与同一播放列表的 `getGroupRuns()` 响应比较 `total`，可以发现两次读取之间曲目数量发生变化。两者相等不能排除等量替换或重排；两次调用各自读取快照，并不共享播放列表版本。

## getTracksPage(index, start, count, formats?, fields?)

已弃用：与 `getTracks()` 是同一个调用，返回同样的分页响应。请改用 `getTracks()`。

## getGroupRuns(patterns, index?)

把整份播放列表按顺序切成分组游程，只返回游程边界，不返回行。

游程是相邻且分组键相同的最长一段，比较时按 ASCII 的 `A-Z`/`a-z` 折叠大小写。不重排行，所以游程顺序就是列表顺序。

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

成功时，`runs` 覆盖整份列表。非空列表的 `runs[0].start` 为 0，相邻游程首尾相接，`count` 之和等于 `total`。空列表返回 `total: 0` 和 `runs: []`。`sub` 只在传了两个模式时出现，其 `start` 与父游程采用相同的绝对行号。

与 `getTracks()` 搭配可实现分组虚拟列表：游程提供组头位置，界面据此计算滚动总高度，可见页的曲目行另行获取。只有平均每组约 10 首时，10 万首才对应约 1 万个一级游程。响应大小取决于分组键和二级游程数量；完整曲目行的大小还取决于请求字段及元数据内容。

若模式让每行单独成组，`runs` 的长度就等于 `total`。宿主不设上限，每个游程的字节数也不固定；请选能合并相邻曲目的模式。

`patterns` 形状不对或模式编译失败，返回 `{ success: false, code: 'INVALID_PARAMS' }`；空模式或编译失败的模式由 `details.pattern` 给出下标。播放列表索引超出最后一个时返回 `INVALID_INDEX`，省略索引且没有活动播放列表时返回 `NO_ACTIVE_ITEM`。

## getMatchingRows(query, index?) / getTracksAt(index, rows, formats?, fields?)

不管列表多长，筛选都只要两次调用。`getMatchingRows()` 返回 `{ playlist, playlistGuid, total, items, count }`，`items` 是曲目符合 foobar2000 查询的行，升序。`getTracksAt()` 按行号读行，按传入顺序，形状与 `getTracks()` 相同；超出最后一行的行号跳过，每行都带 `index`。

```javascript
const guid = '{A9624480-77F0-4A2B-A76F-7EAB0C2EEC1C}';
const hits = await fb.playlist.getMatchingRows('artist HAS nachi', guid);
if (hits.success === false) {
  // 查询本身被拒时 details.param 为 'query'
} else {
  const page = await fb.playlist.getTracksAt(guid, hits.items.slice(0, 50), undefined, ['title']);
}
```

解析器拒绝的查询、或带 `SORT BY` 的查询，返回 `INVALID_PARAMS` 并带 `details.param: 'query'`；`error` 文字是宿主语言的原文。查询里的小写字母匹配标签里的任意大小写，大写字母只匹配同样的大写。列表不存在时 `getMatchingRows()` 失败（`INVALID_INDEX`、`NOT_FOUND`、`NO_ACTIVE_ITEM`），`getTracksAt()` 则与 `getTracks()` 一样回答空结果。

这些行是快照。收到 `playlist:itemsAdded`、`itemsRemoved`、`itemsReordered`、`itemsReplaced` 与 `metadb:changed` 时丢掉重取；`total` 与列表现在的长度相等不能排除重排或改标签。`%added% DURING LAST 2 WEEKS` 这类随时间变化的查询，结果也会在没有任何事件的情况下变化。

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

复制播放列表。返回 `{success, index, sourcePlaylist, newPlaylist, name, trackCount}`。`name` 省略或为空时，副本名为原名后加 ` (Copy)`。

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

获取/设置焦点曲目（光标位置）。`getFocused` 返回 `{playlist, index}`，播放列表没有焦点或不存在时 `index` 为 `-1`；`setFocused` 的 `trackIndex` 为负数时去掉焦点。

```javascript
const r = await fb.playlist.getFocused(0);
if (r.success === false) throw new Error(r.error);
console.log(`焦点在第 ${r.index} 首`);
await fb.playlist.setFocused(0, 10);
```

## getSelection(index) / getSelectedTracks(index) 

获取选中曲目索引/详细信息。

- `getSelection` 返回 `{items: number[], count, playlist}`
- `getSelectedTracks` 返回 `{ playlist, count, tracks }`，行是整行 `PlaylistTrack`，不带播放统计。没有选中曲目时成功并给出空的 `tracks`；播放列表不存在时返回失败信封，两者因此分得开。

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

获取智能播放列表信息。返回 `{isAutoplaylist, playlist, keepSorted, source}`。

- `source` — `"sdk"`（SDK 创建）或 `"dui"`（DUI 创建）
- 非智能播放列表时返回 `{isAutoplaylist: false, playlist}`

```javascript
const info = await fb.playlist.getAutoplaylistInfo(0);
if (info.success === false) throw new Error(info.error);
if (info.isAutoplaylist) {
    console.log(info.source, info.keepSorted);
}
```

### getAutoplaylistQuery(index)

签名：`fb.playlist.getAutoplaylistQuery(index: PlaylistRef): Promise<PlaylistGetAutoplaylistQueryResponse>`

返回与 `getAutoplaylistInfo()` 相同的状态，外加 `query`。foobar2000 不公开智能播放列表的查询语句，所以 `query` 始终为 `null`；是智能播放列表时，响应的 `note` 字段也会说明这一点。

```javascript
const res = await fb.playlist.getAutoplaylistQuery(0);
```

### convertToAutoplaylist(index, query, sort?, keepSorted?) 

将普通播放列表转换为智能播放列表。参数同 `createAutoplaylist`。返回 `{success}`。

```javascript
await fb.playlist.convertToAutoplaylist(0, '%rating% GREATER 3', '%rating%', true);
```

### removeAutoplaylist(index)

签名：`fb.playlist.removeAutoplaylist(index: PlaylistRef): Promise<PlaylistRemoveAutoplaylistResponse>`

把智能播放列表变回保留当前曲目的普通列表，此时响应的 `source` 为 `sdk`。由其他组件管理的智能播放列表不能这样解除：调用成功、`source` 为 `dui` 并带 `note`，但什么也不改，要删除它用 `remove()`。不是智能播放列表时以 `NOT_FOUND` 失败。

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

原子操作：清空 + 添加 + 播放。返回 `{success, addedCount}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| options.playlist | number | 播放列表索引 |
| options.paths | string[] | 文件路径数组 |
| options.playIndex | number | 播放起始索引（默认 0） |
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

重新排序播放列表。`order` 是新的顺序，每个播放列表恰好出现一次：全部用当前索引，或全部用 `getAll()` 报告的 `guid`。索引按宿主处理这次调用时的清单解读，所以用早先 `getAll()` 拼出的顺序，只要期间加了一张又删了一张，就会排错列表；GUID 始终指向读取时的那些列表，没有列表持有的 GUID 以 `NOT_FOUND` 失败。

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
