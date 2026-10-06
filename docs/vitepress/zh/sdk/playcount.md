# fb.playcount 播放统计

`fb.playcount` 读取 `foo_playcount` 提供的播放统计，支持单曲、批量读取和媒体库整体汇总。

## getBatch(paths)

签名：`fb.playcount.getBatch(paths: string[]): Promise<PlaycountGetBatchResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要查询的曲目路径。 |

返回 `{ success, count, results }`。每条 `PlaycountInfo` 结果都有自己的 `success`，可能含 `playCount`、`firstPlayed`、`lastPlayed`、`added`、`rating` 和 `inLibrary`。

宿主对这个调用的处理和 `get()` 完全一样：每个路径一行，顺序与请求一致，每行原样回显 `path`，含 `|subsong:N` 后缀。从未播放的曲目 `playCount` 为 `0`；`rating` 取 1 到 5，未评分时不出现；各日期未知时不出现。解析不到的路径那一行为 `success: false`，带 `error`。没装 foo_playcount 时次数读作 `0`，日期不出现。

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
| `path` | `string` | 是 | 曲目路径。 |
| `count` | `number` | 是 | 为保持签名兼容而保留的参数。 |

::: warning 已弃用
该方法已弃用。foo_playcount 不提供修改播放统计的途径，所以宿主总是以 `code: 'NOT_SUPPORTED'` 让调用失败，Promise 返回 `{ success: false }`。SDK 只发送 `path`，`count` 不会传到宿主。评分请用 `fb.rating.set()`，播放次数由实际播放更新。
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
| `path` | `string` | 是 | 曲目路径；带 `\|subsong:N` 后缀时选 CUE 子曲目。 |

`fb.playcount.get()` 使用 `paths: [path]` 调用已注册的 `playcount.get` handler，返回它的信封 `{ count, results }`，这首曲目的条目就是 `results[0]`；调用失败时返回失败信封。条目的字段同 `getBatch()`：`success`、`playCount`、`rating`（未评分时不出现）、`inLibrary`，以及已知时才出现的日期 `firstPlayed`、`lastPlayed` 和 `added`。

```javascript
const res = await fb.playcount.get('E:\\Music\\song.flac');
const info = res.success ? res.results[0] : undefined;
if (info?.success) {
	console.log(info.playCount, info.lastPlayed);
}
```
