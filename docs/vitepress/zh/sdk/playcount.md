# fb.playcount 播放统计

`fb.playcount` 读取 `foo_playcount` 提供的播放统计，支持单曲、批量读取和媒体库整体汇总。

## getBatch(paths)

签名：`fb.playcount.getBatch(paths: string[]): Promise<PlaycountGetBatchResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要查询的曲目路径 |

返回 `{ success, count, results }`。每条 `PlaycountInfo` 结果都有自己的 `success`，可能含 `playCount`、`firstPlayed`、`lastPlayed`、`added`、`rating` 和 `inLibrary`。

宿主对本调用的处理与 `get()` 完全相同：按请求顺序逐项解析路径并返回对应条目，条目原样回显 `path`（包含 `|subsong:N` 后缀）。未播放曲目的 `playCount` 为 `0`；`rating` 范围为 1 至 5，未评分时缺省；时间戳字段在无记录时缺省。无法解析的路径条目标记为 `success: false` 并包含 `error` 描述。若未安装 foo_playcount 组件，播放计数均返回 `0` 且日期字段缺省。

```javascript
const result = await fb.playcount.getBatch([
	'E:\\Music\\one.flac',
	'E:\\Music\\two.flac',
]);
```

## getStats()

签名：`fb.playcount.getStats(): Promise<PlaycountGetStatsResponse>`

返回汇总字段，包括 `totalTracks`、`playedTracks`、`unplayedTracks`、`ratedTracks`、`totalPlayCount`、`maxPlayCount`、`averagePlayCount` 和 `averageRating`。统计范围是整个媒体库。`averagePlayCount` 是已播放曲目的平均播放次数，`averageRating` 是有评分曲目的平均评分；没有这类曲目时各为 `0`。

```javascript
const stats = await fb.playcount.getStats();
```

## set(path, count)

签名：`fb.playcount.set(path: string, count: number): Promise<PlaycountSetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径 |
| `count` | `number` | 是 | 为保持签名兼容而保留的参数 |

::: warning 已弃用
该方法已废弃。底层组件不支持外部直接修改播放统计数据，调用始终返回带有 `code: 'NOT_SUPPORTED'` 的失败响应。SDK 仅向宿主传递 `path` 参数，忽略 `count`。评分修改请使用 `fb.rating.set()`，播放计数仅在曲目播放时由系统自动更新。
:::

```javascript
const result = await fb.playcount.set('E:\\Music\\song.flac', 10);
// result.success === false，result.code === 'NOT_SUPPORTED'
```

## 读取单曲统计

### get(path)

签名：`fb.playcount.get(path: string): Promise<PlaycountGetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；带 `\|subsong:N` 后缀时选 CUE 子曲目 |

`fb.playcount.get()` 将单曲路径包装为单元素数组调用 `playcount.get`，成功时返回信封对象 `{ count, results }`，目标曲目详情位于 `results[0]`；调用失败时返回失败信封。条目数据结构与 `getBatch()` 一致：包含 `success`、`playCount`、`rating`（未评分时缺省）、`inLibrary`，以及在有记录时提供的 `firstPlayed`、`lastPlayed` 与 `added` 时间戳。

```javascript
const res = await fb.playcount.get('E:\\Music\\song.flac');
const info = res.success ? res.results[0] : undefined;
if (info?.success) {
	console.log(info.playCount, info.lastPlayed);
}
```
