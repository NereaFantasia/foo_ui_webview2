# fb.rating 曲目评分

`fb.rating` 读写 0 到 5 的整数曲目评分。

## get(path, opts?)

签名：`fb.rating.get(path: string, opts?: { cueIndex?: number }): Promise<RatingGetResponse>`

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| path | string | 是 | 文件的绝对路径；CUE 条目可以带 `\|subsong:N` 后缀 |
| opts.cueIndex | number | 否 | 显式指定的 CUE 子曲目序号，优先于 `\|subsong:N` 后缀 |

返回值为 `{ path, rating, storage }`；`storage` 为 `'stats'` 表示读取自 foo_playcount 统计数据库，为 `'file'` 表示读取自音频标签。若 foo_playcount 中记录的 `%rating%` 处于 1 至 5 之间则优先采用；否则读取文件内置的 `RATING` 标签并限定在 0 至 5 范围内。`0` 表示未评分。若统计数据库与文件标签均无有效评分，`storage` 默认为 `'file'`。音频文件无法打开时返回 `OPERATION_FAILED`。

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

通过 foo_playcount 提供的上下文菜单命令写入评分。该命令不可用时，宿主改为异步写入音频文件的 `RATING` 标签。评分数值超出 0 至 5 范围时返回 `INVALID_PARAMS`。

返回 `{ path, rating, storage }`。若通过 foo_playcount 菜单写入成功，`storage` 为 `'stats'`，并在 `menuPath` 中记录执行路径；若回退为音频文件标签写入，`storage` 为 `'file'`，并在 `note` 中说明回退原因。

```javascript
await fb.rating.set('E:\\Music\\song.flac', 4);
await fb.rating.set('E:\\Music\\song.flac', 0); // 清除评分
```
