# fb.playlist 播放列表 

## getAll() 

获取所有播放列表。

```javascript
const playlists = await fb.playlist.getAll();
// [{index: 0, name: "Default", trackCount: 100, isActive: true, isPlaying: false, isLocked: false, isAutoplaylist: false}, ...]
```

::: tip TIP
v1.1.18 起 `getAll()` 不再返回 `duration` 字段。如需 duration，请使用 `fb.playlist.getActive()` 或 `fb.playlist.getPlaying()`。
:::

## getActive() / setActive(index) 

获取/设置当前活动播放列表。

```javascript
const info = await fb.playlist.getActive();
// {index, name, trackCount, isActive, isPlaying, isLocked, duration}
console.log(info.index, info.name);

await fb.playlist.setActive(1);
```

## getTracks(index, start, count) 

获取播放列表中的曲目（分页）。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| start | number | 起始位置（默认 0） |
| count | number | 获取数量（默认 100） |

返回 `{playlist, start, count, total, tracks: [{title, artist, album, duration, path, absolutePath, ...}]}`。

```javascript
const r = await fb.playlist.getTracks(0, 0, 50);
console.log(`共 ${r.total} 首，当前返回 ${r.tracks.length} 首`);
```

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
const r = await fb.playlist.add(0, ['E:\\\\Music\\\\song1.flac', 'E:\\\\Music\\\\song2.mp3']);
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

获取当前正在播放的播放列表。返回结构同 `getActive()`。

```javascript
const playing = await fb.playlist.getPlaying();
```

## getCount(index) 

获取播放列表曲目数量。返回 `{count}`。

```javascript
const r = await fb.playlist.getCount(0);
console.log(r.count);
```

## create(name?) 

创建新播放列表。返回 `{success, index}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| name | string | 播放列表名称（默认 "New Playlist"） |

```javascript
const r = await fb.playlist.create('Rock');
console.log(`新播放列表索引: ${r.index}`);
```

## remove(index) / clear(index) 

删除播放列表 / 清空曲目。返回 `{success}`。

```javascript
await fb.playlist.remove(2);
await fb.playlist.clear(0);  // 清空但保留列表
```

## rename(index, name) 

重命名播放列表。返回 `{success}`。

```javascript
await fb.playlist.rename(0, 'Favorites');
```

## duplicate(index) 

复制播放列表。返回 `{success, index, sourcePlaylist, newPlaylist, name, trackCount}`。

```javascript
const r = await fb.playlist.duplicate(0);
console.log(`复制到索引 ${r.newPlaylist}`);
```

## addAsync(index, paths) / addSequential(index, paths) 

异步添加文件（不阻塞 UI）/ 顺序添加（保证文件顺序）。返回 `{success, addedCount}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| paths | string[] | 文件路径数组 |

```javascript
await fb.playlist.addAsync(0, ['E:\\\\Music\\\\*.flac']);
await fb.playlist.addSequential(0, paths); // 保证文件顺序
```

## addHandles(index, handles) 

精确添加轨道，不自动展开 CUE。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| handles | object[] | handle 对象数组，每个包含 {path, subsong?} |

```javascript
await fb.playlist.addHandles(0, [
    { path: 'E:\\\\Music\\\\album.cue', subsong: 1 },
    { path: 'E:\\\\Music\\\\album.cue', subsong: 2 },
]);
```

## insertTracks(index, insertIndex, handles) 

在指定位置插入轨道。返回 `{success, addedCount}`。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| index | number | 播放列表索引 |
| insertIndex | number | 插入位置 |
| handles | object[] | handle 对象数组 |

```javascript
await fb.playlist.insertTracks(0, 5, handles);
```

## removeSelectedTracks(index) 

移除播放列表中已选中的曲目。返回 `{success}`。

## getFocused(index) / setFocused(index, trackIndex) 

获取/设置焦点曲目（光标位置）。`getFocused` 返回 `{index}`，`setFocused` 返回 `{success}`。

```javascript
const r = await fb.playlist.getFocused(0);
console.log(`焦点在第 ${r.index} 首`);
await fb.playlist.setFocused(0, 10);
```

## getSelection(index) / getSelectedTracks(index) 

获取选中曲目索引/详细信息。

- `getSelection` 返回 `{items: number[], count}`
- `getSelectedTracks` 返回 `{tracks: [{title, artist, ...}]}`

```javascript
const sel = await fb.playlist.getSelection(0); // {items: [0, 5, 10], count: 3}
const tracks = await fb.playlist.getSelectedTracks(0);
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
if (info.isAutoplaylist) {
    console.log(info.source, info.keepSorted);
}
```

### getAutoplaylistQuery(index) 

获取智能播放列表的查询表达式。返回 `{isAutoplaylist, playlist, query, keepSorted, source, note}`。

> ⚠️ 由于 foobar2000 SDK 限制，`query` 始终为 `null`。

### convertToAutoplaylist(index, query, sort?, keepSorted?) 

将普通播放列表转换为智能播放列表。参数同 `createAutoplaylist`。返回 `{success}`。

```javascript
await fb.playlist.convertToAutoplaylist(0, '%rating% GREATER 3', '%rating%', true);
```

### removeAutoplaylist(index) 

移除智能播放列表属性（转为普通列表）。返回 `{success}`。

## isLocked(index) / getLockInfo(index) 

获取播放列表锁定状态。

```javascript
const locked = await fb.playlist.isLocked(0);   // {isLocked}
const info = await fb.playlist.getLockInfo(0);   // {isLocked, lockName, ...}
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
    paths: ['E:\\\\Music\\\\album\\\\*.flac'],
    playIndex: 0
});
```

重新排序播放列表。`order` 为新顺序的索引数组。

```javascript
await fb.playlist.reorderPlaylists([2, 0, 1]); // 将第3个移到最前
```

## 补全方法参考

### focusTrack(playlistIndex, trackIndex)

签名：`fb.playlist.focusTrack(playlistIndex: number, trackIndex: number): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| playlistIndex | number | 是 | 播放列表索引 |
| trackIndex | number | 是 | 曲目索引 |

返回值：设置焦点曲目的操作结果。

```javascript
await fb.playlist.focusTrack(0, 12);
```

### getAutoplaylistQuery(index)

签名：`fb.playlist.getAutoplaylistQuery(index: number): Promise<AutoplaylistQueryResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| index | number | 是 | 播放列表索引 |

返回值：智能播放列表查询信息；受 foobar2000 SDK 限制，部分来源的 `query` 可能为 `null`。

```javascript
const query = await fb.playlist.getAutoplaylistQuery(0);
```

### getAvailableColumns()

签名：`fb.playlist.getAvailableColumns(): Promise<PlaylistColumnsResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：可用于播放列表显示或排序的列定义。

```javascript
const columns = await fb.playlist.getAvailableColumns();
```

### getFocusTrack(index)

签名：`fb.playlist.getFocusTrack(index: number): Promise<PlaylistFocusTrackResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| index | number | 是 | 播放列表索引 |

返回值：当前焦点曲目的索引或曲目信息。

```javascript
const focus = await fb.playlist.getFocusTrack(0);
```

### getPlaylistCount()

签名：`fb.playlist.getPlaylistCount(): Promise<{ count: number }>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| - | - | - | 无参数 |

返回值：播放列表总数。

```javascript
const { count } = await fb.playlist.getPlaylistCount();
```

### removeAutoplaylist(index)

签名：`fb.playlist.removeAutoplaylist(index: number): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| index | number | 是 | 播放列表索引 |

返回值：移除智能播放列表属性后的操作结果。

```javascript
await fb.playlist.removeAutoplaylist(0);
```

### removeSelectedTracks(index)

签名：`fb.playlist.removeSelectedTracks(index: number): Promise<BaseResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| index | number | 是 | 播放列表索引 |

返回值：移除所选曲目的操作结果。

```javascript
await fb.playlist.removeSelectedTracks(0);
```
