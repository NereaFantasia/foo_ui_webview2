# Titleformat API

提供 Titleformat 格式化表达式求值能力，支持单曲目、批量路径与具名字段映射。

## 单文件求值

### titleformat.eval

<!-- api-schema:begin titleformat.eval -->
对单首曲目求值指定 Titleformat 表达式（省略路径时默认针对正在播放的曲目求值）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 否 | 待求值的曲目路径（支持 \|subsong:N 后缀指定子曲目；省略时针对正在播放的曲目求值并填充 %playback_time% 与流媒体标题等动态字段，若当前未处于播放状态则以 NO_ACTIVE_ITEM 失败）不能为空。 |
| `pattern` | `string` | 是 | Titleformat 格式化表达式（例如 %artist% - %title%）不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 实际参与求值的曲目路径（请求未传入路径时为当前播放曲目的路径） |
| `pattern` | `string` | 实际求值的 Titleformat 表达式 |
| `result` | `string` | 表达式格式化后的输出文本 |
| `infoAvailable` | `boolean` | 曲目元数据是否可用（为 false 表示宿主无法读取曲目信息，如无缓存的网络流或不可读文件，此时基于标签的输出不可信；未载入过 foobar2000 的本地文件会从磁盘直接读取并返回 true；该标志不涵盖 foo_playcount 维护的 %rating% 等统计字段） |

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
对单首曲目单次求值多个具名 Titleformat 表达式（各表达式合并为单一脚本编译；若编译失败调用仍返回成功，但结果中不包含字段键与 infoAvailable，行为与 titleformat.evalFieldsBatch 报错 INVALID_PARAMS 不同）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `path` | `string` | 是 | 待求值的曲目路径（支持 \|subsong:N 后缀指定子曲目）不能为空。 |
| `fields` | `Record<string, string>` | 是 | 输出字段名到 Titleformat 表达式的键值映射字典（例如 { "year": "$year(%date%)" }） |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `string` | 实际参与求值的曲目路径 |
| `infoAvailable` | `boolean` | 曲目元数据是否可用（取值条件同 titleformat.eval，为 false 表示基于标签的输出不可信；单次请求内所有字段共享该标志；fields 为空时不提供） |
| `[每个键]` | `string` | fields 映射中各字段格式化求值后的文本（键名为 success、path 或 infoAvailable 时将被丢弃以避免覆盖顶层同名结果字段；error 或 code 键名将被保留，调用成功与否以顶层 success 为准） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

内部合并为单次 `format_title()` 调用，执行效率高于多次独立的 `eval` 请求。

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
批量对多首曲目求值同一个 Titleformat 表达式（表达式在底层仅编译一次）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待求值的曲目路径列表（支持 \|subsong:N 后缀指定子曲目） |
| `pattern` | `string` | 是 | 应用于列表中每个路径的 Titleformat 表达式不能为空。 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `pattern` | `string` | 实际求值的 Titleformat 表达式 |
| `total` | `integer` | 传入待求值的曲目路径总数 |
| `successCount` | `integer` | 成功完成格式化求值的条目数 |
| `errorCount` | `integer` | 格式化求值失败的条目数 |
| `results` | `EvalBatchRow[]` | 逐路径求值结果列表（保持与请求路径列表相同的顺序） |
| `results[].path` | `string` | 当前行对应的曲目路径 |
| `results[].success` | `boolean` | 当前行是否成功完成求值 |
| `results[].result` | `string` | 格式化后的输出文本（仅在 success 为 true 时提供） |
| `results[].infoAvailable` | `boolean` | 曲目元数据是否可用（语义同 titleformat.eval；仅在 success 为 true 时提供） |
| `results[].error` | `string` | 当前行求值失败的具体错误信息（仅在 success 为 false 时提供） |

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
批量对多首曲目求值多个具名 Titleformat 表达式（合并后的表达式仅编译一次）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `paths` | `string[]` | 是 | 待求值的曲目路径列表（支持 \|subsong:N 后缀指定子曲目） |
| `fields` | `Record<string, string>` | 是 | 输出字段名到 Titleformat 表达式的键值映射字典 |

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `total` | `integer` | 传入待求值的曲目路径总数（fields 为空时固定为 0） |
| `successCount` | `integer` | 成功完成格式化求值的条目数 |
| `errorCount` | `integer` | 格式化求值失败的条目数 |
| `results` | `(EvalFieldsBatchRow & Record<string, string>)[]` | 逐路径批量求值结果列表（保持与请求路径列表相同的顺序） |
| `results[].path` | `string` | 当前行对应的曲目路径 |
| `results[].success` | `boolean` | 当前行是否成功完成求值 |
| `results[].infoAvailable` | `boolean` | 曲目元数据是否可用（语义同 titleformat.evalFields；仅在 success 为 true 时提供） |
| `results[].error` | `string` | 当前行求值失败的具体错误信息（仅在 success 为 false 时提供） |
| `results[].[每个键]` | `string` | fields 映射中各字段格式化求值后的文本（键名为 path、success 或 infoAvailable 时将被丢弃以避免覆盖当前行同名属性；error 键名将被保留，当前行成功与否以行内 success 为准） |

成功时 `success` 为 `true`；失败时返回 `{ success: false, error, code }`，`code` 见[错误码](../reference/errors.md)。
<!-- api-schema:end -->

内部合并所有字段为单次调用，处理 100 条路径 × 10 个字段仅需 100 次 `format_title()` 调用。

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
获取常用 Titleformat 字段表达式字典（以易读字段名为键）

无参数。

**返回值**

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `fields` | `Record<string, string>` | 常用字段名到对应 Titleformat 表达式的映射字典（例如 "year": "$year(%date%)"；播放统计相关字段依赖 foo_playcount 组件） |

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

- 参数在 handler 执行前校验：必填字符串缺失或为空、类型不符、传入未声明的参数键时，均返回 `success: false` 与 `code: "INVALID_PARAMS"`。
- `titleformat.eval` 要求非空 `pattern`；未指定 `path` 时求值当前正在播放曲目，若当前无播放曲目则返回 `NO_ACTIVE_ITEM`；表达式语法错误或文件无法访问时返回带 `error` 的 `success: false`。
- `titleformat.evalBatch` 要求非空数组 `paths` 与非空 `pattern`；单项求值失败记录在对应行的 `error` 中，批次总体通过 `successCount` 与 `errorCount` 汇总。
- `titleformat.evalFields` 与 `titleformat.evalFieldsBatch` 要求对象 `fields`，其值须为表达式字符串，每个键映射为响应中的同名属性；出现非字符串值时整体返回 `INVALID_PARAMS`。
- `titleformat.getBuiltinFields` 为运行时生成的便捷参考映射表，播放统计等部分字段依赖可选的 foo_playcount 组件。
