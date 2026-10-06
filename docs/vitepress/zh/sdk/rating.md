# fb.rating 曲目评分

`fb.rating` 读写 0 到 5 的整数曲目评分。

## get(path, opts?)

签名：`fb.rating.get(path: string, opts?: { cueIndex?: number }): Promise<RatingGetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 文件的绝对路径；CUE 条目可以带 `\|subsong:N` 后缀 |
| opts.cueIndex | number | 否 | 显式指定的 CUE 子曲目序号，优先于 `\|subsong:N` 后缀 |

返回值为 `{ path, rating, storage }`，`storage` 为 `stats` 表示来自 foo_playcount，否则为 `file`。foo_playcount 的 `%rating%` 在 1 到 5 之间时优先，否则取文件的 `RATING` 标签并夹到 0 到 5。`0` 表示未评分。两处都没有评分时 `storage` 也是 `file`，所以它区分不了「没有标签」和「标签里存的是 0」。路径打不开时以 `OPERATION_FAILED` 失败。

```javascript
const result = await fb.rating.get('E:\\Music\\song.flac');
if (result.success === false) throw new Error(result.error);
console.log(result.rating, result.storage); // 0-5
```

## set(path, rating, opts?)

签名：`fb.rating.set(path: string, rating: number, opts?: { cueIndex?: number }): Promise<RatingSetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 文件的绝对路径；CUE 条目可以带 `\|subsong:N` 后缀 |
| rating | number | 是 | 0 到 5 的整数；`0` 清除评分 |
| opts.cueIndex | number | 否 | 显式指定的 CUE 子曲目序号，优先于 `\|subsong:N` 后缀 |

通过 foo_playcount 右键菜单里的评分命令设置评分。该命令不可用时，宿主改为派发一次文件 `RATING` 标签的写入。评分超出 0 到 5 时以 `INVALID_PARAMS` 失败。

返回 `{ path, rating, storage }`。foo_playcount 的命令执行了时 `storage` 为 `stats`，响应在 `menuPath` 里给出执行的菜单路径；派发了标签写入时 `storage` 为 `file`，原因在 `note` 里。

```javascript
await fb.rating.set('E:\\Music\\song.flac', 4);
await fb.rating.set('E:\\Music\\song.flac', 0); // 清除评分
```
