# Titleformat API

foobar2000 Titleformat 表达式求值。

## 单文件求值

### titleformat.eval

<!-- api-schema:begin titleformat.eval -->
对单个文件求值单个 titleformat 表达式；不给路径时对正在播放的曲目求值。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 要求值的曲目路径；带 `\|subsong:N` 后缀时选子曲目，如 CUE 里的一首。省略时对正在播放的曲目求值，`%playback_time%`、网络流标题等动态字段也会填上；此时没有曲目在播放则以 `NO_ACTIVE_ITEM` 失败。不能为空。 |
| `pattern` | `string` | 是 | titleformat 表达式，如 `%artist% - %title%`。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 被求值的路径；请求没给路径时为正在播放曲目的路径。 |
| `pattern` | `string` | 被求值的表达式。 |
| `result` | `string` | 格式化后的文本。 |
| `infoAvailable` | `boolean` | 为 `false` 表示宿主拿不到该曲目的信息（远程路径且没有缓存，或文件读不了），基于标签的输出不可信。foobar2000 没载入过的本地文件会从磁盘读取，报 `true`。不涵盖 `%rating%` 等 foo_playcount 字段。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

```javascript
const r = await fb2k.invoke('titleformat.eval', {
    path: 'C:\\Music\\song.flac',
    pattern: '%artist% - %title%'
});
if (r.success === false) throw new Error(r.error);
console.log(r.result);
```

### titleformat.evalFields

<!-- api-schema:begin titleformat.evalFields -->
对单个文件一次求值多个具名表达式。各表达式合成一个脚本编译，所以编译失败不会让调用失败：此时结果里没有任何字段键，也没有 `infoAvailable`；`titleformat.evalFieldsBatch` 则以 `INVALID_PARAMS` 失败。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 要求值的曲目路径；带 `\|subsong:N` 后缀时选子曲目。不能为空。 |
| `fields` | `Record<string, string>` | 是 | 输出键到 titleformat 表达式的映射，如 `{ "year": "$year(%date%)" }`。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 被求值的路径。 |
| `infoAvailable` | `boolean` | 与 `titleformat.eval` 同样的条件下为 `false`，基于标签的值不可信；整个请求共用一个标志。`fields` 为空时不出现。 |
| `[每个键]` | `string` | `fields` 的每个键对应一项，值为格式化后的文本。键名为 `success`、`path` 或 `infoAvailable` 的项会被丢弃，响应里的同名字段优先；键名为 `error` 或 `code` 的项会保留，判断是否失败要看 `success`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

内部合并为单次 `format_title()` 调用，比多次 `eval` 更高效。

```javascript
const r = await fb2k.invoke('titleformat.evalFields', {
    path: track.absolutePath,
    fields: {
        artist: '%artist%',
        album: '%album%',
        year: '$year(%date%)'
    }
});
```

## 批量求值

### titleformat.evalBatch

<!-- api-schema:begin titleformat.evalBatch -->
对多个文件求值同一个表达式，表达式只编译一次。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要求值的曲目路径列表；带 `\|subsong:N` 后缀时选子曲目。 |
| `pattern` | `string` | 是 | 对每个路径应用的 titleformat 表达式。不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pattern` | `string` | 被求值的表达式。 |
| `total` | `integer` | 收到的路径数。 |
| `successCount` | `integer` | 求值成功的行数。 |
| `errorCount` | `integer` | 失败的行数。 |
| `results` | `EvalBatchRow[]` | 每个路径一行，顺序与请求一致。 |
| `results[].path` | `string` | 该行的路径。 |
| `results[].success` | `boolean` | 该行是否求值成功。 |
| `results[].result` | `string` | 格式化后的文本；`success` 为 `true` 时出现。 |
| `results[].infoAvailable` | `boolean` | 含义同 `titleformat.eval`；`success` 为 `true` 时出现。 |
| `results[].error` | `string` | 该行失败的原因；`success` 为 `false` 时出现。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

响应示例：

```json
{
    "success": true,
    "pattern": "%artist%",
    "total": 100,
    "successCount": 98,
    "errorCount": 2,
    "results": [
        { "path": "...", "success": true, "result": "Artist" }
    ]
}
```

### titleformat.evalFieldsBatch

<!-- api-schema:begin titleformat.evalFieldsBatch -->
对多个文件求值多个具名表达式，合并后的表达式只编译一次。

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 要求值的曲目路径列表；带 `\|subsong:N` 后缀时选子曲目。 |
| `fields` | `Record<string, string>` | 是 | 输出键到 titleformat 表达式的映射。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `total` | `integer` | 收到的路径数；`fields` 为空时为 0。 |
| `successCount` | `integer` | 求值成功的行数。 |
| `errorCount` | `integer` | 失败的行数。 |
| `results` | `(EvalFieldsBatchRow & Record<string, string>)[]` | 每个路径一行，顺序与请求一致。 |
| `results[].path` | `string` | 该行的路径。 |
| `results[].success` | `boolean` | 该行是否求值成功。 |
| `results[].infoAvailable` | `boolean` | 含义同 `titleformat.evalFields`；`success` 为 `true` 时出现。 |
| `results[].error` | `string` | 该行失败的原因；`success` 为 `false` 时出现。 |
| `results[].[每个键]` | `string` | `fields` 的每个键对应一项，值为格式化后的文本。键名为 `path`、`success` 或 `infoAvailable` 的项会被丢弃，该行的同名字段优先；键名为 `error` 的项会保留，判断该行是否失败要看 `success`。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

合并所有字段为单次调用，100 路径 × 10 字段仅需 100 次 `format_title()`。

响应示例：

```json
{
    "success": true,
    "total": 100,
    "successCount": 100,
    "errorCount": 0,
    "results": [
        { "path": "...", "success": true, "artist": "...", "album": "..." }
    ]
}
```

## 参考

### titleformat.getBuiltinFields

<!-- api-schema:begin titleformat.getBuiltinFields -->
列出常用的 titleformat 字段，以易读的名称为键。

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fields` | `Record<string, string>` | 易读名称到表达式的映射，如 `"year": "$year(%date%)"`。播放统计类条目需要 foo_playcount 组件。 |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

响应示例：

```json
{
    "success": true,
    "fields": {
        "artist": "%artist%",
        "album": "%album%",
        "title": "%title%",
        "duration": "%length%",
        "playCount": "%play_count%",
        "rating": "%rating%",
        "added": "%added%"
    }
}
```

::: tip
完整字段列表包含标准标签、技术信息、文件信息、foo_playcount 字段和常用组合模式。
:::

## 求值语义

- 参数在 handler 执行前校验。必填字符串缺失或为空、类型不符、出现方法未声明的键，都会返回 `success: false`，`code` 为 `"INVALID_PARAMS"`。
- `titleformat.eval` 要求非空 `path` 和 `pattern`。表达式无效或文件不可用时，会返回带 `error` 的 `success: false`。
- `titleformat.evalBatch` 要求数组 `paths` 和非空 `pattern`；单项失败会保留在 `results` 中，`successCount` 与 `errorCount` 汇总批次结果。
- `titleformat.evalFields` 与 `titleformat.evalFieldsBatch` 要求对象 `fields`，其值须为表达式字符串，每个键会成为响应属性。出现非字符串值时整个调用返回 `INVALID_PARAMS`。
- `titleformat.getBuiltinFields` 是 runtime 生成的便捷参考。它不保证每个安装或曲目都填充播放次数等可选组件字段。
