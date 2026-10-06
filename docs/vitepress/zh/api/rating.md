# Rating 评分 API

`rating` 命名空间的方法：读取与设置曲目评分。CUE 等容器内曲目的寻址见[定位容器内的单曲](./metadata.md#subsong-addressing)。

## 评分

### rating.get

<!-- api-schema:begin rating.get -->
读取曲目 0 到 5 的评分：foo_playcount 的 `%rating%` 在 1 到 5 之间时优先，否则取文件的 `RATING` 标签并夹到 0..5。`0` 表示未评分。读不出标签的文件（含不存在的文件）不算错误，按没有 `RATING` 标签处理。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 曲目路径；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目。不能为空。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，优先于 `path` 里的后缀；负数忽略。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径，含后缀。 |
| `rating` | `integer` | 0 到 5 的评分；`0` 表示未评分。 |
| `storage` | `"stats" \| "file"` | foo_playcount 的评分为 `stats`；`RATING` 标签为 `file`，两处都没有评分时也是 `file`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

### rating.set

<!-- api-schema:begin rating.set -->
通过 foo_playcount 的右键菜单设置评分，菜单不可用时改写文件的 `RATING` 标签。省略 `path` 时给正在播放的曲目评分，否则给活动播放列表的选中项；两者都没有时以 `NO_ACTIVE_ITEM` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 曲目路径；`\|subsong:N` 后缀或结尾的 `#N` 选子曲目。省略则取正在播放的曲目，否则取活动播放列表的选中项。不能为空。 |
| `rating` | `integer` | 是 | 0 到 5 的评分；`0` 清除评分。取值 `0` 到 `5`（含端点）。 |
| `cueIndex` | `integer` | 否 | 子曲目序号，优先于 `path` 里的后缀；负数忽略。默认 `-1`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 原样的路径；省略 `path` 且由 foo_playcount 记录时为 `(current)`。 |
| `rating` | `integer` | 设置的评分。 |
| `storage` | `"stats" \| "file"` | foo_playcount 的菜单执行了为 `stats`；派发了 `RATING` 标签写入为 `file`。 |
| `menuPath` | `string` | 执行的右键菜单路径；`storage` 为 `stats` 时出现。 |
| `note` | `string` | 改写标签的原因；`storage` 为 `file` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
await fb2k.invoke('rating.set', { path: 'C:\\Music\\song.flac', rating: 5 });
await fb2k.invoke('rating.set', { rating: 0 }); // 清除当前播放曲目评分
```

## 使用说明

- `rating.set` 只接受 `0` 到 `5`，`0` 表示清除评分。存在匹配的 foo_playcount 上下文菜单时优先使用它，否则写入文件 `RATING` 标签。`rating.get` 通过 `storage` 返回 `stats` 或 `file`。
